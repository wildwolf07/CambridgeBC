"""First death, deaths per round window, and average earned gold/strength per champion at R50..R300.
usage: US_NAME=orZers python3 tools/death_timeline.py DIR"""
import json,glob,os,sys,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import ladder_labels
NAMES=["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
us_name=os.environ.get('US_NAME','orZers')
W=[0,50,100,150,200,300,400,600,2000]
dd=collections.defaultdict(lambda:[0,0]); fb=collections.Counter(); fbr=[]
gold=collections.defaultdict(lambda:[0,0,0]); strg=collections.defaultdict(lambda:[0,0,0])
ng=0
for p in sorted(glob.glob(sys.argv[1]+'/*.json')):
    nm=ladder_labels.names(p)
    if not nm: continue
    us=0 if nm[0]==us_name else (1 if nm[1]==us_name else None)
    if us is None: continue
    js=json.load(open(p)); R={int(k):v for k,v in js['rounds'].items()}; TM=js['teams']; n=len(TM)
    rs=sorted(r for r in R if len(R[r]['ch'])==n); ng+=1
    first=None
    for a,b in zip(rs,rs[1:]):
        for i in range(n):
            if R[a]['ch'][i][0] and not R[b]['ch'][i][0]:
                s=0 if TM[i]==us else 1
                for k in range(len(W)-1):
                    if W[k]<=b<W[k+1]: dd[k][s]+=1
                if first is None: first=(b,s)
    if first: fb[first[1]]+=1; fbr.append(first[0])
    for R0 in (50,100,150,200,300):
        rr=min(rs,key=lambda r:abs(r-R0))
        if abs(rr-R0)>5: continue
        for s in (0,1):
            idx=[i for i in range(n) if (TM[i]==us)==(s==0)]
            gold[R0][s]+=sum(R[rr]['ch'][i][7] for i in idx)/len(idx)
            strg[R0][s]+=sum(R[rr]['ch'][i][13] for i in idx)/len(idx)
        gold[R0][2]+=1; strg[R0][2]+=1
print(us_name,'games',ng,'first death suffered by us/them',fb[0],fb[1],'median first-death round',sorted(fbr)[len(fbr)//2] if fbr else None)
for k in range(len(W)-1): print(f'  deaths R{W[k]}-{W[k+1]}: us {dd[k][0]} them {dd[k][1]}')
for R0 in sorted(gold):
    g=gold[R0]; s=strg[R0]
    print(f'  R{R0}: avg earned gold per champ us {g[0]/g[2]:.0f} them {g[1]/g[2]:.0f} | avg str us {s[0]/s[2]:.0f} them {s[1]/s[2]:.0f}  (n={g[2]})')
