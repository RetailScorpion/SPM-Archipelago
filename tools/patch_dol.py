"""Builds the spm-rel-loader Gecko code into main.dol, so the patched disc needs no cheat codes.
The code is apworld/spm/patcher.py's (shared with the Launcher's patcher).

Usage: py -3 -E patch_dol.py <clean main.dol> <loader usX.txt> <out main.dol>
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "apworld", "spm"))
import patcher  # noqa: E402


def main(dol_path, gecko_path, out_path):
    out = patcher.patch_dol(open(dol_path, "rb").read(), open(gecko_path).read())
    open(out_path, "wb").write(out)
    print(f"Patched main.dol, payload at {patcher.PAYLOAD_ADDR:#x}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
