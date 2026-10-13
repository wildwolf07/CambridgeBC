import json,sys
NAMES = ["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
j=json.load(open(sys.argv[1])); a,b=int(sys.argv[2]),int(sys.argv[3])
R={int(k):v for k,v in j['rounds'].items()}
T=j['types']; TM=j['teams']
ev={}
for e in j['ev']:
    if e[1] in (5,6,11,12) and e[8]==1: ev.setdefault(e[0],[]).append(e)
for r in range(a,b+1):
    if r not in R: continue
    s=' '.join(f"{'AB'[TM[i]]}{NAMES[T[i]]}({c[1]},{c[2]}){c[3]}g{c[5]}" if c[0] else f"{'AB'[TM[i]]}{NAMES[T[i]]}[dead]" for i,c in enumerate(R[r]['ch']))
    evs=' '.join(f"{NAMES[T[e[2]]]}{'>' if e[1]==5 else '*' if e[1]==6 else 'R' if e[1]==11 else 'S'}({e[5]},{e[6]})" for e in ev.get(r,[]))
    print(f"R{r} sub={R[r]['sub']} | {s}\n      {evs}")
