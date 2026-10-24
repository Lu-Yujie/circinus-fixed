# Fixes on top of `artifact1`

This branch is `artifact1` at commit `1a2f0c1` ("[CMake] Add BenchmarkBFL") plus one commit per fix. Each commit
message gives the symptom, the cause with file and line numbers in `1a2f0c1`, the fix, and how it was checked
(`git log 1a2f0c1..`). The algorithms are unchanged except where a shortcut was unsound.

Counts are embeddings (labeled, injective, non-induced; automorphic images counted separately), as Circinus prints
them with the default `--break_symmetry=false`. With `--break_symmetry=true` the count is embeddings divided by the
number of automorphisms of the query.

## Reproducing

Build as in the README, then

    fixes/run.sh BUILD_DIR CASE_DIR PARTITIONS FILTER [more Benchmark flags]

runs `GraphToBinary --partition=PARTITIONS` and one `Benchmark` query (single thread, `--vertex_cover dynamic`,
`--match_limit 0`); the 9th comma-separated field of the result line is the count. `fixes/cases/` has the small
cases; `python3 fixes/brute_force.py data.graph query.graph [--aut]` counts their embeddings by plain backtracking
(`--aut` also divides by the number of automorphisms of the query). The yeast queries are those of FaSTest
(https://github.com/SNUCSE-CTA/FaSTest, `dataset/yeast`; correct counts in `dataset/yeast/yeast_ans.txt`); put
`yeast.graph` and a query into a case directory as `data.graph` and `query.graph`.

"Before" is `1a2f0c1` with only the GCC 12 build fix (and METIS built with 32-bit `idx_t`, see fix 4); "after" is
this branch. For rows 23 and 24, "before" is this branch without that fix; the code is the same in `1a2f0c1`, where
symmetry breaking also has the defects of rows 14 and 21. P is the number of partitions, F the filter.

| # | fix | reproduction (P, F, flags) | before | after |
|---|---|---|---|---|
| 1 | Build with GCC >= 12 (`src/utils/utils.h`) | build | compile error | builds |
| 2 | Build without network access: an installed parallel-hashmap is used (as METIS and GKlib already are) | `cmake` offline with the headers in `third_party/include` | download fails | builds |
| 3 | Undercount: twin-set shortcuts ignore same-label keys that must be excluded | `c1_cycle4_tiny`, P = 2..6, cfl | 28, 36, 32, 36, 0 | 40 |
| | | `c2_star9_v182`, 3, online | 456 | 480 |
| 4 | METIS with 64-bit `idx_t` (as built by `cmake/dep.cmake`): `getMetisParts` cast 32-bit arrays to `idx_t*` | `GraphToBinary` on `c1_cycle4_tiny/data.graph`, `--partition=2` | SIGSEGV | partitioned binary |
| | | `GraphToBinary` on yeast, `--partition=3` | METIS "Memory allocation failed", exit 0 | partitioned binary |
| 5 | Overcount: the set shortcuts assumed that twin vertices have identical sets | `c3_dense8_v8`, 3, online | 6 | 4 |
| | | yeast `query_dense_8_174`, 3, online | 478 | 476 |
| 6 | Undercount under cfl / gql: a reused set was filtered by another vertex's candidates | `c4_tree8_v126`, 3, cfl | 0 | 6 |
| 7 | Overcount: a key that reuses a set took the value of a same-label key added after the set was built | `c7_key_reuse_v19`, 3, online | 1 | 0 |
| | | yeast `query_dense_24_61`, 3, online | 72304239 | 53111964 |
| 8 | Planner abort on queries with >= 32 vertices (`1 << v` on `int`) | yeast `query_sparse_32_27`, 3, online | `Check failed: sum > 0` | 80586396380 |
| 9 | A query vertex without candidates aborted instead of giving 0 | `c6a_absent_label`, 2, cfl | `No candidate matching query vertex?` | 0 |
| | | `c6a_absent_label`, 2, online | `phmap at(): lookup non-existent key` | 0 |
| | | `c6b_pruned_empty`, 3, cfl | `Check failed: input_size != 0` | 0 |
| 10 | Hang: `reserve(0)` on a non-empty parallel-hashmap set | yeast `query_dense_24_66`, 3, online | no result, 100% CPU | 584 (0.1 s) |
| 11 | Hang when the time limit expires during a non-final cfl / gql filter | cfl or gql with a time limit shorter than the filtering phase | no reply | time-out reply |
| 12 | Counts above 2^64 wrapped or stopped at 2^64-1; the result line now ends with `complete` or `limit` | `python3 fixes/gen_star.py 2 60000 o3`, 3, online | 18446744073709551615 | 25920000000000000000 |
| | | `python3 fixes/gen_star.py 1 70000 o2`, 3, online | 5563255926290448384 | 24010000000000000000 |
| 13 | Unpartitioned graphs (`--partition 0` or `1`); one partition is planned like several (bounded memory with threads) | `c1_cycle4_tiny`, P = 0 or 1, online | SIGSEGV | 40 |
| | | `c1_cycle4_tiny`, P = 0 or 1, cfl / gql | `Check failed: n_plans != 0` | 40 |
| 14 | `--break_symmetry=true`: conditions from bliss generators only, factorial division of constrained counts, empty sets in `lowerBound` | `c8_cycle5_tiny`, 2 / 3, cfl or gql, `--break_symmetry=true` | 4 / 3 | 2 |
| | | `c8_cycle5_tiny`, 2 or 3, online, `--break_symmetry=true` | SIGSEGV | 2 |
| 15 | Multi-threaded online undercount: the chunked start vertex's set was reused | yeast `query_dense_8_108`, 3, online, `--num_cores 4` | 4 | 95 |
| | | yeast `query_sparse_8_156`, 3, online, `--num_cores 4` | 0 | 1833 |
| 16 | Executor threads are pinned within the process's CPU set (taskset, cgroups) | `taskset -c 20-23 ... --num_cores 4` | threads on CPUs 0-3 | CPUs 20-23 |
| 17 | `GraphToBinary --sort_by_degree=false`: invalid `std::sort` comparator | yeast, `--partition=3 --sort_by_degree=false` | SIGABRT | binary; counts as published |
| 18 | Intermittent crash (or a wrong count of 0) under cfl / gql: the filter phase's reply could be pushed before its data was set | `c2_star9_v182`, 3, cfl and gql, 1,000 runs each, 64 at a time | 2 crashes (SIGSEGV, or `Check failed: result != nullptr`) | none |
| 19 | Crash at the time limit with the unpartitioned driver (`--upg=false`, cfl / gql) | a cfl / gql query with `--upg=false` that reaches `time_limit` | SIGSEGV | time-out reply |
| 20 | Multi-threaded online: a start vertex without candidates aborted | `c9_star5_tiny`, 3, online, `--num_cores 2` | `Check failed: false CandidateSetView with multiple ranges unsupported` | 0 |
| 21 | `--break_symmetry=true`: off-by-one in `PartialOrder::orderVertices`, the query never replied | yeast `query_sparse_16_193`, 3, cfl, `--pqv cc --break_symmetry=true` | no reply (server: `phmap at(): lookup non-existent key`) | 15177 |
| 22 | An exception while planning a query left the client waiting forever | (the case of row 21 without its fix) | no reply | error reply |
| 23 | `--break_symmetry=true`: when the partial order reordered the keys that an operator enumerates, the keys were written to each other's query vertices | `c10_wheel4_v55`, 5, cfl or gql, `--break_symmetry=true` | 0 | 1 |
| | | yeast `query_dense_24_173`, 3, gql, `--break_symmetry=true` | 124550 | 199280 |
| 24 | `--break_symmetry=true`: a condition "set larger than key" was checked against another of the keys enumerated together | `c11_arms7_self`, 1, online, `--break_symmetry=true` | 0 | 1 |
| | | `c11_arms7_self`, 2, 3 or 5, online, `--break_symmetry=true` | 2 | 1 |

## Result line

`Benchmark` prints `dataset,size,mode,index,elapsed_s,filter_s,plan_s,enumerate_s,count,order,max_task_s,status`.
The last field is new: `complete` means that every match was counted and the count is exact (at any size; above
2^128 it is kept in GMP), `limit` that the search stopped at `--match_limit`. A query vertex without candidates gives
count 0 with status `complete`.

## Not changed

- `TestMinimumWeightVertexCoverAlgorithm.Correctness` still fails: it expects all minimum covers and the minimum
  weight from `getBestObjective()`, while `WeightedBnB` keeps the best 3 covers and does not prune on the incumbent
  (its `best_cover_weight_` is never lowered). This affects planning time only.
- The other unit tests that read datasets from `/data/share/...` need those files.
