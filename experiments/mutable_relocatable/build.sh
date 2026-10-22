#!/bin/bash
# Build and run reloc_tax with the official GCC 16 image (P2996 reflection via -freflection).
# usage: experiments/mutable_relocatable/build.sh [reps] ; run from the repository root.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
IMG=${XOFFSET_GCC16_IMAGE:-mirror.gcr.io/library/gcc:16}
REPS=${1:-201}
docker run --rm -v "$ROOT":/src -w /src "$IMG" bash -c "
  set -e
  mkdir -p build/mutable_relocatable
  g++ -std=c++26 -freflection -O3 -DNDEBUG \
      -I. -Iexternal/typelayout/include -Ibench/slg/include \
      experiments/mutable_relocatable/reloc_tax.cpp -o build/mutable_relocatable/reloc_tax -pthread
  taskset -c 2 build/mutable_relocatable/reloc_tax $REPS"
