#!/usr/bin/env python3
"""Who creates contacts: for each champion of US_NAME, rounds where its nearest enemy goes from >4 to <=4 tiles;
credited to 'we stepped in' if our champion's own move reduced the distance to that enemy, else 'they came'.
Also the full-game share of alive rounds with an enemy within 4, and deaths within 10 rounds after a contact.
usage: US_NAME=orZers python3 tools/contact_origin.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    team = os.environ.get("US_NAME", "orZers")
    c = collections.Counter()
    ng = 0
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        ng += 1
        js = json.load(open(path))
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        camps = {(cc[2], cc[3]) for cc in (js["camps"] or [])}
        evs = collections.defaultdict(list)
        for e in js["ev"]:
            if e[8] == 1 and e[1] in (5, 6) and e[2] < n:
                evs[(e[0], e[2])].append(e)
        deaths = collections.defaultdict(list)
        for a, b in zip(rs, rs[1:]):
            for i in range(n):
                if R[a]["ch"][i][0] and not R[b]["ch"][i][0]:
                    deaths[i].append(b)
        for a, b in zip(rs, rs[1:]):
            if b != a + 1:
                continue
            ca, cb = R[a]["ch"], R[b]["ch"]
            for i in range(n):
                if TM[i] != us or not (ca[i][0] and cb[i][0]):
                    continue
                pa, pb = (ca[i][1], ca[i][2]), (cb[i][1], cb[i][2])
                ens = [j for j in range(n) if TM[j] != us and ca[j][0] and cb[j][0]]
                if not ens:
                    continue
                c["alive"] += 1
                db = min(cheb(pb, (cb[j][1], cb[j][2])) for j in ens)
                if db <= 4:
                    c["contact"] += 1
                da = min(cheb(pa, (ca[j][1], ca[j][2])) for j in ens)
                if da > 4 and db <= 4:
                    j = min(ens, key=lambda j: cheb(pb, (cb[j][1], cb[j][2])))
                    qa, qb = (ca[j][1], ca[j][2]), (cb[j][1], cb[j][2])
                    mine = cheb(pb, qa) < cheb(pa, qa)   # my own step closed distance to where it was
                    key = "we stepped in" if mine else "they came"
                    c[key] += 1
                    c[key + " died10"] += any(b < d <= b + 10 for d in deaths[i])
                    if mine:
                        # what did it do in the next 3 rounds: hit a champion / hit a camp / neither
                        hc = hk = 0
                        for rr in range(b, b + 3):
                            for e in evs.get((rr, i), []):
                                if rr in R and len(R[rr]["ch"]) == n and any(R[rr]["ch"][k][0] and TM[k] != us and (R[rr]["ch"][k][1], R[rr]["ch"][k][2]) == (e[5], e[6]) for k in range(n)):
                                    hc = 1
                                elif (e[5], e[6]) in camps:
                                    hk = 1
                        why = "then hit champion" if hc else ("then hit camp" if hk else "neither")
                        c[why] += 1
                        c[why + " died10"] += any(b < d <= b + 10 for d in deaths[i])
                        c["gems_carried"] += cb[i][5]
                        c["hpfrac"] += cb[i][3] / max(1, cb[i][4])
    print(f"{team}: {ng} games  contact share {c['contact']/max(1,c['alive']):.3f}")
    for k in ("we stepped in", "they came"):
        print(f"  {k:14} {c[k]/ng:5.1f}/game   died within 10: {c[k+' died10']/max(1,c[k]):.3f}")
    m = max(1, c["we stepped in"])
    print(f"  stepped in with HP frac {c['hpfrac']/m:.2f}, carrying {c['gems_carried']/m:.1f} gems")
    for k in ("then hit champion", "then hit camp", "neither"):
        print(f"    {k:18} {c[k]/ng:5.1f}/game   died within 10: {c[k+' died10']/max(1,c[k]):.3f}")


if __name__ == "__main__":
    main()
