#!/usr/bin/env python3
"""Aggregate per-match statistics from ladder_fetch.py metadata (no replays needed).

usage: python3 tools/ladder_stats.py TEAM [--kind ranked|unranked|all] [--since ISO] [--vs TEAM2]
Prints per-map record, game length, gems, K/D, drafts, item builds and consumables for TEAM and its opponents.
"""
import argparse, collections, json, sys
DRAFT = ["ryth", "vesper", "vendetta", "C&I", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
CH = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]
ITEMS = ["M_str", "M_STR*", "M_foc", "M_FOC*", "M_hp", "M_HP*", "P_str", "P_STR*", "P_foc", "P_FOC*", "P_hp", "P_HP*",
         "D_str", "D_STR*", "D_foc", "D_FOC*", "D_hp", "D_HP*", "T_str", "T_STR*", "T_foc", "T_FOC*", "T_hp", "T_HP*",
         "J_str", "J_STR*", "J_foc", "J_FOC*", "J_hp", "J_HP*", "H_str", "H_STR*", "H_foc", "H_FOC*", "H_hp", "H_HP*",
         "B_str", "B_STR*", "B_foc", "B_FOC*", "B_hp", "B_HP*"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("team")
    ap.add_argument("--kind", default="all")
    ap.add_argument("--since", default="")
    ap.add_argument("--until", default="9999")
    ap.add_argument("--vs", default=None)
    ap.add_argument("--dir", default="ladder_data")
    a = ap.parse_args()
    d = json.load(open(f"{a.dir}/{a.team.replace(' ', '_')}/groups.json"))
    tid = d["teamId"]
    per_map = collections.defaultdict(lambda: collections.Counter())
    side_stats = {"me": collections.Counter(), "opp": collections.Counter()}
    drafts = collections.defaultdict(collections.Counter)
    items = collections.defaultdict(collections.Counter)
    cons = collections.Counter()
    champ = collections.defaultdict(collections.Counter)
    ngroups = 0
    for x in d["groups"]:
        g = x["group"]
        if a.kind != "all" and g["kind"] != a.kind:
            continue
        if not (a.since <= g["createdAt"] < a.until):
            continue
        opp = [s for s in g["sides"] if s["teamId"] != tid][0]["teamName"]
        if a.vs and opp != a.vs:
            continue
        ngroups += 1
        for m in x["matches"]:
            st = m.get("statistics")
            if not st:
                continue
            sides = m["sides"]
            my_side = [s for s in sides if s["teamId"] == tid]
            if not my_side:
                continue
            my_letter = my_side[0]["side"]
            me = 0 if my_letter == "a" else 1
            won = (st["outcome"] == "teamA") == (me == 0)
            mp = m.get("map", "?")
            c = per_map[mp]
            c["n"] += 1
            c["w"] += won
            c["rounds"] += st["round"]
            c["my_gems"] += st["teams"][me]["submittedGems"]
            c["op_gems"] += st["teams"][1 - me]["submittedGems"]
            npk = len([p for p in st["teams"][me]["draftPicks"] if p >= 0])
            drafts[(mp, npk)][tuple(st["teams"][me]["draftPicks"])] += 1
            for ch in st["champions"]:
                key = "me" if ch["team"] == me else "opp"
                s = side_stats[key]
                s["k"] += ch.get("kills", 0)
                s["d"] += ch.get("deaths", 0)
                s["coll"] += ch.get("collectedGems", 0)
                s["gold"] += ch.get("earnedGold", 0)
                s["champs"] += 1
                if key == "me":
                    cc = champ[CH[ch["type"]]]
                    cc["n"] += 1
                    cc["k"] += ch.get("kills", 0)
                    cc["d"] += ch.get("deaths", 0)
                    cc["coll"] += ch.get("collectedGems", 0)
                    cc["gold"] += ch.get("earnedGold", 0)
                    cc["heal"] += ch.get("healingDone", 0)
                    for i, v in enumerate(ch.get("itemsBought", [])):
                        if v:
                            items[CH[ch["type"]]][ITEMS[i]] += v
                    for i, v in enumerate(ch.get("consumables", [])):
                        cons[i] += v
            side_stats["me"]["games"] += 1
            side_stats["me"]["rounds"] += st["round"]
            side_stats["me"]["wins"] += won
    m = side_stats["me"]
    o = side_stats["opp"]
    print(f"== {a.team} ({a.kind}, groups {ngroups}, games {m['games']})  win {m['wins']}/{m['games']} = {m['wins']/max(1,m['games']):.2f}  avg rounds {m['rounds']/max(1,m['games']):.0f}")
    print(f"   per game: kills {m['k']/max(1,m['games']):.2f} deaths {m['d']/max(1,m['games']):.2f} | opp kills {o['k']/max(1,m['games']):.2f} deaths {o['d']/max(1,m['games']):.2f}")
    print(f"   collected gems/game {m['coll']/max(1,m['games']):.0f} vs opp {o['coll']/max(1,m['games']):.0f}; gold/game {m['gold']/max(1,m['games']):.0f} vs {o['gold']/max(1,m['games']):.0f}")
    for mp, c in sorted(per_map.items()):
        print(f"   {mp:10} {c['w']:3}/{c['n']:<3} rounds {c['rounds']/c['n']:4.0f}  gems {c['my_gems']/c['n']:4.0f} - {c['op_gems']/c['n']:4.0f}")
    print("   drafts:")
    for (mp, k), cnt in sorted(drafts.items()):
        top = cnt.most_common(2)
        print(f"     {mp:10} {k}p: " + "; ".join(f"{'/'.join(DRAFT[p] for p in t if p >= 0)} x{v}" for t, v in top))
    print("   champions (per game): " + "  ".join(f"{k}: n{v['n']} k{v['k']/v['n']:.2f} d{v['d']/v['n']:.2f} gems{v['coll']/v['n']:.0f} gold{v['gold']/v['n']:.0f}" + (f" heal{v['heal']/v['n']:.0f}" if v['heal'] else "") for k, v in sorted(champ.items(), key=lambda x: -x[1]['n'])))
    print("   items (top per champion): ")
    for ch, c in sorted(items.items(), key=lambda x: -sum(x[1].values())):
        print(f"     {ch:9} " + " ".join(f"{k}:{v}" for k, v in c.most_common(7)))
    print(f"   consumables used (raw ids 0 TP,2 lock): {dict(cons)}")


if __name__ == "__main__":
    main()
