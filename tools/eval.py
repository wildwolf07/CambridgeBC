#!/usr/bin/env python3
"""Evaluate a candidate with several seeded copies against an opponent pool (both sides, all maps).

usage: python3 tools/eval.py CAND [--seeds 3] [--opps starter-cpp,beast_v2,beast_v3] [--jobs 9]
"""
import argparse
import concurrent.futures as cf
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match import CAMBC, MAPS, ROOT, run_one  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cand")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--opps", default="starter-cpp,beast_v2,beast_v3")
    ap.add_argument("--jobs", type=int, default=9)
    ap.add_argument("--maps", default=",".join(MAPS))
    a = ap.parse_args()
    opps = a.opps.split(",")
    maps = a.maps.split(",")
    subprocess.run([os.path.join(ROOT, "tools", "seeds.sh"), a.cand, str(a.seeds)], check=True, capture_output=True)
    cands = [f"{a.cand}_s{i}" for i in range(1, a.seeds + 1)]
    for bot in cands + opps:
        p = subprocess.run([CAMBC, "build", bot], cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            print(bot, p.stdout[-2000:], p.stderr[-2000:])
            sys.exit(1)
    outdir = tempfile.mkdtemp(prefix="bce_")
    jobs = [(c, o, m) for c in cands for o in opps for m in maps]
    jobs += [(o, c, m) for c in cands for o in opps for m in maps]
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(lambda j: run_one(j[0], j[1], j[2], outdir), jobs))
    per = {o: [0, 0, 0] for o in opps}
    per_map = {m: [0, 0] for m in maps}
    kd = [0, 0]
    crash = [0]
    rounds = []
    for r in results:
        cside = "A" if r["a"] in cands else "B"
        oside = "B" if cside == "A" else "A"
        opp = r["b"] if cside == "A" else r["a"]
        won = r["winner"] == cside
        per[opp][1] += 1
        per_map[r["map"]][1] += 1
        if won:
            per[opp][0] += 1
            per_map[r["map"]][0] += 1
            rounds.append(r["round"])
        rc, ro = r["rows"].get(cside, {}), r["rows"].get(oside, {})
        per[opp][2] += rc.get("gems", 0) - ro.get("gems", 0)
        kd[0] += rc.get("kills", 0)
        kd[1] += rc.get("deaths", 0)
        crash[0] += rc.get("crashes", 0)
    for o in opps:
        w, n, gd = per[o]
        print(f"{a.cand} vs {o:14}: {w:3}/{n:<3} ({100*w/max(1,n):5.1f}%)  gemdiff/game {gd/max(1,n):+7.1f}")
    print("per map: " + "  ".join(f"{m}:{v[0]}/{v[1]}" for m, v in per_map.items()))
    tw = sum(v[0] for v in per.values())
    tn = sum(v[1] for v in per.values())
    print(f"TOTAL {tw}/{tn} ({100*tw/max(1,tn):.1f}%)  K/D {kd[0]}/{kd[1]}  avg win round {sum(rounds)/max(1,len(rounds)):.0f}  CRASHES {crash[0]}")


if __name__ == "__main__":
    main()
