"""Disassembler for SPM evt scripts in relF.rel (and main.dol).

Pointers inside the rel are relocations, so they're resolved through the rel's relocation table:
calls into main.dol get symbol names from the spm-headers lst, pointers into the rel itself
show as rel:<section>+<offset> (or the string they point to).

Usage: py -3 -E evtdis.py <relF.rel> <file offset> [count]
       (file offset of the first instruction; disassembles until RETURN/END or count instructions)
"""

import struct
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from rel import Rel, R_PPC_ADDR32

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LST = os.path.join(ROOT, "mod", "spm-headers", "linker", "spm.us0.lst")
EVT_CMD_H = os.path.join(ROOT, "mod", "spm-headers", "mod", "evt_cmd.h")

VAR_BASES = [  # (base, name); id = value - base, ids count upwards towards 0
    (-30000000, "LW"), (-50000000, "GW"), (-70000000, "LF"), (-90000000, "GF"),
    (-110000000, "LSWF"), (-130000000, "GSWF"), (-150000000, "LSW"), (-170000000, "GSW"),
    (-190000000, "UW"), (-210000000, "UF"),
]
FLOAT_BASE = -240000000
END_OPS = {1, 2}  # END_SCRIPT, RETURN


def load_opcodes():
    import re
    ops, cur = {1: "END_SCRIPT"}, None
    for line in open(EVT_CMD_H):
        m = re.match(r"#define\s+(\w+)", line)
        if m:
            cur = m.group(1)
        m = re.search(r"EVT_HELPER_CMD\(\s*[^,]+,\s*(\d+)\)", line)
        if m and cur and cur != "EVT_HELPER_CMD":
            ops.setdefault(int(m.group(1)), cur)
    return ops


def load_symbols():
    syms = {}
    for line in open(LST):
        line = line.strip()
        if ":" in line and not line.startswith("//") and "," not in line.split(":")[0]:
            addr, name = line.rsplit(":", 1)
            try:
                syms[int(addr, 16)] = name
            except ValueError:
                pass
    return syms


class Disassembler:
    def __init__(self, rel_data, dol_data=None):
        self.data = rel_data
        self.rel = Rel(rel_data)
        self.ops = load_opcodes()
        self.syms = load_symbols()
        self.dol = dol_data
        # file offset -> (module, target section, target); ADDR32 only, which is how scripts store pointers
        self.relocs = {}
        for mod, _ in self.rel.imports():
            for sec, off, rtype, target_sec, addend in self.rel.relocs(mod):
                if rtype == R_PPC_ADDR32:
                    self.relocs[self.rel.file_offset(sec, off)] = (mod, target_sec, addend)

    def string_at(self, file_offset):
        end = self.data.find(b"\0", file_offset)
        s = self.data[file_offset:end]
        if 0 < len(s) < 64 and all(32 <= c < 127 for c in s):
            return s.decode()
        return None

    def dol_string(self, addr):
        if self.dol is None:
            return None
        off = struct.unpack(">18I", self.dol[0:0x48])
        base = struct.unpack(">18I", self.dol[0x48:0x90])
        size = struct.unpack(">18I", self.dol[0x90:0xD8])
        for i in range(18):
            if size[i] and base[i] <= addr < base[i] + size[i]:
                o = off[i] + addr - base[i]
                end = self.dol.find(b"\0", o)
                s = self.dol[o:end]
                if 0 < len(s) < 64 and all(32 <= c < 127 for c in s):
                    return s.decode()
        return None

    def arg(self, file_offset):
        value = struct.unpack_from(">i", self.data, file_offset)[0]
        if file_offset in self.relocs:
            mod, target_sec, target = self.relocs[file_offset]
            if mod == 0:
                if target in self.syms:
                    return self.syms[target]
                s = self.dol_string(target)
                return f'"{s}"' if s else f"{target:#010x}"
            # pointer into this rel
            fo = self.rel.file_offset(target_sec, target)
            s = self.string_at(fo)
            return f'"{s}"' if s else f"rel@{fo:#x}"
        for base, name in VAR_BASES:
            if base <= value < base + 10000000 and value - base < 10000:
                return f"{name}({value - base})"
        if -250000000 < value < -220000000:
            return f"{(value - FLOAT_BASE) / 1024.0:g}f"
        return str(value)

    def disassemble(self, start, count=10000, stop_at_end=True):
        pos, depth, lines = start, 0, []
        for _ in range(count):
            hdr = struct.unpack_from(">I", self.data, pos)[0]
            op, n = hdr & 0xFFFF, hdr >> 16
            name = self.ops.get(op, f"OP_{op}")
            args = [self.arg(pos + 4 + 4 * i) for i in range(n)]
            if name in ("ELSE", "END_IF", "END_SWITCH", "WHILE", "CASE_END", "END_INLINE", "END_BROTHER") \
                    or name.startswith("CASE_"):
                depth = max(0, depth - 1)
            lines.append(f"{pos:#08x}  {'  ' * depth}{name}({', '.join(args)})")
            if name.startswith("IF") or name in ("ELSE", "DO", "SWITCH", "SWITCHI", "INLINE_EVT", "INLINE_EVT_ID",
                                                 "BROTHER_EVT", "BROTHER_EVT_ID") or name.startswith("CASE_"):
                depth += 1
            pos += 4 + 4 * n
            if stop_at_end and op in END_OPS and depth == 0:
                break
        return lines


if __name__ == "__main__":
    rel_path, start = sys.argv[1], int(sys.argv[2], 0)
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 10000
    dol_path = os.path.join(ROOT, "disc", "DATA", "sys", "main.dol")
    dis = Disassembler(open(rel_path, "rb").read(), open(dol_path, "rb").read() if os.path.exists(dol_path) else None)
    print("\n".join(dis.disassemble(start, count)))
