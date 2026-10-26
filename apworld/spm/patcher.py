"""Turns an extracted Super Paper Mario disc (US 1.0, R8PE01 revision 0) into the Archipelago version.

Patches the extracted folder in place, keeping the original files next to the patched ones as .orig, and
always patches from those originals, so running it again (for example after updating the APworld)
just applies the new version:
  sys/main.dol      + the rel loader (spm-rel-loader by SeekyCt, GPLv3) built into the dol, so no cheat
                      codes are needed
  files/mod/mod.rel   the Archipelago mod
  files/wicon.tpl, files/wicon.bin   item icons, plus the two Archipelago icons

Only the standard library is used, so this runs inside the frozen Archipelago Launcher.
Dolphin boots the result from sys/main.dol.
"""

import hashlib
import os
import pkgutil
import shutil
import struct

GAME_ID = b"R8PE01"
REVISION = 0
# SHA-1 of the clean US 1.0 files
CLEAN_SHA1 = {
    "sys/main.dol": "4669c63d8365dfdced78e5c1518fe2a3f915a0c6",
    "files/wicon.tpl": "fc363d4a7c689d82d9a3152676c8ec1daff52e9a",
    "files/wicon.bin": "a200e7f8df9270532b9033bec2a123f2abb337e9",
}


class PatchError(Exception):
    pass


def bundled(name: str) -> bytes:
    """A file from the APworld's bin/ folder (works from the .apworld zip too)."""
    data = pkgutil.get_data(__name__, f"bin/{name}")
    if data is None:
        raise PatchError(f"bin/{name} is missing from the APworld")
    return data


# main.dol: the rel loader

# Free low memory. Not 0x80001800 itself: Dolphin writes a "STUBHAXX" marker at 0x80001804 when booting.
# The whole 0x80001800-0x80003000 area is also where a Gecko codehandler goes, so the patched game
# can't be combined with Gecko codes in Dolphin.
PAYLOAD_ADDR = 0x80001900
NUM_TEXT = 7
NUM_SECTIONS = 18


def parse_gecko(text: str):
    """The loader's two Gecko code types: 04 (write a word) and C2 (insert asm)."""
    lines = []
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if line:
            a, b = line.split()
            lines.append((int(a, 16), int(b, 16)))
    writes, inserts = [], []
    i = 0
    while i < len(lines):
        a, b = lines[i]
        kind, addr = a >> 24, 0x80000000 | (a & 0x01FFFFFF)
        if kind == 0x04:
            writes.append((addr, b))
            i += 1
        elif kind == 0xC2:
            words = [w for pair in lines[i + 1:i + 1 + b] for w in pair]
            inserts.append((addr, words))
            i += 1 + b
        else:
            raise ValueError(f"Unsupported gecko code type {kind:#04x}")
    return writes, inserts


def branch(src: int, dst: int) -> int:
    return 0x48000000 | ((dst - src) & 0x03FFFFFC)


class Dol:
    def __init__(self, data: bytes):
        self.data = bytearray(data)
        self.off = list(struct.unpack(">18I", data[0:0x48]))
        self.addr = list(struct.unpack(">18I", data[0x48:0x90]))
        self.size = list(struct.unpack(">18I", data[0x90:0xD8]))

    def file_offset(self, addr: int) -> int:
        for i in range(NUM_SECTIONS):
            if self.size[i] and self.addr[i] <= addr < self.addr[i] + self.size[i]:
                return self.off[i] + addr - self.addr[i]
        raise ValueError(f"{addr:#x} isn't in any dol section")

    def read32(self, addr: int) -> int:
        return struct.unpack_from(">I", self.data, self.file_offset(addr))[0]

    def write32(self, addr: int, value: int):
        struct.pack_into(">I", self.data, self.file_offset(addr), value)

    def add_text_section(self, addr: int, payload: bytes):
        slot = next(i for i in range(NUM_TEXT) if self.size[i] == 0)
        # sections are 32-byte aligned in the file
        start = (len(self.data) + 31) & ~31
        payload = payload + b"\0" * (-len(payload) % 32)
        self.data += b"\0" * (start - len(self.data)) + payload
        self.off[slot], self.addr[slot], self.size[slot] = start, addr, len(payload)
        struct.pack_into(">18I", self.data, 0x00, *self.off)
        struct.pack_into(">18I", self.data, 0x48, *self.addr)
        struct.pack_into(">18I", self.data, 0x90, *self.size)


def patch_dol(clean: bytes, gecko_text: str) -> bytes:
    """main.dol with the Gecko code built in: 04 writes in place, C2 hooks moved into a new text section."""
    dol = Dol(clean)
    writes, inserts = parse_gecko(gecko_text)
    if any(dol.size[i] and dol.addr[i] == PAYLOAD_ADDR for i in range(NUM_SECTIONS)):
        raise PatchError("This main.dol is already patched")
    for addr, value in writes:
        dol.write32(addr, value)
    payload = bytearray()
    for addr, words in inserts:
        start = PAYLOAD_ADDR + len(payload)
        # like the Gecko handler: the C2 block ends with the original instruction then a 0 word,
        # which becomes the branch back to the instruction after the hook
        assert words[-1] == 0, "C2 block should end with a 00000000 placeholder"
        original = dol.read32(addr)
        if words[-2] != original:
            raise PatchError(f"main.dol at {addr:#x} isn't what the loader expects: wrong game version?")
        words[-1] = branch(start + (len(words) - 1) * 4, addr + 4)
        payload += struct.pack(f">{len(words)}I", *words)
        dol.write32(addr, branch(addr, start))
    dol.add_text_section(PAYLOAD_ADDR, bytes(payload))
    return bytes(dol.data)


# wicon.tpl / wicon.bin: adding icons
#
# wicon.tpl is a TPL: header (magic, image count, image table offset), a table of (image header,
# palette header) offsets, image headers and texture data. To add images without moving any existing
# data, the new image table (the old entries plus the new ones) goes at the end of the file, followed by
# the new images' headers and data, and the header points at the new table.
#
# wicon.bin: u16 icon count, then a u16 offset per icon, then each icon's record: 6 zero bytes, u16
# frame count, u16 texture index per frame, u16 0. Icon ids (the item table's iconId) index this list.

FORMAT_RGB5A3 = 5


def _align(data: bytearray, n: int):
    data += b"\0" * (-len(data) % n)


def add_textures(tpl_data: bytes, images) -> tuple[bytes, list[int]]:
    """images: [(width, height, RGB5A3 data)]. Returns the new TPL and the new texture indices."""
    out = bytearray(tpl_data)
    magic, count, table = struct.unpack_from(">III", out, 0)
    entries = [struct.unpack_from(">II", out, table + 8 * i) for i in range(count)]
    # a copy of an existing image header, for the fields we don't change (wrap, filters, LOD)
    template = bytes(out[entries[0][0]:entries[0][0] + 0x24])
    _align(out, 32)
    new_table = len(out)
    out += b"\0" * (8 * (count + len(images)))
    _align(out, 32)
    headers = len(out)
    out += b"\0" * (0x24 * len(images))
    for i, (width, height, pixels) in enumerate(images):
        _align(out, 32)
        data_offset = len(out)
        out += pixels
        header = bytearray(template)
        struct.pack_into(">HHII", header, 0, height, width, FORMAT_RGB5A3, data_offset)
        out[headers + 0x24 * i:headers + 0x24 * (i + 1)] = header
        entries.append((headers + 0x24 * i, 0))
    for i, entry in enumerate(entries):
        struct.pack_into(">II", out, new_table + 8 * i, *entry)
    struct.pack_into(">III", out, 0, magic, len(entries), new_table)
    return bytes(out), list(range(count, len(entries)))


def add_icons(bin_data: bytes, textures: list[int]) -> tuple[bytes, list[int]]:
    """Returns the new wicon.bin and the icon ids of single-frame icons using those textures."""
    count = struct.unpack_from(">H", bin_data, 0)[0]
    offsets = struct.unpack_from(f">{count}H", bin_data, 2)
    ends = list(offsets[1:]) + [len(bin_data)]
    records = [bin_data[o:e] for o, e in zip(offsets, ends)]
    records += [b"\0" * 6 + struct.pack(">HHH", 1, t, 0) for t in textures]
    out = bytearray(struct.pack(">H", len(records)))
    pos = 2 + 2 * len(records)
    for r in records:
        out += struct.pack(">H", pos)
        pos += len(r)
    for r in records:
        out += r
    assert len(out) < 0x10000, "wicon.bin offsets are 16-bit"
    return bytes(out), list(range(count, len(records)))


def ap_icon_images():
    """The Archipelago icons, pre-encoded by tools/wicon.py: [(width, height, RGB5A3 data)]."""
    data = bundled("ap_icons.bin")
    images, pos = [], 0
    while pos < len(data):
        width, height = struct.unpack_from(">HH", data, pos)
        size = ((width + 3) // 4 * 4) * ((height + 3) // 4 * 4) * 2
        images.append((width, height, data[pos + 4:pos + 4 + size]))
        pos += 4 + size
    return images


def patch_icons(tpl_data: bytes, bin_data: bytes) -> tuple[bytes, bytes, list[int]]:
    tpl_out, textures = add_textures(tpl_data, ap_icon_images())
    bin_out, icons = add_icons(bin_data, textures)
    return tpl_out, bin_out, icons


# The whole folder

def find_game_folder(path: str) -> str:
    """The folder holding sys/ and files/: the one picked, or its DATA/ partition (Dolphin's layout)."""
    for candidate in (path, os.path.join(path, "DATA")):
        if os.path.isfile(os.path.join(candidate, "sys", "main.dol")) and os.path.isdir(os.path.join(candidate, "files")):
            return candidate
    raise PatchError("That folder doesn't look like an extracted disc: there's no sys/main.dol and files/ in it "
                     "(or in its DATA folder).")


def _original(root: str, rel: str) -> bytes:
    """The clean file: the .orig backup if we've patched before, else the file itself (backed up now)."""
    path = os.path.join(root, *rel.split("/"))
    backup = path + ".orig"
    if not os.path.exists(backup):
        data = open(path, "rb").read()
        if hashlib.sha1(data).hexdigest() != CLEAN_SHA1[rel]:
            raise PatchError(f"{rel} isn't the original US 1.0 file. Extract the disc again from a clean "
                             f"Super Paper Mario (USA, revision 0) image.")
        shutil.copyfile(path, backup)
    data = open(backup, "rb").read()
    if hashlib.sha1(data).hexdigest() != CLEAN_SHA1[rel]:
        raise PatchError(f"{rel}.orig has been changed; extract the disc again.")
    return data


def patch_folder(path: str) -> str:
    """Patches an extracted disc in place. Returns the main.dol to boot."""
    root = find_game_folder(path)
    boot = open(os.path.join(root, "sys", "boot.bin"), "rb").read(8)
    if boot[:6] != GAME_ID or boot[7] != REVISION:
        raise PatchError(f"This is {boot[:6].decode(errors='replace')} revision {boot[7]}; the prototype needs "
                         f"Super Paper Mario USA revision 0 ({GAME_ID.decode()}, rev {REVISION}).")

    dol = patch_dol(_original(root, "sys/main.dol"), bundled("loader_us0.txt").decode())
    tpl, icon_bin, _ = patch_icons(_original(root, "files/wicon.tpl"), _original(root, "files/wicon.bin"))

    open(os.path.join(root, "sys", "main.dol"), "wb").write(dol)
    open(os.path.join(root, "files", "wicon.tpl"), "wb").write(tpl)
    open(os.path.join(root, "files", "wicon.bin"), "wb").write(icon_bin)
    os.makedirs(os.path.join(root, "files", "mod"), exist_ok=True)
    open(os.path.join(root, "files", "mod", "mod.rel"), "wb").write(bundled("mod.rel"))
    return os.path.join(root, "sys", "main.dol")


def launch_patcher(*args):
    """Launcher component: pick the extracted disc folder and patch it."""
    import Utils
    path = args[0] if args else Utils.open_directory("Pick the extracted Super Paper Mario (USA) disc folder")
    if not path:
        return
    try:
        dol = patch_folder(path)
    except (PatchError, OSError) as e:
        Utils.messagebox("Super Paper Mario Patcher", str(e), error=True)
        return
    Utils.messagebox("Super Paper Mario Patcher",
                     f"Done! In Dolphin, open:\n{dol}\n\nThen start the Super Paper Mario Client and connect.")
