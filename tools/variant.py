#!/usr/bin/env python3
"""Clone bots/beast into bots/NAME, optionally overriding the draft order.

usage: python3 tools/variant.py NAME [--draft vendetta,varg,...] [--src beast]
"""
import argparse
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALL = ["vendetta", "varg", "ryth", "marcella", "tavi", "stellar", "ren", "vesper_vale", "willow", "prism", "cassi_and_issac"]

ap = argparse.ArgumentParser()
ap.add_argument("name")
ap.add_argument("--draft", default=None)
ap.add_argument("--src", default="beast")
ap.add_argument("--define", action="append", default=[])
a = ap.parse_args()
src = os.path.join(ROOT, "bots", a.src, "src")
dst = os.path.join(ROOT, "bots", a.name, "src")
os.makedirs(dst, exist_ok=True)
for f in os.listdir(src):
    if f.endswith((".cpp", ".h")):
        shutil.copy(os.path.join(src, f), os.path.join(dst, f))
if a.draft:
    picks = a.draft.split(",")
    picks += [c for c in ALL if c not in picks]
    with open(os.path.join(dst, "draft_prefs.h"), "w") as fh:
        fh.write("#pragma once\n#define BEAST_DRAFT_PREFS " + ", ".join(picks) + "\n")
if a.define:
    cfg = os.path.join(dst, "config.h")
    body = open(cfg).read()
    pre = "".join(f"#define {d.split('=')[0]} {d.split('=')[1]}\n" for d in a.define)
    body = body.replace("#pragma once\n", "#pragma once\n" + pre, 1)
    open(cfg, "w").write(body)
print("created", a.name)
