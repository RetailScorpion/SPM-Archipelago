"""Adds icons to SPM's item icon files (files/wicon.tpl textures, files/wicon.bin icon list).

wicon.tpl is a TPL: header (magic, image count, image table offset), a table of (image header, palette
header) offsets, image headers and texture data. To add images without moving any existing data, the
new image table (the old entries plus the new ones) goes at the end of the file, followed by the new
images' headers and data, and the header points at the new table.

wicon.bin: u16 icon count, then a u16 offset per icon, then each icon's record:
6 zero bytes, u16 frame count, u16 texture index per frame, u16 0 (seen on every single-frame icon).
Icon ids (the item table's iconId) index this list, so new icons get ids from the old count up.

Usage: py -3 -E tools/wicon.py <clean files dir> <out files dir>   (adds the AP icons from assets/icons)
"""

import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tpl  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AP_ICONS = [os.path.join(ROOT, "assets", "icons", "ap_progression.png"),
            os.path.join(ROOT, "assets", "icons", "ap_filler.png")]

FORMAT_RGB5A3 = 5


def align(data: bytearray, n: int):
    data += b"\0" * (-len(data) % n)


def add_textures(tpl_data: bytes, images) -> tuple[bytes, list[int]]:
    """Returns the new TPL and the texture indices of the added images (RGBA PIL images)."""
    out = bytearray(tpl_data)
    magic, count, table = struct.unpack_from(">III", out, 0)
    entries = [struct.unpack_from(">II", out, table + 8 * i) for i in range(count)]
    # a copy of an existing image header, for the fields we don't change (wrap, filters, LOD)
    template = bytes(out[entries[0][0]:entries[0][0] + 0x24])

    align(out, 32)
    new_table = len(out)
    out += b"\0" * (8 * (count + len(images)))
    align(out, 32)
    headers = len(out)
    out += b"\0" * (0x24 * len(images))
    for i, im in enumerate(images):
        align(out, 32)
        data_offset = len(out)
        out += tpl.encode_rgb5a3(im)
        header = bytearray(template)
        struct.pack_into(">HHII", header, 0, im.size[1], im.size[0], FORMAT_RGB5A3, data_offset)
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


def build(clean_dir: str, out_dir: str, pngs=AP_ICONS) -> list[int]:
    """Writes wicon.tpl/wicon.bin with the icons added to out_dir; returns the new icon ids."""
    images = [Image.open(p).convert("RGBA") for p in pngs]
    tpl_data, textures = add_textures(open(os.path.join(clean_dir, "wicon.tpl"), "rb").read(), images)
    bin_data, icons = add_icons(open(os.path.join(clean_dir, "wicon.bin"), "rb").read(), textures)
    open(os.path.join(out_dir, "wicon.tpl"), "wb").write(tpl_data)
    open(os.path.join(out_dir, "wicon.bin"), "wb").write(bin_data)
    return icons


if __name__ == "__main__":
    ids = build(sys.argv[1], sys.argv[2])
    print("Added icons", ", ".join(map(str, ids)))
