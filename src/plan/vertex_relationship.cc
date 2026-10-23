#include "plan/vertex_relationship.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "graph/types.h"
#include "utils/flags.h"
#include "utils/utils.h"

namespace circinus {

std::pair<bool, uint32_t> VertexRelationship::canBeReusedBy(QueryVertexID u1, QueryVertexID u2) const {
  auto q = qv_equivalence_->getQueryGraph();
  if (q->getVertexLabel(u1) != q->getVertexLabel(u2)) return {false, 0};
  if (q->getVertexOutDegree(u1) > q->getVertexOutDegree(u2)) return {false, 0};

  auto u2_nbrs = q->getOutNeighbors(u2);
  auto u1_nbrs = q->getOutNeighbors(u1);
  unordered_set<QueryVertexID> uncovered_parent_vertices(u2_nbrs.first, u2_nbrs.first + u2_nbrs.second);
  for (uint32_t i = 0; i < u1_nbrs.second; ++i) {
    if (uncovered_parent_vertices.count(u1_nbrs.first[i]) == 0) {
      return {false, 0};
    }
  }

  auto po = qv_equivalence_->getPartialOrder();
  if (po != nullptr) {  // partial order constraint equivalence
    auto u2_constraints = po->constraints.find(u2);
    auto u1_constraints = po->constraints.find(u1);
    if (u1_constraints != po->constraints.end()) {
      for (auto cond : u1_constraints->second.first) {
        if (u2_constraints == po->constraints.end() || u2_constraints->second.first.count(cond) == 0) {
          return {false, 0};
        }
      }
      for (auto cond : u1_constraints->second.second) {
        if (u2_constraints == po->constraints.end() || u2_constraints->second.second.count(cond) == 0) {
          return {false, 0};
        }
      }
    }
  }
  return {true, u2_nbrs.second - u1_nbrs.second};
}

std::pair<QueryVertexID, std::vector<QueryVertexID>> VertexRelationship::findReusableSet(
    QueryVertexID target, std::vector<QueryVertexID>& set_vertices,
    const unordered_set<QueryVertexID>& existing_vertices, uint64_t exclude_mask) const {
  DLOG(INFO) << "findReusableSet for " << target << " among [" << toString(set_vertices) << " ]";
  std::pair<QueryVertexID, std::vector<QueryVertexID>> res;
  res.first = DUMMY_QUERY_VERTEX;
  auto q = qv_equivalence_->getQueryGraph();
  auto po = qv_equivalence_->getPartialOrder();

  unordered_set<QueryVertexID> uncovered_parent_vertices;
  auto target_nbrs = q->getOutNeighbors(target);
  for (uint32_t i = 0; i < target_nbrs.second; ++i) {
    if (existing_vertices.count(target_nbrs.first[i])) {
      uncovered_parent_vertices.insert(target_nbrs.first[i]);
    }
  }

  for (QueryVertexID set_vertex : set_vertices) {
    if (exclude_mask >> set_vertex & 1) continue;
    if (q->getVertexLabel(set_vertex) != q->getVertexLabel(target)) continue;
    // A reused set has been intersected with the candidate set of set_vertex, not with that of target.
    // Under a candidate filter (cfl/gql/...: every mode except online, which sets candidate_set_intersection = 3)
    // the candidates of target may contain data vertices that the candidates of set_vertex do not, so the reused set
    // silently drops matches. It is only safe when the two query vertices are twins (same label, same
    // neighbourhood): swapping them is an automorphism, so every match of target is a match of set_vertex and survives
    // any sound filter of set_vertex.
    if (FLAGS_candidate_set_intersection != 3 && !qv_equivalence_->isEquivalent(set_vertex, target)) continue;
    bool is_reusable = true;
    DLOG(INFO) << "check neighbor of " << target << " and " << set_vertex;
    // neighborhood equivalence
    auto set_nbrs = q->getOutNeighbors(set_vertex);
    if (set_nbrs.second > target_nbrs.second) continue;  // the degree filter of set_vertex will make it unusable
    auto uncovered = uncovered_parent_vertices;
    for (uint32_t i = 0; i < set_nbrs.second; ++i) {
      if (existing_vertices.count(set_nbrs.first[i])) {
        auto pos = uncovered.find(set_nbrs.first[i]);
        if (pos != uncovered.end()) {
          uncovered.erase(pos);
        } else {
          is_reusable = false;
          break;
        }
      }
    }
    if (uncovered.size() == uncovered_parent_vertices.size()) continue;
    if (!is_reusable) continue;

    if (po != nullptr) {
      auto target_constraints = po->constraints.find(target);
      DLOG(INFO) << "check po gt of " << target << " and " << set_vertex;
      // partial order constraint equivalence
      auto set_constraints = po->constraints.find(set_vertex);
      if (set_constraints != po->constraints.end()) {
        for (auto cond : set_constraints->second.first) {
          if (existing_vertices.count(cond) == 0) continue;
          if (target_constraints == po->constraints.end() || target_constraints->second.first.count(cond) == 0) {
            is_reusable = false;
            break;
          }
        }
        if (!is_reusable) continue;
        DLOG(INFO) << "check po lt of " << target << " and " << set_vertex;
        for (auto cond : set_constraints->second.second) {
          if (existing_vertices.count(cond) == 0) continue;
          if (target_constraints == po->constraints.end() || target_constraints->second.second.count(cond) == 0) {
            is_reusable = false;
            break;
          }
        }
      }
      if (!is_reusable) continue;
    }

    if (res.first == DUMMY_QUERY_VERTEX || res.second.size() > uncovered.size()) {
      // if not check target_parent_index == uncovered_parent_vertices.size(),
      // target neighbors can be a superset of reusable set neighbors
      res.first = set_vertex;
      res.second.clear();
      res.second.insert(res.second.end(), uncovered.begin(), uncovered.end());
    }
  }
  if (res.first != DUMMY_QUERY_VERTEX && verbosePlannerLog()) {
    LOG(INFO) << "target " << target << " reusable set vertex " << res.first << " uncovered parents "
              << toString(res.second);
  }
  return res;
}

}  // namespace circinus
