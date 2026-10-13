#!/usr/bin/env python3
"""Summarise recent finished ladder matches: map, rounds, drafts, gems, K/D per team.

usage: python3 tools/scout.py [--team NAME] [--n 10]
"""
import argparse
import json
import subprocess

BASE = "https://2026a.battlecode.co.uk/api/v1"
NAMES = ["Ryth", "Vesper", "Vendetta", "C&I", "Stellar", "Prism", "Marcella", "Willow", "Ren", "Varg", "Tavi"]
CHAMP = ["ryth", "vesper", "vendetta", "cassi", "issac", "stellar", "prism", "marcella", "willow", "ren", "varg", "tavi"]


def get(path):
    out = subprocess.run(["curl", "-s", "--max-time", "20", BASE + path], capture_output=True, text=True).stdout
    return json.loads(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--team", default=None)
    ap.add_argument("--n", type=int, default=8)
    a = ap.parse_args()
    opts = get("/match-groups/options")
    maps = {}
    for m in opts.get("maps", []) if isinstance(opts, dict) else []:
        maps[m.get("id")] = m.get("name")
    q = "/match-groups?scope=all"
    if a.team:
        lad = get("/ladder")
        for t in lad["teams"] + lad.get("unrated", []):
            if t.get("name") == a.team:
                q += "&team=" + t["teamId"]
                print(f"{a.team}: rank {t.get('rank')} rating {round(t.get('rating', 0))} W{t.get('wins')}-L{t.get('losses')}")
    groups = get(q)["groups"]
    shown = 0
    for g in groups:
        if g["status"] != "finished":
            continue
        names = [s["teamName"] for s in g["sides"]]
        if a.team and a.team not in names:
            continue
        d = get(f"/match-groups/{g['id']}")
        print(f"== {names[0]} v{g['sides'][0]['version']} {g['sides'][0]['wins']} : {g['sides'][1]['wins']} "
              f"{names[1]} v{g['sides'][1]['version']}  ({g['kind']})")
        for m in d["matches"]:
            st = m.get("statistics") or {}
            if not st:
                continue
            teams = st["teams"]
            kd = [[0, 0], [0, 0]]
            for c in st["champions"]:
                kd[c["team"]][0] += c.get("kills", 0)
                kd[c["team"]][1] += c.get("deaths", 0)
            dr = ["/".join(NAMES[p] for p in t["draftPicks"]) for t in teams]
            mp = maps.get(m["mapId"], m["mapId"][:8])
            print(f"   {mp:10} r{st['round']:5} {st['outcome']:6} gems {teams[0]['submittedGems']:4}-{teams[1]['submittedGems']:<4} "
                  f"KD {kd[0][0]}/{kd[0][1]} vs {kd[1][0]}/{kd[1][1]}  [{dr[0]}] vs [{dr[1]}]")
        shown += 1
        if shown >= a.n:
            break


if __name__ == "__main__":
    main()
