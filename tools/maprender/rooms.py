"""Room names (ref/map_names.txt) and which overview area each room belongs to."""
import os
import re

from spmfiles import REPO, all_rooms, map_files

CHAPTERS = {'he': 1, 'mi': 2, 'ta': 3, 'sp': 4, 'gn': 5, 'wa': 6, 'an': 7, 'ls': 8}
FLIPSIDE = {'mac_01', 'mac_02', 'mac_03', 'mac_04', 'mac_05', 'mac_06', 'mac_07', 'mac_08', 'mac_09',
            'mac_22', 'mac_30'}


def names():
    """{room: description} from ref/map_names.txt."""
    out = {}
    for line in open(os.path.join(REPO, 'ref', 'map_names.txt'), encoding='utf-8-sig'):
        m = re.match(r'^([a-z0-9]{3}_\d\d):\s*(.*)$', line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


# rooms left out altogether (not rendered): cutscene/boss-only rooms that aren't part of a level's layout
# (the boss rooms sp4_17, Brobot's fight, and ls4_12, the wedding and final battle, are drawn again since
# the viewer can follow Mario into them: user, 2026-11-04)
EXCLUDED = set()  # every room is drawn now (mac_22, the cutscene copy of Flipside Tower, too: user, 2026-11-05)


def playable_rooms():
    n = names()
    return [r for r in all_rooms() if 'UNUSED' not in n.get(r, '') and r not in EXCLUDED]


def drawn_as():
    """{room: the room whose render it shares}, for rooms that load the same map file as another (Chapter 6's
    gates: wa1_03 to wa1_25 all load wa1_02's file and share wa1_02's image). The first room by name that
    uses a file is the one rendered. Rooms with a file of their own aren't listed."""
    users = {}
    for r in playable_rooms():
        users.setdefault(map_files().get(r, r), []).append(r)
    return {r: rs[0] for rs in users.values() if len(rs) > 1 for r in sorted(rs)[1:]}


def rendered_rooms():
    """The rooms render.py draws: every playable room except those sharing another's image."""
    shared = drawn_as()
    return [r for r in playable_rooms() if r not in shared]


def area(room):
    """Overview area id: 'chapter1'..'chapter8', 'flipside', 'flopside', 'pits' or 'other'."""
    pre = room[:2]
    if pre in CHAPTERS and room[2].isdigit():
        return f'chapter{CHAPTERS[pre]}'
    if room.startswith('mac_'):
        return 'flipside' if room in FLIPSIDE else 'flopside'
    if room.startswith('dan_'):
        return 'pits'
    return 'other'


AREA_TITLES = {**{f'chapter{n}': f'Chapter {n}' for n in range(1, 9)},
               'flipside': 'Flipside', 'flopside': 'Flopside', 'pits': 'Pits of 100 Trials',
               'other': 'Other rooms (Mario\'s house, Bowser\'s castle, minigames, ...)'}
