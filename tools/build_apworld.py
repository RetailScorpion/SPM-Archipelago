"""Zips apworld/spm into build/spm.apworld (regenerating data.py and the patcher's bin/ files first).

Usage: py -3 -E tools/build_apworld.py [--install]   (--install copies it to Archipelago's custom_worlds)
"""

import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "apworld", "spm")
OUT = os.path.join(ROOT, "build", "spm.apworld")
CUSTOM_WORLDS = r"D:\Tools\Archipelago\custom_worlds"


MOD_REL = os.path.join(ROOT, "mod", "out", "us0.rel")
LOADER = os.path.join(ROOT, "ref", "spm-rel-loader", "spm-rel-loader", "loader", "loader us0.txt")
BIN = os.path.join(SRC, "bin")


def main():
    subprocess.run([sys.executable, "-E", os.path.join(ROOT, "tools", "gen_ap_data.py")], check=True)
    # What the patcher bundles: the built mod, the rel loader's Gecko code and the encoded AP icons
    if not os.path.exists(MOD_REL):
        sys.exit(f"Build the mod first: {MOD_REL} is missing")
    os.makedirs(BIN, exist_ok=True)
    shutil.copyfile(MOD_REL, os.path.join(BIN, "mod.rel"))
    shutil.copyfile(LOADER, os.path.join(BIN, "loader_us0.txt"))
    subprocess.run([sys.executable, "-E", os.path.join(ROOT, "tools", "wicon.py")], check=True)
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        for folder, dirs, files in os.walk(SRC):
            dirs[:] = [d for d in dirs if d != "__pycache__"]
            for f in files:
                path = os.path.join(folder, f)
                z.write(path, os.path.join("spm", os.path.relpath(path, SRC)))
    print(f"Wrote {OUT}")
    if "--install" in sys.argv:
        shutil.copyfile(OUT, os.path.join(CUSTOM_WORLDS, "spm.apworld"))
        print(f"Installed to {CUSTOM_WORLDS}")


if __name__ == "__main__":
    main()
