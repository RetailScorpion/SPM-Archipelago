"""evt script disassembly for scripts inside main.dol (raw words; pointers named from the symbol map).

Usage: py -3 -E dolevt.py <address> [count]
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dolasm import Dol, Symbols  # noqa: E402
from evtdis import END_OPS, FLOAT_BASE, VAR_BASES, load_opcodes  # noqa: E402


def main():
    addr = int(sys.argv[1], 0)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    dol, ops, syms = Dol(), load_opcodes(), Symbols()

    def arg(v):
        u = v & 0xFFFFFFFF
        if 0x80000000 <= u < 0x81800000:
            name = syms.name(u)
            if name and "+" not in name:
                return name
            raw = dol.read(u, 48)
            if raw:
                s = raw.split(b"\0")[0]
                if 0 < len(s) and all(32 <= c < 127 for c in s):
                    return f'"{s.decode()}"'
            return f"{u:#010x}"
        for base, name in VAR_BASES:
            if base <= v < base + 10000000 and v - base < 10000:
                return f"{name}({v - base})"
        if -250000000 < v < -220000000:
            return f"{(v - FLOAT_BASE) / 1024.0:g}f"
        return str(v)

    depth = 0
    for _ in range(count):
        hdr = struct.unpack(">I", dol.read(addr, 4))[0]
        op, n = hdr & 0xFFFF, hdr >> 16
        name = ops.get(op, f"OP_{op}")
        args = [arg(x) for x in struct.unpack(f">{n}i", dol.read(addr + 4, 4 * n))] if n else []
        if name in ("ELSE", "END_IF", "END_SWITCH", "WHILE", "CASE_END", "END_INLINE", "END_BROTHER") \
                or name.startswith("CASE_"):
            depth = max(0, depth - 1)
        print(f"{addr:#010x}  {'  ' * depth}{name}({', '.join(args)})")
        if name.startswith("IF") or name in ("ELSE", "DO", "SWITCH", "SWITCHI", "INLINE_EVT", "INLINE_EVT_ID",
                                             "BROTHER_EVT", "BROTHER_EVT_ID") or name.startswith("CASE_"):
            depth += 1
        addr += 4 + 4 * n
        if op in END_OPS and depth == 0:
            break


if __name__ == "__main__":
    main()
