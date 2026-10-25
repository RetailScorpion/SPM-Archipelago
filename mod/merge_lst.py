"""Concatenates symbol lists for elf2rel, failing if a later list redefines a symbol from an earlier one.
(Duplicates within one list are left alone; spm-headers' own lst has a few.)

Usage: py -3 -E merge_lst.py <out> <in.lst>...
"""

import sys


def main(out, inputs):
    seen = {}  # name -> (file, line)
    lines = []
    for path in inputs:
        for line in open(path):
            stripped = line.strip()
            if stripped and not stripped.startswith("//") and ":" in stripped:
                name = stripped.rsplit(":", 1)[1]
                if name in seen and seen[name][0] != path and seen[name][1] != stripped:
                    sys.exit(f"{path}: {name} already defined in {seen[name][0]} as {seen[name][1]}")
                seen.setdefault(name, (path, stripped))
            lines.append(line.rstrip("\n"))
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
