"""Finds every location in Super Paper Mario's map scripts (US 1.0).

Starts from each map's init script (data/maps_us0.json, read from the running game's map table),
follows every script it can reach, and records anything that gives Mario something:
  item     evt_item_entry: items placed in the world, or spawned in cutscenes
  chest    evt_mobj_thako
  block    evt_mobj_blk with an item (? blocks, item bricks)
  gift     evt_pouch_add_item: added directly by a script (Pixls, characters, story items)
  shop     evt_shop_set_defs: shop inventories
and, for the logic, anything that needs a key item:
  lock     evt_mobj_zyo (locked doors/gates and the key that opens them)
  check    evt_pouch_check_have_item (scripts that test for an item)

Each entry has the map(s) whose scripts reach it, the vanilla item, the flag that marks it
collected, and the conditions (IF/SWITCH on story value and flags) it sits under.

Usage: py -3 -E tools/locscan.py
Writes data/locations_us0.json and data/locations_us0.md
"""

import json
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
from evtdis import VAR_BASES, load_opcodes
from rel import Rel, R_PPC_ADDR32
from dolasm import Symbols

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL_PATH = os.path.join(ROOT, "build", "relF.rel")
MAPS = os.path.join(ROOT, "data", "maps_us0.json")
ITEMS_TXT = os.path.join(ROOT, "ref", "spm-docs", "misc", "items.txt")
OUT_JSON = os.path.join(ROOT, "data", "locations_us0.json")
OUT_MD = os.path.join(ROOT, "data", "locations_us0.md")
OUT_STORY = os.path.join(ROOT, "data", "story_us0.json")  # every SET(GSW(0), value), with its map(s)
NOTES = os.path.join(ROOT, "data", "location_notes.json")  # hand-checked roles, keyed by 'at'

END_SCRIPT, USER_FUNC = 1, 92
MAX_ARGS = 24
MAX_INSTRUCTIONS = 6000
DATA_SCAN = 0x400  # bytes of a non-script pointer target searched for further script pointers

BLOCK_TYPES = {1: "? block", 2: "coin brick", 3: "multi-coin brick", 0x0A: "2D ? block", 0x0B: "3D ? block"}
COIN_ITEMS = {1, 2}  # COIN, COIN_x3: placed coins and coin blocks aren't locations

CHAPTER_PREFIXES = {"he": 1, "mi": 2, "ta": 3, "sp": 4, "gn": 5, "wa": 6, "an": 7, "ls": 8}

COND_OPS = {
    "IF_EQUAL": "==", "IF_NOT_EQUAL": "!=", "IF_SMALL": "<", "IF_LARGE": ">",
    "IF_SMALL_EQUAL": "<=", "IF_LARGE_EQUAL": ">=", "IF_FLAG": "&", "IF_NOT_FLAG": "!&",
}
NEGATE = {"==": "!=", "!=": "==", "<": ">=", ">": "<=", "<=": ">", ">=": "<", "&": "!&", "!&": "&"}

# Expected argument counts, to reject data that happens to look like a script
FIXED_ARGS = {
    "END_SCRIPT": 0, "RETURN": 0, "ELSE": 0, "END_IF": 0, "END_SWITCH": 0, "WHILE": 0, "DO_BREAK": 0,
    "DO_CONTINUE": 0, "CASE_ETC": 0, "CASE_END": 0, "SWITCH_BREAK": 0, "END_INLINE": 0, "END_BROTHER": 0,
    "LBL": 1, "GOTO": 1, "DO": 1, "WAIT_FRM": 1, "WAIT_MSEC": 1, "SWITCH": 1, "SWITCHI": 1,
    "CASE_EQUAL": 1, "CASE_NOT_EQUAL": 1, "CASE_SMALL": 1, "CASE_LARGE": 1, "CASE_SMALL_EQUAL": 1,
    "CASE_LARGE_EQUAL": 1, "CASE_OR": 1, "CASE_AND": 1, "CASE_FLAG": 1, "CASE_BETWEEN": 2,
    "SET": 2, "SETI": 2, "SETF": 2, "ADD": 2, "SUB": 2, "MUL": 2, "DIV": 2, "MOD": 2,
    "RUN_EVT": 1, "RUN_CHILD_EVT": 1, "RUN_EVT_ID": 2,
}
for _op in COND_OPS:
    FIXED_ARGS[_op] = 2


def load_item_names():
    """Display names; items without one (field pickups like the touch-to-heal Mushroom) get their
    internal name in brackets."""
    names, internal, cur = {}, {}, None
    for line in open(ITEMS_TXT, encoding="utf-8", errors="replace"):
        if line.startswith("Item "):
            cur = int(line.split()[1].rstrip(":"))
        elif line.strip().startswith("Name:") and cur is not None:
            internal[cur] = line.split(":", 1)[1].strip()
        elif "Name Message:" in line and cur is not None and "(" in line:
            names[cur] = line.split("(", 1)[1].rsplit(")", 1)[0]
    for i, n in internal.items():
        if names.get(i) in (None, "Unavailable Item"):
            names[i] = f"[{n}]"
    return names


def chapter_of(map_name):
    if map_name.startswith("mac_"):
        return "hub"
    return CHAPTER_PREFIXES.get(map_name[:2], "other")


class Scanner:
    def __init__(self):
        self.data = open(REL_PATH, "rb").read()
        self.rel = Rel(self.data)
        self.ops = load_opcodes()
        self.syms = Symbols()
        self.items = load_item_names()
        self.self_ptr, self.dol_ptr = {}, {}
        for mod, _ in self.rel.imports():
            for sec, off, rt, ts, ad in self.rel.relocs(mod):
                if rt != R_PPC_ADDR32:
                    continue
                fo = self.rel.file_offset(sec, off)
                if mod == 0:
                    self.dol_ptr[fo] = ad
                elif mod == self.rel.id:
                    self.self_ptr[fo] = self.rel.file_offset(ts, ad)
        self.fn = {name: addr for addr, name in self.syms.syms}
        self.script_cache = {}
        self.coins = 0

    # --- script decoding ---

    def decode(self, start):
        """List of (pos, opname, [arg positions]) up to END_SCRIPT, or None if it isn't a valid script."""
        if start in self.script_cache:
            return self.script_cache[start]
        out, pos = [], start
        result = None
        for _ in range(MAX_INSTRUCTIONS):
            if pos + 4 > len(self.data):
                break
            hdr = struct.unpack_from(">I", self.data, pos)[0]
            op, n = hdr & 0xFFFF, hdr >> 16
            if op not in self.ops or n > MAX_ARGS:
                break
            name = self.ops[op]
            if name in FIXED_ARGS and FIXED_ARGS[name] != n:
                break
            argpos = [pos + 4 + 4 * i for i in range(n)]
            if op == USER_FUNC and (n == 0 or (argpos[0] not in self.dol_ptr and argpos[0] not in self.self_ptr)):
                break
            out.append((pos, name, argpos))
            pos += 4 + 4 * n
            if op == END_SCRIPT:
                result = out
                break
        self.script_cache[start] = result
        return result

    def word(self, pos):
        return struct.unpack_from(">i", self.data, pos)[0]

    def var(self, pos):
        """Readable form of an argument."""
        if pos in self.dol_ptr:
            a = self.dol_ptr[pos]
            return self.syms.name(a) if self.syms.name(a).find("+") < 0 else f"{a:#010x}"
        if pos in self.self_ptr:
            s = self.string_at(self.self_ptr[pos])
            return f'"{s}"' if s is not None else f"rel@{self.self_ptr[pos]:#x}"
        v = self.word(pos)
        for base, name in VAR_BASES:
            if base <= v < base + 10000000 and v - base < 10000:
                return f"{name}({v - base})"
        if -250000000 < v < -220000000:
            return f"{(v + 240000000) / 1024.0:g}f"
        return str(v)

    def const(self, pos):
        """Integer constant argument, or None if it's a variable or pointer."""
        if pos in self.dol_ptr or pos in self.self_ptr:
            return None
        v = self.word(pos)
        return None if v <= -10000000 else v

    def string_at(self, fo):
        end = self.data.find(b"\0", fo)
        s = self.data[fo:end]
        if 0 < len(s) < 64 and all(32 <= c < 127 for c in s):
            return s.decode()
        return None

    # --- reachability ---

    def children(self, script):
        """Scripts referenced from a script: pointer args that decode as scripts, plus scripts referenced
        from data blocks the script points at (NPC tables, door descriptors, ...)."""
        found = set()
        for _, _, argpos in script:
            for p in argpos:
                t = self.self_ptr.get(p)
                if t is None:
                    continue
                if self.decode(t):
                    found.add(t)
                    continue
                for q in range(t, min(t + DATA_SCAN, len(self.data)), 4):
                    t2 = self.self_ptr.get(q)
                    if t2 is not None and self.decode(t2):
                        found.add(t2)
        return found

    def reachable(self, root):
        seen, stack = set(), [root]
        while stack:
            s = stack.pop()
            if s in seen:
                continue
            script = self.decode(s)
            if not script:
                continue
            seen.add(s)
            stack.extend(self.children(script) - seen)
        return seen

    # --- location extraction ---

    def conditions_walk(self, script):
        """Yields (instruction, conditions) with the IF/SWITCH conditions active at each instruction."""
        stack = []  # entries: list of condition strings for that block
        switch_vars = []
        for ins in script:
            pos, name, argpos = ins
            if name in COND_OPS:
                stack.append([f"{self.var(argpos[0])} {COND_OPS[name]} {self.var(argpos[1])}"])
            elif name == "ELSE" and stack:
                stack[-1] = [f"not ({' and '.join(stack[-1])})"]
            elif name == "END_IF" and stack:
                stack.pop()
            elif name in ("SWITCH", "SWITCHI"):
                switch_vars.append(self.var(argpos[0]))
                stack.append([])
            elif name.startswith("CASE_") and switch_vars and stack:
                v = switch_vars[-1]
                if name == "CASE_BETWEEN":
                    stack[-1] = [f"{self.var(argpos[0])} <= {v} <= {self.var(argpos[1])}"]
                elif name == "CASE_ETC":
                    stack[-1] = [f"{v}: other case"]
                elif name != "CASE_END" and argpos:
                    rel = {"CASE_EQUAL": "==", "CASE_NOT_EQUAL": "!=", "CASE_SMALL": "<", "CASE_LARGE": ">",
                           "CASE_SMALL_EQUAL": "<=", "CASE_LARGE_EQUAL": ">="}.get(name, name)
                    stack[-1] = [f"{v} {rel} {self.var(argpos[0])}"]
            elif name == "END_SWITCH" and switch_vars:
                switch_vars.pop()
                if stack:
                    stack.pop()
            yield ins, [c for block in stack for c in block]

    def item_name(self, item):
        if item is None:
            return None
        return self.items.get(item, f"item {item}")

    def extract(self, script_start):
        out = []
        script = self.decode(script_start)
        for (pos, name, argpos), conds in self.conditions_walk(script):
            # Story transitions: SET(GSW(0), value)
            if name == "SET" and self.var(argpos[0]) == "GSW(0)" and self.const(argpos[1]) is not None:
                out.append({"type": "story", "value": self.const(argpos[1]), "script": f"{script_start:#x}",
                            "at": f"{pos:#x}", "conditions": conds})
                continue
            if name != "USER_FUNC" or argpos[0] not in self.dol_ptr:
                continue
            fn = self.syms.name(self.dol_ptr[argpos[0]])
            a = argpos[1:]
            entry = None
            if fn == "evt_item_entry" and len(a) >= 9:
                item = self.const(a[1])
                entry = {"type": "item", "name": self.var(a[0]), "item": item, "item_var": None if item is not None else self.var(a[1]),
                         "flag": self.var(a[8]), "pos": [self.var(a[3]), self.var(a[4]), self.var(a[5])]}
            elif fn == "evt_mobj_thako" and len(a) >= 9:
                item = self.const(a[7])
                entry = {"type": "chest", "name": self.var(a[1]), "item": item, "item_var": None if item is not None else self.var(a[7]),
                         "flag": self.var(a[8])}
            elif fn == "evt_mobj_blk" and len(a) >= 8:
                btype = self.const(a[0])
                item = self.const(a[5])
                table = self.chance_table(a[5])
                if (item and item > 0) or table:
                    entry = {"type": "block", "block": BLOCK_TYPES.get(btype, f"type {btype}"), "name": self.var(a[1]),
                             "item": item if item and item > 0 else None, "chance_table": table, "flag": self.var(a[7])}
            elif fn == "evt_pouch_add_item" and a:
                item = self.const(a[0])
                entry = {"type": "gift", "item": item, "item_var": None if item is not None else self.var(a[0])}
            elif fn == "evt_shop_set_defs" and len(a) >= 2:
                entry = {"type": "shop", "shops": self.shop_defs(a[0], self.const(a[1]) or 0)}
            elif fn == "evt_mobj_zyo" and len(a) >= 2:
                key = self.const(a[1])
                entry = {"type": "lock", "name": self.var(a[0]), "item": key}
            elif fn == "evt_pouch_check_have_item" and a:
                item = self.const(a[0])
                entry = {"type": "check", "item": item, "item_var": None if item is not None else self.var(a[0])}
            if entry is not None and entry["type"] in ("item", "chest", "block") and entry.get("item") in COIN_ITEMS:
                self.coins += 1
                entry = None
            if entry is not None:
                entry["item_name"] = self.item_name(entry.get("item"))
                entry["script"] = f"{script_start:#x}"
                entry["at"] = f"{pos:#x}"
                entry["conditions"] = conds
                out.append(entry)
        return out

    def chance_table(self, pos):
        t = self.self_ptr.get(pos)
        if t is None:
            return None
        rows = []
        for i in range(16):
            item, chance = struct.unpack_from(">ii", self.data, t + 8 * i)
            if item <= 0 and chance <= 0:
                break
            rows.append({"item": item, "item_name": self.item_name(item), "chance": chance})
        return rows or None

    def shop_defs(self, pos, count):
        t = self.self_ptr.get(pos)
        if t is None:
            return []
        shops = []
        for i in range(count):
            base = t + 0x14 * i
            npc = self.self_ptr.get(base + 4)
            items_ptr = self.self_ptr.get(base + 8)
            items = []
            if items_ptr is not None:
                # [itemId, price] pairs ending with -1 (spm-headers' comment says triples; the data says pairs)
                for k in range(64):
                    item, price = struct.unpack_from(">ii", self.data, items_ptr + 8 * k)
                    if item == -1:
                        break
                    items.append({"item": item, "item_name": self.item_name(item), "price": price})
            shops.append({"type": self.word(base), "npc": self.string_at(npc) if npc else None, "items": items})
        return shops


def main():
    sc = Scanner()
    maps = json.load(open(MAPS))
    script_maps = defaultdict(set)
    for m in maps:
        if m["init_rel"] is None:
            continue
        for s in sc.reachable(m["init_rel"]):
            script_maps[s].add(m["name"])

    # A script can be reached from a pointer into its middle as well as its start, so merge entries
    # by instruction address (keeping the longest condition list, from the outermost start)
    by_at = {}
    for s, ms in sorted(script_maps.items()):
        for e in sc.extract(s):
            prev = by_at.get(e["at"])
            if prev is None:
                e["maps"] = set(ms)
                by_at[e["at"]] = e
            else:
                prev["maps"] |= ms
                if len(e["conditions"]) > len(prev["conditions"]):
                    prev["conditions"] = e["conditions"]
    locations = []
    for e in by_at.values():
        e["maps"] = sorted(e["maps"])
        chapters = sorted({str(chapter_of(x)) for x in e["maps"]})
        e["chapter"] = chapters[0] if len(chapters) == 1 else "/".join(chapters)
        locations.append(e)
    locations.sort(key=lambda e: int(e["at"], 16))

    # Coverage: calls of each function anywhere in relF vs calls reached from a map
    for fn in ("evt_item_entry", "evt_mobj_thako", "evt_mobj_blk", "evt_pouch_add_item", "evt_shop_set_defs", "evt_mobj_zyo"):
        addr = sc.fn[fn]
        total = [fo - 4 for fo, a in sc.dol_ptr.items() if a == addr and struct.unpack_from(">I", sc.data, fo - 4)[0] & 0xFFFF == USER_FUNC]
        if fn == "evt_mobj_blk":  # only blocks holding an item (not coins) are locations
            total = [t for t in total if (sc.const(t + 28) or 0) > 2 or sc.self_ptr.get(t + 28) is not None]
        if fn in ("evt_item_entry", "evt_mobj_thako"):
            item_arg = {"evt_item_entry": 3, "evt_mobj_thako": 9}[fn]  # arg index + 1 for the function pointer
            total = [t for t in total if sc.const(t + 4 * item_arg) not in COIN_ITEMS]
        reached = {int(e["at"], 16) for e in locations}
        missed = [f"{t:#x}" for t in total if t not in reached]
        print(f"{fn}: {len(total) - len(missed)}/{len(total)} reached" + (f", missed {missed[:8]}" if missed else ""))

    notes = json.load(open(NOTES)) if os.path.exists(NOTES) else {}
    for e in locations:
        if e["at"] in notes:
            e.update({k: v for k, v in notes[e["at"]].items()})

    stories = [e for e in locations if e["type"] == "story"]
    locations = [e for e in locations if e["type"] != "story"]
    json.dump(locations, open(OUT_JSON, "w"), indent=1)
    json.dump(sorted(stories, key=lambda e: e["value"]), open(OUT_STORY, "w"), indent=1)

    from collections import Counter
    kinds = Counter(e["type"] for e in locations)
    lines = ["# Super Paper Mario locations (US 1.0, from map scripts)", "",
             "Generated by `tools/locscan.py`. Types: item = placed in the world or spawned by a script; "
             "chest; block = ? block or item brick; gift = added directly by a script; shop; "
             "lock = needs the listed key item; check = a script tests for the listed item.", "",
             f"Scripts reached from {len(maps)} maps: {len(script_maps)}.  Entries: " +
             ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())) + f".  Coins left out: {sc.coins}.", ""]
    for chapter in ["hub", "1", "2", "3", "4", "5", "6", "7", "8", "other"]:
        rows = [e for e in locations if e["chapter"] == chapter]
        if not rows:
            continue
        lines += [f"## {'Flipside / Flopside' if chapter == 'hub' else 'Other' if chapter == 'other' else 'Chapter ' + chapter}", "",
                  "| Map | Type | Item | Flag | Conditions |", "|---|---|---|---|---|"]
        for e in sorted(rows, key=lambda e: (e["maps"][0], e["type"])):
            if e["type"] == "shop":
                what = "; ".join(f"{s['npc']}: " + ", ".join(i["item_name"] for i in s["items"]) for s in e["shops"])
            elif e.get("chance_table"):
                what = "random: " + ", ".join(f"{r['item_name']} ({r['chance']})" for r in e["chance_table"])
            else:
                what = e.get("item_name") or e.get("item_var") or ""
                if e["type"] == "block":
                    what = f"{e['block']}: {what}"
            flag = e.get("flag", "")
            mp = e["maps"][0] + (f" (+{len(e['maps']) - 1})" if len(e["maps"]) > 1 else "")
            conds = "; ".join(e["conditions"])[:120]
            kind = e["type"] if e.get("role") in (None, "location") else f"{e['type']} ({e['role'].replace('_', ' ')})"
            if e.get("note"):
                conds = f"**{e['note']}**" + (f"; {conds}" if conds else "")
            lines.append(f"| {mp} | {kind} | {what} | {flag} | {conds} |")
        lines.append("")
    open(OUT_MD, "w", encoding="utf-8").write("\n".join(lines))
    print(f"{len(script_maps)} scripts reached; " + ", ".join(f"{k}: {v}" for k, v in sorted(kinds.items())))


if __name__ == "__main__":
    main()
