#!/usr/bin/env python3
"""Per-(opponent, map) results of orZers games as side A in a time window (challenges are always side A; ladder games
are deterministic, so one game per pairing is exact while neither bot changes).
usage: python3 tools/live_table.py SINCE_ISO [UNTIL_ISO] [--teams "Mite,SHINK AI 6500,..."]
Compare two uploads: run once per window and pair cells with the same opponent and map.
"""
import argparse, collections, json, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = ['collision', 'corners', 'elkring', 'expanse', 'interlock', 'lanes', 'monolith', 'packs', 'rows']


def table(since, until="9999", teams=None):
    d = json.load(open(os.path.join(ROOT, 'ladder_data/orZers/groups.json')))
    tid = d['teamId']
    tab = collections.defaultdict(dict)
    for g in sorted(d['groups'], key=lambda x: x['group']['createdAt']):
        G = g['group']
        if not (since <= G['createdAt'] < until):
            continue
        me = [s for s in G['sides'] if s['teamId'] == tid][0]
        if me['side'] != 'a':
            continue
        opp = [s['teamName'] for s in G['sides'] if s['teamId'] != tid][0]
        if teams and opp not in teams:
            continue
        for m in g['matches']:
            st = m.get('statistics')
            if st:
                tab[opp][m['map']] = (st['outcome'] == 'teamA', st['teams'][0]['submittedGems'], st['teams'][1]['submittedGems'])
    return tab


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('since')
    ap.add_argument('until', nargs='?', default='9999')
    ap.add_argument('--teams', default=None)
    a = ap.parse_args()
    teams = a.teams.split(',') if a.teams else None
    tab = table(a.since, a.until, teams)
    print(f"{'opponent':28}" + ''.join(f"{m[:5]:>11}" for m in MAPS) + "   W/N")
    for opp in sorted(tab):
        row = tab[opp]
        print(f"{opp[:28]:28}" + ''.join(f"{(('W' if row[m][0] else 'l') + str(row[m][1]) + '-' + str(row[m][2])) if m in row else '-':>11}" for m in MAPS)
              + f"   {sum(v[0] for v in row.values())}/{len(row)}")


if __name__ == '__main__':
    main()
