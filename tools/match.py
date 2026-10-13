#!/usr/bin/env python3
"""Run bot A vs bot B on every map, both sides, in parallel; print a summary.

usage: python3 tools/match.py A B [--maps m1,m2] [--jobs 8] [--keep]
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAMBC = os.path.join(ROOT, ".venv", "bin", "cambc")
MAPS = sorted(f[:-7] for f in os.listdir(os.path.join(ROOT, "maps")) if f.endswith(".map26a"))

WIN_RE = re.compile(r"Team ([AB]) \((.+?)\) wins: (.+?) in round (\d+)")
ROW_RE = re.compile(r"^\s+([AB])\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(.*)$")


def run_one(a, b, m, outdir):
    cmd = [CAMBC, "run", a, b, "--map", m, "-o", outdir]
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    out = p.stdout + p.stderr
    res = {"map": m, "a": a, "b": b, "winner": None, "how": "", "round": 0, "rows": {}}
    for line in out.splitlines():
        w = WIN_RE.search(line)
        if w:
            res["winner"] = w.group(1)
            res["how"] = w.group(3)
            res["round"] = int(w.group(4))
        r = ROW_RE.match(line)
        if r:
            res["rows"][r.group(1)] = {
                "gems": int(r.group(3)), "kills": int(r.group(4)), "deaths": int(r.group(5)),
                "gold": int(r.group(6)), "crashes": int(r.group(7)), "draft": r.group(8).strip()}
    if res["winner"] is None:
        res["how"] = out[-400:]
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--maps", default=",".join(MAPS))
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--one-side", action="store_true")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    maps = args.maps.split(",")
    for bot in {args.a, args.b}:
        p = subprocess.run([CAMBC, "build", bot], cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout[-3000:], p.stderr[-3000:])
            sys.exit(1)
    outdir = args.out or tempfile.mkdtemp(prefix="bcm_")
    jobs = []
    for m in maps:
        jobs.append((args.a, args.b, m))
        if not args.one_side:
            jobs.append((args.b, args.a, m))
    results = []
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        futs = [ex.submit(run_one, a, b, m, outdir) for a, b, m in jobs]
        for f in futs:
            results.append(f.result())
    wins = {args.a: 0, args.b: 0}
    print(f"{'map':10} {'A':>12} {'B':>12} {'win':>12} {'rnd':>5}  gemsA/gemsB  kills A/B  deaths A/B  gold A/B")
    for r in results:
        wn = None
        if r["winner"] == "A":
            wn = r["a"]
        elif r["winner"] == "B":
            wn = r["b"]
        if wn:
            wins[wn] += 1
        ra, rb = r["rows"].get("A", {}), r["rows"].get("B", {})
        print(f"{r['map']:10} {r['a']:>12} {r['b']:>12} {str(wn):>12} {r['round']:>5}  "
              f"{ra.get('gems','?'):>4}/{rb.get('gems','?'):<4}   {ra.get('kills','?'):>3}/{rb.get('kills','?'):<3}"
              f"   {ra.get('deaths','?'):>3}/{rb.get('deaths','?'):<3}   {ra.get('gold','?'):>5}/{rb.get('gold','?'):<5}"
              f" crash {ra.get('crashes','?')}/{rb.get('crashes','?')}")
        if r["winner"] is None:
            print("   !!", r["how"].replace("\n", " | ")[-300:])
    print(f"TOTAL {args.a}: {wins[args.a]}  {args.b}: {wins[args.b]}  (of {len(results)})  replays: {outdir}")


if __name__ == "__main__":
    main()
