#!/usr/bin/env python3
"""Extract concrete decision rules of each team from tools/replay_dump.mjs JSON.

Per team label: recall HP, tribute size, reaction to enemy contact (approach/retreat) split by local numbers and HP,
low-HP behaviour, chasing of weak enemies, grouping, time in enemy half, C&I spacing.
usage: python3 tools/policy.py DUMP_DIR
Team labels come from file names (c3=:3, et=Emperor Tung, cab=Cabbage, whim=WHIM, kr=KratosMessi, miru, larp, sssa,
nameless_vs_colon3 = Nameless(T0) vs :3(T1)); our bot is detected by its draft 3,2,9,0,10 prefix.
"""
import collections, glob, json, os, sys
ALIVE, X, Y, HP, MAXHP, GEMS = range(6)
CHT = 11


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def labels(name, js):
    us_draft = [3, 2, 9, 0, 10]
    d = [[x for x in js["drafts"][t] if x >= 0] for t in (0, 1)]
    ours = [t for t in (0, 1) if d[t] == us_draft[:len(d[t])]]
    if name.startswith("nameless_vs_colon3"):
        return {0: "Nameless", 1: ":3"}
    if name.startswith("et_vs_il"):
        return {0: "EmperorTung", 1: "il"}
    if len(ours) == 2:  # mirror draft (miru): we never place locks/wards
        lw = [sum(1 for r in js["rounds"].values() for l in r["locks"] + r["wards"] if l[0] == t) for t in (0, 1)]
        ours = [0 if lw[0] < lw[1] else 1]
    if name == "et3":
        return {1: "EmperorTung", 0: "other"}
    opp = {"c3": ":3", "c3b": ":3", "et2": "EmperorTung", "et4": "EmperorTung", "et": "EmperorTung", "cab": "Cabbage", "whim": "WHIM",
           "kr": "KratosMessi", "miru": "MirukuDragon", "larp": "larp", "sssa": "SSSA"}.get(name.split("_")[0], "opp")
    if not ours:
        return {0: "?", 1: "?"}
    u = ours[0]
    return {u: "US(old)", 1 - u: opp}


def main():
    d = sys.argv[1]
    S = collections.defaultdict(lambda: collections.defaultdict(list))
    for path in sorted(glob.glob(os.path.join(d, "*.json"))):
        name = os.path.basename(path)[:-5]
        js = json.load(open(path))
        lab = labels(name, js)
        R = {int(k): v for k, v in js["rounds"].items()}
        T, TM = js["types"], js["teams"]
        n = len(T)
        rs = sorted(r for r in R if len(R[r]["ch"]) == n)
        first = R[rs[0]]["ch"]
        spawn = {t: (sum(first[i][X] for i in range(n) if TM[i] == t) / max(1, sum(1 for i in range(n) if TM[i] == t)),
                     sum(first[i][Y] for i in range(n) if TM[i] == t) / max(1, sum(1 for i in range(n) if TM[i] == t))) for t in (0, 1)}
        for e in js["ev"]:
            if e[2] >= n or e[8] != 1 or e[0] not in R or len(R[e[0]]["ch"]) != n:
                continue
            c = R[e[0]]["ch"][e[2]]
            L = lab.get(TM[e[2]], "?")
            if e[1] == 11:
                S[L]["recall_hp"].append(c[HP] / max(1, c[MAXHP]))
            if e[1] == 12:
                S[L]["tribute_gems"].append(c[GEMS])
        idx = {r: k for k, r in enumerate(rs)}
        for k in range(1, len(rs) - 3):
            r = rs[k]
            ch, prev, nx = R[r]["ch"], R[rs[k - 1]]["ch"], R[rs[k + 3]]["ch"]
            for i in range(n):
                if not ch[i][ALIVE] or ch[i][CHT]:
                    continue
                L = lab.get(TM[i], "?")
                p = (ch[i][X], ch[i][Y])
                hpf = ch[i][HP] / max(1, ch[i][MAXHP])
                S[L]["n"].append(1)
                allies5 = sum(1 for j in range(n) if j != i and TM[j] == TM[i] and ch[j][ALIVE] and cheb((ch[j][X], ch[j][Y]), p) <= 5
                              and not (T[i] in (3, 4) and T[j] in (3, 4)))
                S[L]["allies5"].append(allies5)
                S[L]["enemy_half"].append(1 if cheb(p, spawn[1 - TM[i]]) < cheb(p, spawn[TM[i]]) else 0)
                S[L]["carry"].append(ch[i][GEMS])
                ens = [j for j in range(n) if TM[j] != TM[i] and ch[j][ALIVE]]
                if not ens:
                    continue
                dnow = min(cheb(p, (ch[j][X], ch[j][Y])) for j in ens)
                pens = [j for j in range(n) if TM[j] != TM[i] and prev[j][ALIVE]]
                dprev = min([cheb((prev[i][X], prev[i][Y]), (prev[j][X], prev[j][Y])) for j in pens] or [99]) if prev[i][ALIVE] else 99
                q = (nx[i][X], nx[i][Y]) if nx[i][ALIVE] else None
                # contact onset: an enemy just came within 5
                if dnow <= 5 and dprev > 5 and q is not None:
                    jn = min(ens, key=lambda j: cheb(p, (ch[j][X], ch[j][Y])))
                    d3 = cheb(q, (nx[jn][X], nx[jn][Y])) if nx[jn][ALIVE] else 99
                    a5 = 1 + allies5
                    e5 = sum(1 for j in ens if cheb(p, (ch[j][X], ch[j][Y])) <= 5)
                    num = "up" if a5 > e5 else ("even" if a5 == e5 else "down")
                    hb = "hp>60" if hpf > 0.6 else "hp<=60"
                    act = "approach" if d3 < dnow else ("retreat" if d3 > dnow else "hold")
                    S[L][f"contact_{num}_{hb}"].append(act)
                    S[L]["contact_all"].append(act)
                # chase: weak enemy (<30% hp) within 4
                for j in ens:
                    if ch[j][HP] < 0.3 * ch[j][MAXHP] and cheb(p, (ch[j][X], ch[j][Y])) <= 4 and q is not None and nx[j][ALIVE]:
                        d0 = cheb(p, (ch[j][X], ch[j][Y]))
                        d3 = cheb(q, (nx[j][X], nx[j][Y]))
                        S[L]["chase_weak"].append("approach" if d3 < d0 else ("retreat" if d3 > d0 else "hold"))
                        break
                # low hp behaviour: < 35% hp, moving toward own spawn over next 3 rounds?
                if hpf < 0.35 and q is not None:
                    S[L]["lowhp_home"].append(1 if cheb(q, spawn[TM[i]]) < cheb(p, spawn[TM[i]]) else 0)
                    S[L]["lowhp_fight"].append(1 if dnow <= 3 else 0)
        # C&I spacing
        for t in (0, 1):
            L = lab.get(t, "?")
            ci = [i for i in range(n) if TM[i] == t and T[i] in (3, 4)]
            if len(ci) == 2:
                for r in rs:
                    a, b = R[r]["ch"][ci[0]], R[r]["ch"][ci[1]]
                    if a[ALIVE] and b[ALIVE]:
                        S[L]["ci_dist"].append(cheb((a[X], a[Y]), (b[X], b[Y])))

    def med(v):
        v = sorted(v)
        return v[len(v) // 2] if v else float("nan")

    def frac(v, x):
        return sum(1 for a in v if a == x) / max(1, len(v))
    order = sorted(S, key=lambda L: (L != "US(old)", L))
    for L in order:
        s = S[L]
        if len(s["n"]) < 500:
            continue
        print(f"\n=== {L}  (champion-rounds {len(s['n'])})")
        print(f"  recall HP median {med(s['recall_hp']):.2f} (n={len(s['recall_hp'])}) | tribute gems median {med(s['tribute_gems'])} mean {sum(s['tribute_gems'])/max(1,len(s['tribute_gems'])):.0f} (n={len(s['tribute_gems'])})"
              f" | carried mean {sum(s['carry'])/len(s['carry']):.1f}")
        print(f"  allies within 5 mean {sum(s['allies5'])/len(s['allies5']):.2f} | time in enemy half {sum(s['enemy_half'])/len(s['enemy_half']):.2f}"
              f" | C&I dist median {med(s['ci_dist'])}")
        print(f"  low HP (<35%): heading home {sum(s['lowhp_home'])/max(1,len(s['lowhp_home'])):.2f}, enemy within 3 {sum(s['lowhp_fight'])/max(1,len(s['lowhp_fight'])):.2f}"
              f" | chase weak enemy: approach {frac(s['chase_weak'],'approach'):.2f} retreat {frac(s['chase_weak'],'retreat'):.2f} (n={len(s['chase_weak'])})")
        line = "  contact reaction (approach/retreat):"
        for key in ("contact_all", "contact_up_hp>60", "contact_even_hp>60", "contact_down_hp>60", "contact_up_hp<=60", "contact_even_hp<=60", "contact_down_hp<=60"):
            v = s[key]
            if v:
                line += f"  {key[8:]}={frac(v,'approach'):.2f}/{frac(v,'retreat'):.2f}(n={len(v)})"
        print(line)


if __name__ == "__main__":
    main()
