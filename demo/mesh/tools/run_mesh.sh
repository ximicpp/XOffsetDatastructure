#!/usr/bin/env bash
# Device mesh demo, cloud run (INTERFACES.md section 7): one hub on Linux and
# one device per column and exception mode, all on this host. Each device
# runs as built for its platform: Linux natively, Linux arm64 and Android
# arm64 under qemu-aarch64, Windows through wine. Every device takes evil
# jobs (--evil), so every ordered pair also checks every refusal.
#
#   demo/mesh/tools/run_mesh.sh [--build-only | --run-only] [--out DIR] [COLUMN ...]
#
# Columns (default: every one whose toolchain this host has):
#   linux-x64-gcc      host g++ (XMESH_GCC, default g++)
#   linux-x64-clang    host clang++ (XMESH_CLANG, default clang++)
#   linux-arm64-gcc    aarch64-linux-gnu-g++, static, under qemu-aarch64-static
#   windows-mingw      MinGW-w64 g++ (cmake/toolchains/mingw-w64-x86_64.cmake), wine
#   windows-msvc       MSVC under wine ($XOFFSET_MSVC_ROOT, default /opt/msvc), wine
#   windows-clang-cl   clang-cl + lld-link with the MSVC STL and SDK, wine
#   android-arm64      NDK ($ANDROID_NDK, default /opt/android-ndk-r29), static, qemu-aarch64-static
#   android-x64        NDK x86_64, static, run natively
# Each column gives two devices, NAME and NAME-noexc (xo_peer_noexc).
#
# Before the mesh, every column's xmesh_selftest runs (selftest.txt); a
# failing self-test is reported but the device still joins.
#
# Environment: XMESH_BUILD_ROOT (build trees, default <repo>/build/mesh),
# XMESH_HUB_CMAKE (extra cmake arguments for the hub build, e.g. a C++26
# compiler), XMESH_HUB_ARGS (extra xo_hub arguments, e.g. "--types Order
# --scales small"), XMESH_PORT (47801; HTTP is XMESH_PORT + 1),
# XMESH_LABEL (run label, default cloud-<date>), XMESH_TIMEOUT_S (device
# --timeout-s, default 3600), XMESH_JOBS (build parallelism).
#
# Output DIR (default <repo>/build/mesh-run): hub/ (xo_hub --out), devices/<name>/
# (peer.log, build.txt, stdout.txt, exit), selftest/<column>.txt, and
# MESH_RESULT.md (tools/mesh_report.py). Exit 0 only when the hub passed
# every job and every device exited 0.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
HERE=$REPO/demo/mesh/tools
BUILD_ROOT=${XMESH_BUILD_ROOT:-$REPO/build/mesh}
OUT=$REPO/build/mesh-run
PORT=${XMESH_PORT:-47801}
HTTP_PORT=$((PORT + 1))
LABEL=${XMESH_LABEL:-cloud-$(date -u +%Y%m%d-%H%M)}
TIMEOUT_S=${XMESH_TIMEOUT_S:-3600}
JOBS=${XMESH_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}
NDK=${ANDROID_NDK:-/opt/android-ndk-r29}
MSVC_ROOT=${XOFFSET_MSVC_ROOT:-/opt/msvc}
DO_BUILD=1
DO_RUN=1
COLUMNS=""
ALL_COLUMNS="linux-x64-gcc linux-x64-clang linux-arm64-gcc windows-mingw windows-msvc windows-clang-cl android-arm64 android-x64"

while [ $# -gt 0 ]; do
    case "$1" in
        --build-only) DO_RUN=0; shift ;;
        --run-only) DO_BUILD=0; shift ;;
        --out) OUT=$2; shift 2 ;;
        -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
        -*) echo "unknown option $1" >&2; exit 2 ;;
        *) COLUMNS="$COLUMNS $1"; shift ;;
    esac
done

have() { command -v "$1" >/dev/null 2>&1; }

# available COLUMN: true when this host can build and run the column.
available() {
    case "$1" in
        linux-x64-gcc) have "${XMESH_GCC:-g++}" ;;
        linux-x64-clang) have "${XMESH_CLANG:-clang++}" ;;
        linux-arm64-gcc) have aarch64-linux-gnu-g++ && have qemu-aarch64-static ;;
        windows-mingw) have wine && { have x86_64-w64-mingw32-g++-posix || have x86_64-w64-mingw32-g++ || [ -x /opt/mingw-gcc16/bin/x86_64-w64-mingw32-g++ ]; } ;;
        windows-msvc) have wine && [ -d "$MSVC_ROOT/VC" ] ;;
        windows-clang-cl) have wine && [ -d "$MSVC_ROOT/VC" ] && have clang-cl && { have lld-link || have lld-link-19 || [ -x /usr/lib/llvm-19/bin/lld-link ]; } ;;
        android-arm64) [ -f "$NDK/build/cmake/android.toolchain.cmake" ] && have qemu-aarch64-static ;;
        android-x64) [ -f "$NDK/build/cmake/android.toolchain.cmake" ] ;;
        *) return 1 ;;
    esac
}

if [ -z "$COLUMNS" ]; then
    for c in $ALL_COLUMNS; do
        if available "$c"; then COLUMNS="$COLUMNS $c"; else echo "skip $c: toolchain not on this host" >&2; fi
    done
fi
COLUMNS=$(echo $COLUMNS)
[ -n "$COLUMNS" ] || { echo "no device column available" >&2; exit 2; }

COMMON="-G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_BUILD_MESH_DEMO=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF"
PEER="-DXMESH_PEER_ONLY=ON -DXOFFSET_CXX_STANDARD=20"
TC=$REPO/cmake/toolchains

# cmake_args COLUMN: configure arguments of a device column.
cmake_args() {
    case "$1" in
        linux-x64-gcc) echo "-DCMAKE_CXX_COMPILER=${XMESH_GCC:-g++}" ;;
        linux-x64-clang) echo "-DCMAKE_CXX_COMPILER=${XMESH_CLANG:-clang++}" ;;
        linux-arm64-gcc) echo "-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_EXE_LINKER_FLAGS=-static" ;;
        windows-mingw) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/mingw-w64-x86_64.cmake" ;;
        windows-msvc) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/msvc-wine-x86_64.cmake" ;;
        windows-clang-cl) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/clang-cl-x86_64.cmake" ;;
        android-arm64) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/android-arm64.cmake -DANDROID_NDK=$NDK -DXOFFSET_ANDROID_QEMU=ON" ;;
        android-x64) echo "-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_static -DCMAKE_EXE_LINKER_FLAGS=-static" ;;
    esac
}

# runner COLUMN: the command prefix that runs the column's programs here.
runner() {
    case "$1" in
        linux-arm64-gcc|android-arm64) echo "qemu-aarch64-static" ;;
        windows-*) echo "wine" ;;
        *) echo "" ;;
    esac
}

exe() { case "$1" in windows-*) echo ".exe" ;; *) echo "" ;; esac; }

# path_for COLUMN PATH: a path the column's programs understand (wine: Z: drive).
path_for() { case "$1" in windows-*) echo "Z:$2" ;; *) echo "$2" ;; esac; }

export WINEDEBUG=-all
export XOFFSET_MSVC_ROOT=$MSVC_ROOT
export ANDROID_NDK=$NDK
FAILED=0

# ---- build ----------------------------------------------------------------------------------
if [ $DO_BUILD = 1 ]; then
    mkdir -p "$BUILD_ROOT"
    have wine && { wineserver -p >/dev/null 2>&1 || true; }
    echo "== hub"
    # shellcheck disable=SC2086
    if ! { cmake -S "$REPO" -B "$BUILD_ROOT/hub" $COMMON ${XMESH_HUB_CMAKE:--DXOFFSET_CXX_STANDARD=20} >"$BUILD_ROOT/hub.configure.log" 2>&1 &&
           cmake --build "$BUILD_ROOT/hub" --target xo_hub xmesh_selftest -j "$JOBS" >"$BUILD_ROOT/hub.build.log" 2>&1; }; then
        echo "hub build FAILED (see $BUILD_ROOT/hub.*.log)" >&2
        tail -20 "$BUILD_ROOT/hub.build.log" >&2
        exit 1
    fi
    for c in $COLUMNS; do
        echo "== $c"
        # shellcheck disable=SC2046
        if ! { cmake -S "$REPO" -B "$BUILD_ROOT/$c" $COMMON $PEER $(cmake_args "$c") >"$BUILD_ROOT/$c.configure.log" 2>&1 &&
               cmake --build "$BUILD_ROOT/$c" -j "$JOBS" >"$BUILD_ROOT/$c.build.log" 2>&1; }; then
            echo "$c build FAILED (see $BUILD_ROOT/$c.*.log)" >&2
            tail -20 "$BUILD_ROOT/$c.build.log" "$BUILD_ROOT/$c.configure.log" 2>/dev/null | tail -25 >&2
            FAILED=1
        fi
    done
    [ $FAILED = 0 ] || exit 1
fi
[ $DO_RUN = 1 ] || exit 0

# ---- run --------------------------------------------------------------------------------------
HUB=$BUILD_ROOT/hub/bin/Release/xo_hub
[ -x "$HUB" ] || { echo "no hub at $HUB (build first)" >&2; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT/devices" "$OUT/selftest"
OUT=$(cd "$OUT" && pwd)
have wine && { wineserver -p >/dev/null 2>&1 || true; }

echo "== self-tests"
for c in $COLUMNS; do
    bin=$BUILD_ROOT/$c/bin/Release/xmesh_selftest$(exe "$c")
    if [ ! -f "$bin" ]; then echo "$c: no xmesh_selftest" > "$OUT/selftest/$c.txt"; continue; fi
    # shellcheck disable=SC2046
    $(runner "$c") "$bin" > "$OUT/selftest/$c.txt" 2>&1
    echo "exit $?" >> "$OUT/selftest/$c.txt"
    echo "$c: $(tail -1 "$OUT/selftest/$c.txt")"
done

N=0
for c in $COLUMNS; do N=$((N + 2)); done
echo "== mesh: hub + $N devices, run $LABEL, dashboard http://localhost:$HTTP_PORT/"
# shellcheck disable=SC2086
"$HUB" --run "$LABEL" --out "$OUT/hub" --port "$PORT" --http-port "$HTTP_PORT" --expect "$N" --wait-s 600 \
    --job-timeout-s 300 --max-inflight 16 ${XMESH_HUB_ARGS:-} > "$OUT/hub.log" 2>&1 &
HUB_PID=$!
sleep 1
PIDS=""
for c in $COLUMNS; do
    for mode in exc noexc; do
        prog=xo_peer
        name=$c
        [ $mode = noexc ] && { prog=xo_peer_noexc; name=$c-noexc; }
        bin=$BUILD_ROOT/$c/bin/Release/$prog$(exe "$c")
        dir=$OUT/devices/$name
        mkdir -p "$dir"
        if [ ! -f "$bin" ]; then echo "missing $bin" > "$dir/stdout.txt"; echo 127 > "$dir/exit"; continue; fi
        (
            # shellcheck disable=SC2046
            $(runner "$c") "$bin" --hub "127.0.0.1:$PORT" --name "$name" --evil --out "$(path_for "$c" "$dir")" \
                --timeout-s "$TIMEOUT_S" --connect-s 120 > "$dir/stdout.txt" 2>&1
            echo $? > "$dir/exit"
        ) &
        PIDS="$PIDS $!"
    done
done
wait "$HUB_PID"
HUB_RC=$?
echo "hub exit $HUB_RC"
# Devices leave after Bye; give the slow ones (wine, qemu) time.
for p in $PIDS; do
    for _ in $(seq 1 60); do kill -0 "$p" 2>/dev/null || break; sleep 1; done
    kill "$p" 2>/dev/null
done
wait 2>/dev/null
for d in "$OUT"/devices/*; do
    rc=$(cat "$d/exit" 2>/dev/null || echo "none")
    echo "$(basename "$d"): exit $rc"
    [ "$rc" = 0 ] || FAILED=1
done
[ $HUB_RC = 0 ] || FAILED=1
python3 "$HERE/mesh_report.py" "$OUT" --columns "$COLUMNS" > "$OUT/MESH_RESULT.md" || FAILED=1
tail -3 "$OUT/hub/summary.txt"
echo "report: $OUT/MESH_RESULT.md"
exit $FAILED
