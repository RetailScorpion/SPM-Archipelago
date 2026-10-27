"""Builds the release zip: the APworld, the YAML template, the README and the license.

Usage: py -3 -E tools/make_release.py      (build the mod first: configure.py + ninja in mod/)
Writes build/release/SuperPaperMario_AP_<version>.zip
"""

import json
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    subprocess.run([sys.executable, "-E", os.path.join(ROOT, "tools", "build_apworld.py")], check=True)
    version = json.load(open(os.path.join(ROOT, "apworld", "spm", "archipelago.json")))["world_version"]
    out_dir = os.path.join(ROOT, "build", "release")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, f"SuperPaperMario_AP_{version}.zip")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(ROOT, "build", "spm.apworld"), "spm.apworld")
        z.write(os.path.join(ROOT, "release", "Super Paper Mario.yaml"), "Super Paper Mario.yaml")
        z.write(os.path.join(ROOT, "README.md"), "README.md")
        z.write(os.path.join(ROOT, "LICENSE"), "LICENSE")
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
