"""Builds the release zips.

SuperPaperMario_AP_<version>.zip: the APworld, the YAML template, the README and the license.
SPM_helpful_tools_<version>.zip: the tracker, coords and SPM Archipelago Tools windows, with the files
they need, laid out like the repo so their relative paths work.

Usage: py -3 -E tools/make_release.py      (build the mod first: configure.py + ninja in mod/)
Writes both to build/release/
"""

import json
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TOOLS_FILES = [
    "client/tracker.py",
    "client/coords.py",
    "client/ap_tools.py",
    "client/theme.py",
    "client/mailbox.py",
    "apworld/spm/game_link.py",
    "data/ap_locations.json",
    "data/item_names_us0.json",
    "LICENSE",
]

TOOLS_README = """SPM helpful tools
=================

Three small windows that read Super Paper Mario live from Dolphin, for use with the
Super Paper Mario Archipelago randomizer.

  SPM Tracker            client/tracker.py    Every location by level and map, checked or not.
                                              Add --missing to hide the checked ones.
  SPM Coords             client/coords.py     Mario's position, the map, and that map's
                                              unchecked locations (always on top).
  SPM Archipelago Tools  client/ap_tools.py   Add or remove characters, Pixls and the Save
                                              Block, set coins, and swap key items between
                                              your 32 slots and the overflow storage.

Needs Python 3 (python.org) and dolphin-memory-engine:

    py -3 -m pip install dolphin-memory-engine

Start Dolphin with the game first, then run a tool, for example:

    pyw -3 client\tracker.py --missing

The tracker and coords only read from the game. SPM Archipelago Tools writes to it (party,
coins, key items), so use it with care. Keep the folders as they are: the tools find their
data through the relative paths.
"""


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

    tools = os.path.join(out_dir, f"SPM_helpful_tools_{version}.zip")
    with zipfile.ZipFile(tools, "w", zipfile.ZIP_DEFLATED) as z:
        for path in TOOLS_FILES:
            z.write(os.path.join(ROOT, path), "SPM_helpful_tools/" + path)
        z.writestr("SPM_helpful_tools/README.txt", TOOLS_README)
    print(f"Wrote {tools}")


if __name__ == "__main__":
    main()
