# NATIVE_MACOS 2026-10-27T02:31:18Z, commit dcb1dfdb9e6e2a3a2ab00bad390881a4e38e8ea5
ProductName:		macOS
ProductVersion:		26.3
BuildVersion:		25D125
Apple clang version 21.0.0 (clang-2100.1.1.101)
/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
cmake version 4.4.4
ninja 1.13.2

# Native target macos, C++20

- 2026-10-27T02:30:40Z, source tree without .git (exported)
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

C++26 default deployment target: passed

## /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__config_site
25:#define _LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS 1
## /Volumes/XOffsetCS/toolchain/p2996-d77eff1/include/c++/v1/__config_site
25:#define _LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS 0
## Apple clang default include of __config_site
... /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__config_site
