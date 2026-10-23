#include "algorithms/automorphism_check.h"

#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bliss/graph.hh"

#include "graph/query_graph.h"
#include "graph/types.h"
#include "utils/flags.h"

namespace circinus {

AutomorphismCheck::AutomorphismCheck(QueryGraph& q)
    : q_(&q), mapping_(q_->getNumVertices()), sorted_v_(q_->getNumVertices()) {
  // construct bliss graph with vertices sorted by degree
  QueryVertexID n_qvs = q.getNumVertices();

  std::iota(sorted_v_.begin(), sorted_v_.end(), 0);
  std::sort(sorted_v_.begin(), sorted_v_.end(), [&q, this](QueryVertexID a, QueryVertexID b) {
    return q.getVertexOutDegree(a) > q.getVertexOutDegree(b) ||
           (q.getVertexOutDegree(a) == q.getVertexOutDegree(b) &&
            compareNeighbors(q.getOutNeighbors(a), q.getOutNeighbors(b)));
  });
  for (uint32_t i = 0; i < n_qvs; ++i) {
    mapping_[sorted_v_[i]] = i;
    bliss_q_.add_vertex(q_->getVertexLabel(sorted_v_[i]));
  }
  for (uint32_t i = 0; i < n_qvs; ++i) {
    auto nbrs = q.getOutNeighbors(i);
    LOG(INFO) << "vertex " << i << " mapping " << mapping_[i] << " first neighbor " << nbrs.first[0];
    for (uint32_t nbr_i = 0; nbr_i < nbrs.second && i > nbrs.first[nbr_i]; ++nbr_i) {
      DLOG(INFO) << "edge " << mapping_[i] << ' ' << mapping_[nbrs.first[nbr_i]] << " from " << i << " "
                 << nbrs.first[nbr_i];
      bliss_q_.add_edge(mapping_[i], mapping_[nbrs.first[nbr_i]]);
    }
  }
}

PartialOrder AutomorphismCheck::getPartialOrder() {
  // Symmetry-breaking conditions (Grochow and Kellis): take the orbit O of the smallest vertex n0 that the current
  // group moves, require n0 < m for every m in O \ {n0}, continue with the stabilizer of n0, and repeat until the
  // group is trivial. Every class of automorphic matches then has exactly one member that satisfies all conditions.
  // This needs the orbits of the whole group and the stabilizer of n0. bliss reports only a generating set of the
  // automorphism group, so the orbits are computed as the closure of the generators, and the stabilizer of n0 is
  // computed by bliss itself after giving n0 a color of its own (a generating set of the stabilizer is not the subset
  // of the generators that fix n0).
  const uint32_t n = bliss_q_.get_nof_vertices();
  std::vector<uint32_t> color(n);  // per sorted vertex: dense label colors, then unique colors for fixed vertices
  {
    std::map<LabelID, uint32_t> label_color;
    for (uint32_t i = 0; i < n; ++i) label_color.insert({q_->getVertexLabel(sorted_v_[i]), 0});
    uint32_t c = 0;
    for (auto& p : label_color) p.second = c++;
    for (uint32_t i = 0; i < n; ++i) color[i] = label_color.at(q_->getVertexLabel(sorted_v_[i]));
  }
  uint32_t next_color = *std::max_element(color.begin(), color.end()) + 1;
  std::vector<std::pair<QueryVertexID, QueryVertexID>> conds;
  while (true) {
    bliss::Graph g(n);
    for (uint32_t i = 0; i < n; ++i) g.change_color(i, color[i]);
    for (QueryVertexID u = 0; u < q_->getNumVertices(); ++u) {
      auto nbrs = q_->getOutNeighbors(u);
      for (uint32_t j = 0; j < nbrs.second; ++j) {
        if (nbrs.first[j] < u) g.add_edge(mapping_[u], mapping_[nbrs.first[j]]);
      }
    }
    std::vector<std::vector<uint32_t>> generators;
    bliss::Stats stats;
    g.find_automorphisms(stats,
                         [](void* resultp, uint32_t n_vertices, const uint32_t* aut) {
                           ((std::vector<std::vector<uint32_t>>*)resultp)
                               ->push_back(std::vector<uint32_t>(aut, aut + n_vertices));
                         },
                         &generators);
    // orbits: connected components of the union of the generator permutations
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<uint32_t(uint32_t)> find = [&parent, &find](uint32_t x) {
      return parent[x] == x ? x : (parent[x] = find(parent[x]));
    };
    for (auto& perm : generators) {
      for (uint32_t i = 0; i < n; ++i) {
        auto a = find(i), b = find(perm[i]);
        if (a != b) parent[std::max(a, b)] = std::min(a, b);
      }
    }
    // the smallest vertex in a non-trivial orbit; it is the smallest element of its orbit
    uint32_t n0 = n;
    for (uint32_t i = 0; i < n && n0 == n; ++i) {
      for (uint32_t j = i + 1; j < n; ++j) {
        if (find(j) == find(i)) {
          n0 = i;
          break;
        }
      }
    }
    if (n0 == n) break;  // trivial group
    for (uint32_t m = n0 + 1; m < n; ++m) {
      if (find(m) == find(n0)) {
        conds.emplace_back(n0, m);
        if (verbosePlannerLog()) {
          DLOG(INFO) << "add partial order condition " << n0 << '<' << m;
        }
      }
    }
    color[n0] = next_color++;  // stabilize n0
  }
  std::sort(conds.begin(), conds.end());
  conds.erase(std::unique(conds.begin(), conds.end()), conds.end());
  return orderVertices(conds);
}

PartialOrder AutomorphismCheck::orderVertices(
    const std::vector<std::pair<QueryVertexID, QueryVertexID>>& sorted_conds) {
  auto n_qvs = q_->getNumVertices();
  PartialOrder po(n_qvs);

  if (sorted_conds.empty()) return po;

  auto& smaller_larger_qvs = po.constraints;  // qv: {smaller qvs, larger qvs}
  std::queue<QueryVertexID> queue;
  std::vector<uint32_t> checked_smaller_count(n_qvs, 0);  // i: # smaller qvs already checked regarding query vertices i
  unordered_set<QueryVertexID> covered_smaller_qvs;       // tmp
  uint32_t ordered_count = 0;

  /* find qvs with constraints */
  for (const auto& cond : sorted_conds) {
    QueryVertexID u = sorted_v_[cond.first];
    QueryVertexID v = sorted_v_[cond.second];
    smaller_larger_qvs[u].second.insert(v);
    smaller_larger_qvs[v].first.insert(u);
  }

  /* order qvs from smaller to larger by partial order */
  for (auto& p : smaller_larger_qvs) {  // find qvs without constraint of smaller qvs
    if (p.second.first.empty()) {
      // a query vertex is put in queue when all smaller qvs are poped and ordered
      queue.push(p.first);
    }
  }
  while (!queue.empty()) {
    auto current_qv = queue.front();
    queue.pop();
    auto& entry = smaller_larger_qvs.at(current_qv);
    covered_smaller_qvs.clear();
    // find all smaller qvs than `smaller` and deduplicate constraints
    for (auto smaller : entry.first) {
      // here `smaller_than_smaller` is an exhaustive set of smaller qvs regarding `smaller`
      auto& smaller_than_smaller = smaller_larger_qvs.at(smaller).first;
      covered_smaller_qvs.insert(smaller_than_smaller.begin(), smaller_than_smaller.end());
    }
    for (auto smaller : entry.first) {
      if (covered_smaller_qvs.insert(smaller).second) {  // not covered by any other smaller qv
        po.po_constraint_adj[current_qv].push_back(smaller);
      }
    }
    // update smaller qvs to an exhaustive set
    entry.first.swap(covered_smaller_qvs);
    po.order_index[current_qv] = ordered_count++;
    // check all larger qvs and update queue
    for (auto next_qv : entry.second) {
      if (++checked_smaller_count[next_qv] == smaller_larger_qvs.at(next_qv).first.size()) {
        queue.push(next_qv);
      }
    }
  }
  DCHECK_EQ(ordered_count, smaller_larger_qvs.size());

  // make exhaustive set of larger qvs
  for (auto& p : smaller_larger_qvs) {
    for (auto smaller : p.second.first) {
      smaller_larger_qvs.at(smaller).second.insert(p.first);
    }
  }
  for (auto& p : smaller_larger_qvs) {
    po.n_all_related_constraints[p.first] += p.second.first.size() + p.second.second.size();
  }
  return po;
}

std::map<QueryVertexID, std::set<QueryVertexID>> AutomorphismCheck::getAEquivalenceClasses(
    const std::vector<std::vector<QueryVertexID>>& A) {
  std::map<QueryVertexID, std::set<QueryVertexID>> eclasses;
  for (unsigned int id = 0; id < bliss_q_.get_nof_vertices(); ++id) {
    std::set<QueryVertexID> eclass;
    eclass.insert(id);
    for (auto& perm : A) {
      eclass.insert(perm[id]);
    }
    QueryVertexID rep = *eclass.begin();  // the smallest element
    eclasses[rep].insert(eclass.cbegin(), eclass.cend());
  }
  return eclasses;
}

}  // namespace circinus
