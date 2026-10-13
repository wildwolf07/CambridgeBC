#!/usr/bin/env python3
"""Where does each alive champion-round go? Per team, per champion type.

Categories (first match wins): recall/submit channel, at spawn, hit champion, hit camp, on dead-camp tile (capturing),
moved (travel), idle.
usage: python3 tools/timebudget.py DUMP_DIR [--us 3,2,9,0,10]
"""
import argparse, collections, glob, json, os
ALIVE, X, Y, HP, MAXHP, GEMS, GOLD, EARNED, K, D, A, CHT, CHTM, STR, FOC = range(15)
NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
CATS = ["recall", "submit", "spawn", "fight", "camp", "capture", "travel", "idle"]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def find_us(js, us_draft):
    drafts = js["drafts"]
    c = [t for t in (0, 1) if [x for x in drafts[t] if x >= 0] == us_draft[:len([x for x in drafts[t] if x >= 0])]]
    if len(c) == 1:
        return c[0]
    if len(c) == 2:
        lw = [sum(1 for r in js["rounds"].values() for l in r["locks"] + r["wards"] if l[0] == t) for t in (0, 1)]
        if lw[0] != lw[1]:
            return 0 if lw[0] < lw[1] else 1
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--us", default="3,2,9,0,10")
    ap.add_argument("--per-game", action="store_true")
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    agg = {s: collections.Counter() for s in ("us", "opp")}
    per = {s: collections.defaultdict(collections.Counter) for s in ("us", "opp")}
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft)
        if us is None:
            continue
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        first = R[rs[0]]["ch"]
        spawn = {t: [(first[i][X], first[i][Y]) for i in range(n) if TM[i] == t] for t in (0, 1)}
        camps = js["camps"] or []
        campset = {(c[2], c[3]) for c in camps}
        acts = collections.defaultdict(lambda: collections.defaultdict(set))
        for e in js["ev"]:
            if e[2] < n and (len(e) < 9 or e[8] == 1):
                acts[e[0]][e[2]].add((e[1], (e[5], e[6])))
        g = {s: collections.Counter() for s in ("us", "opp")}
        for r in rs:
            ch = R[r]["ch"]
            dead_tiles = {(cp[2], cp[3]) for ci, cp in enumerate(camps) if R[r]["camps"][ci][0] <= 0}
            for i in range(n):
                if not ch[i][ALIVE]:
                    continue
                s = "us" if TM[i] == us else "opp"
                p = (ch[i][X], ch[i][Y])
                ev = acts[r].get(i, set())
                if ch[i][CHT] == 1:
                    cat = "recall"
                elif ch[i][CHT] == 2:
                    cat = "submit"
                elif min(cheb(p, q) for q in spawn[TM[i]]) <= 1 and not ev:
                    cat = "spawn"
                elif any(k in (5, 6) and to not in campset for k, to in ev):
                    cat = "fight"
                elif any(k in (5, 6) and to in campset for k, to in ev):
                    cat = "camp"
                elif p in dead_tiles:
                    cat = "capture"
                elif any(k == 4 for k, to in ev):
                    cat = "travel"
                else:
                    cat = "idle"
                g[s][cat] += 1
                per[s][NAMES[T[i]]][cat] += 1
        for s in g:
            agg[s] += g[s]
        if a.per_game:
            print(os.path.basename(path)[:28], " | ".join(s + ": " + " ".join(f"{c}={g[s][c]/max(1,sum(g[s].values())):.2f}" for c in CATS) for s in ("us", "opp")))
    for s in ("us", "opp"):
        t = sum(agg[s].values())
        print(f"{s:3} n={t:6} " + " ".join(f"{c}={agg[s][c]/t:.3f}" for c in CATS))
    for s in ("us", "opp"):
        for nm, c in sorted(per[s].items()):
            t = sum(c.values())
            print(f"  {s:3} {nm:9} n={t:5} " + " ".join(f"{k}={c[k]/t:.2f}" for k in CATS))


if __name__ == "__main__":
    main()
