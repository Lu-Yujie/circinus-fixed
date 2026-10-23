#!/bin/bash
# Run one query of a case with the Benchmark driver, in the layout used by scripts/exp_scripts/.
#
# usage: fixes/run.sh BUILD_DIR CASE_DIR PARTITIONS FILTER [more Benchmark flags...]
#   BUILD_DIR   cmake build directory (contains GraphToBinary and tests/Benchmark)
#   CASE_DIR    directory with data.graph and query.graph (text format: t N M / v id label degree / e u v)
#   PARTITIONS  number of partitions for GraphToBinary --partition and Benchmark --partition
#   FILTER      online | cfl | gql (also used as --match_order)
# Prints the Benchmark result line: dataset,size,mode,index,elapsed_s,filter_s,plan_s,enumerate_s,COUNT,order,
# max_task_s,status. The count is the 9th comma-separated field (embeddings: labeled, injective, non-induced; with
# --break_symmetry=true, embeddings / automorphisms of the query). status is complete (exact) or limit.
# Work files go to ./work/<case>_p<PARTITIONS>/ (set WORK=... to change).
set -e
BUILD=$1; CASE=$2; P=$3; FILTER=$4; shift 4
G2B=$BUILD/GraphToBinary; BENCH=$BUILD/tests/Benchmark
W=${WORK:-$PWD/work}/$(basename "$CASE")_p$P
N=$(head -1 "$CASE/query.graph" | awk '{print $2}')
mkdir -p "$W/g/data_graph" "$W/g/query_graph"
cp "$CASE/data.graph" "$W/g/data_graph/g.graph"
cp "$CASE/query.graph" "$W/g/query_graph/query_x_${N}_1.graph"
[ -f "$W/g/data_graph/g.circinus.bin.p$P" ] || \
  "$G2B" "$W/g/data_graph/g.graph" "$W/g/data_graph/g.circinus.bin.p$P" --partition=$P > "$W/graph_to_binary.log" 2>&1
"$BENCH" --data_dir "$W" --dataset g --query_mode x --query_size "$N" --query_index 1 --partition "$P" \
  --filter "$FILTER" --match_order "$FILTER" --vertex_cover dynamic --pqv cc --match_limit 0 --num_cores 1 \
  --upg 1 --batch_size 128 --verbosity 0 "$@"
