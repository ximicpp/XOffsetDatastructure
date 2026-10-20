#!/bin/zsh
# usage: validate3.sh <source-dir> <tag> : native full, TSan all, ASan+UBSan all; new result dirs
cd /Users/ximi/workspace/XOffsetDatastructure-mac-20261020 || exit 2
T=/Volumes/XOffsetCS/toolchain/p2996-d77eff1; R=/Volumes/XOffsetCS/results; S=$1; G=$2
COM=(--deps external --compiler $T/bin/clang++ --sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk --library-dir $T/lib)
python3 -I -B tools/validate_vm.py native --suite full --source-dir $S $COM --jobs 8 --out $R/full-native-$G > $R/$G-native.log 2>&1; echo "$G native=$? $(grep -h 'tests passed' $R/full-native-$G/ctest.log)"
TG=$(python3 -c "import json;print(','.join(json.load(open('$R/full-native-$G/build-test-inventory.json'))['runtime_test_programs']))")
python3 -I -B tools/validate_vm.py native --suite full --source-dir $S $COM --sanitizer thread --runtime-dir $T/lib/clang/21/lib/darwin --targets "$TG" --jobs 8 --out $R/full-tsan-$G > $R/$G-tsan.log 2>&1; echo "$G tsan=$? $(grep -h 'tests passed' $R/full-tsan-$G/ctest.log)"
python3 -I -B tools/validate_vm.py native --suite full --source-dir $S $COM --sanitizer address-undefined --targets "$TG" --jobs 8 --out $R/full-asan-$G > $R/$G-asan.log 2>&1; echo "$G asan=$? $(grep -h 'tests passed' $R/full-asan-$G/ctest.log)"
grep -E "\(Failed\)|aborted" $R/full-asan-$G/ctest.log | awk '{print $3}' | sort | diff - /tmp/claude-501/c14f > /dev/null && echo "$G asan-failset=platform9" || echo "$G asan-failset=DIFFERENT"
