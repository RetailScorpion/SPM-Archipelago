"""Proof-of-concept client: talks to the mod through Dolphin, no Archipelago server yet.

Usage: py -3 poc_client.py
Commands:
  give <item id or name>   queue an item, e.g. "give 221" or "give thoreau"
  state                    show the mod's state and story values
  warp <map> [door]        debug: go to a map, e.g. "warp mi1_07"
  story <chapter> <value>  debug: set a chapter's story value, e.g. "story 2 72"
  nojoin on|off            debug: Pixl joins give nothing (like holding another world's item)
  quit
Item pickups the mod reports are printed as they happen.
"""

import sys
import threading
import time

from mailbox import DEBUG_JOINS_GIVE_NOTHING, Mailbox, MailboxError, STATE_NAMES

POC_LOCATION_BASE = 0x10000

# A few handy ids from the game's item table (spm-docs items.txt). Characters and Pixls must use
# the inventory ids (216-231); the later "Thoreau" etc. entries (477+) are separate copies that
# pouchAddItem ignores.
NAMED_ITEMS = {
    "peach": 217, "bowser": 218, "luigi": 219,
    "tippi": 220, "thoreau": 221, "boomer": 222, "slim": 223, "thudley": 224, "carrie": 225,
    "fleep": 226, "cudge": 227, "dottie": 228, "piccolo": 229, "barry": 230, "dashell": 231,
    "ruinskey": 16, "returnpipe": 50, "mushroom": 6, "hpplus": 89,
}


def format_story(context, slots, live):
    where = {-1: "other map", 0: "hub"}.get(context, f"chapter {context}")
    chapters = " ".join(f"{i}:{v}" for i, v in enumerate(slots) if i > 0)
    return f"{where}, story value {live} | hub {slots[0]} | {chapters}"


def watch(mb: Mailbox, lock: threading.Lock):
    last_story = None
    while True:
        with lock:
            locations = mb.poll_sent()
            story = mb.story_state()
        for loc in locations:
            item = loc - POC_LOCATION_BASE
            print(f"\n[game] picked up item {item} (location {loc:#x})\n> ", end="", flush=True)
        # print on map changes (context) and on story progress within the current map
        if story[0] != (last_story or (None,))[0] or story[2] != (last_story or (0, 0, None))[2]:
            print(f"\n[story] {format_story(*story)}\n> ", end="", flush=True)
            last_story = story
        time.sleep(0.25)


def main():
    mb = Mailbox()
    try:
        mb.connect()
    except MailboxError as e:
        print(f"Couldn't connect: {e}")
        sys.exit(1)
    print(f"Connected. Mailbox at {mb.addr:#010x}, mod state: {STATE_NAMES.get(mb.mod_state(), '?')}")

    lock = threading.Lock()
    threading.Thread(target=watch, args=(mb, lock), daemon=True).start()

    while True:
        try:
            line = input("> ").strip().lower()
        except EOFError:
            break
        if not line:
            continue
        cmd, _, arg = line.partition(" ")
        if cmd == "quit":
            break
        elif cmd == "state":
            with lock:
                print(STATE_NAMES.get(mb.mod_state(), "?"))
                print(format_story(*mb.story_state()))
        elif cmd == "warp":
            parts = arg.split()
            if not parts:
                print("warp <map> [door]")
                continue
            with lock:
                mb.warp(parts[0], parts[1] if len(parts) > 1 else "")
            print(f"Warping to {parts[0]}")
        elif cmd == "story":
            try:
                chapter, value = (int(x) for x in arg.split())
            except ValueError:
                print("story <chapter> <value>")
                continue
            with lock:
                mb.set_chapter_value(chapter, value)
            print(f"Chapter {chapter} set to {value}")
        elif cmd == "nojoin":
            with lock:
                mb.set_debug_flag(DEBUG_JOINS_GIVE_NOTHING, arg == "on")
            print(f"Pixl joins give nothing: {arg == 'on'}")
        elif cmd == "give":
            item = NAMED_ITEMS.get(arg.replace(" ", ""))
            if item is None:
                try:
                    item = int(arg, 0)
                except ValueError:
                    print(f"Unknown item {arg!r}. Names: {', '.join(NAMED_ITEMS)}")
                    continue
            with lock:
                mb.give(item)
            print(f"Queued item {item}")
        else:
            print(__doc__)


if __name__ == "__main__":
    main()
