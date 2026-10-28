#!/usr/bin/env bash
# SC-8 differential: builds tests/fuzz/validation_outcomes.cpp from one
# checkout twice, with exceptions (throwing entries wrapped by the try_*
# entries; -DXOFFSET_OUTCOMES_NO_RAW drops the raw and receive columns, which
# a build without exceptions does not have) and with -fno-exceptions (the
# try_* entries return failures up the receive path), and requires, per
# seed, the same typed load, reload and direct reload outcome for every
# mutated wire, exact error messages included.
#
# usage: tools/no_exceptions_differential.sh <dir> <cases> <seed>...
# Compiler: $CXX (default g++) with $XOFFSET_DIFF_FLAGS, as in
# tools/validation_differential.sh.
set -euo pipefail
dir=$1 cases=$2
shift 2
out=${XOFFSET_DIFF_OUT:-$dir/build/no-exceptions-differential}
cxx=${CXX:-g++}
read -r -a lang <<< "${XOFFSET_DIFF_FLAGS:--std=c++26 -freflection}"
mkdir -p "$out"
flags=(-I"$dir" -isystem "$dir/external/typelayout/include")
for lib in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef; do
    flags+=(-isystem "$dir/external/boost/libs/$lib/include")
done
"$cxx" -O2 "${lang[@]}" "${flags[@]}" -DXOFFSET_OUTCOMES_NO_RAW "$dir/tests/fuzz/validation_outcomes.cpp" \
    -o "$out/outcomes-exceptions" & a=$!
"$cxx" -O2 "${lang[@]}" "${flags[@]}" -fno-exceptions "$dir/tests/fuzz/validation_outcomes.cpp" \
    -o "$out/outcomes-no-exceptions" & b=$!
wait $a && wait $b
status=0
for seed in "$@"; do
    "$out/outcomes-exceptions" "$cases" "$seed" > "$out/exceptions-$seed.txt" & a=$!
    "$out/outcomes-no-exceptions" "$cases" "$seed" > "$out/no-exceptions-$seed.txt" & b=$!
    wait $a && wait $b
    if cmp -s "$out/exceptions-$seed.txt" "$out/no-exceptions-$seed.txt"; then
        echo "seed $seed: $cases cases, exceptions on/off identical outcomes"
    else
        echo "seed $seed: OUTCOMES DIFFER between exceptions on and off"
        diff "$out/exceptions-$seed.txt" "$out/no-exceptions-$seed.txt" | head -40 || true
        status=1
    fi
done
exit $status
