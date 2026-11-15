"""For one location (relF file offset of its spawn), every story-value gate on the way from a map's
setup to it: the IF/SWITCH/CASE instruction (file offset), its condition, and the reference site.
Usage: py -3 -E tools/story_gate_paths.py <map> <file offset hex>"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locscan

CMP = {"IF_EQUAL": "==", "IF_NOT_EQUAL": "!=", "IF_SMALL": "<", "IF_LARGE": ">", "IF_SMALL_EQUAL": "<=",
       "IF_LARGE_EQUAL": ">="}
CASES = {"CASE_EQUAL": "==", "CASE_NOT_EQUAL": "!=", "CASE_SMALL": "<", "CASE_LARGE": ">",
         "CASE_SMALL_EQUAL": "<=", "CASE_LARGE_EQUAL": ">=", "CASE_BETWEEN": "between", "CASE_ETC": "other"}

sc = locscan.Scanner()
mapname, target = sys.argv[1], int(sys.argv[2], 16)
m = next(x for x in json.load(open(locscan.MAPS)) if x["name"] == mapname)

def walk(script_start):
    """Yields (pos, name, argpos, gates): gates = list of (gate pos, text) of story conditions open there."""
    stack = []      # per open block: list of (pos, text) or None
    switches = []   # (pos, var)
    for pos, name, argpos in sc.decode(script_start):
        if name in CMP:
            a, b = sc.var(argpos[0]), sc.var(argpos[1])
            stack.append([(pos, f"{name} {a} {b}")] if "GSW(0)" in (a, b) else [])
        elif name == "ELSE" and stack:
            stack[-1] = [(p, "ELSE of " + t) for p, t in stack[-1]]
        elif name == "END_IF" and stack:
            stack.pop()
        elif name in ("SWITCH", "SWITCHI"):
            switches.append((pos, sc.var(argpos[0])))
            stack.append([])
        elif name in CASES and switches and stack:
            spos, var = switches[-1]
            stack[-1] = [(pos, f"{name}({', '.join(sc.var(p) for p in argpos)}) of SWITCH {var} at {spos:#x}")] \
                if var == "GSW(0)" else []
        elif name == "END_SWITCH" and switches:
            switches.pop()
            if stack:
                stack.pop()
        yield pos, name, argpos, [g for blk in stack for g in blk]

def kids_of(argpos):
    out = set()
    for p in argpos:
        t = sc.self_ptr.get(p)
        if t is None:
            continue
        if sc.decode(t):
            out.add(t)
            continue
        for q in range(t, min(t + locscan.DATA_SCAN, len(sc.data)), 4):
            t2 = sc.self_ptr.get(q)
            if t2 is not None and sc.decode(t2):
                out.add(t2)
    return out

results = set()
def dfs(s, path, seen):
    for pos, name, argpos, gates in walk(s):
        here = path + [(s, pos, tuple(gates))]
        if pos <= target < pos + 4 + 4 * len(argpos):
            results.add(tuple((f"{ps:#x}", f"{pp:#x}", g) for ps, pp, g in here if g))
        for k in kids_of(argpos):
            if k not in seen:
                dfs(k, here, seen | {k})

sys.setrecursionlimit(10000)
dfs(m["init_rel"], [], {m["init_rel"]})
for r in sorted(results):
    print("path:")
    for script, at, gates in r:
        for gp, text in gates:
            print(f"   script {script} ref {at}: gate {gp:#x} {text}")
    if not r:
        print("   (no story gates)")
