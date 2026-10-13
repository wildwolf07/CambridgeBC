#!/usr/bin/env python3
"""Fetch ladder standings, a team's full match history (with per-match statistics) and replay bundles.

usage:
  python3 tools/ladder_fetch.py standings
  python3 tools/ladder_fetch.py history TEAM_NAME [--kind ranked] [--out ladder_data]      # metadata only
  python3 tools/ladder_fetch.py replays TEAM_NAME [--kind ranked] [--max N] [--out ladder_data]
Output: OUT/<team>/groups.json (all groups + matches + statistics); OUT/<team>/replays/<match_id>.zip
API notes: GET /api/v1/match-groups?scope=all&team=ID[&kind=ranked][&before=<createdAt>_<id>] pages by 25;
GET /api/v1/match-groups/ID -> matches[] with statistics and replayUrl (/api/v1/matches/ID/bundle).
"""
import argparse
import json
import os
import subprocess
import time

BASE = "https://2026a.battlecode.co.uk/api/v1"


def get(path, params=None):
    args = ["curl", "-s", "--max-time", "60", "-G", BASE + path]
    for k, v in (params or {}).items():
        args += ["--data-urlencode", f"{k}={v}"]
    for attempt in range(4):
        out = subprocess.run(args, capture_output=True, text=True).stdout
        try:
            return json.loads(out)
        except json.JSONDecodeError:
            time.sleep(2 ** attempt)
    raise RuntimeError(f"bad response for {path} {params}: {out[:200]}")


def standings():
    return get("/ladder")


def team_id(name):
    lad = standings()
    for t in lad["teams"] + lad.get("unrated", []):
        if t.get("name") == name:
            return t["teamId"], t
    raise SystemExit(f"team {name} not found")


def history(tid, kind=None):
    groups, before = [], None
    while True:
        p = {"scope": "all", "team": tid}
        if kind:
            p["kind"] = kind
        if before:
            p["before"] = before
        d = get("/match-groups", p)
        g = d.get("groups", [])
        groups += g
        if not d.get("more") or not g:
            break
        before = f"{g[-1]['createdAt']}_{g[-1]['id']}"
        time.sleep(0.2)
    return groups


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["standings", "history", "replays"])
    ap.add_argument("team", nargs="?")
    ap.add_argument("--kind", default=None)
    ap.add_argument("--max", type=int, default=10 ** 9)
    ap.add_argument("--out", default="ladder_data")
    a = ap.parse_args()
    if a.cmd == "standings":
        lad = standings()
        for t in lad["teams"]:
            print(f"{t['rank']:3} {t['name']:24} {round(t['rating']):5} W{t['wins']:3} L{t['losses']:3}")
        return
    tid, info = team_id(a.team)
    tdir = os.path.join(a.out, a.team.replace(" ", "_"))
    os.makedirs(tdir, exist_ok=True)
    gpath = os.path.join(tdir, "groups.json")
    cache = {}
    if os.path.exists(gpath):
        for g in json.load(open(gpath)).get("groups", []):
            cache[g["group"]["id"]] = g
    groups = history(tid, a.kind)
    full = []
    for g in groups:
        if g["status"] != "finished":
            continue
        if g["id"] in cache:
            full.append(cache[g["id"]])
            continue
        d = get(f"/match-groups/{g['id']}")
        full.append({"group": g, "matches": d.get("matches", [])})
        time.sleep(0.1)
    json.dump({"team": a.team, "teamId": tid, "info": info, "groups": full}, open(gpath, "w"))
    print(f"{a.team}: {len(full)} finished groups, {sum(len(x['matches']) for x in full)} matches -> {gpath}")
    if a.cmd == "replays":
        rdir = os.path.join(tdir, "replays")
        os.makedirs(rdir, exist_ok=True)
        n = 0
        for x in full:
            for m in x["matches"]:
                if n >= a.max:
                    break
                if m.get("status") != "finished" or not m.get("replayUrl"):
                    continue
                dst = os.path.join(rdir, f"{m['id']}.zip")
                if not os.path.exists(dst) or os.path.getsize(dst) == 0:
                    subprocess.run(["curl", "-s", "--max-time", "120", "-o", dst, "https://2026a.battlecode.co.uk" + m["replayUrl"]])
                n += 1
        print(f"replays in {rdir}: {len(os.listdir(rdir))}")


if __name__ == "__main__":
    main()
