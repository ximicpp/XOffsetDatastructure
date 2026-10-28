#!/bin/bash
# Android phone test of one checkout (docs/DEVICE_TESTING.md). Runs on a macOS or Linux
# host with the phone on USB (adb), not timed:
#   1. library: full CTest of the C++20 field-list build (Android NDK, arm64-v8a) on the
#      phone, plus the cross-compiler wire corpus: the reference wires written by the
#      Linux GCC 16 C++26 reflection build are accepted and the phone writes the same bytes;
#   2. game demo: Linux servers A/B (GCC 16, C++26 reflection) in a docker container on
#      this host; the demo clients (with and without exceptions) run on the phone and
#      reach A through `adb reverse` (the phone's 127.0.0.1:47800-48049 is this host).
#      demo/mapstate/tools/run_e2e.sh: self-check, replay, 1/100/10000 units x 500 steps
#      with dense/compact/zstd snapshots, evil;
#   3. SC-6 slg (bench/slg on the field-list backend, with FlatBuffers 25.12.19 built from
#      source): verify, then $XO_SLG_RUNS timed runs, thermal status before each;
#   4. SC-12 memory probe: the largest single maximum_capacity, live reservations, and
#      committed memory in 64 MiB steps until the system kills the program.
#
#   tools/device/android_device.sh [all|setup|build|run|clean]     (default all)
#
#   setup  checks the host tools; downloads Android platform-tools and NDK r29 from
#          dl.google.com into $XO_WORK/sdk when adb / ANDROID_NDK are not given
#   build  copies this checkout to $W/src, builds the Android programs ($W/android) and,
#          in the container, the servers and the reference corpus ($W/srv)
#   run    the two parts above on the phone; results in $XO_OUT
#   all    setup, build, run
#   clean  removes the container and the phone's /data/local/tmp/xo
# More subcommands go in as cmd_<name> functions (see the end of this file).
#
# Environment:
#   XO_WORK        work root (default ~/xo-device); $W = $XO_WORK/<version>-<sha7>
#   XO_OUT         result directory (default $W/out-<UTC time>), packed as $XO_OUT.tar.gz
#   ANDROID_SERIAL the phone when more than one device is attached (adb devices -l)
#   ADB            adb to use (default: adb on PATH, else the downloaded platform-tools)
#   ANDROID_NDK    NDK root (default: NDK r29 downloaded into $XO_WORK/sdk)
#   XO_JOBS        build jobs (default: CPU count)
#   XO_SKIP        space-separated steps to skip: ctest corpus slg mem demo
#   XO_SLG_RUNS    timed slg runs (default 3); XO_SLG_SCALE S|M|L (M), XO_SLG_CYCLES (41)
#   XO_MEM_LIMIT   MiB the commit probe stops at if the system has not killed it (default:
#                  the phone's MemTotal)
#   XO_NO_FB=1     slg without FlatBuffers (no flatbuffers clone and flatc build)
#   XO_FB_GENERATED  directory with an slg_generated.h from flatc 25.12.19 (e.g. the Mac's
#                  run_phone_mac.sh fb): used instead of building flatc
#   XO_STATIC=1    fully static programs (cloud dry run with a qemu adb stand-in only)
#   XO_CMAKE_TOOL  a CMake root from tools/prepare_vm_cmake.py for the container (instead of downloading)
#
# Exit status: 0 everything that ran passed, 3 something failed (RESULT.md says what),
# 2 host tools, inputs or the phone missing (nothing ran). macOS bash 3.2 compatible.
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
CMD=${1:-all}
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
XO_WORK=${XO_WORK:-$HOME/xo-device}
VERSION=$(tr -d ' \r\n' < "$REPO/XOFFSET_VERSION")
SHA=$(git -C "$REPO" rev-parse HEAD 2>/dev/null || echo unknown)
SHA7=$(printf '%s' "$SHA" | cut -c1-7)
W=$XO_WORK/$VERSION-$SHA7
SDK=$XO_WORK/sdk
NDK_REV=29.0.14206865          # NDK r29, the one the cloud and emulator runs used
DEV=/data/local/tmp/xo
RT=xo-device-srv-$SHA7
PORTS="47800-47849 47900-47949 48000-48049"  # run_e2e.sh MAPDEMO_PORT_BASE=47800: the corpus run 47800+, target I 47800+100*(I+1)+
SKIP=" ${XO_SKIP:-} "
case "$(uname -s)" in Darwin) HOSTOS=macosx; JOBS=${XO_JOBS:-$(sysctl -n hw.ncpu)} ;; *) HOSTOS=linux; JOBS=${XO_JOBS:-$(nproc)} ;; esac
FB_TAG=v25.12.19; FB_COMMIT=7e163021e59cca4f8e1e35a7c828b5c6b7915953   # flatc for bench/slg, as on the Mac runs
BOOST_LIBS="assert config container container_hash core intrusive move predef static_assert throw_exception type_traits winapi"

utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
log() { echo "== $(utc) $*"; }
sha() { if command -v shasum > /dev/null; then shasum -a 256 "$1"; else sha256sum "$1"; fi | awk '{print $1}'; }
ROWS=""
row() { ROWS="$ROWS| $1 | $2 | $3 |
"; }
RC=0
fail() { RC=3; }
skip() { case "$SKIP" in *" $1 "*) return 0 ;; esac; return 1; }
die() { echo "STOP: $1" >&2; [ -n "${XO_OUT:-}" ] && [ -d "${XO_OUT:-}" ] && { STOPMSG=$1; finish 2; }; exit 2; }

# ---------------------------------------------------------------- tools
find_adb() {
    if [ -n "${ADB:-}" ]; then return 0; fi
    if command -v adb > /dev/null; then ADB=$(command -v adb); elif [ -x "$SDK/platform-tools/adb" ]; then ADB=$SDK/platform-tools/adb; else return 1; fi
}
find_ndk() {
    local v
    for v in "${ANDROID_NDK:-}" "${ANDROID_NDK_ROOT:-}" "${ANDROID_NDK_HOME:-}" "$SDK/ndk/$NDK_REV"; do
        [ -n "$v" ] && [ -f "$v/build/cmake/android.toolchain.cmake" ] && { ANDROID_NDK=$v; export ANDROID_NDK; return 0; }
    done
    return 1
}
sdk_download() {  # sdk_download platform-tools|ndk: newest stable platform-tools, or NDK $NDK_REV, sha1 checked
    mkdir -p "$SDK"
    python3 - "$SDK" "$1" "$HOSTOS" "$NDK_REV" <<'PY'
import hashlib, os, subprocess, sys, urllib.request, xml.etree.ElementTree as ET
sdk, what, hostos, ndk_rev = sys.argv[1:5]
BASE = "https://dl.google.com/android/repository/"
def tag(e): return e.tag.split('}')[-1]
def kid(e, name): return next((c for c in e if tag(c) == name), None)
def rev(p):
    r = kid(p, "revision"); return tuple(int(kid(r, n).text) if kid(r, n) is not None else 0 for n in ("major", "minor", "micro"))
def arch_of(p):
    for a in kid(p, "archives"):
        o = kid(a, "host-os")
        if o is not None and o.text != hostos: continue
        c = kid(a, "complete")
        return kid(c, "url").text, kid(c, "checksum").text
    return None
with urllib.request.urlopen(BASE + "repository2-3.xml", timeout=300) as r: repo = ET.fromstring(r.read())
pk = [p for p in repo if tag(p) == "remotePackage"]
if what == "ndk":
    cands = [p for p in pk if p.get("path") == "ndk;" + ndk_rev and arch_of(p)]
    dest = os.path.join(sdk, "ndk")
else:
    cands = [p for p in pk if p.get("path") == "platform-tools" and arch_of(p)
             and (kid(p, "channelRef") is None or kid(p, "channelRef").get("ref") == "channel-0")]
    dest = sdk
if not cands: sys.exit("no %s package for %s" % (what, hostos))
url, sha1 = arch_of(max(cands, key=rev))
zpath = os.path.join(sdk, os.path.basename(url))
print("download", BASE + url, flush=True)
urllib.request.urlretrieve(BASE + url, zpath)
h = hashlib.sha1()
with open(zpath, "rb") as f:
    for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
if h.hexdigest() != sha1: sys.exit("sha1 mismatch for %s" % url)
os.makedirs(dest, exist_ok=True)
subprocess.check_call(["unzip", "-q", "-o", zpath, "-d", dest])  # keeps symlinks and modes
os.remove(zpath)
if what == "ndk":
    top = [d for d in os.listdir(dest) if d.startswith("android-ndk-")]
    if top: os.replace(os.path.join(dest, top[0]), os.path.join(dest, ndk_rev))
print("installed", what, flush=True)
PY
}
cmake_ok() {  # CMake 3.30 or newer on PATH
    local v; v=$(cmake --version 2>/dev/null | sed -n 's/^cmake version \([0-9]*\)\.\([0-9]*\).*/\1 \2/p')
    [ -n "$v" ] || return 1
    set -- $v; [ "$1" -gt 3 ] || { [ "$1" = 3 ] && [ "$2" -ge 30 ]; }
}
cmd_setup() {
    log "setup ($HOSTOS, $VERSION $SHA7, work $W)"
    local c missing=""
    for c in python3 unzip tar docker ninja git; do command -v $c > /dev/null || missing="$missing $c"; done
    cmake_ok || missing="$missing cmake>=3.30"
    [ -z "$missing" ] || die "missing on this host:$missing (docs/DEVICE_TESTING.md, step 1)"
    docker info > /dev/null 2>&1 || die "docker is installed but not running (macOS: colima start --cpu 4 --memory 6, or start Docker Desktop)"
    find_adb || { log "downloading Android platform-tools into $SDK"; sdk_download platform-tools || die "platform-tools download failed"; find_adb || die "no adb"; }
    find_ndk || { log "downloading Android NDK $NDK_REV into $SDK/ndk (about 1 GB)"; sdk_download ndk || die "NDK download failed"; find_ndk || die "no NDK"; }
    log "adb $ADB ($("$ADB" version | head -1)); NDK $ANDROID_NDK"
}

# ---------------------------------------------------------------- build
srv_image() {  # gcc:16 (GCC 16.2), pinned per architecture: the images of the cloud and Mac runs
    case "$(docker info --format '{{.Architecture}}' 2>/dev/null)" in
        aarch64|arm64) echo "${XO_IMAGE:-gcc@sha256:325784a7e64cece62cc2ba202f8ee9e9b6e09dbcecabe98697a79a41148b177a}" ;;
        *) echo "${XO_IMAGE:-mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240}" ;;
    esac
}
vm() { docker exec "$RT" bash -c "$1"; }
srv_start() {  # the server container: $W mounted at the same path, ports published on 127.0.0.1
    docker inspect "$RT" > /dev/null 2>&1 && return 0
    local img pub="" r; img=$(srv_image)
    docker pull -q "$img" > /dev/null || die "docker pull $img failed"
    for r in $PORTS; do pub="$pub -p 127.0.0.1:$r:$r"; done
    # on a Linux host the files the container writes into $W belong to the user (Docker Desktop
    # and colima on macOS map them already)
    local user=""; [ $HOSTOS = linux ] && user="--user $(id -u):$(id -g)"
    mkdir -p "$W/home"
    # shellcheck disable=SC2086
    docker run -d --name "$RT" $user -e HOME=$W/home $pub -v "$W:$W" "$img" sleep infinity > /dev/null || die "container $RT did not start"
    vm "touch $W/.mount-probe" && [ -f "$W/.mount-probe" ] || die "the container cannot write $W (docker must share this directory: Docker Desktop / colima share the home directory)"
}
srv_cmake() {  # CMake for the container; XO_CMAKE_TOOL: one prepared with prepare_vm_cmake.py (offline hosts)
    [ -n "${XO_CMAKE_TOOL:-}" ] && [ ! -x "$W/cmake-tool/root/bin/cmake" ] && { rm -rf "$W/cmake-tool"; cp -R "$XO_CMAKE_TOOL" "$W/cmake-tool"; }
    [ -x "$W/cmake-tool/root/bin/cmake" ] || vm "rm -rf $W/cmake-tool; cd $W/src && python3 tools/prepare_vm_cmake.py --platform linux-\$(uname -m) --out $W/cmake-tool" > "$W/logs/srv_cmake.log" 2>&1 \
        || die "CMake for the container failed ($W/logs/srv_cmake.log)"
}
cmd_build() {
    find_ndk || die "no NDK: run setup first (or set ANDROID_NDK)"
    mkdir -p "$W"; local L=$W/logs; mkdir -p "$L"
    log "build 1/4: source of $SHA7 with the header-only submodules -> $W/src"
    git -C "$REPO" submodule update --init --depth 1 external/typelayout external/boost > "$L/submodules.log" 2>&1 || die "git submodule update failed ($L/submodules.log)"
    local l
    for l in $BOOST_LIBS; do
        [ -e "$REPO/external/boost/libs/$l/include" ] || git -C "$REPO/external/boost" submodule update --init --depth 1 "libs/$l" >> "$L/submodules.log" 2>&1 || die "boost $l: git submodule update failed"
    done
    rm -rf "$W/src"; mkdir -p "$W/src"
    ( cd "$REPO" && tar -cf - --exclude=./.git --exclude=./build --exclude='./external/*/.git' --exclude='./external/boost/libs/*/.git' . ) | tar -xf - -C "$W/src" || die "copy of the source failed"
    printf '%s\n' "$SHA" > "$W/src/.xo-commit"
    curl -fsSL -o "$W/zstd-1.5.7.tar.gz" https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz 2>/dev/null \
        || python3 -c "import sys,urllib.request; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])" https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz "$W/zstd-1.5.7.tar.gz" \
        || die "zstd 1.5.7 download failed"

    local fb=""
    if [ "${XO_NO_FB:-0}" != 1 ]; then
        log "build 2/4: FlatBuffers $FB_TAG: headers, slg_generated.h (flatc built in docker unless XO_FB_GENERATED) -> $W/fb"
        if [ "$(git -C "$W/flatbuffers" rev-parse HEAD 2>/dev/null)" != $FB_COMMIT ]; then
            rm -rf "$W/flatbuffers"
            git clone -q --depth 1 --branch $FB_TAG https://github.com/google/flatbuffers.git "$W/flatbuffers" > "$L/fb_clone.log" 2>&1 \
                || die "git clone of flatbuffers $FB_TAG failed ($L/fb_clone.log)"
            [ "$(git -C "$W/flatbuffers" rev-parse HEAD)" = $FB_COMMIT ] || die "flatbuffers $FB_TAG is not commit $FB_COMMIT"
        fi
        if [ -n "${XO_FB_GENERATED:-}" ]; then
            [ -f "$XO_FB_GENERATED/slg_generated.h" ] || die "no slg_generated.h in XO_FB_GENERATED=$XO_FB_GENERATED"
            rm -rf "$W/fb"; mkdir -p "$W/fb"; cp "$XO_FB_GENERATED/slg_generated.h" "$W/fb/"
            grep -m1 'FLATBUFFERS_VERSION_MAJOR == 25' "$W/fb/slg_generated.h" > /dev/null || die "$XO_FB_GENERATED/slg_generated.h is not from flatc 25"
            echo "slg_generated.h given by XO_FB_GENERATED (not built here)" > "$W/fb/flatc_version.txt"
        else
            srv_start; srv_cmake
            vm "export PATH=$W/cmake-tool/root/bin:\$PATH; rm -rf $W/fb;
                { [ -x $W/flatc-build/flatc ] || { cmake -S $W/flatbuffers -B $W/flatc-build -DCMAKE_BUILD_TYPE=Release -DFLATBUFFERS_BUILD_TESTS=OFF \
                      -DFLATBUFFERS_BUILD_FLATLIB=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF && cmake --build $W/flatc-build -j $JOBS --target flatc; }; } \
                && $W/flatc-build/flatc --cpp -o $W/fb $W/src/bench/slg/schema/slg.fbs && $W/flatc-build/flatc --version > $W/fb/flatc_version.txt" \
                > "$L/fb_build.log" 2>&1 || die "flatc build or slg_generated.h failed ($L/fb_build.log)"
        fi
        fb="-DXO_SLG_FLATBUFFERS_INCLUDE=$W/flatbuffers/include -DXO_SLG_GENERATED=$W/fb"
    fi

    log "build 3/4: Android arm64-v8a, NDK $(sed -n 's/^Pkg.Revision *= *//p' "$ANDROID_NDK/source.properties"), C++20 field lists -> $W/android"
    local B=$W/android st=""
    [ "${XO_STATIC:-0}" = 1 ] && st="-DCMAKE_EXE_LINKER_FLAGS=-static"
    rm -rf "$B"
    # shellcheck disable=SC2086
    cmake -S "$W/src" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20 \
        -DCMAKE_TOOLCHAIN_FILE="$W/src/cmake/toolchains/android-arm64.cmake" -DANDROID_NDK="$ANDROID_NDK" \
        -DCMAKE_CROSSCOMPILING_EMULATOR="$W/src/tools/device/adb_run.sh" -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
        -DXOFFSET_BUILD_DEMOS=ON -DMAPDEMO_CLIENT_ONLY=ON -DMAPDEMO_ZSTD_URL="$W/zstd-1.5.7.tar.gz" $st \
        -DXOFFSET_PHONE_PROGRAMS=ON $fb \
        > "$L/android_configure.log" 2>&1 || die "Android configure failed ($L/android_configure.log)"
    cmake --build "$B" --parallel "$JOBS" > "$L/android_build.log" 2>&1 || die "Android build failed ($L/android_build.log)"
    # tests whose command is cmake (compile-fail tests) need the NDK: they run here, not on the phone
    ctest --test-dir "$B" -N -V | awk '/Test command:/ {cmd=$0} /^ *Test *#[0-9]+:/ {name=$3; if (cmd ~ /cmake/) print name; cmd=""}' > "$B/compile_tests.txt"
    ctest --test-dir "$B" -R "^($(paste -sd'|' - < "$B/compile_tests.txt"))\$" --output-on-failure > "$B/host_compile_tests.txt" 2>&1 || true

    log "build 4/4: Linux servers A/B and the reference corpus (GCC 16, C++26 reflection) in docker -> $W/srv"
    srv_start; srv_cmake
    vm "export PATH=$W/cmake-tool/root/bin:\$PATH; rm -rf $W/srv $W/ref-corpus;
        cmake -S $W/src -B $W/srv -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DMAPDEMO_ZSTD_URL=$W/zstd-1.5.7.tar.gz \
            -DXOFFSET_BUILD_DEMOS=ON -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF \
        && cmake --build $W/srv -j $JOBS --target mapdemo_a mapdemo_b mapdemo_selftest test_cross_compiler_wire \
        && $W/srv/bin/Release/test_cross_compiler_wire --write $W/ref-corpus" > "$L/srv_build.log" 2>&1 \
        || die "server build failed ($L/srv_build.log)"
    vm "uname -srm; g++ --version | head -1" > "$W/srv_env.txt" 2>&1
    log "build done"
}

# ---------------------------------------------------------------- run on the phone
finish() {
    local rc=$1
    log "finish rc=$rc"
    {
        echo "# Android phone run ($VERSION, $SHA7)"
        echo
        echo "- $(utc), host $(uname -sm), work $W"
        [ -f "$XO_OUT/device.txt" ] && echo "- phone: $(sed -n 's/^model=//p' "$XO_OUT/device.txt"), SoC $(sed -n 's/^soc=//p' "$XO_OUT/device.txt"), Android $(sed -n 's/^android=//p' "$XO_OUT/device.txt") (device.txt)"
        [ -f "$W/srv_env.txt" ] && echo "- servers: $(tr '\n' ' ' < "$W/srv_env.txt")in docker"
        echo "- exit $rc"
        echo
        echo "| step | result | detail |"
        echo "| --- | --- | --- |"
        printf '%s' "$ROWS"
        [ -n "${STOPMSG:-}" ] && { echo; echo "STOP: $STOPMSG"; }
    } > "$XO_OUT/RESULT.md"
    cat "$XO_OUT/RESULT.md"
    if [ -n "${ADB:-}" ]; then "$ADB" reverse --remove-all > /dev/null 2>&1; "$ADB" shell "rm -rf $DEV" > /dev/null 2>&1; fi
    ( cd "$XO_OUT" && find . -type f ! -name SHA256SUMS | sort | while read -r f; do echo "$(sha "$f")  $f"; done > SHA256SUMS )
    tar -czf "$XO_OUT.tar.gz" -C "$(dirname "$XO_OUT")" "$(basename "$XO_OUT")"
    echo "result: $XO_OUT/RESULT.md; send back $XO_OUT.tar.gz (sha256 $(sha "$XO_OUT.tar.gz"))"
    exit "$rc"
}
phone() {  # the one phone: ANDROID_SERIAL or the only attached device
    local n
    if [ -z "${ANDROID_SERIAL:-}" ]; then
        n=$("$ADB" devices | awk 'NR > 1 && $2 == "device"' | wc -l | tr -d ' ')
        [ "$n" = 1 ] || die "$n devices ready (adb devices -l); attach one phone or set ANDROID_SERIAL"
        ANDROID_SERIAL=$("$ADB" devices | awk 'NR > 1 && $2 == "device" {print $1; exit}'); export ANDROID_SERIAL
    fi
    [ "$("$ADB" get-state 2>/dev/null)" = device ] || die "phone $ANDROID_SERIAL not ready: unlock it and accept the USB debugging prompt (adb devices -l shows 'unauthorized' until then)"
    gp() { "$ADB" shell getprop "$1" 2>/dev/null | tr -d '\r'; }
    {
        echo "serial=$ANDROID_SERIAL"
        echo "model=$(gp ro.product.manufacturer) $(gp ro.product.model)"
        echo "soc=$(gp ro.soc.manufacturer) $(gp ro.soc.model)"
        echo "android=$(gp ro.build.version.release) (API $(gp ro.build.version.sdk))"
        echo "abi=$(gp ro.product.cpu.abi)"
        echo "fingerprint=$(gp ro.build.fingerprint)"
        echo "page_size=$("$ADB" shell getconf PAGE_SIZE 2>/dev/null | tr -d '\r')"
        "$ADB" shell uname -a 2>/dev/null | tr -d '\r'
        "$ADB" shell cat /proc/meminfo 2>/dev/null | tr -d '\r' | head -2
    } > "$XO_OUT/device.txt"
    [ "$(sed -n 's/^abi=//p' "$XO_OUT/device.txt")" = arm64-v8a ] || die "the phone's ABI is not arm64-v8a (device.txt)"
    row phone ready "$(sed -n 's/^model=//p; s/^android=/Android /' "$XO_OUT/device.txt" | tr '\n' ' ')"
}
cmd_run() {
    find_adb || die "no adb: run setup first"
    [ -f "$W/android/compile_tests.txt" ] && [ -d "$W/ref-corpus" ] || die "nothing built in $W: run build first"
    XO_OUT=${XO_OUT:-$W/out-$(date -u +%Y%m%dT%H%M%SZ)}; mkdir -p "$XO_OUT"; XO_OUT=$(cd "$XO_OUT" && pwd)
    export ADB XO_ADB_STAMPS=$W/adb-stamps-$$
    phone
    "$ADB" shell "rm -rf $DEV; mkdir -p $DEV" > /dev/null
    cp "$W/android/host_compile_tests.txt" "$XO_OUT/"
    row "compile-fail tests (host, same NDK)" "see detail" "$(grep -E 'tests passed|tests failed' "$W/android/host_compile_tests.txt" | tail -1)"
    local B=$W/android e s
    if ! skip ctest; then
        log "run 1/5: CTest on the phone (each test program pushed and run with adb)"
        ( cd "$B" && ctest --test-dir "$B" -E "^($(paste -sd'|' - < "$B/compile_tests.txt"))\$" --output-on-failure --timeout 1800 ) > "$XO_OUT/ctest.log" 2>&1; e=$?
        s=$(grep -E 'tests passed|tests failed' "$XO_OUT/ctest.log" | tail -1)
        if [ $e = 0 ]; then row "library CTest (phone)" passed "$s"; else row "library CTest (phone)" "**failed** (rc $e)" "$s (ctest.log)"; fail; fi
    fi
    if ! skip corpus; then
        log "run 2/5: wire corpus"
        local CW=$B/bin/Release/test_cross_compiler_wire
        rm -rf "$W/corpus-phone"
        "$W/src/tools/device/adb_run.sh" "$CW" --write "$W/corpus-phone" > "$XO_OUT/corpus-write.log" 2>&1 \
            && "$W/src/tools/device/adb_run.sh" "$CW" --read "$W/ref-corpus" > "$XO_OUT/corpus-read.log" 2>&1 \
            && diff -r "$W/ref-corpus" "$W/corpus-phone" > "$XO_OUT/corpus-diff.log" 2>&1; e=$?
        ( cd "$W/corpus-phone" 2>/dev/null && for f in *; do echo "$(sha "$f")  $f"; done ) > "$XO_OUT/corpus.sha256"
        if [ $e = 0 ]; then row "wire corpus" passed "$(ls "$W/ref-corpus" | wc -l | tr -d ' ') reference wires (Linux GCC 16 C++26) read, written bytes identical"
        else row "wire corpus" "**failed** (rc $e)" "corpus-*.log"; fail; fi
    fi
    skip slg || slg_phone
    skip mem || mem_phone
    skip demo || demo_phone
    finish $RC
}
demo_phone() {
    log "run 5/5: demo, servers in docker, clients on the phone through adb reverse"
    docker rm -f "$RT" > /dev/null 2>&1; srv_start   # a fresh container publishes the current $PORTS
    local r p lo hi CB=$W/bin-phone c targets="" unders="" names="" model e
    for r in $PORTS; do
        lo=${r%-*}; hi=${r#*-}; p=$lo
        while [ $p -le $hi ]; do "$ADB" reverse tcp:$p tcp:$p > /dev/null || { row demo "**not run**" "adb reverse tcp:$p failed"; fail; return; }; p=$((p + 1)); done
    done
    mkdir -p "$CB" "$W/srv-bin"
    for c in mapdemo_a mapdemo_b mapdemo_selftest; do
        printf '#!/bin/bash\nexec docker exec -i %s %s/srv/bin/Release/%s "$@"\n' "$RT" "$W" "$c" > "$W/srv-bin/$c"
    done
    chmod +x "$W/srv-bin"/mapdemo_*
    model=$(sed -n 's/^model=//p' "$XO_OUT/device.txt")
    for c in mapdemo_client mapdemo_client_noexc; do
        [ -f "$W/android/bin/Release/$c" ] || continue
        "$ADB" shell "mkdir -p $DEV/bin" > /dev/null && "$ADB" push "$W/android/bin/Release/$c" "$DEV/bin/$c" > /dev/null && "$ADB" shell chmod 755 "$DEV/bin/$c"
        # run_e2e.sh calls this as its client: --csv-dir is a phone directory pulled back afterwards,
        # --replay DIR is pushed once; an online run first waits until A listens in the container
        # (the published port accepts before that and then closes)
        cat > "$CB/$c" <<WRAP
#!/bin/bash
out=""; args=""; port=""; off=0
while [ \$# -gt 0 ]; do
    case "\$1" in
        --csv-dir) out=\$2; shift 2 ;;
        --replay)
            if [ ! -f "$XO_ADB_STAMPS/corpus" ]; then
                "$ADB" shell "rm -rf $DEV/corpus; mkdir -p $DEV/corpus" && "$ADB" push "\$2/." "$DEV/corpus/" > /dev/null && mkdir -p "$XO_ADB_STAMPS" && touch "$XO_ADB_STAMPS/corpus"
            fi
            args="\$args --replay $DEV/corpus"; off=1; shift 2 ;;
        --self-check) args="\$args \$1"; off=1; shift ;;
        --port) port=\$2; args="\$args \$1 \$2"; shift 2 ;;
        *) args="\$args \$1"; shift ;;
    esac
done
if [ \$off = 0 ] && [ -n "\$port" ]; then
    hex=\$(printf '%04X' "\$port"); t=0
    until docker exec $RT sh -c "grep -qE '^ *[0-9]+: [0-9A-F]+:\$hex [0-9A-F]+:0000 0A' /proc/net/tcp /proc/net/tcp6 2>/dev/null"; do
        sleep 0.2; t=\$((t + 1)); [ \$t -lt 300 ] || { echo "error: A did not listen on \$port within 60 s" >&2; exit 3; }
    done
fi
d=$DEV/run/\$(echo "\$out" | tr '/' '_')
"$ADB" shell "rm -rf \$d; mkdir -p \$d"
"$ADB" shell "cd \$d && $DEV/bin/$c --host 127.0.0.1\$args --csv-dir \$d"
rc=\$?
mkdir -p "\$out"
for f in \$("$ADB" shell "ls \$d" | tr -d '\r'); do "$ADB" pull "\$d/\$f" "\$out/" > /dev/null; done
exit \$rc
WRAP
        chmod +x "$CB/$c"
        case $c in mapdemo_client) targets="$targets android=$CB/$c"; names="$names android"; unders="android=$model (USB, adb reverse)" ;;
                   *) targets="$targets android-noexc=$CB/$c"; names="$names android-noexc"; unders="$unders;android-noexc=$model (USB, adb reverse)" ;; esac
    done
    [ -n "$targets" ] || { row demo "**not run**" "no demo client in $W/android"; fail; return; }
    vm "pkill -x mapdemo_a; pkill -x mapdemo_b; true" > /dev/null 2>&1
    rm -rf "$W/demo"
    # shellcheck disable=SC2086
    MAPDEMO_RUNS_UNDER="$unders" MAPDEMO_A_HOST=0.0.0.0 MAPDEMO_PORT_BASE=47800 \
        MAPDEMO_LABEL="Android phone $model; Linux $(uname -m | sed 's/arm64/aarch64/') GCC 16 C++26 servers in docker; $VERSION $SHA7" \
        bash "$W/src/demo/mapstate/tools/run_e2e.sh" "$W/srv-bin" "$W/demo" $targets > "$XO_OUT/demo_run.log" 2>&1; e=$?
    cp -R "$W/demo" "$XO_OUT/demo"
    if [ $e = 0 ]; then row demo passed "targets$names: self-check, replay, 1/100/10000 units x 500 steps (dense/compact/zstd), evil: E2E PASSED (demo/E2E_RESULT.md)"
    else row demo "**failed** (rc $e)" "targets$names: demo_run.log, demo/E2E_RESULT.md"; fail; fi
}
thermal() {  # thermal() FILE: thermal status, battery and power state before a timed run
    {
        echo "utc $(utc)"
        "$ADB" shell dumpsys thermalservice 2>/dev/null | tr -d '\r' | grep -m1 'Thermal Status' | sed 's/^ *Thermal Status: */thermal_status /'
        "$ADB" shell dumpsys battery 2>/dev/null | tr -d '\r' | grep -E '^ *(level|temperature|AC powered|USB powered|status):' | sed 's/^ *//; s/: / /; s/ /_/'
        "$ADB" shell settings get global low_power 2>/dev/null | tr -d '\r' | sed 's/^/low_power /'
    } > "$1"
}
slg_phone() {
    local B=$W/android d=$XO_OUT/slg i e runs=${XO_SLG_RUNS:-3} scale=${XO_SLG_SCALE:-M} cycles=${XO_SLG_CYCLES:-41}
    local SLG=$B/bin/Release/xo_slg_bench RUN=$W/src/tools/device/adb_run.sh
    log "run 3/5: slg (SC-6): verify, $runs runs scale $scale x $cycles cycles"
    mkdir -p "$d"
    [ -f "$SLG" ] || { row slg "**not run**" "no xo_slg_bench in $B"; fail; return; }
    "$RUN" "$SLG" --mode verify --scale S > "$d/verify.log" 2>&1 || { row slg "**failed**" "verify (slg/verify.log)"; fail; return; }
    for i in $(seq 1 "$runs"); do
        thermal "$d/info_$i.txt"
        rm -f "$d/run_$i.jsonl"
        SLG_REUSE=1 "$RUN" "$SLG" --mode bench --scale "$scale" --cycles "$cycles" --out "$d/run_$i.jsonl" > "$d/run_$i.log" 2>&1; e=$?
        [ $e = 0 ] || { row slg "**failed**" "run $i rc $e (slg/run_$i.log)"; fail; return; }
        python3 "$W/src/bench/slg/tools/analyze.py" "$d/run_$i.jsonl" > "$d/run_$i.md" 2>&1
    done
    cat "$d"/run_*.jsonl > "$d/all.jsonl"
    python3 "$W/src/bench/slg/tools/analyze.py" "$d/all.jsonl" > "$d/ANALYSIS.md" 2>&1
    [ -f "$W/fb/flatc_version.txt" ] && cp "$W/fb/flatc_version.txt" "$d/" && echo "$(sha "$W/fb/slg_generated.h")  slg_generated.h" > "$d/slg_generated.sha256"
    row slg passed "verify ok; $runs runs scale $scale x $cycles cycles$([ -f "$W/fb/flatc_version.txt" ] && echo ', with FlatBuffers') (slg/ANALYSIS.md); thermal status before runs: $(cat "$d"/info_*.txt | sed -n 's/^thermal_status //p' | tr '\n' ' ')"
}
mem_phone() {
    local d=$XO_OUT/mem e MP=$W/android/bin/Release/xo_mem_probe RUN=$W/src/tools/device/adb_run.sh limit
    limit=${XO_MEM_LIMIT:-$(awk '/^MemTotal:/ {print int($2 / 1024)}' "$XO_OUT/device.txt")}
    log "run 4/5: memory probe (SC-12), commit up to $limit MiB or until killed"
    mkdir -p "$d"
    [ -f "$MP" ] || { row mem "**not run**" "no xo_mem_probe in $W/android"; fail; return; }
    "$RUN" "$MP" --single > "$d/single.txt" 2>&1 || { row mem "**failed**" "--single (mem/single.txt)"; fail; return; }
    "$RUN" "$MP" --va 1024 > "$d/va_1024.txt" 2>&1
    "$RUN" "$MP" --va 64 > "$d/va_64.txt" 2>&1
    "$ADB" shell cat /proc/meminfo 2>/dev/null | tr -d '\r' > "$d/meminfo_before.txt"
    rm -rf "$W/mem-progress"
    "$RUN" "$MP" --commit 64 "$limit" "$W/mem-progress/commit_progress.txt" > "$d/commit.txt" 2>&1; e=$?
    cp "$W/mem-progress/commit_progress.txt" "$d/" 2>/dev/null
    "$ADB" logcat -d -b main,system,events 2>/dev/null | tr -d '\r' | grep -iE 'lowmemorykiller|lmkd|xo_mem_probe' | tail -40 > "$d/lmkd.txt"
    {
        echo "# memory probe (Android)"; echo
        echo "- phone: $(sed -n 's/^model=//p' "$XO_OUT/device.txt"), $(sed -n 's/^android=//p' "$XO_OUT/device.txt"), $(grep '^MemTotal:' "$XO_OUT/device.txt"), page size $(sed -n 's/^page_size=//p' "$XO_OUT/device.txt")"
        echo "- single buffer: $(grep -E '^(single_max_capacity_mib|available_mib) ' "$d/single.txt" | tr '\n' ' ')"
        echo "- 1 GiB reservations: $(grep -E '^(va_buffers|va_reserved_gib|va_stop) ' "$d/va_1024.txt" | tr '\n' ' ')"
        echo "- 64 MiB reservations: $(grep -E '^(va_buffers|va_reserved_gib|va_stop) ' "$d/va_64.txt" | tr '\n' ' ')"
        echo "- commit 64 MiB steps up to $limit MiB: rc $e (137 = killed by the system); last progress line: $(tail -1 "$d/commit_progress.txt" 2>/dev/null)"
        echo "- low memory killer lines: mem/lmkd.txt"
    } > "$d/SUMMARY.md"
    row mem passed "mem/SUMMARY.md (commit probe rc $e: 137 = killed by the system at the limit)"
}
cmd_clean() {
    docker rm -f "$RT" > /dev/null 2>&1
    find_adb && { "$ADB" reverse --remove-all > /dev/null 2>&1; "$ADB" shell "rm -rf $DEV" > /dev/null 2>&1; }
    log "removed container $RT and $DEV on the phone; $W stays (delete it by hand)"
}
cmd_all() { cmd_setup && cmd_build && cmd_run; }

# ---------------------------------------------------------------- dispatch
case "$CMD" in -h|--help|help) sed -n '2,44p' "$0"; exit 0 ;; esac
type "cmd_$CMD" > /dev/null 2>&1 || { echo "usage: $0 [all|setup|build|run|clean]" >&2; exit 2; }
if [ "$CMD" != setup ] && [ "$CMD" != all ]; then find_adb; find_ndk; fi
"cmd_$CMD"
