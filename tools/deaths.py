#!/usr/bin/env python3
"""Death forensics from tools/replay_dump.mjs JSON: who died where, local numbers, who hit them, camp involvement.

usage: python3 tools/deaths.py DUMP_DIR [--us 3,2,9,0,10] [--side us|opp|both] [--verbose]
"""
import argparse
import collections
import glob
import json
import os

NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--us", default="3,2,9,0,10")
    ap.add_argument("--side", default="both")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    cats = {s: collections.Counter() for s in ("us", "opp")}
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        js = json.load(open(path))
        us = find_us(js, us_draft)
        if us is None:
            continue
        name = os.path.basename(path)[:-5]
        R = {int(k): v for k, v in js["rounds"].items()}
        types, teams = js["types"], js["teams"]
        n = len(types)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        first = R[rs[0]]["ch"]
        spawn = {}
        for t in (0, 1):
            ps = [(first[i][X], first[i][Y]) for i in range(n) if teams[i] == t]
            spawn[t] = (sum(p[0] for p in ps) / len(ps), sum(p[1] for p in ps) / len(ps))
        hits = collections.defaultdict(list)  # round -> [(actor, kind, to)]
        for e in js["ev"]:
            if e[1] in (5, 6) and (len(e) < 9 or e[8] == 1):
                hits[e[0]].append((e[2], e[1], (e[5], e[6]), e[7]))
        out = []
        for k in range(1, len(rs)):
            r0, r1 = rs[k - 1], rs[k]
            A0, A1 = R[r0]["ch"], R[r1]["ch"]
            for i in range(n):
                if not (A0[i][ALIVE] and not A1[i][ALIVE]):
                    continue
                side = "us" if teams[i] == us else "opp"
                if a.side != "both" and a.side != side:
                    continue
                t = teams[i]
                vp = (A0[i][X], A0[i][Y])
                rr = R[rs[max(0, k - 6)]]["ch"]
                hp_then = rr[i][HP]
                al = sum(1 for j in range(n) if j != i and teams[j] == t and A0[j][ALIVE] and cheb((A0[j][X], A0[j][Y]), vp) <= 4)
                en = sum(1 for j in range(n) if teams[j] != t and A0[j][ALIVE] and cheb((A0[j][X], A0[j][Y]), vp) <= 4)
                by = collections.Counter()
                for q in range(r1 - 6, r1 + 1):
                    for (act, kind, to, slot) in hits.get(q, []):
                        if act < n and teams[act] != t and cheb(to, vp) <= 2:
                            by[NAMES[types[act]] + ("*" if kind == 6 else "")] += 1
                # camp involvement: awake camps within 3 tiles
                awake = 0
                for c in R[r0]["camps"]:
                    if c[1] and c[2] >= 0 and cheb((c[2], c[3]), vp) <= 3:
                        awake += 1
                ds, de = cheb(vp, spawn[t]), cheb(vp, spawn[1 - t])
                half = "own" if ds < de else "enemy"
                if not by:
                    cause = "camp" if awake else "unseen"
                elif awake:
                    cause = "champ+camp"
                else:
                    cause = "champ"
                ctx = "outnumbered" if en > al + 1 else ("even" if en >= al + 1 - 0 else "ahead")
                cats[side][cause] += 1
                cats[side][half] += 1
                cats[side]["ctx_" + ctx] += 1
                cats[side]["alone" if al == 0 else "with_ally"] += 1
                cats[side]["gems"] += A0[i][GEMS]
                cats[side]["n"] += 1
                out.append(f"  R{r1:4} {side:3} {NAMES[types[i]]:9} @({vp[0]:2},{vp[1]:2}) {half:5} gems={A0[i][GEMS]:3} hp6ago={hp_then:4}/{A0[i][MAXHP]} "
                           f"allies4={al} enemies4={en} awake_camps={awake} cause={cause} by={dict(by)}")
        if a.verbose and out:
            print(f"== {name} (us=T{us})")
            print("\n".join(out))
    for side in ("us", "opp"):
        c = cats[side]
        if not c["n"]:
            continue
        print(f"\n{side}: {c['n']} deaths, gems carried {c['gems']} ({c['gems']/c['n']:.1f}/death)")
        print("  cause: " + ", ".join(f"{k}={c[k]}" for k in ("champ", "champ+camp", "camp", "unseen")))
        print(f"  half: own={c['own']} enemy={c['enemy']}   alone={c['alone']} with_ally={c['with_ally']}")
        print(f"  ctx: outnumbered={c['ctx_outnumbered']} even={c['ctx_even']} ahead={c['ctx_ahead']}")


if __name__ == "__main__":
    main()
