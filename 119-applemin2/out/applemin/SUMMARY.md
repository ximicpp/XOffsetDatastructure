# APPLEMIN, main 7d69d66e0f9b073cd0c1037d1dd80cae99870a91
ProductName:		macOS
ProductVersion:		27.2.1
BuildVersion:		26B131
Xcode 27.2
Build version 27B52
Apple clang version 21.0.0 (clang-2100.4.1.1)

# Native target macos, C++20

- 2026-11-24T10:29:09Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 215 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |

# Native target macos, C++23

- 2026-11-24T10:30:36Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: native
- extra CMake arguments: 

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | passed | `native-logs/ctest.log` |
| ctest summary | 100% tests passed out of 215 | |
| corpus write | passed | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |

# Native target ios-sim, C++20

- 2026-11-24T10:32:14Z, source tree without .git (exported)
- compiler: `/usr/bin/c++` — Apple clang version 21.0.0 (clang-2100.4.1.1)
- machine: Darwin 27.2.0 arm64
- CTest runner: /usr/bin/xcrun simctl spawn BF939968-6A21-42F1-88EB-D44C85C3E0FC
- extra CMake arguments: -DXOFFSET_IOS_SIM_DEVICE=BF939968-6A21-42F1-88EB-D44C85C3E0FC

| step | result | log |
| --- | --- | --- |
| configure | passed | `native-logs/configure.log` |
| build | passed | `native-logs/build.log` |
| ctest | **failed** (rc 8) | `native-logs/ctest.log` |
| ctest summary | 9% tests passed, 196 tests failed out of 215 | |
| corpus write | **failed** (rc 149) | `native-logs/corpus-write.log` |
| corpus read / diff | not run: no --ref | |

ios-device-c++20 configure+build rc=0

# APPLEMIN checks, main 7d69d66e0f9b073cd0c1037d1dd80cae99870a91

macos 12.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 0 0 0
macos 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 1 1 1
ios-sim 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 0 0 0
ios 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 0 0 0

== macos-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=12.0
sample binary: /Volumes/XOffsetCS/applemin/build-macos-c++20/bin/Release/test_adaptive_reservation
 platform 1
    minos 12.0
test binaries: 159; importing std::pmr symbols: 0
minos count:  159 12.0
== macos-c++23
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=12.0
sample binary: /Volumes/XOffsetCS/applemin/build-macos-c++23/bin/Release/test_adaptive_reservation
 platform 1
    minos 12.0
test binaries: 159; importing std::pmr symbols: 0
minos count:  159 12.0
== ios-sim-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=14.0
sample binary: /Volumes/XOffsetCS/applemin/build-ios-sim-c++20/bin/Release/test_adaptive_reservation
 platform 7
    minos 14.0
test binaries: 159; importing std::pmr symbols: 0
minos count:  159 14.0
== ios-device-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=14.0
sample binary: /Volumes/XOffsetCS/applemin/build-ios-device-c++20/bin/Release/test_adaptive_reservation
 platform 2
    minos 14.0
test binaries: 159; importing std::pmr symbols: 0
minos count:  159 14.0
