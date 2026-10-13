#!/usr/bin/env python3
"""Classify deaths by what the victim was doing in its last 3 alive rounds.

modes: recall (died channelling recall), submit (died channelling a tribute), farming (hit a camp while being hit by
champions), fleeing (moved away from attackers without hitting back), trading (hit champions), passive (none).
usage: US_NAME=orZers python3 tools/death_modes.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    us_name = os.environ.get("US_NAME", "orZers")
    agg = {s: collections.Counter() for s in ("us", "opp")}
    per = {s: collections.defaultdict(collections.Counter) for s in ("us", "opp")}
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        names = ladder_labels.names(path)
        if not names:
            continue
        us = 0 if names[0] == us_name else (1 if names[1] == us_name else None)
        if us is None:
            continue
        js = json.load(open(path))
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        camps = {(c[2], c[3]) for c in (js["camps"] or [])}
        ev = collections.defaultdict(lambda: collections.defaultdict(list))
        for e in js["ev"]:
            if e[2] < n and e[8] == 1:
                ev[e[0]][e[2]].append(e)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        for k in range(4, len(rs)):
            r0, r1 = rs[k - 1], rs[k]
            for i in range(n):
                a, b = R[r0]["ch"][i], R[r1]["ch"][i]
                if not (a[0] and not b[0]):
                    continue
                side = "us" if TM[i] == us else "opp"
                window = rs[k - 3:k]
                hit_camp = hit_champ = 0
                for r in window:
                    for e in ev[r].get(i, []):
                        if e[1] in (5, 6):
                            to = (e[5], e[6])
                            if to in camps:
                                hit_camp += 1
                            elif any(R[r]["ch"][j][0] and TM[j] != TM[i] and (R[r]["ch"][j][1], R[r]["ch"][j][2]) == to for j in range(n)):
                                hit_champ += 1
                if a[11] == 1:
                    mode = "recall"
                elif a[11] == 2:
                    mode = "submit"
                elif hit_champ:
                    mode = "trading"
                elif hit_camp:
                    mode = "farming"
                else:
                    # moved away from nearest enemy over the window?
                    p0 = (R[window[0]]["ch"][i][1], R[window[0]]["ch"][i][2])
                    p1 = (a[1], a[2])
                    ens = [j for j in range(n) if TM[j] != TM[i] and a[j][0]] if False else [j for j in range(n) if TM[j] != TM[i] and R[r0]["ch"][j][0]]
                    if ens:
                        jn = min(ens, key=lambda j: cheb(p1, (R[r0]["ch"][j][1], R[r0]["ch"][j][2])))
                        q0 = (R[window[0]]["ch"][jn][1], R[window[0]]["ch"][jn][2])
                        q1 = (R[r0]["ch"][jn][1], R[r0]["ch"][jn][2])
                        mode = "fleeing" if cheb(p1, q1) >= cheb(p0, q0) and p1 != p0 else "passive"
                    else:
                        mode = "passive"
                agg[side][mode] += 1
                agg[side]["n"] += 1
                agg[side]["gems"] += a[5]
                per[side][NAMES[T[i]]][mode] += 1
    for s in ("us", "opp"):
        c = agg[s]
        if not c["n"]:
            continue
        print(f"{s}: {c['n']} deaths  " + "  ".join(f"{m}={c[m]} ({c[m]/c['n']:.0%})" for m in ("trading", "farming", "fleeing", "recall", "submit", "passive")) + f"  gems lost {c['gems']}")
        for nm, cc in sorted(per[s].items(), key=lambda x: -sum(x[1].values())):
            print(f"    {nm:9} " + " ".join(f"{m}={cc[m]}" for m in ("trading", "farming", "fleeing", "recall", "submit", "passive")))


if __name__ == "__main__":
    main()
