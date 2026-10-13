#!/usr/bin/env python3
"""Re-run the bot's fight sim (sim_race, non-TEAMFIGHT path) on replay states and compare its verdict with what
happened. For each alive champion of US_NAME with an enemy within 4 (sampled every 3 rounds): allies within 7,
enemies within 8, HP/str from the dump; v = sim value; outcome = died within 10 rounds / enemy deaths within 10
rounds near it. Prints P(death) and kills per v bin. Useful to see if the sim is over-optimistic against a team.
usage: US_NAME=orZers python3 tools/sim_replay_calib.py DUMP_DIR
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ladder_labels  # noqa: E402
RESET = {0, 1, 2, 4, 7, 8, 10, 11}
SPELL = {0: 1.0, 3: 2.0, 5: 2.0, 6: 1.5, 7: 2.0, 9: 1.25, 11: 2.2}


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def dps(t, s, f):
    return s * (1.125 if t in RESET else 1.0) + SPELL.get(t, 0) * f / 8.0


def race(A, E):
    # A/E: lists of [hp, dps, val, me]
    a0 = sum(x[0] for x in A); e0 = sum(x[0] for x in E)
    def hit(side, dmg):
        while dmg > 0:
            alive = [x for x in side if x[0] > 0]
            if not alive:
                return
            t = min(alive, key=lambda x: x[0])
            d = min(dmg, t[0]); t[0] -= d; dmg -= d
    for _ in range(30):
        da = sum(x[1] for x in A if x[0] > 0); de = sum(x[1] for x in E if x[0] > 0)
        if da == 0 or de == 0:
            break
        hit(E, da); hit(A, de)
    v = sum(x[2] for x in E if x[0] <= 0) - sum(x[2] + (0.5 if x[3] else 0) for x in A if x[0] <= 0)
    a1 = sum(max(0, x[0]) for x in A); e1 = sum(max(0, x[0]) for x in E)
    return v + 0.5 * ((e0 - e1) / max(1, e0) - (a0 - a1) / max(1, a0))


def main():
    team = os.environ.get("US_NAME", "orZers")
    bins = collections.defaultdict(collections.Counter)
    lose = collections.Counter()
    lose_t = collections.Counter()
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.json"))):
        nm = ladder_labels.names(path)
        if not nm or team not in nm.values():
            continue
        us = 0 if nm[0] == team else 1
        js = json.load(open(path))
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        deaths = collections.defaultdict(list)
        for a, b in zip(rs, rs[1:]):
            for i in range(n):
                if R[a]["ch"][i][0] and not R[b]["ch"][i][0]:
                    deaths[i].append((b, (R[a]["ch"][i][1], R[a]["ch"][i][2])))
        for r in rs[::3]:
            ch = R[r]["ch"]
            for i in range(n):
                if TM[i] != us or not ch[i][0]:
                    continue
                p = (ch[i][1], ch[i][2])
                ens = [j for j in range(n) if TM[j] != us and ch[j][0] and cheb(p, (ch[j][1], ch[j][2])) <= 8]
                if not any(cheb(p, (ch[j][1], ch[j][2])) <= 4 for j in ens):
                    continue
                als = [j for j in range(n) if TM[j] == us and j != i and ch[j][0] and cheb(p, (ch[j][1], ch[j][2])) <= 7]
                val = lambda t: 2.0 if t in (3, 4) else 1.0
                A = [[ch[i][3], dps(T[i], ch[i][13], ch[i][14]), val(T[i]), True]] + [[ch[j][3], dps(T[j], ch[j][13], ch[j][14]), val(T[j]), False] for j in als]
                E = [[ch[j][3], dps(T[j], ch[j][13], ch[j][14]), val(T[j]), False] for j in ens]
                v = race(A, E)
                b = max(-3, min(3, round(v)))
                died = any(r < rd <= r + 10 for rd, _ in deaths[i])
                if v <= -1.5 and r + 1 in R and R[r + 1]["ch"][i][0]:
                    # what the champion did next: move away from / toward the nearest enemy, or stay
                    jn = min(ens, key=lambda j: cheb(p, (ch[j][1], ch[j][2])))
                    q = (R[r + 1]["ch"][i][1], R[r + 1]["ch"][i][2])
                    d0 = cheb(p, (ch[jn][1], ch[jn][2])); d1 = cheb(q, (ch[jn][1], ch[jn][2]))
                    act = "away" if d1 > d0 else ("toward" if d1 < d0 else ("stay" if q == p else "side"))
                    lose[act] += 1; lose[act + "_died"] += died
                    lose_t[(T[i], act)] += 1
                kills = sum(1 for j in ens for rd, pd in deaths[j] if r < rd <= r + 10)
                c = bins[b]
                c["n"] += 1; c["died"] += died; c["kills"] += kills
    print(f"{team}: sim value bin -> n, P(I die within 10), enemy deaths within 10 per sample")
    for b in sorted(bins):
        c = bins[b]
        print(f"  v~{b:+d}: n={c['n']:6}  P(die)={c['died']/c['n']:.3f}  kills/sample={c['kills']/c['n']:.3f}")
    tot = sum(lose[a] for a in ("away", "toward", "stay", "side"))
    print(f"  in losing contact (v<=-1.5), next step: " + "  ".join(
        f"{a} {lose[a]/max(1,tot):.2f} (P die {lose[a+'_died']/max(1,lose[a]):.2f})" for a in ("away", "toward", "stay", "side")))
    NM = ["ryt", "ves", "ven", "cas", "iss", "ste", "pri", "mar", "wil", "ren", "var", "tav"]
    for t in sorted({k[0] for k in lose_t}):
        tt = sum(lose_t[(t, a)] for a in ("away", "toward", "stay", "side"))
        print(f"    {NM[t]}: n={tt:5} " + " ".join(f"{a} {lose_t[(t, a)]/max(1,tt):.2f}" for a in ("away", "toward", "stay", "side")))


if __name__ == "__main__":
    main()
