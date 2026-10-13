#!/usr/bin/env python3
"""Team behaviour profiles from tools/replay_dump.mjs JSON: cohesion, territory, income sources, banking, shopping.

usage: python3 tools/profile.py DUMP_DIR [--us 3,2,9,0,10]
"""
import argparse
import collections
import glob
import json
import os

NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
DRAFT = ["ryth", "vesper", "vendetta", "C&I", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
CAMP = ["wolf", "elk", "gem", "gold", "mono"]
ALIVE, X, Y, HP, MAXHP, GEMS, GOLD, EARNED, K, D, A, CHT, CHTM, STR, FOC = range(15)


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def find_us(js, us_draft):
    drafts = js["drafts"]
    cands = [t for t in (0, 1) if [x for x in drafts[t] if x >= 0] == us_draft[:len([x for x in drafts[t] if x >= 0])]]
    if len(cands) == 1:
        return cands[0]
    if len(cands) == 2:
        lw = [sum(1 for r in js["rounds"].values() for l in r["locks"] + r["wards"] if l[0] == t) for t in (0, 1)]
        if lw[0] != lw[1]:
            return 0 if lw[0] < lw[1] else 1
    return None


def profile(js, team):
    R = {int(k): v for k, v in js["rounds"].items()}
    types, teams = js["types"], js["teams"]
    n = len(types)
    rs = sorted(r for r in R if len(R[r]["ch"]) == n)
    first = R[rs[0]]["ch"]
    spawn = {}
    for t in (0, 1):
        ps = [(first[i][X], first[i][Y]) for i in range(n) if teams[i] == t]
        spawn[t] = (sum(p[0] for p in ps) / len(ps), sum(p[1] for p in ps) / len(ps))
    camps = js["camps"] or []
    c = collections.Counter()
    mine = [i for i in range(n) if teams[i] == team]
    prev = None
    for r in rs:
        ch = R[r]["ch"]
        for i in mine:
            if not ch[i][ALIVE]:
                continue
            p = (ch[i][X], ch[i][Y])
            c["alive"] += 1
            near = sum(1 for j in mine if j != i and ch[j][ALIVE] and cheb((ch[j][X], ch[j][Y]), p) <= 4
                       and not (types[i] in (3, 4) and types[j] in (3, 4)))
            c["near_sum"] += near
            c["alone"] += near == 0
            ds, de = cheb(p, spawn[team]), cheb(p, spawn[1 - team])
            c["enemy_half"] += de < ds
            c["at_spawn"] += ds <= 2
            if prev is not None and prev[i][ALIVE] and ch[i][GEMS] > prev[i][GEMS]:
                g = ch[i][GEMS] - prev[i][GEMS]
                best, bd = None, 99
                for k, cp in enumerate(camps):
                    d = cheb((cp[2], cp[3]), p)
                    if d < bd:
                        bd, best = d, k
                if best is not None and bd <= 4:
                    c["g_" + CAMP[camps[best][0]]] += g
                    side = "own" if cheb((camps[best][2], camps[best][3]), spawn[team]) < cheb((camps[best][2], camps[best][3]), spawn[1 - team]) else "enemy"
                    c["g_" + side] += g
                else:
                    c["g_other"] += g
        prev = ch
    # submissions: group consecutive submit channels; count size distribution from submit events
    sizes = []
    for e in js["ev"]:
        if e[1] == 12 and e[2] < n and teams[e[2]] == team:
            r = e[0]
            if r - 1 in R and len(R[r - 1]["ch"]) == n:
                sizes.append(R[r - 1]["ch"][e[2]][GEMS])
    c["subs"] = len(sizes)
    c["sub_gems"] = sum(sizes)
    buys = sum(1 for e in js["ev"] if e[1] == 13 and e[2] < n and teams[e[2]] == team)
    c["buys"] = buys
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--us", default="3,2,9,0,10")
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    agg = {"us": collections.Counter(), "opp": collections.Counter()}
    print(f"{'game':28} {'side':4} {'draft':32} near alone enemyH | gems: gem wolf elk gold other own enemy | sub avg")
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft)
        name = os.path.basename(path)[:-5][:28]
        for t in (0, 1):
            c = profile(js, t)
            side = "us" if t == us else ("opp" if us is not None else f"T{t}")
            if side in agg:
                agg[side] += c
            al = max(1, c["alive"])
            d = " ".join(DRAFT[x] for x in js["drafts"][t] if x >= 0)
            print(f"{name:28} {side:4} {d:32} {c['near_sum']/al:4.2f} {c['alone']/al:5.2f} {c['enemy_half']/al:5.2f} | "
                  f"{c['g_gem']:4} {c['g_wolf']:4} {c['g_elk']:4} {c['g_gold']:4} {c['g_other']:4} {c['g_own']:4} {c['g_enemy']:4} | "
                  f"{c['subs']:3} {c['sub_gems']/max(1,c['subs']):5.1f}")
    for side, c in agg.items():
        al = max(1, c["alive"])
        print(f"\nTOTAL {side}: near={c['near_sum']/al:.2f} alone={c['alone']/al:.2f} enemy_half={c['enemy_half']/al:.2f} "
              f"gems gem/wolf/elk/other={c['g_gem']}/{c['g_wolf']}/{c['g_elk']}/{c['g_other']} own/enemy={c['g_own']}/{c['g_enemy']} "
              f"subs={c['subs']} avg_sub={c['sub_gems']/max(1,c['subs']):.1f}")


if __name__ == "__main__":
    main()
