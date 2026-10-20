#!/bin/zsh
B=/Volumes/XOffsetCS/mac-validation/business; V=/Volumes/XOffsetCS/mac-validation
PY=$(/opt/homebrew/bin/python3.13 -I -c 'import sys;print(sys.executable)')
cd $B || exit 2
SC_MAC_META=c14 $PY -B $B/mac_campaign.py prepare --fixture-module $B/meta-c14/experiments/serialization_comparison/model/reference.py --out $V/prepared-v1 | tail -c 400; echo
for f in input.canonical.bin trace.bin final.canonical.bin query-before.bin query-after.bin; do cmp $V/prepared-v1/S/$f $B/meta-c14/experiments/serialization_comparison/fixtures/golden-S/$f && echo "golden-S $f same"; done
for id in c14 r1m2; do for mode in normal san; do $PY -I -B $B/make_profiles.py --meta $id --mode $mode --out $V/profiles/$id/profile-$mode.json || echo "profile fail $id $mode"; done; done
for id in c14 r1m2; do for mode in normal san; do
  out=$V/runs/$id-functional-$mode
  SC_MAC_META=$id $PY -B $B/runner/experiments/vm_coordination/run_job.py run --owner W3 --kind functional --out $out/queue --timeout-seconds 3600 --wait-seconds 0 -- \
    $PY -B $B/mac_campaign.py functional --prepared $V/prepared-v1 --profile $V/profiles/$id/profile-$mode.json --out $out --queue $out/queue > $out.runner.log 2>&1
  echo "functional $id $mode rc=$?"
  pin=$(SC_MAC_META=$id $PY -B $B/mac_campaign.py pin --bundle $out | $PY -I -c 'import json,sys;print(json.load(sys.stdin)["pin"])')
  echo "$pin" > $out.postrun-pin.txt
  SC_MAC_META=$id $PY -B $B/mac_campaign.py audit --bundle $out --postrun-pin $pin | head -c 300; echo
  $PY -I -c 'import json,sys;s=json.load(open(sys.argv[1]+"/status.json"));print("STATUS",s["status"],len(s["attempted"]),len(s["completed"]),len(s["unexecuted"]),s["error"])' $out
done; done
