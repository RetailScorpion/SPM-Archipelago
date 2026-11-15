"""The map tracker, started from the Archipelago Launcher ("Super Paper Mario Tracker").

It shows the map viewer (viewer.html and its pictures, built by tools/maprender) and hands the page the
running game's state. The pictures aren't in the APworld: they're renders of the game's own maps, so they
stay in a folder of their own, chosen the first time and remembered in
%LOCALAPPDATA%/SPM_Archipelago/tracker.json.

The map opens in its own window (pywebview), the way tools/maprender/tracker.pyw does it. The Launcher's
own Python can't add packages, so unless Archipelago runs from source, the window is run by a Python on the
computer that has pywebview and dolphin_memory_engine (found through the py launcher), importing this
package straight from the APworld. With no such Python, it serves the folder on http://localhost:8765 and
opens the browser there, the way tools/maprender/serve.py does.

game_state() is also what serve.py and tracker.pyw send the page (/game.json), so the three stay the same.
"""

import http.server
import json
import os
import struct
import threading

import dolphin_memory_engine as dme

from .game_link import GP_PTR, GSWF_OFFSET, Mailbox, read_server_checks
from .locations import LOCATION_LIST, location_id

SETTINGS_PATH = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"),
                             "SPM_Archipelago", "tracker.json")
PORT = 8765
WINDOW_PORT = 8766  # the window's own server: fixed, so the page's saved settings (per address) stay

GAME_ID = b"R8PE01"        # Super Paper Mario (USA)
MAP_NAME_OFFSET = 0x44     # in the game's main state: the current map's name
GSW_OFFSET = 0x544         # in it: the save bytes
STORY_SLOTS_GSW = 1792     # each chapter's own story value, 2 bytes, high first (mod save_data.h AP_GSW_STORY_SLOTS)
LSWF_OFFSET = 0xD44        # in it: the level's own save flags (spmario.h lswf) ...
LSW_OFFSET = 0xD84         # ... and save bytes (lsw), for rooms shown by them (stages.py 'choice')
LSW_COUNT = 32
MARIO_WORK = 0x804CD458    # Mario's state (US 1.0)
MARIO_POSITION = 0x5C      # in it: his position, x, y, z
MARIO_GRAV_UP = 0x1370     # the unit vector pointing "up" under his gravity (mario.h gravUnitUp)
MARIO_CHARACTER = 0x38     # who's being played, 0 Mario, 1 Peach, 2 Bowser, 3 Luigi (mario.h)
CHARACTERS = ["Mario", "Peach", "Bowser", "Luigi"]
HEART_FLAG_BASE = 7168 + 24  # Pure Heart 1's save flag (mod save_data.h AP_GSWF_HEART)
HEART_COUNT = 8

_lock = threading.Lock()  # one Dolphin read at a time


def _connect():
    if not dme.is_hooked():
        dme.hook()
    if not dme.is_hooked():
        raise RuntimeError("Dolphin isn't running, or no game is loaded")
    if dme.read_bytes(0x80000000, 6) != GAME_ID:
        raise RuntimeError("Super Paper Mario (USA) isn't running")


def game_state(locations=None) -> dict:
    """What the map's panels show, read from the running game (it only reads):
      {"ok": true, "map", "pos": [x, y, z], "up": [x, y, z], "character", "story": [Chapters 1-8],
       "lswf", "lsw", "checked": [location ids the save collected], "server": [ids the server has, while
       the client runs], "hearts": [Pure Hearts held], "flags": "<the 8192 save flags, hex>"}
    or {"ok": false, "error": "..."} while Dolphin or the game isn't running.
    locations: [(location id, save flag)], the APworld's own list if not given."""
    if locations is None:
        locations = [(location_id(i), loc.flag) for i, loc in enumerate(LOCATION_LIST)]
    with _lock:
        try:
            _connect()
            gp = dme.read_word(GP_PTR)
            flags = dme.read_bytes(gp + GSWF_OFFSET, 1024)
            state = {
                "ok": True,
                "map": dme.read_bytes(gp + MAP_NAME_OFFSET, 32).split(b"\0")[0].decode(errors="replace"),
                "pos": list(struct.unpack(">3f", dme.read_bytes(MARIO_WORK + MARIO_POSITION, 12))),
                "up": list(struct.unpack(">3f", dme.read_bytes(MARIO_WORK + MARIO_GRAV_UP, 12))),
                "character": CHARACTERS[dme.read_bytes(MARIO_WORK + MARIO_CHARACTER, 1)[0] & 3],
                "story": list(struct.unpack(">8H", dme.read_bytes(gp + GSW_OFFSET + STORY_SLOTS_GSW, 16))),
                "lswf": dme.read_word(gp + LSWF_OFFSET),
                "lsw": list(dme.read_bytes(gp + LSW_OFFSET, LSW_COUNT)),
            }
        except RuntimeError as e:
            dme.un_hook()
            return {"ok": False, "error": str(e)}
    server = read_server_checks()
    state["checked"] = [i for i, flag in locations if Mailbox.flag_set(flags, flag)]
    state["server"] = [i for i, flag in locations if flag in server]
    state["hearts"] = [n for n in range(1, HEART_COUNT + 1) if Mailbox.flag_set(flags, HEART_FLAG_BASE + n - 1)]
    state["flags"] = flags.hex()  # every save flag, for the map's staged rooms: word n >> 5, bit n & 31
    return state


# ---- Where the map is ----

def _saved_folder() -> str | None:
    try:
        with open(SETTINGS_PATH) as f:
            return json.load(f)["maps"]
    except (OSError, ValueError, KeyError):
        return None


def _has_viewer(folder) -> bool:
    return bool(folder) and os.path.isfile(os.path.join(folder, "viewer.html"))


def maps_folder() -> str | None:
    """The folder with viewer.html: the remembered one, or asks for it (and remembers it)."""
    folder = _saved_folder()
    if _has_viewer(folder):
        return folder
    import tkinter
    from tkinter import filedialog, messagebox
    root = tkinter.Tk()
    root.withdraw()
    while True:
        folder = filedialog.askdirectory(title="Super Paper Mario Tracker: the map folder (with viewer.html)")
        if not folder:
            root.destroy()
            return None
        if _has_viewer(folder):
            break
        messagebox.showerror("Super Paper Mario Tracker", f"There's no viewer.html in\n{folder}")
    root.destroy()
    os.makedirs(os.path.dirname(SETTINGS_PATH), exist_ok=True)
    with open(SETTINGS_PATH, "w") as f:
        json.dump({"maps": folder}, f)
    return folder


# ---- Showing it ----

class _Api:
    """What the page can call in the pywebview window (window.pywebview.api.*)."""

    def game_state(self):
        return game_state()

    def logic(self, received, slot_data):
        return logic(received, slot_data)


def _open_window(folder) -> bool:
    """The map in its own window, if pywebview is there."""
    try:
        import webview
    except ImportError:
        return False
    storage = os.path.join(os.path.dirname(SETTINGS_PATH), "tracker_window")
    # The page comes from our own server rather than pywebview's: it tells the window to check every file
    # before using its cached copy, so a rebuilt viewer.html shows at once while the pictures, unchanged,
    # come from the cache (pywebview's let it keep an old viewer.html, 2026-11-08). Same address as
    # pywebview's (127.0.0.1:8766), so the page's saved settings stay.
    server = http.server.ThreadingHTTPServer(("127.0.0.1", WINDOW_PORT), _handler(folder))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    webview.create_window("SPM Tracker", f"http://127.0.0.1:{WINDOW_PORT}/viewer.html", js_api=_Api(),
                          width=1400, height=900)
    webview.start(private_mode=False, storage_path=storage)
    return True


def logic(received, slot_data) -> dict:
    """The locations the received items reach, by the APworld's own rules (tracker_logic.py): the items tab
    greys out items whose locations aren't reachable yet."""
    from .tracker_logic import reachable
    return {"reachable": reachable(received or [], slot_data or {})}


def _handler(folder):
    """Serves the map folder plus /game.json. "no-cache": the browser asks each time and gets "not
    modified" for files that haven't changed, so it uses its cached copy only while it's current."""

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=folder, **kwargs)

        def end_headers(self):
            self.send_header("Cache-Control", "no-cache")
            super().end_headers()

        def do_GET(self):
            if self.path.split("?")[0] != "/game.json":
                return super().do_GET()
            body = json.dumps(game_state()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            if self.path.split("?")[0] != "/logic.json":
                return self.send_error(404)
            ask = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            body = json.dumps(logic(ask.get("received"), ask.get("slot_data"))).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):  # quiet: the panels ask twice a second
            pass

    return Handler


def _serve(folder):
    """Serves the map folder plus /game.json on localhost and opens the browser there."""
    import webbrowser
    Handler = _handler(folder)

    try:
        server = http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    except OSError:
        # already serving (another tracker, or serve.py): just open the page
        webbrowser.open(f"http://localhost:{PORT}/viewer.html")
        return
    webbrowser.open(f"http://localhost:{PORT}/viewer.html")
    server.serve_forever()


_NO_WINDOW = 0x08000000  # CREATE_NO_WINDOW: no console flashing up for the checks
_DETACHED = 0x00000008   # DETACHED_PROCESS: the window outlives the Launcher's process for it


def _windowed_python() -> str | None:
    """A Python on this computer (from the py launcher's list) that has pywebview and
    dolphin_memory_engine: its pythonw.exe, or None."""
    import subprocess
    try:
        listing = subprocess.run(["py", "-0p"], capture_output=True, text=True, timeout=10,
                                 creationflags=_NO_WINDOW).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    for line in listing.splitlines():
        python = line.split()[-1] if line.strip() else ""
        if not python.lower().endswith("python.exe") or not os.path.isfile(python):
            continue
        try:
            found = subprocess.run([python, "-c", "import webview, dolphin_memory_engine"], capture_output=True,
                                   timeout=20, creationflags=_NO_WINDOW).returncode == 0
        except (OSError, subprocess.SubprocessError):
            continue
        if found:
            pythonw = os.path.join(os.path.dirname(python), "pythonw.exe")
            return pythonw if os.path.isfile(pythonw) else python
    return None


# Run by that Python: this package straight from the APworld (a zip Python can import from), without its
# __init__ (that one needs Archipelago), then the window.
_BOOTSTRAP = """
import sys, types
package = types.ModuleType("spm_tracker")
package.__path__ = [{package!r}]
sys.modules["spm_tracker"] = package
from spm_tracker.tracker_app import window_main
window_main({folder!r})
"""


def _open_window_elsewhere(folder) -> bool:
    """The map in its own window through another Python (the Launcher's own can't add pywebview)."""
    import subprocess
    python = _windowed_python()
    if python is None:
        return False
    code = _BOOTSTRAP.format(package=os.path.dirname(os.path.abspath(__file__)), folder=folder)
    subprocess.Popen([python, "-c", code], creationflags=_DETACHED | _NO_WINDOW, close_fds=True)
    return True


def window_main(folder):
    """The window, run by _open_window_elsewhere's Python. With no console, an error goes to a message box."""
    try:
        _open_window(folder)
    except Exception:
        import ctypes
        import traceback
        ctypes.windll.user32.MessageBoxW(0, traceback.format_exc()[-1500:], "Super Paper Mario Tracker: error", 0x10)


def launch(*args):
    folder = maps_folder()
    if folder and not _open_window(folder) and not _open_window_elsewhere(folder):
        _serve(folder)
