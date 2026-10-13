import json,sys,collections
NAMES = ["ryt","ves","ven","cas","iss","ste","pri","mar","wil","ren","var","tav"]
j=json.load(open(sys.argv[1]))
R={int(k):v for k,v in j['rounds'].items()}
T=j['types']; TM=j['teams']; n=len(T)
camps=j['camps']
rs=sorted(r for r in R if len(R[r]['ch'])==n)
cheb=lambda a,b:max(abs(a[0]-b[0]),abs(a[1]-b[1]))
shown=0
for k in range(1,len(rs)):
    r0,r1=rs[k-1],rs[k]
    for ci,c in enumerate(camps):
        h0=R[r0]['camps'][ci][0]; h1=R[r1]['camps'][ci][0]
        if h0>0 and h1<=0:
            # find gem gains within next 12 rounds
            gains=[]
            for q in range(r1, r1+12):
                if q not in R or q-1 not in R: continue
                for i in range(n):
                    a=R[q-1]['ch'][i]; b=R[q]['ch'][i]
                    if a[0] and b[0] and b[5]>a[5]:
                        gains.append((q-r1, NAMES[T[i]]+'T%d'%TM[i], b[5]-a[5], cheb((b[1],b[2]),(c[2],c[3]))))
            near=[(NAMES[T[i]]+'T%d'%TM[i], cheb((R[r1]['ch'][i][1],R[r1]['ch'][i][2]),(c[2],c[3]))) for i in range(n) if R[r1]['ch'][i][0] and cheb((R[r1]['ch'][i][1],R[r1]['ch'][i][2]),(c[2],c[3]))<=5]
            print(f"R{r1} camp{ci} type{c[0]} grp{c[1]} @({c[2]},{c[3]}) near={near} gains={gains}")
            shown+=1
    if shown>25: break
