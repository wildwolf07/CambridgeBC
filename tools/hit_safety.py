"""Per hit by a champion type (WHO=3 Cassi default) of team US_NAME on an enemy champion: distance to target,
nearest enemy melee, allies within 3, and whether the hitter died within 6 rounds. usage: WHO=3 US_NAME=orZers python3 tools/hit_safety.py DIR"""
# For every hit by a ranged champion of TEAM on an enemy champion: distance to target, distance to nearest enemy
# melee/any enemy, kind (basic/cast), what the shooter did on its next turn (closer/hold/away from nearest enemy),
# and whether the shooter died within 6 rounds. Also the state before the shot: allies within 3.
import json,glob,os,sys,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import ladder_labels
NAMES=["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
RANGED={1,3,5,6,8,11}; DASH={1,2,4,9}
def cheb(a,b): return max(abs(a[0]-b[0]),abs(a[1]-b[1]))
team=os.environ['US_NAME']; who=int(os.environ.get('WHO','3'))
agg=collections.defaultdict(collections.Counter); ng=0
for p in sorted(glob.glob(sys.argv[1]+'/*.json')):
    nm=ladder_labels.names(p)
    if not nm or team not in nm.values(): continue
    us=0 if nm[0]==team else 1
    js=json.load(open(p)); R={int(k):v for k,v in js['rounds'].items()}; T=js['types']; TM=js['teams']; n=len(T)
    me=[i for i in range(n) if TM[i]==us and T[i]==who]
    if not me: continue
    me=me[0]; ng+=1
    shots=[e for e in js['ev'] if e[2]==me and e[8]==1 and e[1] in (5,6)]
    last=-99
    for e in shots:
        r=e[0]
        if r not in R or r+1 not in R or len(R[r]['ch'])!=n: continue
        ch=R[r]['ch']
        tgt=[j for j in range(n) if TM[j]!=us and ch[j][0] and (ch[j][1],ch[j][2])==(e[5],e[6])]
        if not tgt: continue
        t=tgt[0]; pm=(e[3],e[4])
        dt=cheb(pm,(e[5],e[6]))
        ens=[j for j in range(n) if TM[j]!=us and ch[j][0]]
        dm=min([cheb(pm,(ch[j][1],ch[j][2])) for j in ens if T[j] not in RANGED] or [99])
        al=sum(1 for j in range(n) if TM[j]==us and j!=me and ch[j][0] and cheb(pm,(ch[j][1],ch[j][2]))<=3)
        died=any((r+k) in R and len(R[r+k]['ch'])==n and not R[r+k]['ch'][me][0] for k in range(1,7))
        newep = r-last>4; last=r
        key=f"d{dt}" 
        for k in ('all',key,f"melee<= {min(dm,5)}", f"allies3={min(al,2)}", 'kind5' if e[1]==5 else 'cast', 'first_shot' if newep else 'follow'):
            agg[k]['n']+=1; agg[k]['died']+=died
print(team,NAMES[who],'games',ng)
for k in sorted(agg): c=agg[k]; print(f"  {k:14} shots {c['n']:5} ({c['n']/ng:.1f}/g)  died<=6r {c['died']/c['n']:.3f}")
