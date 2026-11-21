"""Builds build/maps/graph.json from the renders' framing files (build/maps/raw/<room>.json).

graph.json:
  rooms: {room: {name, area, image, width, height, doors: [...]}}; same_as: the room whose image it shares
     each door: name, kind (map_door | pipe | door), px [x, y] and box [x0, y0, x1, y1] in the room image
     (null when the hit object wasn't found), dest_room, dest_door, located_by
     objects: blocks, chests, save blocks and Star Blocks the init scripts spawn: kind (block | chest | save |
     star_block), type
     (evt_mobj_blk kind incl. flags, or chest type), name, item (chests: item id), model, pos (game units),
     px [x, y] (bottom centre in the room image), conditional (spawned inside an IF in the script)
  edges: [{from, from_door, to, to_door, kind, inferred}]
     inferred = true for the way back through a door only listed on one side
     elevators also have direction: up | down; kind pit = falling in takes Mario there (one way)
Every room is in the graph. rooms[room].linked says whether the doors (and Star Blocks) lead there from
Flipside Tower: the viewer only offers the linked ones to open by hand (the area maps, the level menus) and
shows the others (Mario's house, boss rooms only a fight's script goes to, ...) only while following Mario.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from spmfiles import OUT_DIR  # noqa: E402
from rooms import names, area, playable_rooms, rendered_rooms, drawn_as  # noqa: E402
import projection  # noqa: E402
import stages  # noqa: E402
START = 'mac_02'  # Flipside Tower, where the linked rooms are counted from

RAW = os.path.join(OUT_DIR, 'raw')
# Flipside / Flopside floor numbers, for the up/down direction of elevators
FLOORS = {'mac_01': 3, 'mac_02': 2, 'mac_09': 1, 'mac_04': 0, 'mac_05': -1,
          'mac_11': 3, 'mac_12': 2, 'mac_19': 1, 'mac_14': 0, 'mac_15': -1}


# Corrections to what the scripts say, from playing the game (user):
#   no_dest: doors whose table entry leads nowhere in play (their link is dropped, the door stays drawn)
#   add: ways through that aren't in a door table, such as pits (kind 'pit', one way)
CORRECTIONS = {
    # 3-1: the two red pipes beside the pit are scenery (the table points both at Flipside's Return
    # Pipe); the way on is falling into the pit between them, into the retro underground (now found by
    # scripts.pits, as are mac_06's pit and mac_08's spring that used to be added here by hand)
    'ta1_01': {'no_dest': {'dokan_m', 'dokan_m2'}},
    # 1-2: Green's house (he2_05) is loaded over the village as a frame by the bridge-building scenes, with no
    # door of its own; this one on the house in the village's background opens it on the map (user,
    # 2026-11-16: its check, the Red & Green Card, now shows in the house itself)
    'he2_03': {'add': [{'name': "Green's house", 'name_l': None, 'name_r': None, 'kind': 'door',
                        'px': [2321, 600], 'box': [2299, 579, 2344, 621], 'dest_room': 'he2_05', 'dest_door': None,
                        'located_by': 'by hand'}]},
}


def door_matches(door, name):
    return name is not None and name in (door.get('name'), door.get('name_l'), door.get('name_r'))


def shared_info(info, script):
    """A room drawn with another room's render (rooms.drawn_as): that render's framing and door positions
    (same model, same hit objects), with this room's own door table and objects from its own script."""
    placed = {d['name']: d for d in info.get('doors', [])}
    doors = []
    for d in script.get('doors', []):
        own = dict(placed.get(d['name'], {'px': None, 'box': None, 'located_by': None}))
        own.update({k: d.get(k) for k in ('name', 'name_l', 'name_r', 'kind', 'dest_room', 'dest_door')})
        doors.append(own)
    mobjs = [dict(m, model=None, px=projection.to_pixels(info, m['pos'])) for m in script.get('mobjs', [])
             if m.get('pos')]
    return dict(info, doors=doors, mobjs=mobjs)


def lock_conditions(room, ifs):
    """What a lock's IFs say, as the viewer's conditions for it still being there (all must hold):
    ['story_below', N] / ['story_from', N] (the room's chapter's story value under N / N or more), ['flag_clear', N] /
    ['flag_set', N] (GSWF(N)). None if one can't be read, or a story one has no chapter (Flipside's Old Key
    gate, mac_02, only locked below the story value the mod's hub already starts past)."""
    out = []
    for op, args, in_else in ifs:
        a, b = (args + [None, None])[:2]
        if a == 'GSW(0)' and op in ('IF_SMALL', 'IF_LARGE') and stages.chapter_of(room):
            n = int(b) if op == 'IF_SMALL' else int(b) + 1  # IF_SMALL: below N; IF_LARGE: from N + 1
            out.append(['story_from' if (op == 'IF_LARGE') != in_else else 'story_below', n])
        elif a and a.startswith('GSWF(') and op in ('IF_EQUAL', 'IF_NOT_EQUAL') and b in ('0', '1'):
            on = (op == 'IF_EQUAL') == (b == '1')
            out.append(['flag_set' if on != in_else else 'flag_clear', int(a[5:-1])])
        else:
            return None
    return out


def next_section(room, rooms):
    """The first room of a chapter's next part, which its Star Block leads to (he1_05 -> he2_01), as the
    viewer's nextSection does."""
    if room[:2].isalpha() and room[2].isdigit():
        nxt = f'{room[:2]}{int(room[2]) + 1}_01'
        # the init script places it, or the room's name says it has one (wa2_25 and wa3_25 place theirs in a
        # way scripts.py doesn't read)
        if nxt in rooms and (any(o['kind'] == 'star_block' for o in rooms[room]['objects'])
                             or 'with the Star Block' in rooms[room]['name']):
            return nxt
    return None


def mark_linked(rooms, edges):
    """rooms[r]['linked']: reachable from START through doors, pipes, pits and the like, and Star Blocks."""
    out = {}
    for e in edges:
        out.setdefault(e['from'], set()).add(e['to'])
    seen, todo = {START}, [START]
    while todo:
        r = todo.pop()
        for n in out.get(r, set()) | {next_section(r, rooms)} - {None}:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    for r, v in rooms.items():
        v['linked'] = r in seen


def build():
    desc = names()
    rooms = {}
    shared = drawn_as()
    scripts = json.load(open(os.path.join(OUT_DIR, 'scripts.json')))
    for room in playable_rooms():
        image = shared.get(room, room)  # rooms on the same map file share one render
        path = os.path.join(RAW, f'{image}.json')
        if not os.path.exists(path):
            continue
        info = json.load(open(path))
        if 'error' in info:
            continue
        if image != room:
            info = shared_info(info, scripts.get(room, {}))
        doors = []
        for d in info.get('doors', []):
            doors.append({k: d.get(k) for k in ('name', 'name_l', 'name_r', 'kind', 'px', 'box',
                                                 'dest_room', 'dest_door', 'located_by')})
            if d.get('kind') == 'elevator' and room in FLOORS and d.get('dest_room') in FLOORS:
                doors[-1]['direction'] = 'up' if FLOORS[d['dest_room']] > FLOORS[room] else 'down'
        fix = CORRECTIONS.get(room, {})
        for d in doors:
            if d['name'] in fix.get('no_dest', ()):
                d['dest_room'] = d['dest_door'] = None
        doors += [dict(d) for d in fix.get('add', [])]
        rooms[room] = {'name': desc.get(room, ''), 'area': area(room), 'image': f'rooms/{image}.png',
                       'width': info['width'], 'height': info['height'], 'ppu': info['ppu'],
                       'pitch_deg': info['pitch_deg'], 'x0': info['x0'], 'y0': info['y0'],
                       'yaw_deg': info.get('yaw_deg', 0.0), 'right': info.get('right'), 'up': info.get('up'),
                       'turned': bool(info.get('turned')),  # drawn from the other side: (x, z) -> (-x, -z)
                       'doors': doors, **({'same_as': image} if image != room else {}),
                       'objects': [{k: m.get(k) for k in ('kind', 'type', 'name', 'item', 'model', 'pos', 'px',
                                                          'conditional')}
                                   for m in info.get('mobjs', [])]}
        locks = []
        for m in info.get('locks', []):
            cond = lock_conditions(room, m.get('ifs', []))
            if m.get('img') and cond is not None:
                locks.append({'name': m['name'], 'item': m.get('item'), 'img': m['img'], 'left': m['left'],
                              'top': m['top'], 'chapter': stages.chapter_of(room), 'while': cond})
        if locks:
            rooms[room]['locks'] = locks

    # Chapter doors: the tower door into a chapter arrives at a door in its first room whose own entry
    # leads back to itself (the game's real way back is the Return Pipe item). Those lead back to the tower
    # door instead, marked 'return' (user, 2026-11-04)
    for hub in ('mac_02', 'mac_12'):
        for hd in rooms.get(hub, {}).get('doors', []):
            target = rooms.get(hd.get('dest_room'))
            if not target or hd['dest_room'] == hub:
                continue
            for d in target['doors']:
                if door_matches(d, hd.get('dest_door')) and d.get('dest_room') == hd['dest_room']:
                    d.update(dest_room=hub, dest_door=hd['name'], ret=True)

    edges, seen = [], set()
    for room, r in rooms.items():
        for d in r['doors']:
            if not d['dest_room']:
                continue
            target = d['dest_room']
            if target not in rooms:
                continue
            to_door = next((x['name'] for x in rooms[target]['doors'] if door_matches(x, d['dest_door'])),
                           d['dest_door'])
            edges.append({'from': room, 'from_door': d['name'], 'to': target, 'to_door': to_door,
                          'kind': d['kind'], 'inferred': False})
            seen.add((room, d['name'], target))
    for e in list(edges):
        if e['kind'] == 'pit' or (e['to'], e['to_door'], e['from']) in seen:  # pits: one way
            continue
        if any(x['from'] == e['to'] and x['to'] == e['from'] for x in edges):
            continue
        edges.append({'from': e['to'], 'from_door': e['to_door'], 'to': e['from'], 'to_door': e['from_door'],
                      'kind': e['kind'], 'inferred': True})
        seen.add((e['to'], e['to_door'], e['from']))
    mark_linked(rooms, edges)
    graph = {'note': 'Pixel positions are in build/maps/rooms/<room>.png. To place a game-space point '
                     '(script units) in a room image: m = point / 10 (x, z negated if turned); px = (dot(m, right) - x0) * '
                     'ppu; py = (y0 - dot(m, up)) * ppu. See tools/maprender/projection.py.',
             'rooms': rooms, 'edges': edges}
    json.dump(graph, open(os.path.join(OUT_DIR, 'graph.json'), 'w'), indent=1)
    return graph


def notes(graph):
    """build/maps/NOTES.md: rooms that failed, doors with no position, parts left out of each render."""
    lines = ['# Map render notes', '', 'Generated by tools/maprender/graph.py.', '']
    failed = []
    for room in rendered_rooms():
        path = os.path.join(RAW, f'{room}.json')
        if not os.path.exists(path):
            failed.append(f'{room} (not rendered)')
            continue
        info = json.load(open(path))
        if 'error' in info:
            failed.append(f"{room} ({info['error']})")
    lines += ['## Rooms that did not render', ''] + [f'- {f}' for f in failed or ['none']] + ['']
    lines += ['## Doors and pipes without a position', '',
              'Their hit object was not found in the model; they are in graph.json with px = null.', '']
    for room, r in sorted(graph['rooms'].items()):
        miss = [d['name'] for d in r['doors'] if not d.get('px')]
        if miss:
            lines.append(f"- {room}: {', '.join(str(m) for m in miss)}")
    lines += ['', '## Rooms with no door or pipe table in the init script', '']
    lines += [f"- {room} {r['name']}" for room, r in sorted(graph['rooms'].items()) if not r['doors']]
    lines += ['', '## Parts left out of each render', '',
              'Hidden by the init script, or skipped by the guesses in render_blender.skip_reason.', '']
    for room in sorted(r for r, v in graph['rooms'].items() if not v.get('same_as')):
        info = json.load(open(os.path.join(RAW, f'{room}.json')))
        hid, sk = info.get('hidden_by_script', []), info.get('skipped_parts', [])
        if hid or sk:
            lines.append(f"- {room}: script hides {len(hid)} ({', '.join(hid[:6])}{', ...' if len(hid) > 6 else ''}); "
                         f"skipped {len(sk)} parts")
    open(os.path.join(OUT_DIR, 'NOTES.md'), 'w', encoding='utf-8').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    g = build()
    notes(g)
    n = sum(len(r['doors']) for r in g['rooms'].values())
    unplaced = sum(1 for r in g['rooms'].values() for d in r['doors'] if d['px'] is None)
    print(f"{len(g['rooms'])} rooms, {n} doors ({unplaced} without a position), {len(g['edges'])} edges "
          f"({sum(e['inferred'] for e in g['edges'])} inferred)")
