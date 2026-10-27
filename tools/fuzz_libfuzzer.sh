#!/usr/bin/env bash
# Nightly libFuzzer on a Linux x86-64 host, without GitHub Actions or Docker
# (H4, v1.1 O5). Runs the libfuzzer job of .github/workflows/fuzz.yml natively
# with the P2996 toolchain (clang-p2996 with compiler-rt, which carries the
# libFuzzer runtime) for each target in turn:
#
#   seeds -> corpus replay (-runs=0) -> fuzz (-fork=4, --seconds per target)
#   -> merge into the kept corpus -> tools/fuzz_findings.py findings + summary
#
# The corpus lives across nights as <corpus-dir>/<target>.tar.gz (default
# /mnt/project-files/fuzz-nightly/corpus). Writes into --out:
# libfuzzer-<target>/ (findings.json, stats.json, inputs, logs) and the
# `libfuzzer` entry of needs.json, merged with what tools/fuzz_nightly.sh
# wrote there, so one tools/fuzz_report.py report covers both scripts:
#
#   tools/fuzz_report.py --repo ximicpp/XOffsetDatastructure report \
#     --findings-dir OUT --needs-json OUT/needs.json \
#     --expect gcc16-diff,gcc16-replay,libfuzzer-v2_wire,libfuzzer-wire_schemas ...
#
# --inject is the drill: builds with XOFFSET_FUZZ_INJECT_CRASH=ON, runs an
# XOFFSET-INJECT input on v2_wire (one finding expected) and keeps the corpus
# out of the store.
#
# Exit status: 0 when the run finished (findings or not; see needs.json),
# 2 on bad arguments.
set -uo pipefail

P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
P2996_TARBALL=${P2996_TARBALL:-/mnt/project-files/toolchains/p2996-d77eff1-linux-x86_64.tar.xz}
# Same limits as fuzz.yml: fuzzing, merging, replay and minimization.
FUZZ_FLAGS='-max_len=262144 -rss_limit_mb=2048 -timeout=30'
MODE_FLAGS='-fork=4 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -use_value_profile=1'

OUT='' SECS=1200 INJECT=OFF TARGETS='v2_wire wire_schemas'
CORPUS_STORE=/mnt/project-files/fuzz-nightly/corpus
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --seconds) SECS=$2; shift 2 ;;
    --corpus-dir) CORPUS_STORE=$2; shift 2 ;;
    --targets) TARGETS=${2//,/ }; shift 2 ;;
    --inject) INJECT=ON; shift ;;
    *) echo "usage: $0 --out DIR [--seconds N] [--corpus-dir DIR] [--targets a,b] [--inject]" >&2; exit 2 ;;
  esac
done
if [ -z "$OUT" ] || ! [[ "$SECS" =~ ^[0-9]+$ ]] || [ "$SECS" -lt 10 ]; then
  echo "usage: $0 --out DIR [--seconds N>=10] [--corpus-dir DIR] [--targets a,b] [--inject]" >&2; exit 2
fi
for T in $TARGETS; do
  case "$T" in v2_wire|wire_schemas) ;; *) echo "unknown target '$T'" >&2; exit 2 ;; esac
done

ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) || exit 2
cd "$ROOT" || exit 2
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd) || exit 2
WORK=build/libfuzzer-nightly
rm -rf "$WORK" && mkdir -p "$WORK" build
START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
RESULT=failure

finish() {
  python3 - "$OUT" "$RESULT" "$START" "$SECS" "$INJECT" "$TARGETS" <<'PY'
import datetime, json, sys
from pathlib import Path
out, result, start, secs, inject, targets = sys.argv[1:]
out = Path(out)
needs_path = out / 'needs.json'
needs = json.loads(needs_path.read_text()) if needs_path.is_file() else {}
needs['libfuzzer'] = {'result': result}
needs_path.write_text(json.dumps(needs) + '\n')
end = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
lines = ['', '## libFuzzer (P2996 Clang, ASan+UBSan)', '',
         '- %s to %s, %s s per target, -fork=4%s' % (start, end, secs, ', **inject drill**' if inject == 'ON' else '')]
meta = {'start': start, 'end': end, 'seconds_per_target': int(secs), 'inject': inject == 'ON', 'result': result, 'targets': {}}
for t in targets.split():
    d = out / ('libfuzzer-' + t)
    found = json.loads((d / 'findings.json').read_text())['findings'] if (d / 'findings.json').is_file() else None
    stats = json.loads((d / 'stats.json').read_text()) if (d / 'stats.json').is_file() else {}
    meta['targets'][t] = {'findings': None if found is None else len(found),
                          **{k: stats.get(k) for k in ('fuzzer_exit', 'fuzz_execs', 'corpus_files', 'corpus_cov', 'corpus_ft')}}
    lines.append('- libfuzzer-%s: %s, execs %s, corpus %s files, cov %s, exit %s' % (
        t, 'no findings.json' if found is None else '%d finding(s)' % len(found), stats.get('fuzz_execs', '?'),
        stats.get('corpus_files', '?'), stats.get('corpus_cov', '?'), stats.get('fuzzer_exit', '?')))
    for f in found or []:
        lines.append('  - `%s` %s: %s in `%s`' % (f['signature'], t, f['error_type'], f.get('top_function') or '?'))
(out / 'libfuzzer-meta.json').write_text(json.dumps(meta, indent=2) + '\n')
with open(out / 'summary.md', 'a') as fh:
    fh.write('\n'.join(lines) + '\n')
print('\n'.join(lines))
PY
}
trap finish EXIT

echo "== libFuzzer nightly $START, $(git rev-parse --short HEAD), $SECS s per target, targets: $TARGETS, inject=$INJECT"
if [ ! -x "$P2996_ROOT/bin/clang++" ]; then
  { (cd "$(dirname "$P2996_TARBALL")" && sha256sum -c "$(basename "$P2996_TARBALL").sha256") &&
    mkdir -p "$(dirname "$P2996_ROOT")" && tar -xJf "$P2996_TARBALL" -C "$(dirname "$P2996_ROOT")"; } > "$OUT/toolchain.log" 2>&1 ||
    { echo "P2996 toolchain unavailable, see toolchain.log"; exit 0; }
fi
FUZZ_RT=$(find "$P2996_ROOT/lib/clang" -name libclang_rt.fuzzer.a | head -n 1)
[ -n "$FUZZ_RT" ] || { echo "no libclang_rt.fuzzer.a under $P2996_ROOT" | tee "$OUT/toolchain.log"; exit 0; }
BOOST_LIBS=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sed 's#^external/boost/##; s#/include$##' | sort -u)
{ git submodule update --init --force external/typelayout external/boost &&
  git -C external/boost submodule update --init --force $BOOST_LIBS; } > "$OUT/submodules.log" 2>&1 ||
  { echo "submodule checkout failed, see submodules.log"; exit 0; }
if [ ! -x build/cmake-tool/root/bin/cmake ]; then
  rm -rf build/cmake-tool
  python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool > "$OUT/cmake.log" 2>&1 ||
    { echo "cmake preparation failed, see cmake.log"; exit 0; }
fi
CMAKE=build/cmake-tool/root/bin/cmake

# Fuzzers and tools/fuzz_findings.py run through this wrapper (the native
# counterpart of fuzz.yml's build/xo-docker). The build maps the checkout to
# /workspace in debug info, so stack frames and signatures match the Docker
# jobs.
SYMBOLIZER=$(command -v llvm-symbolizer || true)
SAN=halt_on_error=1${SYMBOLIZER:+:external_symbolizer_path=$SYMBOLIZER}
cat > build/xo-native <<EOF
#!/bin/bash
export ASAN_OPTIONS=detect_leaks=1:$SAN UBSAN_OPTIONS=print_stacktrace=1:$SAN
exec "\$@"
EOF
chmod +x build/xo-native

{ "$CMAKE" -S . -B build/libfuzzer -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="$P2996_ROOT/bin/clang++" \
    "-DCMAKE_CXX_FLAGS=-O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -ffile-prefix-map=$ROOT=/workspace" \
    -DCMAKE_BUILD_RPATH="$P2996_ROOT/lib/x86_64-unknown-linux-gnu" \
    -DXOFFSET_BUILD_FUZZERS=ON -DXOFFSET_BUILD_EXAMPLES=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_FUZZ_INJECT_CRASH=$INJECT &&
  "$CMAKE" --build build/libfuzzer --parallel "$(nproc)" \
    --target test_v2_fuzz fuzz_v2_wire fuzz_wire_schemas fuzz_wire_schemas_seeds; } > "$OUT/libfuzzer-build.log" 2>&1 ||
  { echo "libFuzzer build failed, see libfuzzer-build.log"; tail -n 20 "$OUT/libfuzzer-build.log"; exit 0; }
BIN=build/libfuzzer/bin/Debug

ALL_OK=1
for T in $TARGETS; do
  R="$OUT/libfuzzer-$T" W="$WORK/$T"
  FUZZER=$BIN/fuzz_$T CORPUS=$W/corpus ARTS=$W/artifacts
  mkdir -p "$R" "$W/seeds" "$W/work" "$CORPUS" "$ARTS"
  case "$T" in
    v2_wire) $BIN/test_v2_fuzz --write-corpus "$W/seeds" > "$R/seeds.log" 2>&1 ;;
    wire_schemas) $BIN/fuzz_wire_schemas_seeds "$W/seeds" > "$R/seeds.log" 2>&1 ;;
  esac
  SOURCE=none
  if [ -f "$CORPUS_STORE/$T.tar.gz" ] && tar -xzf "$CORPUS_STORE/$T.tar.gz" -C "$CORPUS" 2>> "$R/corpus-restore.log"; then
    SOURCE="store $(date -u -r "$CORPUS_STORE/$T.tar.gz" +%Y-%m-%dT%H:%MZ)"
  fi
  echo "$(find "$CORPUS" -type f | wc -l) $(find "$CORPUS" -type f -printf '%s\n' | awk '{s += $1} END {print s + 0}') $SOURCE" > "$R/before.txt"
  echo "== $T: corpus $(cat "$R/before.txt"), seeds $(find "$W/seeds" -type f | wc -l)"
  {
    if [ "$INJECT" = ON ] && [ "$T" = v2_wire ]; then
      # libFuzzer never saves an input file it is given directly, and fork
      # mode drops crashing initial-corpus inputs, so copy the drill input.
      mkdir -p "$W/inject" && printf 'XOFFSET-INJECT' > "$W/inject/XOFFSET-INJECT"
      build/xo-native $FUZZER $FUZZ_FLAGS "$W/inject/XOFFSET-INJECT" ||
        cp "$W/inject/XOFFSET-INJECT" "$ARTS/$T-crash-$(sha1sum < "$W/inject/XOFFSET-INJECT" | cut -c1-40)"
    fi
    # Fork mode also skips crashing initial-corpus inputs silently, so the
    # kept corpus and the seeds are replayed once in a single process first.
    build/xo-native $FUZZER -runs=0 $FUZZ_FLAGS -artifact_prefix="$ARTS/$T-" "$CORPUS" "$W/seeds" ||
      echo "corpus replay failed (exit $?)"
    build/xo-native timeout -s KILL $((SECS * 2 + 600)) $FUZZER -max_total_time="$SECS" $FUZZ_FLAGS $MODE_FLAGS \
      -print_final_stats=1 -artifact_prefix="$ARTS/$T-" "$W/work" "$CORPUS" "$W/seeds"
    echo $? > "$W/exit"
  } > "$R/fuzz.log" 2>&1
  echo "== $T: fuzzer exit $(cat "$W/exit")"
  build/xo-native bash -c "rm -rf '$W/merged' && mkdir -p '$W/merged' &&
    $FUZZER -merge=1 -use_value_profile=1 $FUZZ_FLAGS -artifact_prefix='$W/merge-' '$W/merged' '$CORPUS' '$W/work' '$W/seeds' &&
    rm -rf '$CORPUS' && mv '$W/merged' '$CORPUS'" > "$R/merge.log" 2>&1 || echo "== $T: merge failed, see merge.log"
  # Statistics only; an input it trips on lands in $W, not the checkout.
  build/xo-native $FUZZER -runs=0 $FUZZ_FLAGS -artifact_prefix="$W/stats-" "$CORPUS" > "$R/corpus.log" 2>&1 || true
  python3 tools/fuzz_findings.py findings --target "$T" --bin "$FUZZER" --flags="$FUZZ_FLAGS" \
    --wrapper build/xo-native --artifacts "$ARTS" --out "$R" > "$R/findings.log" 2>&1
  # Reproduce steps for this host (fuzz_report.py's default ones are for the
  # Actions artifacts and build/xo-docker).
  python3 - "$R/findings.json" "$(git rev-parse HEAD)" "$T" "$FUZZER" "$FUZZ_FLAGS" <<'PY'
import json, sys
path, sha, target, fuzzer, flags = sys.argv[1:]
try:
    report = json.load(open(path))
except (OSError, ValueError):
    sys.exit(0)
for f in report['findings']:
    name = f.get('input') or '<input>'
    f['reproduce'] = [
        f'# input: tests/fuzz/regressions/{target}/{f["signature"]} on branch fuzz-crash/{f["signature"]}, '
        f'or decode the base64 below into {name}',
        f'git checkout {sha} && git submodule update --init --recursive',
        '# tools/fuzz_libfuzzer.sh --out /tmp/lf --seconds 10 builds build/libfuzzer and build/xo-native '
        '(P2996 toolchain at /opt/p2996-toolchain), then:',
        f'build/xo-native {fuzzer} {flags} {name}',
        f'# without libFuzzer, on any compiler: fuzz_{target}_replay {name}',
    ]
json.dump(report, open(path, 'w'), indent=2)
PY
  python3 tools/fuzz_findings.py summary --target "$T" --mode nightly --seconds "$SECS" --log "$R/fuzz.log" \
    --before "$R/before.txt" --corpus "$CORPUS" --corpus-log "$R/corpus.log" --merge-log "$R/merge.log" \
    --exit-file "$W/exit" --findings "$R/findings.json" --out "$R" > "$R/summary.log" 2>&1
  N=$(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))["findings"]))' "$R/findings.json" 2>/dev/null || echo missing)
  [ "$N" = 0 ] && [ "$(cat "$W/exit")" = 0 ] || ALL_OK=0
  echo "== $T: $N finding(s)"
  # Drill corpora never go back to the store.
  if [ "$INJECT" = OFF ] && [ -n "$(ls -A "$CORPUS")" ]; then
    mkdir -p "$CORPUS_STORE" && tar -czf "$WORK/$T.tar.gz" -C "$CORPUS" . &&
      cp "$WORK/$T.tar.gz" "$CORPUS_STORE/$T.tar.gz.new" && mv "$CORPUS_STORE/$T.tar.gz.new" "$CORPUS_STORE/$T.tar.gz" ||
      echo "== $T: could not store the corpus"
  fi
done
[ "$ALL_OK" = 1 ] && RESULT=success
echo "== libfuzzer: $RESULT"
exit 0
