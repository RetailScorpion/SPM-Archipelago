"""Quick auto tracker: which locations you've checked, read live from the game through Dolphin.

Reads each location's save flag (the same way the Archipelago client detects checks) and lists every
location by level and map, with its vanilla item and rough spot. Every level is listed with its
status (Complete, checked/total, or no checks). The map you're in is marked.
Refreshes when something changes. Read-only: it never writes to the game.

Positions are the game's own coordinates: x runs left to right along the level, y is height, z is
depth. Mario's own position is shown at the top, to compare.

Usage: py -3 -E tracker.py [--missing]   (--missing: hide checked locations)
"""

import json
import os
import struct
import sys
import time

import dolphin_memory_engine as dme

from mailbox import GP_PTR, GSWF_OFFSET, Mailbox

LOCATIONS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "ap_locations.json")
MAP_NAME_OFFSET = 0x44  # SpmarioGlobals mapName
MARIO_WORK = 0x804CD458  # marioGetPtr() (US 1.0)
MARIO_POSITION = 0x5C    # MarioWork position (x, y, z floats)
POLL_SECONDS = 0.5


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
    if loc["pos"] is None:
        return f"{loc['where']}, position set by the script"
    x, y, z = loc["pos"]
    return f"{loc['where']} at x {x:g}, height {y:g}, depth {z:g}"


def render(levels, locations, flags, here, missing_only, mario=None):
    """Every level, with its status; under it, its locations (only the unchecked ones with --missing)."""
    by_level = {title: [] for title in levels}
    for loc in locations:
        by_level.setdefault(loc["name"].split(" - ")[0], []).append((loc, Mailbox.flag_set(flags, loc["flag"])))
    total = sum(len(v) for v in by_level.values())
    done_total = sum(done for v in by_level.values() for _, done in v)
    lines = [f"Super Paper Mario tracker: {done_total}/{total} checked   (you're in {here})"]
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


def main():
    missing_only = "--missing" in sys.argv
    data = json.load(open(LOCATIONS))
    levels = data["levels"]
    locations = sorted(data["locations"], key=lambda l: (l["map"], l["name"]))
    last = None
    while True:
        try:
            connect_dolphin()
            screen = render(levels, locations, read_flags(), current_map(), missing_only, mario_position())
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
