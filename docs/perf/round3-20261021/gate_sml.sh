#!/bin/zsh
# N4 gate: S/M/L model truth (canonical) and query results identical to W9a's
# functional run; V1 input rejected. usage: gate_sml.sh <sc_xoffset> <outdir>
B=$1; O=$2; mkdir -p $O || exit 2
W=/Volumes/XOffsetCS/mac-validation/runs/w9a-functional-normal
P=$W/prepared; rc=0
for z in S M L; do
  $B encode --canonical $P/$z/input.canonical.bin --wire $O/$z.input.wire || { echo "$z encode FAIL"; rc=1; continue; }
  $B inspect --wire $O/$z.input.wire --canonical $O/$z.initial.canonical || rc=1
  $B run --wire $O/$z.input.wire --trace $P/$z/trace.bin --wire-out $O/$z.wire --canonical-out $O/$z.canonical --queries-out $O/$z.queries --result $O/$z.native.json || rc=1
  $B inspect --wire $O/$z.wire --canonical $O/$z.reloaded.canonical || rc=1
  $B validate --wire $O/$z.input.wire --trace $P/$z/trace.bin --result $O/$z.validation.json || rc=1
  for f in initial.canonical canonical reloaded.canonical queries; do
    cmp -s $O/$z.$f $W/commands/xoffset-$z.$f && echo "$z $f identical" || { echo "$z $f DIFFERENT"; rc=1; }
  done
  echo "$z wire input $(stat -f %z $O/$z.input.wire) (W9a $(stat -f %z $W/commands/xoffset-$z.input.wire)) final $(stat -f %z $O/$z.wire) (W9a $(stat -f %z $W/commands/xoffset-$z.wire))"
done
# V1 rejection with a real W9a wire.
$B inspect --wire $W/commands/xoffset-S.input.wire --canonical $O/v1.canonical > $O/v1.out 2> $O/v1.err && { echo "V1 ACCEPTED"; rc=1; } || { grep -q "format version 1" $O/v1.err && echo "V1 rejected: $(head -c 160 $O/v1.err)" || { echo "V1 rejected without clear error: $(head -c 200 $O/v1.err)"; rc=1; }; }
exit $rc
