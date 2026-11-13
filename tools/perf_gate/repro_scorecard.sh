#!/usr/bin/env bash
# One command from a git ref to the SLG scorecard (4.0 W4, docs/RELEASING.md section 2b).
#
# usage: tools/perf_gate/repro_scorecard.sh <ref> <outdir> [batches] [cycles]
#   e.g. tools/perf_gate/repro_scorecard.sh v3.6.0 /tmp/sc-v3.6.0
#
# Needs git, python3, a running docker daemon and network access (image and
# dependency fetches). Everything that decides the numbers is pinned here:
#   - the gcc:16 image by digest (GCC 16, the one tools/local_validate.sh uses);
#   - zpp_bits v4.7.6 and cista v0.16 by commit (rivals); zstd v1.5.7 and
#     lz4 v1.10.0 by commit (the report-only compressed-bytes pass);
#   - the build: one translation unit, bench/slg/bench/slg_bench.cpp, -O3;
#   - the run: M scale, seed 20261021, SLG_REUSE=1, SLG_CASES=demo, every
#     format of a batch in a process of its own, batches interleaved, one
#     pinned core. Per batch: one SLG_NATIVE_OFF=1 process per format (tag
#     head) and one SLG_D3=1 process with native on (tag d3, the D rows) for
#     XOffset and for each rival shared encoding in SHARED (4.0 W1), when the
#     ref's bench has it.
# The bench and library come from <ref>; the scoring (scorecard.py) comes from
# the checkout this script is in, so an old tag is scored by today's rules.
#
# Writes into <outdir>:
#   SCORECARD.md     the scorecard (tools/perf_gate/scorecard.py)
#   scorecard.json   the same, machine-readable
#   env.json         commits, image digest, compiler, flags, binary sha256, CPU, times
#   raw/             one JSONL per process: head_<format>_<batch>.jsonl, d3_<format>_<batch>.jsonl
#   rawz/            the compressed build's run (zstd-3 bytes, report-only)
#   logs/            build and run logs
#   src/ deps/ bin/  the exported tree, the pinned dependencies, the binaries
#
# env CPU=2           core to pin (keep other heavy work off it)
#     FORMATS="xoffset_trusted zpp_bits cista_unchecked"
#     SHARED="zpp_bits zpp_bits_dirty"   rivals' shared encodings, run as d3 processes
#                     (4.0 W1); skipped for a ref whose bench lacks them
#     ZSTD=1          also build with SLG_WITH_COMPRESSION=1 and run ZBATCHES (default 1)
#                     batch(es) of it for zstd-3 bytes; 0 skips it (bytes then raw)
#     BASELINE=FILE   scorecard.json to diff against (tools/perf_gate/scorecard_v3.6.0.json
#                     is the v3.6.0 reproduction this script was accepted on)
#     REPO=DIR        git repository to read <ref> from (default: this checkout)
#     JOBS=N          make -j for zstd/lz4 (default 4)
#     XPORT=1         also run one SLG_XPORT=1 process per format and batch (tag x, raw/x_*)
#                     and append the transport-model columns L/B/S/SD .send+T/.recv+T
#                     (tools/perf_gate/xport_columns.py, report only) to SCORECARD.md
set -euo pipefail

REF=${1:?usage: repro_scorecard.sh <ref> <outdir> [batches] [cycles]}
OUT=$(readlink -f -m "${2:?usage: repro_scorecard.sh <ref> <outdir> [batches] [cycles]}")
NB=${3:-8}; CY=${4:-21}
CPU=${CPU:-2}; ZSTD=${ZSTD:-1}; ZBATCHES=${ZBATCHES:-1}; JOBS=${JOBS:-4}; XPORT=${XPORT:-0}
FORMATS=${FORMATS:-"xoffset_trusted zpp_bits cista_unchecked"}
SHARED=${SHARED-"zpp_bits zpp_bits_dirty"}
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=${REPO:-$(git -C "$HERE" rev-parse --show-toplevel)}
SEED=20261021; SCALE=M

IMAGE=mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240
# name url commit (tag)
DEPS=(
  "zpp_bits https://github.com/eyalz800/zpp_bits 4a57ecd68a481c2cd6e7ee94aaf83282a735fded v4.7.6"
  "cista https://github.com/felixguendling/cista 015b94e66c4c0d7c1ef29742383a024c84fbbba1 v0.16"
  "zstd https://github.com/facebook/zstd f8745da6ff1ad1e7bab384bd1f9d742439278e99 v1.5.7"
  "lz4 https://github.com/lz4/lz4 ebb370ca83af193212df4dcbadcc5d87bc0de2f0 v1.10.0"
)
CXXFLAGS="-std=c++26 -freflection -O3 -DNDEBUG -DSLG_WITH_XOFFSET=1 -DSLG_WITH_ZPP_BITS=1 -DSLG_WITH_CISTA=1"
INCLUDES="-I/src -I/src/bench/slg/include -I/src/bench/slg/adapters -I/src/bench/slg/bench -isystem /deps/zpp_bits -isystem /deps/cista/include"

log() { echo "[$(date -u +%H:%M:%SZ)] $*"; }
for tool in git python3 docker; do
  command -v "$tool" > /dev/null || { echo "repro_scorecard: $tool not found" >&2; exit 2; }
done
docker info > /dev/null 2>&1 || { echo "repro_scorecard: docker daemon not reachable (start dockerd)" >&2; exit 2; }
if [[ -e "$OUT/raw" ]] && [[ -n "$(ls -A "$OUT/raw" 2>/dev/null)" ]]; then
  echo "repro_scorecard: $OUT/raw is not empty; use a fresh <outdir>" >&2; exit 2
fi
mkdir -p "$OUT"/{src,deps,bin,raw,rawz,logs}
START=$(date -u +%Y-%m-%dT%H:%M:%SZ)

# 1. Source: the ref's tree, no submodules (the bench needs none).
if ! COMMIT=$(git -C "$REPO" rev-parse --verify -q "$REF^{commit}"); then
  log "fetching $REF"
  git -C "$REPO" fetch -q origin "+refs/tags/$REF:refs/tags/$REF" 2>/dev/null || git -C "$REPO" fetch -q origin "$REF"
  COMMIT=$(git -C "$REPO" rev-parse --verify "$REF^{commit}" 2>/dev/null || git -C "$REPO" rev-parse --verify FETCH_HEAD)
fi
log "source $REF = $COMMIT"
rm -rf "$OUT/src" && mkdir -p "$OUT/src"
git -C "$REPO" archive "$COMMIT" | tar -x -C "$OUT/src"

# 2. Pinned dependencies, each checked out at its commit and verified.
for d in "${DEPS[@]}"; do
  read -r name url commit tag <<< "$d"
  dir="$OUT/deps/$name"
  if [[ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" != "$commit" ]]; then
    rm -rf "$dir"; git init -q "$dir"
    git -C "$dir" fetch -q --depth 1 "$url" "$commit"
    git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
  fi
  got=$(git -C "$dir" rev-parse HEAD)
  [[ "$got" == "$commit" ]] || { echo "repro_scorecard: $name is $got, expected $commit" >&2; exit 3; }
  log "dep $name $tag $commit"
done

# 3. Image by digest (docker checks the digest).
docker image inspect "$IMAGE" > /dev/null 2>&1 || docker pull -q "$IMAGE" > /dev/null
in_image() { docker run --rm --network none -v "$OUT/src":/src:ro -v "$OUT/deps":/deps:ro -v "$OUT/bin":/out "$IMAGE" "$@"; }

# 4. Build: one translation unit, -O3. The heavy step runs alone (the
# container has 4 cores; nothing else should be timing on this machine).
log "build slg_bench"
in_image g++ $CXXFLAGS $INCLUDES /src/bench/slg/bench/slg_bench.cpp -o /out/slg_bench -pthread \
  > "$OUT/logs/build.log" 2>&1 || { tail -30 "$OUT/logs/build.log" >&2; exit 4; }
if [[ $ZSTD == 1 ]]; then
  log "build zstd/lz4 static libraries and slg_bench_z (SLG_WITH_COMPRESSION=1)"
  docker run --rm --network none -v "$OUT/deps":/deps -v "$OUT/bin":/out "$IMAGE" bash -c "
    set -e
    rm -rf /tmp/zstd /tmp/lz4 && cp -r /deps/zstd /tmp/zstd && cp -r /deps/lz4 /tmp/lz4
    make -s -C /tmp/zstd/lib -j$JOBS libzstd.a ZSTD_LEGACY_SUPPORT=0 > /dev/null
    make -s -C /tmp/lz4/lib -j$JOBS liblz4.a > /dev/null
    mkdir -p /out/zlib && cp /tmp/zstd/lib/libzstd.a /tmp/lz4/lib/liblz4.a /out/zlib/" \
    > "$OUT/logs/build_zlib.log" 2>&1 || { tail -30 "$OUT/logs/build_zlib.log" >&2; exit 4; }
  in_image g++ $CXXFLAGS -DSLG_WITH_COMPRESSION=1 $INCLUDES -isystem /deps/zstd/lib -isystem /deps/lz4/lib \
    /src/bench/slg/bench/slg_bench.cpp -o /out/slg_bench_z -pthread /out/zlib/libzstd.a /out/zlib/liblz4.a \
    > "$OUT/logs/build_z.log" 2>&1 || { tail -30 "$OUT/logs/build_z.log" >&2; exit 4; }
fi

# The rivals' shared encodings this ref's bench has (4.0 W1; older trees have none):
# the format must be built (its name in the binary) and have D rows (the shared
# view's own error text).
SHARED_RUN=""
for f in $SHARED; do
  if grep -aq "zpp_bits shared" "$OUT/bin/slg_bench" && grep -aq -- "$f" "$OUT/bin/slg_bench"; then SHARED_RUN="$SHARED_RUN $f"; fi
done
SHARED_RUN=${SHARED_RUN# }
log "shared-encoding rivals: ${SHARED_RUN:-none}"

# 5. Environment record.
GXX=$(docker run --rm --network none "$IMAGE" g++ --version | head -1)
LIBC=$(docker run --rm --network none "$IMAGE" ldd --version 2>&1 | head -1)
REPO_HEAD=$(git -C "$REPO" rev-parse HEAD)
export OUT REF COMMIT IMAGE GXX LIBC CXXFLAGS INCLUDES NB CY CPU SEED SCALE FORMATS ZSTD ZBATCHES START REPO_HEAD SHARED_RUN
export DEPS_TXT=$(printf '%s\n' "${DEPS[@]}")
python3 - <<'PY'
import hashlib, json, os, platform, re, subprocess
e = os.environ
def sh(cmd):
    try: return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout.strip()
    except Exception: return ""
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest() if os.path.exists(p) else None
lscpu = sh("lscpu")
field = lambda k: (re.search(rf"^{k}:\s*(.+)$", lscpu, re.M) or [None, None])[1]
deps = {}
for line in e["DEPS_TXT"].splitlines():
    name, url, commit, tag = line.split()
    deps[name] = {"url": url, "tag": tag, "commit": commit}
env = {
  "source": {"ref": e["REF"], "commit": e["COMMIT"], "scored_by": e["REPO_HEAD"]},
  "toolchain": {"image": e["IMAGE"], "compiler": e["GXX"], "libc": e["LIBC"],
                "cxxflags": e["CXXFLAGS"], "includes": e["INCLUDES"],
                "translation_unit": "bench/slg/bench/slg_bench.cpp",
                "binary_sha256": sha(os.path.join(e["OUT"], "bin/slg_bench")),
                "binary_z_sha256": sha(os.path.join(e["OUT"], "bin/slg_bench_z"))},
  "deps": deps,
  "run": {"scale": e["SCALE"], "seed": int(e["SEED"]), "batches": int(e["NB"]), "cycles": int(e["CY"]),
          "cpu": e["CPU"], "formats": e["FORMATS"].split(),
          "env_head": "SLG_REUSE=1 SLG_CASES=demo SLG_NATIVE_OFF=1",
          "env_d3": "SLG_REUSE=1 SLG_CASES=demo SLG_D3=1 (native on), XOffset and the shared rivals",
          "shared": e["SHARED_RUN"].split(),
          "order": "per batch: head process per format in FORMATS order, then the d3 processes",
          "zstd_pass": {"enabled": e["ZSTD"] == "1", "batches": int(e["ZBATCHES"])}},
  "host": {"cpu_model": field("Model name"), "nproc": os.cpu_count(), "kernel": platform.release(),
           "machine": platform.machine(), "mem_total": sh("grep MemTotal /proc/meminfo"),
           "docker": sh("docker version --format '{{.Server.Version}}'"),
           "loadavg_start": open("/proc/loadavg").read().split()[:3]},
  "time": {"start": e["START"]},
}
json.dump(env, open(os.path.join(e["OUT"], "env.json"), "w"), indent=1)
PY

# 6. Runs. Every process: fresh container, no network, one pinned core.
run() {  # bin rawdir tag format batch extra-env...
  local bin=$1 dir=$2 tag=$3 fmt=$4 b=$5; shift 5
  local envs=(-e SLG_REUSE=1 -e SLG_CASES=demo); for x in "$@"; do envs+=(-e "$x"); done
  docker run --rm --network none --cpuset-cpus="$CPU" "${envs[@]}" \
    -v "$OUT/bin":/bin_dir:ro -v "$OUT/$dir":/out "$IMAGE" \
    /bin_dir/"$bin" --mode bench --scale "$SCALE" --cycles "$CY" --seed "$SEED" \
    --formats "$fmt" --out /out/"${tag}_${fmt}_${b}.jsonl" > "$OUT/logs/${dir}_${tag}_${fmt}_${b}.log" 2>&1 \
    || { echo "repro_scorecard: run $dir/$tag/$fmt/$b failed" >&2; tail -5 "$OUT/logs/${dir}_${tag}_${fmt}_${b}.log" >&2; exit 5; }
}
batch() {  # bin rawdir batch
  for f in $FORMATS; do run "$1" "$2" head "$f" "$3" SLG_NATIVE_OFF=1; done
  for f in $FORMATS; do
    if [[ $f == xoffset* ]]; then run "$1" "$2" d3 "$f" "$3" SLG_D3=1; fi
  done
  for f in $SHARED_RUN; do run "$1" "$2" d3 "$f" "$3" SLG_D3=1; done
  if [[ $XPORT == 1 && $2 == raw ]]; then
    for f in $FORMATS; do run "$1" "$2" x "$f" "$3" SLG_NATIVE_OFF=1 SLG_XPORT=1; done
  fi
}
for b in $(seq 1 "$NB"); do
  batch slg_bench raw "$b"
  log "batch $b/$NB done"
done
if [[ $ZSTD == 1 ]]; then
  for b in $(seq 1 "$ZBATCHES"); do batch slg_bench_z rawz "$b"; done
  log "compressed-bytes pass done"
fi

# 7. Score.
END=$(date -u +%Y-%m-%dT%H:%M:%SZ)
python3 - "$OUT/env.json" "$END" <<'PY'
import json, sys
p, end = sys.argv[1], sys.argv[2]
e = json.load(open(p)); e["time"]["end"] = end
e["host"]["loadavg_end"] = open("/proc/loadavg").read().split()[:3]
json.dump(e, open(p, "w"), indent=1)
PY
ARGS=("$OUT/raw" --env "$OUT/env.json" --json "$OUT/scorecard.json" --rivals "$(echo $FORMATS | tr ' ' '\n' | grep -v '^xoffset' | paste -sd,)"
      --shared-rivals "$(echo $SHARED_RUN | tr ' ' ',')")
[[ $ZSTD == 1 ]] && ARGS+=(--bytes-dir "$OUT/rawz")
[[ -n ${BASELINE:-} ]] && ARGS+=(--baseline "$BASELINE")
python3 "$HERE/scorecard.py" "${ARGS[@]}" > "$OUT/SCORECARD.md"
if [[ $XPORT == 1 ]]; then
  { echo; python3 "$HERE/xport_columns.py" "$OUT/raw" --json "$OUT/xport.json" \
      --xo "$(echo $FORMATS | tr ' ' '\n' | grep '^xoffset' | head -1)" \
      --rivals "$(echo $FORMATS | tr ' ' '\n' | grep -v '^xoffset' | paste -sd,)"; } >> "$OUT/SCORECARD.md"
fi
cat "$OUT/SCORECARD.md"
log "done: $OUT/SCORECARD.md"
