"""Builds/updates the development disc folder that Dolphin boots directly (no ISO rebuild).

  build/devdisc/   copy of the extracted game partition (disc/DATA), made once
    sys/main.dol   clean dol + rel-loader (tools/patch_dol.py)
    files/mod/mod.rel   the mod (mod/out/us0.rel)

Usage: py -3 -E tools/make_dev_disc.py            (from the SPM_AP folder)
Then in Dolphin: File > Open > build/devdisc/sys/main.dol
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLEAN = os.path.join(ROOT, "disc", "DATA")
DEV = os.path.join(ROOT, "build", "devdisc")
LOADER = os.path.join(ROOT, "ref", "spm-rel-loader", "spm-rel-loader", "loader", "loader us0.txt")
REL = os.path.join(ROOT, "mod", "out", "us0.rel")


def main():
    if not os.path.isdir(DEV):
        print("Copying the extracted disc (one time)...")
        shutil.copytree(CLEAN, DEV)

    dol = os.path.join(DEV, "sys", "main.dol")
    os.remove(dol)
    subprocess.run([sys.executable, "-E", os.path.join(ROOT, "tools", "patch_dol.py"),
                    os.path.join(CLEAN, "sys", "main.dol"), LOADER, dol], check=True)

    if not os.path.exists(REL):
        sys.exit(f"Mod not built yet: {REL} is missing (run configure.py and ninja in mod/)")
    os.makedirs(os.path.join(DEV, "files", "mod"), exist_ok=True)
    shutil.copyfile(REL, os.path.join(DEV, "files", "mod", "mod.rel"))
    print(f"Dev disc ready: {dol}")


if __name__ == "__main__":
    main()
