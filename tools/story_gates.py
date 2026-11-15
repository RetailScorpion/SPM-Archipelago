"""For every check with a script spot (data.py), the story-value (GSW(0)) range its script can run in,
following the whole call chain from each map's init script (conditions at every reference site).
Also lists every lock (evt_mobj_zyo) whose open script sets the story value.
Usage: py -3 -E tools/story_gates.py   Writes data/story_gates.txt."""
import json, os, re, sys, importlib, types
from collections import defaultdict
sys.path.insert(0, r"F:\Tools\Claude\SPM_AP\tools")
import locscan

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "story_gates.txt")
pkg = types.ModuleType("spm"); pkg.__path__ = [r"F:\Tools\Claude\SPM_AP\apworld\spm"]; sys.modules["spm"] = pkg
data = importlib.import_module("spm.data")

INF = 10 ** 9
sc = locscan.Scanner()
maps = json.load(open(locscan.MAPS))

NEG = {"<": ">=", ">=": "<", ">": "<=", "<=": ">", "==": "!=", "!=": "=="}

def interval(cond):
    """(lo, hi) for GSW(0) from one condition string, or None when it doesn't constrain it."""
    neg = False
    m = re.fullmatch(r"not \((.*)\)", cond)
    if m:
        if " and " in m.group(1):
            return None
        cond, neg = m.group(1), True
    m = re.fullmatch(r"(-?\d+) <= GSW\(0\) <= (-?\d+)", cond)
    if m:
        return None if neg else (int(m.group(1)), int(m.group(2)))
    m = re.fullmatch(r"GSW\(0\) (<|>|<=|>=|==|!=) (-?\d+)", cond)
    if not m:
        return None
    op, v = m.group(1), int(m.group(2))
    if neg:
        op = NEG[op]
    return {"<": (-INF, v - 1), "<=": (-INF, v), ">": (v + 1, INF), ">=": (v, INF),
            "==": (v, v), "!=": None}[op]

def narrow(iv, conds):
    lo, hi = iv
    for c in conds:
        r = interval(c)
        if r:
            lo, hi = max(lo, r[0]), min(hi, r[1])
    return lo, hi

# Reference sites: (parent script, instruction) -> child scripts, with the conditions there
def refs(script_start):
    script = sc.decode(script_start)
    out = []
    for (pos, name, argpos), conds in sc.conditions_walk(script):
        kids = set()
        for p in argpos:
            t = sc.self_ptr.get(p)
            if t is None:
                continue
            if sc.decode(t):
                kids.add(t)
                continue
            for q in range(t, min(t + locscan.DATA_SCAN, len(sc.data)), 4):
                t2 = sc.self_ptr.get(q)
                if t2 is not None and sc.decode(t2):
                    kids.add(t2)
        if kids:
            out.append((pos, name, argpos, conds, kids))
    return out

ref_cache = {}
ranges = defaultdict(set)   # script -> {(lo, hi, map)}
for m in maps:
    if m["init_rel"] is None:
        continue
    stack = [(m["init_rel"], (-INF, INF))]
    seen = set()
    while stack:
        s, iv = stack.pop()
        if (s, iv) in seen or iv[0] > iv[1]:
            continue
        seen.add((s, iv))
        ranges[s].add((iv[0], iv[1], m["name"]))
        if s not in ref_cache:
            ref_cache[s] = refs(s)
        for pos, name, argpos, conds, kids in ref_cache[s]:
            iv2 = narrow(iv, conds)
            for k in kids:
                stack.append((k, iv2))

locs = json.load(open(locscan.OUT_JSON))
entries = sorted(((int(e["at"], 16), e) for e in locs if "script" in e and not e["at"].startswith("dol")), key=lambda x: x[0])

def entry_for(off):
    best = None
    for at, e in entries:
        if at <= off < at + 4 + 4 * 24:
            if best is None or at > best[0]:
                best = (at, e)
    return best[1] if best else None

def fmt(lo, hi):
    return f"{'' if lo <= -INF else lo}..{'' if hi >= INF else hi}"

lines = ["# Checks whose script only runs in a story-value range", ""]
for loc in data.LOCATION_TABLE if hasattr(data, "LOCATION_TABLE") else data.LOCATIONS:
    name, kind = loc[0], loc[1]
    spots = loc[5] if len(loc) > 5 else []
    for off, item in spots or []:
        e = entry_for(off)
        if e is None:
            lines.append(f"?? {name}: no scanned entry for {off:#x}")
            continue
        s = int(e["script"], 16)
        rs = ranges.get(s, set())
        per = []
        for lo, hi, mp in rs:
            lo2, hi2 = narrow((lo, hi), e["conditions"])
            if lo2 <= hi2:
                per.append((lo2, hi2, mp))
        if not per:
            lines.append(f"!! {name}: unreachable? script {s:#x}")
            continue
        lo = min(p[0] for p in per); hi = max(p[1] for p in per)
        if hi < INF:
            mps = sorted({p[2] for p in per})
            lines.append(f"{name}: GSW(0) {fmt(lo, hi)}  [{', '.join(mps[:4])}]  script {s:#x} at {e['at']}")

lines += ["", "# Locks whose open script sets the story value", ""]
zyo = sc.fn["evt_mobj_zyo"]
for s, rlist in ref_cache.items():
    for pos, name, argpos, conds, kids in rlist:
        if name != "USER_FUNC" or sc.dol_ptr.get(argpos[0]) != zyo:
            continue
        key = sc.const(argpos[2])
        sets = []
        for k in kids:
            for r in sc.reachable(k):
                for (p2, n2, a2), c2 in sc.conditions_walk(sc.decode(r)):
                    if n2 == "SET" and sc.var(a2[0]) == "GSW(0)":
                        sets.append((sc.const(a2[1]), hex(p2)))
        if sets:
            mps = sorted({mp for _, _, mp in ranges.get(s, set())})
            lines.append(f"lock key {key} ({sc.item_name(key)}) at {pos:#x} [{', '.join(mps[:3])}] "
                         f"appears in GSW(0) {fmt(*narrow((-INF, INF), conds))}: sets {sorted(set(sets))}")
open(OUT, "w", encoding="utf-8").write("\n".join(lines))
print(len(lines), "lines ->", OUT)

# ---- Story sets that can jump past each flagged check's window ----
stories = json.load(open(locscan.OUT_STORY))
set_ranges = []
for e in stories:
    s = int(e["script"], 16)
    for lo, hi, mp in ranges.get(s, set()):
        lo2, hi2 = narrow((lo, hi), e["conditions"])
        if lo2 <= hi2:
            set_ranges.append((e["value"], lo2, hi2, mp, e["at"]))
out = ["", "# Story sets that can run inside a check's window and jump past it", ""]
for ln in lines:
    m = re.match(r"(.+?): GSW\(0\) (-?\d*)\.\.(-?\d*)  \[(.*?)\]", ln)
    if not m:
        continue
    lo = int(m.group(2)) if m.group(2) else -INF
    hi = int(m.group(3)) if m.group(3) else INF
    chap = m.group(4).split(",")[0][:2]
    jumps = sorted({(v, mp, at, f"{fmt(l, h)}") for v, l, h, mp, at in set_ranges
                    if v > hi and l <= hi and h >= max(lo, hi - 30) and mp[:2] == chap and v < hi + 40})
    out.append(f"{m.group(1)} (window {fmt(lo, hi)}):")
    for v, mp, at, r in jumps:
        out.append(f"    -> {v} in {mp} at {at}, runs in {r}")
open(OUT, "a", encoding="utf-8").write("\n".join(out))

# ---- Extra spots (story gifts), by relF file offset ----
extra = {"Bowser rejoins (an2_04)": 0x1a30e4, "Peach rejoins (an3_06)": 0x1b29f4, "Bowser joins (ta1_04)": 0x123a24}
out = ["", "# Story gifts", ""]
for what, off in extra.items():
    for s in ranges:
        sc_ = sc.decode(s)
        hit = None
        for (pos, name, argpos), conds in sc.conditions_walk(sc_):
            if pos <= off < pos + 4 + 4 * len(argpos):
                hit = conds
        if hit is None:
            continue
        per = [narrow((lo, hi), hit) for lo, hi, mp in ranges[s]]
        per = [p for p in per if p[0] <= p[1]]
        if per:
            out.append(f"{what}: script {s:#x}: GSW(0) {fmt(min(p[0] for p in per), max(p[1] for p in per))}  conds {hit}")
open(OUT, "a", encoding="utf-8").write("\n".join(out))
