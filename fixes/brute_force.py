"""Count embeddings (labeled, injective, non-induced) of a small case by plain backtracking.
usage: python3 brute_force.py data.graph query.graph [--aut]
  --aut  also print the number of automorphisms of the query and embeddings / automorphisms (the count that
         Circinus prints with --break_symmetry=true)"""
import sys


def rd(p):
    lab, adj = {}, {}
    for l in open(p):
        t = l.split()
        if not t:
            continue
        if t[0] == 'v':
            lab[int(t[1])] = int(t[2]); adj.setdefault(int(t[1]), set())
        elif t[0] == 'e':
            u, v = int(t[1]), int(t[2]); adj.setdefault(u, set()).add(v); adj.setdefault(v, set()).add(u)
    return lab, adj


def count(gl, ga, ql, qa, exact_degree=False):
    n = len(ql)
    order = [max(ql, key=lambda u: len(qa[u]))]
    while len(order) < n:
        order.append(max((u for u in ql if u not in order and qa[u] & set(order)), key=lambda u: len(qa[u])))
    m, used, cnt = {}, set(), [0]

    def rec(i):
        if i == n:
            cnt[0] += 1; return
        u = order[i]; back = [w for w in qa[u] if w in m]
        pool = ga[m[back[0]]] if back else list(gl)
        for v in pool:
            if v in used or gl[v] != ql[u] or len(ga[v]) < len(qa[u]):
                continue
            if exact_degree and len(ga[v]) != len(qa[u]):
                continue
            if all(m[w] in ga[v] for w in back):
                m[u] = v; used.add(v); rec(i + 1); used.discard(v); del m[u]
    rec(0)
    return cnt[0]


gl, ga = rd(sys.argv[1]); ql, qa = rd(sys.argv[2])
emb = count(gl, ga, ql, qa)
print(emb)
if '--aut' in sys.argv[3:]:
    aut = count(ql, qa, ql, qa, exact_degree=True)  # embeddings of the query into itself = automorphisms
    print("automorphisms", aut, "embeddings / automorphisms", emb // aut)
