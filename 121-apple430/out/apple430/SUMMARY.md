# Apple clang client columns, v4.3.0 6906b78508edcffd2abbf13bda6cef6bcd27e8ac

# Native target macos, C++20

- 2026-11-25T09:07:57Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 247 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |

# Native target macos, C++23

- 2026-11-25T09:09:36Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 247 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |

# Native target ios-sim, C++20

- 2026-11-25T09:11:03Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: /usr/bin/xcrun simctl spawn BF939968-6A21-42F1-88EB-D44C85C3E0FC
- extra CMake arguments: -DXOFFSET_IOS_SIM_DEVICE=BF939968-6A21-42F1-88EB-D44C85C3E0FC

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 247 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |
