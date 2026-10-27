#!/usr/bin/env bash
# Map state demo, end to end: Linux A and B serve one or more clients, each
# built for its own platform (native, MinGW under wine, Android under qemu or
# an emulator, iOS Simulator, ...). For every client target:
#   selfcheck/  client --self-check (offline positive and refusal cases);
#   replay/     client --replay of one snapshot corpus written by A, so every
#               platform verifies the same bytes against A's digests;
#   tiers       run_all_tiers.sh with that client (--encoding cycle: dense,
#               compact and zstd snapshots), evil included.
# Then e2e_report.py writes OUT_ROOT/E2E_RESULT.md, one row per target.
#
#   run_e2e.sh SERVER_BIN OUT_ROOT NAME=CLIENT_CMD [NAME=CLIENT_CMD ...]
#
# NAME is a directory name (e.g. linux-gcc13, windows-mingw, android-arm64);
# CLIENT_CMD is the client command, words split on spaces, e.g.
#   linux=build/bin/Release/mapdemo_client
#   windows-mingw="wine /x/mapdemo_client.exe"
#   android-arm64="qemu-aarch64-static /x/mapdemo_client"
#
# Environment: MAPDEMO_CORPUS (an existing corpus directory; default: A
# writes OUT_ROOT/corpus in a 10000-unit, 200-step run with the first
# target's client), MAPDEMO_CORPUS_MAX (60 snapshots), MAPDEMO_TIERS,
# MAPDEMO_STEPS, MAPDEMO_MODES (default "reuse"), MAPDEMO_PORT_BASE (47800;
# each target gets its own range of 100 ports), MAPDEMO_LABEL (prefix of
# every target's label), MAPDEMO_ENCODING (client --encoding, default
# cycle), MAPDEMO_A_HOST (see run_tier.sh), MAPDEMO_COMPARE (an OUT_ROOT of run_all_tiers.sh: every target's
# step wires must equal it; default: targets after the first compare with
# the first). The server self-test runs once, with the first target.
#
# Exit 0 only when every target passed. Portable to macOS bash 3.2.
set -u

if [ $# -lt 3 ]; then
    echo "usage: run_e2e.sh SERVER_BIN OUT_ROOT NAME=CLIENT_CMD [NAME=CLIENT_CMD ...]" >&2
    exit 2
fi
BIN=$1
OUT_ROOT=$2
shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
PYTHON=${PYTHON:-python3}
PORT=${MAPDEMO_PORT_BASE:-47800}
MODES=${MAPDEMO_MODES:-reuse}
ENCODING=${MAPDEMO_ENCODING:-cycle}
LABEL=${MAPDEMO_LABEL:-}

for t in "$@"; do
    case "$t" in
        *=?*) ;;
        *) echo "error: target '$t' is not NAME=CLIENT_CMD" >&2; exit 2 ;;
    esac
done
mkdir -p "$OUT_ROOT"
OUT_ROOT=$(cd "$OUT_ROOT" && pwd)
FAILED=0

# client_offline NAME CMD MODE_ARGS OUT_DIR: one offline client run.
client_offline() {
    local name=$1 cmd=$2 args=$3 out=$4 rc
    mkdir -p "$out"
    rm -f "$out"/*.csv "$out"/*.log "$out"/*.txt
    echo "client_cmd=$cmd" > "$out/meta.txt"
    echo "client_args=$args" >> "$out/meta.txt"
    # shellcheck disable=SC2086 # cmd and args are word lists on purpose
    $cmd $args --csv-dir "$out" > "$out/client.log" 2>&1
    rc=$?
    echo "client=$rc" > "$out/exit_codes.txt"
    echo "$name $(basename "$out"): exit $rc"
    [ "$rc" = 0 ] || FAILED=1
}

FIRST_NAME=${1%%=*}
FIRST_CMD=${1#*=}
CORPUS=${MAPDEMO_CORPUS:-}
if [ -z "$CORPUS" ]; then
    CORPUS=$OUT_ROOT/corpus
    echo "== corpus: A writes up to ${MAPDEMO_CORPUS_MAX:-60} snapshots (10000 units, 200 steps, client $FIRST_NAME)"
    rm -rf "$CORPUS"
    mkdir -p "$CORPUS"
    MAPDEMO_PORT_A=$PORT MAPDEMO_PORT_B=$(( PORT + 1 )) MAPDEMO_CLIENT="$FIRST_CMD" \
        MAPDEMO_CLIENT_ARGS="--encoding cycle" \
        MAPDEMO_A_ARGS="--corpus-dir $CORPUS --corpus-max ${MAPDEMO_CORPUS_MAX:-60}" \
        "$HERE/run_tier.sh" "$BIN" 10000 "$OUT_ROOT/corpus_run" 200 || FAILED=1
    PORT=$(( PORT + 2 ))
fi
if [ ! -f "$CORPUS/corpus.csv" ]; then
    echo "error: no corpus.csv in $CORPUS" >&2
    exit 1
fi

I=0
REF=${MAPDEMO_COMPARE:-}
for t in "$@"; do
    NAME=${t%%=*}
    CMD=${t#*=}
    ROOT=$OUT_ROOT/$NAME
    mkdir -p "$ROOT"
    echo "== target $NAME: $CMD"
    client_offline "$NAME" "$CMD" "--self-check" "$ROOT/selfcheck"
    client_offline "$NAME" "$CMD" "--replay $CORPUS" "$ROOT/replay"
    SKIP_SELFTEST=1
    [ "$I" = 0 ] && SKIP_SELFTEST=0
    MAPDEMO_CLIENT="$CMD" MAPDEMO_CLIENT_ARGS="--encoding $ENCODING" MAPDEMO_MODES="$MODES" \
        MAPDEMO_SKIP_SELFTEST=$SKIP_SELFTEST MAPDEMO_COMPARE="$REF" \
        MAPDEMO_PORT_BASE=$(( PORT + 100 * (I + 1) )) \
        MAPDEMO_LABEL="${LABEL:+$LABEL; }client $NAME" \
        "$HERE/run_all_tiers.sh" "$BIN" "$ROOT" || FAILED=1
    [ -z "$REF" ] && REF=$ROOT
    I=$(( I + 1 ))
done

echo "== e2e report"
NAMES=
for t in "$@"; do NAMES="$NAMES ${t%%=*}"; done
# shellcheck disable=SC2086 # NAMES is a word list on purpose
"$PYTHON" "$HERE/e2e_report.py" "$OUT_ROOT" --corpus "$CORPUS" --label "$LABEL" $NAMES || FAILED=1

if [ "$FAILED" = 0 ]; then
    echo "E2E PASSED ($OUT_ROOT/E2E_RESULT.md)"
else
    echo "E2E FAILED (see $OUT_ROOT/E2E_RESULT.md and each target's RESULT_TABLES.md)"
fi
exit "$FAILED"
