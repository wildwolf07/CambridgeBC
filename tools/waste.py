#!/usr/bin/env python3
"""Wasted-action diagnostics from tools/replay_dump.mjs JSON.

Per team: idle turns (alive, not channelling, no move/attack/cast), kill->capture delay and steals,
rounds spent next to an enemy-locked altar while carrying gems, failed/cancelled tributes.
usage: python3 tools/waste.py DUMP_DIR [--us 3,2,9,0,10]
"""
import argparse, collections, glob, json, os
ALIVE, X, Y, HP, MAXHP, GEMS, GOLD, EARNED, K, D, A, CHT, CHTM, STR, FOC = range(15)
NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]


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
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    tot = {"us": collections.Counter(), "opp": collections.Counter()}
    delays = {"us": [], "opp": []}
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft)
        if us is None:
            continue
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        side = lambda t: "us" if t == us else "opp"
        acted = collections.defaultdict(set)  # round -> actor indices that moved/attacked/cast/etc
        for e in js["ev"]:
            if e[2] < n and (len(e) < 9 or e[8] == 1):
                acted[e[0]].add(e[2])
        c = {"us": collections.Counter(), "opp": collections.Counter()}
        for r in rs:
            ch = R[r]["ch"]
            locks = R[r]["locks"]
            for i in range(n):
                s = side(TM[i])
                if not ch[i][ALIVE]:
                    continue
                c[s]["alive"] += 1
                if ch[i][CHT]:
                    continue
                if i not in acted.get(r, ()):
                    c[s]["idle"] += 1
                    if ch[i][GEMS] > 0:
                        c[s]["idle_with_gems"] += 1
                p = (ch[i][X], ch[i][Y])
                for l in locks:
                    if l[0] != TM[i] and cheb(p, (l[1], l[2])) <= 2 and ch[i][GEMS] > 0:
                        c[s]["near_enemy_lock_with_gems"] += 1
        # kill -> capture: camp hp>0 -> 0, then find first gem gain by a champion on that tile
        camps = js["camps"] or []
        for k in range(1, len(rs)):
            r0, r1 = rs[k - 1], rs[k]
            for ci, cp in enumerate(camps):
                if not (R[r0]["camps"][ci][0] > 0 and R[r1]["camps"][ci][0] <= 0):
                    continue
                if cp[0] == 4:
                    continue
                killers = [i for i in range(n) if R[r0]["ch"][i][ALIVE] and cheb((R[r0]["ch"][i][X], R[r0]["ch"][i][Y]), (cp[2], cp[3])) <= 3]
                kt = collections.Counter(TM[i] for i in killers)
                kteam = kt.most_common(1)[0][0] if kt else None
                got = None
                for q in range(r1, r1 + 60):
                    if q not in R or q - 1 not in R or len(R[q]["ch"]) != n:
                        continue
                    for i in range(n):
                        a0, b0 = R[q - 1]["ch"][i], R[q]["ch"][i]
                        if a0[ALIVE] and b0[ALIVE] and b0[GEMS] - a0[GEMS] > 0 and cheb((b0[X], b0[Y]), (cp[2], cp[3])) == 0:
                            got = (q - r1, TM[i])
                            break
                    if got:
                        break
                if kteam is None:
                    continue
                s = side(kteam)
                c[s]["kills"] += 1
                if got is None:
                    c[s]["uncaptured60"] += 1
                elif got[1] != kteam:
                    c[s]["stolen_from"] += 1
                else:
                    delays[s].append(got[0])
        for s in ("us", "opp"):
            tot[s] += c[s]
        print(f"{os.path.basename(path)[:30]:30} " + " | ".join(
            f"{s}: idle={c[s]['idle']/max(1,c[s]['alive']):.2f} idleG={c[s]['idle_with_gems']} lockG={c[s]['near_enemy_lock_with_gems']} kills={c[s]['kills']} unc={c[s]['uncaptured60']} stolen={c[s]['stolen_from']}"
            for s in ("us", "opp")))
    for s in ("us", "opp"):
        t = tot[s]
        d = delays[s]
        d.sort()
        print(f"\nTOTAL {s}: idle_frac={t['idle']/max(1,t['alive']):.3f} idle_with_gems={t['idle_with_gems']} near_enemy_lock_with_gems={t['near_enemy_lock_with_gems']} "
              f"kills={t['kills']} uncaptured60={t['uncaptured60']} stolen={t['stolen_from']} capture_delay mean={sum(d)/max(1,len(d)):.1f} "
              f"median={d[len(d)//2] if d else -1} p90={d[int(len(d)*0.9)] if d else -1} n={len(d)}")


if __name__ == "__main__":
    main()
