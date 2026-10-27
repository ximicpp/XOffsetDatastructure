#!/usr/bin/env bash
# Cross-build wire matrix (b5): every build writes the cross-compiler corpus
# (tests/test_cross_compiler_wire.cpp), every build reads every other build's
# corpus through load_verified and requires it to equal its own wire byte for
# byte, and each corpus is compared with the first build's with cmp.
#
#   tools/wire_matrix.sh OUT NAME=CMD [NAME=CMD ...]
#
# CMD runs one build's test_cross_compiler_wire: its path, or a wrapper and the
# path (for example a docker run prefix for the GCC 16 image). OUT receives one
# directory of .xwire files per build, logs/, and matrix.md (rows: the build
# that wrote, columns: the build that read). The first build is the reference
# for the byte comparison. Exit status 0 when every write, read and comparison
# passed.
set -uo pipefail

(( $# >= 2 )) || { sed -n '2,15p' "$0"; exit 2; }
out=$1; shift
names=(); cmds=()
for arg; do
    [[ $arg == *=* ]] || { echo "wire_matrix: expected NAME=CMD, got '$arg'" >&2; exit 2; }
    names+=("${arg%%=*}"); cmds+=("${arg#*=}")
done
rm -rf "$out"; mkdir -p "$out/logs"
rc=0; n=${#names[@]}
declare -A wrote same got

for ((i = 0; i < n; i++)); do
    w=${names[i]}
    if eval "${cmds[i]} --write '$out/$w'" > "$out/logs/write-$w.txt" 2>&1 &&
       compgen -G "$out/$w/*.xwire" > /dev/null; then
        wrote[$w]=1
    else
        wrote[$w]=0; rc=1
    fi
done

ref=${names[0]}
for ((i = 0; i < n; i++)); do
    w=${names[i]}
    if [[ ${wrote[$w]} != 1 || ${wrote[$ref]} != 1 ]]; then same[$w]="-"; continue; fi
    if diff -rq "$out/$ref" "$out/$w" > "$out/logs/cmp-$w.txt" 2>&1; then
        same[$w]="same ($(ls "$out/$w" | wc -l) files)"
    else
        same[$w]="DIFFERENT"; rc=1
    fi
done

for ((r = 0; r < n; r++)); do
    for ((i = 0; i < n; i++)); do
        w=${names[i]} reader=${names[r]}
        if [[ ${wrote[$w]} != 1 ]]; then got[$w,$reader]="-"; continue; fi
        if eval "${cmds[r]} --read '$out/$w'" > "$out/logs/read-$w-by-$reader.txt" 2>&1; then
            got[$w,$reader]="ok"
        else
            got[$w,$reader]="FAIL"; rc=1
        fi
    done
done

{
    printf '| written by | bytes vs %s |' "$ref"
    for reader in "${names[@]}"; do printf ' read by %s |' "$reader"; done
    printf '\n| --- | --- |'
    for reader in "${names[@]}"; do printf ' --- |'; done
    printf '\n'
    for w in "${names[@]}"; do
        if [[ ${wrote[$w]} == 1 ]]; then printf '| %s | %s |' "$w" "${same[$w]}"; else printf '| %s | WRITE FAILED |' "$w"; fi
        for reader in "${names[@]}"; do printf ' %s |' "${got[$w,$reader]}"; done
        printf '\n'
    done
} > "$out/matrix.md"
cat "$out/matrix.md"
fails=$(grep -o 'FAIL\|DIFFERENT' "$out/matrix.md" | wc -l)
echo "wire matrix: $n builds, $(( n * n )) reads, $fails failures"
exit $rc
