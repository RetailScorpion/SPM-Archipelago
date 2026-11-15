"""Names from the game that the data tools share: level names, readable names for the small world
pickups, and which chapter and level a map belongs to."""

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MESSAGES = os.path.join(ROOT, "build", "devdisc", "files", "msg", "US", "global.txt")

# map prefix -> chapter (levels are the prefix's digit: he1 = 1-1, he2 = 1-2, ...)
CHAPTERS = {"he": 1, "mi": 2, "ta": 3, "sp": 4, "gn": 5, "wa": 6, "an": 7, "ls": 8}

# World pickups (ids 1-15) have no display name; readable names where they're clear
WORLD_ITEM_NAMES = {
    "[COIN]": "Coin", "[COIN_x3]": "3 Coins", "[SPEED_FLOWER]": "Speed Flower",
    "[SLOW_FLOWER]": "Slow Flower", "[HAPPY_FLOWER]": "Happy Flower", "[KINOKO]": "Mushroom",
    "[SUPER_KINOKO]": "Super Mushroom", "[ULTRA_KINOKO]": "Ultra Mushroom",
}


def level_names():
    """Each level's name, from the game's message file (in the dev disc): {(chapter, level): name}."""
    names = {}
    if os.path.exists(MESSAGES):
        parts = open(MESSAGES, "rb").read().split(b"\0")
        for key, text in zip(parts, parts[1:]):
            m = re.fullmatch(rb"place_stg(\d)_(\d)", key)
            if m:
                names[(int(m[1]), int(m[2]))] = text.decode()
    return names


def place(loc):
    """(chapter, level) of a location, from its first map's name."""
    m = loc["maps"][0]
    prefix = m[:2]
    if prefix in CHAPTERS and m[2].isdigit():
        return CHAPTERS[prefix], int(m[2])
    if m.startswith("mac_"):
        return "hub", None
    if m.startswith("dan_"):
        return "pit", None
    if m == "main.dol":
        return "dol", None
    return "other", None
