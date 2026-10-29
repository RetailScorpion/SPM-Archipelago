"""Quick auto tracker: which locations you've checked, read live from the game through Dolphin.

Reads each location's save flag (the same way the Archipelago client detects checks) and lists every
location by level and map, with its vanilla item and rough spot (or how to trigger it, for the
story gifts: character joins, the Helmet, Bestovius's lesson), plus the Pure Hearts held. Every level is listed with its
status (Complete, checked/total, or no checks). The map you're in is marked.
Refreshes when something changes, in its own window that keeps where you've scrolled to. Read-only:
it never writes to the game.

Positions are the game's own coordinates: x runs left to right along the level, y is height, z is
depth. coords.py shows Mario's own position, to compare.

Usage: pyw -3 -E tracker.py [--missing]   (--missing: hide checked locations)
"""

import json
import os
import struct
import sys
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from mailbox import GP_PTR, GSWF_OFFSET, Mailbox

LOCATIONS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "ap_locations.json")
MAP_NAME_OFFSET = 0x44  # SpmarioGlobals mapName
MARIO_WORK = 0x804CD458  # marioGetPtr() (US 1.0)
MARIO_POSITION = 0x5C    # MarioWork position (x, y, z floats)
POLL_MS = 1000
HEART_FLAG_BASE = 7168 + 24  # AP_GSWF_HEART(1) (mod/include/savedata.h)
HEART_COUNT = 7              # the tower's doors


def connect_dolphin():
    """Only the game is needed, not the mod's mailbox: the save flags work with any mod version."""
    if not dme.is_hooked():
        dme.hook()
    if not dme.is_hooked():
        raise RuntimeError("Dolphin isn't running, or no game is loaded")
    if dme.read_bytes(0x80000000, 6) != b"R8PE01":
        raise RuntimeError("Super Paper Mario (USA) isn't running")


def read_flags():
    return dme.read_bytes(dme.read_word(GP_PTR) + GSWF_OFFSET, 1024)


def current_map():
    return dme.read_bytes(dme.read_word(GP_PTR) + MAP_NAME_OFFSET, 32).split(b"\0")[0].decode(errors="replace")


def mario_position():
    return struct.unpack(">3f", dme.read_bytes(MARIO_WORK + MARIO_POSITION, 12))


def describe_spot(loc):
    if loc["kind"] == "gift":
        return loc["where"]
    if loc["pos"] is None:
        return f"{loc['where']}, position set by the script"
    x, y, z = loc["pos"]
    return f"{loc['where']} at x {x:g}, height {y:g}, depth {z:g}"


def pure_hearts(flags):
    """'Pure Hearts: 1 3 5 7' from the mod's heart flags (doors.h), or a note when the save has none."""
    held = [n for n in range(1, HEART_COUNT + 1) if Mailbox.flag_set(flags, HEART_FLAG_BASE + n - 1)]
    if not held:
        return "Pure Hearts: none (every door is open without the client)"
    return "Pure Hearts (doors open): " + " ".join(map(str, held))


def render(levels, locations, flags, here, missing_only, mario=None):
    """Every level, with its status; under it, its locations (only the unchecked ones with --missing)."""
    by_level = {title: [] for title in levels}
    for loc in locations:
        by_level.setdefault(loc["name"].split(" - ")[0], []).append((loc, Mailbox.flag_set(flags, loc["flag"])))
    total = sum(len(v) for v in by_level.values())
    done_total = sum(done for v in by_level.values() for _, done in v)
    lines = [f"Super Paper Mario tracker: {done_total}/{total} checked   (you're in {here})", pure_hearts(flags)]
    if mario is not None:
        x, y, z = mario
        lines.append(f"Mario: x {x:.0f}, height {y:.0f}, depth {z:.0f}")
    lines.append("")
    for title, entries in by_level.items():
        done = sum(d for _, d in entries)
        if not entries:
            status = "no checks"
        elif done == len(entries):
            status = "Complete"
        else:
            status = f"{done}/{len(entries)}"
        lines.append(f"{title}  -  {status}")
        for loc, checked in entries:
            if missing_only and checked:
                continue
            item = loc["name"].split(" - ", 1)[1].rsplit(" (", 1)[0]
            marker = "<- here" if loc["map"] == here else ""
            lines.append(f"  [{'x' if checked else ' '}] {loc['map']:7} {item:28} {describe_spot(loc)} {marker}")
    return "\n".join(lines)


def load_locations():
    data = json.load(open(LOCATIONS))
    return data["levels"], sorted(data["locations"], key=lambda l: (l["map"], l["name"]))


def main():
    missing_only = "--missing" in sys.argv
    root = tk.Tk()
    theme.apply(root)
    root.title("SPM tracker")
    root.geometry("1100x800")
    scroll = tk.Scrollbar(root)
    scroll.pack(side="right", fill="y")
    text = tk.Text(root, font=("Consolas", 11), wrap="none", yscrollcommand=scroll.set)
    text.pack(side="left", fill="both", expand=True)
    scroll.config(command=text.yview)
    state = {"last": None, "mtime": None, "levels": None, "locations": None}

    def refresh():
        # Reload when the data is regenerated (new locations), so a running tracker stays current
        mtime = os.path.getmtime(LOCATIONS)
        if mtime != state["mtime"]:
            state["levels"], state["locations"] = load_locations()
            state["mtime"] = mtime
        try:
            connect_dolphin()
            # No Mario position here (coords.py shows it): the text only changes when a check or the map does
            screen = render(state["levels"], state["locations"], read_flags(), current_map(), missing_only)
        except RuntimeError as e:
            dme.un_hook()
            screen = f"Waiting for the game: {e}"
        if screen != state["last"]:
            # Replace the text but stay scrolled where you were
            top = text.yview()[0]
            text.config(state="normal")
            text.delete("1.0", "end")
            text.insert("1.0", screen)
            text.config(state="disabled")
            text.yview_moveto(top)
            state["last"] = screen
        root.after(POLL_MS, refresh)

    refresh()
    root.mainloop()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
