#!/bin/bash
# Build and run handoff (X4) with the official GCC 16 image. Linux only.
# usage: experiments/mutable_relocatable/build_handoff.sh [reps] ; run from the repository root.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
IMG=${XOFFSET_GCC16_IMAGE:-mirror.gcr.io/library/gcc:16}
REPS=${1:-101}
docker run --rm -v "$ROOT":/src -w /src "$IMG" bash -c "
  set -e
  mkdir -p build/mutable_relocatable
  g++ -std=c++26 -freflection -O3 -DNDEBUG \
      -I. -Iexternal/typelayout/include -Ibench/slg/include \
      experiments/mutable_relocatable/handoff.cpp -o build/mutable_relocatable/handoff -pthread
  taskset -c 2,3 build/mutable_relocatable/handoff $REPS"
