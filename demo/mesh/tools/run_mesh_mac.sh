#!/bin/bash
# MESH_MAC: unattended, NOT timed batch for mac_queue.sh (macOS bash 3.2).
# Device mesh demo, phases 3 and 4 on the Mac (demo/mesh/INTERFACES.md 6 and 7).
# All numbers it prints are emulator / simulator numbers, not phone numbers.
#
#   run_mesh_mac.sh [all|live|mailbox]        (default all: live, then mailbox)
#
# One input tarball (PACK below, built in the cloud by tools/pack_mesh_mac.sh) carries
# the source of one commit (with the header-only submodules), the Android arm64 device
# programs (NDK r29, C++20 field lists, API 28, PIE) and the cloud's mailbox for the
# Mac devices (mac_inbox/: Job and Hop files addressed to ids 101-108).
#
# Devices (tools/mac_devices.txt), each built with and without exceptions:
#   macos-arm64-clang       Apple clang, C++20, native
#   ios-sim-arm64           iOS Simulator SDK; xo_peer runs inside the test host app
#                           (tools/test_host, libxoh_xo_peer, xo_host_run.py --backend sim),
#                           as on a phone; the app holds one program at a time, so
#   ios-sim-arm64-noexc     runs on the same simulator through `xcrun simctl spawn`
#                           (XO_IOS_RUN=spawn: both through spawn)
#   android-emu-arm64       the pack's NDK programs on the Android emulator (adb), hub at 10.0.2.2
#   colima-linux-arm64-gcc  GCC 16 (gcc:16 image), C++20, in the colima VM, next to the hub
#
# 1. builds: macOS, iOS (simulator + test host app), colima (hub + devices); Android emulator
#    booted, programs pushed. Self-tests (xmesh_selftest[_noexc] --corpus-check) on all four.
# 2. live (phase 3): xo_hub (Linux arm64, in colima, ports 47801/47802 published on the Mac)
#    and the 8 devices: every ordered pair x 6 types x small/medium x verified/compact, one
#    chain per type and scale, every evil case for every pair. status.json is saved every
#    5 s while it runs; mesh/MESH_RESULT.md is mesh_report.py's report.
# 3. mailbox (phase 4): the 8 devices answer the cloud's Job and Hop files (xo_peer --mailbox)
#    in rounds until nothing is left; every file they wrote goes into mailbox/outbox.tar.gz
#    for the cloud (tools/run_mailbox.sh post), which scores the cross-site jobs.
#
# Builds go to /Volumes/XOffsetCS/mesh/<pack> and are deleted at the end (XO_KEEP_W=1 keeps
# them). Downloads (first Android run only): emulator + platform-tools + one arm64 system
# image into $ANDROID_HOME (default /Volumes/XOffsetCS/android-sdk, kept). colima profile h1
# must be stopped (started with 4 GB, stopped at the end).
#
# Results ($XOQ_OUT): RESULT.md, env.txt, build/ logs, mesh/ (hub/, devices/, selftest/,
# status.json, dashboard.html, MESH_RESULT.md), mailbox/ (outbox.tar.gz, logs/), SHA256SUMS.
#   exit 2  inputs or environment missing (nothing ran)
#   exit 3  something ran and failed (see RESULT.md)
#   exit 0  everything that ran passed
#
# RESULT.md and the clean-up (emulator, simulator, container, colima, builds) also happen when
# a step fails or the batch is interrupted (INT, TERM, HUP); the live mesh is bounded by
# XO_HUB_LIMIT_S (default 5400 s for the hub, then 120 s for the devices). Run it under nohup.
#
# XO_MAILBOX_PRIOR=<an earlier run's mailbox/outbox.tar.gz>: the mailbox starts from that
# run's answers, the devices answer only what is left (e.g. the devices that did not run then)
# and outbox.tar.gz holds only the new files; run_mailbox.sh post takes both outputs.
set -u -o pipefail
# xcode-select may still point at the Command Line Tools; xcrun simctl and the iOS SDK need Xcode
if [ -z "${DEVELOPER_DIR:-}" ] && [ -d /Applications/Xcode.app/Contents/Developer ] \
   && ! xcode-select -p 2>/dev/null | grep -q '\.app/'; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
PACK=${XO_PACK:-$(cd "$(dirname "$0")" && pwd)/mesh_mac_pack_@SHA7@.tar.gz}   # next to this script
PACK_SHA=${XO_PACK_SHA:-@PACK_SHA@}
WHAT=${1:-all}
XOQ_OUT=${XOQ_OUT:-$PWD/out}; mkdir -p "$XOQ_OUT"; XOQ_OUT=$(cd "$XOQ_OUT" && pwd)
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
M=${XO_M:-/Volumes/XOffsetCS/mesh}      # unpack root
export ANDROID_HOME=${ANDROID_HOME:-/Volumes/XOffsetCS/android-sdk}
export ANDROID_SDK_ROOT=$ANDROID_HOME
export ANDROID_AVD_HOME=$M/avd
AVD=xo-arm64
EMU_PORT=5580
export ANDROID_SERIAL=emulator-$EMU_PORT
ADB=$ANDROID_HOME/platform-tools/adb
H=${XO_H1:-/Volumes/XOffsetCS/h1}       # colima home of the GCC 16 Mac jobs
export COLIMA_HOME=$H/colima
PROF=h1
export DOCKER_CONTEXT=${XO_DOCKER_CONTEXT:-colima-$PROF}
G=${XO_IMAGE:-gcc@sha256:325784a7e64cece62cc2ba202f8ee9e9b6e09dbcecabe98697a79a41148b177a}  # gcc:16 linux/arm64
RT=xo-mesh-hub
PORT=47801
HTTP_PORT=47802
DEV=/data/local/tmp/xo-mesh
XO_IOS_RUN=${XO_IOS_RUN:-host}
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
log() { echo "== $(utc) $*"; }
sha() { shasum -a 256 "$1" | awk '{print $1}'; }
freeg() { df -g "$1" 2>/dev/null | awk 'NR==2 {print $4}'; }
ROWS=""
row() { ROWS="$ROWS| $1 | $2 | $3 |
"; }
RC=0
fail() { RC=3; }
EMU_PID=""; IOS_UDID=""; VM_UP=0; RT_UP=0; W=""
BG_PIDS=""       # live mesh background jobs (hub, poller, watchdog, devices), stopped by finish
FINISHED=0

# kill_tree PID: PID and its children (a device's subshell runs xo_peer, adb or python3)
kill_tree() { pkill -TERM -P "$1" 2>/dev/null; kill -TERM "$1" 2>/dev/null; }
finish() {
    local rc=$1 p
    [ $FINISHED = 0 ] || return 0
    FINISHED=1
    trap - INT TERM HUP
    log "finish rc=$rc"
    for p in $BG_PIDS; do kill_tree "$p"; done
    {
        echo "# Device mesh on the Mac: live mesh and cross-site mailbox (EMULATOR/SIMULATOR numbers)"
        echo
        echo "- $(utc), pack $(basename "$PACK") sha256 $PACK_SHA"
        [ -f "$XOQ_OUT/pack_info.txt" ] && sed 's/^/- /' "$XOQ_OUT/pack_info.txt"
        echo "- exit $rc"
        echo
        echo "| step | result | detail |"
        echo "| --- | --- | --- |"
        printf '%s' "$ROWS"
        [ -n "${STOPMSG:-}" ] && { echo; echo "STOP: $STOPMSG"; }
        [ -f "$XOQ_OUT/mesh/MESH_RESULT.md" ] && { echo; sed -n 3p "$XOQ_OUT/mesh/MESH_RESULT.md"; }
    } > "$XOQ_OUT/RESULT.md"
    cat "$XOQ_OUT/RESULT.md"
    [ -n "$EMU_PID" ] && { "$ADB" emu kill > /dev/null 2>&1; sleep 5; kill "$EMU_PID" 2>/dev/null; }
    [ -n "$IOS_UDID" ] && { xcrun simctl shutdown "$IOS_UDID" > /dev/null 2>&1; xcrun simctl delete "$IOS_UDID" > /dev/null 2>&1; }
    [ $RT_UP = 1 ] && docker rm -f "$RT" > /dev/null 2>&1
    [ $VM_UP = 1 ] && colima stop --profile $PROF > /dev/null 2>&1
    # everything worth keeping is in $XOQ_OUT; the Android SDK in $ANDROID_HOME stays for later runs
    rm -rf "${ANDROID_AVD_HOME:?}"
    [ -n "$W" ] && [ -d "$W" ] && [ "${XO_KEEP_W:-0}" != 1 ] && rm -rf "${W:?}"
    ( cd "$XOQ_OUT" && find . -type f ! -name SHA256SUMS | sort | while read -r f; do shasum -a 256 "$f"; done > SHA256SUMS )
    exit "$rc"
}
stop() { STOPMSG=$1; echo "STOP: $1"; finish "${2:-2}"; }
# an interrupted or failing batch still writes RESULT.md and cleans up (finish runs once)
on_signal() { STOPMSG="interrupted by SIG$1"; echo "STOP: $STOPMSG"; finish 3; }
trap 'on_signal INT' INT
trap 'on_signal TERM' TERM
trap 'on_signal HUP' HUP
on_exit() {  # an exit that did not go through finish (a plain `exit`, an error): keep its code
    local rc=$1
    [ $FINISHED = 1 ] && return
    [ "$rc" = 0 ] && rc=3
    STOPMSG=${STOPMSG:-"the batch exited (rc $1) before it finished"}
    finish "$rc"
}
trap 'on_exit $?' EXIT

# ---------------------------------------------------------------- 0. inputs
log "0. inputs ($WHAT)"
case "$WHAT" in all|live|mailbox) ;; *) echo "usage: $0 [all|live|mailbox]"; exit 2 ;; esac
case "$PACK$PACK_SHA" in *@*) echo "STOP: PACK/PACK_SHA not filled in"; exit 2 ;; esac
for c in python3 curl unzip shasum colima docker tar cmake ninja xcrun; do command -v $c > /dev/null || stop "$c not found"; done
# the pack travels in 10 MB parts (<pack>.part-00, -01, ...): join them when the pack is missing
if [ ! -f "$PACK" ] && [ -f "$PACK.part-00" ]; then cat "$PACK".part-?? > "$PACK.tmp" && mv "$PACK.tmp" "$PACK"; fi
[ -f "$PACK" ] && [ "$(sha "$PACK")" = "$PACK_SHA" ] || stop "$PACK missing or sha256 differs"
mkdir -p "$M"
TOP=$(tar -tzf "$PACK" | head -1 | cut -d/ -f1)
[ ! -e "$M/$TOP" ] || stop "$M/$TOP exists (left over from an earlier attempt: look before removing it)"
W=$M/$TOP
tar -xzf "$PACK" -C "$M" || stop "unpack failed"
SRC=$W/src
cp "$W/PACK_INFO.txt" "$XOQ_OUT/pack_info.txt"
DEVICES=$SRC/demo/mesh/tools/mac_devices.txt
[ -f "$DEVICES" ] && [ -d "$W/mac_inbox/inbox" ] && [ -d "$W/android/bin" ] || stop "pack without mac_devices.txt, mac_inbox/ or android/bin/"
# the cloud planned with the same ids and names
diff <(awk '!/^#/ && NF == 3' "$DEVICES") <(awk '$3 == "mac"' "$W/mac_inbox/devices.txt") > /dev/null \
    || stop "mac_inbox/devices.txt does not list the Mac devices of mac_devices.txt"
id_of() { awk -v n="$1" '!/^#/ && $2 == n {print $1}' "$DEVICES"; }
CPU=$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null || sysctl -n hw.ncpu)
{ sw_vers; uname -m; sysctl -n machdep.cpu.brand_string; echo "mem $(( $(sysctl -n hw.memsize) / 1073741824 )) GB"
  echo "free: data $(freeg /System/Volumes/Data) GB, XOffsetCS $(freeg /Volumes/XOffsetCS) GB"
  xcodebuild -version 2>&1 | head -2; clang++ --version | head -1; cmake --version | head -1
  echo "ninja $(ninja --version 2>&1)"; python3 --version; colima version | head -1; } > "$XOQ_OUT/env.txt" 2>&1
[ "${XO_SKIP_DISK_CHECK:-0}" = 1 ] || [ "$(freeg /System/Volumes/Data)" -ge 8 ] || stop "less than 8 GB free on the Mac disk"
[ "${XO_SKIP_DISK_CHECK:-0}" = 1 ] || [ "$(freeg /Volumes/XOffsetCS)" -ge 8 ] || stop "less than 8 GB free on /Volumes/XOffsetCS"
mkdir -p "$XOQ_OUT/build" "$XOQ_OUT/mesh/selftest" "$XOQ_OUT/mesh/devices" "$XOQ_OUT/mailbox/logs"
CMAKE_COMMON="-G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_BUILD_MESH_DEMO=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF -DXOFFSET_CXX_STANDARD=20"
PROGS="xo_peer xo_peer_noexc xmesh_selftest xmesh_selftest_noexc"
OK_MAC=0; OK_IOS=0; OK_AND=0; OK_VM=0

# ---------------------------------------------------------------- 1a. macOS
MB=$W/b-macos
log "1a. macOS build (Apple clang, C++20)"
# shellcheck disable=SC2086
cmake -S "$SRC" -B "$MB" $CMAKE_COMMON -DXMESH_PEER_ONLY=ON > "$XOQ_OUT/build/macos-configure.log" 2>&1 \
    && cmake --build "$MB" --parallel "$CPU" --target $PROGS > "$XOQ_OUT/build/macos-build.log" 2>&1
if [ $? = 0 ]; then OK_MAC=1; row "macos build" built "Apple clang $(clang++ --version | head -1 | sed 's/.*version //; s/ .*//'), C++20 field lists"
else row "macos build" "**failed**" "build/macos-*.log"; fail; fi
MACBIN=$MB/bin/Release

# ---------------------------------------------------------------- 1b. iOS Simulator
IB=$W/b-ios
ios_up() {
    log "1b. iOS Simulator and build (simulator SDK, Apple clang C++20, test host app)"
    xcrun --sdk iphonesimulator --show-sdk-path > "$XOQ_OUT/build/ios-sdk.txt" 2>&1 || { row ios "**not run**" "no Xcode / iOS simulator SDK (build/ios-sdk.txt)"; fail; return; }
    local rt dt pairs
    # (runtime, iPhone) pairs from each runtime's own supportedDeviceTypes, newest runtime first,
    # numbered iPhones first; the first pair simctl can create is used. XO_IOS_RUNTIME / XO_IOS_DEVICE override
    pairs=$(xcrun simctl list -j runtimes | XO_IOS_RUNTIME="${XO_IOS_RUNTIME:-}" XO_IOS_DEVICE="${XO_IOS_DEVICE:-}" python3 -c '
import json, os, re, sys
want_rt, want_dt = os.environ["XO_IOS_RUNTIME"], os.environ["XO_IOS_DEVICE"]
rts = [r for r in json.load(sys.stdin)["runtimes"] if r.get("isAvailable") and r.get("platform", "iOS") == "iOS" and "iOS" in r["name"]]
rts.sort(key=lambda r: [int(v) for v in r["version"].split(".") if v.isdigit()], reverse=True)
if want_rt: rts = [r for r in rts if want_rt in (r["identifier"], r["name"], r["version"])]
out = []
for ri, r in enumerate(rts):
    for d in r.get("supportedDeviceTypes", []):
        n = d["name"]
        if d.get("productFamily", "iPhone") != "iPhone" or not n.startswith("iPhone"): continue
        if want_dt and want_dt not in (d["identifier"], n): continue
        numbered = re.match(r"iPhone \d+[a-z]?$", n) is not None
        variant = any(w in n for w in ("Pro", "Plus", "SE", "Max", "mini"))
        num = re.match(r"iPhone (\d+)", n)
        out.append(((num is None, ri, variant, not numbered, -int(num.group(1)) if num else 0, n), r["identifier"], d["identifier"]))
for _, rt, dt in sorted(out)[:6]: print(rt, dt)
')
    [ -n "$pairs" ] || { row ios "**not run**" "no available iOS simulator runtime with an iPhone (Xcode > Settings > Components)"; fail; return; }
    xcrun simctl delete xo-mesh-sim > /dev/null 2>&1 || true
    IOS_UDID=""
    while read -r rt dt; do
        IOS_UDID=$(xcrun simctl create xo-mesh-sim "$dt" "$rt" 2>> "$XOQ_OUT/build/ios-simctl-create.log") && break
        echo "create $dt on $rt failed" >> "$XOQ_OUT/build/ios-simctl-create.log"; IOS_UDID=""
    done <<< "$pairs"
    [ -n "$IOS_UDID" ] || { row ios "**not run**" "simctl create failed for every pair (build/ios-simctl-create.log)"; fail; return; }
    xcrun simctl boot "$IOS_UDID" && xcrun simctl bootstatus "$IOS_UDID" -b > /dev/null 2>&1 || { row ios "**not run**" "simulator did not boot"; fail; return; }
    row "ios simulator" booted "$(echo "$rt" | sed 's/.*SimRuntime\.//'), $(echo "$dt" | sed 's/.*SimDeviceType\.//')"
    # shellcheck disable=SC2086
    cmake -S "$SRC" -B "$IB" $CMAKE_COMMON -DXMESH_PEER_ONLY=ON -DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/toolchains/ios-simulator.cmake" \
        -DXOFFSET_IOS_SIM_DEVICE="$IOS_UDID" -DXOFFSET_TEST_HOST=ON > "$XOQ_OUT/build/ios-configure.log" 2>&1 \
        && cmake --build "$IB" --parallel "$CPU" --target $PROGS xoffset_test_host_bundle > "$XOQ_OUT/build/ios-build.log" 2>&1
    if [ $? = 0 ] && [ -d "$IB/bin/Release/xoffset_test_host.app" ]; then
        OK_IOS=1; row "ios build" built "xo_peer[_noexc], xmesh_selftest[_noexc] and the test host app with libxoh_xo_peer"
    else row "ios build" "**failed**" "build/ios-*.log"; fail; fi
}
ios_up
IOSBIN=$IB/bin/Release
export XO_HOST_BACKEND=sim XO_HOST_APP=$IOSBIN/xoffset_test_host.app XO_HOST_STATE=$IB/host-state XO_HOST_TIMEOUT=5400
# ios_peer EXC|NOEXC ARGS...: one xo_peer run on the simulator (exc: the test host app unless XO_IOS_RUN=spawn)
ios_peer() {
    local which=$1; shift
    if [ "$which" = exc ] && [ "$XO_IOS_RUN" = host ]; then
        XO_HOST_DEVICE=$IOS_UDID python3 "$SRC/tools/test_host/xo_host_run.py" "$IOSBIN/xo_peer" "$@"
    elif [ "$which" = exc ]; then
        xcrun simctl spawn "$IOS_UDID" "$IOSBIN/xo_peer" "$@"
    else
        xcrun simctl spawn "$IOS_UDID" "$IOSBIN/xo_peer_noexc" "$@"
    fi
}

# ---------------------------------------------------------------- 1c. Android emulator
android_sdk() {  # emulator, platform-tools, one arm64 system image; nothing when present
    [ -x "$ANDROID_HOME/emulator/emulator" ] && [ -x "$ADB" ] && ls -d "$ANDROID_HOME"/system-images/android-*/google_apis/arm64-v8a > /dev/null 2>&1 && return 0
    mkdir -p "$ANDROID_HOME/platforms" "$M/dl"
    python3 - "$ANDROID_HOME" "$M/dl" > "$XOQ_OUT/build/android-sdk-download.log" 2>&1 <<'PY'
import hashlib, os, sys, urllib.request, xml.etree.ElementTree as ET, zipfile
sdk, dl = sys.argv[1], sys.argv[2]
BASE = "https://dl.google.com/android/repository/"
def get(url):
    with urllib.request.urlopen(url, timeout=600) as r: return r.read()
def tag(e): return e.tag.split('}')[-1]
def kids(e, name): return [c for c in e if tag(c) == name]
def kid(e, name):
    k = kids(e, name); return k[0] if k else None
def stable(pkg):
    ref = kid(pkg, "channelRef"); return ref is None or ref.get("ref") == "channel-0"
def rev(pkg):
    r = kid(pkg, "revision"); return tuple(int((kid(r, n).text if kid(r, n) is not None else 0)) for n in ("major", "minor", "micro"))
def archive(pkg):
    for a in kids(kid(pkg, "archives"), "archive"):
        os_ = kid(a, "host-os"); arch = kid(a, "host-arch")
        if os_ is not None and os_.text != "macosx": continue
        if arch is not None and arch.text not in ("aarch64", "arm64"): continue
        c = kid(a, "complete")
        return kid(c, "url").text, kid(c, "checksum").text, int(kid(c, "size").text)
    return None
def fetch(base, pkg, dest_parent):
    url, sha1, size = archive(pkg)
    path = os.path.join(dl, os.path.basename(url))
    if not os.path.exists(path):
        print("download", base + url, size, flush=True)
        data = get(base + url)
        open(path, "wb").write(data)
    h = hashlib.sha1(open(path, "rb").read()).hexdigest()
    if h != sha1: sys.exit("sha1 mismatch for %s: %s != %s" % (url, h, sha1))
    os.makedirs(dest_parent, exist_ok=True)
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            out = z.extract(info, dest_parent)
            mode = info.external_attr >> 16
            if mode: os.chmod(out, mode & 0o7777)
    os.remove(path)
    print("installed", pkg.get("path"), "rev", rev(pkg), "into", dest_parent, flush=True)
repo = ET.fromstring(get(BASE + "repository2-3.xml"))
pk = [p for p in repo if tag(p) == "remotePackage"]
for name in ("emulator", "platform-tools"):
    cands = [p for p in pk if p.get("path") == name and stable(p) and archive(p)]
    if not cands: sys.exit("no %s package for macOS arm64" % name)
    fetch(BASE, max(cands, key=rev), sdk)
sbase = BASE + "sys-img/google_apis/"
si = ET.fromstring(get(sbase + "sys-img2-3.xml"))
best = None
for p in si:
    if tag(p) != "remotePackage" or not stable(p): continue
    parts = p.get("path").split(";")      # system-images;android-35;google_apis;arm64-v8a
    if len(parts) != 4 or parts[3] != "arm64-v8a" or not parts[1].split("-")[-1].isdigit(): continue
    api = int(parts[1].split("-")[-1])
    if api > 35 or not archive(p): continue
    if best is None or api > best[0]: best = (api, p)
if best is None: sys.exit("no google_apis arm64-v8a system image")
fetch(sbase, best[1], os.path.join(sdk, "system-images", "android-%d" % best[0], "google_apis"))
PY
}
android_avd() {
    local img api
    img=$(ls -d "$ANDROID_HOME"/system-images/android-*/google_apis/arm64-v8a | sort -t- -k2 -n | tail -1)
    api=$(echo "$img" | sed 's#.*/android-\([0-9]*\)/.*#\1#')
    mkdir -p "$ANDROID_AVD_HOME/$AVD.avd"
    printf 'avd.ini.encoding=UTF-8\npath=%s\npath.rel=avd/%s.avd\ntarget=android-%s\n' "$ANDROID_AVD_HOME/$AVD.avd" "$AVD" "$api" > "$ANDROID_AVD_HOME/$AVD.ini"
    cat > "$ANDROID_AVD_HOME/$AVD.avd/config.ini" <<CFG
AvdId=$AVD
avd.ini.displayname=$AVD
abi.type=arm64-v8a
hw.cpu.arch=arm64
hw.cpu.ncore=4
hw.ramSize=3072
vm.heapSize=256
disk.dataPartition.size=4G
image.sysdir.1=system-images/android-$api/google_apis/arm64-v8a/
tag.id=google_apis
tag.display=Google APIs
hw.lcd.width=1080
hw.lcd.height=2400
hw.lcd.density=420
hw.keyboard=yes
hw.gpu.enabled=yes
hw.gpu.mode=swiftshader_indirect
fastboot.forceColdBoot=yes
CFG
    echo "android-$api google_apis arm64-v8a"
}
android_up() {
    log "1c. Android SDK ($ANDROID_HOME) and emulator"
    android_sdk || { row android "**not run**" "SDK download failed (build/android-sdk-download.log)"; fail; return; }
    local img t=0 p
    img=$(android_avd)
    "$ANDROID_HOME/emulator/emulator" -avd $AVD -port $EMU_PORT -no-window -no-audio -no-boot-anim -no-snapshot \
        -gpu swiftshader_indirect -cores 4 -memory 3072 > "$XOQ_OUT/build/android-emulator.log" 2>&1 &
    EMU_PID=$!
    "$ADB" start-server > /dev/null 2>&1
    until [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ]; do
        sleep 5; t=$((t + 5))
        kill -0 $EMU_PID 2>/dev/null || { row android "**not run**" "emulator exited (build/android-emulator.log)"; fail; EMU_PID=""; return; }
        [ $t -lt 900 ] || { row android "**not run**" "emulator did not boot in 15 min (build/android-emulator.log)"; fail; return; }
    done
    { "$ADB" shell getprop ro.build.fingerprint; "$ADB" shell getprop ro.product.cpu.abi; "$ADB" shell uname -a; } \
        > "$XOQ_OUT/build/android-device.txt" 2>&1
    "$ADB" shell "rm -rf $DEV; mkdir -p $DEV/bin $DEV/run" > /dev/null
    for p in $PROGS; do
        "$ADB" push "$W/android/bin/$p" "$DEV/bin/$p" > /dev/null && "$ADB" shell chmod 755 "$DEV/bin/$p" || { row android "**failed**" "adb push $p"; fail; return; }
    done
    "$ADB" push "$SRC/demo/mesh/corpus" "$DEV/corpus" > /dev/null
    OK_AND=1
    row "android emulator" booted "$img, boot ${t}s, $(tr -d '\r' < "$XOQ_OUT/build/android-device.txt" | head -1); programs from the pack (NDK, PIE, API 28)"
}
android_up

# ---------------------------------------------------------------- 1d. colima: hub and Linux arm64 devices
VB=$W/b-colima
vm() { docker exec "$RT" bash -c "$1"; }
colima_up() {
    log "1d. colima: xo_hub and the Linux arm64 devices (GCC 16, C++20)"
    colima status --profile $PROF > /dev/null 2>&1 && { row colima "**not run**" "colima profile $PROF is already running (another batch uses it?)"; fail; return; }
    colima start --profile $PROF --vm-type vz --arch aarch64 --cpu "$CPU" --memory 4 --disk 40 \
        --mount-type virtiofs --mount "$W:w" --runtime docker > "$XOQ_OUT/build/colima-start.log" 2>&1 \
        || { row colima "**not run**" "colima start failed (build/colima-start.log)"; fail; return; }
    VM_UP=1
    docker pull -q "$G" > /dev/null || { row colima "**not run**" "docker pull $G failed"; fail; return; }
    # $RT is this batch's container (it runs `sleep infinity`; everything it writes is under $W):
    # a stopped one is left over from an earlier batch that did not finish and is removed
    if docker inspect "$RT" > /dev/null 2>&1; then
        [ "$(docker inspect -f '{{.State.Running}}' "$RT" 2>/dev/null)" = false ] \
            || { row colima "**not run**" "container $RT is running (another batch uses it?)"; fail; return; }
        docker rm "$RT" > /dev/null || { row colima "**not run**" "could not remove the stopped container $RT"; fail; return; }
        echo "removed the stopped container $RT left over from an earlier batch" >> "$XOQ_OUT/build/colima-start.log"
    fi
    docker run -d --name "$RT" -p 127.0.0.1:$PORT:$PORT -p 127.0.0.1:$HTTP_PORT:$HTTP_PORT -v "$W:$W" "$G" sleep infinity > /dev/null \
        || { row colima "**not run**" "$RT start failed"; fail; return; }
    RT_UP=1
    vm "cd $SRC && python3 tools/prepare_vm_cmake.py --platform linux-\$(uname -m) --out /opt/cmake-tool" > "$XOQ_OUT/build/colima-cmake-tool.log" 2>&1 \
        || { row colima "**not run**" "prepare_vm_cmake failed (build/colima-cmake-tool.log)"; fail; return; }
    vm "export PATH=/opt/cmake-tool/root/bin:\$PATH; cmake -S $SRC -B $VB -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DXOFFSET_BUILD_MESH_DEMO=ON \
            -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF -DXOFFSET_CXX_STANDARD=20 \
        && cmake --build $VB -j $CPU --target xo_hub $PROGS" > "$XOQ_OUT/build/colima-build.log" 2>&1 \
        || { row colima "**failed**" "hub and device build (build/colima-build.log)"; fail; return; }
    vm "uname -srm; g++ --version | head -1" > "$XOQ_OUT/build/colima-env.txt" 2>&1
    OK_VM=1
    row "colima" built "xo_hub + devices, $(sed -n 1,2p "$XOQ_OUT/build/colima-env.txt" | tr '\n' ' ')"
}
colima_up
VMBIN=$VB/bin/Release

# ---------------------------------------------------------------- 1e. self-tests
log "1e. self-tests (xmesh_selftest, xmesh_selftest_noexc --corpus-check)"
selftest() {  # selftest NAME OK COMMAND...: selftest/NAME.txt
    local name=$1 ok=$2; shift 2
    local f=$XOQ_OUT/mesh/selftest/$name.txt
    if [ "$ok" != 1 ]; then echo "exit 127 (not built)" > "$f"; return; fi
    "$@" > "$f" 2>&1
    echo "exit $?" >> "$f"
    echo "$name: $(grep -E '^RESULT' "$f" | tail -1) ($(tail -1 "$f"))"
}
CORPUS=$SRC/demo/mesh/corpus
selftest macos-arm64-clang $OK_MAC "$MACBIN/xmesh_selftest" --corpus-check "$CORPUS"
selftest macos-arm64-clang-noexc $OK_MAC "$MACBIN/xmesh_selftest_noexc" --corpus-check "$CORPUS"
selftest ios-sim-arm64 $OK_IOS xcrun simctl spawn "$IOS_UDID" "$IOSBIN/xmesh_selftest" --corpus-check "$CORPUS"
selftest ios-sim-arm64-noexc $OK_IOS xcrun simctl spawn "$IOS_UDID" "$IOSBIN/xmesh_selftest_noexc" --corpus-check "$CORPUS"
selftest android-emu-arm64 $OK_AND "$ADB" shell "cd $DEV && ./bin/xmesh_selftest --corpus-check $DEV/corpus"
selftest android-emu-arm64-noexc $OK_AND "$ADB" shell "cd $DEV && ./bin/xmesh_selftest_noexc --corpus-check $DEV/corpus"
selftest colima-linux-arm64-gcc $OK_VM docker exec "$RT" "$VMBIN/xmesh_selftest" --corpus-check "$CORPUS"
selftest colima-linux-arm64-gcc-noexc $OK_VM docker exec "$RT" "$VMBIN/xmesh_selftest_noexc" --corpus-check "$CORPUS"
n_pass=$(grep -l '^RESULT PASS' "$XOQ_OUT"/mesh/selftest/*.txt 2>/dev/null | wc -l | tr -d ' ')
if [ "$n_pass" = 8 ]; then row "self-tests" "8/8 passed" "mesh/selftest/*.txt (types, hops, evil cases, the cloud's corpus)"
else row "self-tests" "**$n_pass/8 passed**" "mesh/selftest/*.txt"; fail; fi

# ---------------------------------------------------------------- 2. live mesh
LIVE=$W/live
live_run() {
    log "2. live mesh: xo_hub in colima + 8 devices"
    [ $OK_VM = 1 ] || { row "live mesh" "**not run**" "no hub (colima)"; fail; return; }
    local n=0 hex t pids="" name dir hub_pid poll_pid e p left
    for v in $OK_MAC $OK_MAC $OK_IOS $OK_IOS $OK_AND $OK_AND $OK_VM $OK_VM; do n=$((n + v)); done
    mkdir -p "$LIVE/devices"
    docker exec "$RT" "$VMBIN/xo_hub" --run "mac-$(date -u +%Y%m%d-%H%M)" --out "$LIVE/hub" --port $PORT --http-port $HTTP_PORT \
        --expect "$n" --wait-s 900 --job-timeout-s 600 --max-inflight 16 --advertise 127.0.0.1 --no-archive \
        > "$XOQ_OUT/mesh/hub.log" 2>&1 &
    hub_pid=$!
    BG_PIDS="$BG_PIDS $hub_pid"
    # the hub ends by itself (--wait-s, --job-timeout-s); this bounds it if it does not
    ( t=0; while kill -0 $hub_pid 2>/dev/null; do
          sleep 5; t=$((t + 5))
          [ $t -lt "${XO_HUB_LIMIT_S:-5400}" ] && continue
          echo "the hub still ran after $t s: stopped" >> "$XOQ_OUT/mesh/hub.log"
          # (no procps in the gcc image)
          docker exec "$RT" sh -c 'for p in /proc/[0-9]*; do [ "$(cat $p/comm 2>/dev/null)" = xo_hub ] && kill ${p#/proc/}; done'
          sleep 5; kill $hub_pid; break
      done ) > /dev/null 2>&1 &
    BG_PIDS="$BG_PIDS $!"
    # The published port accepts (docker-proxy) before the hub listens and then resets the
    # connection: wait until the hub listens inside the container.
    hex=$(printf '%04X' $PORT); t=0
    until docker exec "$RT" sh -c "grep -qE '^ *[0-9]+: [0-9A-F]+:$hex [0-9A-F]+:0000 0A' /proc/net/tcp /proc/net/tcp6 2>/dev/null"; do
        sleep 0.5; t=$((t + 1)); [ $t -lt 120 ] || { row "live mesh" "**failed**" "the hub did not listen within 60 s (mesh/hub.log)"; fail; return; }
    done
    ( while kill -0 $hub_pid 2>/dev/null; do
          curl -s -m 3 "http://127.0.0.1:$HTTP_PORT/status.json" -o "$LIVE/status.tmp" && mv "$LIVE/status.tmp" "$LIVE/status.json"
          curl -s -m 3 "http://127.0.0.1:$HTTP_PORT/" -o "$LIVE/dash.tmp" && mv "$LIVE/dash.tmp" "$LIVE/dashboard.html"
          sleep 5
      done ) > /dev/null 2>&1 &
    poll_pid=$!
    BG_PIDS="$BG_PIDS $poll_pid"
    # name ok command...: one device in the background, exit code in devices/NAME/exit
    start() {
        local name=$1 ok=$2; shift 2
        local dir=$LIVE/devices/$name
        mkdir -p "$dir"
        [ "$ok" = 1 ] || { echo 127 > "$dir/exit"; return; }
        ( "$@" > "$dir/stdout.txt" 2>&1; echo $? > "$dir/exit" ) < /dev/null &
        pids="$pids $!"
        BG_PIDS="$BG_PIDS $!"
    }
    local args="--evil --timeout-s 3600 --connect-s 120"
    # shellcheck disable=SC2086
    {
    start macos-arm64-clang $OK_MAC "$MACBIN/xo_peer" --hub 127.0.0.1:$PORT --name macos-arm64-clang --out "$LIVE/devices/macos-arm64-clang" $args
    start macos-arm64-clang-noexc $OK_MAC "$MACBIN/xo_peer_noexc" --hub 127.0.0.1:$PORT --name macos-arm64-clang-noexc --out "$LIVE/devices/macos-arm64-clang-noexc" $args
    start ios-sim-arm64 $OK_IOS ios_peer exc --hub 127.0.0.1:$PORT --name ios-sim-arm64 --out "$LIVE/devices/ios-sim-arm64/out" $args
    start ios-sim-arm64-noexc $OK_IOS ios_peer noexc --hub 127.0.0.1:$PORT --name ios-sim-arm64-noexc --out "$LIVE/devices/ios-sim-arm64-noexc" $args
    for name in android-emu-arm64 android-emu-arm64-noexc; do
        p=xo_peer; case $name in *-noexc) p=xo_peer_noexc ;; esac
        start $name $OK_AND "$ADB" shell "rm -rf $DEV/run/$name; mkdir -p $DEV/run/$name; cd $DEV && ./bin/$p --hub 10.0.2.2:$PORT --name $name --out $DEV/run/$name $args"
    done
    start colima-linux-arm64-gcc $OK_VM docker exec "$RT" "$VMBIN/xo_peer" --hub 127.0.0.1:$PORT --name colima-linux-arm64-gcc --out "$LIVE/devices/colima-linux-arm64-gcc" $args
    start colima-linux-arm64-gcc-noexc $OK_VM docker exec "$RT" "$VMBIN/xo_peer_noexc" --hub 127.0.0.1:$PORT --name colima-linux-arm64-gcc-noexc --out "$LIVE/devices/colima-linux-arm64-gcc-noexc" $args
    }
    wait $hub_pid; e=$?
    log "hub exit $e"
    # The devices leave when the hub ends: 120 s for all of them, then they are stopped. Only
    # these jobs are waited for; a bare `wait` also waits for the Android emulator (forever).
    t=0
    while [ $t -lt 120 ]; do
        left=0
        for p in $pids; do kill -0 "$p" 2>/dev/null && left=1; done
        [ $left = 1 ] || break
        sleep 1; t=$((t + 1))
    done
    for p in $pids; do kill -0 "$p" 2>/dev/null && { kill_tree "$p"; log "device job $p still ran 120 s after the hub: stopped"; }; done
    kill_tree $poll_pid
    # shellcheck disable=SC2086
    wait $pids $poll_pid 2> /dev/null
    BG_PIDS=""
    for name in android-emu-arm64 android-emu-arm64-noexc; do
        [ $OK_AND = 1 ] && "$ADB" exec-out "tar -cf - -C $DEV/run/$name ." 2>/dev/null | tar -xf - -C "$LIVE/devices/$name"
    done
    rm -rf "$XOQ_OUT/mesh/hub" "$XOQ_OUT/mesh/devices"
    cp -R "$LIVE/hub" "$XOQ_OUT/mesh/hub" 2>/dev/null
    cp -R "$LIVE/devices" "$XOQ_OUT/mesh/devices"
    cp "$LIVE/status.json" "$LIVE/dashboard.html" "$XOQ_OUT/mesh/" 2>/dev/null
    python3 "$SRC/demo/mesh/tools/mesh_report.py" "$XOQ_OUT/mesh" > "$XOQ_OUT/mesh/MESH_RESULT.md"; p=$?
    local line; line=$(sed -n 3p "$XOQ_OUT/mesh/MESH_RESULT.md" | sed 's/\*\*//g')
    if [ $e = 0 ] && [ $p = 0 ]; then row "live mesh" passed "$line (mesh/MESH_RESULT.md)"
    else row "live mesh" "**failed** (hub rc $e, report rc $p)" "$line (mesh/MESH_RESULT.md, mesh/hub.log)"; fail; fi
}

# ---------------------------------------------------------------- 3. mailbox
MBOX=$W/mbox
# mb_device NAME ROUND: NAME answers what it can in $MBOX/box (its files go into the box)
mb_device() {
    local name=$1 r=$2 id tmp f p
    id=$(id_of "$name")
    local lg=$XOQ_OUT/mailbox/logs/$name.log
    echo "-- round $r" >> "$lg"
    case $name in
        macos-arm64-clang) "$MACBIN/xo_peer" --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1 ;;
        macos-arm64-clang-noexc) "$MACBIN/xo_peer_noexc" --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1 ;;
        ios-sim-arm64)
            if [ "$XO_IOS_RUN" = host ]; then
                # the app gets a copy of the box and writes into a new directory, merged here
                tmp=$MBOX/tmp-$name-$r; rm -rf "$tmp"
                ios_peer exc --mailbox "$MBOX/box" "$tmp" --id "$id" --name "$name" >> "$lg" 2>&1 || return 1
                [ -d "$tmp" ] && { cp "$tmp"/* "$MBOX/box/" 2>/dev/null; rm -rf "$tmp"; }
            else
                ios_peer exc --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1
            fi ;;
        ios-sim-arm64-noexc) ios_peer noexc --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1 ;;
        android-emu-arm64*)
            p=xo_peer; case $name in *-noexc) p=xo_peer_noexc ;; esac
            # only the files it has not answered yet go to the emulator
            tmp=$MBOX/stage-$name; rm -rf "$tmp"; mkdir -p "$tmp/in" "$tmp/out"
            "$MACBIN/xo_peer" --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --list > "$tmp/list" 2>> "$lg" || return 1
            [ -s "$tmp/list" ] || return 0
            while read -r f; do cp "$MBOX/box/$f" "$tmp/in/"; done < "$tmp/list"
            "$ADB" shell "rm -rf $DEV/mbox; mkdir -p $DEV/mbox/in $DEV/mbox/out" > /dev/null || return 1
            "$ADB" push "$tmp/in/." "$DEV/mbox/in/" > /dev/null || return 1
            "$ADB" shell "cd $DEV && ./bin/$p --mailbox $DEV/mbox/in $DEV/mbox/out --id $id --name $name" >> "$lg" 2>&1 || return 1
            "$ADB" exec-out "tar -cf - -C $DEV/mbox/out ." | tar -xf - -C "$tmp/out" || return 1
            cp "$tmp/out/"* "$MBOX/box/" 2>/dev/null
            rm -rf "$tmp" ;;
        colima-linux-arm64-gcc) docker exec "$RT" "$VMBIN/xo_peer" --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1 ;;
        colima-linux-arm64-gcc-noexc) docker exec "$RT" "$VMBIN/xo_peer_noexc" --mailbox "$MBOX/box" "$MBOX/box" --id "$id" --name "$name" >> "$lg" 2>&1 ;;
    esac
}
mailbox_run() {
    log "3. mailbox: the Mac devices answer the cloud's files"
    [ $OK_MAC = 1 ] || { row mailbox "**not run**" "the macOS build failed (it lists the Android files)"; fail; return; }
    local devs="" r=0 before after name bad="" n_in n_out f b
    [ $OK_MAC = 1 ] && devs="$devs macos-arm64-clang macos-arm64-clang-noexc"
    [ $OK_IOS = 1 ] && devs="$devs ios-sim-arm64 ios-sim-arm64-noexc"
    [ $OK_AND = 1 ] && devs="$devs android-emu-arm64 android-emu-arm64-noexc"
    [ $OK_VM = 1 ] && devs="$devs colima-linux-arm64-gcc colima-linux-arm64-gcc-noexc"
    rm -rf "$MBOX"; mkdir -p "$MBOX"
    cp -R "$W/mac_inbox/inbox" "$MBOX/box"
    n_in=$(ls "$MBOX/box" | wc -l | tr -d ' ')
    local prior_note=""
    if [ -n "${XO_MAILBOX_PRIOR:-}" ]; then
        # an earlier run's answers stay answered (xo_peer --mailbox skips what has a report)
        mkdir -p "$MBOX/prior"
        tar -xzf "$XO_MAILBOX_PRIOR" -C "$MBOX/prior" 2>> "$XOQ_OUT/mailbox/logs/prior.log" && [ -d "$MBOX/prior/outbox" ] \
            || { row mailbox "**not run**" "XO_MAILBOX_PRIOR $XO_MAILBOX_PRIOR: no outbox/ in it"; fail; return; }
        cp "$MBOX/prior/outbox/"* "$MBOX/box/"
        sha "$XO_MAILBOX_PRIOR" > "$XOQ_OUT/mailbox/PRIOR_SHA256.txt"
        prior_note="; the $(ls "$MBOX/prior/outbox" | wc -l | tr -d ' ') files of the earlier outbox $(basename "$XO_MAILBOX_PRIOR") (sha256 $(cut -c1-12 "$XOQ_OUT/mailbox/PRIOR_SHA256.txt")) are not sent again"
    fi
    while :; do
        r=$((r + 1))
        before=$(ls "$MBOX/box" | wc -l)
        for name in $devs; do
            mb_device "$name" $r || { echo "round $r: failed" >> "$XOQ_OUT/mailbox/logs/$name.log"; case " $bad " in *" $name "*) ;; *) bad="$bad $name" ;; esac; }
        done
        after=$(ls "$MBOX/box" | wc -l)
        log "mailbox round $r: $((after - before)) new files"
        [ "$after" != "$before" ] || break
        [ $r -lt 80 ] || { bad="$bad (still writing after 80 rounds)"; break; }
    done
    mkdir -p "$MBOX/outbox"
    for f in "$MBOX/box"/*; do
        b=$(basename "$f")
        [ -e "$W/mac_inbox/inbox/$b" ] && continue
        [ -e "$MBOX/prior/outbox/$b" ] && cmp -s "$f" "$MBOX/prior/outbox/$b" && continue
        cp "$f" "$MBOX/outbox/"
    done
    n_out=$(ls "$MBOX/outbox" | wc -l | tr -d ' ')
    tar -czf "$XOQ_OUT/mailbox/outbox.tar.gz" -C "$MBOX" outbox
    cp "$W/mac_inbox/INBOX_INFO.txt" "$XOQ_OUT/mailbox/"
    { echo "inbox_files=$n_in"; echo "outbox_files=$n_out"; echo "rounds=$r"
      # shellcheck disable=SC2010
      echo "reports=$(ls "$MBOX/outbox" | grep -c '\.xmrep$')"; echo "no_meta_errors=$(ls "$MBOX/outbox" | grep -c '\.xmerr$')"
      # shellcheck disable=SC2010
      echo "hop_files_for_cloud=$(ls "$MBOX/outbox" | grep -c '_to[0-9]\{1,2\}\.xmhop$')"; } > "$XOQ_OUT/mailbox/OUTBOX_INFO.txt"
    local missing=""
    for name in $devs; do [ -f "$MBOX/box/build_$(id_of "$name").txt" ] || missing="$missing $name"; done
    if [ -z "$bad$missing" ]; then row "mailbox" "answered" "$n_in files in, $n_out out in $r rounds, $(grep reports= "$XOQ_OUT/mailbox/OUTBOX_INFO.txt")$prior_note; the cloud scores it (run_mailbox.sh post)"
    else row "mailbox" "**failed**" "devices with errors:$bad${missing:+; never ran:$missing} (mailbox/logs/)"; fail; fi
}

case "$WHAT" in
    all) live_run; mailbox_run ;;
    live) live_run ;;
    mailbox) mailbox_run ;;
esac
finish $RC
