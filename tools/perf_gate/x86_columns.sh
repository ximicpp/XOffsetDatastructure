#!/usr/bin/env bash
# Cloud x86 and client-uplink columns (docs/RELEASING.md section 2a): runs a
# GCC 16 -O3 bench/slg binary once per format per batch, every format in a
# process of its own, the batches interleaved, on one pinned core of the gcc:16
# container; then x86_columns.py judges the cells.
#
# usage: tools/perf_gate/x86_columns.sh <slg_bench> <outdir> [batches] [cycles]
#   env FORMATS   formats to run (default "xoffset_trusted zpp_bits cista_unchecked")
#       TAG       file prefix (default head)
#       CPU       core to pin (default 2)
#       IMAGE     container image (default the gcc:16 digest local_validate uses)
#       D3=1      also run, per batch, one SLG_D3=1 process (native on) per XOffset
#                 format under tag "<TAG>d3"; its D* rows are judged against the
#                 rivals' U* rows of the same batch (x86_columns.py --d3-tag)
set -euo pipefail
BIN=$(readlink -f "$1"); OUT=$(readlink -f -m "$2"); NB=${3:-10}; CY=${4:-21}
FORMATS=${FORMATS:-"xoffset_trusted zpp_bits cista_unchecked"}
TAG=${TAG:-head}; CPU=${CPU:-2}
IMAGE=${IMAGE:-mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240}
mkdir -p "$OUT"
run() {  # tag extra-env format batch
    # The main processes turn native off; the D3 ones must not (with
    # SLG_NATIVE_OFF=1 the bench skips the D* rows too).
    docker run --rm --cpuset-cpus="$CPU" -e SLG_REUSE=1 -e SLG_CASES=demo $2 \
        -v "$(dirname "$BIN")":/bin_dir:ro -v "$OUT":/out "$IMAGE" \
        /bin_dir/"$(basename "$BIN")" --mode bench --scale M --cycles "$CY" --seed 20261021 \
        --formats "$3" --out /out/"$1_$3_$4.jsonl" > /dev/null
}
for b in $(seq 1 "$NB"); do
    for f in $FORMATS; do run "$TAG" "-e SLG_NATIVE_OFF=1" "$f" "$b"; done
    if [[ ${D3:-0} == 1 ]]; then
        for f in $FORMATS; do
            if [[ $f == xoffset* ]]; then run "${TAG}d3" "-e SLG_D3=1" "$f" "$b"; fi
        done
    fi
    echo "batch $b/$NB done $(date -u +%H:%M:%SZ)"
done
RIVALS=$(echo "$FORMATS" | tr ' ' '\n' | grep -v '^xoffset' | paste -sd,)
D3ARGS=()
if [[ ${D3:-0} == 1 ]]; then D3ARGS=(--d3-tag "${TAG}d3"); fi
python3 "$(dirname "$0")/x86_columns.py" "$OUT" --tag "$TAG" "${D3ARGS[@]}" --rivals "$RIVALS"
