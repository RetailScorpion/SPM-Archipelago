"""Zips apworld/spm into build/spm.apworld (regenerating data.py first).

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


def main():
    subprocess.run([sys.executable, "-E", os.path.join(ROOT, "tools", "gen_ap_data.py")], check=True)
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
