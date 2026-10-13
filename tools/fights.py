#!/usr/bin/env python3
"""Fight episodes: clusters of champion-vs-champion hits (<=6 tiles, <=4 rounds apart).

Per episode: champions of each side that hit anything in it, deaths per side, numbers present at start vs end.
usage: python3 tools/fights.py DUMP_DIR [--us 3,2,9,0,10] [-v]
"""
import argparse, collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from timebudget import find_us  # noqa: E402
NAMES = ["ryt", "ves", "ven", "cas", "iss", "ste", "pri", "mar", "wil", "ren", "var", "tav"]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--us", default="3,2,9,0,10")
    ap.add_argument("-v", action="store_true")
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    agg = collections.Counter()
    table = collections.Counter()
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft)
        if us is None:
            continue
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        hits = []
        for e in js["ev"]:
            r = e[0]
            if e[1] in (5, 6) and e[2] < n and e[8] == 1 and r in R and len(R[r]["ch"]) == n:
                ch = R[r]["ch"]
                for i in range(n):
                    if ch[i][0] and TM[i] != TM[e[2]] and (ch[i][1], ch[i][2]) == (e[5], e[6]):
                        hits.append((r, e[2], i, (e[5], e[6])))
                        break
        hits.sort()
        eps = []
        for h in hits:
            for ep in eps:
                if h[0] - ep["last"] <= 4 and cheb(h[3], ep["pos"]) <= 6:
                    ep["last"] = h[0]
                    ep["pos"] = h[3]
                    ep["att"][TM[h[1]]].add(h[1])
                    ep["vic"][TM[h[2]]].add(h[2])
                    break
            else:
                eps.append({"first": h[0], "last": h[0], "pos": h[3], "start": h[3], "att": {0: {h[1]} if TM[h[1]] == 0 else set(), 1: {h[1]} if TM[h[1]] == 1 else set()}, "vic": {0: set(), 1: set()}})
                eps[-1]["vic"][TM[h[2]]].add(h[2])
        for ep in eps:
            r0, r1 = ep["first"], ep["last"]
            dead = {0: 0, 1: 0}
            for i in range(n):
                for r in range(r0, r1 + 3):
                    if r in R and r - 1 in R and len(R[r]["ch"]) == n and R[r - 1]["ch"][i][0] and not R[r]["ch"][i][0]:
                        p = (R[r - 1]["ch"][i][1], R[r - 1]["ch"][i][2])
                        if cheb(p, ep["pos"]) <= 8 or cheb(p, ep["start"]) <= 8:
                            dead[TM[i]] += 1
                        break
            near = {0: 0, 1: 0}
            ch0 = R[r0]["ch"]
            for i in range(n):
                if ch0[i][0] and cheb((ch0[i][1], ch0[i][2]), ep["start"]) <= 6:
                    near[TM[i]] += 1
            ua, oa = len(ep["att"][us]), len(ep["att"][1 - us])
            ud, od = dead[us], dead[1 - us]
            agg["eps"] += 1
            agg["us_att"] += ua
            agg["op_att"] += oa
            agg["us_dead"] += ud
            agg["op_dead"] += od
            key = ("us0" if ua == 0 else "us>" if ua > oa else "us=" if ua == oa else "us<")
            table[key + "_n"] += 1
            table[key + "_ud"] += ud
            table[key + "_od"] += od
            if a.v and (ud or od):
                print(f"{os.path.basename(path)[:22]:22} R{r0}-{r1} @{ep['start']} near us/op={near[us]}/{near[1-us]} attackers us/op={ua}/{oa} dead us/op={ud}/{od}")
    print(f"episodes={agg['eps']} avg attackers us={agg['us_att']/agg['eps']:.2f} op={agg['op_att']/agg['eps']:.2f} deaths us={agg['us_dead']} op={agg['op_dead']}")
    for k in ("us0", "us>", "us=", "us<"):
        print(f"  {k} n={table[k+'_n']:4} our deaths={table[k+'_ud']:4} their deaths={table[k+'_od']:4}")


if __name__ == "__main__":
    main()
