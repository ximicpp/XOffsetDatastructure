#!/usr/bin/env bash
# Map state demo: run one tier (B, A --check, headless client --check) and
# collect its CSV files and logs. OWNER: integration thread.
#
#   run_tier.sh BIN_DIR UNITS OUT_DIR [STEPS]
#
# Environment: MAPDEMO_PORT_A / MAPDEMO_PORT_B (default 47701 / 47702),
# MAPDEMO_SEED (default 1), MAPDEMO_CLIENT_ARGS (extra client arguments),
# MAPDEMO_NO_CLIENT=1 (servers only), MAPDEMO_A_LIMIT_S (A's wall-clock
# limit; default STEPS/5 + 60 s, i.e. 4x slower than 20 Hz plus start-up).
#
# Writes OUT_DIR/{a,b,client}.log, the CSV files of INTERFACES.md section 6,
# OUT_DIR/meta.txt and OUT_DIR/exit_codes.txt. Exit 0 only when all three
# processes exited 0. Portable to macOS bash 3.2 (no `timeout`, no `wait -n`).
set -u

if [ $# -lt 3 ]; then
    echo "usage: run_tier.sh BIN_DIR UNITS OUT_DIR [STEPS]" >&2
    exit 2
fi
BIN=$1
UNITS=$2
OUT=$3
STEPS=${4:-500}
PORT_A=${MAPDEMO_PORT_A:-47701}
PORT_B=${MAPDEMO_PORT_B:-47702}
SEED=${MAPDEMO_SEED:-1}
CLIENT_ARGS=${MAPDEMO_CLIENT_ARGS:-}

for exe in mapdemo_a mapdemo_b mapdemo_client; do
    if [ ! -x "$BIN/$exe" ]; then
        echo "error: $BIN/$exe not found" >&2
        exit 2
    fi
done
mkdir -p "$OUT"
rm -f "$OUT"/*.csv "$OUT"/*.log "$OUT"/exit_codes.txt "$OUT"/meta.txt

A_LIMIT_S=${MAPDEMO_A_LIMIT_S:-$(( STEPS / 5 + 60 ))}
TAIL_LIMIT_S=20

# wait_for PID SECONDS NAME: waits for a child; kills it at the deadline.
# Sets RC to the exit status (124 on timeout). Not called through $(...):
# a subshell cannot wait for this shell's children.
wait_for() {
    local pid=$1 limit=$2 name=$3 ticks=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$ticks" -ge $(( limit * 10 )) ]; then
            echo "error: $name did not exit within ${limit} s; killing it" >&2
            kill "$pid" 2>/dev/null
            sleep 1
            kill -9 "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            RC=124
            return
        fi
        sleep 0.1
        ticks=$(( ticks + 1 ))
    done
    wait "$pid"
    RC=$?
}

{
    echo "units=$UNITS"
    echo "steps=$STEPS"
    echo "seed=$SEED"
    echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "host=$(uname -srm)"
    echo "load_before=$(uptime | sed 's/.*load average[s]*: //')"
    if command -v git >/dev/null 2>&1; then
        echo "commit=$(git -C "$(dirname "$0")" rev-parse HEAD 2>/dev/null || echo unknown)"
    fi
} > "$OUT/meta.txt"

"$BIN/mapdemo_b" --port "$PORT_B" --seed "$SEED" --csv-dir "$OUT" > "$OUT/b.log" 2>&1 &
B_PID=$!
"$BIN/mapdemo_a" --units "$UNITS" --seed "$SEED" --steps "$STEPS" --check \
    --port "$PORT_A" --b-port "$PORT_B" --csv-dir "$OUT" > "$OUT/a.log" 2>&1 &
A_PID=$!
C_PID=
if [ "${MAPDEMO_NO_CLIENT:-0}" != 1 ]; then
    # shellcheck disable=SC2086 # CLIENT_ARGS is a word list on purpose
    "$BIN/mapdemo_client" --headless --check --port "$PORT_A" --csv-dir "$OUT" \
        $CLIENT_ARGS > "$OUT/client.log" 2>&1 &
    C_PID=$!
fi

wait_for "$A_PID" "$A_LIMIT_S" mapdemo_a
A_RC=$RC
C_RC=skipped
if [ -n "$C_PID" ]; then
    wait_for "$C_PID" "$TAIL_LIMIT_S" mapdemo_client
    C_RC=$RC
fi
wait_for "$B_PID" "$TAIL_LIMIT_S" mapdemo_b
B_RC=$RC

{
    echo "a=$A_RC"
    echo "b=$B_RC"
    echo "client=$C_RC"
} > "$OUT/exit_codes.txt"
echo "load_after=$(uptime | sed 's/.*load average[s]*: //')" >> "$OUT/meta.txt"

echo "tier units=$UNITS steps=$STEPS: a=$A_RC b=$B_RC client=$C_RC (logs in $OUT)"
for f in a b client; do
    if [ -f "$OUT/$f.log" ] && grep -q '^error:' "$OUT/$f.log"; then
        grep '^error:' "$OUT/$f.log" | sed "s/^/  $f: /" | head -n 5
    fi
done
if [ "$A_RC" = 0 ] && [ "$B_RC" = 0 ] && { [ "$C_RC" = 0 ] || [ "$C_RC" = skipped ]; }; then
    exit 0
fi
exit 1
