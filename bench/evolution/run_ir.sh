#!/bin/bash
# Builds bench/evolution/evolution_ir.cpp with the P2996 toolchain and prints
# instructions per message for every scenario and operation (callgrind).
# Record only; not a gate.
#
#   bench/evolution/run_ir.sh [--out DIR] [--iterations N]
set -euo pipefail
cd "$(dirname "$0")/../.."
P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
OUT=build/evolution-ir N=20
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --iterations) N=$2; shift 2 ;;
    *) echo "usage: $0 [--out DIR] [--iterations N]" >&2; exit 2 ;;
  esac
done
mkdir -p "$OUT"
BOOST=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sort -u | sed 's/^/-isystem /')
"$P2996_ROOT/bin/clang++" -O3 -DNDEBUG -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ \
  -Wno-nontrivial-memcall -I. $BOOST -isystem external/typelayout/include -Ibench/slg -Ibench/slg/include \
  bench/evolution/evolution_ir.cpp -o "$OUT/evolution_ir" \
  -L"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib" -Wl,-rpath,"$P2996_ROOT/lib/x86_64-unknown-linux-gnu"

printf '%-9s %-10s %14s  %s\n' scenario op Ir/message info
for S in A B D; do
  for OP in recv_same load_old recv_old send_same send_old; do
    F="$OUT/callgrind.$S.$OP"
    # Same glibc setting as tools/perf_gate (rep movsb/stosb counted per byte otherwise).
    INFO=$(GLIBC_TUNABLES=glibc.cpu.x86_rep_movsb_threshold=0x7fffffffffffffff:glibc.cpu.x86_rep_stosb_threshold=0x7fffffffffffffff valgrind --tool=callgrind --collect-atstart=no --callgrind-out-file="$F" \
      "$OUT/evolution_ir" "$S" "$OP" "$N" 2> "$F.log")
    IR=$(awk '/^summary:/ {print $2}' "$F")
    printf '%-9s %-10s %14s  %s\n' "$S" "$OP" "$((IR / N))" "$INFO"
  done
done
