#!/bin/bash
set -e
cd /Volumes/XOffsetCS/diag/freeblocks
SC=/Volumes/XOffsetCS/mac-validation/business/meta-r1m2/experiments/serialization_comparison
C=/Volumes/XOffsetCS/candidates/w7
E=/Users/ximi/workspace/XOffsetDatastructure-mac-20261020/external
B=()
for l in container core config throw_exception static_assert container_hash type_traits winapi intrusive predef; do B+=(-isystem "$E/boost/libs/$l/include"); done
/Volumes/XOffsetCS/toolchain/p2996-d77eff1/bin/clang++ -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ -O2 \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk -L/Volumes/XOffsetCS/toolchain/p2996-d77eff1/lib \
  -Wl,-rpath,/Volumes/XOffsetCS/toolchain/p2996-d77eff1/lib -I"$SC/include" -I"$SC" -I"$C" -isystem "$E/typelayout/include" "${B[@]}" \
  wire_composition.cpp -o wire_composition
