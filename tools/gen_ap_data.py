"""Builds the Archipelago location and item data, the single source for the APworld, client and mod.

Writes:
  data/ap_locations.json       every location: id, name, category, vanilla item, detection flag, patches
  apworld/spm/data.py          the same as Python tables for the APworld and its client

A location is detected by a save flag the client polls, and placed by rewriting the item argument of the
script instruction that spawns it (relF scripts stay loaded all game). Kinds:
  flagged    a pickup or chest with its own vanilla GSWF flag (one location per flag)
  join       a Pixl join (placement through the mod's join table, flag AP_GSWF_JOIN)
  key_spot   an unflagged key item spot, reviewed by hand (KEY_SPOTS); the mod gives it the flag
             AP_GSWF_LOC(n) and replaces its story gates (mod/source/locations.cpp)

Every patched word is checked against the clean relF.rel before it's written out.

Usage: py -3 -E tools/gen_ap_data.py
"""

import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import checks_list  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCATIONS = os.path.join(ROOT, "data", "locations_us0.json")
ITEM_NAMES = os.path.join(ROOT, "data", "item_names_us0.json")
RELF = os.path.join(ROOT, "build", "relF.rel")
OUT_JSON = os.path.join(ROOT, "data", "ap_locations.json")
OUT_PY = os.path.join(ROOT, "apworld", "spm", "data.py")

# Archipelago ids: base + SPM item id for items, base + location index for locations
ID_BASE = 0x53504D00  # "SPM\0"

# evt opcodes and variable encodings (mod/source/joins.cpp)
OP_USER_FUNC = 92
GSWF_BASE = -130000000

# Argument positions of the item and the save flag, per spawning function (words after the header and
# the function pointer)
ARGS = {"item": {"item": 1, "flag": 8},   # evt_item_entry(name, item, behaviour, x, y, z, ?, script, flag, ?)
        "chest": {"item": 7, "flag": 8}}  # evt_mobj_thako(type, name, x, y, z, interact, open, item, opened)

# Reserved save flags (mod/include/savedata.h)
AP_GSWF_BASE = 7168
AP_GSWF_JOIN_BASE = AP_GSWF_BASE + 1          # + Pixl item id - 221 (Thoreau .. Dashell: 11 flags)
AP_GSWF_LOC_BASE = AP_GSWF_BASE + 32          # unflagged key spots, in KEY_SPOTS order
AP_GSWF_GIFT_BASE = AP_GSWF_BASE + 16         # story gifts, in GIFTS order (mod/source/hubstory.cpp sGifts)

# Story gift locations (hubstory.cpp sGifts), in the mod's order: (vanilla item, map, label, how to get
# it, for the tracker). New ones
# go at the end, so earlier location ids don't change. The characters' are their join scenes; the
# Helmet is always vanilla: without it Outer Space's movement breaks, and nothing blocks Chapter 4
# without it yet.
GIFTS = [(217, "mac_02", "Peach joins", "talk to Peach in mac_01 holding a Spicy Soup (Saffron cooks one from a Fire Burst)"),
         (219, "an1_07", "Luigi joins", "his scene: step onto the ledge on the right"),
         (24, "sp1_01", "Helmet (entering Chapter 4)", "given on entering any Chapter 4 map"),
         (218, "ta1_04", "Bowser joins", "after the fight with Bowser"),
         (80, "he1_06", "Bestovius's lesson", "talk to Bestovius in his house")]
ALWAYS_VANILLA_GIFTS = {24}

PIXLS = {221: ("Thoreau", "he2_07"), 222: ("Boomer", "mi1_07"), 223: ("Slim", "mi3_01"),
         224: ("Thudley", "ta2_04"), 225: ("Carrie", "ta4_12"), 226: ("Fleep", "sp2_01"),
         227: ("Cudge", "gn2_02"), 228: ("Dottie", "gn4_03"), 229: ("Piccolo", "mac_19"),
         230: ("Barry", "ta1_01"), 231: ("Dashell", "dan_30")}
OPTIONAL_PIXLS = {229, 230, 231}

# Unflagged key item spots that have been reviewed: (relF offset of the evt_item_entry, gates). Each gate is
# the relF offset of an IF_SMALL(GSW(0), N) around the spawn that the mod turns into "not collected".
# Filled in chapter by chapter as each spot is checked in game.
KEY_SPOTS = []


def word(data, offset):
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
    names = {int(k): v for k, v in json.load(open(ITEM_NAMES, encoding="utf-8")).items()}
    for k, v in names.items():
        names[k] = checks_list.WORLD_ITEM_NAMES.get(v, v)
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
    chapter, level = checks_list.place({"maps": [map_name]})
    if isinstance(chapter, int):
        return f"{chapter}-{level} {levels.get((chapter, level), '')}".strip()
    return {"hub": "Flipside/Flopside", "pit": "Pit of 100 Trials"}.get(chapter, "Other")


def main():
    rel = open(RELF, "rb").read()
    names = item_names()
    names = {i: display_name(i, names) for i in names}
    levels = checks_list.level_names()
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

    # Reviewed unflagged key spots
    for n, (at, gates) in enumerate(KEY_SPOTS):
        assert word(rel, at) & 0xFFFF == OP_USER_FUNC
        item = word(rel, at + 4 * (2 + ARGS["item"]["item"]))
        assert word(rel, at + 4 * (2 + ARGS["item"]["flag"])) == 0, hex(at)
        loc = next(x for x in scan if x.get("at") == hex(at))
        locations.append({"kind": "key_spot", "map": loc["maps"][0], "item": item, "flag": AP_GSWF_LOC_BASE + n,
                          "label": None, "spot": n, "spawn": at, "gates": gates,
                          "where": "lying around", "pos": position(rel, at, "item"),
                          "patches": [{"offset": at + 4 * (2 + ARGS["item"]["item"]), "original": item}]})

    # Story gifts, last so earlier locations keep their ids (given by the mod when their trigger first holds; placed through the mailbox)
    for i, (item, map_name, label, how) in enumerate(GIFTS):
        locations.append({"kind": "gift", "map": map_name, "item": item, "flag": AP_GSWF_GIFT_BASE + i,
                          "label": label, "patches": [], "where": how, "pos": None})

    # Names: "<level> - <vanilla item> (<map>)", numbered when one map has several of the same item
    seen = {}
    for loc in locations:
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

    item_ids = sorted({loc["item"] for loc in locations})
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

    counts = {}
    for loc in locations:
        counts[loc["category"]] = counts.get(loc["category"], 0) + 1
    print(f"{len(locations)} locations, {len(item_ids)} distinct items: {counts}")


if __name__ == "__main__":
    main()
