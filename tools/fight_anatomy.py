#!/usr/bin/env python3
"""Fight anatomy: why a team wins or loses champion fights.

Episodes are clustered like tools/fights.py (champion-vs-champion hits <=6 tiles, <=4 rounds apart). For each
episode and side: participants (hitters + victims), HP fraction / summed HP / summed strength at the start, casts
per participant-round, hit concentration (share of hits on the most-hit enemy), who struck first, and deaths.
Prints aggregates split by committed-attacker balance (us0/us>/us=/us<) and, for equal fights, by which side had
more HP / strength / the first strike.
usage: US_NAME=orZers [RMIN=0 RMAX=100] python3 tools/fight_anatomy.py DUMP_DIR [-v]
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
NAMES = ["ryt", "ves", "ven", "cas", "iss", "ste", "pri", "mar", "wil", "ren", "var", "tav"]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def episodes(js):
    R = {int(k): v for k, v in js["rounds"].items()}
    T, TM = js["types"], js["teams"]
    n = len(T)
    hits, casts = [], collections.Counter()
    for e in js["ev"]:
        r = e[0]
        if e[2] >= n or e[8] != 1 or r not in R or len(R[r]["ch"]) != n:
            continue
        if e[1] == 6:
            casts[(r, e[2])] += 1
        if e[1] in (5, 6):
            ch = R[r]["ch"]
            for i in range(n):
                if ch[i][0] and TM[i] != TM[e[2]] and (ch[i][1], ch[i][2]) == (e[5], e[6]):
                    hits.append((r, e[2], i, (e[5], e[6]), e[1]))
                    break
    hits.sort()
    eps = []
    for h in hits:
        for ep in eps:
            if h[0] - ep["last"] <= 4 and cheb(h[3], ep["pos"]) <= 6:
                ep["last"], ep["pos"] = h[0], h[3]
                ep["hits"].append(h)
                break
        else:
            eps.append({"first": h[0], "last": h[0], "pos": h[3], "start": h[3], "hits": [h]})
    return R, T, TM, n, eps, casts


def main():
    us_name = os.environ.get("US_NAME", "orZers")
    verbose = "-v" in sys.argv
    global RMIN, RMAX
    RMIN, RMAX = int(os.environ.get("RMIN", 0)), int(os.environ.get("RMAX", 99999))
    agg = collections.defaultdict(collections.Counter)
    eq = collections.defaultdict(collections.Counter)
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        names = ladder_labels.names(path)
        if not names:
            continue
        us = 0 if names[0] == us_name else (1 if names[1] == us_name else None)
        if us is None:
            continue
        js = json.load(open(path))
        R, T, TM, n, eps, casts = episodes(js)
        for ep in eps:
            r0, r1 = ep["first"], ep["last"]
            if not (RMIN <= r0 < RMAX):
                continue
            part = {0: set(), 1: set()}
            att = {0: set(), 1: set()}
            tgt = {0: collections.Counter(), 1: collections.Counter()}
            for h in ep["hits"]:
                part[TM[h[1]]].add(h[1])
                part[TM[h[2]]].add(h[2])
                att[TM[h[1]]].add(h[1])
                tgt[TM[h[1]]][h[2]] += 1
            first = TM[ep["hits"][0][1]]
            pre = max(r for r in R if r <= r0 - 1) if any(r <= r0 - 1 for r in R) else r0
            ch0 = R[pre]["ch"]
            st = {}
            for s in (0, 1):
                hp = sum(ch0[i][3] for i in part[s])
                mx = sum(ch0[i][4] for i in part[s]) or 1
                strn = sum(ch0[i][13] for i in part[s])
                nc = sum(casts[(r, i)] for i in part[s] for r in range(r0, r1 + 1))
                tot = sum(tgt[s].values())
                conc = (max(tgt[s].values()) / tot) if tot else 0
                st[s] = dict(hp=hp, frac=hp / mx, strn=strn, casts=nc, conc=conc, hits=tot)
            dead = {0: 0, 1: 0}
            for i in range(n):
                for r in range(r0, r1 + 3):
                    if r in R and r - 1 in R and len(R[r]["ch"]) == n and R[r - 1]["ch"][i][0] and not R[r]["ch"][i][0]:
                        p = (R[r - 1]["ch"][i][1], R[r - 1]["ch"][i][2])
                        if cheb(p, ep["pos"]) <= 8 or cheb(p, ep["start"]) <= 8:
                            dead[TM[i]] += 1
                        break
            ua, oa = len(att[us]), len(att[1 - us])
            key = "us0" if ua == 0 else "us>" if ua > oa else "us=" if ua == oa else "us<"
            c = agg[key]
            c["n"] += 1
            c["ud"] += dead[us]
            c["od"] += dead[1 - us]
            for s, lab in ((us, "u"), (1 - us, "o")):
                c[lab + "frac"] += st[s]["frac"]
                c[lab + "str"] += st[s]["strn"] / max(1, len(part[s]))
                c[lab + "casts"] += st[s]["casts"]
                c[lab + "conc"] += st[s]["conc"]
                c[lab + "hits"] += st[s]["hits"]
                c[lab + "part"] += len(part[s])
            c["first_us"] += first == us
            c["dur"] += r1 - r0 + 1
            if key == "us=":
                for cond, val in (("hp_adv", st[us]["frac"] - st[1 - us]["frac"] > 0.1),
                                  ("hp_dis", st[1 - us]["frac"] - st[us]["frac"] > 0.1),
                                  ("str_adv", st[us]["strn"] > st[1 - us]["strn"] * 1.1),
                                  ("str_dis", st[1 - us]["strn"] > st[us]["strn"] * 1.1),
                                  ("first_us", first == us), ("first_op", first != us)):
                    if val:
                        eq[cond]["n"] += 1
                        eq[cond]["ud"] += dead[us]
                        eq[cond]["od"] += dead[1 - us]
            if verbose and (dead[0] or dead[1]):
                pu = ",".join(NAMES[T[i]] for i in sorted(part[us]))
                po = ",".join(NAMES[T[i]] for i in sorted(part[1 - us]))
                print(f"{os.path.basename(path)[:8]} R{r0}-{r1} {key} us[{pu}] hp{st[us]['frac']:.2f} str{st[us]['strn']} "
                      f"casts{st[us]['casts']} | op[{po}] hp{st[1-us]['frac']:.2f} str{st[1-us]['strn']} casts{st[1-us]['casts']} "
                      f"first={'us' if first == us else 'op'} dead {dead[us]}/{dead[1-us]}")
    print(f"team {us_name}: per episode averages (u = {us_name}, o = opponent)")
    for k in ("us0", "us>", "us=", "us<"):
        c = agg[k]
        N = c["n"] or 1
        print(f"  {k} n={c['n']:4} deaths u/o={c['ud']}/{c['od']}  part {c['upart']/N:.2f}/{c['opart']/N:.2f}  "
              f"hp {c['ufrac']/N:.2f}/{c['ofrac']/N:.2f}  str/champ {c['ustr']/N:.0f}/{c['ostr']/N:.0f}  "
              f"hits {c['uhits']/N:.1f}/{c['ohits']/N:.1f}  casts {c['ucasts']/N:.2f}/{c['ocasts']/N:.2f}  "
              f"conc {c['uconc']/N:.2f}/{c['oconc']/N:.2f}  first_us {c['first_us']/N:.2f}  dur {c['dur']/N:.1f}")
    print("  equal fights split:")
    for k, c in eq.items():
        print(f"    {k:9} n={c['n']:4} deaths u/o={c['ud']}/{c['od']}")


if __name__ == "__main__":
    main()
