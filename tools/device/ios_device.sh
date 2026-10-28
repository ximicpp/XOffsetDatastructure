#!/bin/bash
# The library tests and the phone measurements on an iPhone or an iOS simulator, through
# the test host app (tools/test_host; docs/CLIENT_TARGETS.md, "手机上运行"). Mac with
# Xcode; macOS bash 3.2.
#
#   tools/device/ios_device.sh sim|device OUT [options] [-- extra cmake args]
#
# (local: the same steps with the command-line host on Linux or macOS, to check this script.)
#
#   --udid ID          simulator UDID (sim, default: the booted one) or phone UDID
#                      (device, default: the only paired iPhone devicectl lists)
#   --build DIR        build directory (default build/ios-<mode>)
#   --steps LIST       comma-separated, in this order (default build,info,ctest,corpus,slg,mem):
#                        build   configure + build with -DXOFFSET_TEST_HOST=ON, sign (device:
#                                sign_ios_app.sh with the Apple ID signed in to Xcode)
#                        info    @info: model, iOS, memory, thermal state
#                        ctest   the whole CTest suite, one program per app launch
#                        corpus  reference wire corpus (--ref): read, write, diff -r
#                        slg     SC-6: slg_bench verify, then --slg-runs timed runs (A..D)
#                        mem     SC-12: xo_mem_probe --single, --va, --commit until killed
#                        net     @net HOST PORT 180 (--server-host): waits until the app may
#                                reach the server (the phone asks once for local network access)
#                        clients wrappers OUT/clients/mapdemo_client[_noexc] for run_e2e.sh,
#                                connecting to --server-host
#   --ref DIR          reference wire corpus (test_cross_compiler_wire --write output)
#   --fb-include DIR   flatbuffers include directory and
#   --fb-generated DIR directory with slg_generated.h (flatc 25.12.19): slg also runs FlatBuffers
#   --zstd PATH|URL    zstd 1.5.7 tarball for the demo client (MAPDEMO_ZSTD_URL)
#   --server-host IP   address of the demo servers as the app sees them (sim: 127.0.0.1,
#                      device: the Mac's Wi-Fi address)
#   --slg-runs N       timed slg runs (default 3), --slg-scale S|M|L (M), --slg-cycles N (41)
#   --mem-limit MIB    --commit probe limit (default 16384 device, 2048 simulator)
#   -j N               build jobs
#
# OUT gets RESULT.md (one row per step), the logs, slg/ (raw jsonl, analyze.py tables) and
# mem/. Phone timings: Auto-Lock "Never", charger on, nothing else running; thermal state
# and low power mode are recorded before each timed run (slg/info_<n>.txt).
# Exit 0 every step passed, 3 a step failed, 2 usage or environment.
set -u -o pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
HOSTRUN=$REPO/tools/test_host/xo_host_run.py
MODE=${1:-}; OUT=${2:-}
case "$MODE" in sim|device|local) ;; *) sed -n 2,42p "$0"; exit 2 ;; esac
[ -n "$OUT" ] || { echo "OUT missing" >&2; exit 2; }
shift 2
UDID=""; B=""; STEPS=build,info,ctest,corpus,slg,mem; REF=""; FBI=""; FBG=""; ZSTD=""; SERVER=""
SLG_RUNS=3; SLG_SCALE=M; SLG_CYCLES=41; MEM_LIMIT=""; JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4); EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --udid) UDID=$2; shift 2 ;;
        --build) B=$2; shift 2 ;;
        --steps) STEPS=$2; shift 2 ;;
        --ref) REF=$2; shift 2 ;;
        --fb-include) FBI=$2; shift 2 ;;
        --fb-generated) FBG=$2; shift 2 ;;
        --zstd) ZSTD=$2; shift 2 ;;
        --server-host) SERVER=$2; shift 2 ;;
        --slg-runs) SLG_RUNS=$2; shift 2 ;;
        --slg-scale) SLG_SCALE=$2; shift 2 ;;
        --slg-cycles) SLG_CYCLES=$2; shift 2 ;;
        --mem-limit) MEM_LIMIT=$2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        --) shift; EXTRA=("$@"); break ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
B=${B:-$REPO/build/ios-$MODE}
[ -n "$MEM_LIMIT" ] || { [ "$MODE" = device ] && MEM_LIMIT=16384 || MEM_LIMIT=2048; }
if [ -z "${DEVELOPER_DIR:-}" ] && [ -d /Applications/Xcode.app/Contents/Developer ] \
   && ! xcode-select -p 2>/dev/null | grep -q '\.app/'; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
log() { echo "== $(utc) ios_device $MODE: $*"; }
ROWS=""; RC=0
row() { ROWS="$ROWS| $1 | $2 | $3 |
"; }
bad() { row "$1" "**failed**${3:+ ($3)}" "$2"; RC=3; }
finish() {
    {
        echo "# iOS $MODE run"
        echo
        echo "- $(utc), commit $(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo '?'), build $B"
        [ -f "$OUT/device.txt" ] && sed 's/^/- /' "$OUT/device.txt"
        echo "- exit $RC"
        echo
        echo "| step | result | detail |"
        echo "| --- | --- | --- |"
        printf '%s' "$ROWS"
    } > "$OUT/RESULT.md"
    cat "$OUT/RESULT.md"
    exit $RC
}

# ---------------------------------------------------------------- the device
if [ "$MODE" = device ]; then
    xcrun devicectl list devices --json-output "$OUT/devices.json" > /dev/null 2>&1 \
        || { echo "devicectl list devices failed (Xcode 15+?)" >&2; exit 2; }
    pick=$(python3 - "$OUT/devices.json" "$UDID" <<'PY'
import json, sys
want = sys.argv[2]
for d in json.load(open(sys.argv[1])).get("result", {}).get("devices", []):
    hw, dp, cp = d.get("hardwareProperties", {}), d.get("deviceProperties", {}), d.get("connectionProperties", {})
    if hw.get("platform") != "iOS" or hw.get("reality") == "virtual":
        continue
    if want and want not in (hw.get("udid"), d.get("identifier")):
        continue
    if cp.get("pairingState") != "paired":
        continue
    print("\t".join([hw.get("udid", ""), hw.get("marketingName", "?"), hw.get("productType", "?"),
                     dp.get("osVersionNumber", "?"), cp.get("transportType", "?")]))
    break
PY
)
    [ -n "$pick" ] || { echo "no paired iPhone${UDID:+ $UDID} (connect it, unlock it, trust this Mac, Developer Mode on)" >&2; exit 2; }
    IFS=$'\t' read -r UDID MODEL PTYPE OSV TRANSPORT <<< "$pick"
    printf 'device: %s (%s), iOS %s, %s, udid %s\n' "$MODEL" "$PTYPE" "$OSV" "$TRANSPORT" "$UDID" > "$OUT/device.txt"
    TC=ios-device.cmake; DEVOPT=-DXO_HOST_DEVICE=$UDID
elif [ "$MODE" = local ]; then
    printf 'local: %s\n' "$(uname -srm)" > "$OUT/device.txt"
    TC=""; DEVOPT="-DCMAKE_CROSSCOMPILING_EMULATOR=python3;$HOSTRUN;--backend;local"
else
    UDID=${UDID:-booted}
    xcrun simctl list devices booted | grep -q Booted || { echo "no booted simulator" >&2; exit 2; }
    printf 'simulator: %s\n' "$(xcrun simctl list devices | grep -m1 "${UDID/booted/Booted}" | sed 's/^ *//')" > "$OUT/device.txt"
    TC=ios-simulator.cmake; DEVOPT=-DXOFFSET_IOS_SIM_DEVICE=$UDID
fi
APP=$B/bin/Release/xoffset_test_host.app
[ "$MODE" = local ] && APP=$B/bin/Release/xoffset_test_host
SIGNED=$APP
[ "$MODE" = device ] && SIGNED=$B/signed/xoffset_test_host.app
export XO_HOST_BACKEND=$MODE XO_HOST_DEVICE=$UDID XO_HOST_APP=$SIGNED XO_HOST_STATE=$B/host-state
[ "$MODE" = device ] && export XO_HOST_INSTALL=1
host() { python3 "$HOSTRUN" "$@"; }   # host PROGRAM ARGS...: one run in the app
BIN=$B/bin/Release

step_build() {
    log "build ($TC)"
    local fb=()
    [ -n "$FBI" ] && [ -n "$FBG" ] && fb=(-DXO_SLG_FLATBUFFERS_INCLUDE="$FBI" -DXO_SLG_GENERATED="$FBG")
    cmake -S "$REPO" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20 \
        ${TC:+-DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/toolchains/$TC"} "$DEVOPT" -DXOFFSET_TEST_HOST=ON \
        -DXOFFSET_BUILD_DEMOS=ON -DMAPDEMO_CLIENT_ONLY=ON ${ZSTD:+-DMAPDEMO_ZSTD_URL=$ZSTD} \
        ${fb[@]+"${fb[@]}"} ${EXTRA[@]+"${EXTRA[@]}"} > "$OUT/configure.log" 2>&1 \
        && cmake --build "$B" -j "$JOBS" -- -k 0 > "$OUT/build.log" 2>&1 \
        || { bad build "configure.log, build.log ($(grep -c '^FAILED:' "$OUT/build.log" 2>/dev/null) targets failed; -k 0 builds the rest)"; return 1; }
    local fw=$APP/Frameworks lib n
    [ "$MODE" = local ] && fw=$(dirname "$APP")/hosted
    n=$(ls "$fw" | grep -c '^libxoh_')
    if [ "$MODE" = sim ]; then
        # the linker signs each file ad hoc; the bundle gets an ad hoc signature over all of them
        for lib in "$APP"/Frameworks/*.dylib; do codesign --force --sign - --timestamp=none "$lib" > /dev/null 2>&1; done
        codesign --force --sign - --timestamp=none "$APP" > "$OUT/sign.log" 2>&1 || { bad build "ad hoc signing failed (sign.log)"; return 1; }
    elif [ "$MODE" = device ]; then
        bash "$REPO/tools/test_host/sign_ios_app.sh" "$APP" "$SIGNED" "$UDID" > "$OUT/sign.log" 2>&1 \
            || { bad build "signing failed (sign.log): Apple ID in Xcode > Settings > Accounts, phone unlocked"; return 1; }
    fi
    row build passed "$(sed -n 's/^set(CMAKE_CXX_COMPILER_ID "\(.*\)")/\1/p' "$B"/CMakeFiles/*/CMakeCXXCompiler.cmake | head -1) $(sed -n 's/^set(CMAKE_CXX_COMPILER_VERSION "\(.*\)")/\1/p' "$B"/CMakeFiles/*/CMakeCXXCompiler.cmake | head -1), C++20 field lists, $n program libraries in the app$([ -n "$FBI" ] && echo ', slg with FlatBuffers')"
}
step_info() {
    host @info > "$OUT/info.txt" 2>&1 || { bad info "info.txt"; return 1; }
    row info passed "$(grep -E '^(hw.machine|kern.osproductversion|hw.memsize|os_proc_available_memory|thermal_state|low_power_mode) ' "$OUT/info.txt" | tr '\n' ' ')"
}
step_ctest() {
    log "ctest (one app launch per program)"
    local s e
    ctest --test-dir "$B" -j 1 --timeout 1800 --output-on-failure > "$OUT/ctest.log" 2>&1; e=$?
    s=$(grep -E 'tests passed|tests failed' "$OUT/ctest.log" | tail -1)
    if [ $e = 0 ]; then row ctest passed "$s"; else bad ctest "$s (ctest.log)" "rc $e"; fi
}
step_corpus() {
    [ -n "$REF" ] || { row corpus skipped "no --ref"; return; }
    log "corpus"
    rm -rf "$OUT/corpus"
    host "$BIN/test_cross_compiler_wire" --read "$REF" > "$OUT/corpus-read.log" 2>&1 \
        && host "$BIN/test_cross_compiler_wire" --write "$OUT/corpus" > "$OUT/corpus-write.log" 2>&1 \
        && diff -r "$REF" "$OUT/corpus" > "$OUT/corpus-diff.log" 2>&1 \
        && row corpus passed "$(ls "$REF" | wc -l | tr -d ' ') reference wires accepted, written bytes identical" \
        || bad corpus "corpus-*.log"
}
step_slg() {
    log "slg (SC-6)"
    local d=$OUT/slg i e
    mkdir -p "$d"
    host "$BIN/xo_slg_bench" --mode verify --scale S > "$d/verify.log" 2>&1 || { bad slg "verify failed (slg/verify.log)"; return; }
    for i in $(seq 1 "$SLG_RUNS"); do
        host @info > "$d/info_$i.txt" 2>&1
        SLG_REUSE=1 host "$BIN/xo_slg_bench" --mode bench --scale "$SLG_SCALE" --cycles "$SLG_CYCLES" \
            --out "$d/run_$i.jsonl" > "$d/run_$i.log" 2>&1; e=$?
        [ $e = 0 ] || { bad slg "run $i rc $e (slg/run_$i.log)"; return; }
        python3 "$REPO/bench/slg/tools/analyze.py" "$d/run_$i.jsonl" > "$d/run_$i.md" 2>&1
    done
    cat "$d"/run_*.jsonl > "$d/all.jsonl"
    python3 "$REPO/bench/slg/tools/analyze.py" "$d/all.jsonl" > "$d/ANALYSIS.md" 2>&1
    row slg passed "verify ok; $SLG_RUNS runs scale $SLG_SCALE x $SLG_CYCLES cycles (slg/ANALYSIS.md); thermal before runs: $(cat "$d"/info_*.txt | sed -n 's/^thermal_state //p' | tr '\n' ' ')"
}
step_mem() {
    log "mem (SC-12)"
    local d=$OUT/mem e
    mkdir -p "$d"
    host @info > "$d/info.txt" 2>&1
    host "$BIN/xo_mem_probe" --single > "$d/single.txt" 2>&1 || { bad mem "--single (mem/single.txt)"; return; }
    host "$BIN/xo_mem_probe" --va 1024 > "$d/va_1024.txt" 2>&1
    host "$BIN/xo_mem_probe" --va 64 > "$d/va_64.txt" 2>&1
    rm -f "$d/commit_progress.txt"
    host "$BIN/xo_mem_probe" --commit 64 "$MEM_LIMIT" "$d/commit_progress.txt" > "$d/commit.txt" 2>&1; e=$?
    {
        echo "# memory probe ($MODE)"; echo
        echo "- device: $(sed -n 's/^hw.machine //p' "$d/info.txt" | head -1), iOS $(sed -n 's/^kern.osproductversion //p' "$d/info.txt" | head -1), $(sed -n 's/^hw.memsize //p' "$d/info.txt" | head -1) bytes physical, os_proc_available_memory at start $(sed -n 's/^os_proc_available_memory //p' "$d/info.txt" | head -1)"
        echo "- single buffer: $(grep -E '^(single_max_capacity_mib|available_mib) ' "$d/single.txt" | tr '\n' ' ')"
        echo "- 1 GiB reservations: $(grep -E '^(va_buffers|va_reserved_gib|va_stop) ' "$d/va_1024.txt" | tr '\n' ' ')"
        echo "- 64 MiB reservations: $(grep -E '^(va_buffers|va_reserved_gib|va_stop) ' "$d/va_64.txt" | tr '\n' ' ')"
        echo "- commit 64 MiB steps up to $MEM_LIMIT MiB: rc $e; last progress line: $(tail -1 "$d/commit_progress.txt" 2>/dev/null)"
    } > "$d/SUMMARY.md"
    row mem passed "mem/SUMMARY.md (commit probe rc $e: 125 = killed by the system at the limit)"
}
step_net() {
    [ -n "$SERVER" ] || { bad net "no --server-host"; return 1; }
    log "net: tap Allow on the phone if it asks for local network access (180 s)"
    host @net "$SERVER" "${XO_NET_PORT:-47999}" 180 > "$OUT/net.txt" 2>&1 \
        && row net passed "the app reaches $SERVER" \
        || { bad net "the app cannot reach $SERVER:${XO_NET_PORT:-47999} (net.txt): same Wi-Fi? local network access allowed (Settings > Privacy > Local Network)?"; return 1; }
}
step_clients() {
    [ -n "$SERVER" ] || { bad clients "no --server-host"; return 1; }
    local c d=$OUT/clients
    mkdir -p "$d"
    for c in mapdemo_client mapdemo_client_noexc; do
        [ -n "$(ls "$BIN/hosted/libxoh_$c."* "$APP/Frameworks/libxoh_$c."* 2>/dev/null)" ] || continue
        printf '#!/bin/bash\nexport XO_HOST_BACKEND=%s XO_HOST_DEVICE=%s XO_HOST_APP=%s XO_HOST_STATE=%s%s\nexec python3 %s %s --host %s "$@"\n' \
            "$MODE" "$UDID" "$SIGNED" "$B/host-state" "$([ "$MODE" = device ] && echo ' XO_HOST_INSTALL=1')" \
            "$HOSTRUN" "$BIN/$c" "$SERVER" > "$d/$c"
        chmod +x "$d/$c"
    done
    row clients written "$(ls "$d" | tr '\n' ' ')for run_e2e.sh, server $SERVER"
}

for s in $(echo "$STEPS" | tr ',' ' '); do
    case "$s" in
        build) step_build || finish ;;
        info|ctest|corpus|slg|mem|clients) "step_$s" ;;
        net) step_net || finish ;;
        *) echo "unknown step $s" >&2; RC=2; finish ;;
    esac
done
finish
