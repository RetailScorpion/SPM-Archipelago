"""Finds which sound effects loop, from the game's sound archive (sound/wiimario_snd.brsar).

A looping effect shuffled into a one-shot effect's place would never be stopped, so the mod's sound
effect shuffle (mod/source/sfxshuffle.cpp) only swaps effects that loop with each other, and the rest
with each other. Wave effects: a wave in their RWSD bank has its loop flag set. Sequenced effects:
their sequence jumps back to itself (or opens a track that does). Anything this can't decode counts
as looping (the harmless side: a one-shot in a looping effect's place just ends early).

Usage: py -3 -E tools/sfxloops.py [--header out.h]   (reads disc/DATA/files/sound/wiimario_snd.brsar)
"""

import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRSAR = os.path.join(ROOT, "disc", "DATA", "files", "sound", "wiimario_snd.brsar")
SEQ, STRM, WAVE = 1, 2, 3


class Brsar:
    def __init__(self, data):
        self.d = data
        assert data[:4] == b"RSAR"
        symb_off, _, info_off, _, file_off, _ = struct.unpack(">IIIIII", data[0x10:0x28])
        self.symb = symb_off + 8
        self.info = info_off + 8
        names = self.symb + self.u32(self.symb)
        self.name_count = self.u32(names)
        self.names_at = names
        self.sounds = self.table(self.ref(self.info))
        self.files = self.table(self.ref(self.info + 24))
        self.groups = self.table(self.ref(self.info + 32))

    def u32(self, at):
        return struct.unpack(">I", self.d[at:at + 4])[0]

    def ref(self, at, base=None):
        """A Ref (u8 type, u8 data type, u16, u32 offset) relative to the INFO block (or base)."""
        return (self.info if base is None else base) + self.u32(at + 4)

    def table(self, at, base=None):
        return [self.ref(at + 4 + 8 * i, base) for i in range(self.u32(at))]

    def name(self, i):
        if i >= self.name_count:
            return None
        o = self.u32(self.names_at + 4 + 4 * i)
        return self.d[self.symb + o:self.d.index(b"\0", self.symb + o)].decode()

    def sound(self, i):
        s = self.sounds[i]
        name_idx, file_id, _ = struct.unpack(">III", self.d[s:s + 12])
        kind = self.d[s + 22]
        detail = self.ref(s + 24)
        return self.name(name_idx), file_id, kind, detail

    def file_data(self, file_id):
        """(file data offset, wave data offset) of a file, from the first group holding it."""
        fi = self.files[file_id]
        positions = self.table(self.ref(fi + 20)) if self.u32(fi + 24) else []
        for pos in positions:
            group_id, index = struct.unpack(">II", self.d[pos:pos + 8])
            g = self.groups[group_id]
            g_off, _, g_wave, _ = struct.unpack(">IIII", self.d[g + 16:g + 32])
            items = self.table(self.ref(g + 32))
            item = items[index]
            fid, i_off, _, i_wave, _ = struct.unpack(">IIIII", self.d[item:item + 20])
            assert fid == file_id
            return g_off + i_off, g_wave + i_wave
        return None


def wave_loops(b, file_id, sub):
    """Does wave sound `sub` of an RWSD bank play a looping wave?"""
    d = b.d
    off, _ = b.file_data(file_id)
    assert d[off:off + 4] == b"RWSD"
    data_off, _, wave_off, _ = struct.unpack(">IIII", d[off + 0x10:off + 0x20])
    data = off + data_off
    waves = off + wave_off
    assert d[data:data + 4] == b"DATA" and d[waves:waves + 4] == b"WAVE"
    base = data + 8     # RWSD refs are relative to the DATA block's contents
    sounds = b.table(data + 8, base)
    wsd = sounds[sub]
    notes = b.table(b.ref(wsd + 16, base), base)
    wave_count = b.u32(waves + 8)
    loops = False
    for note in notes:
        wave = b.u32(note)
        if wave >= wave_count:
            raise ValueError(f"note wave {wave} past {wave_count}")
        info = waves + b.u32(waves + 12 + 4 * wave)
        loops |= d[info + 1] != 0
    return loops


class Undecodable(Exception):
    pass


def _varlen(d, p):
    value = 0
    for _ in range(4):
        byte = d[p]
        p += 1
        value = value << 7 | byte & 0x7F
        if not byte & 0x80:
            return value, p
    raise Undecodable("varlen too long")


# Argument sizes after each command byte ("v" = variable-length number), NW4R RSEQ
def _args(cmd):
    if cmd < 0x80:
        return [1, "v"]                     # note: velocity, length
    if cmd in (0x80, 0x81):
        return ["v"]                        # wait, program change
    if cmd == 0x88:
        return [1, 3]                       # open track: track number, offset
    if cmd in (0x89, 0x8A):
        return [3]                          # jump, call: offset
    if 0xB0 <= cmd <= 0xDF:
        return [1]                          # one-byte parameters (pan, volume, loop start...)
    if cmd in (0xE0, 0xE1, 0xE3):
        return [2]                          # modulation delay, tempo, sweep pitch
    if cmd in (0xFB, 0xFC, 0xFD, 0xFF):
        return []                           # envelope reset, loop end, return, end
    if cmd == 0xFE:
        return [2]                          # allocate tracks
    raise Undecodable(f"command {cmd:#04x}")


def _skip(d, p, sizes):
    for size in sizes:
        if size == "v":
            _, p = _varlen(d, p)
        else:
            p += size
    return p


def seq_loops(b, file_id, label_offset):
    """Does the sequence starting at label_offset ever play forever?"""
    d = b.d
    off, _ = b.file_data(file_id)
    assert d[off:off + 4] == b"RSEQ"
    data = off + b.u32(off + 0x10)
    assert d[data:data + 4] == b"DATA"
    code = data + b.u32(data + 8)

    def run(start, active):
        """Follows a track from start; active = code addresses on the current path."""
        p = code + start
        seen = set()
        while True:
            if p in seen or p in active:
                return True                 # back to code already played: it repeats forever
            seen.add(p)
            cmd = d[p]
            q = p + 1
            if cmd == 0xA2:                 # "if" prefix: the command may or may not run
                cmd = d[q]
                q += 1
            if cmd in (0xA0, 0xA1, 0xA3, 0xA4, 0xA5):
                # random / variable argument, or timed: the next command's last argument changes
                prefix = cmd
                cmd = d[q]
                q += 1
                sizes = _args(cmd)
                extra = {0xA0: [4], 0xA1: [1], 0xA3: [2], 0xA4: [4], 0xA5: [1]}[prefix]
                q = _skip(d, q, (sizes[:-1] + extra) if prefix in (0xA0, 0xA1) else sizes + extra)
                p = q
                continue
            if cmd == 0xF0:                 # extended commands: variables and comparisons
                sub = d[q]
                if 0x80 <= sub <= 0x95:
                    p = q + 1 + 3
                    continue
                raise Undecodable(f"extended command {sub:#04x}")
            if cmd in (0xFF, 0xFD):         # end of track, return from call
                return False
            if cmd == 0xD4 and d[q] == 0:   # loop start with no count: forever
                return True
            if cmd in (0x88, 0x89, 0x8A):
                target_at = q + (1 if cmd == 0x88 else 0)
                target = int.from_bytes(d[target_at:target_at + 3], "big")
                if cmd == 0x88:
                    if run(target, active | seen):
                        return True
                elif cmd == 0x8A:
                    if run(target, active | seen):
                        return True
                else:
                    if code + target in seen or code + target in active:
                        return True
                    p = code + target
                    continue
            p = _skip(d, q, _args(cmd))
            if p - code > b.u32(data + 4):
                raise Undecodable("ran past the sequence data")

    return run(label_offset, frozenset())


def looping_sounds(b, last):
    """Archive indexes (0..last) of effects that loop, and the undecodable ones (counted as looping)."""
    loops, unknown = set(), []
    for i in range(last + 1):
        _, file_id, kind, detail = b.sound(i)
        try:
            if kind == WAVE:
                looped = wave_loops(b, file_id, b.u32(detail))
            elif kind == SEQ:
                looped = seq_loops(b, file_id, b.u32(detail))
            else:
                looped = True
        except (Undecodable, ValueError, IndexError, AssertionError):
            looped = True
            unknown.append(i)
        if looped:
            loops.add(i)
    return loops, unknown


# The archive's sound effects come before its first music stream (SYS_TITLE1)
LAST_EFFECT = 1233


def write_header(path, loops):
    bits = bytearray((LAST_EFFECT + 8) // 8)
    for i in loops:
        bits[i // 8] |= 1 << (i % 8)
    rows = [", ".join(f"0x{v:02x}" for v in bits[i:i + 16]) for i in range(0, len(bits), 16)]
    with open(path, "w", newline="\n") as f:
        f.write("// Generated by tools/sfxloops.py from sound/wiimario_snd.brsar (US 1.0): do not edit.\n")
        f.write("// Bit n (byte n / 8, bit n % 8) is set if sound archive entry n is an effect that loops.\n")
        f.write("#pragma once\n\n")
        f.write(f"#define SFX_LOOP_SOUND_COUNT {LAST_EFFECT + 1}\n\n")
        f.write("static const u8 sSfxLoops[] = {\n")
        for row in rows:
            f.write(f"    {row},\n")
        f.write("};\n")


def main():
    b = Brsar(open(BRSAR, "rb").read())
    name, _, kind, _ = b.sound(LAST_EFFECT + 1)
    assert kind == STRM and name == "SYS_TITLE1", "not the US 1.0 sound archive"
    loops, unknown = looping_sounds(b, LAST_EFFECT)
    print(f"{len(loops)} of {LAST_EFFECT + 1} effects loop ({len(unknown)} undecodable, counted as looping)")
    if "--header" in sys.argv:
        path = sys.argv[sys.argv.index("--header") + 1]
        write_header(path, loops)
        print(f"Wrote {path}")


if __name__ == "__main__":
    main()
