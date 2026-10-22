#!/bin/bash
# H1 gates V3-V6 and timing on one Linux machine, inside the runtime container (gcc:16 image,
# glibc malloc) where both binaries run. Used by perf-gcc16.yml; the Mac VM steps call the same
# xc_ab.py commands.
#
#   tools/perf/h1_linux_batch.sh <work>
#
# Environment:
#   GCC_OUT    h1_build.sh output for GCC (required)
#   CLANG_OUT  h1_build.sh output for clang-p2996; empty = GCC-only batch (Q1 and A/A: A = GCC,
#              B = a second byte-identical copy of the GCC binary)
#   O2_OUT     optional GCC -O2 build for the secondary -O2 vs -O3 comparison
#   PAIRS (16) PAIRS_SEC (8) PAIRS_O2 (8) AA_PAIRS (3) AA_RANGE (0.05) DRIFT_RANGE (0.05)
#   PIN_CPU (2) QUIET_S (60) LOAD_MAX (3)
#   CYCLES (41) SCALE (M) FORMATS (xoffset,cereal,protobuf,flatbuffers)
#   LIB_COMMIT  measured next_cpp26 commit (required; the freeze refuses library-code differences)
#   RUNTIME_IMAGE  runtime image digest, recorded in the freeze
set -euo pipefail
W=$(mkdir -p "$1" && cd "$1" && pwd)
HERE=$(cd "$(dirname "$0")" && pwd)
XC="python3 $HERE/xc_ab.py"
: "${GCC_OUT:?}"
CLANG_OUT=${CLANG_OUT:-}
PAIRS=${PAIRS:-16}; PAIRS_SEC=${PAIRS_SEC:-8}; PAIRS_O2=${PAIRS_O2:-8}; AA_PAIRS=${AA_PAIRS:-3}
AA_RANGE=${AA_RANGE:-0.05}; DRIFT_RANGE=${DRIFT_RANGE:-0.05}
PIN_CPU=${PIN_CPU:-2}; QUIET_S=${QUIET_S:-60}; LOAD_MAX=${LOAD_MAX:-3}
CYCLES=${CYCLES:-41}; SCALE=${SCALE:-M}; FORMATS=${FORMATS:-xoffset,cereal,protobuf,flatbuffers}
BENCH=(--mode bench --scale "$SCALE" --cycles "$CYCLES" --formats "$FORMATS")
GATE=(--pin-cpu "$PIN_CPU" --quiet-s "$QUIET_S" --load-max "$LOAD_MAX")

mkdir -p "$W/bin"
cp "$GCC_OUT/slg_bench" "$W/bin/gcc_slg_bench"
if [ -n "$CLANG_OUT" ]; then
  A=$W/bin/clang_slg_bench; cp "$CLANG_OUT/slg_bench" "$A"
  B=$W/bin/gcc_slg_bench; NA=clang; NB=gcc
else
  A=$W/bin/gcc_slg_bench; B=$W/bin/gcc_slg_bench_b
  cp "$A" "$B"; cmp "$A" "$B"; NA=gcc; NB=gcc-copy
fi

{
  echo "## binaries"
  for f in "$W"/bin/*; do sha256sum "$f"; done
  for f in "$A" "$B"; do echo "### ldd $f"; ldd "$f"; size "$f" || true; done
  g++ --version | head -1
  [ -n "$CLANG_OUT" ] && cat "$CLANG_OUT/compiler_version.txt" 2>/dev/null || true
  echo "## machine"; uname -a; nproc; (lscpu 2>/dev/null || true); cat /proc/loadavg
} > "$W/environment.txt" 2>&1

# V3/V4: every format agrees inside each binary; per-cycle state digests and wire tables agree across binaries.
$XC verify --bin "$A" --bin "$B" --scales S,M --protocols reuse,per_message --json "$W/verify.json" | tee "$W/verify.txt"

# V6: compile-time path decisions.
if [ -n "$CLANG_OUT" ]; then
  "$CLANG_OUT/path_audit" > "$W/path_audit_clang.txt"
  "$GCC_OUT/path_audit" > "$W/path_audit_gcc.txt"
  if diff <(grep -v '^#' "$W/path_audit_clang.txt") <(grep -v '^#' "$W/path_audit_gcc.txt") > "$W/path_audit.diff"; then
    echo "V6 path_audit: identical ($(grep -vc '^#' "$W/path_audit_gcc.txt") lines)" | tee "$W/v6.txt"
  else
    echo "V6 path_audit: DIFFERENT, see path_audit.diff" | tee "$W/v6.txt"; exit 1
  fi
else
  "$GCC_OUT/path_audit" > "$W/path_audit_gcc.txt"
  echo "V6 path_audit: GCC only ($(grep -vc '^#' "$W/path_audit_gcc.txt") lines), no clang to compare" | tee "$W/v6.txt"
fi
"$GCC_OUT/slg_strlen" --scale "$SCALE" --cycles "$CYCLES" > "$W/strlen.txt"

# Pre-registration for this machine (H1 section 5.7), written before the first timed run.
FZ=(--out "$W/FREEZE.json" --lib-commit "${LIB_COMMIT:?}" --bin "$NA=$A" --bin "$NB=$B"
    --compiler gcc="g++" --pairs "$PAIRS" --pairs-secondary "$PAIRS_SEC" --aa-pairs "$AA_PAIRS" --pin-cpu "$PIN_CPU"
    --aa-range "$AA_RANGE" --drift-range "$DRIFT_RANGE" --repeats 3
    --bench-args "${BENCH[*]}" --formats "$FORMATS" --extra "runtime_image=${RUNTIME_IMAGE:-}" --extra "platform=$(uname -m)")
[ -n "$CLANG_OUT" ] && FZ+=(--extra "clang=$(head -1 "$CLANG_OUT/compiler_version.txt")")
for f in "$HERE/xc_ab.py" "$HERE/h1_build.sh" "$HERE/h1_linux_batch.sh"; do FZ+=(--file "$(basename "$f")=$f"); done
[ -f "$HERE/../../bench/slg/MANIFEST" ] && FZ+=(--file "bench/slg/MANIFEST=$HERE/../../bench/slg/MANIFEST")
$XC freeze "${FZ[@]}"

# Primary protocol (c): SLG_REUSE=1.
$XC run --protocol xcomp --pairs "$PAIRS" --aa-pairs "$AA_PAIRS" --a "$A" --b "$B" --aa-copy "$W/bin/aa_copy" \
  --outdir "$W/primary" --env SLG_REUSE=1 "${GATE[@]}" -- "${BENCH[@]}"
$XC analyze --outdir "$W/primary" --a-name "$NA" --b-name "$NB" --aa-range "$AA_RANGE" --drift-range "$DRIFT_RANGE" \
  --md "$W/primary.md" --json "$W/primary.json" > /dev/null
$XC aa-budget --outdir "$W/primary" --budget-min 20 --json "$W/aa_budget.json" > "$W/aa_budget.md"

# Secondary protocol (a): default API (no SLG_REUSE), report only.
$XC run --protocol xcomp --pairs "$PAIRS_SEC" --aa-pairs 0 --a "$A" --b "$B" --outdir "$W/secondary" \
  "${GATE[@]}" -- "${BENCH[@]}"
$XC analyze --outdir "$W/secondary" --a-name "$NA" --b-name "$NB" --md "$W/secondary.md" --json "$W/secondary.json" > /dev/null

# Secondary: GCC -O2 vs -O3 (B = -O2).
if [ -n "${O2_OUT:-}" ]; then
  cp "$O2_OUT/slg_bench" "$W/bin/gcc_o2_slg_bench"
  $XC verify --bin "$W/bin/gcc_slg_bench" --bin "$W/bin/gcc_o2_slg_bench" --scales M --protocols reuse --json "$W/verify_o2.json"
  $XC run --protocol xcomp --pairs "$PAIRS_O2" --aa-pairs 0 --a "$W/bin/gcc_slg_bench" --b "$W/bin/gcc_o2_slg_bench" \
    --outdir "$W/o2" --env SLG_REUSE=1 "${GATE[@]}" -- "${BENCH[@]}"
  $XC analyze --outdir "$W/o2" --a-name "gcc -O3" --b-name "gcc -O2" --md "$W/o2.md" --json "$W/o2.json" > /dev/null
fi

{
  echo "# H1 Linux batch: $NB vs $NA"
  echo; echo '```'; cat "$W/verify.txt" "$W/v6.txt"; echo '```'
  echo; echo "# Primary (c) SLG_REUSE=1"; cat "$W/primary.md"
  echo; echo "# A/A budget (20 min)"; cat "$W/aa_budget.md"
  echo; echo "# Secondary (a) default API"; cat "$W/secondary.md"
  [ -f "$W/o2.md" ] && { echo; echo "# Secondary: GCC -O2 vs -O3"; cat "$W/o2.md"; }
  echo; echo "# String lengths"; echo '```'; head -8 "$W/strlen.txt"; echo '```'
} > "$W/SUMMARY.md"
echo "h1_linux_batch: done, $W/SUMMARY.md"
