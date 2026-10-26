"""Encodes the Archipelago icons for SPM's item icon files, and adds them to a folder's wicon files.

The patching itself lives in apworld/spm/patcher.py (so the Launcher's patcher needs no Pillow); this
tool encodes the PNGs to RGB5A3 for it:
  apworld/spm/bin/ap_icons.bin: for each icon, u16 width, u16 height, then its RGB5A3 texture data

Usage: py -3 -E tools/wicon.py [<clean files dir> <out files dir>]
  no arguments: just write ap_icons.bin; with folders: also write patched wicon.tpl/wicon.bin there
"""

import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "apworld", "spm"))
import patcher  # noqa: E402
import tpl  # noqa: E402

AP_ICONS = [os.path.join(ROOT, "assets", "icons", "ap_progression.png"),
            os.path.join(ROOT, "assets", "icons", "ap_filler.png")]
OUT_ICONS = os.path.join(ROOT, "apworld", "spm", "bin", "ap_icons.bin")


def encode_icons(pngs=AP_ICONS) -> bytes:
    out = bytearray()
    for p in pngs:
        im = Image.open(p).convert("RGBA")
        out += struct.pack(">HH", *im.size) + tpl.encode_rgb5a3(im)
    return bytes(out)


def write_icons():
    os.makedirs(os.path.dirname(OUT_ICONS), exist_ok=True)
    open(OUT_ICONS, "wb").write(encode_icons())


def build(clean_dir: str, out_dir: str) -> list[int]:
    """Writes wicon.tpl/wicon.bin with the AP icons added to out_dir; returns the new icon ids."""
    write_icons()
    tpl_data, bin_data, icons = patcher.patch_icons(open(os.path.join(clean_dir, "wicon.tpl"), "rb").read(),
                                                    open(os.path.join(clean_dir, "wicon.bin"), "rb").read())
    open(os.path.join(out_dir, "wicon.tpl"), "wb").write(tpl_data)
    open(os.path.join(out_dir, "wicon.bin"), "wb").write(bin_data)
    return icons


if __name__ == "__main__":
    if len(sys.argv) > 2:
        print("Added icons", ", ".join(map(str, build(sys.argv[1], sys.argv[2]))))
    else:
        write_icons()
        print(f"Wrote {OUT_ICONS}")
