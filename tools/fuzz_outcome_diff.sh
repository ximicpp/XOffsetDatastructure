#!/usr/bin/env bash
# Nightly cross-build replay (3.2 SC-5): the same inputs through three builds
# of the replay harnesses, each with ASan+UBSan (P2996 toolchain, natively):
#
#   reflection  C++26, reflection schema backend (servers)
#   fieldlist   C++20, XOFFSET_RECORD field lists (what clients build)
#   noexc       C++26 reflection with exceptions off (SC-8, engine clients)
#
# Inputs per target (v2_wire, wire_schemas): the kept libFuzzer corpora
# (<target>.tar.gz and fieldlist-<target>.tar.gz in the corpus store),
# tests/fuzz/regressions/<target>/ and the harness's seed corpus. Each build
# replays them with XOFFSET_FUZZ_OUTCOMES set, so every input prints one line
# with each acceptor's outcome and exact error text (tests/fuzz/v2_fuzz_core.hpp).
# The field-list lines must equal the reflection lines; the no-exceptions
# lines must equal them without the raw and receive columns (R:, V:), which a
# build without exceptions does not have. A replay that aborts (sanitizer or
# harness check) or a differing line is a finding.
#
# Writes into --out: outcome-diff-<target>/ (findings.json, stats.json,
# outcome files, logs), the `outcome-diff` entry of needs.json and a
# summary.md section, so tools/fuzz_report.py covers it with
#   --expect ...,outcome-diff-v2_wire,outcome-diff-wire_schemas
#
# usage: tools/fuzz_outcome_diff.sh --out DIR [--corpus-dir DIR]
# Exit status: 0 when the run finished (findings or not; see needs.json),
# 2 on bad arguments.
set -uo pipefail

P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
P2996_TARBALL=${P2996_TARBALL:-/mnt/project-files/toolchains/p2996-d77eff1-linux-x86_64.tar.xz}
OUT='' CORPUS_STORE=/mnt/project-files/fuzz-nightly/corpus
TARGETS='v2_wire wire_schemas'
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --corpus-dir) CORPUS_STORE=$2; shift 2 ;;
    *) echo "usage: $0 --out DIR [--corpus-dir DIR]" >&2; exit 2 ;;
  esac
done
[ -n "$OUT" ] || { echo "usage: $0 --out DIR [--corpus-dir DIR]" >&2; exit 2; }

ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) || exit 2
cd "$ROOT" || exit 2
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd) || exit 2
WORK=build/outcome-diff-nightly
rm -rf "$WORK" && mkdir -p "$WORK"
START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
RESULT=failure

finish() {
  python3 - "$OUT" "$RESULT" "$START" "$TARGETS" <<'PY'
import datetime, json, sys
from pathlib import Path
out, result, start, targets = sys.argv[1:]
out = Path(out)
needs_path = out / 'needs.json'
needs = json.loads(needs_path.read_text()) if needs_path.is_file() else {}
needs['outcome-diff'] = {'result': result}
needs_path.write_text(json.dumps(needs) + '\n')
end = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
lines = ['', '## Cross-build replay (reflection / field list / no exceptions, ASan+UBSan)', '',
         '- %s to %s' % (start, end)]
for t in targets.split():
    stats_path = out / ('outcome-diff-' + t) / 'stats.json'
    stats = json.loads(stats_path.read_text()) if stats_path.is_file() else {}
    found = stats.get('findings')
    lines.append('- outcome-diff-%s: %s, %s inputs (%s accepted), field list %s, no exceptions %s' % (
        t, 'did not finish' if found is None else '%d finding(s)' % found, stats.get('inputs', '?'),
        stats.get('accepted', '?'), stats.get('fieldlist', '?'), stats.get('noexc', '?')))
with open(out / 'summary.md', 'a') as fh:
    fh.write('\n'.join(lines) + '\n')
print('\n'.join(lines))
PY
}
trap finish EXIT

echo "== outcome diff $START, $(git rev-parse --short HEAD)"
if [ ! -x "$P2996_ROOT/bin/clang++" ]; then
  { (cd "$(dirname "$P2996_TARBALL")" && sha256sum -c "$(basename "$P2996_TARBALL").sha256") &&
    mkdir -p "$(dirname "$P2996_ROOT")" && tar -xJf "$P2996_TARBALL" -C "$(dirname "$P2996_ROOT")"; } > "$OUT/outcome-diff-toolchain.log" 2>&1 ||
    { echo "P2996 toolchain unavailable, see outcome-diff-toolchain.log"; exit 0; }
fi
BOOST_LIBS=$(grep -o 'external/boost/libs/[^|]*' cmake/XOffsetDependencyLock.cmake | sed 's#^external/boost/##; s#/include$##' | sort -u)
{ git submodule update --init --force external/typelayout external/boost &&
  git -C external/boost submodule update --init --force $BOOST_LIBS; } > "$OUT/outcome-diff-submodules.log" 2>&1 ||
  { echo "submodule checkout failed, see outcome-diff-submodules.log"; exit 0; }
if [ ! -x build/cmake-tool/root/bin/cmake ]; then
  rm -rf build/cmake-tool
  python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool > "$OUT/outcome-diff-cmake.log" 2>&1 ||
    { echo "cmake preparation failed, see outcome-diff-cmake.log"; exit 0; }
fi
CMAKE=build/cmake-tool/root/bin/cmake
SYMBOLIZER=$(command -v llvm-symbolizer || true)
SAN=halt_on_error=1${SYMBOLIZER:+:external_symbolizer_path=$SYMBOLIZER}
export ASAN_OPTIONS=detect_leaks=1:$SAN UBSAN_OPTIONS=print_stacktrace=1:$SAN

# Two configurations, built in parallel halves: the C++26 one carries the
# reflection and the no-exceptions replays.
build() { # <dir> <C++ standard> <targets...>
  local dir=$1 std=$2; shift 2
  "$CMAKE" -S . -B "$dir" -DCMAKE_BUILD_TYPE=Debug -DXOFFSET_CXX_STANDARD="$std" \
    -DCMAKE_CXX_COMPILER="$P2996_ROOT/bin/clang++" \
    "-DCMAKE_CXX_FLAGS=-O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -ffile-prefix-map=$ROOT=/workspace" \
    -DCMAKE_BUILD_RPATH="$P2996_ROOT/lib/x86_64-unknown-linux-gnu" \
    -DXOFFSET_BUILD_EXAMPLES=OFF -DXOFFSET_BUILD_TOOLS=OFF &&
  "$CMAKE" --build "$dir" --parallel "$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 ))" --target "$@"
}
build build/outcomes-cxx26 26 fuzz_v2_wire_replay fuzz_wire_schemas_replay fuzz_v2_wire_replay_noexc \
  fuzz_wire_schemas_replay_noexc > "$OUT/outcome-diff-build-cxx26.log" 2>&1 & B26=$!
build build/outcomes-cxx20 20 fuzz_v2_wire_replay fuzz_wire_schemas_replay > "$OUT/outcome-diff-build-cxx20.log" 2>&1 & B20=$!
wait $B26; S26=$?; wait $B20; S20=$?
if [ $S26 != 0 ] || [ $S20 != 0 ]; then
  echo "cross-build replay build failed, see outcome-diff-build-cxx26.log / -cxx20.log"
  tail -n 20 "$OUT/outcome-diff-build-cxx26.log" "$OUT/outcome-diff-build-cxx20.log"
  exit 0
fi

ALL_OK=1
for T in $TARGETS; do
  R="$OUT/outcome-diff-$T" W="$WORK/$T"
  mkdir -p "$R" "$W/inputs"
  for store in "$CORPUS_STORE/$T.tar.gz" "$CORPUS_STORE/fieldlist-$T.tar.gz"; do
    [ -f "$store" ] && tar -xzf "$store" -C "$W/inputs" 2>> "$R/corpus-restore.log"
  done
  build/outcomes-cxx26/bin/Debug/fuzz_${T}_replay --write-seeds "$W/seeds" > "$R/seeds.log" 2>&1
  declare -A BIN=([reflection]=build/outcomes-cxx26/bin/Debug/fuzz_${T}_replay
                  [fieldlist]=build/outcomes-cxx20/bin/Debug/fuzz_${T}_replay
                  [noexc]=build/outcomes-cxx26/bin/Debug/fuzz_${T}_replay_noexc)
  PIDS=()
  for B in reflection fieldlist noexc; do
    XOFFSET_FUZZ_OUTCOMES=1 "${BIN[$B]}" "$W/inputs" "tests/fuzz/regressions/$T" "$W/seeds" \
      > "$R/$B.outcomes" 2> "$R/$B.log" &
    PIDS+=($!)
  done
  EXITS=()
  for P in "${PIDS[@]}"; do wait "$P"; EXITS+=($?); done
  python3 - "$R" "$T" "${EXITS[@]}" <<'PY'
import hashlib, json, re, shutil, sys
from pathlib import Path
out, target, *exits = sys.argv[1:]
out = Path(out)
builds = ['reflection', 'fieldlist', 'noexc']
exit_of = dict(zip(builds, exits))
findings, notes = [], {}

def signature(*parts):
    return hashlib.sha1(':'.join(parts).encode()).hexdigest()[:10]

def outcomes(build):
    return [line.rstrip('\n').split('\t') for line in (out / (build + '.outcomes')).read_text().splitlines()
            if line.startswith('outcome\t')]

def inputs(build):
    return re.findall(r'^replay: (.+) \(\d+ bytes\)$', (out / (build + '.log')).read_text(), re.M)

def keep(path, name):
    try:
        shutil.copyfile(path, out / name)
        return name
    except OSError:
        return None

# A replay that did not finish stopped at its last named input.
for b in builds:
    if exit_of[b] == '0':
        continue
    log = (out / (b + '.log')).read_text(errors='replace')
    named = inputs(b)
    summary = next((l for l in log.splitlines() if l.startswith('SUMMARY:')), None) or \
        next((l for l in log.splitlines() if 'FAILED' in l), 'exit ' + exit_of[b])
    kind = re.sub(r'\s+', ' ', summary)[:120]
    frame = re.search(r'#\d+ 0x[0-9a-f]+ in (\S+)', log)
    sig = signature('crash', target, b, re.sub(r'0x[0-9a-f]+|/[^ ]*', '', kind), frame.group(1) if frame else '')
    findings.append({'signature': sig, 'kind': 'crash', 'error_type': '%s build: %s' % (b, kind),
                     'top_function': frame.group(1) if frame else None,
                     'log_head': '\n'.join(log.splitlines()[-60:]),
                     'input': keep(named[-1], '%s-%s' % (target, sig)) if named else None,
                     'replay_cmd': 'XOFFSET_FUZZ_OUTCOMES=1 fuzz_%s_replay%s <input>' % (target, '_noexc' if b == 'noexc' else '')})

stats = {'target': target, 'fuzzer_exit': '0', 'exits': exit_of}
if all(exit_of[b] == '0' for b in builds):
    ref, names = outcomes('reflection'), inputs('reflection')
    stats['inputs'] = len(ref)
    stats['accepted'] = sum(1 for l in ref if len(l) > 2 and l[2] == 'T:ok')
    def strip_raw(line):
        return [f for f in line if not f.startswith(('R:', 'V:'))]
    for b, view in (('fieldlist', lambda l: l), ('noexc', strip_raw)):
        other = outcomes(b)
        if len(other) != len(ref):
            stats[b] = 'line count %d vs %d' % (len(other), len(ref))
            findings.append({'signature': signature('count', target, b), 'kind': 'differential',
                             'error_type': '%s build printed %d outcome lines, reflection %d' % (b, len(other), len(ref)),
                             'top_function': None, 'log_head': '', 'input': None, 'replay_cmd': ''})
            continue
        diffs = [i for i, (x, y) in enumerate(zip(ref, other)) if view(x) != y]
        stats[b] = 'identical' if not diffs else '%d differing' % len(diffs)
        if diffs:
            i = diffs[0]
            field = next((f.split(':', 1)[0] for f, g in zip(view(ref[i]), other[i]) if f != g), 'line')
            findings.append({'signature': signature('differential', target, b, field), 'kind': 'differential',
                             'error_type': 'outcome differs, reflection vs %s build (%s)' % (b, field),
                             'top_function': None,
                             'log_head': '%d of %d inputs differ; first:\nreflection: %s\n%s: %s' % (
                                 len(diffs), len(ref), '\t'.join(ref[i]), b, '\t'.join(other[i])),
                             'input': keep(names[i], '%s-diff-%s' % (target, b)) if i < len(names) else None,
                             'replay_cmd': 'XOFFSET_FUZZ_OUTCOMES=1 fuzz_%s_replay <input> in both builds' % target})
stats['findings'] = len(findings)
json.dump({'target': target, 'job': 'outcome-diff-' + target, 'needs': 'outcome-diff',
           'artifact': 'outcome-diff-' + target, 'findings': findings}, open(out / 'findings.json', 'w'), indent=2)
json.dump(stats, open(out / 'stats.json', 'w'), indent=2)
print('== outcome-diff-%s: %s inputs, field list %s, no exceptions %s, %d finding(s)' % (
    target, stats.get('inputs', '?'), stats.get('fieldlist', '?'), stats.get('noexc', '?'), len(findings)))
sys.exit(1 if findings else 0)
PY
  [ $? = 0 ] || ALL_OK=0
  unset BIN
done
[ "$ALL_OK" = 1 ] && RESULT=success
echo "== outcome-diff: $RESULT"
exit 0
