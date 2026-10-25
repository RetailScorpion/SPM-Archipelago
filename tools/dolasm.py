"""PowerPC disassembly of main.dol functions, with US 1.0 symbol names on calls.

Gekko paired-single instructions aren't supported by Capstone; they show as ".byte".

Usage: py -3 -E dolasm.py <address> [length]
"""

import bisect
import os
import struct
import sys

from capstone import CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN, Cs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOL = os.path.join(ROOT, "disc", "DATA", "sys", "main.dol")
LST = os.path.join(ROOT, "mod", "spm-headers", "linker", "spm.us0.lst")


class Dol:
    def __init__(self, path=DOL):
        self.data = open(path, "rb").read()
        self.off = struct.unpack(">18I", self.data[0:0x48])
        self.addr = struct.unpack(">18I", self.data[0x48:0x90])
        self.size = struct.unpack(">18I", self.data[0x90:0xD8])

    def read(self, addr, n):
        for i in range(18):
            if self.size[i] and self.addr[i] <= addr < self.addr[i] + self.size[i]:
                o = self.off[i] + addr - self.addr[i]
                return self.data[o:o + n]
        raise ValueError(f"{addr:#x} not in dol")


class Symbols:
    def __init__(self, path=LST):
        syms = []
        for line in open(path):
            line = line.strip()
            if ":" in line and not line.startswith("//") and "," not in line.split(":")[0]:
                a, n = line.rsplit(":", 1)
                try:
                    syms.append((int(a, 16), n))
                except ValueError:
                    pass
        syms.sort()
        self.syms, self.keys = syms, [s[0] for s in syms]

    def name(self, addr):
        i = bisect.bisect_right(self.keys, addr) - 1
        if i < 0:
            return f"{addr:#x}"
        base, n = self.syms[i]
        return n if base == addr else f"{n}+{addr - base:#x}"


def disassemble(dol, syms, start, length):
    md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    for ins in md.disasm(dol.read(start, length), start):
        text = f"{ins.mnemonic} {ins.op_str}"
        if ins.mnemonic in ("bl", "b") and ins.op_str.startswith("0x"):
            text = f"{ins.mnemonic} {syms.name(int(ins.op_str, 16))}"
        yield ins.address, text


if __name__ == "__main__":
    start = int(sys.argv[1], 0)
    length = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x100
    for a, t in disassemble(Dol(), Symbols(), start, length):
        print(f"{a:#010x}  {t}")
