"""Serves the map viewer (build/maps) on http://localhost:8765/viewer.html, plus the running game's
state for its Coords and Tracker panels.

GET /game.json, read through Dolphin like client/coords.py and client/tracker.py (it only reads):
  {"ok": true, "map": "he1_01", "pos": [x, y, z], "up": [which way is up under Mario's gravity], "character": "Mario" (who's being played),
   "story": [Chapters 1-8's story values], "flags": "<the 8192 save flags as hex, 4-byte words>",
   "checked": [location ids the loaded save has
   collected], "server": [location ids the Archipelago server has as checked, while the client runs],
   "hearts": [Pure Hearts held, 1-8]}
or {"ok": false, "error": "..."} while Dolphin or the game isn't running.

POST /logic.json {"received": [item ids], "slot_data": {...}} -> {"reachable": [location ids]}: the APworld's
own rules run on the items received (apworld/spm/tracker_logic.py), for the items tab.

Files are sent with "no-cache", so a rebuilt viewer shows up on a plain reload.

Usage: py -3 -E tools/maprender/serve.py [port]
"""
import http.server
import json
import os
import socket
import struct
import sys
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'client'))
import dolphin_memory_engine as dme  # noqa: E402
from game_memory import connect_dolphin, current_map, mario_position, read_flags  # noqa: E402
from mailbox import Mailbox, read_server_checks  # noqa: E402

MAPS = os.path.join(ROOT, 'build', 'maps')
LOCATIONS = os.path.join(ROOT, 'data', 'ap_locations.json')
HEART_FLAG_BASE = 7168 + 24  # Pure Heart 1's save flag (mod save_data.h AP_GSWF_HEART)
HEART_COUNT = 8

MARIO_WORK = 0x804CD458    # Mario's state (US 1.0; client/game_memory.py)
MARIO_GRAV_UP = 0x1370     # in it: the unit vector pointing "up" under his gravity (mario.h gravUnitUp)
MARIO_CHARACTER = 0x38     # in it: who's being played, 0 Mario, 1 Peach, 2 Bowser, 3 Luigi (mario.h)
CHARACTERS = ['Mario', 'Peach', 'Bowser', 'Luigi']

_lock = threading.Lock()  # one Dolphin read at a time


GP_PTR = 0x8056D198        # the game's main state (client/game_link.py)
GSW_OFFSET = 0x544         # in it: the save bytes
STORY_SLOTS_GSW = 1792     # each chapter's own story value, 2 bytes, high first (mod save_data.h AP_GSW_STORY_SLOTS)
LSWF_OFFSET = 0xD44        # in it: the level's own save flags (spmario.h lswf) ...
LSW_OFFSET = 0xD84         # ... and save bytes (lsw), for rooms shown by them (stages.py 'choice')
LSW_COUNT = 32


def story_values():
    """Each chapter's story value, Chapters 1-8 (the mod keeps them in the save, synced while playing)."""
    raw = dme.read_bytes(dme.read_word(GP_PTR) + GSW_OFFSET + STORY_SLOTS_GSW, 16)
    return list(struct.unpack('>8H', raw))


def level_bytes():
    """The level's own save flags (first 32, as one word) and bytes (the first LSW_COUNT)."""
    gp = dme.read_word(GP_PTR)
    return dme.read_word(gp + LSWF_OFFSET), list(dme.read_bytes(gp + LSW_OFFSET, LSW_COUNT))


def mario_up():
    """Which way is up for Mario: (0, 1, 0) normally; the Whoa Zone's gravity turns it."""
    return list(struct.unpack('>3f', dme.read_bytes(MARIO_WORK + MARIO_GRAV_UP, 12)))
_locations = {'mtime': None, 'list': []}


def locations():
    """[(location id, save flag)], reloaded when the list is regenerated."""
    mtime = os.path.getmtime(LOCATIONS)
    if mtime != _locations['mtime']:
        _locations['list'] = [(loc['id'], loc['flag']) for loc in json.load(open(LOCATIONS))['locations']]
        _locations['mtime'] = mtime
    return _locations['list']


def game_state():
    with _lock:
        try:
            connect_dolphin()
            flags = read_flags()
            state = {'ok': True, 'map': current_map(), 'pos': list(mario_position()), 'up': mario_up(),
                     'character': CHARACTERS[dme.read_bytes(MARIO_WORK + MARIO_CHARACTER, 1)[0] & 3],
                     'story': story_values()}
            state['lswf'], state['lsw'] = level_bytes()
        except RuntimeError as e:
            dme.un_hook()
            return {'ok': False, 'error': str(e)}
    server = read_server_checks()
    locs = locations()
    state['checked'] = [i for i, flag in locs if Mailbox.flag_set(flags, flag)]
    state['server'] = [i for i, flag in locs if flag in server]
    state['hearts'] = [n for n in range(1, HEART_COUNT + 1) if Mailbox.flag_set(flags, HEART_FLAG_BASE + n - 1)]
    state['flags'] = flags.hex()  # every save flag, for the map's staged rooms (stages.py): word n >> 5, bit n & 31
    return state


def logic(received, slot_data):
    """The locations the received items reach, by the APworld's own rules (tracker_logic.py)."""
    if 'spm_logic' not in sys.modules:
        import types
        package = types.ModuleType('spm_logic')  # the APworld's folder, without its __init__ (Archipelago's)
        package.__path__ = [os.path.join(ROOT, 'apworld', 'spm')]
        sys.modules['spm_logic'] = package
    from spm_logic.tracker_logic import reachable
    return {'reachable': reachable(received or [], slot_data or {})}


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=MAPS, **kwargs)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def do_GET(self):
        if self.path.split('?')[0] != '/game.json':
            return super().do_GET()
        body = json.dumps(game_state()).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path.split('?')[0] != '/logic.json':
            return self.send_error(404)
        ask = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))) or b'{}')
        body = json.dumps(logic(ask.get('received'), ask.get('slot_data'))).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):  # quiet: the panels ask twice a second
        pass


class IPv6Server(http.server.ThreadingHTTPServer):
    address_family = socket.AF_INET6


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    # Both of this computer's own addresses: "localhost" tries IPv6 (::1) first on Windows, and with
    # nothing there each request waits about 0.2 s before falling back to IPv4
    servers = [http.server.ThreadingHTTPServer(('127.0.0.1', port), Handler)]
    try:
        servers.append(IPv6Server(('::1', port), Handler))
    except OSError:
        pass  # no IPv6: IPv4 alone still works
    for s in servers[1:]:
        threading.Thread(target=s.serve_forever, daemon=True).start()
    print(f'Map viewer: http://localhost:{port}/viewer.html')
    servers[0].serve_forever()


if __name__ == '__main__':
    main()
