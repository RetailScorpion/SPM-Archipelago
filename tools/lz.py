"""Nintendo LZ10 decompression (relF.bin, map .bin files).

Usage as a script: py -3 -E lz.py <in> <out>
"""

import sys


def decompress_lz10(data: bytes) -> bytes:
    assert data[0] == 0x10, f"not LZ10 (type byte {data[0]:#x})"
    size = int.from_bytes(data[1:4], "little")
    pos = 4
    if size == 0:  # extended header
        size = int.from_bytes(data[4:8], "little")
        pos = 8
    out = bytearray()
    while len(out) < size:
        flags = data[pos]
        pos += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if flags & (0x80 >> bit):
                b1, b2 = data[pos], data[pos + 1]
                pos += 2
                length = (b1 >> 4) + 3
                disp = ((b1 & 0xF) << 8 | b2) + 1
                for _ in range(length):
                    out.append(out[-disp])
            else:
                out.append(data[pos])
                pos += 1
    return bytes(out)


if __name__ == "__main__":
    open(sys.argv[2], "wb").write(decompress_lz10(open(sys.argv[1], "rb").read()))
