#!/usr/bin/env bash
# One-command local validation of a PR in a Linux cloud container. GitHub CI is off
# for good (susu 2026-10-23), so this is the merge evidence: it validates the checked-out
# working tree (HEAD) against a base commit and writes REPORT.md for the PR description.
#
#   tools/local_validate.sh [--base REV] [--steps LIST] [--work DIR] [-j N] [--fail-fast]
#
# Steps, in this order (default: every step the host can run):
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
ALL_STEPS="gcc asan fuzz diff smoke ir p2996 p2996-ir"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --base) BASE_REV=$2; shift 2 ;;
        --steps) STEPS=${2//,/ }; shift 2 ;;
        --work) WORK=$2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        --fail-fast) FAIL_FAST=1; shift ;;
        -h|--help) sed -n '2,29p' "$0"; exit 0 ;;
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

need_gcc=0; need_p2996=0
for s in $STEPS; do
    case " $ALL_STEPS " in *" $s "*) ;; *) echo "unknown step: $s" >&2; exit 2 ;; esac
    case $s in p2996*) need_p2996=1 ;; *) need_gcc=1 ;; esac
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

if (( need_gcc )); then
    docker_up || exit 2
    docker image inspect "$IMG" >/dev/null 2>&1 || docker pull -q "$IMG" >/dev/null || exit 2
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

# perf_gate for one compiler: prepare (git), build, measure (valgrind on the host), compare.
perf_gate() {
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
        --cereal "$CACHE/cereal/include" --extra-flags "$extra" --jobs "$JOBS") || return 1
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

detail_of() {
    local s=$1 f="$WORK/logs/$1.log" w
    case $s in
        gcc|asan) ctest_line "$f" ;;
        p2996) { ctest_line "$f"; grep -m1 -E 'type signatures changed|completed successfully' "$f"; } | paste -sd ';' ;;
        fuzz) grep -E '^test_v2_fuzz: seed=' "$f" | sed -E 's/.*seed=([0-9]+).* cases=([0-9]+).*inconsistencies=([0-9]+).*/seed \1: \2 cases, \3 inconsistencies/' | paste -sd ';' ;;
        diff) grep -E '^seed [0-9]+:' "$f" | paste -sd ';' ;;
        smoke) grep -E 'bench smoke passed|input wire' "$f" | tail -n 1 ;;
        ir|p2996-ir)
            w="$WORK/ir-${s%-ir}"; [[ $s == ir ]] && w="$WORK/ir-gcc16"
            [[ -f $w/gate.json ]] && python3 -c 'import json, sys
v = json.load(open(sys.argv[1]))["verdict"]
print(f"verdict {v}; S-class Ir condition: " + ("yes" if v == "pass" else "no"))' "$w/gate.json" ;;
    esac
}

overall=0
for s in $ALL_STEPS; do
    case " $STEPS " in *" $s "*) ;; *) continue ;; esac
    if [[ $s == p2996* ]] && (( ! P2996_OK )); then
        printf '%s\tnot run\t0\tno P2996 toolchain at %s (and no %s)\n' "$s" "$P2996_ROOT" "$P2996_TARBALL" >> "$RESULTS"
        log "$s: not run (no toolchain)"
        continue
    fi
    log "$s: start"
    t0=$(date +%s)
    "step_${s//-/_}" > "$WORK/logs/$s.log" 2>&1
    rc=$?
    mins=$(( ($(date +%s) - t0 + 30) / 60 ))
    if (( rc == 0 )); then res=pass; else res=FAIL; overall=1; fi
    detail=$(detail_of "$s" 2>/dev/null | tr '\t\n' '  ')
    printf '%s\t%s\t%s\t%s\n' "$s" "$res" "$mins" "$detail" >> "$RESULTS"
    log "$s: $res in ${mins} min${detail:+ ($detail)}"
    if (( rc != 0 && FAIL_FAST )); then break; fi
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
