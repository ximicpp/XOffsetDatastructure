#!/bin/bash
# Builds the HFT demo (phase 1) and runs every case on one ITCH window.
#
#   bench/hft/run.sh --feed FILE [--out DIR] [--time] [--ir] [--reps N]
#                    [--max-updates N] [--jobs N] [--cpu N]
#
# FILE is a window cut by tools/itch_extract (.itch) or its cache (.feed,
# written next to the outputs on the first run). --time (default) runs the
# timed binary REPS times, one process per case, pinned to CPU; --ir counts
# instructions with callgrind, JOBS processes at a time. Both write JSON lines
# to DIR; tools/summarize.py turns them into tables and checks that every
# model and format reaches the same digest. Record only; not a gate.
#
# Dependencies (paths by environment variable):
#   P2996_ROOT        P2996 Clang + libc++            (/opt/p2996-toolchain)
#   CISTA_INCLUDE     cista v0.16 include/            (/home/user/deps/cista/include)
#   ZPP_BITS_INCLUDE  dir with zpp_bits.h (v4.7.6)    (/home/user/deps/zpp_bits)
#   FLATBUFFERS_ROOT  flatbuffers v25.12.19 checkout, include/ and build/flatc
#                                                     (/home/user/deps/flatbuffers)
#   SBE_JAR           sbe-all 1.41.0 jar (needs java) (/home/user/deps/sbe-all-1.41.0.jar)
set -euo pipefail
cd "$(dirname "$0")/../.."
P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
DEPS=${DEPS:-/home/user/deps}
CISTA_INCLUDE=${CISTA_INCLUDE:-$DEPS/cista/include}
ZPP_BITS_INCLUDE=${ZPP_BITS_INCLUDE:-$DEPS/zpp_bits}
FLATBUFFERS_ROOT=${FLATBUFFERS_ROOT:-$DEPS/flatbuffers}
FLATC=${FLATC:-$FLATBUFFERS_ROOT/build/flatc}
SBE_JAR=${SBE_JAR:-$DEPS/sbe-all-1.41.0.jar}
OUT=build/hft FEED="" TIME=0 IR=0 REPS=3 CUT="" JOBS=4 CPU=2
while [ $# -gt 0 ]; do
  case "$1" in
    --feed) FEED=$2; shift 2 ;;
    --out) OUT=$2; shift 2 ;;
    --time) TIME=1; shift ;;
    --ir) IR=1; shift ;;
    --reps) REPS=$2; shift 2 ;;
    --max-updates) CUT="--max-updates $2"; shift 2 ;;
    --jobs) JOBS=$2; shift 2 ;;
    --cpu) CPU=$2; shift 2 ;;
    *) sed -n '2,25p' "$0" >&2; exit 2 ;;
  esac
done
[ -n "$FEED" ] || { echo "--feed is required" >&2; exit 2; }
[ "$TIME$IR" = 00 ] && TIME=1
mkdir -p "$OUT/gen"

# Generated code (not committed): FlatBuffers and SBE codecs.
"$FLATC" --cpp -o "$OUT/gen" bench/hft/schema/hft.fbs
rm -rf "$OUT/gen/hft_sbe"
java -Dsbe.output.dir="$OUT/gen" -Dsbe.target.language=Cpp -Dsbe.validation.stop.on.error=true \
  -jar "$SBE_JAR" bench/hft/schema/hft-sbe.xml 2>&1 | grep -v '^Picked up' || true
[ -f "$OUT/gen/hft_sbe/Snapshot.h" ] || { echo "SBE code generation failed" >&2; exit 1; }

BOOST=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sort -u | sed 's/^/-isystem /')
build() {  # build OUTPUT [flags...]
  local out=$1; shift
  "$P2996_ROOT/bin/clang++" -O2 -DNDEBUG -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ "$@" \
    -I. -Ibench/hft/include -I"$OUT/gen" -isystem "$CISTA_INCLUDE" -isystem "$ZPP_BITS_INCLUDE" \
    -isystem "$FLATBUFFERS_ROOT/include" $BOOST bench/hft/hft_bench.cpp -o "$out" \
    -L"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib/x86_64-unknown-linux-gnu"
}
build "$OUT/hft_bench"
[ "$IR" = 1 ] && build "$OUT/hft_bench_ir" -DHFT_IR -g

# The normalized window, cached.
case "$FEED" in
  *.feed) ;;
  *) C="$OUT/$(basename "${FEED%.*}").feed"
     [ -f "$C" ] || "$OUT/hft_bench" --feed "$FEED" --case prepare --out "$C" > "$OUT/prepare.json"
     FEED=$C ;;
esac

STATE="pool std xo cista"
HOT="raw fixed sbe xo"
JOIN="xo xo_verified pool cista cista_checked zpp fb fb_verified sbe"

"$OUT/hft_bench" --case handshake > "$OUT/handshake.json"

if [ "$TIME" = 1 ]; then
  PIN=""
  command -v taskset > /dev/null && PIN="taskset -c $CPU"
  : > "$OUT/time.jsonl"
  for r in $(seq "$REPS"); do
    for m in $STATE; do $PIN "$OUT/hft_bench" --feed "$FEED" $CUT --case state --model "$m" >> "$OUT/time.jsonl"; done
    for f in $HOT; do $PIN "$OUT/hft_bench" --feed "$FEED" $CUT --case hot --format "$f" >> "$OUT/time.jsonl"; done
    for f in $JOIN; do $PIN "$OUT/hft_bench" --feed "$FEED" $CUT --case join --format "$f" >> "$OUT/time.jsonl"; done
  done
fi

if [ "$IR" = 1 ]; then
  # One callgrind process per (case, name, phase); join counts its three
  # phases in one process (--ir all dumps each region on its own).
  mkdir -p "$OUT/cg"
  cg() {  # cg CASE NAME PHASE args...
    local c=$1 n=$2 ph=$3; shift 3
    local f="$OUT/cg/$c.$n.$ph"
    rm -f "$f" "$f".*
    # Same glibc setting as tools/perf_gate (rep movsb/stosb counted per byte otherwise).
    GLIBC_TUNABLES=glibc.cpu.x86_rep_movsb_threshold=0x7fffffffffffffff:glibc.cpu.x86_rep_stosb_threshold=0x7fffffffffffffff \
      valgrind --tool=callgrind --collect-atstart=no --callgrind-out-file="$f" \
      "$OUT/hft_bench_ir" --feed "$FEED" $CUT "$@" --ir "$ph" > "$f.json" 2> "$f.log"
    local ir
    for g in "$f" "$f".*; do
      [ -f "$g" ] || continue
      ir=$(awk '/^summary:/ {print $2}' "$g")
      local p=$ph
      [ "$ph" = all ] && p=$(awk -F': ' '/^desc: Trigger: Client Request:/ {print $4}' "$g")
      [ -n "$ir" ] && [ -n "$p" ] && printf '{"ir_case":"%s","name":"%s","phase":"%s","ir":%s,"run":%s}\n' \
        "$c" "$n" "$p" "$ir" "$(cat "$f.json")"
    done
    return 0
  }
  export -f cg
  export OUT FEED CUT
  {
    for m in $STATE; do echo "state $m apply --case state --model $m"; done
    for f in $HOT; do echo "hot $f encode --case hot --format $f"; echo "hot $f read --case hot --format $f"; done
    for f in $JOIN; do echo "join $f all --case join --format $f"; done
  } | xargs -P "$JOBS" -L 1 bash -c 'cg "$@"' _ > "$OUT/ir.jsonl"
fi

python3 bench/hft/tools/summarize.py "$OUT"
