#!/usr/bin/env bash
# Reproduces the four feasibility probes of DESIGN_REMOVE_CPP26.md.
# usage: run_probes.sh <xoffset repo checkout with external/typelayout initialized>
# Needs: g++ / clang++ on PATH (C++20), /opt/p2996-toolchain (see
# /mnt/project-files/toolchains/README.md), optional docker for GCC 16.
set -u
R=$(cd "${1:?repo path}" && pwd); HERE=$(cd "$(dirname "$0")" && pwd); P=/opt/p2996-toolchain
TL=$R/external/typelayout/include; OUT=$(mktemp -d); cd "$OUT"; cp "$HERE"/*.hpp "$HERE"/*.cpp "$HERE"/*.py .
P26="$P/bin/clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ -Wl,-rpath,$P/lib/x86_64-unknown-linux-gnu"
INC="-I. -I$R -I$TL -I$R/bench/slg/include"
echo "== probe 1: descriptor vs reflection signature/hash/padding (P2996, real library)"
$P26 -O1 $INC probe_equiv.cpp -o probe_equiv && ./probe_equiv | tail -1
echo "== probe 2: C++20 compilers, same hashes as P2996 reflection"
for cxx in "g++ -std=c++20" "clang++ -std=c++20" "$P/bin/clang++ -std=c++20 -stdlib=libc++ -Wl,-rpath,$P/lib/x86_64-unknown-linux-gnu"; do
  $cxx -O2 -Wall -Wextra -Werror -I. -I$TL probe_portable.cpp -o pp && echo "$cxx: $(./pp | grep -c '== P2996') of 7 reference hashes equal"; done
echo "== probe 2b: incomplete / misordered / duplicated lists must not compile"
for n in 1 2 3 4 5; do for cxx in g++ clang++; do
  if $cxx -std=c++20 -fsyntax-only -DNEG=$n -I. -I$TL probe_portable.cpp 2>/dev/null; then echo "NEG=$n $cxx COMPILED (bad)"; else echo "NEG=$n $cxx rejected"; fi; done; done
echo "== probe 3: machine code of a hot walk, reflection vs descriptor (P2996)"
for o in -O2 -O3; do $P26 $o -DNDEBUG -S $INC probe_codegen.cpp -o cg$o.s && python3 cmp_asm2.py cg$o.s | tail -1; done
echo "== probe 4: reflection writes the C++20 description lines"
$P26 -O1 $INC probe_dump.cpp -o probe_dump && ./probe_dump
if command -v docker >/dev/null && docker info >/dev/null 2>&1; then
  IMG=mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240
  echo "== GCC 16 (docker): probes 1, 2, 3"
  docker run --rm -v "$R:$R" -v "$OUT:$OUT" -w "$OUT" "$IMG" bash -c "
    g++ -std=c++26 -freflection -O1 $INC probe_equiv.cpp -o pe16 && ./pe16 | tail -1
    g++ -std=c++20 -O2 -Wall -Wextra -Werror -I. -I$TL probe_portable.cpp -o pp16 && echo \"gcc16 c++20: \$(./pp16 | grep -c '== P2996') of 7\"
    for o in -O2 -O3; do g++ -std=c++26 -freflection \$o -DNDEBUG -S $INC probe_codegen.cpp -o cg16\$o.s; done"
  for o in -O2 -O3; do python3 cmp_asm2.py cg16$o.s | tail -1; done
fi
echo "work dir: $OUT"
