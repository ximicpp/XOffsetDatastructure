#!/usr/bin/env bash
# Nightly fuzz on any Linux x86-64 host with Docker, without GitHub Actions
# (H4). Runs the two GCC 16 ASan+UBSan jobs of .github/workflows/fuzz.yml:
#
#   gcc16-diff    test_v2_fuzz in 4 processes, seeds YYYYMMDD*10+i, cases
#                 calibrated to --seconds (default 2700)
#   gcc16-replay  fuzz_<target>_replay over tests/fuzz/regressions/<target>/
#                 and each harness's seed corpus
#
# and writes into --out: <job>/findings.json (tools/fuzz_gcc16.py), logs,
# needs.json (each job's result, as the Actions `needs` context), meta.json
# and summary.md. tools/fuzz_report.py report then files the issues:
#
#   tools/fuzz_report.py --repo ximicpp/XOffsetDatastructure report \
#     --findings-dir OUT --needs-json OUT/needs.json \
#     --expect gcc16-diff,gcc16-replay --mode nightly \
#     --sha "$(git rev-parse HEAD)" --ref next_cpp26 --run-id <name> --run-url <link>
#
# --inject is the drill: builds with XOFFSET_FUZZ_INJECT_CRASH=ON and adds an
# XOFFSET-INJECT input to the v2_wire replay, which must come back as one
# finding. The libFuzzer jobs need P2996 Clang and are not run here.
#
# Exit status: 0 when the run finished (findings or not; see needs.json),
# 2 on bad arguments.
set -uo pipefail

# Same image and sanitizer flags as the gcc16 jobs in ci.yml and fuzz.yml.
IMAGE=gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240
export XOFFSET_GCC_SANITIZE='-fsanitize=address,undefined -fno-sanitize=null,nonnull-attribute,returns-nonnull-attribute -fno-sanitize-recover=undefined -fno-omit-frame-pointer'

OUT='' SECS=2700 INJECT=OFF
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --seconds) SECS=$2; shift 2 ;;
    --inject) INJECT=ON; shift ;;
    *) echo "usage: $0 --out DIR [--seconds N] [--inject]" >&2; exit 2 ;;
  esac
done
if [ -z "$OUT" ] || ! [[ "$SECS" =~ ^[0-9]+$ ]] || [ "$SECS" -lt 10 ]; then
  echo "usage: $0 --out DIR [--seconds N>=10] [--inject]" >&2; exit 2
fi

ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) || exit 2
cd "$ROOT" || exit 2
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd) || exit 2
WORK=build/fuzz-nightly
rm -rf "$WORK" && mkdir -p "$WORK/diff/logs" "$WORK/replay" build
START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
DAY=$(date -u +%Y%m%d)
SEEDS="$((DAY * 10)) $((DAY * 10 + 1)) $((DAY * 10 + 2)) $((DAY * 10 + 3))"
DIFF=failure REPLAY=failure CASES=0

write_wrapper() {
cat > build/gcc-docker <<EOF
#!/bin/bash
exec docker run --rm --platform linux/amd64 \\
  -v "$ROOT:/workspace" -w /workspace \\
  -e PATH=/workspace/build/cmake-tool/root/bin:/usr/local/bin:/usr/bin:/bin \\
  -e XOFFSET_GCC_SANITIZE -e XOFFSET_FUZZ_DUMP_DIR \\
  -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:handle_abort=1 \\
  -e UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \\
  "$1" "\$@"
EOF
chmod +x build/gcc-docker
}

finish() {
  printf '{"gcc16-diff": {"result": "%s"}, "gcc16-replay": {"result": "%s"}}\n' "$DIFF" "$REPLAY" > "$OUT/needs.json"
  python3 - "$OUT" "$START" "$(git rev-parse HEAD)" "$SECS" "$SEEDS" "$CASES" "$INJECT" "$IMAGE" <<'PY'
import datetime, json, sys
from pathlib import Path
out, start, sha, secs, seeds, cases, inject, image = sys.argv[1:]
needs = json.loads((Path(out) / 'needs.json').read_text())
meta = {'start': start, 'end': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
        'sha': sha, 'seconds': int(secs), 'seeds': seeds.split(), 'cases_per_process': int(cases),
        'inject': inject == 'ON', 'image': image, 'jobs': {k: v['result'] for k, v in needs.items()}}
lines = ['# Nightly fuzz %s' % start[:10], '', '- next_cpp26 `%s`, %s to %s' % (sha[:10], start, meta['end']),
         '- GCC 16 ASan+UBSan, %s s, seeds %s, %s cases per process%s' % (secs, seeds, cases, ', **inject drill**' if meta['inject'] else '')]
for job in ('gcc16-diff', 'gcc16-replay'):
    path = Path(out) / job / 'findings.json'
    found = json.loads(path.read_text())['findings'] if path.is_file() else None
    meta['jobs'][job + '_findings'] = None if found is None else len(found)
    lines.append('- %s: %s, %s' % (job, needs[job]['result'],
                 'no findings.json' if found is None else '%d finding(s)' % len(found)))
    for f in found or []:
        lines.append('  - `%s` %s: %s in `%s`' % (f['signature'], f['target'], f['error_type'], f.get('top_function') or '?'))
(Path(out) / 'meta.json').write_text(json.dumps(meta, indent=2) + '\n')
(Path(out) / 'summary.md').write_text('\n'.join(lines) + '\n')
print('\n'.join(lines))
PY
}
trap finish EXIT

echo "== fuzz nightly $START, $(git rev-parse --short HEAD), $SECS s, inject=$INJECT"
# TypeLayout plus only the Boost libraries the build uses (the ones locked in
# cmake/XOffsetDependencyLock.cmake), not all of Boost's submodules.
BOOST_LIBS=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sed 's#^external/boost/##; s#/include$##' | sort -u)
{ git submodule update --init --force external/typelayout external/boost &&
  git -C external/boost submodule update --init --force $BOOST_LIBS; } > "$OUT/submodules.log" 2>&1 ||
  { echo "submodule checkout failed, see submodules.log"; exit 0; }
# Docker Hub rate-limits shared egress addresses; mirror.gcr.io serves the
# same digest. Findings keep the Docker Hub name in their commands.
RUN_IMAGE=$IMAGE
if ! docker pull --platform linux/amd64 "$IMAGE" > "$OUT/pull.log" 2>&1; then
  RUN_IMAGE=mirror.gcr.io/library/$IMAGE
  docker pull --platform linux/amd64 "$RUN_IMAGE" >> "$OUT/pull.log" 2>&1 || { echo "docker pull failed, see pull.log"; exit 0; }
fi
write_wrapper "$RUN_IMAGE"
if [ ! -x build/cmake-tool/root/bin/cmake ]; then
  rm -rf build/cmake-tool
  python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool > "$OUT/cmake.log" 2>&1 ||
    { echo "cmake preparation failed, see cmake.log"; exit 0; }
fi

# One build for both jobs; the cache line keeps a drill build from leaking
# into the next nightly.
build/gcc-docker bash -euo pipefail -c "
  g++ --version | head -n 1
  cmake -S . -B build/gcc16-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ \
    \"-DCMAKE_CXX_FLAGS=-O1 \$XOFFSET_GCC_SANITIZE\" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
    -DXOFFSET_BUILD_EXAMPLES=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_FUZZ_INJECT_CRASH=$INJECT
  cmake --build build/gcc16-asan --parallel \$(nproc) --target test_v2_fuzz fuzz_v2_wire_replay fuzz_wire_schemas_replay
  for T in v2_wire wire_schemas; do
    build/gcc16-asan/bin/Debug/fuzz_\${T}_replay --write-seeds $WORK/seeds/\$T
  done" > "$OUT/build.log" 2>&1 || { echo "build failed, see build.log"; tail -n 20 "$OUT/build.log"; exit 0; }

# gcc16-diff: per-case cost from one process (2000 cases minus the fixed
# part), then 90 % for four processes sharing the cores.
BIN=build/gcc16-asan/bin/Debug
SEED=${SEEDS%% *}
t() { local a b; a=$(date +%s.%N); build/gcc-docker $BIN/test_v2_fuzz "$1" "$SEED" > "$WORK/diff/logs/calibrate-$1.log" 2>&1; b=$(date +%s.%N); echo "$a $b" | awk '{printf "%.2f", $2 - $1}'; }
T0=$(t 0); T1=$(t 2000)
CASES=$(python3 -c "import sys; t0, t1, s = map(float, sys.argv[1:]); per = max((t1 - t0) / 2000, 1e-6); print(max(1000, int((s - t0) / per * 0.9)))" "$T0" "$T1" "$SECS")
echo "== gcc16-diff: fixed ${T0}s, 2000 cases ${T1}s, $CASES cases per process, seeds $SEEDS"
LIMIT=$((SECS * 2 + 300)) i=0
for S in $SEEDS; do
  ( XOFFSET_FUZZ_DUMP_DIR=$WORK/diff/dump/p$i build/gcc-docker timeout -s KILL "$LIMIT" $BIN/test_v2_fuzz "$CASES" "$S" \
      > "$WORK/diff/logs/p$i.log" 2>&1
    echo $? > "$WORK/diff/logs/p$i.exit" ) &
  i=$((i + 1))
done
wait
mkdir -p "$OUT/gcc16-diff"
if python3 tools/fuzz_gcc16.py diff --logs "$WORK/diff/logs" --seeds "$SEEDS" --cases "$CASES" \
     --image "$IMAGE" --out "$OUT/gcc16-diff" > "$OUT/gcc16-diff/findings.log" 2>&1; then
  N=$(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))["findings"]))' "$OUT/gcc16-diff/findings.json")
  [ "$N" = 0 ] && DIFF=success
fi
cp -r "$WORK/diff/logs" "$OUT/gcc16-diff/logs"
echo "== gcc16-diff: $DIFF"

# gcc16-replay: regression inputs and seed corpora (the libFuzzer corpus is
# not available without the P2996 jobs); the drill adds one inject input.
V2_EXTRA=''
if [ "$INJECT" = ON ]; then
  mkdir -p "$WORK/drill" && printf 'XOFFSET-INJECT' > "$WORK/drill/XOFFSET-INJECT" && V2_EXTRA=":$WORK/drill"
fi
mkdir -p "$OUT/gcc16-replay"
python3 tools/fuzz_gcc16.py replay --image "$IMAGE" --out "$OUT/gcc16-replay" \
  --spec "v2_wire:$BIN/fuzz_v2_wire_replay:tests/fuzz/regressions/v2_wire:$WORK/seeds/v2_wire$V2_EXTRA" \
  --spec "wire_schemas:$BIN/fuzz_wire_schemas_replay:tests/fuzz/regressions/wire_schemas:$WORK/seeds/wire_schemas" \
  > "$OUT/gcc16-replay/findings.log" 2>&1
if [ -f "$OUT/gcc16-replay/findings.json" ]; then
  N=$(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))["findings"]))' "$OUT/gcc16-replay/findings.json")
  [ "$N" = 0 ] && REPLAY=success
fi
echo "== gcc16-replay: $REPLAY"
exit 0
