# usage: python3 gen_star.py HUBS LEAVES_PER_LABEL OUTDIR
# data: HUBS hubs (label 0), each adjacent to the same LEAVES_PER_LABEL leaves of each label 1..4
# query: a star with centre label 0 and leaves labelled 1, 2, 3, 4
# exact count (embeddings) = HUBS * LEAVES_PER_LABEL**4
import os, sys
h, n, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
labels = [0] * h + [l for l in range(1, 5) for _ in range(n)]
edges = [(a, b) for a in range(h) for b in range(h, h + 4 * n)]
def write(path, labels, edges):
    deg = [0] * len(labels)
    for u, v in edges:
        deg[u] += 1; deg[v] += 1
    with open(path, "w") as f:
        f.write("t %d %d\n" % (len(labels), len(edges)))
        f.writelines("v %d %d %d\n" % (i, l, deg[i]) for i, l in enumerate(labels))
        f.writelines("e %d %d\n" % e for e in edges)
write(os.path.join(out, "data.graph"), labels, edges)
write(os.path.join(out, "query.graph"), [0, 1, 2, 3, 4], [(0, 1), (0, 2), (0, 3), (0, 4)])
print("exact count:", h * n ** 4, " (2^64 =", 2 ** 64, ")")
