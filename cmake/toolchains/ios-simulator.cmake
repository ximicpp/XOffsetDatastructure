# iOS simulator (arm64) with Xcode's Apple clang and the iOS simulator SDK, C++20 or
# C++23. Not yet run: it needs Xcode and an iOS simulator runtime on the Mac.
#
#   cmake -S . -B build/iossim -G Ninja -DXOFFSET_CXX_STANDARD=20 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios-simulator.cmake
#
# CTest runs each program with `xcrun simctl spawn`, on the simulator device named by
# XOFFSET_IOS_SIM_DEVICE (a UDID, or "booted" for the one already booted). With
# -DXOFFSET_TEST_HOST=ON it runs them the way a phone does instead: inside the test
# host app (tools/test_host), through xo_host_run.py --backend sim.
# std::pmr lives in the system libc++ from iOS 17, hence the deployment floor.
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_SYSROOT iphonesimulator)
set(CMAKE_OSX_ARCHITECTURES arm64)
# A cache entry: project() would otherwise create an empty one over a plain variable.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0 CACHE STRING "Minimum iOS deployment version" FORCE)
endif()
# Plain executables, not app bundles, so simctl spawn can run them.
set(CMAKE_MACOSX_BUNDLE OFF)
set(XOFFSET_IOS_SIM_DEVICE booted CACHE STRING "Simulator device for CTest (UDID or booted)")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XOFFSET_IOS_SIM_DEVICE XOFFSET_TEST_HOST)
if(XOFFSET_TEST_HOST)
    find_program(XOFFSET_PYTHON3 python3 REQUIRED)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_PYTHON3};${CMAKE_CURRENT_LIST_DIR}/../../tools/test_host/xo_host_run.py;--backend;sim;--device;${XOFFSET_IOS_SIM_DEVICE}")
else()
    find_program(XOFFSET_XCRUN xcrun REQUIRED)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_XCRUN};simctl;spawn;${XOFFSET_IOS_SIM_DEVICE}")
endif()
