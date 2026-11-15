"""Builds the Archipelago location and item data, the single source for the APworld, client and mod.

Writes:
  data/ap_locations.json       every location: id, name, category, vanilla item, detection flag, patches
  apworld/spm/data.py          the same as Python tables for the APworld and its client
  mod/include/location_flags.h every location's save flag, in the same order, for the mod

A location is detected by a save flag the client polls, and placed by rewriting the item argument of the
script instruction that spawns it (relF scripts stay loaded all game). Kinds:
  flagged    a pickup or chest with its own vanilla GSWF flag (one location per flag)
  join       a Pixl join (placement through the mod's join table, flag AP_GSWF_JOIN)
  spot       an item spot with no flag of its own (UNFLAGGED_SPOTS); the mod gives it the flag
             AP_GSWF_LOC_BASE + n (mod/source/spot_flags.cpp, same order)
  gift       a story gift (GIFTS): given by the mod, placed through the mailbox
  heart      the Pure Heart at the end of Chapters 1-7 (CHAPTER_HEARTS): given by the mod, placed through
             the mailbox (heartLocItems); its vanilla item is a Pure Heart, which isn't a game item

Every patched word is checked against the clean relF.rel before it's written out.

Usage: py -3 -E tools/gen_ap_data.py
"""

import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import game_names  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCATIONS = os.path.join(ROOT, "data", "locations_us0.json")
ITEM_NAMES = os.path.join(ROOT, "data", "item_names_us0.json")
RELF = os.path.join(ROOT, "build", "relF.rel")
OUT_JSON = os.path.join(ROOT, "data", "ap_locations.json")
OUT_PY = os.path.join(ROOT, "apworld", "spm", "data.py")
OUT_H = os.path.join(ROOT, "mod", "include", "location_flags.h")

# Archipelago ids: base + SPM item id for items, base + location index for locations
ID_BASE = 0x53504D00  # "SPM\0"

# evt opcodes and variable encodings (mod/include/script_patching.h)
OP_USER_FUNC = 92
GSWF_BASE = -130000000

# Argument positions of the item and the save flag, per spawning function (words after the header and
# the function pointer)
ARGS = {"item": {"item": 1, "flag": 8},   # evt_item_entry(name, item, behaviour, x, y, z, ?, script, flag, ?)
        "chest": {"item": 7, "flag": 8}}  # evt_mobj_thako(type, name, x, y, z, interact, open, item, opened)

# Reserved save flags (mod/include/save_data.h)
AP_GSWF_BASE = 7168
AP_GSWF_JOIN_BASE = AP_GSWF_BASE + 1          # + Pixl item id - 221 (Thoreau .. Dashell: 11 flags)
AP_GSWF_LOC_BASE = AP_GSWF_BASE + 32          # unflagged key spots, in KEY_SPOTS order
AP_GSWF_GIFT_BASE = AP_GSWF_BASE + 16         # story gifts, in GIFTS order (mod/source/story_gifts.cpp sGifts)
AP_GSWF_CHAPTER_HEART_BASE = AP_GSWF_BASE + 1008  # + chapter - 1 (mod save_data.h AP_GSWF_CHAPTER_HEART)

# The Pure Heart each of Chapters 1-7 ends with (Chapter N: heart N + 1), and the map of its scene
# (where the chapter's finished value is set). Pure Hearts are items 1001-1008 (mod pure_hearts.h),
# not game items.
HEART_ITEM_FIRST = 1001
CHAPTER_HEARTS = {1: "he4_11", 2: "mi4_14", 3: "ta4_13", 4: "sp4_14", 5: "gn4_16", 6: "wa1_27", 7: "an4_10"}

# Story gift locations (story_gifts.cpp sGifts), in the mod's order: (vanilla item, map, label, how to get
# it, for the tracker). New ones
# go at the end, so earlier location ids don't change. The characters' are their join scenes; the
# Helmet is always vanilla: without it Outer Space's movement breaks, and nothing blocks Chapter 4
# without it yet.
GIFTS = [(217, "mac_01", "Peach joins", "talk to Peach in mac_01 holding a Spicy Soup (Saffron cooks one from a Fire Burst)"),
         (219, "an1_07", "Luigi joins", "his scene: step onto the ledge on the right"),
         (24, "sp1_01", "Helmet (entering Chapter 4)", "given on entering any Chapter 4 map"),
         (218, "ta1_04", "Bowser joins", "after the fight with Bowser"),
         (80, "he1_06", "Bestovius's lesson", "talk to Bestovius in his house"),
         (82, "wa1_27", "Brobot L-type beaten", "beat Brobot L-type at the end of the World of Nothing (given on the next map change)")]
ALWAYS_VANILLA_GIFTS = {24}
# Item spots with no flag of their own added later, continuing UNFLAGGED_SPOTS' flags and the mod's order
# (spot_flags.cpp sSpots): Chapter 7's Door Keys (user, 2026-11-05). Each scene spawns its key, waits for
# it to be collected and moves the story on, so it doesn't play again. Listed after every other location
LATE_SPOTS = [
    (0x199a60, "7-1: an1_04 (opens an1_03's lock, the way to Luigi)"),
    (0x1a19bc, "7-2: the D-Men give it after Bowser's fight (opens an2_03's lock to an2_05)"),
    (0x1a6cec, "7-2: the sisters give it for the Diet Book (opens an2_06's lock to an2_08)"),
    (0x1bc3a8, "7-4: an4_03 (opens one of the Overthere's two locks)"),
    (0x1bd6e0, "7-4: an4_04 (opens one of the Overthere's two locks)"),
    # 7-4's orbs (user, 2026-11-05): each scene sets its own flag at the end (GSWF 1235-1237)
    (0x1be18c, "7-4: the Blue Orb, an4_04 (one of the three for an4_03's rainbow)"),
    (0x1c04d8, "7-4: the Yellow Orb, an4_06 (one of the three for an4_03's rainbow)"),
    (0x1c12e8, "7-4: the Red Orb, an4_07 (one of the three for an4_03's rainbow)"),
    # Every other key (user, 2026-11-07: all keys are checks; not the Pit Keys, the Old Key or the Random
    # House Key yet). Each spawns from a box, a block or a one-time scene that moves the story on once it's
    # collected (or, for he4_05, he4_07, sp2_01, gn4_06, gn4_10 and ls2_12, from the room's own setup until
    # its pickup script moves the story on), so a collected spot doesn't need to spawn again
    (0xf9c44, "1-4: he4_04's box (the first of three Ruins Keys; he4_03's lock)"),
    (0xfa3a0, "1-4: he4_05 (the second Ruins Key; he4_05's lock)"),
    (0xfac58, "1-4: he4_07 (the third Ruins Key; he4_07's lock)"),
    (0x10249c, "2-1: mi1_05 (opens mi1_01's lock)"),
    (0x10aa6c, "2-2: mi2_06's box (opens mi2_03's lock to mi2_08)"),
    (0x137bdc, "3-4: ta4_02's box (opens ta4_01's lock)"),
    (0x14b798, "4-2: sp2_01, after the flip (opens sp2_01's lock to sp2_02)"),
    (0x156434, "4-4: the box in sp4_04/05 (opens sp4_03's lock)"),
    (0x15688c, "4-4: sp4_07's box (opens sp4_07's lock to sp4_16)"),
    (0x157794, "4-4: sp4_12's box (opens sp4_11's lock to sp4_13)"),
    (0x16d460, "5-3: from the NPC in gn3_01 (opens gn3_03's lock)"),
    (0x1769cc, "5-4: gn4_06/07, behind the drawing (opens gn4_02's lock)"),
    (0x177130, "5-4: gn4_10 (opens gn4_10's card door to gn4_11)"),
    (0x1ce2f4, "8-1: ls1_07's box (ls1_05's lock)"),
    (0x1d6064, "8-2: ls2_02's box (ls2_02's lock)"),
    (0x1dbce0, "8-2: ls2_12 (ls2_09's lock)"),
    (0x1e1584, "8-3: ls3_03's block (ls3_03's lock)"),
    (0x1e2e94, "8-3: ls3_09's box (ls3_04's lock)"),
    (0x1e3874, "8-3: ls3_11's box (ls3_11's lock)"),
    (0x1e8650, "8-4: ls4_05's box (ls4_04's lock)"),
]

# Story gifts added later, continuing GIFTS' flags and the mod's order (story_gifts.h GIFT_BOWSER_REUNION
# on): listed after every other location so earlier location ids don't change (user, 2026-11-05)
LATE_GIFTS = [(82, "an2_04", "Bowser rejoins", "after the fight with Bowser in 7-2"),
              (82, "an3_06", "Peach rejoins", "talk to Peach in 7-3")]

PIXLS = {221: ("Thoreau", "he2_07"), 222: ("Boomer", "mi1_07"), 223: ("Slim", "mi3_01"),
         224: ("Thudley", "ta2_04"), 225: ("Carrie", "ta4_12"), 226: ("Fleep", "sp2_01"),
         227: ("Cudge", "gn2_02"), 228: ("Dottie", "gn4_03"), 229: ("Piccolo", "mac_19"),
         230: ("Barry", "ta1_01"), 231: ("Dashell", "dan_30")}
OPTIONAL_PIXLS = {229, 230, 231}

# Item spots with no save flag of their own: (relF file offset of their evt_item_entry, how to get it, for
# the tracker). The mod gives the nth one the flag AP_GSWF_LOC_BASE + n (spot_flags.cpp sSpots, same
# order). New ones go at the end.
SAMMER_REWARD = "King Sammer hands it over after the 100th duel of post-game Sammer's Kingdom"
UNFLAGGED_SPOTS = [
    # King Sammer's TTYD partner Catch Cards (wa4_26), handed over one after another
    (0x192e78, SAMMER_REWARD), (0x192ec8, SAMMER_REWARD), (0x192f18, SAMMER_REWARD), (0x192f68, SAMMER_REWARD),
    (0x192fb8, SAMMER_REWARD), (0x193008, SAMMER_REWARD), (0x193058, SAMMER_REWARD),
]



# Items no location holds in vanilla but the apworld's filler mix uses (items.py FILLER_WEIGHTS; user,
# 2026-11-12): Gold Bar x3, Volt Shroom, Catch Card, HP Plus, Power Plus, Star Medal, Gold Medal,
# Peachy Peach, Keel Mango, Turtley Leaf, Cake Mix, Whacka Bump, Horsetail, Hot Sauce, Big Egg, Trial Stew
FILLER_ONLY_ITEMS = {72, 76, 87, 89, 90, 96, 97, 100, 101, 104, 105, 106, 107, 109, 118, 174}

def word(data, offset):
    """The signed 4-byte number at a position in the data."""
    return struct.unpack_from(">i", data, offset)[0]


# Position argument (x, y, z) of each spawning function, as argument indices
POS_ARGS = {"item": 3, "chest": 2}  # evt_item_entry(name, item, behaviour, x, y, z, ...); evt_mobj_thako(type, name, x, y, z, ...)
FLOAT_BASE = -240000000
VAR_LIMIT = -1000000  # evt variables are encoded far below this; plain numbers are above it


def position(data, at, kind):
    """The spawn position from the instruction's arguments, or None if it's computed by the script."""
    out = []
    for i in range(3):
        v = word(data, at + 4 * (2 + POS_ARGS[kind] + i))
        if -250000000 < v < -220000000:  # an evt float
            out.append(round((v - FLOAT_BASE) / 1024.0, 1))
        elif v > VAR_LIMIT:
            out.append(v)
        else:
            return None  # a variable: the script works it out (often where an enemy fell or a scene ends)
    return out


def item_names():
    """Every item's name by id, using the names world items show in the game."""
    names = {int(k): v for k, v in json.load(open(ITEM_NAMES, encoding="utf-8")).items()}
    for k, v in names.items():
        names[k] = game_names.WORLD_ITEM_NAMES.get(v, v)
    return names


def unique_item_names(names, ids):
    """Archipelago item names must be unique; SPM reuses names ("Door Key" x6), so those get their id."""
    counts = {}
    for i in ids:
        counts[names[i]] = counts.get(names[i], 0) + 1
    return {i: names[i] if counts[names[i]] == 1 else f"{names[i]} ({i})" for i in ids}


def display_name(item, names):
    """Catch cards share names with Pixls and enemies, so they're "<name> Card"."""
    return f"{names[item]} Card" if 282 <= item < 538 else names[item]


def category(item):
    """Which option shuffles an item (pixl, key_item, overworld_item...)."""
    if item >= HEART_ITEM_FIRST:
        return "pure_heart"
    if 216 <= item < 220:
        return "character"
    if 220 <= item < 232:
        return "optional_pixl" if item in OPTIONAL_PIXLS else "pixl"
    if 16 <= item < 65:
        return "key_item"
    if 282 <= item < 538:
        return "catch_card"
    return "overworld_item"


def level_title(map_name, levels):
    """A map's level title, like "1-2 Mount Lineland"."""
    chapter, level = game_names.place({"maps": [map_name]})
    if isinstance(chapter, int):
        return f"{chapter}-{level} {levels.get((chapter, level), '')}".strip()
    return {"hub": "Flipside/Flopside", "pit": "Pit of 100 Trials"}.get(chapter, "Other")


# ---- Tattle checks (user, 2026-11-07): one location per kind of enemy, collected by tattling it with Tatl ----

SETUP_DIR = os.path.join(ROOT, "disc", "DATA", "files", "setup")
ENEMY_TEMPLATES = 0x80409BE8   # npcEnemyTemplates (US 1.0): 0x68 bytes each, the tribe at +0x14
ENEMY_TEMPLATE_SIZE = 0x68
NPC_TRIBES = 0x803FC290        # npcTribes: 0x68 bytes each, the catch card item at +0x08
NPC_TRIBE_SIZE = 0x68
# Setup file entry sizes by version (setup_data.h; Skawo's editor): each starts with x, y, z, template.
# Versions 1-3 are only used by test and unused maps
SETUP_ENTRY_SIZE = {4: 0x68, 5: 0x6C, 6: 0x70}
SETUP_ENEMIES = 100
FIRST_CARD = 282
AP_GSWF_TATTLE_BASE = AP_GSWF_BASE + 512      # + kind number (mod save_data.h AP_GSWF_TATTLE_BASE)
TATTLE_VANILLA_ITEM = 80                     # what a tattle location "holds" vanilla: a Shroom Shake (filler)


# Setup files for rooms the game never reaches (user, 2026-11-12: hei_02 was counted as a free room for its
# tattles): hei_01 and hei_02 (no door leads there; not even in the map list) and gn3_02 (marked unused)
UNREACHED_SETUP_MAPS = {"hei_01", "hei_02", "gn3_02"}
# The Pit of 100 Trials' enemies aren't in setup files: each floor's list is in pitText, LZ-compressed text
# in main.dol (US 1.0), <Dungeon> entries numbered 0-199 (Flipside's 100 floors, then Flopside's), each
# <enemy> giving <name> = the tribe + 1. Tattles there count as rooms dan_01 (Flipside) and dan_41 (Flopside)
PIT_TEXT = 0x80367EF0
PIT_TEXT_SIZE = 0x3B48
PIT_FLOORS = 100


def pit_tribes(dol):
    """[(dungeon number, tribe)] for every enemy the Pit's floors can spawn."""
    import lz
    text = lz.decompress_lz10(dol.read(PIT_TEXT, PIT_TEXT_SIZE + 16)).decode("ascii", "replace")
    out = []
    for block in text.split("<Dungeon>")[1:]:
        number = int(re.search(r"<no>\s*(-?\d+)", block).group(1))
        for enemy in block.split("<enemy>")[1:]:
            name = int(re.search(r"<name>\s*(-?\d+)", enemy).group(1))
            num = int(re.search(r"<num>\s*(-?\d+)", enemy).group(1))
            if name > 0 and num > 0:
                out.append((number, name - 1))
    return out


def tattle_kinds(names):
    """[(card item, name, [maps])]: every kind of enemy the maps' setup files spawn, by its catch card
    (variants of one enemy share a card, so they're one kind), sorted by card. Every spawned enemy has a
    card; Tippi's own text covers the ones that don't (the game's tattle does that already)."""
    from dolasm import Dol
    dol = Dol()
    maps_of = {}
    for f in sorted(os.listdir(SETUP_DIR)):
        data = open(os.path.join(SETUP_DIR, f), "rb").read()
        size = SETUP_ENTRY_SIZE.get(struct.unpack(">H", data[:2])[0])
        if not size:
            continue
        for i in range(SETUP_ENEMIES):
            entry = data[4 + size * i:4 + size * (i + 1)]
            if not any(entry):
                continue
            template = struct.unpack(">i", entry[0xC:0x10])[0]
            tribe = struct.unpack(">i", dol.read(ENEMY_TEMPLATES + ENEMY_TEMPLATE_SIZE * template + 0x14, 4))[0]
            card = struct.unpack(">i", dol.read(NPC_TRIBES + NPC_TRIBE_SIZE * tribe + 0x8, 4))[0]
            if card >= FIRST_CARD:
                # the kinds stay the ones every setup file has (their order is each tattle's save flag,
                # so it can't change under a seed); unreached rooms just aren't places to find them
                maps_of.setdefault(card, set())
                if f[:-4] not in UNREACHED_SETUP_MAPS:
                    maps_of[card].add(f[:-4])
    # The Pit's floors, for those kinds (enemies only the Pit has would be new tattle checks: not added)
    for number, tribe in pit_tribes(dol):
        card = struct.unpack(">i", dol.read(NPC_TRIBES + NPC_TRIBE_SIZE * tribe + 0x8, 4))[0]
        if card in maps_of:
            maps_of[card].add("dan_01" if number < PIT_FLOORS else "dan_41")
    return [(card, names[card].removesuffix(" Card"), sorted(maps_of[card])) for card in sorted(maps_of)]


def write_mod_flags(flags, path=OUT_H, tattle_cards=()):
    """Writes the mod's list of location save flags, in location order (offline_checks)."""
    body = ", ".join(str(x) for x in flags)
    lines = ["#pragma once", "",
             "// Generated by tools/gen_ap_data.py; don't edit by hand. Every location's save flag (GSWF), in",
             "// location order (a location's position here is its Archipelago id minus ID_BASE).", "",
             "#include <common.h>", "",
             "namespace mod::location_flags {", "",
             f"constexpr s32 LOCATION_COUNT = {len(flags)};", "",
             "// The locations before the tattle checks: the only ones that can be collected offline (a tattle",
             "// check needs the client's settings), so the only ones offline_checks keeps a bit for",
             f"constexpr s32 OFFLINE_LOCATION_COUNT = {len(flags) - len(tattle_cards)};", "",
             f"constexpr u16 FLAGS[LOCATION_COUNT] = {{{body}}};", "",
             "// Tattle checks: each kind's catch card item, in kind order (its flag is AP_GSWF_TATTLE_BASE + kind)",
             f"constexpr s32 TATTLE_KIND_COUNT = {len(tattle_cards)};", "",
             f"constexpr u16 TATTLE_CARDS[TATTLE_KIND_COUNT] = {{{', '.join(str(c) for c in tattle_cards)}}};", "",
             "}", ""]
    text = "\n".join(lines)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def main():
    """Builds the location list (data/ap_locations.json) and the APworld's data.py."""
    rel = open(RELF, "rb").read()
    names = item_names()
    names = {i: display_name(i, names) for i in names}
    levels = game_names.level_names()
    scan = json.load(open(LOCATIONS))

    locations = []

    # Pixl joins, in story order (the optional ones last)
    for pixl, (pixl_name, map_name) in sorted(PIXLS.items(), key=lambda p: (p[0] in OPTIONAL_PIXLS, p[0])):
        locations.append({"kind": "join", "map": map_name, "item": pixl, "flag": AP_GSWF_JOIN_BASE + pixl - 221,
                          "label": f"{pixl_name} joins", "patches": [], "where": "Pixl join scene", "pos": None})

    # Flagged pickups and chests: one location per flag, with every instruction that spawns it
    by_flag = {}
    for loc in scan:
        m = re.fullmatch(r"GSWF\((\d+)\)", loc.get("flag") or "")
        if not m or loc["type"] not in ARGS or loc.get("item_var") or loc["at"].startswith("dol"):
            continue
        # Item 0: the chest's open script hands out the reward itself (Pit chests, some story chests);
        # there's no item argument to rewrite
        if loc["item"] == 0:
            continue
        at = int(loc["at"], 16)
        args = ARGS[loc["type"]]
        item_offset = at + 4 * (2 + args["item"])
        flag_offset = at + 4 * (2 + args["flag"])
        assert word(rel, at) & 0xFFFF == OP_USER_FUNC, loc
        assert word(rel, item_offset) == loc["item"], loc
        assert word(rel, flag_offset) == GSWF_BASE + int(m[1]), loc
        entry = by_flag.setdefault(int(m[1]), {"kind": "flagged", "map": loc["maps"][0], "item": loc["item"],
                                               "flag": int(m[1]), "label": None, "patches": [],
                                               "where": "chest" if loc["type"] == "chest" else "lying around",
                                               "pos": position(rel, at, loc["type"])})
        assert entry["item"] == loc["item"], (entry, loc)
        entry["patches"].append({"offset": item_offset, "original": loc["item"]})
    locations += sorted(by_flag.values(), key=lambda e: (e["map"], e["flag"]))

    # Story gifts, after the scanned spots so those keep their ids (given by the mod when their trigger
    # first holds; placed through the mailbox)
    for i, (item, map_name, label, how) in enumerate(GIFTS):
        locations.append({"kind": "gift", "map": map_name, "item": item, "flag": AP_GSWF_GIFT_BASE + i,
                          "label": label, "patches": [], "where": how, "pos": None})

    # Spots the mod gives flags to, last so every earlier location keeps its id
    for n, (at, how) in enumerate(UNFLAGGED_SPOTS):
        assert word(rel, at) & 0xFFFF == OP_USER_FUNC
        item = word(rel, at + 4 * (2 + ARGS["item"]["item"]))
        assert word(rel, at + 4 * (2 + ARGS["item"]["flag"])) == 0, hex(at)
        loc = next(x for x in scan if x.get("at") == hex(at))
        locations.append({"kind": "spot", "map": loc["maps"][0], "item": item, "flag": AP_GSWF_LOC_BASE + n,
                          "label": None, "where": how, "pos": None,
                          "patches": [{"offset": at + 4 * (2 + ARGS["item"]["item"]), "original": item}]})

    # Each chapter's Pure Heart, last so every earlier location keeps its id
    for chapter, map_name in CHAPTER_HEARTS.items():
        heart = chapter + 1
        locations.append({"kind": "heart", "map": map_name, "item": HEART_ITEM_FIRST + heart - 1,
                          "flag": AP_GSWF_CHAPTER_HEART_BASE + chapter - 1, "label": f"Pure Heart {heart}", "patches": [],
                          "where": f"the end of Chapter {chapter} (given on the next map change)", "pos": None})

    # Story gifts added later, last so every earlier location keeps its id
    for j, (item, map_name, label, how) in enumerate(LATE_GIFTS):
        locations.append({"kind": "gift", "map": map_name, "item": item, "flag": AP_GSWF_GIFT_BASE + len(GIFTS) + j,
                          "label": label, "patches": [], "where": how, "pos": None})

    # Spots given flags later (after the gifts above, so their ids don't change either)
    for k, (at, how) in enumerate(LATE_SPOTS):
        n = len(UNFLAGGED_SPOTS) + k
        assert word(rel, at) & 0xFFFF == OP_USER_FUNC
        item = word(rel, at + 4 * (2 + ARGS["item"]["item"]))
        assert word(rel, at + 4 * (2 + ARGS["item"]["flag"])) == 0, hex(at)
        loc = next(x for x in scan if x.get("at") == hex(at))
        locations.append({"kind": "spot", "map": loc["maps"][0], "item": item, "flag": AP_GSWF_LOC_BASE + n,
                          "label": None, "where": how, "pos": None,
                          "patches": [{"offset": at + 4 * (2 + ARGS["item"]["item"]), "original": item}]})

    # Tattle checks, last so every earlier location keeps its id: one per kind of enemy
    kinds = tattle_kinds(names)
    for k, (card, enemy, maps) in enumerate(kinds):
        locations.append({"kind": "tattle", "map": maps[0], "maps": maps, "item": TATTLE_VANILLA_ITEM,
                          "flag": AP_GSWF_TATTLE_BASE + k, "label": enemy, "patches": [], "pos": None,
                          "where": f"tattle a {enemy} with Tatl (in {', '.join(maps)})", "card": card})

    # Names: "<level> - <vanilla item> (<map>)", numbered when one map has several of the same item
    seen = {}
    for loc in locations:
        if loc["kind"] == "tattle":
            base = f"Tattle - {loc['label']}"
        else:
            base = f"{level_title(loc['map'], levels)} - {loc['label'] or names[loc['item']]} ({loc['map']})"
        seen[base] = seen.get(base, 0) + 1
        loc["name"] = base
    repeats = {}
    for loc in locations:
        if seen[loc["name"]] > 1:
            repeats[loc["name"]] = repeats.get(loc["name"], 0) + 1
            loc["name"] = f"{loc['name']} #{repeats[loc['name']]}"
    for i, loc in enumerate(locations):
        loc["id"] = ID_BASE + i
        loc["category"] = "fixed" if loc["kind"] == "gift" and loc["item"] in ALWAYS_VANILLA_GIFTS else category(loc["item"])
        if loc["kind"] == "tattle":
            loc["category"] = "tattle"

    item_ids = sorted({loc["item"] for loc in locations if loc["item"] < HEART_ITEM_FIRST} | FILLER_ONLY_ITEMS)
    item_table = unique_item_names(names, item_ids)

    # Every level's title in play order (for trackers), whether or not it has locations yet
    level_titles = [f"{c}-{l} {levels.get((c, l), '')}".strip() for c in range(1, 9) for l in range(1, 5)]
    level_titles += ["Flipside/Flopside", "Pit of 100 Trials"]

    json.dump({"id_base": ID_BASE, "levels": level_titles, "locations": locations,
               "items": [{"id": ID_BASE + i, "spm": i, "name": item_table[i], "category": category(i)} for i in item_ids]},
              open(OUT_JSON, "w"), indent=1)

    os.makedirs(os.path.dirname(OUT_PY), exist_ok=True)
    with open(OUT_PY, "w", encoding="utf-8") as f:
        f.write('"""Generated by tools/gen_ap_data.py from data/ap_locations.json; don\'t edit by hand."""\n\n')
        f.write(f"ID_BASE = {ID_BASE:#x}\n\n")
        f.write("# spm item id -> (name, category)\n")
        f.write("ITEMS = {\n")
        for i in item_ids:
            f.write(f"    {i}: ({item_table[i]!r}, {category(i)!r}),\n")
        f.write("}\n\n")
        f.write("# (name, kind, category, vanilla spm item, detection flag, [(relF offset, original word)])\n")
        f.write("LOCATIONS = [\n")
        for loc in locations:
            patches = [(p["offset"], p["original"]) for p in loc["patches"]]
            f.write(f"    ({loc['name']!r}, {loc['kind']!r}, {loc['category']!r}, {loc['item']}, {loc['flag']}, {patches}),\n")
        f.write("]\n")

    with open(OUT_PY, "a", encoding="utf-8") as f:
        f.write("\n# Tattle locations: the maps whose enemies include that kind (for the logic)\n")
        f.write("TATTLE_MAPS = {\n")
        for loc in locations:
            if loc["kind"] == "tattle":
                f.write(f"    {loc['name']!r}: {loc['maps']!r},\n")
        f.write("}\n")

    write_mod_flags([loc["flag"] for loc in locations],
                    tattle_cards=[loc["card"] for loc in locations if loc["kind"] == "tattle"])

    counts = {}
    for loc in locations:
        counts[loc["category"]] = counts.get(loc["category"], 0) + 1
    print(f"{len(locations)} locations, {len(item_ids)} distinct items: {counts}")


if __name__ == "__main__":
    main()
