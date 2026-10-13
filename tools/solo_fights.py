"""Fight episodes split into group vs solo (one participant of ours) by champion and by who struck first.
usage: US_NAME=orZers python3 tools/solo_fights.py DIR"""
import json,glob,os,sys,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import ladder_labels
from fight_anatomy import episodes, cheb
NAMES=["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
team=os.environ['US_NAME']
agg=collections.defaultdict(collections.Counter); ng=0
for p in sorted(glob.glob(sys.argv[1]+'/*.json')):
    nm=ladder_labels.names(p)
    if not nm or team not in nm.values(): continue
    us=0 if nm[0]==team else 1; ng+=1
    js=json.load(open(p)); R,T,TM,n,eps,casts=episodes(js)
    for ep in eps:
        r0,r1=ep['first'],ep['last']
        part={0:set(),1:set()}
        for h in ep['hits']: part[TM[h[1]]].add(h[1]); part[TM[h[2]]].add(h[2])
        first=TM[ep['hits'][0][1]]
        dead={0:0,1:0}
        for i in range(n):
            for r in range(r0,r1+3):
                if r in R and r-1 in R and len(R[r]['ch'])==n and R[r-1]['ch'][i][0] and not R[r]['ch'][i][0]:
                    p_=(R[r-1]['ch'][i][1],R[r-1]['ch'][i][2])
                    if cheb(p_,ep['pos'])<=8 or cheb(p_,ep['start'])<=8: dead[TM[i]]+=1
                    break
        nu=len(part[us]); no=len(part[1-us])
        solo = nu==1
        key=('solo_' + NAMES[T[list(part[us])[0]]]) if solo else 'group'
        k2=key+('_init' if first==us else '_jumped')
        for k in (key,k2,'all'):
            agg[k]['n']+=1; agg[k]['ud']+=dead[us]; agg[k]['od']+=dead[1-us]
print(team,'games',ng)
for k in sorted(agg,key=lambda k:-agg[k]['ud']):
    c=agg[k]; print(f"  {k:22} n={c['n']:4} deaths us/them {c['ud']:4}/{c['od']:4}  per game {c['ud']/ng:.2f}/{c['od']/ng:.2f}")
