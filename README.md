# Circinus with counting fixes (branch `complete`)

This branch is the `artifact1` branch of [TatianaJin/circinus](https://github.com/TatianaJin/circinus) (commit `1a2f0c1`,
"[CMake] Add BenchmarkBFL") plus one commit per fix. While using the artifact in a benchmark study of subgraph matching and
counting, we found wrong counts (too low and too high), aborts, crashes and hangs. This branch fixes all of them.
The original README follows below; the two places where this branch behaves differently are marked.

- [`FIXES.md`](FIXES.md) lists every fix with the symptom, a minimal reproduction and the counts before and after.
  Each commit message gives the cause (file and line in `1a2f0c1`), the fix and how it was checked
  (`git log 1a2f0c1..complete`).
- `fixes/` has the small cases, a script that runs one query (`fixes/run.sh BUILD_DIR CASE_DIR PARTITIONS FILTER`),
  a brute-force embedding counter and a generator for the counter-width cases.

## What was wrong

| kind | problems (details in `FIXES.md`) |
|---|---|
| wrong counts | twin-set shortcuts that ignore same-label keys to be excluded (undercount); the 2–3-set inclusion–exclusion and twin shortcuts assuming that twin sets are identical (overcount); **set reuse** in three forms: a reused set filtered by another vertex's candidates under cfl / gql (undercount), a key taking values from a set that misses same-label keys added later (overcount; yeast `query_dense_24_61`: 72,304,239 instead of 53,111,964), and a reused start-vertex set that holds one chunk with several threads (undercount); `--break_symmetry=true` (constraints from generators only, constrained counts divided by k!, keys stored under each other's query vertices after reordering, conditions checked against the wrong key, a vertex sort that read outside its vector on queries with more than 16 vertices); 64-bit counts that wrap or stop at 2^64 − 1 |
| aborts and crashes | queries with ≥ 32 vertices; a query vertex without candidates; `--partition 0` or `1`; METIS built with 64-bit `idx_t` (as in `cmake/dep.cmake`); a reply race and a use-after-free race under cfl / gql; the time limit with `--upg=false`; sort comparators that are not strict weak orderings (`GraphToBinary --sort_by_degree=false`; with `--break_symmetry=true`, heap corruption on queries with more than 16 vertices); the online mode in Debug builds |
| hangs | `reserve(0)` on a non-empty parallel-hashmap set; the time limit during a cfl / gql filter; an exception while planning |
| build and runtime | GCC ≥ 12; building without network access; thread pinning that ignored the process's CPU set |

The algorithms are unchanged except where a shortcut was unsound. Set reuse is kept, but only where the reused set
equals the target vertex's candidate set; without set reuse the original code counts correctly.

## What changes for users

- **Exact counts.** Counts are exact at any size: 128-bit, with GMP above 2^128. GMP is a new dependency.
- **Result line.** `Benchmark` prints
  `dataset,query_size,query_mode,query_index,elapsed_execution_time,filter_time,plan_time,enumerate_time,n_embeddings,order,max_task_time,status`.
  The new last field `status` is `complete` (every match was counted and the count is exact), `limit` (the search
  stopped at `--match_limit`) or `timeout` (the query reached `--time_limit`; the count is not complete).
- **What is counted.** `n_embeddings` counts embeddings: labeled, injective, non-induced, with automorphic images
  counted separately. With `--break_symmetry=true` it is embeddings divided by the number of automorphisms of the query.
- **Time limit.** `--time_limit` is checked between batches, so a single long counting step can run past it.
- **Boolean flags.** These are gflags booleans: `--break_symmetry 1` and `--upg 1` turn the option on, but so does
  `--upg 0`, because a bare boolean flag takes no value. Write `--upg=false` to turn an option off.
- **Partitioned graphs.** The partitioning depends on the METIS build. Regenerate the partitioned binary graphs with
  this branch's `GraphToBinary` instead of reusing files written by another build.

## How it was checked

- Every commit builds with GCC 14; `circinus_unit_tests` 11 / 11.
- 72 configurations, single-threaded:
  - online, cfl and gql;
  - dynamic, static and no vertex cover;
  - 1, 2, 3 and 5 partitions;
  - symmetry breaking off and on.

  They ran on three corpora: 1,200 random labeled cases, 400 cases with large same-label groups, and 600 cases whose
  queries have automorphisms.
- All 1,800 FaSTest yeast queries in online, cfl and gql.
- Multi-threaded runs (4 threads, 13 configurations).
- A stress corpus for symmetry breaking: 260 cases with many twin vertices (stars, complete bipartite graphs, wheels
  with pendant twins, random queries with groups of twins), automorphism groups of up to 30! elements, and counts
  just below and above 2^64 and 2^128 and around 10^40; launched directly and through Python's `subprocess`.
- Builds with AddressSanitizer and UndefinedBehaviorSanitizer on the stress corpus and samples of the other corpora.
- Reference counts: FaSTest's published counts and the agreement of other subgraph matchers for yeast; independent
  exact counters (and plain backtracking where small) for the random cases.
- In about 440,000 runs before rows 25–28 of `FIXES.md`, and in about 560,000 runs of the same sweeps with the
  additions above after them, there was no wrong count and no crash, and the sanitizer builds reported nothing. Runs
  that reached the time limit are not counted as errors.

## Status and license

This is a fix branch, not a new version of Circinus: the method and its performance are those of the paper. The
original repository is no longer maintained. It has no LICENSE file; its source headers state Apache-2.0, and the
changes in this branch are offered under the Apache License 2.0.

---

# Original README (`artifact1`)

Circinus: an efficient parallel subgraph matching framework that reduces computation redundancy and optimizes memory usage
=======

### Dependency

Circinus has the following required dependencies:

- CMake (>= 3.13.0)
- GCC (>= 8.2.0, requires c++17 support)
- Gflags
- Glog
- [bliss-0.73](http://www.tcs.hut.fi/Software/bliss/bliss-0.73.zip)
- GMP (exact match counts above 2^128)
- ZeroMQ (libzmq and cppzmq)

GKlib, METIS and parallel-hashmap are downloaded and built by cmake unless they are already installed in
`third_party/` (or under a prefix in `CMAKE_PREFIX_PATH`); with all of them installed, the build needs no network access.

See [FIXES.md](FIXES.md) for the fixes on top of the `artifact1` branch and how to reproduce each defect.

For testing, Circinus uses [googletest](https://github.com/google/googletest/releases/tag/release-1.8.0) (needed when `cmake -DBUILD_TESTS=ON`).

#### Installing Bliss

A bash script `scripts/install_bliss.sh` is provided to download and install bliss in the `third_party` folder.
```
git clone <git link> && cd circinus
./scripts/install_bliss.sh
```

### Build Circinus

1. Build and install
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release # CMAKE_BUILD_TYPE: Release, Debug, RelWithDebInfo
make Circinus Benchmark GraphToBinary -j4 # 4 if to use 4 threads to compile
# (this branch: the target is Benchmark, built under tests/ when googletest is found; the original line said Benckmark)
```

### Experimental Data and Query

The experiment results in paper is in `exp_results_in_paper/exp/data`. If you want to reproduce the results in paper, please follow steps below.

#### Export the home dir of circinus.
```
export CIRCINUS_HOME=  #project dir
```

#### Download Datasets and queries

Go to https://gofile.io/d/zkjYel and download the datasets and queries file `subgraph_matching_datasets.tar.gz` to `$CIRCINUS_HOME` and decompress it.
```
cd $CIRCINUS_HOME
tar -xzvf subgraph_matching_datasets
```


#### Run querise
Create file to store artifact experiment results.
```
cd $CIRCINUS_HOME
mkdir -p exp_results_artifact/exp/data/
cd exp_results_artifact/exp/data/
mkdir cardinality_sensitivity  compare_peregrine  compare_single_thread  compare_unlabeled  parallelization  redundancy_multithread  vc_influence
```

If we run `tree` in `exp_results_artifact`, we will get 
```
.
└── exp
    └── data
        ├── cardinality_sensitivity
        ├── compare_peregrine
        ├── compare_single_thread
        ├── compare_unlabeled
        ├── parallelization
        ├── redundancy_multithread
        └── vc_influence

9 directories, 0 files
```
which is same as `exp_results_in_paper`.

All experiment scripts are in the `scripts/exp_scripts/`, check details in `scripts/exp_scripts/run_all_exps.sh`. If you run `scripts/exp_scripts/run_all_exps.sh`, the results file format and results format should be same as the circinus' results in `exp_results_in_paper/exp/data/`. The screenshot below is an example of comparison experiment results of single thread labeled queries. 

The file name `batch_human_8_cfl_cfl_3_dynamic_cc_1_single` means query size: 8, algorithm: CFL, vertex cover: dynamic(Circinus in paper), number cores: 1, experiment name: single thread. 

Note that `batch_human_8_cfl_cfl_3_dynamic_cc_1_single_count` is the file from accumulating the intersection count (from the profile result, the scripts will do this automatically) as the last column to `batch_human_8_cfl_cfl_3_dynamic_cc_1_single`.

Each file has 200 lines corresponding to 200 queries. The result format is `dataset,query_size,query_mode,query_index,elapsed_execution_time,filter_time,plan_time,enumerate_time,n_embeddings,order,max_task_time,n_intersections`, first four is about the query graph. 

`elapsed_execution_time` is the execution time of the query, `n_intersections` is the totaly intersection count. Note that lines of **timeout** queries only have four fileds `dataset,query_size,query_mode,query_index`.

> **This branch:** `Benchmark` adds a `status` field (`complete`, `limit` or `timeout`) after `max_task_time`, before any column the scripts append; see "What changes for users" above.
![screenshot](screenshot.png)


