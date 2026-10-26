"""Fails if the ELF calls or uses a game symbol that isn't in the merged lst. elf2rel doesn't complain:
an unknown symbol silently becomes a branch to itself, which hangs the game when reached.

Usage: py -3 -E check_syms.py <nm> <elf> <lst> <stamp>
"""

import subprocess
import sys


def main(nm, elf, lst, stamp):
    known = set()
    for line in open(lst):
        line = line.strip()
        if line and not line.startswith("//") and ":" in line:
            known.add(line.rsplit(":", 1)[1])
    out = subprocess.run([nm, "-u", elf], capture_output=True, text=True, check=True).stdout
    missing = sorted(name for name in (l.split()[-1] for l in out.splitlines() if l.strip()) if name not in known)
    if missing:
        sys.exit("Symbols missing from the lst (add them to symbols/): " + ", ".join(missing))
    open(stamp, "w").close()


if __name__ == "__main__":
    main(*sys.argv[1:])
