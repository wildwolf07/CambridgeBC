#!/usr/bin/env python3
"""Round-by-round trace around one champion: every champion within D tiles, with HP, gems and its actions.

usage: python3 tools/trace_focus.py DUMP.json R0 R1 FOCUS_INDEX [D=8]
Actions: m=move >=basic attack *=cast (slot) R=recall S=submit; target in brackets (champion name or 'camp').
"""
import json, sys
NAMES = ["ryt", "ves", "ven", "cas", "iss", "ste", "pri", "mar", "wil", "ren", "var", "tav"]
j = json.load(open(sys.argv[1]))
a, b, f = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
D = int(sys.argv[5]) if len(sys.argv) > 5 else 8
R = {int(k): v for k, v in j["rounds"].items()}
T, TM = j["types"], j["teams"]
n = len(T)
camps = {(c[2], c[3]) for c in (j["camps"] or [])}
ev = {}
for e in j["ev"]:
    if e[8] == 1 and e[1] in (4, 5, 6, 11, 12, 20):
        ev.setdefault((e[0], e[2]), []).append(e)


def cheb(p, q):
    return max(abs(p[0] - q[0]), abs(p[1] - q[1]))


for r in range(a, b + 1):
    if r not in R or len(R[r]["ch"]) != n:
        continue
    ch = R[r]["ch"]
    fp = (ch[f][1], ch[f][2])
    parts = []
    for i in range(n):
        c = ch[i]
        if not c[0]:
            if i == f:
                parts.append(f"{'AB'[TM[i]]}{NAMES[T[i]]}{i}[dead]")
            continue
        if cheb((c[1], c[2]), fp) > D and i != f:
            continue
        acts = []
        for e in ev.get((r, i), []):
            to = (e[5], e[6])
            tgt = ""
            if e[1] in (5, 6):
                tgt = next((NAMES[T[k]] + str(k) for k in range(n) if ch[k][0] and (ch[k][1], ch[k][2]) == to and k != i), "camp" if to in camps else f"{to}")
            sym = {4: "m", 5: ">", 6: f"*{e[7]}", 11: "R", 12: "S", 20: "C"}[e[1]]
            acts.append(f"{sym}{tgt}" if tgt else f"{sym}({e[5]},{e[6]})")
        parts.append(f"{'AB'[TM[i]]}{NAMES[T[i]]}{i}({c[1]},{c[2]}){c[3]}/{c[4]} s{c[13]} g{c[5]} {' '.join(acts)}")
    print(f"R{r}: " + " | ".join(parts))
