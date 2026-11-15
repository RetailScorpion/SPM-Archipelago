"""Reads each room's init script (build/relF.rel) for what the map needs:

- doors: evt_door_set_map_door_descs (loading zones), evt_door_set_dokan_descs (pipes),
  evt_door_set_door_descs (doors inside the room, no destination), and warps set up as hit events with
  the destination room and entrance in LW(0)/LW(1), or that teleport Mario to another hit object in the
  same room (black holes); elevators from the elevator tables (Flipside/Flopside); pits that drop
  Mario into another room (kind 'pit')
- hide: map objects the script hides with evt_mapobj_flag_onoff (flag 1) and never shows again
- anims: map animations the script plays (evt_map_playanim) and the play modes used
- mobjs: blocks, chests, save blocks and Star Blocks the scripts spawn (kind, type, name, pos in game units)
- mapchange: other rooms the room's scripts send Mario to with evt_seq_mapchange (not tied to a door)

Writes build/maps/scripts.json. Run with the normal Python (needs only the repo's tools/).
"""
import bisect
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from spmfiles import REPO, OUT_DIR, all_rooms  # noqa: E402

sys.path.insert(0, os.path.join(REPO, 'tools'))
from evtdis import Disassembler  # noqa: E402

REL = os.path.join(REPO, 'build', 'relF.rel')
DOL = os.path.join(REPO, 'disc', 'DATA', 'sys', 'main.dol')
CHILD_OPS = {'RUN_CHILD_EVT', 'RUN_EVT', 'RUN_EVT_ID'}
# user function that runs a script when Mario touches a hit object; scripts set LW(0) = room and
# LW(1) = entrance just before, which the touched script then warps to (black holes in Chapter 4)
HIT_EVENT = '0x800e0e74'
STAR_BLOCK = '0x800f929c'  # (name, x, y, z, script, 0): the Star Block at the end of a level (evt_mobj_eria_block)
JUMP_STAND = '0x800f4ef8'  # (type, name, x, y, z, ...): a spring (1 red, 0 blue; mac_08's red one is type 1)
JUMP_CLOUD = '0x800f4b94'  # (type, name, x, y, z, bounce, 0): a bouncy cloud (7-3; MOBJ_jump_kumo, MOBJ_big_jump_kumo)
HAMMER_BLK = '0x800f50e0'  # (type, name, x, y, z, break script, 0): yellow blocks Cudge breaks (evt_mobj_hunmer_blk)
HIP_KUI = '0x800fa370'     # (type, name, x, y, z, down script, up script, pounded flag): ground-pound stakes (evt_mobj_hip_kui)
HIT_POSITION = '0x800eb3f8'  # (hit object name, x, y, z): a hit object's position
ROOMS = set(all_rooms())


def warp_name(hit):
    """Name of a warp from its hit object; black holes (A2D_BH_1, A2D_BH01, A2D_h_BH_3, A2D_BH) are the
    entrances BH_01 etc. that other rooms' scripts send Mario to."""
    m = re.search(r'BH_?(\d*)$', hit)
    return f'BH_{int(m.group(1) or 1):02d}' if m else hit


class ScriptReader:
    def __init__(self):
        self._handlers = {}
        self.dis = Disassembler(open(REL, 'rb').read(), open(DOL, 'rb').read() if os.path.exists(DOL) else None)
        self.data = self.dis.data

    def string(self, fo):
        a = self.dis.arg(fo)
        return a[1:-1] if a.startswith('"') else None

    def rel_target(self, arg):
        m = re.match(r'rel@(0x[0-9a-f]+)$', arg)
        return int(m.group(1), 16) if m else None

    def instructions(self, start, limit=4000):
        """(offset, op name, [args], depth) until the script's RETURN/END."""
        pos, depth = start, 0
        for _ in range(limit):
            hdr = struct.unpack_from('>I', self.data, pos)[0]
            op, n = hdr & 0xFFFF, hdr >> 16
            name = self.dis.ops.get(op, f'OP_{op}')
            args = [self.dis.arg(pos + 4 + 4 * i) for i in range(n)]
            if name in ('ELSE', 'END_IF', 'END_SWITCH', 'WHILE', 'CASE_END', 'END_INLINE', 'END_BROTHER') \
                    or name.startswith('CASE_'):
                depth = max(0, depth - 1)
            yield pos, name, args, depth
            if name.startswith('IF') or name in ('ELSE', 'DO', 'SWITCH', 'SWITCHI', 'INLINE_EVT', 'INLINE_EVT_ID',
                                                 'BROTHER_EVT', 'BROTHER_EVT_ID') or name.startswith('CASE_'):
                depth += 1
            pos += 4 + 4 * n
            if op in (1, 2) and depth == 0:
                return

    def warp_handler(self, fo):
        """What a hit-event script does: ('mapchange', None) when it changes map (to LW(0)/LW(1)),
        ('teleport', hit object) when it moves Mario to another hit object's position, else (None, None)."""
        if fo in self._handlers:
            return self._handlers[fo]
        res, lookup = (None, None), None
        for _, name, args, _ in self.instructions(fo):
            if name != 'USER_FUNC' or not args:
                continue
            if args[0] == 'evt_seq_mapchange':
                res = ('mapchange', None)
                break
            if args[0] == HIT_POSITION and len(args) > 1 and args[1].startswith('"'):
                lookup = args[1][1:-1]
            if args[0] == 'evt_mario_set_pos' and lookup:
                res = ('teleport', lookup)
                break
        self._handlers[fo] = res
        return res

    def map_door_descs(self, fo, count):
        out = []
        for i in range(count):
            e = fo + 0x20 * i
            s = lambda off: self.string(e + off)
            out.append({'kind': 'map_door', 'name': s(4) or s(8), 'name_l': s(4), 'name_r': s(8),
                        'hit2d': s(0x0C), 'hit3d': s(0x10), 'dest_room': s(0x14), 'dest_door': s(0x18)})
        return out

    def dokan_descs(self, fo, count):
        out = []
        for i in range(count):
            e = fo + 0x20 * i
            s = lambda off: self.string(e + off)
            out.append({'kind': 'pipe', 'name': s(8), 'mapobj': s(0x0C), 'hit2d': s(0x10), 'hit3d': s(0x14),
                        'dest_room': s(0x18), 'dest_door': s(0x1C)})
        return out

    def door_descs(self, fo, count):
        out = []
        for i in range(count):
            e = fo + 0x58 * i
            s = lambda off: self.string(e + off)
            out.append({'kind': 'door', 'name': s(0x0C), 'mapobj': s(0x2C), 'hit2d': s(0x30), 'hit3d': s(0x34),
                        'dest_room': None, 'dest_door': None})
        return out

    def room(self, init_fo, room_name=None):
        doors, hide, show, mapchange, anims = [], set(), set(), [], {}
        todo, seen = [init_fo], set()
        while todo:
            start = todo.pop()
            if start in seen or len(seen) > 40:
                continue
            seen.add(start)
            strs = {}  # LW variables currently holding a string (for warps set up through hit events)
            for pos, name, args, depth in self.instructions(start):
                if name == 'SET' and len(args) == 2:
                    strs[args[0]] = args[1][1:-1] if args[1].startswith('"') else None
                if name == 'USER_FUNC' and args and args[0] == HIT_EVENT and len(args) >= 6:
                    hit2d = args[3][1:-1] if args[3].startswith('"') else None
                    hit3d = args[4][1:-1] if args[4].startswith('"') else None
                    handler = self.rel_target(args[5])
                    kind, target = self.warp_handler(handler) if handler is not None else (None, None)
                    dest = None
                    if kind == 'mapchange' and strs.get('LW(0)') in ROOMS:
                        dest = (strs['LW(0)'], strs.get('LW(1)'))  # the handler warps to LW(0)/LW(1)
                    elif kind == 'teleport' and room_name:
                        dest = (room_name, warp_name(target))  # moved to another hit object in this room
                    if dest and (hit2d or hit3d):
                        name_ = warp_name(hit2d or hit3d)
                        doors.append({'kind': 'black_hole' if name_.startswith('BH_') else 'warp', 'name': name_,
                                      'hit2d': hit2d, 'hit3d': hit3d, 'mapobj': None,
                                      'dest_room': dest[0], 'dest_door': dest[1]})
                if name in CHILD_OPS and args:
                    t = self.rel_target(args[0])
                    if t is not None:
                        todo.append(t)
                if name != 'USER_FUNC' or not args:
                    continue
                fn, a = args[0], args[1:]
                fo = self.rel_target(a[0]) if a else None
                if fn == 'evt_door_set_map_door_descs' and fo is not None:
                    doors += self.map_door_descs(fo, int(a[1]))
                elif fn == 'evt_door_set_dokan_descs' and fo is not None:
                    doors += self.dokan_descs(fo, int(a[1]))
                elif fn == 'evt_door_set_door_descs' and fo is not None:
                    doors += self.door_descs(fo, int(a[1]))
                elif fn == 'evt_mapobj_flag_onoff' and len(a) >= 4 and a[2].startswith('"'):
                    if a[3].isdigit() and int(a[3]) & 1:
                        (hide if a[1] == '1' else show).add(a[2][1:-1])
                elif fn == 'evt_map_playanim' and len(a) >= 2 and a[0].startswith('"'):
                    anims.setdefault(a[0][1:-1], set()).add(a[1])
                elif fn == 'evt_seq_mapchange' and len(a) >= 1 and a[0].startswith('"'):
                    mapchange.append({'room': a[0][1:-1], 'door': a[1][1:-1] if len(a) > 1 and a[1].startswith('"') else None,
                                      'conditional': depth > 0})
        mobjs, keys = [], set()
        for start in sorted(seen):
            for m in self.run_mobjs(start):
                k = (m['kind'], m.get('type'), m['pos'])
                if k not in keys:
                    keys.add(k)
                    mobjs.append(m)
        return {'doors': doors, 'hide': sorted(hide - show), 'mapchange': mapchange,
                'anims': {k: sorted(v) for k, v in anims.items()}, 'mobjs': mobjs}

    def run_mobjs(self, start, max_steps=50000):
        """Blocks, chests, save blocks and Star Blocks a script spawns, with their positions (game units). Runs the
        script's loops and variable arithmetic (SET/ADD/SUB/MUL on LW etc.), takes every IF branch, runs
        DO(0) (endless) loops once. conditional = spawned inside an IF."""
        ins = list(self.instructions(start))
        var, loops, out = {}, [], []
        if_depth, pc, steps = 0, 0, 0
        ifs = []  # the IFs around this point: [op, args, in its ELSE]

        def val(a):
            if a in var:
                return var[a]
            try:
                return float(a[:-1]) if a.endswith('f') else float(int(a))
            except ValueError:
                return None

        def arith(name, args, op):
            a, b = var.get(args[0]), val(args[1])
            var[args[0]] = op(a, b) if a is not None and b is not None else None

        while pc < len(ins) and steps < max_steps:
            steps += 1
            _, name, args, _ = ins[pc]
            if name == 'DO':
                n = val(args[0]) if args else 0
                loops.append([pc, int(n) if n and n > 0 else 1])
            elif name == 'WHILE' and loops:
                loops[-1][1] -= 1
                if loops[-1][1] > 0:
                    pc = loops[-1][0] + 1
                    continue
                loops.pop()
            elif name.startswith('IF'):
                if_depth += 1
                ifs.append([name, list(args), False])
            elif name == 'ELSE' and ifs:
                ifs[-1][2] = True
            elif name == 'END_IF':
                if_depth = max(0, if_depth - 1)
                if ifs:
                    ifs.pop()
            elif name in ('SET', 'SETF') and len(args) == 2:
                var[args[0]] = val(args[1])
            elif name in ('ADD', 'ADDF') and len(args) == 2:
                arith(name, args, lambda a, b: a + b)
            elif name in ('SUB', 'SUBF') and len(args) == 2:
                arith(name, args, lambda a, b: a - b)
            elif name in ('MUL', 'MULF') and len(args) == 2:
                arith(name, args, lambda a, b: a * b)
            elif name == 'USER_FUNC' and args:
                fn, a = args[0], args[1:]
                m = None
                if fn == 'evt_mobj_blk' and len(a) >= 5:
                    m = {'kind': 'block', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5]}
                elif fn == 'evt_mobj_thako' and len(a) >= 5:
                    m = {'kind': 'chest', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5],
                         'item': val(a[7]) if len(a) > 7 else None}
                elif fn == 'evt_mobj_zyo' and len(a) >= 9:
                    # a lock on a door or gate (evt_mobj.h evt_mobj_zyo: name, key item, x, y, z, y rotation,
                    # interact script, open script, flag set once it's open)
                    m = {'kind': 'lock', 'type': 0, 'name': a[0], 'xyz': a[2:5], 'item': val(a[1]),
                         'rot': val(a[5]), 'state_var': a[8]}
                elif fn == 'evt_mobj_save_blk' and len(a) >= 4:
                    m = {'kind': 'save', 'type': 0, 'name': a[0], 'xyz': a[1:4]}
                elif fn == STAR_BLOCK and len(a) >= 4:
                    m = {'kind': 'star_block', 'type': 0, 'name': a[0], 'xyz': a[1:4]}
                elif fn == JUMP_STAND and len(a) >= 5:
                    m = {'kind': 'spring', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5]}
                elif fn == JUMP_CLOUD and len(a) >= 5:
                    m = {'kind': 'cloud', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5]}
                elif fn == HAMMER_BLK and len(a) >= 5:
                    m = {'kind': 'hammer', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5]}
                elif fn == HIP_KUI and len(a) >= 8:
                    # ground-pound stakes (type 0 medium, 1 tall); the last argument is the flag holding
                    # whether it's pounded down (evt_mobj.h evt_mobj_hip_kui)
                    m = {'kind': 'stake', 'type': val(a[0]), 'name': a[1], 'xyz': a[2:5], 'state_var': a[7]}
                if m:
                    xyz = [val(v) for v in m.pop('xyz')]
                    if None not in xyz and m['type'] is not None:
                        m['type'] = int(m['type'])
                        m['name'] = m['name'][1:-1] if m['name'].startswith('"') else None
                        m['pos'] = tuple(round(v, 3) for v in xyz)
                        m['conditional'] = if_depth > 0
                        if m['kind'] == 'lock':  # what decides whether it's still there (viewer: locks)
                            m['ifs'] = [list(x) for x in ifs]
                        out.append(m)
            pc += 1
        return out


def elevators(rd, maps):
    """Elevator tables (Flipside/Flopside): entries of 11 words, (name "elvN", 3 map objects, hit 2D,
    hit 3D, switch hit 2D, switch hit 3D, destination room, destination elevator, flags). A table sits in
    the data just before its room's init script, so it belongs to the room whose init script follows it."""
    inits = sorted((m['init_rel'], m['name']) for m in maps)
    starts = [i for i, _ in inits]
    seen, out = set(), {}
    for fo, (mod, sec, add) in rd.dis.relocs.items():
        if mod == 0 or fo in seen:
            continue
        name = rd.string(fo)
        if not name or not re.match(r'^elv\d+$', name):
            continue
        w = [rd.string(fo + 4 * k) for k in range(10)]
        if w[8] not in ROOMS or not w[9]:
            continue
        seen.add(fo)
        i = bisect.bisect_right(starts, fo)
        if i >= len(inits):
            continue
        room = inits[i][1]
        out.setdefault(room, []).append({'kind': 'elevator', 'name': name, 'mapobj': w[1], 'hit2d': w[4],
                                         'hit3d': w[5], 'dest_room': w[8], 'dest_door': w[9]})
    return out


# The call that changes room with (room, entrance); the symbol list names it one function off.
MAPCHANGE = 'evt_frame_delete'
# names of trigger zones and holes that mean "falling in here takes you to another room"
PIT_WORDS = ('naraku', 'ana', 'oti', 'yuka', 'hai_yama', 'idou')
SPRING = '0x800f4ef8'  # (1, name, x, y, z, ...): a spring that launches Mario; its script changes room


def pit_name(name):
    """True when a name has a PIT_WORDS word as a whole part (A2_ana, d_ana_1; not uranai, kinghana)."""
    n = (name or '').lower()
    parts = re.split(r'[_\d]+', n)
    return any(w in parts if '_' not in w else w in n for w in PIT_WORDS)


def pits(rd, maps):
    """Pits: map changes with a fixed destination that happen when Mario falls in (Flipside/Flopside,
    Merlee's mansion, Fort Francis trapdoors, ...). For each such call: the script around it, what
    triggers that script (a hit event's zone) or the hole it opens, and the room it belongs to (the room
    whose init script follows it). A pit when a zone/hole name has a PIT_WORDS word or the entrance
    starts with "fall". Springs that launch Mario to another room (mac_08's red spring) are included
    the same way, placed at the spring's position ('pos', game units). Returns {room: [door dicts with
    kind 'pit']}."""
    d = rd.dis
    mc = next((a for a, n in d.syms.items() if n == MAPCHANGE), None)
    inits = sorted((m['init_rel'], m['name']) for m in maps)
    starts = [i for i, _ in inits]
    refs = {}
    for fo, (mod, sec, add) in d.relocs.items():
        if mod != 0:
            refs.setdefault(d.rel.file_offset(sec, add), []).append(fo)
    targets = sorted(refs)

    def enclosing(call):
        i = bisect.bisect_right(targets, call) - 1
        for k in range(i, max(i - 40, -1), -1):
            try:
                if any(pos == call - 4 for pos, *_ in rd.instructions(targets[k], 3000)):
                    return targets[k]
            except Exception:
                pass
        return None

    def instr_at(off):  # the instruction whose argument words include off
        for k in range(1, 12):
            h = off - 4 * k
            hdr = struct.unpack_from('>I', d.data, h)[0]
            if (hdr >> 16) >= k and (hdr & 0xFFFF) < 0x200:
                return h, hdr
        return None, None

    out = {}
    for fo, (mod, sec, add) in d.relocs.items():
        if mod != 0 or add != mc:
            continue
        hdr = struct.unpack_from('>I', d.data, fo - 4)[0]
        if hdr & 0xFFFF != 0x5C:
            continue
        room_to = rd.string(fo + 4)
        door_to = rd.string(fo + 8)
        if room_to not in ROOMS:
            continue
        st = enclosing(fo)
        hit2d = hit3d = mapobj = pos = None
        spring = False
        for r in refs.get(st, []) if st is not None else []:
            h, hd = instr_at(r)
            if h is not None and d.arg(h + 4) == HIT_EVENT:
                hit2d, hit3d = rd.string(h + 16), rd.string(h + 20)
            elif h is not None and d.arg(h + 4) == SPRING:  # a spring: one way back up, at its position
                spring = True
                xyz = [d.arg(h + 4 * k) for k in (4, 5, 6)]
                try:
                    pos = [float(v[:-1]) if v.endswith('f') else float(v) for v in xyz]
                except ValueError:
                    pos = None
        if spring:
            i = bisect.bisect_right(starts, fo)
            if i < len(inits) and pos is not None:
                out.setdefault(inits[i][1], []).append({
                    'kind': 'pit', 'name': 'spring', 'hit2d': None, 'hit3d': None, 'mapobj': None,
                    'pos': pos, 'dest_room': room_to, 'dest_door': door_to})
            continue
        if st is not None and not (hit2d or hit3d):  # no trigger zone: the hole the script opens
            for _, name, args, _ in rd.instructions(st, 3000):
                for a in args[1:]:
                    n = a[1:-1] if a.startswith('"') else ''
                    if pit_name(n) and not n.startswith(('anm_', 'BGM', 'SFX')):
                        if n.startswith(('A2', 'A3')):
                            hit2d = hit2d or n
                        else:
                            mapobj = mapobj or n
                if args and args[0] == 'evt_map_playanim' and args[1].startswith('"anm_'):
                    n = args[1][5:-1]
                    if pit_name(n):  # the hole's animation: its trigger zones are usually A2D_/A3D_<name>
                        mapobj = mapobj or n
                        hit2d, hit3d = hit2d or f'A2D_{n}', hit3d or f'A3D_{n}'
        if not (any(pit_name(x) for x in (hit2d, hit3d, mapobj)) or (door_to or '').startswith('fall')):
            continue
        i = bisect.bisect_right(starts, fo)
        if i >= len(inits):
            continue
        room = inits[i][1]
        entry = {'kind': 'pit', 'name': 'pit_' + (hit2d or mapobj or room_to), 'hit2d': hit2d, 'hit3d': hit3d,
                 'mapobj': mapobj, 'dest_room': room_to, 'dest_door': door_to}
        if all((e['name'], e['dest_room']) != (entry['name'], room_to) for e in out.get(room, [])):
            out.setdefault(room, []).append(entry)
    return out


def main():
    maps = json.load(open(os.path.join(REPO, 'data', 'maps_us0.json')))
    on_disc = set(all_rooms())
    rd = ScriptReader()
    out = {}
    for m in maps:
        if m['name'] in on_disc:
            out[m['name']] = rd.room(m['init_rel'], m['name'])
    for room, pit in pits(rd, maps).items():
        if room in out:
            out[room]['doors'] += pit
    for room, elv in elevators(rd, maps).items():
        if room in out:
            have = {(d['name'], d['kind']) for d in out[room]['doors']}
            out[room]['doors'] += [e for e in elv if (e['name'], 'elevator') not in have]
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump(out, open(os.path.join(OUT_DIR, 'scripts.json'), 'w'), indent=1)
    n = sum(len(v['doors']) for v in out.values())
    print(f'{len(out)} rooms, {n} doors/pipes, {sum(len(v["hide"]) for v in out.values())} hidden objects, '
          f'{sum(len(v["mobjs"]) for v in out.values())} blocks/chests/save blocks')


if __name__ == '__main__':
    main()
