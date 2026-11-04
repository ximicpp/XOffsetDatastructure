#!/bin/bash
# Builds bench/fixedmsg/fixedmsg_bench.cpp with the P2996 toolchain and prints
# instructions per packet for every path and batch size (callgrind): encode
# (write the packet into its slot) and read (receive + read every field, per
# reader). Record only; not a gate.
#
#   bench/fixedmsg/run_ir.sh [--out DIR]
set -euo pipefail
cd "$(dirname "$0")/../.."
P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
OUT=build/fixedmsg-ir PACKETS=8192 ROUNDS=3 READERS=4
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    *) echo "usage: $0 [--out DIR]" >&2; exit 2 ;;
  esac
done
mkdir -p "$OUT"
BOOST=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sort -u | sed 's/^/-isystem /')
"$P2996_ROOT/bin/clang++" -O2 -DNDEBUG -DFIXEDMSG_IR -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ \
  -I. $BOOST bench/fixedmsg/fixedmsg_bench.cpp -o "$OUT/fixedmsg_ir" \
  -L"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib/x86_64-unknown-linux-gnu"

# The first round is warm-up; callgrind counts the other ROUNDS - 1.
N=$((PACKETS * (ROUNDS - 1)))
printf '%-3s %-10s %9s %10s %10s\n' k path bytes encode read
for K in 0 1 4 16; do
  if [ "$K" = 0 ]; then PATHS="raw fixed copy_floor xo_root"; else PATHS="raw fixed copy_floor xo"; fi
  for P in $PATHS; do
    LINE=""
    for R in encode read; do
      F="$OUT/callgrind.$P.$K.$R"
      # Same glibc setting as tools/perf_gate (rep movsb/stosb counted per byte otherwise).
      GLIBC_TUNABLES=glibc.cpu.x86_rep_movsb_threshold=0x7fffffffffffffff:glibc.cpu.x86_rep_stosb_threshold=0x7fffffffffffffff \
        valgrind --tool=callgrind --collect-atstart=no --callgrind-out-file="$F" \
        "$OUT/fixedmsg_ir" --path "$P" --k "$K" --packets "$PACKETS" --rounds "$ROUNDS" --ir "$R" > "$F.json" 2> "$F.log"
      IR=$(awk '/^summary:/ {print $2}' "$F")
      if [ "$R" = encode ]; then LINE="$((IR / N))"; else LINE="$LINE $((IR / N / READERS))"; fi
    done
    BYTES=$(grep -o '"bytes_per_packet":[0-9.]*' "$F.json" | cut -d: -f2)
    printf '%-3s %-10s %9s %10s %10s\n' "$K" "$P" "$BYTES" $LINE
  done
done
