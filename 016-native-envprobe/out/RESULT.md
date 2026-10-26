# Mac native toolchain probe 2026-10-26T18:28:53Z
## Environment
```
$ sw_vers
ProductName:		macOS
ProductVersion:		26.3
BuildVersion:		25D125
rc=0
```
```
$ xcode-select -p
/Library/Developer/CommandLineTools
rc=0
```
```
$ clang++ --version
Apple clang version 21.0.0 (clang-2100.1.1.101)
Target: arm64-apple-darwin25.3.0
Thread model: posix
InstalledDir: /Library/Developer/CommandLineTools/usr/bin
rc=0
```
```
$ xcrun --show-sdk-path; xcrun --show-sdk-version
/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
26.5
rc=0
```
```
$ ls -d /Applications/Xcode*.app
ls: /Applications/Xcode*.app: No such file or directory
rc=1
```
```
$ xcrun --sdk iphonesimulator --show-sdk-path
xcrun: error: SDK "iphonesimulator" cannot be located
xcrun: error: SDK "iphonesimulator" cannot be located
xcrun: error: unable to lookup item 'Path' in SDK 'iphonesimulator'
rc=1
```
```
$ xcrun --sdk iphoneos --show-sdk-path
xcrun: error: SDK "iphoneos" cannot be located
xcrun: error: SDK "iphoneos" cannot be located
xcrun: error: unable to lookup item 'Path' in SDK 'iphoneos'
rc=1
```
```
$ xcrun simctl list runtimes
xcrun: error: unable to find utility "simctl", not a developer tool or in PATH
rc=72
```
```
$ pkgutil --pkg-info=com.apple.pkg.CLTools_Executables
package-id: com.apple.pkg.CLTools_Executables
version: 26.5.0.0.1777544298
volume: /
location: /
install-time: 1779122269
rc=0
```
```
$ df -g / /Users/ximi /Volumes/XOffsetCS
Filesystem     1G-blocks Used Available Capacity iused     ifree %iused  Mounted on
/dev/disk3s1s1       228   11        32    27%  453127 336059240    0%   /
/dev/disk3s5         228  173        32    85% 1343897 336059240    0%   /System/Volumes/Data
/dev/disk7s1         119   60        32    66%  638440 336059240    0%   /Volumes/XOffsetCS
rc=0
```
```
$ cmake --version | head -1; ninja --version
cmake version 4.4.4
1.13.2
rc=0
```
## SDK libc++ pmr availability markup
```
$ grep -n 'AVAILABILITY_PMR\|AVAILABILITY_HAS_PMR' '/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__configuration/availability.h'
340:#define _LIBCPP_AVAILABILITY_HAS_PMR _LIBCPP_INTRODUCED_IN_LLVM_16
341:#define _LIBCPP_AVAILABILITY_PMR
rc=0
```
```
$ grep -n -A8 '^// LLVM 16' '/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__configuration/availability.h' | head -20
183:// LLVM 16
184-#  if (defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 140000) ||       \
185-      (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 170000) ||     \
186-      (defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 170000) ||             \
187-      (defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 100000) ||       \
188-      (defined(__ENVIRONMENT_BRIDGE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_BRIDGE_OS_VERSION_MIN_REQUIRED__ < 80000) ||      \
189-      (defined(__ENVIRONMENT_DRIVERKIT_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_DRIVERKIT_VERSION_MIN_REQUIRED__ < 230000)
190-#    define _LIBCPP_INTRODUCED_IN_LLVM_16 0
191-#  else
rc=0
```
## System libc++ exports pmr
```
$ dyld_info -exports /usr/lib/libc++.1.dylib 2>/dev/null | grep -c pmr
33
rc=0
```
## pmr deployment-target probe (compile, link, run here, list pmr imports)
### -std=c++20 -mmacosx-version-min=10.15
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=10.15 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_10.15' && '/tmp/xo_envprobe.jWzIcx/p_20_10.15' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_10.15' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_10.15' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 11.0
rc=0
```
### -std=c++20 -mmacosx-version-min=11.0
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=11.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_11.0' && '/tmp/xo_envprobe.jWzIcx/p_20_11.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_11.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_11.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 11.0
rc=0
```
### -std=c++20 -mmacosx-version-min=12.0
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=12.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_12.0' && '/tmp/xo_envprobe.jWzIcx/p_20_12.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_12.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_12.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 12.0
rc=0
```
### -std=c++20 -mmacosx-version-min=13.0
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=13.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_13.0' && '/tmp/xo_envprobe.jWzIcx/p_20_13.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_13.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_13.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 13.0
rc=0
```
### -std=c++20 -mmacosx-version-min=13.3
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=13.3 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_13.3' && '/tmp/xo_envprobe.jWzIcx/p_20_13.3' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_13.3' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_13.3' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 13.3
rc=0
```
### -std=c++20 -mmacosx-version-min=14.0
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=14.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_14.0' && '/tmp/xo_envprobe.jWzIcx/p_20_14.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_14.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_14.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 14.0
rc=0
```
### -std=c++20 -mmacosx-version-min=15.0
```
$ clang++ -std=c++20 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=15.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_20_15.0' && '/tmp/xo_envprobe.jWzIcx/p_20_15.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_20_15.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_20_15.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 15.0
rc=0
```
### -std=c++23 -mmacosx-version-min=10.15
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=10.15 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_10.15' && '/tmp/xo_envprobe.jWzIcx/p_23_10.15' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_10.15' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_10.15' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 11.0
rc=0
```
### -std=c++23 -mmacosx-version-min=11.0
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=11.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_11.0' && '/tmp/xo_envprobe.jWzIcx/p_23_11.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_11.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_11.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 11.0
rc=0
```
### -std=c++23 -mmacosx-version-min=12.0
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=12.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_12.0' && '/tmp/xo_envprobe.jWzIcx/p_23_12.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_12.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_12.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 12.0
rc=0
```
### -std=c++23 -mmacosx-version-min=13.0
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=13.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_13.0' && '/tmp/xo_envprobe.jWzIcx/p_23_13.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_13.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_13.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 13.0
rc=0
```
### -std=c++23 -mmacosx-version-min=13.3
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=13.3 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_13.3' && '/tmp/xo_envprobe.jWzIcx/p_23_13.3' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_13.3' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_13.3' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 13.3
rc=0
```
### -std=c++23 -mmacosx-version-min=14.0
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=14.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_14.0' && '/tmp/xo_envprobe.jWzIcx/p_23_14.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_14.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_14.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 14.0
rc=0
```
### -std=c++23 -mmacosx-version-min=15.0
```
$ clang++ -std=c++23 -O2 -Werror=unguarded-availability-new -mmacosx-version-min=15.0 '/tmp/xo_envprobe.jWzIcx/p.cpp' -o '/tmp/xo_envprobe.jWzIcx/p_23_15.0' && '/tmp/xo_envprobe.jWzIcx/p_23_15.0' && (nm -u '/tmp/xo_envprobe.jWzIcx/p_23_15.0' | c++filt | grep -i pmr; otool -l '/tmp/xo_envprobe.jWzIcx/p_23_15.0' | grep -A4 LC_BUILD_VERSION | grep minos)
ok 42
std::__1::pmr::memory_resource::~memory_resource()
std::__1::pmr::new_delete_resource()
typeinfo for std::__1::pmr::memory_resource
vtable for std::__1::pmr::monotonic_buffer_resource
    minos 15.0
rc=0
```
## iOS simulator compile-only probe (needs an iOS SDK)
no iphonesimulator SDK
