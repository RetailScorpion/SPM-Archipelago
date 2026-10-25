"""Proof-of-concept client: talks to the mod through Dolphin, no Archipelago server yet.

Usage: py -3 poc_client.py
Commands:
  give <item id or name>   queue an item, e.g. "give 477" or "give thoreau"
  state                    show the mod's state
  quit
Item pickups the mod reports are printed as they happen.
"""

import sys
import threading
import time

from mailbox import Mailbox, MailboxError, STATE_NAMES

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


def watch_pickups(mb: Mailbox, lock: threading.Lock):
    while True:
        with lock:
            locations = mb.poll_sent()
        for loc in locations:
            item = loc - POC_LOCATION_BASE
            print(f"\n[game] picked up item {item} (location {loc:#x})\n> ", end="", flush=True)
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
    threading.Thread(target=watch_pickups, args=(mb, lock), daemon=True).start()

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
