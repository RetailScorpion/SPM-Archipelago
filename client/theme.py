"""Dark colours for the client's windows (tracker, coords, party).

apply(root) right after tk.Tk(): every widget then defaults to the dark palette, and on Windows the
title bar goes dark too. Widgets that pick their own colour use the constants below.
"""

import ctypes
import sys

BACKGROUND = "#1e1e1e"
TEXT = "#d4d4d4"
MUTED = "#9a9a9a"       # secondary lines (status, "you're in")
HIGHLIGHT = "#ff6b6b"   # the selected Pixl in the party editor
BUTTON = "#333333"
SELECTION = "#264f78"
DISABLED = "#6a6a6a"

DWMWA_USE_IMMERSIVE_DARK_MODE = 20  # Windows 10 20H1 and later
SWP_FRAME_ONLY = 0x0027  # SetWindowPos: SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED


def apply(root):
    root.tk_setPalette(background=BACKGROUND, foreground=TEXT, activeBackground=BUTTON, activeForeground=TEXT,
                       highlightBackground=BACKGROUND, highlightColor=MUTED, insertBackground=TEXT,
                       selectBackground=SELECTION, selectForeground=TEXT, selectColor=BUTTON,
                       disabledForeground=DISABLED, troughColor=BUTTON)
    if sys.platform == "win32":
        dark_title_bar(root)
        # Windows only repaints the frame when it changes: again once the window is up
        root.after(100, lambda: dark_title_bar(root))


def dark_title_bar(root):
    root.update_idletasks()
    hwnd = ctypes.windll.user32.GetParent(root.winfo_id())
    on = ctypes.c_int(1)
    ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ctypes.byref(on), ctypes.sizeof(on))
    ctypes.windll.user32.SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_FRAME_ONLY)
