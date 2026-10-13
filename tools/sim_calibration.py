import json, glob, os, re, zipfile, collections, sys
S = sys.argv[1]
cheb = lambda a, b: max(abs(a[0]-b[0]), abs(a[1]-b[1]))
rows = []
for rep in sorted(glob.glob(S + '/cal/*.repl26a')):
    name = os.path.basename(rep)[:-8]
    js = json.load(open(f"{S}/calj/{name}.json"))
    dteam = 0 if name.startswith('dbgc') else 1
    z = zipfile.ZipFile(rep)
    out = z.read(f"team-{'ab'[dteam]}.stdout").decode(errors='ignore').replace('\x00', '')
    R = {int(k): v for k, v in js['rounds'].items()}
    T, TM = js['types'], js['teams']; n = len(T)
    for m in re.finditer(r"SIMD R(\d+) type(\d+) pos\((\d+),(\d+)\) v=(-?[\d.]+) m=(-?[\d.]+) fight=(\d) hp=(\d+)/(\d+) na=(\d+) ne=(\d+)", out):
        r, ty, x, y = int(m[1]), int(m[2]), int(m[3]), int(m[4])
        v, mg, fight = float(m[5]), float(m[6]), int(m[7])
        me = None
        for rr in (r, r + 1, r - 1):
            if rr in R and len(R[rr]['ch']) == n:
                for i in range(n):
                    c = R[rr]['ch'][i]
                    if TM[i] == dteam and T[i] == ty and c[0] and cheb((c[1], c[2]), (x, y)) <= 1: me = i
            if me is not None: break
        if me is None: continue
        died = 0; ekill = 0; adead = 0
        for q in range(r + 1, r + 13):
            if q not in R or q - 1 not in R or len(R[q]['ch']) != n: continue
            A, B = R[q - 1]['ch'], R[q]['ch']
            for i in range(n):
                if A[i][0] and not B[i][0] and cheb((A[i][1], A[i][2]), (x, y)) <= 9:
                    if i == me: died = 1
                    elif TM[i] == dteam: adead += 1
                    else: ekill += 1
        rows.append((v, mg, fight, died, ekill, adead, int(m[10]), int(m[11])))
print("decisions", len(rows))
def b(v):
    for lo in (-3, -2, -1, -0.5, 0, 0.5, 1, 2):
        if v < lo: return f"<{lo}"
    return ">=2"
agg = collections.defaultdict(lambda: [0, 0, 0, 0])
for v, mg, f, d, ek, ad, na, ne in rows:
    k = (b(v), f)
    agg[k][0] += 1; agg[k][1] += d; agg[k][2] += ek; agg[k][3] += ad
order = ["<-3", "<-2", "<-1", "<-0.5", "<0", "<0.5", "<1", "<2", ">=2"]
print(f"{'v bucket':9} {'fight':5} {'n':>5} {'P(I die)':>9} {'enemy deaths':>13} {'ally deaths':>12} {'net/decision':>13}")
for bk in order:
    for f in (0, 1):
        n_, d, ek, ad = agg[(bk, f)]
        if n_ == 0: continue
        print(f"{bk:9} {f:5} {n_:5} {d/n_:9.3f} {ek/n_:13.3f} {ad/n_:12.3f} {(ek-ad-d)/n_:13.3f}")
