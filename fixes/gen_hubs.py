# usage: python3 gen_hubs.py COUNT LABELS OUTDIR
# query: a star with centre label 0 and LABELS leaves labelled 1..LABELS (no automorphism)
# data: hubs (label 0, not adjacent to each other), hub h with m_hl leaves of label l, chosen so that the number of
# embeddings, sum over hubs of prod_l m_hl, is exactly COUNT (one hub per non-zero digit of COUNT in base B)
import os, sys
count, L, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
B = 2
while B ** L <= count:
    B *= 2
hubs, x, i = [], count, 0
while x:
    if x % B:
        hubs.append([B] * i + [x % B] + [1] * (L - i - 1))
    x //= B
    i += 1
labels, edges = [], []
for ms in hubs:
    c = len(labels)
    labels.append(0)
    for l, m in enumerate(ms, 1):
        for _ in range(m):
            edges.append((c, len(labels)))
            labels.append(l)
def write(path, labels, edges):
    deg = [0] * len(labels)
    for u, v in edges:
        deg[u] += 1; deg[v] += 1
    with open(path, "w") as f:
        f.write("t %d %d\n" % (len(labels), len(edges)))
        f.writelines("v %d %d %d\n" % (i, l, deg[i]) for i, l in enumerate(labels))
        f.writelines("e %d %d\n" % e for e in edges)
os.makedirs(out, exist_ok=True)
write(os.path.join(out, "data.graph"), labels, edges)
write(os.path.join(out, "query.graph"), list(range(L + 1)), [(0, l) for l in range(1, L + 1)])
total = 0
for ms in hubs:
    p = 1
    for m in ms:
        p *= m
    total += p
assert total == count
print("exact count:", count, " hubs:", len(hubs), " data vertices:", len(labels))
