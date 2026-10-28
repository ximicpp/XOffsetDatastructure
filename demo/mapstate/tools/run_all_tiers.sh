#!/usr/bin/env bash
# Map state demo: self-test, the schema lock check, the three tiers (1 / 100
# / 10000 units x 500 steps, checks on) in both buffer modes, the --evil
# online failure-input test in both modes, then summarize.py.
#
#   run_all_tiers.sh BIN_DIR [OUT_ROOT]
#
# Environment: MAPDEMO_TIERS (default "1 100 10000"), MAPDEMO_STEPS (500),
# MAPDEMO_MODES (default "fresh reuse": --buffers of every process),
# MAPDEMO_PORT_BASE (47710; each run uses its own port pair so a socket in
# TIME_WAIT never blocks the next one), MAPDEMO_SKIP_EVIL=1, MAPDEMO_LABEL
# (free text stored in the result, e.g. "cloud GCC 16 smoke"),
# MAPDEMO_COMPARE (another OUT_ROOT, e.g. a run with another compiler or on
# another platform: its per-tick step wire digests must equal these),
# MAPDEMO_CLIENT / MAPDEMO_CLIENT_ARGS (see run_tier.sh; the evil run adds
# --evil to MAPDEMO_CLIENT_ARGS), MAPDEMO_CLIENT2 / MAPDEMO_CLIENT2_ARGS (a
# second client in every tier, at the same time; not in the evil run),
# MAPDEMO_EDITS (run_tier.sh: write-back every Nth normal snapshot, default
# 10; the evil run sends its own bad write-backs), MAPDEMO_SKIP_SELFTEST=1 (the self-test and schema lock check of the
# servers already ran for these binaries).
#
# OUT_ROOT (default ./mapdemo-results) gets selftest/, schema_check/,
# tier_<N>_<mode>/, evil_<mode>/, RESULT_TABLES.md and summary.json. Exit 0 only when every run
# and every check passed. Portable to macOS bash 3.2.
set -u

if [ $# -lt 1 ]; then
    echo "usage: run_all_tiers.sh BIN_DIR [OUT_ROOT]" >&2
    exit 2
fi
BIN=$1
OUT_ROOT=${2:-mapdemo-results}
TIERS=${MAPDEMO_TIERS:-"1 100 10000"}
STEPS=${MAPDEMO_STEPS:-500}
MODES=${MAPDEMO_MODES:-"fresh reuse"}
PORT=${MAPDEMO_PORT_BASE:-47710}
LABEL=${MAPDEMO_LABEL:-}
HERE=$(cd "$(dirname "$0")" && pwd)
PYTHON=${PYTHON:-python3}

mkdir -p "$OUT_ROOT"
FAILED=0

if [ "${MAPDEMO_SKIP_SELFTEST:-0}" != 1 ]; then
    echo "== selftest"
    mkdir -p "$OUT_ROOT/selftest"
    rm -f "$OUT_ROOT/selftest"/*
    "$BIN/mapdemo_selftest" --csv-dir "$OUT_ROOT/selftest" > "$OUT_ROOT/selftest/selftest.log" 2>&1
    RC=$?
    echo "selftest=$RC" > "$OUT_ROOT/selftest/exit_codes.txt"
    echo "selftest: exit $RC"
    [ "$RC" = 0 ] || FAILED=1

    # The root types still match demo/mapstate/schema.lock, or changed with
    # a lineage entry for the last frozen version (schema_freeze.cpp).
    echo "== schema lock check"
    mkdir -p "$OUT_ROOT/schema_check"
    rm -f "$OUT_ROOT/schema_check"/*
    "$BIN/mapdemo_schema_freeze" check "$HERE/../schema.lock" > "$OUT_ROOT/schema_check/check.log" 2>&1
    RC=$?
    echo "schema_check=$RC" > "$OUT_ROOT/schema_check/exit_codes.txt"
    echo "schema lock check: exit $RC"
    [ "$RC" = 0 ] || FAILED=1
fi

for units in $TIERS; do
    for mode in $MODES; do
        echo "== tier $units units x $STEPS steps, --buffers $mode"
        MAPDEMO_PORT_A=$PORT MAPDEMO_PORT_B=$(( PORT + 1 )) MAPDEMO_BUFFERS=$mode \
            "$HERE/run_tier.sh" "$BIN" "$units" "$OUT_ROOT/tier_${units}_$mode" "$STEPS" || FAILED=1
        PORT=$(( PORT + 2 ))
    done
done

if [ "${MAPDEMO_SKIP_EVIL:-0}" != 1 ]; then
    # Malformed requests, subscriptions and handshakes against a running A
    # (100 units, 200 steps): the evil client exits 0 only when every one
    # was rejected and A still answers normal requests (also on the
    # connection that sent a rejected one); A must keep running to its step
    # limit and exit 0.
    for mode in $MODES; do
        echo "== evil (online failure inputs), --buffers $mode"
        MAPDEMO_PORT_A=$PORT MAPDEMO_PORT_B=$(( PORT + 1 )) MAPDEMO_BUFFERS=$mode \
            MAPDEMO_CLIENT_ARGS="--evil ${MAPDEMO_CLIENT_ARGS:-}" MAPDEMO_A_ARGS= MAPDEMO_CLIENT2= MAPDEMO_EDITS=0 \
            "$HERE/run_tier.sh" "$BIN" 100 "$OUT_ROOT/evil_$mode" 200 || FAILED=1
        PORT=$(( PORT + 2 ))
        REJECTED=$(grep -c '^rejected:' "$OUT_ROOT/evil_$mode/a.log" 2>/dev/null || true)
        echo "a_rejected_lines=${REJECTED:-0}" >> "$OUT_ROOT/evil_$mode/exit_codes.txt"
        echo "evil: A logged ${REJECTED:-0} rejected line(s)"
    done
fi

echo "== summarize"
SUMMARY_ARGS="--min-steps $STEPS"
[ "${MAPDEMO_SKIP_EVIL:-0}" = 1 ] && SUMMARY_ARGS="$SUMMARY_ARGS --no-evil"
[ "${MAPDEMO_SKIP_SELFTEST:-0}" = 1 ] && SUMMARY_ARGS="$SUMMARY_ARGS --no-selftest"
[ -n "${MAPDEMO_COMPARE:-}" ] && SUMMARY_ARGS="$SUMMARY_ARGS --compare ${MAPDEMO_COMPARE}"
# shellcheck disable=SC2086
"$PYTHON" "$HERE/summarize.py" "$OUT_ROOT" $SUMMARY_ARGS --label "$LABEL" \
    --out "$OUT_ROOT/RESULT_TABLES.md" --json "$OUT_ROOT/summary.json" || FAILED=1

if [ "$FAILED" = 0 ]; then
    echo "ALL PASSED ($OUT_ROOT/RESULT_TABLES.md)"
else
    echo "FAILED (see $OUT_ROOT/RESULT_TABLES.md and the logs under $OUT_ROOT)"
fi
exit "$FAILED"
