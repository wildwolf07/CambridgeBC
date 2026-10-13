#!/usr/bin/env python3
"""Opening profile (rounds 1..R_END, default 80) per team: share of champion-rounds in the enemy half / centre band,
rounds with an enemy champion within 4, deaths, gems submitted+carried and gold earned at R_END.
usage: US_NAME=Mite [R_END=80] python3 tools/opening.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    team = os.environ.get("US_NAME", "orZers")
    rend = int(os.environ.get("R_END", 80))
    acc = {s: collections.Counter() for s in ("us", "op")}
    ng = 0
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        js = json.load(open(path))
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n and r <= rend)
        if not rs:
            continue
        ng += 1
        sp = {s: [(R[rs[0]]["ch"][i][1], R[rs[0]]["ch"][i][2]) for i in range(n) if TM[i] == s] for s in (0, 1)}
        spc = {s: (sum(p[0] for p in sp[s]) / len(sp[s]), sum(p[1] for p in sp[s]) / len(sp[s])) for s in (0, 1)}
        prev = None
        for r in rs:
            ch = R[r]["ch"]
            for i in range(n):
                side = "us" if TM[i] == us else "op"
                if not ch[i][0]:
                    continue
                p = (ch[i][1], ch[i][2])
                dm, de = cheb(p, spc[TM[i]]), cheb(p, spc[1 - TM[i]])
                acc[side]["alive"] += 1
                acc[side]["enemy_half"] += de < dm
                acc[side]["centre"] += abs(dm - de) <= 4
                acc[side]["contact"] += any(ch[j][0] and TM[j] != TM[i] and cheb(p, (ch[j][1], ch[j][2])) <= 4 for j in range(n))
                if prev and prev[i][0] and not ch[i][0]:
                    pass
            if prev:
                for i in range(n):
                    if prev[i][0] and not ch[i][0]:
                        acc["us" if TM[i] == us else "op"]["deaths"] += 1
            prev = ch
        last = R[rs[-1]]["ch"]
        for s, lab in ((us, "us"), (1 - us, "op")):
            acc[lab]["gems"] += R[rs[-1]]["sub"][s] + sum(last[i][5] for i in range(n) if TM[i] == s)
            acc[lab]["gold"] += sum(last[i][7] for i in range(n) if TM[i] == s)
    print(f"{team}: {ng} games, rounds 1..{rend}")
    for s in ("us", "op"):
        c = acc[s]
        a = max(1, c["alive"])
        print(f"  {s}: enemy half {c['enemy_half']/a:.3f}  centre band {c['centre']/a:.3f}  enemy<=4 {c['contact']/a:.3f}  "
              f"deaths/g {c['deaths']/ng:.2f}  gems/g {c['gems']/ng:.0f}  gold/g {c['gold']/ng:.0f}")


if __name__ == "__main__":
    main()
