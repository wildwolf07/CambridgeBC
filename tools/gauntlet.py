#!/usr/bin/env python3
"""Run a candidate bot against a pool of opponents on every map, both sides.

usage: python3 tools/gauntlet.py CANDIDATE [OPP1,OPP2,...] [--jobs 9]
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
    ap.add_argument("opps", nargs="?", default="starter-cpp,beast_v1,beast_v2")
    ap.add_argument("--jobs", type=int, default=9)
    ap.add_argument("--maps", default=",".join(MAPS))
    ap.add_argument("-v", action="store_true")
    args = ap.parse_args()
    opps = args.opps.split(",")
    maps = args.maps.split(",")
    for bot in [args.cand] + opps:
        p = subprocess.run([CAMBC, "build", bot], cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout[-3000:], p.stderr[-3000:])
            sys.exit(1)
    outdir = tempfile.mkdtemp(prefix="bcg_")
    jobs = []
    for o in opps:
        for m in maps:
            jobs.append((args.cand, o, m))
            jobs.append((o, args.cand, m))
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(lambda j: run_one(j[0], j[1], j[2], outdir), jobs))
    per = {o: [0, 0] for o in opps}
    gem_diff = {o: 0 for o in opps}
    kd = [0, 0]
    rounds = []
    for r in results:
        cand_side = "A" if r["a"] == args.cand else "B"
        opp = r["b"] if cand_side == "A" else r["a"]
        other = "B" if cand_side == "A" else "A"
        per[opp][1] += 1
        if r["winner"] == cand_side:
            per[opp][0] += 1
            rounds.append(r["round"])
        rc, ro = r["rows"].get(cand_side, {}), r["rows"].get(other, {})
        gem_diff[opp] += rc.get("gems", 0) - ro.get("gems", 0)
        kd[0] += rc.get("kills", 0)
        kd[1] += rc.get("deaths", 0)
        if args.v or r["winner"] != cand_side:
            print(f"  {r['map']:10} {r['a']:>12} vs {r['b']:<12} winner={r['winner']} rnd={r['round']} "
                  f"gems {r['rows'].get('A',{}).get('gems','?')}/{r['rows'].get('B',{}).get('gems','?')} "
                  f"kd {rc.get('kills','?')}/{rc.get('deaths','?')}")
    tot_w = sum(v[0] for v in per.values())
    tot_n = sum(v[1] for v in per.values())
    for o in opps:
        print(f"{args.cand} vs {o:14}: {per[o][0]:2}/{per[o][1]}  gemdiff {gem_diff[o]:+6}")
    avg_r = sum(rounds) / len(rounds) if rounds else 0
    print(f"TOTAL {tot_w}/{tot_n}  K/D {kd[0]}/{kd[1]}  avg win round {avg_r:.0f}")


if __name__ == "__main__":
    main()
