"""The Super Paper Mario Patcher: turns an extracted game (US 1.0, R8PE01 revision 0) into the
Archipelago version.

It changes the extracted folder in place. Before changing a file it keeps the original next to it as
<name>.orig, and it always starts from those originals, so running it again (after updating the
APworld, say) applies the new version cleanly. It changes:
  - sys/main.dol: the game's main program. SeekyCt's rel loader (spm-rel-loader, GPLv3) is built
    into it, which loads the mod when the game boots
  - files/mod/mod.rel: the Archipelago mod itself (added)
  - files/wicon.tpl, files/wicon.bin: the item pictures, with the Archipelago icons, the Save
    Block's and the title banner added
  - files/win/us/pause.tpl: the pause menu pictures, where the unused Maps button becomes Options

It uses only Python's standard library, so it runs inside the Archipelago Launcher. Dolphin then
boots the game from sys/main.dol.
"""

import hashlib
import json
import os
import pkgutil
import shutil
import struct

GAME_ID = b"R8PE01"
REVISION = 0
# A fingerprint (SHA-1) of each original US 1.0 file the patcher changes, to make sure it starts from
# the right game version
CLEAN_SHA1 = {
    "sys/main.dol": "4669c63d8365dfdced78e5c1518fe2a3f915a0c6",
    "files/wicon.tpl": "fc363d4a7c689d82d9a3152676c8ec1daff52e9a",
    "files/wicon.bin": "a200e7f8df9270532b9033bec2a123f2abb337e9",
    "files/win/us/pause.tpl": "bf92332ad8d2ffbc167180eb44564d7eb87c73b9",
}


class PatchError(Exception):
    """Something is wrong with the folder or its files; the message says what."""


def bundled(name: str) -> bytes:
    """A file from the APworld's bin/ folder (works from the .apworld zip too)."""
    data = pkgutil.get_data(__name__, f"bin/{name}")
    if data is None:
        raise PatchError(f"bin/{name} is missing from the APworld")
    return data


def apworld_version() -> str:
    """The APworld's version (world_version in its archipelago.json), or "unknown" if it can't be read."""
    try:
        return json.loads(pkgutil.get_data(__name__, "archipelago.json"))["world_version"]
    except (OSError, TypeError, ValueError, KeyError):
        return "unknown"


# ---- main.dol: building in the rel loader ----
#
# The rel loader is published as a "Gecko code": a cheat-code list of memory changes, normally applied
# by a cheat engine while the game runs. The patcher applies the same changes to main.dol itself, so
# no cheat engine is needed. The loader uses two kinds of Gecko line:
#   - 04: write one 4-byte value at an address
#   - C2: run a block of extra code at an address ("insert"). The patcher puts all such blocks in a new
#     section of main.dol at PAYLOAD_ADDR, jumps there from the address, and jumps back afterwards.
#
# main.dol's layout: a header with up to 7 code ("text") and 11 data sections, each with a position
# in the file, an address in memory and a size.

# Where the inserted code goes: free low memory. Not 0x80001800 itself: Dolphin writes a "STUBHAXX"
# marker at 0x80001804 when booting. Not 0x80001800 itself: Dolphin writes a "STUBHAXX" marker at 0x80001804 when booting.
# The whole 0x80001800-0x80003000 area is also where a Gecko codehandler goes, so the patched game
# can't be combined with Gecko codes in Dolphin.
PAYLOAD_ADDR = 0x80001900
NUM_TEXT = 7
NUM_SECTIONS = 18


def parse_gecko(text: str):
    """Reads the loader's Gecko code. Returns (writes, inserts): [(address, value)] and
    [(address, [code words])]."""
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
    """The PowerPC instruction at address src that jumps to dst."""
    return 0x48000000 | ((dst - src) & 0x03FFFFFC)


class Dol:
    """main.dol in memory, with its section table, so addresses can be read and written."""

    def __init__(self, data: bytes):
        self.data = bytearray(data)
        self.off = list(struct.unpack(">18I", data[0:0x48]))
        self.addr = list(struct.unpack(">18I", data[0x48:0x90]))
        self.size = list(struct.unpack(">18I", data[0x90:0xD8]))

    def file_offset(self, addr: int) -> int:
        """Where in the file a memory address is."""
        for i in range(NUM_SECTIONS):
            if self.size[i] and self.addr[i] <= addr < self.addr[i] + self.size[i]:
                return self.off[i] + addr - self.addr[i]
        raise ValueError(f"{addr:#x} isn't in any dol section")

    def read32(self, addr: int) -> int:
        return struct.unpack_from(">I", self.data, self.file_offset(addr))[0]

    def write32(self, addr: int, value: int):
        struct.pack_into(">I", self.data, self.file_offset(addr), value)

    def add_text_section(self, addr: int, payload: bytes):
        """Adds a code section loaded at `addr`, at the end of the file."""
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
        # As a cheat engine does it: the C2 block ends with the original instruction, then a 0 word that
        # becomes the jump back to the instruction after the hooked one
        assert words[-1] == 0, "C2 block should end with a 00000000 placeholder"
        original = dol.read32(addr)
        if words[-2] != original:
            raise PatchError(f"main.dol at {addr:#x} isn't what the loader expects: wrong game version?")
        words[-1] = branch(start + (len(words) - 1) * 4, addr + 4)
        payload += struct.pack(f">{len(words)}I", *words)
        dol.write32(addr, branch(addr, start))
    dol.add_text_section(PAYLOAD_ADDR, bytes(payload))
    return bytes(dol.data)


# ---- wicon.tpl / wicon.bin: adding item pictures ----
#
# wicon.tpl holds the pictures ("textures"). It's a TPL file: a header (a magic number, the picture
# count, where the picture list is), the list (for each picture: where its header and colour palette
# are), then the headers and pixel data. To add pictures without moving anything already there, a new
# list (the old entries plus the new ones) goes at the end of the file, then the new pictures, and the
# header points at the new list.
#
# wicon.bin says which pictures make up each icon: a 2-byte icon count, a 2-byte position per icon,
# then each icon's record: 6 zero bytes, a 2-byte frame count, a 2-byte picture number per frame, and
# a 2-byte 0. An item's icon number (iconId in the item table) is its position in this list.
#
# Icon 403 is reserved: the game checks for it in its icon drawing (iconDispGx2, `cmplwi 0x193;
# beq`) and elsewhere, and draws the missing-texture image for it. The game has icons 0-401, so the
# second icon added would be 403: add_icons puts a placeholder there and carries on at 404.
RESERVED_ICON_IDS = {403}

# The pixel format of the added pictures: RGB5A3, 16 bits per pixel (5 bits each of red, green and
# blue, or 4 each plus 3 bits of transparency)
FORMAT_RGB5A3 = 5


def _align(data: bytearray, n: int):
    """Pads data with zeros to a multiple of n bytes."""
    data += b"\0" * (-len(data) % n)


def add_textures(tpl_data: bytes, images) -> tuple[bytes, list[int]]:
    """images: [(width, height, RGB5A3 data)]. Returns the new TPL and the new texture indices."""
    out = bytearray(tpl_data)
    magic, count, table = struct.unpack_from(">III", out, 0)
    entries = [struct.unpack_from(">II", out, table + 8 * i) for i in range(count)]
    # A copy of an existing picture header, for the settings that stay the same (wrapping, filtering)
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
    ids = []
    for t in textures:
        while len(records) in RESERVED_ICON_IDS:
            records.append(b"\0" * 6 + struct.pack(">HHH", 1, t, 0))  # placeholder, never used
        ids.append(len(records))
        records.append(b"\0" * 6 + struct.pack(">HHH", 1, t, 0))
    out = bytearray(struct.pack(">H", len(records)))
    pos = 2 + 2 * len(records)
    for r in records:
        out += struct.pack(">H", pos)
        pos += len(r)
    for r in records:
        out += r
    assert len(out) < 0x10000, "wicon.bin offsets are 16-bit"
    return bytes(out), ids


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
    """Adds the bundled icons to wicon.tpl and wicon.bin. Returns both files and the new icon numbers."""
    tpl_out, textures = add_textures(tpl_data, ap_icon_images())
    bin_out, icons = add_icons(bin_data, textures)
    return tpl_out, bin_out, icons


# ---- pause.tpl: the Options button ----

# The pause menu's Maps button (picture 7, 120x40 pixels, RGB5A3) is never shown in the US game; the
# mod shows it as Options (mod options_menu.cpp) with RetailScorpion's picture
# (assets/pause/options_button.png, converted by tools/build_apworld.py)
OPTIONS_TEXTURE = 7


def patch_pause(tpl_data: bytes, button: bytes) -> bytes:
    """Writes the Options button picture over the Maps button's in pause.tpl."""
    out = bytearray(tpl_data)
    _, _, table = struct.unpack(">III", out[:12])
    header, _ = struct.unpack(">II", out[table + 8 * OPTIONS_TEXTURE:table + 8 * OPTIONS_TEXTURE + 8])
    height, width, fmt, data = struct.unpack(">HHII", out[header:header + 12])
    if (width, height, fmt) != (120, 40, 5) or len(button) != width * height * 2:
        raise PatchError("The Options button doesn't fit the Maps button's texture")
    out[data:data + len(button)] = button
    return bytes(out)


# ---- The whole folder ----

def find_game_folder(path: str) -> str:
    """The folder holding sys/ and files/: the one picked, or the DATA folder inside it (Dolphin
    extracts discs that way)."""
    for candidate in (path, os.path.join(path, "DATA")):
        if os.path.isfile(os.path.join(candidate, "sys", "main.dol")) and os.path.isdir(os.path.join(candidate, "files")):
            return candidate
    raise PatchError("That folder doesn't look like an extracted disc: there's no sys/main.dol and files/ in it "
                     "(or in its DATA folder).")


def _original(root: str, rel: str) -> bytes:
    """A game file as it was before any patching: its .orig copy if the folder was patched before,
    otherwise the file itself (copied to .orig now). Checks its fingerprint either way."""
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
    pause = patch_pause(_original(root, "files/win/us/pause.tpl"), bundled("options_button.bin"))

    open(os.path.join(root, "sys", "main.dol"), "wb").write(dol)
    open(os.path.join(root, "files", "wicon.tpl"), "wb").write(tpl)
    open(os.path.join(root, "files", "wicon.bin"), "wb").write(icon_bin)
    open(os.path.join(root, "files", "win", "us", "pause.tpl"), "wb").write(pause)
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
                     f"Done! You are now running version {apworld_version()}.\n\n"
                     f"In Dolphin, open:\n{dol}\n\nThen start the Super Paper Mario Client and connect.")
