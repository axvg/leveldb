#!/usr/bin/env bash
# Benchmark sweep: stock LevelDB vs the PLR learned index, over several error
# bounds (epsilon) and read/write workloads. Writes a tidy CSV that plot.py
# turns into the thesis figures and tables.
#
# Usage: benchmarks/learned/run.sh [db_bench_path] [num] [reps]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DB_BENCH="${1:-$HERE/../../build/db_bench}"
NUM="${2:-1000000}"     # keys
REPS="${3:-5}"          # repetitions per configuration
OUT="$HERE/results/bench.csv"

EPSILONS=(2 4 8 16 32 64 128)
# Read-only workloads (the learned index only affects reads).
READ_BENCHMARKS="readrandom,readmissing,readseq"

echo "config,epsilon,benchmark,rep,micros_per_op,mb_per_s" > "$OUT"

run_one() {  # $1=config label  $2=use_li  $3=epsilon
  local label="$1" use_li="$2" eps="$3"
  local db
  db="$(mktemp -d)"
  # Build the data once, then read it several times in the same process so each
  # SSTable is opened once (model trained once) and reused across the reads.
  local benches="fillseq"
  for _ in $(seq "$REPS"); do benches="$benches,$READ_BENCHMARKS"; done
  "$DB_BENCH" --db="$db" --num="$NUM" --use_existing_db=0 \
      --use_learned_index="$use_li" --plr_error="$eps" \
      --benchmarks="$benches" 2>/dev/null \
  | awk -v label="$label" -v eps="$eps" '
      /^(fillseq|readrandom|readmissing|readseq)/ {
        bench=$1; micros=$3;
        mb="";
        for (i=1;i<=NF;i++) if ($i=="MB/s") mb=$(i-1);
        rep[bench]++;
        print label","eps","bench","rep[bench]","micros","mb;
      }' >> "$OUT"
  rm -rf "$db"
}

# Baseline (no learned index); epsilon column is n/a -> 0.
run_one baseline 0 0
for eps in "${EPSILONS[@]}"; do
  run_one learned 1 "$eps"
done

echo "Wrote $OUT"
