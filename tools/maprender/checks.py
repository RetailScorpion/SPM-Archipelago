"""Places the randomizer's checks (data/ap_locations.json) on the room renders, for the viewer.

Writes build/maps/checks.json: for each check its room, where it's drawn in that room's image, and
whether that spot is exact. Also writes build/maps/items.json, the viewer's item grid (TRACKER_GRID),
and copies every icon the viewer uses out of the game's icon sheet (build/maps/icons/<name>.png).

Where a check is drawn:
  - checks with a spawn position (items lying around, chests): exactly there
  - the Pure Heart a chapter ends with: on the room's Star Block, if the init script places one
  - checks in a room the map can't open by hand (graph.py: not linked) but entered through a door from
    one it can: on that door
  - Pixl joins and story gifts: where their scene happens (PINNED, read from their scripts)
  - anything else: an approximate spot along the top of its room
  - rooms left off the map altogether (Chapter 6, the Pits): listed as unplaced

Usage: py -3 tools/maprender/checks.py   (after graph.py; then overview.py rebuilds the viewer)
"""

import json
import math
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import projection  # noqa: E402
from dolasm import Dol  # noqa: E402
import tpl  # noqa: E402

OUT_DIR = os.path.join(ROOT, 'build', 'maps')
DISC = os.path.join(ROOT, 'disc', 'DATA')
ITEM_DATA_TABLE = 0x803B6978  # itemDataTable (spm-headers), 0x2c per item; iconId at +4
ITEM_DATA_SIZE = 0x2C
ICON_ITEMS = list(range(216, 220)) + list(range(221, 232))  # Mario, Peach, Bowser, Luigi; Thoreau to Dashell (Mario: the viewer's live marker)
ICON_ITEMS += __import__('bosses').CARD_ITEMS  # the bosses' catch cards (bosses.py)
AP_ID_BASE = 0x53504D00     # apworld/spm/data.py ID_BASE: an item's Archipelago id is this + its number
HEART_ITEM_FIRST = 1001     # Pure Hearts 1-8 are numbers 1001-1008 (apworld/spm/items.py)
ICON_PURE_HEART_FIRST = 0x31  # the game's Pure Heart icons, red (1) to white (8) (mod archipelago_item.cpp)
THRONE_KEY = 'throne_key'     # the grid's Throne Room Key (mod endgame_requirements.cpp)
THRONE_KEY_PNG = os.path.join(ROOT, 'assets', 'icons', 'throne_room_key.png')  # its own icon (wicon 409)
# items the mod gives a picture of their own (tools/wicon.py), instead of the game item's icon
OWN_ICONS = {1200: os.path.join(ROOT, 'assets', 'icons', 'progressive_wallet.png')}  # not a game item (wallet.h)
# 45: the Save Block (a peach in the game): its icon is drawn from the disc's save block, as the patcher does
SAVE_BLOCK_ITEM = 45
SAVE_BLOCK_TPL = os.path.join(ROOT, 'disc', 'DATA', 'files', 'a', 'MOBJ_save_block-')


def save_block_png(path):
    """Writes the Save Block's icon (patcher.build_save_block_icon) as a PNG."""
    import struct
    import sys
    from PIL import Image
    sys.path.insert(0, os.path.join(ROOT, 'apworld', 'spm'))
    import patcher
    width, height, data = patcher.build_save_block_icon(open(SAVE_BLOCK_TPL, 'rb').read())
    im = Image.new('RGBA', (width, height))
    for i, (x, y) in enumerate(patcher._tile_order(width, height)):
        im.putpixel((x, y), patcher._rgb5a3_to_rgba(struct.unpack_from('>H', data, 2 * i)[0]))
    im.save(path)

# The viewer's item grid: (group title, [(game item number, name)]). Every picture comes from the
# game's own icon sheet: an item's icon from the item table, a Pure Heart's from its icon number.
TRACKER_GRID = [
    # Mario is an item to find when starting_character isn't Mario
    ('Characters', [(216, 'Mario'), (217, 'Peach'), (218, 'Bowser'), (219, 'Luigi')]),
    ('Pixls', [(221, 'Thoreau'), (222, 'Boomer'), (223, 'Slim'), (224, 'Thudley'), (225, 'Carrie'),
               (226, 'Fleep'), (227, 'Cudge'), (228, 'Dottie')]),
    # Tatl (the unused Tippi Pixl, item 220): with tattle_checks, needed for the Tattle locations
    ('Optional Pixls', [(229, 'Piccolo'), (230, 'Barry'), (231, 'Dashell'), (220, 'Tatl')]),
    ('Pure Hearts', [(HEART_ITEM_FIRST + n - 1, f'Pure Heart {n}') for n in range(1, 9)]),
    # A kind of key there are several of is one tile counting them all (user, 2026-11-07): (item numbers,
    # name, how many in the pool). Several numbers: the same key as separate game items (Fort Keys 19-21)
    ('Chapter 1', [(16, 'Ruins Key', 3)]),
    ('Chapter 2', [(17, 'Door Key'), (18, 'House Key')]),
    ('Chapter 3', [((19, 20, 21), 'Fort Key', 3)]),
    ('Chapter 4', [(24, 'Helmet'), (26, 'Door Key'), ((27, 28, 29), 'Dimension Key', 3)]),
    ('Chapter 5', [(30, 'Water Tablet'), (31, 'Stone Tablet'), (32, 'Fire Tablet'), ((33, 34), 'Cave Key', 2),
                   (35, 'Card Key')]),
    # Chapter 7: the Underwhere / Underwhere Road door keys (49, 37, 38) and the Overthere's two (40), and
    # the Overthere's three orbs
    ('Chapter 7', [((49, 37, 38, 40), 'Door Key', 5), (41, 'Blue Orb'), (42, 'Yellow Orb'), (43, 'Red Orb')]),
    ('Flipside & Flopside', [(57, 'Cooking Disk 2'), (58, 'Cooking Disk 3'), (59, 'Cooking Disk 4'),
                             (60, 'Cooking Disk 5'), (61, 'Cooking Disk 6'), (64, 'Golden Card')]),
    # Not an Archipelago item: the game gives it once the final requirement is met, so the viewer works
    # that out (its own icon, assets/icons/throne_room_key.png)
    ('Chapter 8', [(46, 'Castle Bleck Key', 7), (THRONE_KEY, 'Throne Room Key')]),
    # The Save Block (an option): the mod's own item, on game item 45's number; belongs to no chapter.
    # Progressive Wallets (progressive_wallet option): three, or four with_infinite (apworld __init__.py;
    # connected, the viewer takes the seed's own number from its slot data)
    ('Other', [(45, 'Save Block'), (1200, 'Progressive Wallet', 4)]),
]
APPROX_ROW_Y = 40  # where checks without a known spot go: this far below the top of the image
VIA_SPACING = 22   # pixels between the markers of checks drawn on the door into their room

# Checks whose scene sets the spot at run time, pinned by hand from their scripts, keyed by name:
# ('chest', name) = that chest in graph.json (the Pixl comes out of it), ('pos', [x, y, z]) = a script
# position (10x model units). Map object centres were read from map.dat (mapdat.World.meshes).
# Locations whose spawning script is shared by two rooms, so the scan named the wrong one (the
# location's own name keeps the scan's map, so seeds and YAMLs don't change): the room it's really in
REAL_ROOM = {
    '4-2 Planet Blobule - Space Food (sp2_04)': 'sp2_05',  # user, 2026-11-04: found Mario standing on it
    # placed by ls2_06's init script (evt_item_entry at relF 0x1d84b8); the scan filed it under ls2_05
    '8-2 Castle Bleck Foyer - Thunder Rage (ls2_05)': 'ls2_06',  # user, 2026-11-05: not in ls2_05
    # one script for both rooms; the box it reads the position of is sp4_05's
    '4-4 The Whoa Zone - Dimension Key (sp4_04)': 'sp4_05',
}

PINNED = {
    '1-2 Mount Lineland - Thoreau joins (he2_07)': ('chest', 'box'),
    '2-1 Gloam Valley - Boomer joins (mi1_07)': ('chest', 'box'),
    "2-3 Merlee's Mansion - Slim joins (mi3_01)": ('chest', 'box'),
    '3-2 The Tile Pool - Thudley joins (ta2_04)': ('chest', 'box'),
    'Flipside/Flopside - Piccolo joins (mac_19)': ('chest', 'box'),
    '3-4 Fort Francis - Carrie joins (ta4_12)': ('pos', [600, 0, 0]),  # Mario is placed here for the scene
    '4-2 Planet Blobule - Fleep joins (sp2_01)': ('pos', [-1100, 160, -125]),  # Fleep's NPC
    '5-2 Gap of Crag - Cudge joins (gn2_02)': ('pos', [-250, 1925, -156]),  # on the rock tou_iwa_02
    '5-4 Floro Caverns - Dottie joins (gn4_03)': ('pos', [100, 0, 0]),  # the fleeing Floro Sapien "dassou"
    '3-1 The Bitlands - Barry joins (ta1_01)': ('pos', [-2000, 0, -145]),  # the weed zassou_1 he hides in
    'Flipside/Flopside - Peach joins (mac_01)': ('pos', [-10, -1, -250]),  # where she lies unconscious
    '7-1 The Underwhere - Luigi joins (an1_07)': ('pos', [600, 200, 0]),  # where Luigi is (user, 2026-11-04)
    '3-1 The Bitlands - Bowser joins (ta1_04)': ('pos', [3575, 0, -6]),  # by the castle (user's spot, 2026-11-04)
    '7-1 The Underwhere - Super Shroom Shake (an1_02)': ('pos', [1225, 150, 4]),  # comes out of pipe dokan_6
    # Inside a room that isn't drawn and has no door the map knows: on the door into it, in a drawn room
    '1-2 Mount Lineland - Red & Green Card (he2_05)': ('room_px', 'he2_03', [2289, 559]),  # the house in the background
}


def extract_icons():
    """Copies the icons the viewer uses out of wicon.tpl (icons/<game item number>.png), and writes
    the item grid (items.json)."""
    dol = Dol(os.path.join(DISC, 'sys', 'main.dol'))
    icon_bin = open(os.path.join(DISC, 'files', 'wicon.bin'), 'rb').read()
    tpl_data = open(os.path.join(DISC, 'files', 'wicon.tpl'), 'rb').read()
    textures = {i: (w, h, fmt, off) for i, w, h, fmt, off in tpl.images(tpl_data)}
    count = struct.unpack_from('>H', icon_bin, 0)[0]
    records = struct.unpack_from(f'>{count}H', icon_bin, 2)
    os.makedirs(os.path.join(OUT_DIR, 'icons'), exist_ok=True)

    def save_icon(item):
        if item == THRONE_KEY:
            shutil.copyfile(THRONE_KEY_PNG, os.path.join(OUT_DIR, 'icons', f'{THRONE_KEY}.png'))
            return
        if item in OWN_ICONS:
            shutil.copyfile(OWN_ICONS[item], os.path.join(OUT_DIR, 'icons', f'{item}.png'))
            return
        if item == SAVE_BLOCK_ITEM:
            save_block_png(os.path.join(OUT_DIR, 'icons', f'{item}.png'))
            return
        if item >= HEART_ITEM_FIRST:
            icon = ICON_PURE_HEART_FIRST + item - HEART_ITEM_FIRST
        else:
            icon = struct.unpack('>h', dol.read(ITEM_DATA_TABLE + item * ITEM_DATA_SIZE + 4, 2))[0]
        texture = struct.unpack_from('>H', icon_bin, records[icon] + 8)[0]  # its first frame's picture
        w, h, fmt, off = textures[texture]
        assert fmt == 5, f'item {item}: icon picture format {fmt}, expected RGB5A3'
        tpl.decode_rgb5a3(tpl_data, off, w, h).save(os.path.join(OUT_DIR, 'icons', f'{item}.png'))

    grid = []
    for title, entries in TRACKER_GRID:
        items = []
        for item, name, *total in entries:
            numbers = item if isinstance(item, tuple) else (item,)
            for n in numbers:
                save_icon(n)
            item = numbers[0]
            items.append({'id': item, 'name': name, 'icon': f'icons/{THRONE_KEY}.png'} if item == THRONE_KEY else
                         {'id': AP_ID_BASE + item, 'name': name, 'icon': f'icons/{item}.png',
                          **({'ids': [AP_ID_BASE + n for n in numbers]} if len(numbers) > 1 else {}),
                          **({'total': total[0]} if total else {})})
        grid.append({'title': title, 'items': items})
    for item in ICON_ITEMS:
        save_icon(item)
    json.dump(grid, open(os.path.join(OUT_DIR, 'items.json'), 'w'), indent=1)


HANDED_OVER = ([0, -1000, 0], [0, 0, 0])  # evt_item_entry spots that go straight to Mario (no place in the room)


def script_arg(text):
    """A constant evt argument ('200', '-162.5f') as a number, or None (a variable, a pointer)."""
    try:
        return float(text[:-1]) if text.endswith('f') else int(text)
    except ValueError:
        return None


def spawn_spot(rd, offset, room):
    """Where an item spot comes out, from the evt_item_entry whose item argument is at relF offset:
    ('pos', [x, y, z]) when the call has a constant position, ('px', [x, y]) when it's the position of a
    box (or other object) the room's script spawns (read just before, or the box whose opening it waits
    for when the item goes straight to Mario), or None (handed over by an NPC, placed by a map part)."""
    for k in range(1, 12):
        at = offset - 4 * k
        hdr = struct.unpack_from('>I', rd.data, at)[0]
        if (hdr >> 16) >= k and (hdr & 0xFFFF) < 0x200:
            break
    else:
        return None
    args = [rd.dis.arg(at + 4 + 4 * i) for i in range(hdr >> 16)]
    if not args or args[0] != 'evt_item_entry':
        return None
    pos = [script_arg(a) for a in args[4:7]]
    if None not in pos and pos not in HANDED_OVER:
        return ('pos', pos)
    handed = None not in pos
    # the position was read just before (evt_mobj_get_position("box", LW(0), LW(1), LW(2))), or, handed
    # over, the script waited for a box to open (evt_mobj_wait_animation_end("box"))
    for start in range(at - 0x200, at, 4):
        try:
            seq = [(p, n, a) for p, n, a, _ in rd.instructions(start, 120) if p <= at]
        except Exception:
            continue
        if len(seq) > 2 and seq[-1][0] == at and not any(n.startswith('OP_') for _, n, _ in seq):
            break
    else:
        return None
    want = 'evt_mobj_wait_animation_end' if handed else 'evt_mobj_get_position'
    for _, name, a in reversed(seq[-3:-1] if handed else seq[:-1]):
        if name == 'USER_FUNC' and a and a[0] == want:
            obj = a[1].strip('"')
            boxes = [o for o in room['objects'] if o.get('px') and o['kind'] == 'chest']
            match = [o for o in room['objects'] if o.get('px') and o['name'] == obj] or                 (boxes if obj == 'me' and len(boxes) == 1 else [])  # 'me': the box running the script
            return ('px', match[0]['px']) if match else None
        if not handed and name == 'USER_FUNC' and a and a[0].endswith('_get_position'):
            return None  # an NPC or map part: not in graph.json
    return None


def to_pixels(room, pos):
    """A script position (10x model units) in a room's image (projection.py)."""
    return projection.to_pixels(room, pos)


def main():
    extract_icons()
    graph = json.load(open(os.path.join(OUT_DIR, 'graph.json')))
    rooms = graph['rooms']
    locations = json.load(open(os.path.join(ROOT, 'data', 'ap_locations.json')))['locations']
    # Rooms the map can't open by hand (graph.py: not linked), by the linked room and door that lead into them
    entrances = {}
    for name, room in rooms.items():
        for door in room['doors']:
            dest = rooms.get(door.get('dest_room'))
            if (door.get('dest_room') and (not dest or not dest.get('linked')) and room.get('linked')
                    and door.get('px')):
                entrances.setdefault(door['dest_room'], (name, door))
    names = {loc['name'] for loc in locations}
    assert not set(PINNED) - names, f'pinned checks that no longer exist: {set(PINNED) - names}'
    approx_count = {}
    checks = []
    from scripts import ScriptReader
    rd = ScriptReader()
    for loc in locations:
        if loc['name'] in REAL_ROOM:
            loc = dict(loc, map=REAL_ROOM[loc['name']])
        check = {'name': loc['name'], 'label': loc['label'] or loc['name'].split(' - ', 1)[-1],
                 'where': loc['where'], 'pos': loc['pos'], 'kind': loc['kind'], 'category': loc['category'],
                 'item': loc['item'], 'id': loc['id'], 'map': loc['map'], 'room': None, 'px': None,
                 'exact': False}
        room = rooms.get(loc['map'])
        pin = PINNED.get(loc['name'])
        if not pin and room and loc['kind'] == 'spot' and not loc['pos'] and loc['patches']:
            pin = spawn_spot(rd, loc['patches'][0]['offset'], room)  # keys from boxes and the like
            if pin and pin[0] == 'px':
                pin = ('room_px', loc['map'], pin[1])
        if loc['kind'] == 'tattle':
            pass  # any of the enemy's rooms: on no map (the tracker panel lists them, with the rooms)
        elif pin and pin[0] == 'room_px':
            check.update(room=pin[1], px=pin[2], exact=True, **({'via': 'in the background'} if pin[1] != loc['map'] else {}))
        elif room and pin and pin[0] == 'chest':
            chest = next(o for o in room['objects'] if o['kind'] == 'chest' and o['name'] == pin[1])
            check.update(room=loc['map'], px=chest['px'], exact=True)
        elif room and pin:
            check.update(room=loc['map'], px=to_pixels(room, pin[1]), exact=True)
        elif room and loc['pos']:
            check.update(room=loc['map'], px=to_pixels(room, loc['pos']), exact=True)
        elif room and loc['kind'] == 'heart' and any(o['kind'] == 'star_block' and o.get('px')
                                                      for o in room['objects']):
            star = next(o for o in room['objects'] if o['kind'] == 'star_block' and o.get('px'))
            check.update(room=loc['map'], px=star['px'], exact=True)
        elif (not room or not room.get('linked')) and loc['map'] in entrances:
            via, door = entrances[loc['map']]
            n = approx_count.get(loc['map'], 0)  # several inside: side by side over the door
            approx_count[loc['map']] = n + 1
            check.update(room=via, px=[door['px'][0] + VIA_SPACING * n, door['px'][1] - VIA_SPACING], via=door['name'])
        elif room:
            n = approx_count.get(loc['map'], 0)
            approx_count[loc['map']] = n + 1
            check.update(room=loc['map'], px=[round(room['width'] * (n + 1) / 6, 1), APPROX_ROW_Y])
        checks.append(check)
    json.dump(checks, open(os.path.join(OUT_DIR, 'checks.json'), 'w'), indent=1)
    placed = [c for c in checks if c['room']]
    print(f"{len(checks)} checks: {sum(c['exact'] for c in placed)} exact, "
          f"{len(placed) - sum(c['exact'] for c in placed)} approximate, {len(checks) - len(placed)} unplaced")


if __name__ == '__main__':
    main()
