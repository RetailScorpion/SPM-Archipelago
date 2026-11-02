"""Encodes the Archipelago icons and the title screen pictures for SPM's icon files, and adds
them to a folder's wicon files.

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
            os.path.join(ROOT, "assets", "icons", "ap_filler.png"),
            os.path.join(ROOT, "assets", "icons", "ap_blank.png"),  # traps: Mario holds up nothing
            os.path.join(ROOT, "assets", "icons", "save_block.png"),  # from the save block's textures
            os.path.join(ROOT, "assets", "title", "archipelago_banner.png"),  # the title screen's banner
            os.path.join(ROOT, "assets", "title", "archipelago_flower.png")]  # and the flower behind the title
# Pictures drawn at another size than their PNG: each is cropped to what it shows, then shrunk to
# this width, the size it's drawn at (the title logo is 380 wide)
ICON_WIDTHS = {"archipelago_banner.png": 320, "archipelago_flower.png": 360}
OUT_ICONS = os.path.join(ROOT, "apworld", "spm", "bin", "ap_icons.bin")


def encode_icons(pngs=AP_ICONS) -> bytes:
    """The icon pictures, converted for the patcher: width, height and RGB5A3 data each."""
    out = bytearray()
    for p in pngs:
        im = Image.open(p).convert("RGBA")
        width = ICON_WIDTHS.get(os.path.basename(p))
        if width:
            im = im.crop(im.getchannel("A").getbbox())
            im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        out += struct.pack(">HH", *im.size) + tpl.encode_rgb5a3(im)
    return bytes(out)


def write_icons():
    """Writes the converted icons to the APworld's bin/ap_icons.bin."""
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
