"""GameCube/Wii TPL texture decoding (all formats SPM's maps use), with numpy.

decode_tpl(data) -> list of dicts: width, height, format, wrap_s, wrap_t, rgba (H x W x 4 uint8).
"""
import struct

import numpy as np

# format -> (block width, block height, bits per pixel)
BLOCK = {0: (8, 8, 4), 1: (8, 4, 8), 2: (8, 4, 8), 3: (4, 4, 16), 4: (4, 4, 16), 5: (4, 4, 16),
         6: (4, 4, 32), 8: (8, 8, 4), 9: (8, 4, 8), 10: (4, 4, 16), 14: (8, 8, 4)}


def _pad(n, b):
    return (n + b - 1) // b * b


def _deblock(vals, w, h, bw, bh):
    """Values stored block by block (bh x bw pixels each) -> (h, w, ...) array."""
    pw, ph = _pad(w, bw), _pad(h, bh)
    rest = vals.shape[1:]
    v = vals[:pw * ph].reshape((ph // bh, pw // bw, bh, bw) + rest)
    v = v.transpose((0, 2, 1, 3) + tuple(range(4, 4 + len(rest)))).reshape((ph, pw) + rest)
    return v[:h, :w]


def _rgb565(v):
    v = v.astype(np.uint32)
    r = (v >> 11) & 31
    g = (v >> 5) & 63
    b = v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2),
                     np.full_like(r, 255)], -1).astype(np.uint8)


def _rgb5a3(v):
    v = v.astype(np.uint32)
    opaque = (v & 0x8000) != 0
    r5, g5, b5 = (v >> 10) & 31, (v >> 5) & 31, v & 31
    a3, r4, g4, b4 = (v >> 12) & 7, (v >> 8) & 15, (v >> 4) & 15, v & 15
    r = np.where(opaque, (r5 << 3) | (r5 >> 2), r4 * 17)
    g = np.where(opaque, (g5 << 3) | (g5 >> 2), g4 * 17)
    b = np.where(opaque, (b5 << 3) | (b5 >> 2), b4 * 17)
    a = np.where(opaque, 255, (a3 << 5) | (a3 << 2) | (a3 >> 1))
    return np.stack([r, g, b, a], -1).astype(np.uint8)


def _ia8(v):
    v = v.astype(np.uint32)
    i, a = v & 0xff, v >> 8
    return np.stack([i, i, i, a], -1).astype(np.uint8)


def _palette(data, fmt, off, count):
    v = np.frombuffer(data, '>u2', count, off)
    return {0: _ia8, 1: _rgb565, 2: _rgb5a3}[fmt](v)


def decode(data, off, w, h, fmt, pal=None):
    bw, bh, bpp = BLOCK[fmt]
    n = _pad(w, bw) * _pad(h, bh)
    if bpp == 4:
        b = np.frombuffer(data, np.uint8, n // 2, off)
        nib = np.stack([b >> 4, b & 15], -1).reshape(-1)
    if fmt == 14:
        return _cmpr(data, off, w, h)
    if fmt == 0:
        i = (_deblock(nib, w, h, bw, bh) * 17).astype(np.uint8)
        return np.stack([i, i, i, i], -1)
    if fmt == 1:
        i = _deblock(np.frombuffer(data, np.uint8, n, off), w, h, bw, bh)
        return np.stack([i, i, i, i], -1)
    if fmt == 2:
        b = _deblock(np.frombuffer(data, np.uint8, n, off), w, h, bw, bh)
        i, a = (b & 15) * 17, (b >> 4) * 17
        return np.stack([i, i, i, a], -1).astype(np.uint8)
    if fmt in (3, 4, 5):
        v = _deblock(np.frombuffer(data, '>u2', n, off), w, h, bw, bh)
        return {3: _ia8, 4: _rgb565, 5: _rgb5a3}[fmt](v)
    if fmt == 6:
        blocks = np.frombuffer(data, np.uint8, n * 4, off).reshape(-1, 2, 16, 2)
        ar, gb = blocks[:, 0], blocks[:, 1]
        px = np.stack([ar[..., 1], gb[..., 0], gb[..., 1], ar[..., 0]], -1).reshape(-1, 4)
        return _deblock(px, w, h, bw, bh)
    if fmt in (8, 9, 10):
        if fmt == 8:
            idx = nib
        elif fmt == 9:
            idx = np.frombuffer(data, np.uint8, n, off)
        else:
            idx = np.frombuffer(data, '>u2', n, off) & 0x3fff
        idx = _deblock(idx.astype(np.int64), w, h, bw, bh)
        return pal[np.minimum(idx, len(pal) - 1)]
    raise ValueError(f'unknown texture format {fmt}')


def _cmpr(data, off, w, h):
    pw, ph = _pad(w, 8), _pad(h, 8)
    sub = np.frombuffer(data, np.uint8, pw * ph // 2, off).reshape(-1, 8)  # 4x4 DXT1 blocks
    c0 = sub[:, 0].astype(np.uint32) << 8 | sub[:, 1]
    c1 = sub[:, 2].astype(np.uint32) << 8 | sub[:, 3]
    p0, p1 = _rgb565(c0).astype(np.int32), _rgb565(c1).astype(np.int32)
    gt = (c0 > c1)[:, None]
    p2 = np.where(gt, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(gt, (p0 + 2 * p1) // 3, 0)
    p3[:, 3] = np.where(gt[:, 0], 255, 0)
    pal = np.stack([p0, p1, p2, p3], 1).astype(np.uint8)  # (blocks, 4, 4ch)
    bits = sub[:, 4:8]
    idx = np.stack([(bits >> s) & 3 for s in (6, 4, 2, 0)], -1).reshape(-1, 16)  # row-major in 4x4
    px = np.take_along_axis(pal, idx[:, :, None].astype(np.int64), 1)  # (blocks, 16, 4)
    # blocks are 2x2 sub-blocks per 8x8 tile
    px = px.reshape(ph // 8, pw // 8, 2, 2, 4, 4, 4).transpose(0, 2, 4, 1, 3, 5, 6).reshape(ph, pw, 4)
    return px[:h, :w]


def decode_tpl(data):
    magic, count, table = struct.unpack('>III', data[:12])
    assert magic == 0x0020AF30, 'not a TPL'
    out = []
    for i in range(count):
        ih, ph = struct.unpack('>II', data[table + 8 * i:table + 8 * i + 8])
        h, w, fmt, doff, ws, wt = struct.unpack('>HHIIII', data[ih:ih + 20])
        pal = None
        if ph:
            pn, _, _, pfmt, poff = struct.unpack('>HBBII', data[ph:ph + 12])
            pal = _palette(data, pfmt, poff, pn)
        out.append({'width': w, 'height': h, 'format': fmt, 'wrap_s': ws, 'wrap_t': wt,
                    'rgba': np.ascontiguousarray(decode(data, doff, w, h, fmt, pal))})
    return out
