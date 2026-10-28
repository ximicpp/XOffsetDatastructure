#!/usr/bin/env bash
# SC-14: binary size XOffset adds to a client, per extra schema, and single-TU compile
# time, for one client target. Release -Os (MinSizeRel), unused sections dropped,
# stripped like a shipped app.
#
#   tools/client_size/measure.sh TARGET OUT_DIR [--std 20|23] [-- cmake args]
#
# TARGET: android-arm64 (ANDROID_NDK set), ios-device, ios-sim, macos, host.
# Writes OUT_DIR/CLIENT_SIZE_<target>_c++<std>.md and .tsv. macOS bash 3.2 compatible.
set -uo pipefail
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TARGET=${1:?target}; OUT=${2:?out dir}; shift 2
STD=20
EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --std) STD=$2; shift 2 ;;
        --) shift; EXTRA+=("$@"); break ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
TC=$REPO/cmake/toolchains
ARGS=(-G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DXOFFSET_CXX_STANDARD="$STD" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
case "$TARGET" in
    android-arm64) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/android-arm64.cmake") ;;
    ios-device) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/ios-device.cmake") ;;
    ios-sim) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/ios-simulator.cmake") ;;
    macos|host) ;;
    *) echo "unknown target $TARGET" >&2; exit 2 ;;
esac
NAME=CLIENT_SIZE_${TARGET}_c++$STD
B=$OUT/build-$TARGET-c++$STD
LOG=$OUT/$NAME.log
: > "$LOG"
build() {  # build DIR ARGS...: configure + build, logs appended
    local dir=$1; shift
    cmake -S "$REPO/tools/client_size" -B "$dir" "${ARGS[@]}" "$@" ${EXTRA[@]+"${EXTRA[@]}"} >> "$LOG" 2>&1 &&
        cmake --build "$dir" >> "$LOG" 2>&1
}
build "$B" || { echo "build failed (see $LOG)"; exit 1; }
build "$B-nozstd" -DMAPDEMO_WITH_ZSTD=OFF || { echo "build without zstd failed (see $LOG)"; exit 1; }

# the toolchain's strip (NDK llvm-strip, Xcode strip)
STRIP=$(sed -n 's/^CMAKE_STRIP:FILEPATH=//p' "$B/CMakeCache.txt")
[ -n "$STRIP" ] || STRIP=strip
size_of() {  # stripped size in bytes of one program
    local f=$1 s=$OUT/stripped.tmp
    cp "$f" "$s"
    case "$TARGET" in
        ios-*|macos) "$STRIP" -x "$s" 2>> "$LOG" ;;   # Mach-O: keep only what dyld needs
        *) "$STRIP" --strip-all "$s" 2>> "$LOG" ;;
    esac
    wc -c < "$s" | tr -d ' '
    rm -f "$s"
}
bin() { find "$1" -type f -name "$2" -perm -u+x | head -1; }

TSV=$OUT/$NAME.tsv
printf 'program\tschemas\texceptions\tstripped_bytes\tunstripped_bytes\n' > "$TSV"
for v in exc noexc; do
    for n in 0 1 2 4 8; do
        f=$(bin "$B" "size_s${n}_$v")
        printf 'size_client\t%s\t%s\t%s\t%s\n' "$n" "$v" "$(size_of "$f")" "$(wc -c < "$f" | tr -d ' ')" >> "$TSV"
    done
done
for d in "$B" "$B-nozstd"; do
    z=zstd; [ "$d" = "$B" ] || z=nozstd
    for p in mapdemo_client mapdemo_client_noexc; do
        f=$(bin "$d" "$p")
        [ -n "$f" ] && printf '%s(%s)\t-\t%s\t%s\t%s\n' "$p" "$z" "$([ $p = mapdemo_client ] && echo exc || echo noexc)" \
            "$(size_of "$f")" "$(wc -c < "$f" | tr -d ' ')" >> "$TSV"
    done
done

# Single-TU compile time: the TU's own command from compile_commands.json, 3 runs, median.
ctime() {  # ctime SOURCE_SUFFIX TARGET_DIR_MATCH -> seconds (median of 3)
    python3 - "$B/compile_commands.json" "$1" "$2" <<'PY'
import json, shlex, subprocess, sys, time, os
cc, suffix, match = sys.argv[1], sys.argv[2], sys.argv[3]
for e in json.load(open(cc)):
    if e["file"].endswith(suffix) and match in e.get("output", e.get("command", "")):
        cmd = shlex.split(e["command"]) if "command" in e else e["arguments"]
        i = cmd.index("-o"); cmd[i + 1] = os.devnull
        ts = []
        for _ in range(3):
            t = time.perf_counter(); subprocess.run(cmd, cwd=e["directory"], check=True, capture_output=True); ts.append(time.perf_counter() - t)
        print("%.2f" % sorted(ts)[1]); break
else:
    print("n/a")
PY
}
T1=$(ctime size_client.cpp size_s1_noexc)
T8=$(ctime size_client.cpp size_s8_noexc)
T0=$(ctime size_client.cpp size_s0_noexc)
TD=$(ctime client_core.cpp mapdemo_client_noexc)
CXX=$(sed -n 's/^set(CMAKE_CXX_COMPILER "\(.*\)")$/\1/p' "$B"/CMakeFiles/*/CMakeCXXCompiler.cmake 2>/dev/null | head -1)
REV=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo exported)
git -C "$REPO" diff --quiet 2>/dev/null || REV="$REV (dirty)"
python3 - "$TSV" "$OUT/$NAME.md" "$TARGET" "$STD" "$("$CXX" --version 2>/dev/null | head -1)" "$T0" "$T1" "$T8" "$TD" \
    "$REV" <<'PY'
import csv, sys
tsv, md, target, std, cxx, t0, t1, t8, td, rev = sys.argv[1:]
rows = list(csv.DictReader(open(tsv), delimiter="\t"))
def get(prog, n, v):
    for r in rows:
        if r["program"] == prog and r["schemas"] == n and r["exceptions"] == v:
            return int(r["stripped_bytes"])
out = ["# Client size, %s, C++%s" % (target, std), "",
       "- compiler: %s; commit %s; MinSizeRel (-Os), -ffunction-sections -fdata-sections, unused sections dropped, stripped" % (cxx, rev),
       "- size_client: receive (try_load_verified), read, change one field, save_compact; N root schemas; baseline N=0 has no XOffset", ""]
out += ["| exceptions | baseline | 1 schema | +lib (1 schema) | 2 | 4 | 8 | per extra schema |", "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
for v in ("exc", "noexc"):
    s = {n: get("size_client", n, v) for n in ("0", "1", "2", "4", "8")}
    if None in s.values():
        continue
    per = (s["8"] - s["1"]) / 7.0
    out.append("| %s | %d | %d | **%+d** | %d | %d | %d | **%+.0f** |" % (
        "on" if v == "exc" else "off", s["0"], s["1"], s["1"] - s["0"], s["2"], s["4"], s["8"], per))
out += ["", "| demo client | stripped bytes |", "| --- | ---: |"]
for r in rows:
    if r["program"].startswith("mapdemo"):
        out.append("| %s, exceptions %s | %s |" % (r["program"], "on" if r["exceptions"] == "exc" else "off", r["stripped_bytes"]))
out += ["", "Single-TU compile time (median of 3, -Os, this machine): size_client without XOffset %s s, "
        "1 schema %s s, 8 schemas %s s; demo client_core.cpp %s s." % (t0, t1, t8, td), ""]
open(md, "w").write("\n".join(out))
print("\n".join(out))
PY
