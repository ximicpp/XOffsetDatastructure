#!/usr/bin/env bash
# One-command local validation of a PR in a Linux cloud container. GitHub CI is off
# for good (susu 2026-10-23), so this is the merge evidence: it validates the checked-out
# working tree (HEAD) against a base commit and writes REPORT.md for the PR description.
#
#   tools/local_validate.sh [--base REV] [--steps LIST] [--work DIR] [-j N] [--columns K] [--fail-fast]
#
# Steps, in this order (default: every step the host can run; LIST may also name the
# groups "classic" = the first nine and "matrix" = the C++20/C++23 columns):
#   gcc       GCC 16 Release: build + full CTest
#   asan      GCC 16 Debug, -fsanitize=address,undefined -fno-sanitize-recover=undefined
#             (no exclusions): build + full CTest
#   fuzz      test_v2_fuzz 200k cases x seeds 1, 2 (Release) and 20k x seed 7 (ASan+UBSan)
#   diff      tools/validation_differential.sh base vs head, 200000 cases, seeds 1 2
#   smoke     bench/ci_smoke.sh with g++ 16 (frozen benchmark trees, formal M wire)
#   ir        tools/perf_gate with GCC 16: instruction counts of anchor/base/head
#   p2996     P2996 Clang + libc++ Release via ./build.sh: build, CTest, signature
#             export/matrix/compat; tools/sigs must not change (the old P2996 CI job)
#   p2996-ir  tools/perf_gate with P2996 Clang + libc++ (the Mac's compiler and library)
#   asm       tools/perf_gate/asm_equal.py, GCC 16 and P2996: do base and head of the three
#             benchmark programs have the same instructions per function and the same .text
#             layout (MERGE_RULES_V2 class-0 evidence)? Reuses the Ir builds; a difference is
#             reported in REPORT.md and fails the step only with --asm-strict
# The C++20/C++23 build matrix (XOFFSET_CXX_STANDARD, XOFFSET_RECORD field lists), each
# column a Release build + full CTest unless named -asan:
#   gcc13-c++20          the host's GCC 13         gcc13-c++20-asan    same, Debug + ASan/UBSan
#   clang18-c++20        the host's Clang 18       clang18-c++20-asan  same, Debug + ASan/UBSan
#   gcc16-c++20          GCC 16 image, C++20       gcc16-c++23         GCC 16 image, C++23
#   gcc16-c++26d         GCC 16 image, C++26 with the field-list schema backend (M26D)
#   p2996-c++20          P2996 Clang + libc++, C++20 (no reflection)
#   diff-c++20           GCC 16: tools/validation_differential.sh, the C++26 reflection build
#                        against the C++20 field-list build of HEAD, 200000 cases, seeds 1 2;
#                        the wires must be the same bytes and the outcomes identical
#   wire-matrix          after the columns: tools/wire_matrix.sh over the C++26 GCC 16
#                        build (the reference, built if missing), the C++26 P2996 build if
#                        present and every non-ASan column: each writes the cross-compiler
#                        corpus, each loads every other's, all corpora the same bytes
# The matrix columns run in parallel after the other steps, at most K at a time (--columns,
# default min(4, columns)), each with -j max(1, N/K). A column whose compiler, sanitizer
# runtime or image is missing, or a tree without the C++20 build, is reported as "not run".
#
# The GCC steps run in the pinned gcc:16 image (the digest the CI used). The P2996 steps
# run on the host with the toolchain at $P2996_ROOT (default /opt/p2996-toolchain): if it
# is missing and $P2996_TARBALL exists, it is unpacked there first; without either they
# are reported as "not run", never silently dropped.
#
# Exit status: 0 when every step that ran passed, 1 otherwise. The Ir steps count as passed
# on "warn"; REPORT.md says whether the S-class condition (verdict "pass") holds.
set -uo pipefail

REPO=$(git rev-parse --show-toplevel) || exit 2
cd "$REPO"
BASE_REV=""
STEPS=""
WORK=""
JOBS=$(nproc)
FAIL_FAST=0
IMG=${XOFFSET_GCC_IMAGE:-mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240}
P2996_ROOT=${P2996_ROOT:-/opt/p2996-toolchain}
P2996_TARBALL=${P2996_TARBALL:-/mnt/project-files/toolchains/p2996-d77eff1-linux-x86_64.tar.xz}
CACHE=${XO_VALIDATE_CACHE:-$HOME/.cache/xo-validate}
SANITIZE="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
CLASSIC_STEPS="gcc asan fuzz diff smoke ir p2996 p2996-ir asm"
ASM_STRICT=0
MATRIX_STEPS="gcc13-c++20 gcc13-c++20-asan clang18-c++20 clang18-c++20-asan gcc16-c++20 gcc16-c++23 gcc16-c++26d p2996-c++20 diff-c++20 wire-matrix"
ALL_STEPS="$CLASSIC_STEPS $MATRIX_STEPS"
COLUMNS=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --base) BASE_REV=$2; shift 2 ;;
        --steps) STEPS=${2//,/ }; shift 2 ;;
        --work) WORK=$2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        --columns) COLUMNS=$2; shift 2 ;;
        --fail-fast) FAIL_FAST=1; shift ;;
        --asm-strict) ASM_STRICT=1; shift ;;
        -h|--help) sed -n '2,48p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

HEAD_SHA=$(git rev-parse HEAD)
if [[ -z "$BASE_REV" ]]; then
    git fetch -q origin next_cpp26 2>/dev/null || true
    BASE_REV=$(git merge-base HEAD origin/next_cpp26) || { echo "no merge-base with origin/next_cpp26; pass --base" >&2; exit 2; }
fi
BASE_SHA=$(git rev-parse "$BASE_REV^{commit}") || exit 2
WORK=${WORK:-$HOME/xo-validate-${HEAD_SHA:0:7}}
mkdir -p "$WORK/logs" "$CACHE"
WORK=$(cd "$WORK" && pwd)
STEPS=${STEPS:-$ALL_STEPS}
STEPS=" $STEPS "
STEPS=${STEPS// classic / $CLASSIC_STEPS }
STEPS=${STEPS// matrix / $MATRIX_STEPS }
DIRTY=$(git status --porcelain --untracked-files=no | wc -l)
RESULTS="$WORK/results.tsv"
: > "$RESULTS"

log() { printf '[local_validate %s] %s\n' "$(date -u +%H:%M:%S)" "$*" >&2; }

# ---------------------------------------------------------------- setup

docker_up() {
    docker info >/dev/null 2>&1 && return 0
    # A daemon left over from an earlier turn of the session is often dead.
    pkill -x dockerd 2>/dev/null; pkill -x containerd 2>/dev/null; sleep 1
    rm -f /var/run/docker.pid /var/run/docker.sock
    (nohup dockerd >/tmp/dockerd.log 2>&1 &)
    for _ in $(seq 60); do docker info >/dev/null 2>&1 && return 0; sleep 1; done
    echo "dockerd did not start; see /tmp/dockerd.log" >&2
    return 1
}

need_gcc=0; need_p2996=0; want_gcc=0
for s in $STEPS; do
    case " $ALL_STEPS " in *" $s "*) ;; *) echo "unknown step: $s" >&2; exit 2 ;; esac
    # The GCC 16 matrix columns report "not run" without docker; the classic steps need it.
    case $s in p2996*) need_p2996=1 ;; gcc13-*|clang18-*) ;; gcc16-c++*|diff-c++20|wire-matrix) want_gcc=1 ;; asm) need_gcc=1; need_p2996=1 ;; *) need_gcc=1 ;; esac
done

log "head $HEAD_SHA (uncommitted tracked changes: $DIRTY), base $BASE_SHA, work $WORK"
git submodule update --init --depth 1 external/typelayout external/boost >/dev/null 2>&1
for lib in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef move assert; do
    [[ -e external/boost/libs/$lib/include ]] || git -C external/boost submodule update --init --depth 1 "libs/$lib" >/dev/null 2>&1
done
[[ -x build/cmake-tool/root/bin/cmake ]] || python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool >/dev/null
CMAKE_BIN=$REPO/build/cmake-tool/root/bin
[[ -d $CACHE/cereal/include ]] || { git clone -q https://github.com/USCiLab/cereal.git "$CACHE/cereal" && git -C "$CACHE/cereal" checkout -q ebef1e929807629befafbb2918ea1a08c7194554; }
[[ -f $CACHE/vginc/valgrind/callgrind.h ]] || { mkdir -p "$CACHE/vginc" && cp -r /usr/include/valgrind "$CACHE/vginc/"; }

GCC_OK=0
if (( need_gcc )); then
    docker_up || exit 2
    docker image inspect "$IMG" >/dev/null 2>&1 || docker pull -q "$IMG" >/dev/null || exit 2
    GCC_OK=1
elif (( want_gcc )); then
    docker_up && { docker image inspect "$IMG" >/dev/null 2>&1 || docker pull -q "$IMG" >/dev/null; } && GCC_OK=1
fi
P2996_OK=0
if (( need_p2996 )); then
    if [[ ! -x $P2996_ROOT/bin/clang++ && -f $P2996_TARBALL ]]; then
        log "unpacking $P2996_TARBALL into $(dirname "$P2996_ROOT")"
        mkdir -p "$(dirname "$P2996_ROOT")" && tar -xJf "$P2996_TARBALL" -C "$(dirname "$P2996_ROOT")"
    fi
    [[ -x $P2996_ROOT/bin/clang++ ]] && P2996_OK=1
fi

# Runs a command in the GCC 16 image with the repository, the work and the cache
# directories at their host paths.
in_gcc() {
    docker run --rm -v "$REPO:$REPO" -v "$WORK:$WORK" -v "$CACHE:$CACHE" -w "$REPO" \
        -e PATH="$CMAKE_BIN:/usr/local/bin:/usr/bin:/bin" \
        -e UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
        -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        "$IMG" bash -c "$1"
}

# ---------------------------------------------------------------- steps

# Each step writes logs/<step>.log and echoes a one-line detail on its last stdout line.
ctest_line() { grep -E 'tests passed|tests failed' "$1" | tail -n 1; }

step_gcc() {
    in_gcc "g++ --version | head -n 1 &&
        cmake -S . -B '$WORK/gcc' -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ &&
        cmake --build '$WORK/gcc' --parallel $JOBS &&
        ctest --test-dir '$WORK/gcc' --output-on-failure --parallel $JOBS --timeout 900"
}

step_asan() {
    in_gcc "cmake -S . -B '$WORK/asan' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ \
            '-DCMAKE_CXX_FLAGS=$SANITIZE' -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined &&
        cmake --build '$WORK/asan' --parallel $JOBS &&
        ctest --test-dir '$WORK/asan' --output-on-failure --parallel $JOBS --timeout 1800"
}

step_fuzz() {
    local rel asan
    rel=$(find "$WORK/gcc" -name test_v2_fuzz -type f -perm -u+x 2>/dev/null | head -n 1)
    asan=$(find "$WORK/asan" -name test_v2_fuzz -type f -perm -u+x 2>/dev/null | head -n 1)
    if [[ -z $rel ]]; then
        in_gcc "cmake -S . -B '$WORK/gcc' -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ &&
            cmake --build '$WORK/gcc' --target test_v2_fuzz --parallel $JOBS" || return 1
        rel=$(find "$WORK/gcc" -name test_v2_fuzz -type f -perm -u+x | head -n 1)
    fi
    if [[ -z $asan ]]; then
        in_gcc "cmake -S . -B '$WORK/asan' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ \
                '-DCMAKE_CXX_FLAGS=$SANITIZE' -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined &&
            cmake --build '$WORK/asan' --target test_v2_fuzz --parallel $JOBS" || return 1
        asan=$(find "$WORK/asan" -name test_v2_fuzz -type f -perm -u+x | head -n 1)
    fi
    # The three runs in parallel; each one's exit status counts.
    in_gcc "'$rel' 200000 1 > '$WORK/logs/fuzz-1.txt' 2>&1 & a=\$!
        '$rel' 200000 2 > '$WORK/logs/fuzz-2.txt' 2>&1 & b=\$!
        '$asan' 20000 7 > '$WORK/logs/fuzz-7-asan.txt' 2>&1 & c=\$!
        rc=0; wait \$a || rc=1; wait \$b || rc=1; wait \$c || rc=1
        tail -n 5 '$WORK/logs/fuzz-1.txt' '$WORK/logs/fuzz-2.txt' '$WORK/logs/fuzz-7-asan.txt'; exit \$rc"
}

step_diff() {
    rm -rf "$WORK/base"
    mkdir -p "$WORK/base"
    git archive "$BASE_SHA" | tar -x -C "$WORK/base"
    in_gcc "XOFFSET_DIFF_OUT='$WORK/diff' tools/validation_differential.sh '$WORK/base' '$REPO' 200000 1 2"
}

step_smoke() {
    rm -rf "$WORK/smoke"
    in_gcc "JOBS=$JOBS bench/ci_smoke.sh g++ '$CACHE/cereal/include' '$CACHE/vginc' '$WORK/smoke'"
}

# perf_gate build for one compiler: prepare (git) and build base and head into $WORK/ir-<compiler>.
perf_gate_build() {
    local compiler=$1 w="$WORK/ir-$1" P="$REPO/tools/perf_gate/perf_gate.py" cxx extra
    rm -rf "$w"
    python3 "$P" prepare --work "$w" --repo "$REPO" --base "$BASE_SHA" --head "$HEAD_SHA" || return 1
    if [[ $compiler == gcc16 ]]; then
        cxx="$WORK/gxx.sh"
        cat > "$cxx" <<EOF
#!/bin/sh
exec docker run --rm -i -v "$REPO:$REPO" -v "$WORK:$WORK" -v "$CACHE:$CACHE" -w "\$PWD" "$IMG" g++ "\$@"
EOF
        chmod +x "$cxx"
        extra="-static -isystem $CACHE/vginc"
    else
        cxx="$P2996_ROOT/bin/clang++"
        extra="-isystem $CACHE/vginc"
    fi
    (cd "$w" && P2996_ROOT="$P2996_ROOT" python3 "$P" build --work "$w" --compiler "$compiler" --cxx "$cxx" \
        --cereal "$CACHE/cereal/include" --extra-flags "$extra" --jobs "$JOBS")
}

# perf_gate for one compiler: build, measure (valgrind on the host), compare.
perf_gate() {
    local compiler=$1 w="$WORK/ir-$1" P="$REPO/tools/perf_gate/perf_gate.py"
    perf_gate_build "$compiler" || return 1
    python3 "$P" measure --work "$w" --compiler "$compiler" --jobs "$JOBS" || return 1
    python3 "$P" compare --work "$w" --compiler "$compiler" --summary "$w/summary.md" --json "$w/gate.json"
    local rc=$?
    cat "$w/summary.md"
    return $rc
}

step_ir() { perf_gate gcc16; }

step_p2996() {
    git diff --quiet -- tools/sigs || { echo "tools/sigs has uncommitted changes before the run"; return 1; }
    PATH="$CMAKE_BIN:$P2996_ROOT/bin:$PATH" CTEST_PARALLEL_LEVEL=$JOBS bash ./build.sh --compiler "$P2996_ROOT/bin/clang++" \
        --build-dir "$WORK/p2996" -j "$JOBS" || return 1
    if ! git diff --quiet -- tools/sigs || [[ -n $(git status --porcelain --untracked-files=all -- tools/sigs) ]]; then
        git status --short --untracked-files=all -- tools/sigs
        echo "type signatures changed: review tools/sigs and commit intentional baseline updates"
        return 1
    fi
}

step_p2996_ir() { perf_gate p2996; }

# Class-0 evidence per compiler, on the Ir builds when those steps ran (else builds them).
step_asm() {
    local c rc=0 r
    for c in gcc16 p2996; do
        [[ $c == p2996 ]] && (( ! P2996_OK )) && { echo "asm $c: not run (no P2996 toolchain)"; rc=1; continue; }
        [[ -f $WORK/ir-$c/build-$c.json ]] || perf_gate_build "$c" || { echo "asm $c: build failed"; rc=1; continue; }
        python3 "$REPO/tools/perf_gate/asm_equal.py" gate --work "$WORK/ir-$c" --compiler "$c" \
            --summary "$WORK/asm-$c.md" --json "$WORK/asm-$c.json"
        r=$?
        (( r == 2 || (r == 1 && ASM_STRICT) )) && rc=1
    done
    return $rc
}

# ---------------------------------------------------------------- C++20/C++23 matrix

# Why a matrix column cannot run here, or nothing when it can.
matrix_blocker() {
    local s=$1
    if ! grep -q XOFFSET_CXX_STANDARD CMakeLists.txt || ! grep -qs '#define XOFFSET_RECORD(' ./*.hpp; then
        echo "this tree has no C++20 build (XOFFSET_CXX_STANDARD / XOFFSET_RECORD missing)"; return
    fi
    case $s in
        gcc13-*|clang18-*)
            local cxx=g++-13; [[ $s == clang18-* ]] && cxx=clang++-18
            command -v $cxx >/dev/null || { echo "no $cxx on the host"; return; }
            if [[ $s == *-asan ]]; then
                printf 'int main() { return 0; }\n' > "$WORK/san_probe.cpp"
                $cxx -fsanitize=address,undefined "$WORK/san_probe.cpp" -o "$WORK/san_probe" 2>/dev/null ||
                    echo "$cxx has no ASan/UBSan runtime (for clang 18: apt-get install libclang-rt-18-dev)"
            fi ;;
        p2996-*) (( P2996_OK )) || echo "no P2996 toolchain at $P2996_ROOT (and no $P2996_TARBALL)" ;;
        *) (( GCC_OK )) || echo "no GCC 16 image (docker or $IMG unavailable)" ;;
    esac
}

# Configure, build and CTest one column on the host: <dir> <cxx> <std> <type> <cxxflags> <ldflags>.
host_column() {
    local dir=$1 cxx=$2 std=$3 type=$4 flags=$5 ldflags=$6 timeout=900
    [[ $type == Debug ]] && timeout=1800
    $cxx --version | head -n 1
    PATH="$CMAKE_BIN:$PATH" cmake -S . -B "$dir" -DCMAKE_BUILD_TYPE=$type -DCMAKE_CXX_COMPILER=$cxx \
        -DXOFFSET_CXX_STANDARD=$std "-DCMAKE_CXX_FLAGS=$flags" "-DCMAKE_EXE_LINKER_FLAGS=$ldflags" &&
    PATH="$CMAKE_BIN:$PATH" cmake --build "$dir" --parallel "$COL_JOBS" &&
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        PATH="$CMAKE_BIN:$PATH" ctest --test-dir "$dir" --output-on-failure --parallel "$COL_JOBS" --timeout $timeout
}

gcc16_column() {
    local dir=$1 std=$2 extra=${3:-}
    in_gcc "g++ --version | head -n 1 &&
        cmake -S . -B '$dir' -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DXOFFSET_CXX_STANDARD=$std $extra &&
        cmake --build '$dir' --parallel $COL_JOBS &&
        ctest --test-dir '$dir' --output-on-failure --parallel $COL_JOBS --timeout 900"
}

step_matrix() {
    local s=$1 dir="$WORK/m-$1" lib="$P2996_ROOT/lib/x86_64-unknown-linux-gnu"
    case $s in
        gcc13-c++20) host_column "$dir" g++-13 20 Release "" "" ;;
        gcc13-c++20-asan) host_column "$dir" g++-13 20 Debug "$SANITIZE" "-fsanitize=address,undefined" ;;
        clang18-c++20) host_column "$dir" clang++-18 20 Release "" "" ;;
        clang18-c++20-asan) host_column "$dir" clang++-18 20 Debug "$SANITIZE" "-fsanitize=address,undefined" ;;
        gcc16-c++20) gcc16_column "$dir" 20 ;;
        gcc16-c++23) gcc16_column "$dir" 23 ;;
        gcc16-c++26d) gcc16_column "$dir" 26 -DXOFFSET_SCHEMA_BACKEND=describe ;;
        p2996-c++20) host_column "$dir" "$P2996_ROOT/bin/clang++" 20 Release "-stdlib=libc++" \
                         "-stdlib=libc++ -Wl,-rpath,$lib" ;;
        diff-c++20)
            in_gcc "XOFFSET_DIFF_OUT='$dir' XOFFSET_DIFF_BASE_FLAGS='-std=c++26 -freflection' \
                XOFFSET_DIFF_FLAGS='-std=c++20' XOFFSET_DIFF_SAME_INPUTS=1 \
                tools/validation_differential.sh '$REPO' '$REPO' 200000 1 2" ;;
    esac
}

# The cross-compiler corpus of every build that has one, read by every other.
step_wire_matrix() {
    local dirs=() names=() d s bin args=() run
    if [[ ! -d $WORK/gcc ]] || [[ -z $(find "$WORK/gcc" -name test_cross_compiler_wire -type f -perm -u+x 2>/dev/null) ]]; then
        in_gcc "cmake -S . -B '$WORK/gcc' -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ &&
            cmake --build '$WORK/gcc' --target test_cross_compiler_wire --parallel $JOBS" || return 1
    fi
    names+=(gcc16-c++26); dirs+=("$WORK/gcc")
    [[ -d $WORK/p2996 ]] && { names+=(p2996-c++26); dirs+=("$WORK/p2996"); }
    for s in $MATRIX_STEPS; do
        case $s in *-asan|diff-*|wire-matrix) continue ;; esac
        [[ -d $WORK/m-$s ]] && { names+=("$s"); dirs+=("$WORK/m-$s"); }
    done
    for i in "${!names[@]}"; do
        bin=$(find "${dirs[i]}" -name test_cross_compiler_wire -type f -perm -u+x 2>/dev/null | head -n 1)
        [[ -n $bin ]] || { echo "${names[i]}: no test_cross_compiler_wire, left out"; continue; }
        run=$bin
        [[ ${names[i]} == gcc16-* ]] && run="docker run --rm -v '$WORK:$WORK' '$IMG' $bin"
        args+=("${names[i]}=$run")
    done
    (( ${#args[@]} >= 2 )) || { echo "fewer than two builds with a cross-compiler corpus"; return 1; }
    tools/wire_matrix.sh "$WORK/wire-matrix" "${args[@]}"
}

detail_of() {
    local s=$1 f="$WORK/logs/$1.log" w
    case $s in
        gcc|asan) ctest_line "$f" ;;
        diff-c++20) grep -E '^seed [0-9]+:' "$f" | sed 's/^/C++26 vs C++20 /' | paste -sd ';' ;;
        wire-matrix) grep -E '^wire matrix:' "$f" | tail -n 1 ;;
        gcc13-*|clang18-*|gcc16-c++*|p2996-c++*)
            { if grep -qE "tests passed|tests failed" "$f"; then ctest_line "$f"
              elif grep -q '^CMake Error' "$f"; then grep -m1 -A1 -E '^CMake Error' "$f" | tail -n 1 | sed 's/^ */configure: /'
              else echo "build failed: $(grep -oE 'CMakeFiles/[A-Za-z0-9_]+\.dir/all' "$f" | sed -E 's|CMakeFiles/||; s|\.dir/all||' | sort -u | paste -sd ' ')"; fi
              [[ -f $WORK/m-$s/test-inventory.json ]] && python3 -c 'import json, sys
n = len(json.load(open(sys.argv[1])).get("cxx26_only_not_built", []))
print(f"{n} C++26-only tests/tools not built")' "$WORK/m-$s/test-inventory.json"; } | paste -sd ';' ;;
        p2996) { ctest_line "$f"; grep -m1 -E 'type signatures changed|completed successfully' "$f"; } | paste -sd ';' ;;
        fuzz) grep -E '^test_v2_fuzz: seed=' "$f" | sed -E 's/.*seed=([0-9]+).* cases=([0-9]+).*inconsistencies=([0-9]+).*/seed \1: \2 cases, \3 inconsistencies/' | paste -sd ';' ;;
        diff) grep -E '^seed [0-9]+:' "$f" | paste -sd ';' ;;
        smoke) grep -E 'bench smoke passed|input wire' "$f" | tail -n 1 ;;
        asm) for c in gcc16 p2996; do [[ -f $WORK/asm-$c.json ]] && python3 -c 'import json, sys
print(sys.argv[2] + ": " + json.load(open(sys.argv[1]))["verdict"])' "$WORK/asm-$c.json" "$c"; done | paste -sd ';' ;;
        ir|p2996-ir)
            w="$WORK/ir-${s%-ir}"; [[ $s == ir ]] && w="$WORK/ir-gcc16"
            [[ -f $w/gate.json ]] && python3 -c 'import json, sys
v = json.load(open(sys.argv[1]))["verdict"]
print(f"verdict {v}; S-class Ir condition: " + ("yes" if v == "pass" else "no"))' "$w/gate.json" ;;
    esac
}

overall=0
record() {   # <step> <rc> <minutes>
    local s=$1 rc=$2 mins=$3 res detail
    if (( rc == 0 )); then res=pass; else res=FAIL; overall=1; fi
    # build.sh colours its last line; keep escape codes out of the table.
    detail=$(detail_of "$s" 2>/dev/null | sed -E $'s/\x1b\\[[0-9;]*m//g' | tr '\t\n' '  ')
    printf '%s\t%s\t%s\t%s\n' "$s" "$res" "$mins" "$detail" >> "$RESULTS"
    log "$s: $res in ${mins} min${detail:+ ($detail)}"
}
stopped=0
for s in $CLASSIC_STEPS; do
    case "$STEPS" in *" $s "*) ;; *) continue ;; esac
    if [[ $s == p2996* ]] && (( ! P2996_OK )); then
        printf '%s\tnot run\t0\tno P2996 toolchain at %s (and no %s)\n' "$s" "$P2996_ROOT" "$P2996_TARBALL" >> "$RESULTS"
        log "$s: not run (no toolchain)"
        continue
    fi
    log "$s: start"
    t0=$(date +%s)
    "step_${s//-/_}" > "$WORK/logs/$s.log" 2>&1
    rc=$?
    record "$s" $rc $(( ($(date +%s) - t0 + 30) / 60 ))
    if (( rc != 0 && FAIL_FAST )); then stopped=1; break; fi
done

# The matrix columns, in parallel: at most COLUMNS at a time, results in canonical order.
cols=()
for s in $MATRIX_STEPS; do
    case "$STEPS" in *" $s "*) ;; *) continue ;; esac
    (( stopped )) && break
    [[ $s == wire-matrix ]] && continue
    why=$(matrix_blocker "$s")
    if [[ -n $why ]]; then
        printf '%s\tnot run\t0\t%s\n' "$s" "$why" > "$WORK/col-$s.tsv"
        log "$s: not run ($why)"
    else
        cols+=("$s")
    fi
done
if (( ${#cols[@]} )); then
    (( COLUMNS > 0 )) || COLUMNS=$(( ${#cols[@]} < 4 ? ${#cols[@]} : 4 ))
    COL_JOBS=$(( JOBS / COLUMNS )); (( COL_JOBS > 0 )) || COL_JOBS=1
    log "matrix: ${#cols[@]} columns, $COLUMNS at a time, -j $COL_JOBS each"
    running=0
    for s in "${cols[@]}"; do
        if (( running >= COLUMNS )); then wait -n; running=$((running - 1)); fi
        log "$s: start"
        ( t0=$(date +%s); step_matrix "$s" > "$WORK/logs/$s.log" 2>&1; rc=$?
          echo "$rc $(( ($(date +%s) - t0 + 30) / 60 ))" > "$WORK/col-$s.rc" ) &
        running=$((running + 1))
    done
    wait
fi
case "$STEPS" in
    *" wire-matrix "*)
        why=$(matrix_blocker wire-matrix)
        if (( stopped )); then :
        elif [[ -n $why ]]; then
            printf '%s\tnot run\t0\t%s\n' wire-matrix "$why" > "$WORK/col-wire-matrix.tsv"
            log "wire-matrix: not run ($why)"
        else
            log "wire-matrix: start"
            t0=$(date +%s); step_wire_matrix > "$WORK/logs/wire-matrix.log" 2>&1; rc=$?
            echo "$rc $(( ($(date +%s) - t0 + 30) / 60 ))" > "$WORK/col-wire-matrix.rc"
        fi ;;
esac
for s in $MATRIX_STEPS; do
    if [[ -f $WORK/col-$s.tsv ]]; then
        cat "$WORK/col-$s.tsv" >> "$RESULTS"; rm -f "$WORK/col-$s.tsv"
    elif [[ -f $WORK/col-$s.rc ]]; then
        read -r rc mins < "$WORK/col-$s.rc"; rm -f "$WORK/col-$s.rc"
        record "$s" "$rc" "$mins"
    fi
done

{
    echo "### Local validation (tools/local_validate.sh)"
    echo
    echo "head \`${HEAD_SHA:0:12}\`$( (( DIRTY )) && echo " + $DIRTY uncommitted tracked files"), base \`${BASE_SHA:0:12}\`, $(nproc) cores, GCC image \`${IMG##*@}\`"
    (( P2996_OK )) && echo "P2996: \`$("$P2996_ROOT/bin/clang++" --version | head -n 1)\`"
    echo
    echo "| Step | Result | Minutes | Detail |"
    echo "| --- | --- | --- | --- |"
    while IFS=$'\t' read -r s r m d; do echo "| $s | $r | $m | $d |"; done < "$RESULTS"
    for c in gcc16 p2996; do
        [[ -f $WORK/asm-$c.md ]] || continue
        echo
        echo "<details><summary>Machine code, $c</summary>"
        echo
        cat "$WORK/asm-$c.md"
        echo
        echo "</details>"
    done
    for c in gcc16 p2996; do
        [[ -f $WORK/ir-$c/summary.md ]] || continue
        echo
        echo "<details><summary>Ir gate, $c</summary>"
        echo
        cat "$WORK/ir-$c/summary.md"
        echo
        echo "</details>"
    done
} > "$WORK/REPORT.md"
cat "$WORK/REPORT.md"
log "report: $WORK/REPORT.md; logs: $WORK/logs/"
exit $overall
