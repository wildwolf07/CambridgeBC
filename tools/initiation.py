#!/usr/bin/env python3
"""Conditions at the start of fights a team initiates (struck first) vs fights it is jumped in:
participant ratio, strength ratio, HP-fraction difference, distance of the nearest extra ally; outcome.
usage: US_NAME=Mite python3 tools/initiation.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
from fight_anatomy import episodes, cheb  # noqa: E402


def main():
    team = os.environ.get("US_NAME", "orZers")
    rows = []
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        js = json.load(open(path))
        R, T, TM, n, eps, casts = episodes(js)
        for ep in eps:
            r0, r1 = ep["first"], ep["last"]
            first = TM[ep["hits"][0][1]]
            pre = max([r for r in R if r < r0] or [r0])
            ch = R[pre]["ch"]
            if len(ch) != n:
                continue
            # local snapshot: all alive champions within 6 of the first hit's location
            c0 = ep["start"]
            near = {s: [i for i in range(n) if ch[i][0] and TM[i] == s and cheb((ch[i][1], ch[i][2]), c0) <= 6] for s in (0, 1)}
            if not near[us] or not near[1 - us]:
                continue
            st = {s: sum(ch[i][13] for i in near[s]) for s in (0, 1)}
            hpf = {s: sum(ch[i][3] for i in near[s]) / max(1, sum(ch[i][4] for i in near[s])) for s in (0, 1)}
            dead = {0: 0, 1: 0}
            for i in range(n):
                for r in range(r0, r1 + 3):
                    if r in R and r - 1 in R and len(R[r]["ch"]) == n and R[r - 1]["ch"][i][0] and not R[r]["ch"][i][0]:
                        p = (R[r - 1]["ch"][i][1], R[r - 1]["ch"][i][2])
                        if cheb(p, ep["pos"]) <= 8 or cheb(p, ep["start"]) <= 8:
                            dead[TM[i]] += 1
                        break
            rows.append(dict(init=first == us, nu=len(near[us]), no=len(near[1 - us]), sr=st[us] / max(1, st[1 - us]),
                             hd=hpf[us] - hpf[1 - us], ud=dead[us], od=dead[1 - us]))
    def show(lab, rs):
        if not rs:
            return
        N = len(rs)
        print(f"  {lab:28} n={N:5}  deaths us/them {sum(r['ud'] for r in rs):4}/{sum(r['od'] for r in rs):4}")
    print(f"{team}: {len(rows)} episodes")
    for init in (True, False):
        rs = [r for r in rows if r["init"] == init]
        tag = "INITIATED" if init else "JUMPED"
        N = max(1, len(rs))
        print(f" {tag}: n={len(rs)}  mean numbers {sum(r['nu'] for r in rs)/N:.2f} vs {sum(r['no'] for r in rs)/N:.2f}  "
              f"str ratio median {sorted(r['sr'] for r in rs)[len(rs)//2] if rs else 0:.2f}  hp diff mean {sum(r['hd'] for r in rs)/N:+.2f}")
        show("numbers ahead", [r for r in rs if r["nu"] > r["no"]])
        show("numbers even", [r for r in rs if r["nu"] == r["no"]])
        show("numbers behind", [r for r in rs if r["nu"] < r["no"]])
        show("str ratio >=1.2", [r for r in rs if r["sr"] >= 1.2])
        show("str ratio 0.85-1.2", [r for r in rs if 0.85 <= r["sr"] < 1.2])
        show("str ratio <0.85", [r for r in rs if r["sr"] < 0.85])


if __name__ == "__main__":
    main()
