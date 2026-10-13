#!/usr/bin/env python3
"""Coordinate-descent tuning of config.h parameters on the economy benchmark (vs idle, all maps).

usage: python3 tools/tune.py PARAM=v1,v2,v3 [PARAM2=...] [--base-define K=V ...]
Prints the average win round for each value (lower is better).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import concurrent.futures as cf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from match import CAMBC, MAPS, ROOT, run_one  # noqa: E402


def build_variant(name, defines):
    args = ["python3", os.path.join(ROOT, "tools", "variant.py"), name]
    for d in defines:
        args += ["--define", d]
    subprocess.run(args, check=True, capture_output=True)
    os.makedirs(os.path.join(ROOT, "bots", name), exist_ok=True)
    p = subprocess.run([CAMBC, "build", name], cwd=ROOT, capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError(p.stdout[-2000:] + p.stderr[-2000:])


def econ(name, outdir, jobs):
    with cf.ThreadPoolExecutor(jobs) as ex:
        res = list(ex.map(lambda m: run_one(name, "idle", m, outdir), MAPS))
    rounds = [r["round"] if r["winner"] == "A" else 2000 for r in res]
    return sum(rounds) / len(rounds), rounds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("params", nargs="+")
    ap.add_argument("--base-define", action="append", default=[])
    ap.add_argument("--jobs", type=int, default=9)
    a = ap.parse_args()
    outdir = tempfile.mkdtemp(prefix="bct_")
    for spec in a.params:
        k, vals = spec.split("=")
        for v in vals.split(","):
            name = "t_" + re.sub(r"[^A-Za-z0-9]", "", k + v)[:30]
            build_variant(name, a.base_define + [f"{k}={v}"])
            avg, rounds = econ(name, outdir, a.jobs)
            print(f"{k}={v:8} avg={avg:6.1f}  {rounds}", flush=True)


if __name__ == "__main__":
    main()
