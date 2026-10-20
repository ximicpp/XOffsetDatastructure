#!/bin/bash
# B5 diagnostic build (single TU, no parallel jobs). Hooked header copy in src/ shadows the candidate.
set -e
cd /Volumes/XOffsetCS/diag/b5
SC=/Volumes/XOffsetCS/mac-validation/business/meta-w9a/experiments/serialization_comparison
E=/Users/ximi/workspace/XOffsetDatastructure-mac-20261020/external
B=()
for l in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef; do B+=(-isystem "$E/boost/libs/$l/include"); done
/Volumes/XOffsetCS/toolchain/p2996-d77eff1/bin/clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ -O2 \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk -L/Volumes/XOffsetCS/toolchain/p2996-d77eff1/lib \
  -Wl,-rpath,/Volumes/XOffsetCS/toolchain/p2996-d77eff1/lib -I"$SC/include" -I"$SC" -I/Volumes/XOffsetCS/diag/b5/src \
  -isystem "$E/typelayout/include" "${B[@]}" replay.cpp -o replay
