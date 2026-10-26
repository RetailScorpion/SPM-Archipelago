"""Read-only view of the mod's mailbox: prints checks, delivered items and story changes as they happen.

Unlike poc_client.py it doesn't acknowledge anything, so other tools can still read the same entries.

Usage: py -3 -E watch.py
"""

import json
import os
import time

import dolphin_memory_engine as dme

from mailbox import (GP_PTR, Mailbox, MailboxError, OFF_RECV_HANDLED, OFF_RECV_ITEMS, OFF_SENT_COUNT,
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


MAP_NAME_OFFSET = 0x44  # SpmarioGlobals mapName


def current_map():
    return dme.read_bytes(dme.read_word(GP_PTR) + MAP_NAME_OFFSET, 32).split(b"\0")[0].decode(errors="replace")


def place_name(map_name):
    """Names for maps outside the chapters and hubs, which the story line would only call "other map"."""
    if map_name.startswith("dan_"):
        # dan_0x-dan_30: Flipside Pit of 100 Trials; dan_4x-dan_70: Flopside's
        return "Flipside Pit" if map_name[4] in "0123" else "Flopside Pit"
    return None


def describe_story(story, map_name):
    text = format_story(*story)
    place = place_name(map_name)
    if place and text.startswith("other map"):
        text = place + text[len("other map"):]
    return f"{text} | map {map_name}"


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
            map_name = current_map()
            if last_story is None or (story[0], story[2], map_name) != last_story:
                print(f"[story] {describe_story(story, map_name)}")
                last_story = (story[0], story[2], map_name)
        except RuntimeError:
            print("Lost Dolphin; reconnecting")
            mb = connect()
        time.sleep(0.25)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
