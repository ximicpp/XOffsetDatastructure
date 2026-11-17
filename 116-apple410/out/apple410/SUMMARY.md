# Apple clang client columns, v4.1.0 9297f94f14fa6275d88fa3d1aeb112036c01a17a

# Native target macos, C++20

- 2026-11-17T13:53:09Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.3.34.2)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 
- reference corpus: `/Volumes/XOffsetCS/apple410/ref-corpus` (      24 files)

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 211 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read (reference wires accepted, same bytes) | passed | `native-logs/corpus-read.log` |
| corpus diff -r against reference | passed | `native-logs/corpus-diff.log` |

# Native target macos, C++23

- 2026-11-17T13:54:35Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.3.34.2)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 
- reference corpus: `/Volumes/XOffsetCS/apple410/ref-corpus` (      24 files)

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 211 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read (reference wires accepted, same bytes) | passed | `native-logs/corpus-read.log` |
| corpus diff -r against reference | passed | `native-logs/corpus-diff.log` |

# Native target ios-sim, C++20

- 2026-11-17T13:55:47Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.3.34.2)
- machine: Darwin 27.2.0 arm64
- CTest runner: /usr/bin/xcrun simctl spawn BF939968-6A21-42F1-88EB-D44C85C3E0FC
- extra CMake arguments: -DXOFFSET_IOS_SIM_DEVICE=BF939968-6A21-42F1-88EB-D44C85C3E0FC
- reference corpus: `/Volumes/XOffsetCS/apple410/ref-corpus` (      24 files)

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 211 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read (reference wires accepted, same bytes) | passed | `native-logs/corpus-read.log` |
| corpus diff -r against reference | passed | `native-logs/corpus-diff.log` |
