"""SPM Coords: a small window with Mario's position, the map, and that map's unchecked locations.

For finding a location from the tracker's coordinates (the game's own: x left to right, height,
depth). It only reads the game, like the tracker, and works with any version of the mod.

Usage: py -3 -E coords.py
"""

import json
import os
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from game_memory import connect_dolphin, current_map, mario_position, read_flags
from mailbox import read_server_checks
from tracker import LOCATIONS, checked_state, describe_spot

REFRESH_MS = 200  # five times a second


def unchecked_here(locations, map_name, flags, server):
    """The text listing a map's unchecked locations."""
    left = [loc for loc in locations if loc["map"] == map_name and checked_state(loc, flags, server) == " "]
    if not left:
        return "No unchecked locations in this map"
    rows = []
    for loc in left:
        item = loc["name"].split(" - ", 1)[1].rsplit(" (", 1)[0]
        rows.append(f"[ ] {item}: {describe_spot(loc)}")
    return "Unchecked here:\n" + "\n".join(rows)


def make_window():
    """The window: Mario's position (big), the map name, and the list. Returns them all."""
    root = tk.Tk()
    theme.apply(root)
    root.title("SPM coords")
    root.resizable(True, True)
    font = ("Consolas", 12)
    position = tk.Label(root, font=("Consolas", 16, "bold"), anchor="w", justify="left")
    position.pack(fill="x", padx=8, pady=(8, 0))
    here = tk.Label(root, font=font, anchor="w", justify="left", fg=theme.MUTED)
    here.pack(fill="x", padx=8)
    nearby = tk.Label(root, font=font, anchor="w", justify="left")
    nearby.pack(fill="x", padx=8, pady=(4, 8))
    return root, position, here, nearby


def main():
    # The location list, reloaded when it's regenerated, so a running window stays current
    loaded = {"mtime": None, "locations": []}

    def current_locations():
        mtime = os.path.getmtime(LOCATIONS)
        if mtime != loaded["mtime"]:
            loaded["locations"] = json.load(open(LOCATIONS))["locations"]
            loaded["mtime"] = mtime
        return loaded["locations"]

    root, position, here, nearby = make_window()

    def update():
        try:
            connect_dolphin()
            x, y, z = mario_position()
            map_name = current_map()
            flags = read_flags()
            position.config(text=f"x {x:.0f}   height {y:.0f}   depth {z:.0f}")
            here.config(text=f"Map: {map_name}")
            nearby.config(text=unchecked_here(current_locations(), map_name, flags, read_server_checks()))
        except RuntimeError as e:
            dme.un_hook()
            position.config(text="Waiting for the game")
            here.config(text=str(e))
            nearby.config(text="")
        root.after(REFRESH_MS, update)

    update()
    root.mainloop()


if __name__ == "__main__":
    main()
