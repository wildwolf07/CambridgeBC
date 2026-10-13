#!/usr/bin/env python3
"""Download replay bundles selected from ladder_fetch.py metadata.

usage: python3 tools/ladder_download.py TEAM OUTDIR [--kind ranked] [--since ISO] [--until ISO] [--vs OPP] [--max N]
Writes OUTDIR/<match_id>.zip plus OUTDIR/index.json (match id -> map, sides, teams, result, round, kind, createdAt).
Newest groups first.
"""
import argparse, json, os, subprocess, concurrent.futures as cf


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("team")
    ap.add_argument("out")
    ap.add_argument("--kind", default="all")
    ap.add_argument("--since", default="")
    ap.add_argument("--until", default="9999")
    ap.add_argument("--vs", default=None)
    ap.add_argument("--max", type=int, default=10 ** 9)
    ap.add_argument("--dir", default="ladder_data")
    a = ap.parse_args()
    d = json.load(open(f"{a.dir}/{a.team.replace(' ', '_')}/groups.json"))
    tid = d["teamId"]
    os.makedirs(a.out, exist_ok=True)
    ipath = os.path.join(a.out, "index.json")
    index = json.load(open(ipath)) if os.path.exists(ipath) else {}
    groups = sorted(d["groups"], key=lambda x: x["group"]["createdAt"], reverse=True)
    todo = []
    for x in groups:
        g = x["group"]
        if a.kind != "all" and g["kind"] != a.kind:
            continue
        if not (a.since <= g["createdAt"] < a.until):
            continue
        opp = [s for s in g["sides"] if s["teamId"] != tid][0]["teamName"]
        if a.vs and opp != a.vs:
            continue
        for m in x["matches"]:
            if m.get("status") != "finished" or not m.get("replayUrl") or not m.get("statistics"):
                continue
            if len(todo) >= a.max:
                break
            sides = {s["side"]: s["teamName"] for s in m["sides"]}
            st = m["statistics"]
            index[m["id"]] = {"map": m.get("map"), "A": sides.get("a"), "B": sides.get("b"), "outcome": st["outcome"],
                              "round": st["round"], "gems": [t["submittedGems"] for t in st["teams"]], "kind": g["kind"],
                              "createdAt": g["createdAt"], "focus": a.team, "opp": opp}
            todo.append(m)
    def fetch(m):
        dst = os.path.join(a.out, f"{m['id']}.zip")
        if not os.path.exists(dst) or os.path.getsize(dst) == 0:
            subprocess.run(["curl", "-s", "--max-time", "120", "-o", dst, "https://2026a.battlecode.co.uk" + m["replayUrl"]])
        return os.path.getsize(dst) if os.path.exists(dst) else 0
    with cf.ThreadPoolExecutor(6) as ex:
        sizes = list(ex.map(fetch, todo))
    json.dump(index, open(ipath, "w"))
    print(f"{a.team}: {len(todo)} replays ({sum(1 for s in sizes if s > 0)} ok) -> {a.out}")


if __name__ == "__main__":
    main()
