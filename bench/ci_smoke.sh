#!/bin/bash
# Smoke test of the frozen benchmark trees (bench/slg, bench/formal, bench/reloc) with one
# compiler. Runs inside the compiler's container from the repository root.
#   bench/ci_smoke.sh <c++ compiler> <cereal include dir> <valgrind include parent> <out dir>
# Checks: MANIFESTs; slg_bench builds (default and SLG_IR_COUNT) and
# --mode verify --scale S agrees across formats under both protocols; reloc_bench
# builds (default and RELOC_IR_COUNT) and --mode verify agrees across sides; the formal
# driver builds (default and SC_IR_COUNT), its contracts pass, fixtures S and M
# equal the committed references, validate S/M passes and the M input wire is
# the formal candidate wire 2bf0c071...
set -euo pipefail
CXX=$1; CEREAL=$2; VALGRIND_PARENT=$3; OUT=$4
mkdir -p "$OUT"
JOBS=${JOBS:-4}
"$CXX" --version | head -n 1

echo "== manifests"
python3 bench/check_manifest.py

echo "== slg_bench"
cmake -S bench/slg -B "$OUT/slg" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
  -DSLG_WITH_XOFFSET=ON -DCEREAL_INCLUDE="$CEREAL"
cmake --build "$OUT/slg" --target slg_bench --parallel "$JOBS"
"$OUT/slg/slg_bench" --mode verify --scale S --formats xoffset,cereal
SLG_REUSE=1 "$OUT/slg/slg_bench" --mode verify --scale S --formats xoffset,cereal
cmake -S bench/slg -B "$OUT/slg-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
  -DSLG_WITH_XOFFSET=ON -DCEREAL_INCLUDE="$CEREAL" -DSLG_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
cmake --build "$OUT/slg-ir" --target slg_bench --parallel "$JOBS"

echo "== reloc_bench"
cmake -S bench/reloc -B "$OUT/reloc" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$OUT/reloc" --target reloc_bench --parallel "$JOBS"
"$OUT/reloc/reloc_bench" --mode verify
cmake -S bench/reloc -B "$OUT/reloc-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
  -DRELOC_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
cmake --build "$OUT/reloc-ir" --target reloc_bench --parallel "$JOBS"

echo "== formal harness"
cmake -S bench/formal -B "$OUT/formal" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$OUT/formal" --parallel "$JOBS"
ctest --test-dir "$OUT/formal" --output-on-failure
cmake -S bench/formal -B "$OUT/formal-ir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" \
  -DSC_IR_COUNT=ON -DCMAKE_PREFIX_PATH="$VALGRIND_PARENT"
cmake --build "$OUT/formal-ir" --target sc_xoffset --parallel "$JOBS"
rm -rf "$OUT/fixtures"
python3 -I bench/formal/scripts/make_fixtures.py --out "$OUT/fixtures"
python3 -I bench/formal/scripts/smoke.py --driver "$OUT/formal/bin/sc_xoffset" --fixtures "$OUT/fixtures"
echo "bench smoke passed ($CXX)"
