#!/usr/bin/env python3
"""Oscillation detector: rounds where a champion steps back onto the tile it occupied two rounds earlier (A->B->A)
without attacking/casting in between, per team. Long runs indicate decision flip-flops (wasted rounds).

usage: US_NAME=orZers python3 tools/oscillation.py DUMP_DIR [-v]
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
NAMES = ["ryt", "ves", "ven", "cas", "iss", "ste", "pri", "mar", "wil", "ren", "var", "tav"]


def main():
    team = os.environ.get("US_NAME", "orZers")
    verbose = "-v" in sys.argv
    agg = {s: collections.Counter() for s in ("us", "opp")}
    per = collections.defaultdict(collections.Counter)
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        js = json.load(open(path))
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        act = collections.defaultdict(set)
        for e in js["ev"]:
            if e[8] == 1 and e[1] in (5, 6, 11, 12) and e[2] < n:
                act[e[0]].add(e[2])
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        for i in range(n):
            side = "us" if TM[i] == us else "opp"
            run = 0
            for k in range(2, len(rs)):
                a, b, c = (R[rs[k - 2]]["ch"][i], R[rs[k - 1]]["ch"][i], R[rs[k]]["ch"][i])
                if not (a[0] and b[0] and c[0]):
                    run = 0
                    continue
                agg[side]["alive"] += 1
                osc = (a[1], a[2]) == (c[1], c[2]) and (a[1], a[2]) != (b[1], b[2]) and i not in act[rs[k - 1]] and i not in act[rs[k]]
                if osc:
                    run += 1
                    agg[side]["osc"] += 1
                    per[(side, NAMES[T[i]])]["osc"] += 1
                else:
                    if run >= 4:
                        agg[side]["runs4"] += 1
                        if verbose and side == "us":
                            print(f"{os.path.basename(path)[:8]} {NAMES[T[i]]}{i} osc run {run} ending R{rs[k]} at ({c[1]},{c[2]}) hp {c[3]}/{c[4]} gems {c[5]}")
                    run = 0
                per[(side, NAMES[T[i]])]["alive"] += 1
            agg[side]["games"] += 0
    for s in ("us", "opp"):
        c = agg[s]
        print(f"{s}: oscillating rounds {c['osc']} of {c['alive']} alive ({c['osc']/max(1,c['alive']):.3%}), runs>=4: {c['runs4']}")
    for (s, nmx), c in sorted(per.items()):
        if c["osc"]:
            print(f"   {s:3} {nmx}: {c['osc']} ({c['osc']/c['alive']:.2%})")


if __name__ == "__main__":
    main()
