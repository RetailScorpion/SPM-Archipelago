"""Checks given by an NPC whose talk (or other) script another script can swap out: for each location
with a script spot, every evt_npc_set_property(npc, n, script) whose script leads to the location, and
every other evt_npc_set_property(npc, n, ...) on the same NPC and property in that map, with the
story-value (GSW(0)) and flag conditions it sits under in its own script. A swap that isn't the
location's own scene ending can hide the check (2026-11-12: Queen Jaydes's key in an1_04).
Usage: py -3 -E tools/talk_swaps.py   Writes data/talk_swaps.txt."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locscan

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "talk_swaps.txt")
CMP = {"IF_EQUAL": "==", "IF_NOT_EQUAL": "!=", "IF_SMALL": "<", "IF_LARGE": ">", "IF_SMALL_EQUAL": "<=",
       "IF_LARGE_EQUAL": ">="}
sc = locscan.Scanner()
maps = json.load(open(locscan.MAPS))
locations = json.load(open(os.path.join(ROOT, "data", "ap_locations.json")))["locations"]


def walk(start):
    """(pos, name, argpos, conditions) for each instruction; conditions = the IF/CASE texts open there."""
    stack, switches = [], []
    for pos, name, argpos in sc.decode(start):
        if name in CMP:
            stack.append(f"{sc.var(argpos[0])} {CMP[name]} {sc.var(argpos[1])}")
        elif name in ("IF_FLAG", "IF_NOT_FLAG"):
            stack.append(f"{name}({', '.join(sc.var(p) for p in argpos)})")
        elif name == "ELSE" and stack:
            stack[-1] = f"not ({stack[-1]})"
        elif name == "END_IF" and stack:
            stack.pop()
        elif name in ("SWITCH", "SWITCHI"):
            switches.append(sc.var(argpos[0]))
            stack.append("")
        elif name.startswith("CASE_") and switches and stack:
            stack[-1] = f"SWITCH {switches[-1]}: {name}({', '.join(sc.var(p) for p in argpos)})"
        elif name == "END_SWITCH" and switches:
            switches.pop()
            if stack:
                stack.pop()
        yield pos, name, argpos, [c for c in stack if c]


lines = []
for m in maps:
    here = [l for l in locations if l["map"] == m["name"] and l["patches"]]
    if not here:
        continue
    scripts = sorted(sc.reachable(m["init_rel"]))
    # which scripts each location's spawn is in
    holders = {}
    for s in scripts:
        for pos, name, argpos in sc.decode(s):
            end = pos + 4 + 4 * len(argpos)
            for l in here:
                for p in l["patches"]:
                    if pos <= p["offset"] < end:
                        holders.setdefault(l["name"], set()).add(s)
    reach = {s: sc.reachable(s) for s in scripts}
    sets = []  # (script, pos, npc, prop, target, conditions)
    for s in scripts:
        for pos, name, argpos, conds in walk(s):
            if name == "USER_FUNC" and len(argpos) >= 4 and sc.var(argpos[0]) == "evt_npc_set_property":
                target = sc.self_ptr.get(argpos[3])
                if target is not None:
                    sets.append((s, pos, sc.var(argpos[1]), sc.var(argpos[2]), target, conds))
    for l in here:
        held = holders.get(l["name"], set())
        givers = [x for x in sets if any(h in reach.get(x[4], ()) or h == x[4] for h in held)]
        for s, pos, npc, prop, target, conds in givers:
            others = [x for x in sets if x[2] == npc and x[3] == prop and x[4] != target
                      and not any(h in reach.get(x[4], ()) for h in held)]
            if not others:
                continue
            lines.append(f"{l['name']}: {npc} property {prop} = {target:#x} (set at {pos:#x}"
                         f"{', under ' + ' and '.join(conds) if conds else ''})")
            for s2, pos2, _, _, target2, conds2 in others:
                inside = " (inside the location's own scene)" if any(s2 in reach.get(target, ()) or s2 == target for _ in [0]) else ""
                lines.append(f"    swapped to {target2:#x} at {pos2:#x}{inside}"
                             f"{', under ' + ' and '.join(conds2) if conds2 else ''}")
open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines))
