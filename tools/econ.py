#!/usr/bin/env python3
"""Seeded economy benchmark: rounds to 1000 gems vs an idle opponent, averaged over seeded copies and all maps.

usage: python3 tools/econ.py BOT [BOT2 ...] [--seeds 3] [--jobs 4]
Single unseeded runs are chaotic (one turn's difference moves a map by +-50 rounds); seeds average that out.
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
    ap.add_argument("bots", nargs="+")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--maps", default=",".join(MAPS))
    a = ap.parse_args()
    maps = a.maps.split(",")
    allc = {}
    for b in a.bots:
        subprocess.run([os.path.join(ROOT, "tools", "seeds.sh"), b, str(a.seeds)], check=True, capture_output=True)
        allc[b] = [f"{b}_s{i}" for i in range(1, a.seeds + 1)]
        for c in allc[b]:
            p = subprocess.run([CAMBC, "build", c], cwd=ROOT, capture_output=True, text=True)
            if p.returncode != 0:
                print(c, p.stdout[-1500:], p.stderr[-1500:])
                sys.exit(1)
    subprocess.run([CAMBC, "build", "idle"], cwd=ROOT, capture_output=True)
    out = tempfile.mkdtemp(prefix="bec_")
    jobs = [(b, c, m) for b in a.bots for c in allc[b] for m in maps]
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda j: (j[0], run_one(j[1], "idle", j[2], out)), jobs))
    for b in a.bots:
        per = {m: [] for m in maps}
        crashes = 0
        for bb, r in res:
            if bb != b:
                continue
            rnd = r["round"] if r["winner"] == "A" else 2000
            per[r["map"]].append(rnd)
            crashes += r["rows"].get("A", {}).get("crashes", 0)
        avg = sum(sum(v) for v in per.values()) / max(1, sum(len(v) for v in per.values()))
        print(f"{b:14} avg {avg:6.1f}  " + " ".join(f"{m[:5]}:{sum(v)/len(v):.0f}" for m, v in per.items()) + f"  crashes {crashes}")


if __name__ == "__main__":
    main()
