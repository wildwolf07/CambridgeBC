#!/usr/bin/env python3
"""Fight micro audit: what each unit does in rounds when an enemy champion is within 3 tiles.

Categories: hit champion, hit camp, cast only, moved toward nearest enemy, moved away, stayed idle, channelling.
usage: python3 tools/micro.py DUMP_DIR [--us 3,2,9,0,10]   (FORCE_US=0/1 env to fix our team)
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
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    agg = {s: collections.Counter() for s in ("us", "opp")}
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft, path)
        if us is None:
            continue
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        ev = collections.defaultdict(lambda: collections.defaultdict(list))
        for e in js["ev"]:
            if e[2] < n and e[8] == 1:
                ev[e[0]][e[2]].append(e)
        for r in sorted(R):
            ch = R[r]["ch"]
            if len(ch) != n or r + 1 not in R or len(R[r + 1]["ch"]) != n:
                continue
            nx = R[r + 1]["ch"]
            for i in range(n):
                if not ch[i][0]:
                    continue
                p = (ch[i][1], ch[i][2])
                en = [j for j in range(n) if TM[j] != TM[i] and ch[j][0] and cheb((ch[j][1], ch[j][2]), p) <= 3]
                if not en:
                    continue
                s = "us" if TM[i] == us else "opp"
                c = agg[s]
                c["n"] += 1
                es = ev[r].get(i, [])
                epos = {(ch[j][1], ch[j][2]) for j in en}
                allpos = {(ch[j][1], ch[j][2]) for j in range(n) if TM[j] != TM[i] and ch[j][0]}
                if ch[i][11]:
                    c["channel"] += 1
                elif any(e[1] == 5 and (e[5], e[6]) in allpos for e in es):
                    c["hit_champ"] += 1
                elif any(e[1] == 6 and (e[5], e[6]) in allpos for e in es):
                    c["cast_champ"] += 1
                elif any(e[1] == 5 for e in es):
                    c["hit_camp"] += 1
                else:
                    d0 = min(cheb(p, q) for q in epos)
                    q1 = (nx[i][1], nx[i][2]) if nx[i][0] else p
                    d1 = min(cheb(q1, (ch[j][1], ch[j][2])) for j in en)
                    if q1 == p:
                        c["idle"] += 1
                    elif d1 < d0:
                        c["approach"] += 1
                    elif d1 > d0:
                        c["retreat"] += 1
                    else:
                        c["sidestep"] += 1
                hpf = ch[i][3] / max(1, ch[i][4])
                c["hp_sum"] += hpf
    for s, c in agg.items():
        t = max(1, c["n"])
        print(f"{s:3} n={t:6} " + " ".join(f"{k}={c[k]/t:.3f}" for k in ("hit_champ", "cast_champ", "hit_camp", "approach", "retreat", "sidestep", "idle", "channel")) + f" meanhp={c['hp_sum']/t:.2f}")


if __name__ == "__main__":
    main()
