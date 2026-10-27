# Native target macos, C++23

- 2026-10-27T02:31:13Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.1.1.101)
- CTest runner: native
- extra CMake arguments: 
- reference corpus: `/Volumes/XOffsetCS/native-c/ref-corpus` (       6 files)

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 101 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read (reference wires accepted, same bytes) | passed | `native-logs/corpus-read.log` |
| corpus diff -r against reference | passed | `native-logs/corpus-diff.log` |
