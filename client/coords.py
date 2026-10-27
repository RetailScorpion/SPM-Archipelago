"""Small always-on-top window: Mario's position, the map, and that map's unchecked locations.

For finding locations from the tracker's coordinates (the game's own: x left to right, height, depth).
Read-only, like the tracker; works with any mod version.

Usage: py -3 -E coords.py
"""

import json
import tkinter as tk

import dolphin_memory_engine as dme

from mailbox import Mailbox
from tracker import LOCATIONS, connect_dolphin, current_map, describe_spot, mario_position, read_flags

REFRESH_MS = 200


def main():
    locations = json.load(open(LOCATIONS))["locations"]

    root = tk.Tk()
    root.title("SPM coords")
    root.attributes("-topmost", True)
    root.resizable(True, True)
    font = ("Consolas", 12)
    position = tk.Label(root, font=("Consolas", 16, "bold"), anchor="w", justify="left")
    position.pack(fill="x", padx=8, pady=(8, 0))
    here = tk.Label(root, font=font, anchor="w", justify="left", fg="#555")
    here.pack(fill="x", padx=8)
    nearby = tk.Label(root, font=font, anchor="w", justify="left")
    nearby.pack(fill="x", padx=8, pady=(4, 8))

    def update():
        try:
            connect_dolphin()
            x, y, z = mario_position()
            map_name = current_map()
            flags = read_flags()
            position.config(text=f"x {x:.0f}   height {y:.0f}   depth {z:.0f}")
            here.config(text=f"Map: {map_name}")
            left = [loc for loc in locations if loc["map"] == map_name and not Mailbox.flag_set(flags, loc["flag"])]
            if left:
                rows = []
                for loc in left:
                    item = loc["name"].split(" - ", 1)[1].rsplit(" (", 1)[0]
                    rows.append(f"[ ] {item}: {describe_spot(loc)}")
                nearby.config(text="Unchecked here:\n" + "\n".join(rows))
            else:
                nearby.config(text="No unchecked locations in this map")
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
