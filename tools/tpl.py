"""Minimal TPL reader/writer for RGB5A3 (format 5) textures, used for SPM's wicon.tpl."""
import struct
from PIL import Image

def _decode_px(v):
    if v & 0x8000:
        r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
        return (r * 255 // 31, g * 255 // 31, b * 255 // 31, 255)
    a, r, g, b = (v >> 12) & 7, (v >> 8) & 15, (v >> 4) & 15, v & 15
    return (r * 17, g * 17, b * 17, a * 255 // 7)

def _encode_px(p):
    r, g, b, a = p
    if a >= 0xE0:
        return 0x8000 | (r * 31 // 255) << 10 | (g * 31 // 255) << 5 | (b * 31 // 255)
    return (a * 7 // 255) << 12 | (r // 17) << 8 | (g // 17) << 4 | (b // 17)

def images(data):
    """Yields (index, width, height, format, data_offset) for each texture."""
    _, n, tbl = struct.unpack('>III', data[:12])
    for i in range(n):
        ih, _ = struct.unpack('>II', data[tbl + 8 * i:tbl + 8 * i + 8])
        h, w, fmt, off = struct.unpack('>HHII', data[ih:ih + 12])
        yield i, w, h, fmt, off

def decode_rgb5a3(data, off, w, h):
    im = Image.new('RGBA', (w, h))
    px = im.load()
    pos = off
    for ty in range(0, h, 4):
        for tx in range(0, w, 4):
            for y in range(4):
                for x in range(4):
                    v = struct.unpack('>H', data[pos:pos + 2])[0]
                    pos += 2
                    if tx + x < w and ty + y < h:
                        px[tx + x, ty + y] = _decode_px(v)
    return im

def encode_rgb5a3(im):
    im = im.convert('RGBA')
    w, h = im.size
    px = im.load()
    out = bytearray()
    for ty in range(0, h, 4):
        for tx in range(0, w, 4):
            for y in range(4):
                for x in range(4):
                    p = px[tx + x, ty + y] if tx + x < w and ty + y < h else (0, 0, 0, 0)
                    out += struct.pack('>H', _encode_px(p))
    return bytes(out)
