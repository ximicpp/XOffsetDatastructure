# Native target macos, C++20

- 2026-11-17T13:47:54Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.3.34.2)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 211 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |
