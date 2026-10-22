#!/usr/bin/env bash
# K4 differential: builds tests/fuzz/validation_outcomes.cpp (from HEAD_DIR)
# against two library checkouts and compares, per seed, the outcome digests of
# typed load, reload, raw validation and verified receive over the same
# mutated wires (exact error messages included). Same inputs with different
# outcomes fail. Different inputs (the two libraries save differently) are
# reported as not comparable.
#
# usage: tools/validation_differential.sh <base dir> <head dir> <cases> <seed>...
# Compiler: $CXX (default g++, as in the GCC 16 image) with $XOFFSET_DIFF_FLAGS
# (default -std=c++26 -freflection; P2996 Clang needs
# "-std=c++26 -freflection -fexpansion-statements -stdlib=libc++"). Both
# checkouts use the dependency include trees of HEAD_DIR.
set -euo pipefail
base=$1 head=$2 cases=$3
shift 3
out=${XOFFSET_DIFF_OUT:-$head/build/validation-differential}
cxx=${CXX:-g++}
read -r -a lang <<< "${XOFFSET_DIFF_FLAGS:--std=c++26 -freflection}"
mkdir -p "$out"
flags() {
    printf -- '-I%s -isystem %s/external/typelayout/include' "$1" "$head"
    for lib in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef; do
        printf -- ' -isystem %s/external/boost/libs/%s/include' "$head" "$lib"
    done
}
# shellcheck disable=SC2046
"$cxx" -O2 "${lang[@]}" $(flags "$base") "$head/tests/fuzz/validation_outcomes.cpp" -o "$out/outcomes-base"
# shellcheck disable=SC2046
"$cxx" -O2 "${lang[@]}" $(flags "$head") "$head/tests/fuzz/validation_outcomes.cpp" -o "$out/outcomes-head"
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
    else
        echo "seed $seed: inputs differ (the libraries save differently); not comparable"
    fi
done
exit $status
