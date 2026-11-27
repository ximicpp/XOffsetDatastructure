#!/usr/bin/env bash
# Runs the game server demo on one machine: Redis, logic nodes, a gateway and
# the bots, then prints the bots' report and every server's last metrics.
#
#   demo/gameserver/tools/run_demo.sh BIN_DIR [bots] [seconds] [rate] [nodes] [shards]
#
# BIN_DIR holds gs_logic, gs_gateway and gs_bot. Needs redis-server on PATH
# (or REDIS_SERVER=/path). Ports: REDIS_PORT (6390), GATEWAY_PORT (7100),
# logic nodes from NODE_PORT (7200). Logs and the JSON report go to OUT_DIR
# (default: a new temporary directory). Exit status: the bots' (0 = every
# check passed).
set -euo pipefail

BIN=${1:?usage: run_demo.sh BIN_DIR [bots] [seconds] [rate] [nodes] [shards]}
BOTS=${2:-200}
SECONDS_=${3:-10}
RATE=${4:-10}
NODES=${5:-2}
SHARDS=${6:-4}
REDIS_SERVER=${REDIS_SERVER:-redis-server}
REDIS_PORT=${REDIS_PORT:-6390}
GATEWAY_PORT=${GATEWAY_PORT:-7100}
NODE_PORT=${NODE_PORT:-7200}
RECONNECTS=${RECONNECTS:-20}
MIGRATIONS=${MIGRATIONS:-20}
OUT_DIR=${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/gsdemo.XXXXXX")}
mkdir -p "$OUT_DIR"

pids=()
cleanup() {
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT

"$REDIS_SERVER" --port "$REDIS_PORT" --save "" --appendonly no --daemonize no \
    >"$OUT_DIR/redis.log" 2>&1 &
pids+=($!)
for _ in $(seq 50); do
    redis-cli -p "$REDIS_PORT" ping >/dev/null 2>&1 && break
    sleep 0.1
done
redis-cli -p "$REDIS_PORT" flushdb >/dev/null

node_list=""
for ((i = 0; i < NODES; i++)); do
    node_list+="${node_list:+,}127.0.0.1:$((NODE_PORT + i))"
done
for ((i = 0; i < NODES; i++)); do
    "$BIN/gs_logic" --node "$i" --nodes "$node_list" --shards "$SHARDS" \
        --redis "127.0.0.1:$REDIS_PORT" >"$OUT_DIR/logic$i.log" 2>&1 &
    pids+=($!)
done
sleep 0.5
"$BIN/gs_gateway" --id 1 --port "$GATEWAY_PORT" --nodes "$node_list" \
    --redis "127.0.0.1:$REDIS_PORT" --rate 1000 >"$OUT_DIR/gateway.log" 2>&1 &
pids+=($!)
for _ in $(seq 50); do
    [ "$(grep -c verified "$OUT_DIR/gateway.log" || true)" -ge "$NODES" ] && break
    sleep 0.1
done

# CPU seconds a process has used so far (Linux /proc; empty elsewhere).
cpu_seconds() {
    local stat
    stat=$(cat "/proc/$1/stat" 2>/dev/null) || return 0
    # After the ") " that ends the command name, utime and stime are fields 12 and 13.
    awk -v hz="$(getconf CLK_TCK)" '{ printf "%.2f", ($12 + $13) / hz }' <<<"${stat##*) }"
}
names=(redis)
for ((i = 0; i < NODES; i++)); do names+=("logic$i"); done
names+=(gateway)
declare -a cpu0
for i in "${!pids[@]}"; do cpu0[i]=$(cpu_seconds "${pids[i]}"); done

status=0
"$BIN/gs_bot" --gateway "127.0.0.1:$GATEWAY_PORT" --redis "127.0.0.1:$REDIS_PORT" \
    --bots "$BOTS" --seconds "$SECONDS_" --rate "$RATE" --nodes "$NODES" \
    --reconnects "$RECONNECTS" --migrations "$MIGRATIONS" \
    --report "$OUT_DIR/report.json" 2>"$OUT_DIR/bot.log" || status=$?

if [ -r /proc/self/stat ]; then
    echo
    echo "== server CPU during the bot run (seconds) =="
    for i in "${!pids[@]}"; do
        now=$(cpu_seconds "${pids[i]}")
        awk -v n="${names[i]}" -v a="${cpu0[i]:-0}" -v b="${now:-0}" 'BEGIN { printf "%-8s %.2f\n", n, b - a }'
    done
fi
sleep 5.2  # one more metrics line from every server
echo
echo "== servers (last metrics line) =="
for f in "$OUT_DIR"/logic*.log "$OUT_DIR/gateway.log"; do
    grep -E '^(node|gateway) [0-9]+: (players|sessions)' "$f" | tail -n 1 || true
done
echo "redis: $(redis-cli -p "$REDIS_PORT" dbsize) keys, $(redis-cli -p "$REDIS_PORT" info memory | grep '^used_memory_human' | cut -d: -f2 | tr -d '\r')"
echo "logs and report.json: $OUT_DIR"
exit "$status"
