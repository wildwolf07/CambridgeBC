"""Strength, max HP, earned and unspent gold, deaths per champion type at R100/200/300/400 (team vs opponents).
usage: US_NAME=orZers python3 tools/econ_by_champion.py DIR"""
import json,glob,os,sys,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import ladder_labels
NAMES=["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
team=os.environ['US_NAME']
acc=collections.defaultdict(lambda: collections.defaultdict(list))
for p in sorted(glob.glob(sys.argv[1]+'/*.json')):
    nm=ladder_labels.names(p)
    if not nm or team not in nm.values(): continue
    us=0 if nm[0]==team else 1
    js=json.load(open(p)); R={int(k):v for k,v in js['rounds'].items()}; T=js['types']; TM=js['teams']; n=len(T)
    rs=sorted(r for r in R if len(R[r]['ch'])==n)
    for R0 in (100,200,300,400):
        cand=[r for r in rs if abs(r-R0)<=3]
        if not cand: continue
        r=cand[0]
        for i in range(n):
            c=R[r]['ch'][i]; side='us' if TM[i]==us else 'op'
            acc[(side,NAMES[T[i]])][R0].append((c[13],c[4],c[7],c[6],c[9]))
for k in sorted(acc):
    s=f"{k[0]} {k[1]}:"
    for R0 in (100,200,300,400):
        v=acc[k][R0]
        if v: s+=f"  R{R0} str{sum(x[0] for x in v)/len(v):4.0f} maxhp{sum(x[1] for x in v)/len(v):5.0f} earned{sum(x[2] for x in v)/len(v):4.0f} unspent{sum(x[3] for x in v)/len(v):4.0f} deaths{sum(x[4] for x in v)/len(v):.1f}"
    print(s)
