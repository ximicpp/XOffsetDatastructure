# Native target macos, C++20

- 2026-11-04T19:23:45Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.3.34.2)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 
- reference corpus: `/Volumes/XOffsetCS/apple330/ref-corpus` (      24 files)

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 148 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read (reference wires accepted, same bytes) | passed | `native-logs/corpus-read.log` |
| corpus diff -r against reference | passed | `native-logs/corpus-diff.log` |
