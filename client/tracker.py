"""SPM Tracker: which locations you've checked, read live from the game through Dolphin.

It reads each location's save flag (the same way the Archipelago client notices checks) and lists
every location by level and map, with its vanilla item and roughly where it is (or, for the story
gifts, how to get it), plus the Pure Hearts held. Each level shows its status: Complete,
checked/total, or no checks. The map you're in is marked "<- here".

A location also counts as checked if the Archipelago server has it, even when the loaded save doesn't
(after a reset or a reload without saving). The client shares that list in a file while it runs.

The window refreshes when something changes and keeps where you've scrolled to. It only reads: it
never writes to the game.

Positions are the game's own coordinates: x runs left to right along the level, y is height, z is
depth. SPM Coords (coords.py) shows Mario's own position, to compare.

Usage: pyw -3 -E tracker.py [--missing]   (--missing: hide checked locations)
"""

import json
import os
import sys
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from game_memory import connect_dolphin, current_map, mario_position, read_flags  # noqa: F401 (used by coords.py)
from mailbox import Mailbox, read_server_checks

# The location list made by tools/gen_ap_data.py
LOCATIONS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "ap_locations.json")
REFRESH_MS = 1000
HEART_FLAG_BASE = 7168 + 24  # Pure Heart 1's save flag (mod save_data.h AP_GSWF_HEART)
HEART_COUNT = 7              # the tower's doors


def checked_state(loc, flags, server):
    """'x' if a location is checked (in the loaded save, or on the server), ' ' if not."""
    return "x" if Mailbox.flag_set(flags, loc["flag"]) or loc["flag"] in server else " "


def describe_spot(loc):
    """Where a location is: its spot and position, or how to get it (story gifts, spots the mod flags)."""
    if loc["kind"] in ("gift", "spot", "heart"):
        return loc["where"]
    if loc["pos"] is None:
        return f"{loc['where']}, position set by the script"
    x, y, z = loc["pos"]
    return f"{loc['where']} at x {x:g}, height {y:g}, depth {z:g}"


def pure_hearts(flags):
    """'Pure Hearts (doors open): 1 3 5 7', or a note when the save has none."""
    held = [n for n in range(1, HEART_COUNT + 1) if Mailbox.flag_set(flags, HEART_FLAG_BASE + n - 1)]
    if not held:
        return "Pure Hearts: none (every door is open without the client)"
    return "Pure Hearts (doors open): " + " ".join(map(str, held))


def level_status(entries):
    """A level's status: "no checks", "Complete", or "checked/total"."""
    done = sum(state != " " for _, state in entries)
    if not entries:
        return "no checks"
    if done == len(entries):
        return "Complete"
    return f"{done}/{len(entries)}"


def location_line(loc, state, here):
    """One location's line: [x] map  item  where  (<- here if Mario is in that map)."""
    item = loc["name"].split(" - ", 1)[1].rsplit(" (", 1)[0]
    marker = "<- here" if loc["map"] == here else ""
    return f"  [{state}] {loc['map']:7} {item:28} {describe_spot(loc)} {marker}"


def render(levels, locations, flags, here, missing_only, mario=None, server=frozenset()):
    """The tracker's whole text: a summary, then every level with its status and, under it, its
    locations (only the unchecked ones with --missing). `server` is the set of save flags of the
    locations the server has as checked."""
    by_level = {title: [] for title in levels}
    for loc in locations:
        by_level.setdefault(loc["name"].split(" - ")[0], []).append((loc, checked_state(loc, flags, server)))
    total = sum(len(entries) for entries in by_level.values())
    done_total = sum(state != " " for entries in by_level.values() for _, state in entries)

    lines = [f"Super Paper Mario tracker: {done_total}/{total} checked   (you're in {here})", pure_hearts(flags)]
    if mario is not None:
        x, y, z = mario
        lines.append(f"Mario: x {x:.0f}, height {y:.0f}, depth {z:.0f}")
    lines.append("")
    for title, entries in by_level.items():
        lines.append(f"{title}  -  {level_status(entries)}")
        for loc, state in entries:
            if missing_only and state != " ":
                continue
            lines.append(location_line(loc, state, here))
    return "\n".join(lines)


def load_locations():
    """(the level titles, every location sorted by map and name), from the location list."""
    data = json.load(open(LOCATIONS))
    return data["levels"], sorted(data["locations"], key=lambda l: (l["map"], l["name"]))


def make_window():
    """The tracker window: a scrolling text box. Returns (window, text box)."""
    root = tk.Tk()
    theme.apply(root)
    root.title("SPM tracker")
    root.geometry("1100x800")
    scroll = tk.Scrollbar(root)
    scroll.pack(side="right", fill="y")
    text = tk.Text(root, font=("Consolas", 11), wrap="none", yscrollcommand=scroll.set)
    text.pack(side="left", fill="both", expand=True)
    scroll.config(command=text.yview)
    return root, text


def show(text, screen):
    """Replaces the text, staying scrolled where it was."""
    top = text.yview()[0]
    text.config(state="normal")
    text.delete("1.0", "end")
    text.insert("1.0", screen)
    text.config(state="disabled")
    text.yview_moveto(top)


def main():
    missing_only = "--missing" in sys.argv
    root, text = make_window()
    state = {"last": None, "mtime": None, "levels": None, "locations": None}

    def refresh():
        # Reload the list when it's regenerated (new locations), so a running tracker stays current
        mtime = os.path.getmtime(LOCATIONS)
        if mtime != state["mtime"]:
            state["levels"], state["locations"] = load_locations()
            state["mtime"] = mtime
        try:
            connect_dolphin()
            # No Mario position here (SPM Coords shows it): the text only changes with a check or the map
            screen = render(state["levels"], state["locations"], read_flags(), current_map(), missing_only,
                            server=read_server_checks())
        except RuntimeError as e:
            dme.un_hook()
            screen = f"Waiting for the game: {e}"
        if screen != state["last"]:
            show(text, screen)
            state["last"] = screen
        root.after(REFRESH_MS, refresh)

    refresh()
    root.mainloop()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
