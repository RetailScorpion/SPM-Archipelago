"""Prints a decoded script (locscan's decoder): py -3 -E tools/evtdump.py <script start hex> [from hex] [to hex]
Or with --at <file offset hex>: the script(s) containing that instruction (from data/story_us0.json and
data/locations_us0.json)."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locscan

sc = locscan.Scanner()

def dump(start, lo=0, hi=1 << 31):
    script = sc.decode(start)
    if not script:
        print(f"{start:#x}: not a script")
        return
    print(f"--- script {start:#x}")
    depth = 0
    for pos, name, argpos in script:
        if name in ("END_IF", "END_SWITCH", "ELSE", "END_LOOP", "END_INLINE", "END_BROTHER") or name.startswith("CASE_"):
            depth = max(0, depth - 1)
        if lo <= pos <= hi:
            args = []
            for p in argpos:
                v = sc.var(p)
                if p in sc.self_ptr and not v.startswith('"'):
                    v = f"rel@{sc.self_ptr[p]:#x}"
                args.append(v)
            print(f"{pos:#08x} {'  ' * depth}{name}({', '.join(args)})")
        if name.startswith("IF_") or name in ("SWITCH", "SWITCHI", "ELSE", "LOOP", "INLINE_EVT", "BROTHER_EVT") or name.startswith("CASE_"):
            depth += 1

if sys.argv[1] == "--map":
    m = next(x for x in json.load(open(locscan.MAPS)) if x["name"] == sys.argv[2])
    at = int(sys.argv[3], 16)
    for s in sorted(sc.reachable(m["init_rel"])):
        if any(pos == at for pos, _, _ in sc.decode(s)):
            dump(s, *(int(x, 16) for x in sys.argv[4:6]))
elif sys.argv[1] == "--at":
    at = int(sys.argv[2], 16)
    starts = set()
    for f in (locscan.OUT_STORY, locscan.OUT_JSON):
        for e in json.load(open(f)):
            if "script" in e and not str(e["at"]).startswith("dol") and abs(int(e["at"], 16) - at) < 0x80:
                starts.add(int(e["script"], 16))
    for s in sorted(starts):
        dump(s)
else:
    dump(int(sys.argv[1], 16), *(int(x, 16) for x in sys.argv[2:4]))
