"""Quick auto tracker: which locations you've checked, read live from the game through Dolphin.

Reads each location's save flag (the same way the Archipelago client detects checks) and lists every
location by level and map, with its vanilla item and rough spot. The map you're in is marked.
Refreshes when something changes. Read-only: it never writes to the game.

Positions are the game's own coordinates: x runs left to right along the level, y is height, and z is
depth (negative = further back, only reachable in 3D for most spots).

Usage: py -3 -E tracker.py [--missing]   (--missing: hide checked locations)
"""

import json
import os
import sys
import time

import dolphin_memory_engine as dme

from mailbox import GP_PTR, GSWF_OFFSET, Mailbox

LOCATIONS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "ap_locations.json")
MAP_NAME_OFFSET = 0x44  # SpmarioGlobals mapName
POLL_SECONDS = 1.0


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


def describe_spot(loc):
    if loc["pos"] is None:
        return f"{loc['where']}, position set by the script"
    x, y, z = loc["pos"]
    depth = ", back (3D)" if z < -50 else ""
    return f"{loc['where']} at x {x:g}, height {y:g}{depth}"


def render(locations, flags, here, missing_only):
    checked = [Mailbox.flag_set(flags, loc["flag"]) for loc in locations]
    lines = [f"Super Paper Mario tracker: {sum(checked)}/{len(locations)} checked   (you're in {here})", ""]
    level = None
    for loc, done in zip(locations, checked):
        if missing_only and done:
            continue
        title = loc["name"].split(" - ")[0]
        if title != level:
            level = title
            lines.append(title)
        item = loc["name"].split(" - ", 1)[1].rsplit(" (", 1)[0]
        marker = "<- here" if loc["map"] == here else ""
        lines.append(f"  [{'x' if done else ' '}] {loc['map']:7} {item:28} {describe_spot(loc)} {marker}")
    return "\n".join(lines)


def main():
    missing_only = "--missing" in sys.argv
    data = json.load(open(LOCATIONS))["locations"]
    # Chapter order: by the level title (1-1 ... 8-4), then hubs and the Pit
    locations = sorted(data, key=lambda l: (not l["name"][0].isdigit(), l["name"].split(" - ")[0], l["map"], l["name"]))
    last = None
    while True:
        try:
            connect_dolphin()
            screen = render(locations, read_flags(), current_map(), missing_only)
        except RuntimeError as e:
            dme.un_hook()
            screen = f"Waiting for the game: {e}"
        if screen != last:
            os.system("cls" if os.name == "nt" else "clear")
            print(screen)
            last = screen
        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
