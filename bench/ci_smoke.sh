#!/bin/bash
# Smoke test of the frozen benchmark trees (bench/slg, bench/formal, bench/reloc) with one
# compiler. Runs inside the compiler's container from the repository root.
#   bench/ci_smoke.sh <c++ compiler> <cereal include dir> <valgrind include parent> <out dir>
# Checks: MANIFESTs; SLG regression tests pass; slg_bench
# builds (default and SLG_IR_COUNT) and
# --mode verify --scale S agrees across formats under both protocols; reloc_bench
# builds (default and RELOC_IR_COUNT) and --mode verify agrees across sides; the formal
# driver builds (default and SC_IR_COUNT), its contracts pass, fixtures S and M
# equal the committed references, validate S/M passes and the M input wire is
# the pinned formal input wire (8ee8ff53... since wire format 3.0; see
# bench/formal/scripts/smoke.py)
set -euo pipefail
CXX=$1; CEREAL=$2; VALGRIND_PARENT=$3; OUT=$4
mkdir -p "$OUT"
JOBS=${JOBS:-4}
"$CXX" --version | head -n 1

echo "== manifests"
python3 bench/check_manifest.py

# The six builds below are one or a few large translation units each, so one after another they
# left most cores idle (4.5 min at 31% CPU on 4 cores, 2026-11-24). They run at the same time,
# each into its own log, printed in this order at the end.
UNIT_JOBS=$(( JOBS / 2 > 0 ? JOBS / 2 : 1 ))

slg() {
  cmake -S bench/slg -B "$OUT/slg" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
    -DSLG_WITH_XOFFSET=ON -DSLG_BUILD_REGRESSION_TESTS=ON -DCEREAL_INCLUDE="$CEREAL"
  cmake --build "$OUT/slg" --target slg_bench slg_regression_tests --parallel "$UNIT_JOBS"
  ctest --test-dir "$OUT/slg" --output-on-failure --no-tests=error -R '^slg\.'
  "$OUT/slg/slg_bench" --mode verify --scale S --formats xoffset,cereal
  SLG_REUSE=1 "$OUT/slg/slg_bench" --mode verify --scale S --formats xoffset,cereal
}
slg_ir() {
  cmake -S bench/slg -B "$OUT/slg-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
    -DSLG_WITH_XOFFSET=ON -DCEREAL_INCLUDE="$CEREAL" -DSLG_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
  cmake --build "$OUT/slg-ir" --target slg_bench --parallel "$UNIT_JOBS"
}
reloc() {
  cmake -S bench/reloc -B "$OUT/reloc" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
  cmake --build "$OUT/reloc" --target reloc_bench --parallel "$UNIT_JOBS"
  "$OUT/reloc/reloc_bench" --mode verify
}
reloc_ir() {
  cmake -S bench/reloc -B "$OUT/reloc-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
    -DRELOC_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
  cmake --build "$OUT/reloc-ir" --target reloc_bench --parallel "$UNIT_JOBS"
}
formal() {
  cmake -S bench/formal -B "$OUT/formal" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
  cmake --build "$OUT/formal" --parallel "$UNIT_JOBS"
  ctest --test-dir "$OUT/formal" --output-on-failure
  rm -rf "$OUT/fixtures"
  python3 -I bench/formal/scripts/make_fixtures.py --out "$OUT/fixtures"
  python3 -I bench/formal/scripts/smoke.py --driver "$OUT/formal/bin/sc_xoffset" --fixtures "$OUT/fixtures"
}
formal_ir() {
  cmake -S bench/formal -B "$OUT/formal-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
    -DSC_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
  cmake --build "$OUT/formal-ir" --target sc_xoffset --parallel "$UNIT_JOBS"
}

UNITS=(slg slg_ir reloc reloc_ir formal formal_ir)
PIDS=()
for u in "${UNITS[@]}"; do
  ( set -euo pipefail; "$u" ) > "$OUT/smoke-$u.log" 2>&1 &
  PIDS+=($!)
done
rc=0
for i in "${!UNITS[@]}"; do
  u=${UNITS[$i]}
  if wait "${PIDS[$i]}"; then r=ok; else r=FAILED; rc=1; fi
  echo "== $u: $r"
  cat "$OUT/smoke-$u.log"
done
if (( rc != 0 )); then
  echo "bench smoke FAILED ($CXX)"
  exit 1
fi
echo "bench smoke passed ($CXX)"
