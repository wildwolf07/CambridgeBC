#!/usr/bin/env python3
"""Compare new challenge results of orZers against the v18 per-map baseline (games are deterministic, so each
(opponent, map) pairing is one exact data point).

usage: python3 tools/compare_baseline.py SINCE_ISO [--team orZers] [--refresh]
  SINCE_ISO: only groups created at/after this time (e.g. 2026-10-14T06:00) = games of the new upload.
  --refresh: re-download the team's match history first (tools/ladder_fetch.py history).
Prints per opponent/map: new result + gems vs the v18 baseline (research/analysis/v18_baseline_vs_top.txt source).
"""
import argparse, collections, json, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def v18_baseline():
    idx = json.load(open(os.path.join(ROOT, "ladder_data/replays/v18_vs_top/index.json")))
    base = {}
    for m in idx.values():
        if m["A"] != "orZers":
            continue
        base[(m["opp"], m["map"])] = (m["outcome"] == "teamA", m["gems"][0], m["gems"][1])
    return base


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("since")
    ap.add_argument("--team", default="orZers")
    ap.add_argument("--refresh", action="store_true")
    a = ap.parse_args()
    if a.refresh:
        subprocess.run([sys.executable, os.path.join(ROOT, "tools/ladder_fetch.py"), "history", a.team], cwd=ROOT, check=True)
    d = json.load(open(os.path.join(ROOT, "ladder_data", a.team.replace(" ", "_"), "groups.json")))
    tid = d["teamId"]
    new = {}
    for x in d["groups"]:
        g = x["group"]
        if g["createdAt"] < a.since:
            continue
        for m in x["matches"]:
            st = m.get("statistics")
            if not st:
                continue
            me = [s for s in m["sides"] if s["teamId"] == tid]
            op = [s for s in m["sides"] if s["teamId"] != tid]
            if not me or not op:
                continue
            k = 0 if me[0]["side"] == "a" else 1
            won = (st["outcome"] == "teamA") == (k == 0)
            new[(op[0]["teamName"], m["map"], me[0]["side"])] = (won, st["teams"][k]["submittedGems"], st["teams"][1 - k]["submittedGems"], g["kind"])
    base = v18_baseline()
    better = worse = same = 0
    by_opp = collections.defaultdict(list)
    for (opp, mp, side), (won, g1, g2, kind) in sorted(new.items()):
        b = base.get((opp, mp)) if side == "a" else None
        if b:
            delta = (g1 - g2) - (b[1] - b[2])
            tag = "BETTER" if delta > 0 else ("worse" if delta < 0 else "same")
            better += delta > 0
            worse += delta < 0
            same += delta == 0
            bs = f"v18 {'WIN ' if b[0] else 'loss'} {b[1]}-{b[2]}"
        else:
            tag, bs = "", "no baseline"
        by_opp[opp].append(f"  {mp:10} side {side} {kind[:4]}  {'WIN ' if won else 'loss'} {g1:4}-{g2:<4}   | {bs:22} {tag}")
    for opp, rows in sorted(by_opp.items()):
        print(opp)
        print("\n".join(rows))
    print(f"\nvs v18 baseline (gem margin): better {better}, worse {worse}, same {same}")


if __name__ == "__main__":
    main()
