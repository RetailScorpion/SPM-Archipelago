"""Builds the spm-rel-loader Gecko code into main.dol, so the patched disc needs no cheat codes.

Supports the two code types the loader uses:
  04XXXXXX VVVVVVVV   write 32-bit V at 0x80XXXXXX   -> patched in place in the dol
  C2XXXXXX NNNNNNNN   insert N lines of asm at 0x80XXXXXX -> the asm goes into a new text section
                      at PAYLOAD_ADDR, and the original instruction is replaced with a branch to it

Usage: py -3 -E patch_dol.py <clean main.dol> <loader usX.txt> <out main.dol>
"""

import struct
import sys

# Free low memory. Not 0x80001800 itself: Dolphin writes a "STUBHAXX" marker at 0x80001804 when booting.
# The whole 0x80001800-0x80003000 area is also where a Gecko codehandler goes, so the patched game
# can't be combined with Gecko codes in Dolphin (see DESIGN.md, Distribution).
PAYLOAD_ADDR = 0x80001900
DOL_HEADER = 0x100
NUM_TEXT = 7
NUM_SECTIONS = 18


def parse_gecko(path):
    lines = []
    for line in open(path):
        line = line.split("#")[0].strip()
        if line:
            a, b = line.split()
            lines.append((int(a, 16), int(b, 16)))
    writes, inserts = [], []
    i = 0
    while i < len(lines):
        a, b = lines[i]
        kind, addr = a >> 24, 0x80000000 | (a & 0x01FFFFFF)
        if kind == 0x04:
            writes.append((addr, b))
            i += 1
        elif kind == 0xC2:
            words = [w for pair in lines[i + 1:i + 1 + b] for w in pair]
            inserts.append((addr, words))
            i += 1 + b
        else:
            raise ValueError(f"Unsupported gecko code type {kind:#04x}")
    return writes, inserts


def branch(src, dst):
    return 0x48000000 | ((dst - src) & 0x03FFFFFC)


class Dol:
    def __init__(self, data):
        self.data = bytearray(data)
        self.off = list(struct.unpack(">18I", data[0:0x48]))
        self.addr = list(struct.unpack(">18I", data[0x48:0x90]))
        self.size = list(struct.unpack(">18I", data[0x90:0xD8]))

    def file_offset(self, addr):
        for i in range(NUM_SECTIONS):
            if self.size[i] and self.addr[i] <= addr < self.addr[i] + self.size[i]:
                return self.off[i] + addr - self.addr[i]
        raise ValueError(f"{addr:#x} isn't in any dol section")

    def read32(self, addr):
        return struct.unpack_from(">I", self.data, self.file_offset(addr))[0]

    def write32(self, addr, value):
        struct.pack_into(">I", self.data, self.file_offset(addr), value)

    def add_text_section(self, addr, payload):
        slot = next(i for i in range(NUM_TEXT) if self.size[i] == 0)
        # sections are 32-byte aligned in the file
        start = (len(self.data) + 31) & ~31
        payload = payload + b"\0" * (-len(payload) % 32)
        self.data += b"\0" * (start - len(self.data)) + payload
        self.off[slot], self.addr[slot], self.size[slot] = start, addr, len(payload)
        struct.pack_into(">18I", self.data, 0x00, *self.off)
        struct.pack_into(">18I", self.data, 0x48, *self.addr)
        struct.pack_into(">18I", self.data, 0x90, *self.size)


def main(dol_path, gecko_path, out_path):
    dol = Dol(open(dol_path, "rb").read())
    writes, inserts = parse_gecko(gecko_path)

    if any(dol.size[i] and dol.addr[i] == PAYLOAD_ADDR for i in range(NUM_SECTIONS)):
        raise SystemExit("This dol is already patched")

    for addr, value in writes:
        dol.write32(addr, value)

    payload = bytearray()
    for addr, words in inserts:
        start = PAYLOAD_ADDR + len(payload)
        # like the Gecko handler: the C2 block ends with the original instruction then a 0 word,
        # which becomes the branch back to the instruction after the hook
        assert words[-1] == 0, "C2 block should end with a 00000000 placeholder"
        original = dol.read32(addr)
        assert words[-2] == original, \
            f"Instruction at {addr:#x} is {original:#010x}, not the {words[-2]:#010x} the code expects: wrong game version?"
        words[-1] = branch(start + (len(words) - 1) * 4, addr + 4)
        payload += struct.pack(f">{len(words)}I", *words)
        dol.write32(addr, branch(addr, start))

    dol.add_text_section(PAYLOAD_ADDR, bytes(payload))
    open(out_path, "wb").write(dol.data)
    print(f"Patched {len(writes)} words and {len(inserts)} hook(s); payload {len(payload):#x} bytes at {PAYLOAD_ADDR:#x}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
