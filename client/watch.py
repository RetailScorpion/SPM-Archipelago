"""Read-only view of the mod's mailbox: prints checks, delivered items and story changes as they happen.

Unlike poc_client.py it doesn't acknowledge anything, so other tools can still read the same entries.

Usage: py -3 -E watch.py
"""

import json
import os
import time

import dolphin_memory_engine as dme

from mailbox import (Mailbox, MailboxError, OFF_RECV_HANDLED, OFF_RECV_ITEMS, OFF_SENT_COUNT,
                     OFF_SENT_LOCATIONS, RECV_SIZE, SENT_SIZE)
from poc_client import JOIN_LOCATION_BASE, JOIN_LOCATION_OTHER_WORLD, PIXL_NAMES, POC_LOCATION_BASE, format_story


# Item display names (data/item_names_us0.json, exported from the item list by tools/locscan.py)
ITEM_NAMES_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "item_names_us0.json")
try:
    ITEM_NAMES = {int(k): v for k, v in json.load(open(ITEM_NAMES_PATH, encoding="utf-8")).items()}
except OSError:
    ITEM_NAMES = {}


def item_name(item):
    name = ITEM_NAMES.get(item)
    return f"{name} ({item})" if name else f"item {item}"


def describe_location(loc):
    if loc >= JOIN_LOCATION_BASE:
        pixl = (loc - JOIN_LOCATION_BASE) & 0xFFF
        held = ", held another world's item" if loc & JOIN_LOCATION_OTHER_WORLD else ""
        return f"[check] {PIXL_NAMES.get(pixl, pixl)}'s join location{held} ({loc:#x})"
    return f"[pickup] {item_name(loc - POC_LOCATION_BASE)} ({loc:#x})"


def connect():
    while True:
        mb = Mailbox()
        try:
            mb.connect()
            return mb
        except MailboxError as e:
            print(f"Waiting for the game: {e}")
            time.sleep(3)


def main():
    mb = connect()
    print(f"Watching the mailbox at {mb.addr:#010x}. Ctrl+C to quit.\n")
    sent = dme.read_word(mb.addr + OFF_SENT_COUNT)
    handled = dme.read_word(mb.addr + OFF_RECV_HANDLED)
    last_story = None
    while True:
        try:
            count = dme.read_word(mb.addr + OFF_SENT_COUNT)
            for i in range(max(sent, count - SENT_SIZE), count):
                print(describe_location(dme.read_word(mb.addr + OFF_SENT_LOCATIONS + (i % SENT_SIZE) * 4)))
            sent = count

            now = dme.read_word(mb.addr + OFF_RECV_HANDLED)
            for i in range(handled, now):
                item = int.from_bytes(dme.read_bytes(mb.addr + OFF_RECV_ITEMS + (i % RECV_SIZE) * 2, 2), "big")
                print(f"[received] {item_name(item)} delivered")
            handled = now

            story = mb.story_state()
            if last_story is None or story[0] != last_story[0] or story[2] != last_story[2]:
                print(f"[story] {format_story(*story)}")
                last_story = story
        except RuntimeError:
            print("Lost Dolphin; reconnecting")
            mb = connect()
        time.sleep(0.25)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
