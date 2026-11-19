#!/usr/bin/env bash
# K4 differential: builds tests/fuzz/validation_outcomes.cpp (from HEAD_DIR)
# against two library checkouts and compares, per seed, the outcome digests of
# typed load and reload over the same mutated wires (exact error messages
# included), per payload: a mutated wire
# with a well-formed extent table is validated as its dense equivalent, since
# releases may accept different sparse framings of one payload. Same inputs
# with different outcomes fail. Different inputs (the two libraries save differently) are
# reported as not comparable.
#
# usage: tools/validation_differential.sh <base dir> <head dir> <cases> <seed>...
# Compiler: $CXX (default g++, as in the GCC 16 image) with $XOFFSET_DIFF_FLAGS
# (default -std=c++26 -freflection; P2996 Clang needs
# "-std=c++26 -freflection -fexpansion-statements -stdlib=libc++"). Both
# checkouts use the dependency include trees of HEAD_DIR.
# The base build may use other flags ($XOFFSET_DIFF_BASE_FLAGS, default the same):
# base = head = HEAD with -std=c++26 -freflection against -std=c++20 compares the
# reflection build with the C++20 field-list build. TypeLayout is on the include
# path only for C++26 flags. XOFFSET_DIFF_SAME_INPUTS=1 makes different inputs
# (different saved wires) a failure instead of "not comparable".
# Checkouts of different wire format majors (a format break, such as 2.1 ->
# 3.0) are not comparable: the harness is written for HEAD's format, so the
# script reports that and exits 0 without building.
set -euo pipefail
base=$1 head=$2 cases=$3
shift 3
out=${XOFFSET_DIFF_OUT:-$head/build/validation-differential}
cxx=${CXX:-g++}
read -r -a lang <<< "${XOFFSET_DIFF_FLAGS:--std=c++26 -freflection}"
read -r -a base_lang <<< "${XOFFSET_DIFF_BASE_FLAGS:-${lang[*]}}"
mkdir -p "$out"
format_major() {   # <checkout>: XWIRE_FORMAT_MAJOR, or the image format major it names
    grep -ohE '(XWIRE_FORMAT_MAJOR|image_format_major) = [0-9]+' "$1"/xoffset_wire.hpp "$1"/xoffset_arena.hpp 2>/dev/null |
        grep -oE '[0-9]+$' | head -n 1
}
base_major=$(format_major "$base") head_major=$(format_major "$head")
if [[ -n $base_major && -n $head_major && $base_major != "$head_major" ]]; then
    for seed in "$@"; do
        echo "seed $seed: wire format $base_major (base) vs $head_major (head); not comparable"
    done
    exit 0
fi
flags() {   # <checkout> <language flags...>
    local dir=$1
    shift
    printf -- '-I%s' "$dir"
    case " $* " in *" -std=c++26 "*) printf -- ' -isystem %s/external/typelayout/include' "$head" ;; esac
    for lib in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef; do
        printf -- ' -isystem %s/external/boost/libs/%s/include' "$head" "$lib"
    done
}
# shellcheck disable=SC2046
"$cxx" -O2 "${base_lang[@]}" $(flags "$base" "${base_lang[@]}") "$head/tests/fuzz/validation_outcomes.cpp" -o "$out/outcomes-base"
# shellcheck disable=SC2046
"$cxx" -O2 "${lang[@]}" $(flags "$head" "${lang[@]}") "$head/tests/fuzz/validation_outcomes.cpp" -o "$out/outcomes-head"
status=0
for seed in "$@"; do
    "$out/outcomes-base" "$cases" "$seed" > "$out/base-$seed.txt" &
    "$out/outcomes-head" "$cases" "$seed" > "$out/head-$seed.txt" &
    wait
    if cmp -s "$out/base-$seed.txt" "$out/head-$seed.txt"; then
        echo "seed $seed: $cases cases, identical outcomes"
    elif cmp -s <(grep -o 'inputs [0-9]*' "$out/base-$seed.txt") <(grep -o 'inputs [0-9]*' "$out/head-$seed.txt"); then
        echo "seed $seed: OUTCOMES DIFFER for the same inputs"
        diff "$out/base-$seed.txt" "$out/head-$seed.txt" | head -40 || true
        status=1
    elif [[ ${XOFFSET_DIFF_SAME_INPUTS:-0} == 1 ]]; then
        echo "seed $seed: INPUTS DIFFER (the two builds must save the same bytes)"
        diff "$out/base-$seed.txt" "$out/head-$seed.txt" | head -40 || true
        status=1
    else
        echo "seed $seed: inputs differ (the libraries save differently); not comparable"
    fi
done
exit $status
