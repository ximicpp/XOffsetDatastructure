#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASELINE_COMMIT="${BASELINE_COMMIT:-6d033a77}"
if [[ -f "$ROOT_DIR/build/CMakeCache.txt" ]]; then
    CLANG_CXX="$(grep '^CMAKE_CXX_COMPILER:' "$ROOT_DIR/build/CMakeCache.txt" | head -n 1 | cut -d= -f2-)"
    CLANG_C="$(grep '^CMAKE_C_COMPILER:' "$ROOT_DIR/build/CMakeCache.txt" | head -n 1 | cut -d= -f2-)"
else
    CLANG_CXX="${CXX:-clang++}"
    CLANG_C="${CC:-clang}"
fi

timestamp="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/build/perf_compare/$timestamp}"
TMP_ROOT="$(mktemp -d /tmp/xoffset-perf-compare.XXXXXX)"
BASELINE_TREE="$TMP_ROOT/baseline"
CURRENT_BUILD_DIR="$TMP_ROOT/current-build"
BASELINE_BUILD_DIR="$TMP_ROOT/baseline-build"
CURRENT_BIN="$CURRENT_BUILD_DIR/bin/Release/bench_compare"
BASELINE_BIN="$BASELINE_BUILD_DIR/bin/Release/bench_compare"
CURRENT_TSV="$OUT_DIR/current.tsv"
BASELINE_TSV="$OUT_DIR/baseline.tsv"
SUMMARY_TSV="$OUT_DIR/summary.tsv"

mkdir -p "$OUT_DIR"

BOOST_SHA="$(git -C "$ROOT_DIR" ls-tree "$BASELINE_COMMIT" external/boost | awk '{print $3}')"
TYPELAYOUT_SHA="$(git -C "$ROOT_DIR" ls-tree "$BASELINE_COMMIT" external/typelayout | awk '{print $3}')"
CURRENT_BOOST_SHA="$(git -C "$ROOT_DIR/external/boost" rev-parse HEAD)"

prepare_baseline_tree() {
    if [[ "$CURRENT_BOOST_SHA" != "$BOOST_SHA" ]]; then
        echo "Current external/boost checkout ($CURRENT_BOOST_SHA) does not match baseline commit ($BOOST_SHA)." >&2
        exit 1
    fi
    mkdir -p "$BASELINE_TREE"
    git -C "$ROOT_DIR" archive "$BASELINE_COMMIT" | tar -x -C "$BASELINE_TREE"
    mkdir -p "$BASELINE_TREE/external/boost" "$BASELINE_TREE/external/typelayout"
    rsync -a --exclude '.git' "$ROOT_DIR/external/boost/" "$BASELINE_TREE/external/boost/"
    git -C "$ROOT_DIR/external/typelayout" archive "$TYPELAYOUT_SHA" | tar -x -C "$BASELINE_TREE/external/typelayout"
    cp "$ROOT_DIR/tools/bench_compare.cpp" "$BASELINE_TREE/tools/bench_compare.cpp"
    if ! grep -q "add_executable(bench_compare bench_compare.cpp)" "$BASELINE_TREE/tools/CMakeLists.txt"; then
        cat <<'EOF' >> "$BASELINE_TREE/tools/CMakeLists.txt"

add_executable(bench_compare bench_compare.cpp)
configure_xoffset_target(bench_compare)
EOF
    fi
}

join_results() {
    awk -F '\t' '
        NR == FNR {
            if (FNR == 1) next;
            base[$2] = $0;
            next;
        }
        FNR == 1 {
            print "benchmark\tbaseline_median_ms\tcurrent_median_ms\tbaseline_ns_per_item\tcurrent_ns_per_item\tspeedup_vs_baseline\tbaseline_used_bytes\tcurrent_used_bytes\tbaseline_wire_bytes\tcurrent_wire_bytes";
            next;
        }
        {
            split(base[$2], b, FS);
            base_ms = b[5] + 0.0;
            cur_ms = $5 + 0.0;
            base_ns = b[9] + 0.0;
            cur_ns = $9 + 0.0;
            speedup = (cur_ms > 0.0) ? (base_ms / cur_ms) : 0.0;
            printf "%s\t%.3f\t%.3f\t%.1f\t%.1f\t%.3f\t%s\t%s\t%s\t%s\n",
                $2, base_ms, cur_ms, base_ns, cur_ns, speedup, b[11], $11, b[12], $12;
        }
    ' "$BASELINE_TSV" "$CURRENT_TSV" > "$SUMMARY_TSV"
}

prepare_baseline_tree

cmake -S "$ROOT_DIR" -B "$CURRENT_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$CLANG_CXX" \
    -DCMAKE_C_COMPILER="$CLANG_C" \
    '-DCMAKE_CXX_FLAGS=-DXOFFSET_BENCH_VERSION_LABEL=\"current_frozen\"'
cmake --build "$CURRENT_BUILD_DIR" --target bench_compare -j 8

cmake -S "$BASELINE_TREE" -B "$BASELINE_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$CLANG_CXX" \
    -DCMAKE_C_COMPILER="$CLANG_C" \
    '-DCMAKE_CXX_FLAGS=-DXOFFSET_BENCH_VERSION_LABEL=\"boost_baseline\"'
cmake --build "$BASELINE_BUILD_DIR" --target bench_compare -j 8

"$BASELINE_BIN" > "$BASELINE_TSV"
"$CURRENT_BIN" > "$CURRENT_TSV"
join_results

cat <<EOF
Performance comparison complete.
Baseline commit: $BASELINE_COMMIT
Baseline boost submodule: $BOOST_SHA
Baseline typelayout submodule: $TYPELAYOUT_SHA
Artifacts:
  $BASELINE_TSV
  $CURRENT_TSV
  $SUMMARY_TSV
Temporary baseline tree:
  $BASELINE_TREE
EOF
