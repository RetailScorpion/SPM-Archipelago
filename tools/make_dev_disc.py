"""Builds/updates the development disc folder that Dolphin boots directly (no ISO rebuild).

  build/devdisc/   copy of the extracted game partition (disc/DATA), made once
    sys/main.dol   clean dol + rel-loader (tools/patch_dol.py)
    files/mod/mod.rel   the mod (mod/out/us0.rel)
    files/wicon.tpl, files/wicon.bin   item icons plus the Archipelago icons (tools/wicon.py)
    files/win/us/pause.tpl   the Options button (assets/pause/options_button.png)
    files/mod/mod.rel   the mod, with its title screen text changed to DEV_TITLE

Usage: py -3 -E tools/make_dev_disc.py            (from the SPM_AP folder)
Then in Dolphin: File > Open > build/devdisc/sys/main.dol
"""

import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wicon  # noqa: E402

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "apworld", "spm"))
import patcher  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLEAN = os.path.join(ROOT, "disc", "DATA")
DEV = os.path.join(ROOT, "build", "devdisc")
LOADER = os.path.join(ROOT, "ref", "spm-rel-loader", "spm-rel-loader", "loader", "loader us0.txt")
REL = os.path.join(ROOT, "mod", "out", "us0.rel")
# The title screen text (sTitleText in mod/source/main.cpp, 48 bytes), and what the dev disc shows
# instead, so screenshots show which build they're from
TITLE_PREFIX = b"SPM Archipelago "
TITLE_SIZE = 48
DEV_TITLE = b"SPM Archipelago Dev Build"


def dev_title(rel: bytes) -> bytes:
    """The mod with its title screen text replaced by DEV_TITLE."""
    # The title text is the only "SPM Archipelago ..." string followed by zeros to TITLE_SIZE (the log
    # message starting the same way is followed by other text)
    for at in range(len(rel)):
        if rel.startswith(TITLE_PREFIX, at):
            text = rel[at:at + TITLE_SIZE].rstrip(b"\0")
            if b"\0" not in text and b"\n" not in text and rel[at + TITLE_SIZE - 1] == 0:
                return rel[:at] + DEV_TITLE.ljust(TITLE_SIZE, b"\0") + rel[at + TITLE_SIZE:]
    sys.exit("The mod's title text wasn't found (sTitleText in mod/source/main.cpp)")


def main():
    """Builds or updates build/devdisc: the patched game folder used for testing."""
    if not os.path.isdir(DEV):
        print("Copying the extracted disc (one time)...")
        shutil.copytree(CLEAN, DEV)

    dol = os.path.join(DEV, "sys", "main.dol")
    patched = patcher.patch_dol(open(os.path.join(CLEAN, "sys", "main.dol"), "rb").read(), open(LOADER).read())
    open(dol, "wb").write(patched)
    print(f"main.dol: rel loader added at {patcher.PAYLOAD_ADDR:#x}")

    icons = wicon.build(os.path.join(CLEAN, "files"), os.path.join(DEV, "files"))
    print("Archipelago icons added as icon ids", ", ".join(map(str, icons)))

    pause = os.path.join("files", "win", "us", "pause.tpl")
    from PIL import Image
    from tpl import encode_rgb5a3
    button = encode_rgb5a3(Image.open(os.path.join(ROOT, "assets", "pause", "options_button.png")))
    open(os.path.join(DEV, pause), "wb").write(patcher.patch_pause(open(os.path.join(CLEAN, pause), "rb").read(), button))
    print("Options button added")

    if not os.path.exists(REL):
        sys.exit(f"Mod not built yet: {REL} is missing (run configure.py and ninja in mod/)")
    os.makedirs(os.path.join(DEV, "files", "mod"), exist_ok=True)
    open(os.path.join(DEV, "files", "mod", "mod.rel"), "wb").write(dev_title(open(REL, "rb").read()))
    print(f"Dev disc ready: {dol}")


if __name__ == "__main__":
    main()
