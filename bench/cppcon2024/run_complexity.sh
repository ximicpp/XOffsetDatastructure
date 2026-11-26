#!/usr/bin/env bash
# Build and run the iso-byte structural complexity comparison across all 8 CppCon 2024 libraries.
#   bench/cppcon2024/run_complexity.sh <out-dir>
# Env: RUNS=7 (process runs, median taken per metric), CPU=2 (docker --cpuset-cpus),
#      QUICK=1 (short smoke run). Needs git, python3, docker with network for the first build.
set -euo pipefail
OUT=$(mkdir -p "${1:?usage: run_complexity.sh <out-dir>}" && cd "$1" && pwd)
RUNS=${RUNS:-7}; CPU=${CPU:-2}; QUICK=${QUICK:-0}
HERE=$(cd "$(dirname "$0")" && pwd); SRC=$(cd "$HERE/../.." && pwd)
IMAGE=xo-cppcon24
log() { echo "[cppcon2024-complexity] $*" >&2; }

log "image $IMAGE (GCC 16.2 + Debian rivals)"
docker build -q -t "$IMAGE" "$HERE" > /dev/null

log "rivals cloned on the host (zpp_bits v4.7.6, cista v0.16)"
mkdir -p "$OUT/deps"
while read -r name url commit; do
  [[ -d "$OUT/deps/$name/.git" ]] || git clone -q "$url" "$OUT/deps/$name"
  git -C "$OUT/deps/$name" checkout -q "$commit"
done <<'DEPS'
zpp_bits https://github.com/eyalz800/zpp_bits 4a57ecd68a481c2cd6e7ee94aaf83282a735fded
cista https://github.com/felixguendling/cista 015b94e66c4c0d7c1ef29742383a024c84fbbba1
DEPS

log "build"
mkdir -p "$OUT/bin"
docker run --rm --network none -v "$SRC":/src:ro -v "$OUT/deps":/deps:ro -v "$OUT/bin":/out "$IMAGE" bash -c '
  set -e; mkdir -p /out/gen && cd /out/gen
  flatc --cpp -o . /src/bench/cppcon2024/schema/character.fbs
  flatc --cpp -o . /src/bench/cppcon2024/schema/rich_character.fbs
  cp /src/bench/cppcon2024/schema/character.capnp . && capnp compile -oc++ character.capnp
  cp /src/bench/cppcon2024/schema/rich_character.capnp . && capnp compile -oc++ rich_character.capnp
  protoc --proto_path=/src/bench/cppcon2024/schema --cpp_out=. /src/bench/cppcon2024/schema/character.proto
  protoc --proto_path=/src/bench/cppcon2024/schema --cpp_out=. /src/bench/cppcon2024/schema/rich_character.proto
  g++ -std=c++26 -freflection -O3 -DNDEBUG -I/src -I/out/gen -isystem /deps/zpp_bits -isystem /deps/cista/include \
    /src/bench/cppcon2024/complexity_bench.cpp \
    character.capnp.c++ character.pb.cc rich_character.capnp.c++ rich_character.pb.cc \
    -o /out/complexity_bench -lprotobuf -lcapnp -lkj -lzstd -pthread
  { g++ --version | head -1; dpkg-query -W libprotobuf-dev libcapnp-dev libflatbuffers-dev libmsgpack-cxx-dev libzstd-dev; } > /out/toolchain.txt'
git -C "$SRC" rev-parse HEAD > "$OUT/bin/commit.txt"

ARGS=(); [[ $QUICK == 1 ]] && ARGS=(--quick)
for i in $(seq 1 "$RUNS"); do
  log "run $i/$RUNS on cpu $CPU"
  docker run --rm --network none --cpuset-cpus="$CPU" -v "$OUT/bin":/out:ro "$IMAGE" \
    /out/complexity_bench --seed "$i" "${ARGS[@]}" > "$OUT/run$i.txt"
done
python3 "$HERE/summarize_complexity.py" "$OUT"/run*.txt > "$OUT/summary.md"
cat "$OUT/summary.md"
