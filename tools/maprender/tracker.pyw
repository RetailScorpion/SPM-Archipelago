#! python3.14
# (the py launcher reads that line: the default Python, 3.15, has no pywebview or dolphin_memory_engine)
"""The map and tracker as a desktop app: double-click this file (or `pyw -3.14 tracker.pyw`).

Opens build/maps/viewer.html in its own window (pywebview, Windows' Edge engine) and hands the page the
running game's state directly (pywebview.api.game_state, the same data serve.py's /game.json gives), so
no server or browser is needed. Needs `py -3.14 -m pip install pywebview` once.

Settings the page keeps (Archipelago connection, panels) are stored in build/maps/.tracker_app between
runs. serve.py + a browser still works the same way.
"""
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
CRASH_LOG = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'build', 'maps', 'tracker_error.txt')


def crashed(text):
    """No console in a .pyw: an error would just vanish, so it goes to a file and a message box."""
    try:
        open(CRASH_LOG, 'w', encoding='utf-8').write(text)
    except OSError:
        pass
    import ctypes
    ctypes.windll.user32.MessageBoxW(0, text[-1500:] + f'\n\n(saved to {CRASH_LOG})', 'SPM Tracker: error', 0x10)


try:
    sys.path.insert(0, HERE)
    import serve  # noqa: E402  (game reading; it also puts client/ on the path)
    import webview  # noqa: E402
except Exception:
    crashed(traceback.format_exc())
    raise SystemExit(1)

VIEWER = os.path.join(serve.MAPS, 'viewer.html')
STORAGE = os.path.join(serve.MAPS, '.tracker_app')
PORT = 8766  # pywebview's own file server inside the app; fixed so the page's saved settings stay put


class Api:
    """What the page can call (window.pywebview.api.*)."""

    def game_state(self):
        return serve.game_state()

    def logic(self, received, slot_data):
        return serve.logic(received, slot_data)


def main():
    if not os.path.exists(VIEWER):
        import ctypes
        ctypes.windll.user32.MessageBoxW(0, f'No map built yet:\n{VIEWER}\n\nRun tools/maprender/overview.py.',
                                         'SPM Tracker', 0x10)
        return
    import shutil  # the window's cache can hand back an old viewer.html after a rebuild; settings stay
    shutil.rmtree(os.path.join(STORAGE, 'EBWebView', 'Default', 'Cache'), ignore_errors=True)
    webview.create_window('SPM Tracker', VIEWER, js_api=Api(), width=1400, height=900)
    webview.start(http_server=True, http_port=PORT, private_mode=False, storage_path=STORAGE)


if __name__ == '__main__':
    try:
        main()
    except Exception:
        crashed(traceback.format_exc())
