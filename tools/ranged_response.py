#!/usr/bin/env python3
"""What a team's ranged champion (WHO, default Cassi=3) does on turns that start with an acting enemy melee within
2 tiles (or adjacent): step away / shoot / step closer / hold, and how often it dies within 6 rounds.
usage: WHO=3 US_NAME=KratosMessi python3 tools/ranged_response.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
RANGED = {1, 3, 5, 6, 8, 11}


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    team = os.environ.get("US_NAME", "orZers")
    who = int(os.environ.get("WHO", 3))
    acc = collections.defaultdict(collections.Counter)
    ng = 0
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        js = json.load(open(path))
        T, TM = js["types"], js["teams"]
        n = len(T)
        me = [i for i in range(n) if TM[i] == us and T[i] == who]
        if not me:
            continue
        me = me[0]
        ng += 1
        R = {int(k): v for k, v in js["rounds"].items()}
        acts = collections.defaultdict(list)
        for e in js["ev"]:
            if e[2] == me and e[8] == 1 and e[1] in (4, 5, 6):
                acts[e[0]].append(e)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        for k in range(1, len(rs) - 1):
            r0, r = rs[k - 1], rs[k]
            if r != r0 + 1:
                continue
            ch0, ch1 = R[r0]["ch"], R[r]["ch"]
            if not (ch0[me][0] and ch1[me][0]):
                continue
            p0 = (ch0[me][1], ch0[me][2])
            mel = [j for j in range(n) if TM[j] != us and ch0[j][0] and T[j] not in RANGED and cheb(p0, (ch0[j][1], ch0[j][2])) <= 2]
            if not mel:
                continue
            dmin = min(cheb(p0, (ch0[j][1], ch0[j][2])) for j in mel)
            key = f"melee at {dmin}"
            p1 = (ch1[me][1], ch1[me][2])
            shot = any(e[1] == 5 for e in acts[r])
            cast = any(e[1] == 6 for e in acts[r])
            d1 = min(cheb(p1, (ch0[j][1], ch0[j][2])) for j in mel)
            if p1 != p0:
                a = "step away" if d1 > dmin else ("step closer" if d1 < dmin else "step side")
            elif shot:
                a = "shoot"
            elif cast:
                a = "cast only"
            else:
                a = "hold"
            died = any(rs[k + q] in R and not R[rs[k + q]]["ch"][me][0] for q in range(1, 7) if k + q < len(rs))
            for kk in (key, "any"):
                acc[kk][a] += 1
                acc[kk]["n"] += 1
                acc[kk]["died"] += died
    print(f"{team} type {who}: {ng} games")
    for kk in sorted(acc):
        c = acc[kk]
        N = c["n"]
        print(f"  {kk:10} n={N:5} ({N/max(1,ng):.1f}/g) died<=6r {c['died']/N:.2f}  " +
              "  ".join(f"{a} {c[a]/N:.2f}" for a in ("step away", "shoot", "cast only", "hold", "step side", "step closer")))


if __name__ == "__main__":
    main()
