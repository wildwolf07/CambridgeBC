"""Cassi positioning per team: distance to spawn/Issac, enemy within 4, ally within 3, attack rate.
usage: US_NAME=Mite python3 tools/cassi_position.py DIR"""
import json,glob,os,sys,collections,statistics as S
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import ladder_labels
team=os.environ['US_NAME']
def cheb(a,b): return max(abs(a[0]-b[0]),abs(a[1]-b[1]))
st=collections.defaultdict(list); ng=0
for p in sorted(glob.glob(sys.argv[1]+'/*.json'))[:int(os.environ.get('MAXG','999'))]:
    nm=ladder_labels.names(p)
    if not nm or team not in nm.values(): continue
    us=0 if nm[0]==team else 1
    js=json.load(open(p)); T=js['types']; TM=js['teams']; n=len(T)
    ci=[i for i in range(n) if TM[i]==us and T[i]==3]
    ii=[i for i in range(n) if TM[i]==us and T[i]==4]
    if not ci: continue
    c,iss=ci[0],ii[0]; ng+=1
    R={int(k):v for k,v in js['rounds'].items()}; rs=sorted(r for r in R if len(R[r]['ch'])==n)
    spawn=(R[rs[0]]['ch'][c][1],R[rs[0]]['ch'][c][2])
    W=js.get('W') ; 
    hits=collections.Counter()
    for e in js['ev']:
        if e[2]==c and e[1] in (5,6) and e[8]==1: hits[e[0]]+=1
    for r in rs:
        ch=R[r]['ch']
        if not ch[c][0]: continue
        pc=(ch[c][1],ch[c][2]); pi=(ch[iss][1],ch[iss][2])
        st['d_spawn'].append(cheb(pc,spawn)); st['d_iss'].append(cheb(pc,pi))
        en=[cheb(pc,(ch[j][1],ch[j][2])) for j in range(n) if TM[j]!=us and ch[j][0]]
        st['enemy<=4'].append(1 if en and min(en)<=4 else 0)
        al=[cheb(pc,(ch[j][1],ch[j][2])) for j in range(n) if TM[j]==us and j!=c and ch[j][0]]
        st['ally<=3'].append(1 if al and min(al)<=3 else 0)
        st['act'].append(1 if hits[r] else 0)
        st['home<=6'].append(1 if cheb(pc,spawn)<=6 else 0)
print(f"{team}: games {ng}  Cassi median dist spawn {S.median(st['d_spawn'])}  frac within 6 of spawn {S.mean(st['home<=6']):.2f}  median dist Issac {S.median(st['d_iss'])}  frac enemy<=4 {S.mean(st['enemy<=4']):.3f}  frac ally<=3 {S.mean(st['ally<=3']):.2f}  attack rate {S.mean(st['act']):.2f}")
