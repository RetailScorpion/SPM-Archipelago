"""Reading SPM room archives: disc/DATA/files/map/<room>.bin (LZ10 + U8)."""
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
from lz import decompress_lz10  # noqa: E402

MAP_DIR = os.path.join(REPO, 'disc', 'DATA', 'files', 'map')
OUT_DIR = os.path.join(REPO, 'build', 'maps')


def read_u8(data):
    """Files in a U8 archive, as {path: bytes} (paths without the leading './')."""
    assert data[:4] == b'\x55\xaa\x38\x2d', 'not a U8 archive'
    root = struct.unpack('>I', data[4:8])[0]
    count = struct.unpack('>I', data[root + 8:root + 12])[0]
    strings = root + count * 12
    files, ends, path = {}, [], []
    for i in range(count):
        ent = data[root + i * 12:root + i * 12 + 12]
        kind, name_off = ent[0], int.from_bytes(ent[1:4], 'big')
        a, b = struct.unpack('>II', ent[4:])
        name = data[strings + name_off:data.index(b'\0', strings + name_off)].decode()
        while ends and ends[-1] <= i:
            ends.pop()
            path.pop()
        if kind == 1:
            ends.append(b)
            path.append(name)
        else:
            full = '/'.join(p for p in path + [name] if p not in ('', '.'))
            files[full] = data[a:a + b]
    return files


_MAP_FILES = []


def map_files():
    """{room name: the map file it loads} (data/maps_us0.json). Most rooms load their own file; Chapter 6's
    gates share a few (wa1_03 to wa1_25 are all wa1_02, wa1_26 is wa1_03, ...)."""
    if not _MAP_FILES:
        import json
        maps = json.load(open(os.path.join(REPO, 'data', 'maps_us0.json')))
        _MAP_FILES.append({m['name']: m['file'] for m in maps})
    return _MAP_FILES[0]


def room_files(room):
    """Files of one room, keyed relative to dvd/ (e.g. 'map/he1_01/map.dat', 'bg/he1_01_02.tpl'). A room
    that loads another room's file gets that file's contents under its own name."""
    file = map_files().get(room, room)
    raw = open(os.path.join(MAP_DIR, file + '.bin'), 'rb').read()
    files = read_u8(decompress_lz10(raw))
    files = {k[4:] if k.startswith('dvd/') else k: v for k, v in files.items()}
    return files if file == room else {k.replace(file, room): v for k, v in files.items()}


def all_rooms():
    """Every room name whose map file is on the disc."""
    on_disc = {f[:-4] for f in os.listdir(MAP_DIR) if f.endswith('.bin')}
    return sorted({r for r, f in map_files().items() if f in on_disc} | on_disc)


def write_png(path, rgba, width, height):
    """Writes 8-bit RGBA rows (bytes, top row first) as a PNG without needing PIL."""
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    stride = width * 4
    raw = b''.join(b'\0' + bytes(rgba[y * stride:(y + 1) * stride]) for y in range(height))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 6)))
        f.write(chunk(b'IEND', b''))
