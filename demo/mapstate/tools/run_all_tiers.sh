#!/usr/bin/env bash
# Map state demo: self-test, the three tiers (1 / 100 / 10000 units x 500
# steps, checks on), the --evil online failure-input test, then
# summarize.py. OWNER: integration thread.
#
#   run_all_tiers.sh BIN_DIR [OUT_ROOT]
#
# Environment: MAPDEMO_TIERS (default "1 100 10000"), MAPDEMO_STEPS (500),
# MAPDEMO_PORT_BASE (47710; each run uses its own port pair so a socket in
# TIME_WAIT never blocks the next one), MAPDEMO_SKIP_EVIL=1, MAPDEMO_LABEL
# (free text stored in the result, e.g. "cloud GCC 16 smoke").
#
# OUT_ROOT (default ./mapdemo-results) gets selftest/, tier_<N>/, evil/,
# RESULT_TABLES.md and summary.json. Exit 0 only when every run and every
# check passed. Portable to macOS bash 3.2.
set -u

if [ $# -lt 1 ]; then
    echo "usage: run_all_tiers.sh BIN_DIR [OUT_ROOT]" >&2
    exit 2
fi
BIN=$1
OUT_ROOT=${2:-mapdemo-results}
TIERS=${MAPDEMO_TIERS:-"1 100 10000"}
STEPS=${MAPDEMO_STEPS:-500}
PORT=${MAPDEMO_PORT_BASE:-47710}
LABEL=${MAPDEMO_LABEL:-}
HERE=$(cd "$(dirname "$0")" && pwd)
PYTHON=${PYTHON:-python3}

mkdir -p "$OUT_ROOT"
FAILED=0

echo "== selftest"
mkdir -p "$OUT_ROOT/selftest"
rm -f "$OUT_ROOT/selftest"/*
"$BIN/mapdemo_selftest" --csv-dir "$OUT_ROOT/selftest" > "$OUT_ROOT/selftest/selftest.log" 2>&1
RC=$?
echo "selftest=$RC" > "$OUT_ROOT/selftest/exit_codes.txt"
echo "selftest: exit $RC"
[ "$RC" = 0 ] || FAILED=1

for units in $TIERS; do
    echo "== tier $units units x $STEPS steps"
    MAPDEMO_PORT_A=$PORT MAPDEMO_PORT_B=$(( PORT + 1 )) \
        "$HERE/run_tier.sh" "$BIN" "$units" "$OUT_ROOT/tier_$units" "$STEPS" || FAILED=1
    PORT=$(( PORT + 2 ))
done

if [ "${MAPDEMO_SKIP_EVIL:-0}" != 1 ]; then
    # Malformed requests against a running A (100 units, 200 steps): the evil
    # client exits 0 only when all three were rejected and A still answers a
    # normal request; A must keep running to its step limit and exit 0.
    echo "== evil (online failure inputs)"
    MAPDEMO_PORT_A=$PORT MAPDEMO_PORT_B=$(( PORT + 1 )) MAPDEMO_CLIENT_ARGS=--evil \
        "$HERE/run_tier.sh" "$BIN" 100 "$OUT_ROOT/evil" 200 || FAILED=1
    REJECTED=$(grep -c '^rejected:' "$OUT_ROOT/evil/a.log" 2>/dev/null || true)
    echo "a_rejected_lines=${REJECTED:-0}" >> "$OUT_ROOT/evil/exit_codes.txt"
    echo "evil: A logged ${REJECTED:-0} rejected line(s)"
fi

echo "== summarize"
SUMMARY_ARGS="--min-steps $STEPS"
[ "${MAPDEMO_SKIP_EVIL:-0}" = 1 ] && SUMMARY_ARGS="$SUMMARY_ARGS --no-evil"
# shellcheck disable=SC2086
"$PYTHON" "$HERE/summarize.py" "$OUT_ROOT" $SUMMARY_ARGS --label "$LABEL" \
    --out "$OUT_ROOT/RESULT_TABLES.md" --json "$OUT_ROOT/summary.json" || FAILED=1

if [ "$FAILED" = 0 ]; then
    echo "ALL PASSED ($OUT_ROOT/RESULT_TABLES.md)"
else
    echo "FAILED (see $OUT_ROOT/RESULT_TABLES.md and the logs under $OUT_ROOT)"
fi
exit "$FAILED"
