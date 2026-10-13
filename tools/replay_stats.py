#!/usr/bin/env python3
"""Per-team and per-champion statistics from tools/replay_dump.mjs JSON files.

usage: python3 tools/replay_stats.py DUMP_DIR [--us ourdraft]   (default: detect our draft 3,2,9,0,10)
"""
import argparse
import collections
import glob
import json
import os

NAMES = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
DRAFT = ["ryth", "vesper", "vendetta", "C&I", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
# ch columns
ALIVE, X, Y, HP, MAXHP, GEMS, GOLD, EARNED, K, D, A, CHT, CHTM, STR, FOC = range(15)


def load(path):
    with open(path) as f:
        return json.load(f)


def analyse(js, us_draft):
    R = {int(k): v for k, v in js["rounds"].items()}
    rs = sorted(R)
    types, teams = js["types"], js["teams"]
    n = len(types)
    drafts = js["drafts"]
    us = None
    cands = [t for t in (0, 1) if [x for x in drafts[t] if x >= 0] == us_draft[:len([x for x in drafts[t] if x >= 0])]]
    if len(cands) == 1:
        us = cands[0]
    elif len(cands) == 2:
        # Mirror draft: we never place altar locks or wards.
        lw = [sum(1 for r in js["rounds"].values() for l in r["locks"] + r["wards"] if l[0] == t) for t in (0, 1)]
        if lw[0] != lw[1]:
            us = 0 if lw[0] < lw[1] else 1
    fin = js["final_sub"]
    winner = 0 if fin[0] > fin[1] else 1
    st = {t: collections.Counter() for t in (0, 1)}
    per = collections.defaultdict(collections.Counter)  # (team, type) -> counter
    prev = None
    for r in rs:
        cur = R[r]["ch"]
        if len(cur) != n:
            continue
        if prev is not None:
            for i in range(n):
                t = teams[i]
                a, b = prev[i], cur[i]
                key = (t, types[i])
                if b[ALIVE]:
                    st[t]["alive_rounds"] += 1
                    per[key]["alive_rounds"] += 1
                    st[t]["carried"] += b[GEMS]
                    if b[CHT]:
                        st[t]["chan_%d" % b[CHT]] += 1
                else:
                    st[t]["dead_rounds"] += 1
                if a[ALIVE] and not b[ALIVE]:
                    st[t]["deaths"] += 1
                    per[key]["deaths"] += 1
                    st[t]["gems_lost_on_death"] += a[GEMS]
                    per[key]["gems_lost"] += a[GEMS]
                if b[ALIVE] and a[ALIVE] and b[GEMS] > a[GEMS]:
                    st[t]["gems_gained"] += b[GEMS] - a[GEMS]
                    per[key]["gems_gained"] += b[GEMS] - a[GEMS]
                if b[EARNED] > a[EARNED]:
                    st[t]["gold_earned"] += b[EARNED] - a[EARNED]
                    per[key]["gold_earned"] += b[EARNED] - a[EARNED]
        prev = cur
    ev_kinds = collections.Counter()
    for e in js["ev"]:
        r, kind, actor = e[0], e[1], e[2]
        if actor < n:
            ev_kinds[(teams[actor], kind)] += 1
    locks = collections.Counter()
    wards = collections.Counter()
    for r in rs:
        for l in R[r]["locks"]:
            locks[l[0]] += 1
        for w in R[r]["wards"]:
            if w[0] in (0, 1):
                wards[w[0]] += 1
    milestones = {}
    for m in (100, 200, 300, 400, 500):
        if m in R:
            milestones[m] = R[m]["sub"]
    return dict(us=us, winner=winner, final=fin, final_round=js["final_round"], drafts=drafts, st=st, per=per,
                ev=ev_kinds, locks=locks, wards=wards, milestones=milestones)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--us", default="3,2,9,0,10")
    ap.add_argument("--detail", action="store_true")
    a = ap.parse_args()
    us_draft = [int(x) for x in a.us.split(",")]
    agg_us = collections.Counter()
    agg_op = collections.Counter()
    per_us = collections.defaultdict(collections.Counter)
    per_op = collections.defaultdict(collections.Counter)
    for path in sorted(glob.glob(os.path.join(a.dir, "*.json"))):
        name = os.path.basename(path)[:-5]
        js = load(path)
        res = analyse(js, us_draft)
        us = res["us"]
        d = [" ".join(DRAFT[x] for x in res["drafts"][t] if x >= 0) for t in (0, 1)]
        tag = "" if us is None else ("WIN " if res["winner"] == us else "LOSS")
        print(f"\n== {name}  R{res['final_round']}  sub {res['final'][0]}/{res['final'][1]}  {tag}  us=team{us}")
        for t in (0, 1):
            s = res["st"][t]
            ar = max(1, s["alive_rounds"])
            print(f"  T{t}{'*' if t == us else ' '} [{d[t]}] gained={s['gems_gained']} lost@death={s['gems_lost_on_death']} "
                  f"deaths={s['deaths']} dead_rounds={s['dead_rounds']} gold={s['gold_earned']} avg_carry={s['carried']/ar:.1f} "
                  f"recall_ch={s['chan_1']} sub_ch={s['chan_2']} ch3={s['chan_3']} locks={res['locks'][t]} wardrounds={res['wards'][t]} "
                  f"ev(atk={res['ev'][(t,5)]} cast={res['ev'][(t,6)]} recall={res['ev'][(t,11)]} submit={res['ev'][(t,12)]} buy={res['ev'][(t,13)]} cons={res['ev'][(t,20)]})")
        print("  milestones: " + "  ".join(f"R{m}:{v[0]}/{v[1]}" for m, v in res["milestones"].items()))
        if a.detail:
            for (t, ty), c in sorted(res["per"].items()):
                ar = max(1, c["alive_rounds"])
                print(f"    T{t} {NAMES[ty]:9} gems/100r={100*c['gems_gained']/ar:5.1f} deaths={c['deaths']} lost={c['gems_lost']} gold={c['gold_earned']}")
        if us is not None:
            op = 1 - us
            for k, v in res["st"][us].items():
                agg_us[k] += v
            for k, v in res["st"][op].items():
                agg_op[k] += v
            agg_us["games"] += 1
            agg_us["wins"] += res["winner"] == us
            for (t, ty), c in res["per"].items():
                tgt = per_us if t == us else per_op
                for k, v in c.items():
                    tgt[NAMES[ty]][k] += v
                tgt[NAMES[ty]]["games"] += 1
    if agg_us["games"]:
        print(f"\n### OUR GAMES: {agg_us['wins']}/{agg_us['games']} won")
        for k in ("gems_gained", "gems_lost_on_death", "deaths", "dead_rounds", "gold_earned", "chan_1", "chan_2"):
            print(f"  {k:20} us {agg_us[k]:7}  opp {agg_op[k]:7}")
        print("  per champion (gems per 100 alive rounds, deaths/game):")
        for side, P in (("us", per_us), ("opp", per_op)):
            for nm, c in sorted(P.items(), key=lambda x: -x[1]["games"]):
                ar = max(1, c["alive_rounds"])
                print(f"    {side:3} {nm:9} n={c['games']:2} gems/100r={100*c['gems_gained']/ar:5.1f} deaths/g={c['deaths']/c['games']:.2f} "
                      f"lost/g={c['gems_lost']/c['games']:.0f} gold/100r={100*c['gold_earned']/ar:.0f}")


if __name__ == "__main__":
    main()
