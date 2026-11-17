#!/usr/bin/env bash
# One command: build the toolchain image, build the premises sweep, run it pinned, summarize.
#   bench/premises/run.sh <out-dir>
# Env: RUNS=31 (processes; each runs every cell once, in its own random order), CPU=2
#      (docker --cpuset-cpus), SLG=1 also runs the SLG demo cases with every rival and both
#      tiers (SLG_BATCHES=8 batches x 21 cycles, M scale, the scorecard's seed).
# Needs git, python3 and docker with network for the first build (the image is
# bench/cppcon2024/Dockerfile: GCC 16.2 + Debian protobuf, FlatBuffers, Cap'n Proto).
set -euo pipefail
OUT=$(mkdir -p "${1:?usage: run.sh <out-dir>}" && cd "$1" && pwd)
RUNS=${RUNS:-31}; CPU=${CPU:-2}; SLG=${SLG:-0}; SLG_BATCHES=${SLG_BATCHES:-8}
HERE=$(cd "$(dirname "$0")" && pwd); SRC=$(cd "$HERE/../.." && pwd)
IMAGE=xo-cppcon24
log() { echo "[premises $(date -u +%H:%M:%SZ)] $*" >&2; }

log "image $IMAGE"
docker build -q -t "$IMAGE" "$SRC/bench/cppcon2024" > /dev/null

log "rivals cloned on the host"
mkdir -p "$OUT/deps"
while read -r name url commit; do
  [[ -d "$OUT/deps/$name/.git" ]] || git clone -q "$url" "$OUT/deps/$name"
  git -C "$OUT/deps/$name" -c advice.detachedHead=false checkout -q "$commit"
done <<'DEPS'
zpp_bits https://github.com/eyalz800/zpp_bits 4a57ecd68a481c2cd6e7ee94aaf83282a735fded
cista https://github.com/felixguendling/cista 015b94e66c4c0d7c1ef29742383a024c84fbbba1
cereal https://github.com/USCiLab/cereal ebef1e929807629befafbb2918ea1a08c7194554
DEPS

log "build"
mkdir -p "$OUT/bin" "$OUT/raw"
docker run --rm --network none -v "$SRC":/src:ro -v "$OUT/deps":/deps:ro -v "$OUT/bin":/out "$IMAGE" bash -c '
  set -e; mkdir -p /out/gen && cd /out/gen
  flatc --cpp -o . /src/bench/cppcon2024/schema/character.fbs
  cp /src/bench/cppcon2024/schema/character.capnp . && capnp compile -oc++ character.capnp
  protoc --proto_path=/src/bench/cppcon2024/schema --cpp_out=. /src/bench/cppcon2024/schema/character.proto
  g++ -std=c++26 -freflection -O3 -DNDEBUG -I/src -I/src/bench/cppcon2024 -I/out/gen \
    -isystem /deps/zpp_bits -isystem /deps/cista/include -isystem /deps/cereal/include \
    /src/bench/premises/premises_bench.cpp character.capnp.c++ character.pb.cc -o /out/premises_bench \
    -lprotobuf -lcapnp -lkj -lzstd -pthread
  if [ "'"$SLG"'" = 1 ]; then
    protoc --cpp_out=. -I /src/bench/slg/schema /src/bench/slg/schema/slg.proto
    flatc --cpp -o . /src/bench/slg/schema/slg.fbs
    g++ -std=c++26 -freflection -O3 -DNDEBUG -DSLG_WITH_XOFFSET=1 -DSLG_WITH_ZPP_BITS=1 -DSLG_WITH_CISTA=1 \
      -DSLG_WITH_PROTOBUF=1 -DSLG_WITH_FLATBUFFERS=1 -DSLG_WITH_CEREAL=1 \
      -I/src -I/src/bench/slg/include -I/src/bench/slg/adapters -I/src/bench/slg/bench -I/out/gen \
      -isystem /deps/zpp_bits -isystem /deps/cista/include -isystem /deps/cereal/include \
      /src/bench/slg/bench/slg_bench.cpp slg.pb.cc -o /out/slg_bench_all -pthread -lprotobuf
  fi
  g++ --version | head -1 > /out/toolchain.txt'
git -C "$SRC" rev-parse HEAD > "$OUT/bin/commit.txt"

for p in $(seq 0 $((RUNS - 1))); do
  log "sweep process $p/$RUNS on cpu $CPU"
  docker run --rm --network none --cpuset-cpus="$CPU" -v "$OUT/bin":/b:ro "$IMAGE" /b/premises_bench --proc "$p" \
    > "$(printf '%s/raw/p%02d.jsonl' "$OUT" "$p")" 2> /dev/null
done
python3 "$HERE/analysis/sweep_analyze.py" "$OUT/raw" "$OUT/sweep.json" > "$OUT/summary.md"
python3 "$HERE/analysis/sweep_extra.py" "$OUT/raw" >> "$OUT/summary.md"

if [[ $SLG == 1 ]]; then
  FORMATS=(xoffset_trusted xoffset zpp_bits cista_unchecked cista protobuf flatbuffers flatbuffers_noverify cereal)
  mkdir -p "$OUT/slg"
  for b in $(seq 1 "$SLG_BATCHES"); do
    n=${#FORMATS[@]}
    for i in $(seq 0 $((n - 1))); do   # formats rotated per batch
      f=${FORMATS[$(( (i + b - 1) % n ))]}
      log "slg batch $b $f"
      docker run --rm --network none --cpuset-cpus="$CPU" -e SLG_REUSE=1 -e SLG_CASES=demo -e SLG_NATIVE_OFF=1 \
        -v "$OUT/bin":/b:ro -v "$OUT/slg":/out "$IMAGE" /b/slg_bench_all --mode bench --scale M --cycles 21 \
        --seed 20261021 --formats "$f" --out "/out/head_${f}_${b}.jsonl" > /dev/null 2>&1
    done
  done
  { python3 "$HERE/analysis/slg_analyze.py" "$OUT/slg" --xo xoffset_trusted \
      --rivals zpp_bits,cista_unchecked,flatbuffers_noverify,protobuf,cereal --title "Trusted tier: xoffset_trusted"
    python3 "$HERE/analysis/slg_analyze.py" "$OUT/slg" --xo xoffset \
      --rivals zpp_bits,cista,flatbuffers,protobuf,cereal --title "Checked tier: xoffset (reload_verified)"; } > "$OUT/slg_summary.md"
fi
cat "$OUT/summary.md"
