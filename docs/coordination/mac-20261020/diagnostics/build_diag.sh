#!/bin/bash
# usage: build_diag.sh <name> <xoffset-source-dir>   -> /Volumes/XOffsetCS/diag/build-<name>/bin/sc_xoffset
set -euo pipefail
T=/Volumes/XOffsetCS/toolchain/p2996-d77eff1; SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
COMP=/Volumes/XOffsetCS/competitors/sources; I=/Volumes/XOffsetCS/mac-validation/install
META=${META:-/Volumes/XOffsetCS/mac-validation/business/meta-r1m2/experiments/serialization_comparison}
B=/Volumes/XOffsetCS/diag/build-$1; [ -e $B ] && { echo "exists $B"; exit 2; }
export LC_ALL=C LANG=C TZ=UTC PATH=/usr/bin:/bin:/opt/homebrew/bin
/opt/homebrew/bin/cmake -G Ninja -S $META -B $B -DCMAKE_MAKE_PROGRAM=/opt/homebrew/bin/ninja -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_CXX_COMPILER=$T/bin/clang++ -DCMAKE_OSX_SYSROOT=$SDK -DCMAKE_OSX_ARCHITECTURES=arm64 \
 "-DCMAKE_CXX_FLAGS=-stdlib=libc++ -pthread -isysroot $SDK ${EXTRA_CXX:-}" "-DCMAKE_EXE_LINKER_FLAGS=-L$T/lib -Wl,-rpath,$T/lib" \
 -DSC_XOFFSET_SOURCE=$2 -DSC_CISTA_SOURCE=$COMP/cista-v0.16 -DSC_FLATBUFFERS_SOURCE=$COMP/flatbuffers-v25.12.19 \
 -DSC_CAPNPROTO_SOURCE=$COMP/capnproto-v1.5.0 -DSC_CEREAL_SOURCE=$COMP/cereal-v1.3.2 -DSC_CAPNPROTO_PREFIX=$I/capnproto \
 -DSC_CAPNP=$I/capnproto/bin/capnp -DSC_CAPNPC_CPP=$I/capnproto/bin/capnpc-c++ -DSC_FLATC=$I/flatbuffers/bin/flatc > $B.configure.log 2>&1
/opt/homebrew/bin/cmake --build $B --target ${TARGETS:-sc_xoffset} --parallel 8 > $B.build.log 2>&1
ls -l $B/bin/sc_xoffset
