#!/usr/bin/env bash
# Pre-release fuzz (K4; susu 2026-11-04 06:15Z): before every release, one
# ASan+UBSan fuzz round over every receive entry point, on a Linux x86-64
# cloud host, without GitHub Actions. Nightly fuzz stays off; this replaces it.
# docs/RELEASING.md section 3 says when to run it; the entry points and the
# harness of each are listed in docs/V2_THREAT_MODEL.md section 5.
#
# Phases (each writes into its own subdirectory of --out, so they can run as
# separate tasks, in any order, and be rerun alone):
#
#   gcc       tools/fuzz_nightly.sh: GCC 16 Docker ASan+UBSan differential
#             fuzz (test_v2_fuzz, 4 processes) and replay of the regressions
#             and seed corpora of every harness
#   libfuzzer tools/fuzz_libfuzzer.sh: coverage-guided libFuzzer, P2996 Clang,
#             server build (C++26 reflection), every harness it knows
#   client    tools/fuzz_libfuzzer.sh --backend fieldlist (the C++20
#             field-list build clients use) then tools/fuzz_outcome_diff.sh
#             (every kept input through the reflection, field-list and
#             no-exceptions builds; outcomes must be identical)
#   verdict   reads the phases' needs.json files and writes VERDICT.md:
#             PASS only when every expected job finished with no finding
#   all       the four in order (about 4 h at the release budget; a cloud
#             background task is stopped after 2 h, so there run the phases
#             one per task: gcc, libfuzzer, client, then verdict)
#
# --budget release (default): gcc 2700 s, libFuzzer 1500 s per target per
# backend. --budget quick: 300 s / 120 s, a dry run of the pipeline that does
# not count for a release. The corpus store (default
# /mnt/project-files/fuzz-nightly/corpus) is reused and updated, so each
# release starts from everything earlier rounds found.
#
# Findings: tools/fuzz_report.py files them (see fuzz_nightly.sh); the fix PR
# adds the minimized input under tests/fuzz/regressions/<target>/.
#
# usage: tools/fuzz_prerelease.sh --out DIR [--phase P] [--budget release|quick]
#                                 [--corpus-dir DIR]
# Exit status: verdict and all: 0 PASS, 1 FAIL; other phases: 0 when the phase
# ran (findings or not, see its needs.json); 2 on bad arguments or a missing
# prerequisite.
set -uo pipefail

OUT='' PHASE=all BUDGET=release CORPUS_ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --phase) PHASE=$2; shift 2 ;;
    --budget) BUDGET=$2; shift 2 ;;
    --corpus-dir) CORPUS_ARGS=(--corpus-dir "$2"); shift 2 ;;
    *) echo "usage: $0 --out DIR [--phase all|gcc|libfuzzer|client|verdict] [--budget release|quick] [--corpus-dir DIR]" >&2; exit 2 ;;
  esac
done
case "$BUDGET" in
  release) GCC_SECS=2700 LF_SECS=1500 ;;
  quick) GCC_SECS=300 LF_SECS=120 ;;
  *) echo "unknown budget '$BUDGET'" >&2; exit 2 ;;
esac
case "$PHASE" in all|gcc|libfuzzer|client|verdict) ;; *) echo "unknown phase '$PHASE'" >&2; exit 2 ;; esac
[ -n "$OUT" ] || { echo "--out is required" >&2; exit 2; }

ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) || exit 2
cd "$ROOT" || exit 2
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd) || exit 2
SHA=$(git rev-parse HEAD)

# Every phase records the commit and budget it ran with; verdict refuses a
# mix of commits or budgets.
stamp() {
  mkdir -p "$OUT/$1"
  printf '{"sha": "%s", "budget": "%s", "dirty": %s, "start": "%s"}\n' "$SHA" "$BUDGET" \
    "$([ -z "$(git status --porcelain --untracked-files=no)" ] && echo false || echo true)" \
    "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$OUT/$1/phase.json"
}

run_gcc() {
  docker info > /dev/null 2>&1 || {
    echo "Docker is not running: start dockerd first (fuzz-nightly README step 1)" >&2; return 2; }
  stamp gcc
  echo "== prerelease gcc: $GCC_SECS s"
  tools/fuzz_nightly.sh --out "$OUT/gcc" --seconds "$GCC_SECS"
}

run_libfuzzer() {
  stamp libfuzzer
  echo "== prerelease libfuzzer: $LF_SECS s per target, reflection build"
  tools/fuzz_libfuzzer.sh --out "$OUT/libfuzzer" --seconds "$LF_SECS" "${CORPUS_ARGS[@]}"
}

run_client() {
  stamp client
  echo "== prerelease client: $LF_SECS s per target, field-list build, then the cross-build outcome diff"
  tools/fuzz_libfuzzer.sh --out "$OUT/client" --seconds "$LF_SECS" --backend fieldlist "${CORPUS_ARGS[@]}"
  tools/fuzz_outcome_diff.sh --out "$OUT/client" "${CORPUS_ARGS[@]}"
}

run_verdict() {
  python3 - "$OUT" "$SHA" "$BUDGET" <<'PY'
import datetime, json, sys
from pathlib import Path
out, sha, budget = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
# phase -> jobs its needs.json must hold
expect = {'gcc': ['gcc16-diff', 'gcc16-replay'],
          'libfuzzer': ['libfuzzer'],
          'client': ['libfuzzer-fieldlist', 'outcome-diff']}
problems, rows, findings = [], [], []
for phase, jobs in expect.items():
    d = out / phase
    try:
        stamp = json.loads((d / 'phase.json').read_text())
    except (OSError, ValueError):
        problems.append('phase %s did not run' % phase)
        continue
    if stamp['sha'] != sha:
        problems.append('phase %s ran on %s, not %s' % (phase, stamp['sha'][:10], sha[:10]))
    if stamp['budget'] != budget:
        problems.append('phase %s ran with budget %s' % (phase, stamp['budget']))
    if stamp.get('dirty'):
        problems.append('phase %s ran on a tree with uncommitted changes' % phase)
    try:
        needs = json.loads((d / 'needs.json').read_text())
    except (OSError, ValueError):
        needs = {}
    for job in jobs:
        result = needs.get(job, {}).get('result', 'missing')
        rows.append((phase, job, result))
        if result != 'success':
            problems.append('%s: %s' % (job, result))
    for f in sorted(d.glob('*/findings.json')):
        try:
            for x in json.loads(f.read_text()).get('findings', []):
                findings.append((f.parent.name, x.get('signature', '?'), x.get('error_type', '?'), x.get('top_function') or '?'))
        except ValueError:
            problems.append('unreadable %s' % f.relative_to(out))
if budget != 'release':
    problems.append('budget %s does not count for a release' % budget)
verdict = 'PASS' if not problems and not findings else 'FAIL'
now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
lines = ['# Pre-release fuzz: %s' % verdict, '',
         '- commit `%s`, budget %s, verdict written %s' % (sha, budget, now), '',
         '| phase | job | result |', '| --- | --- | --- |']
lines += ['| %s | %s | %s |' % r for r in rows]
lines += ['', '## Findings', '']
lines += ['- %s `%s`: %s in `%s`' % f for f in findings] or ['None.']
if problems:
    lines += ['', '## Why not PASS', ''] + ['- ' + p for p in problems]
lines += ['', 'Per-phase details: `<phase>/summary.md` and `<phase>/*/findings.json`.']
(out / 'VERDICT.md').write_text('\n'.join(lines) + '\n')
print('\n'.join(lines))
sys.exit(0 if verdict == 'PASS' else 1)
PY
}

case "$PHASE" in
  gcc) run_gcc ;;
  libfuzzer) run_libfuzzer ;;
  client) run_client ;;
  verdict) run_verdict ;;
  all) run_gcc; run_libfuzzer; run_client; run_verdict ;;
esac
