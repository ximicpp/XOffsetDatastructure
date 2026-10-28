# iPhone (arm64, iOS 17+) with Xcode's Apple clang and the iphoneos SDK, C++20 or C++23.
#
#   cmake -S . -B build/iphone -G Ninja -DXOFFSET_CXX_STANDARD=20 -DXOFFSET_TEST_HOST=ON \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios-device.cmake -DXO_HOST_DEVICE=<udid>
#
# A phone runs no command-line programs: with XOFFSET_TEST_HOST=ON every program is
# also built as a library of the test host app (tools/test_host), and CTest runs each
# program through tools/test_host/xo_host_run.py, which launches the app on the phone
# with devicectl. The app must be signed and installed first
# (tools/test_host/sign_ios_app.sh); the phone scripts do both.
# std::pmr lives in the system libc++ from iOS 17, hence the deployment floor.
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_OSX_ARCHITECTURES arm64)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0 CACHE STRING "Minimum iOS deployment version" FORCE)
endif()
set(CMAKE_MACOSX_BUNDLE OFF)
set(XO_HOST_DEVICE "" CACHE STRING "iPhone for CTest (devicectl device identifier)")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XO_HOST_DEVICE)
find_program(XOFFSET_PYTHON3 python3 REQUIRED)
set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_PYTHON3};${CMAKE_CURRENT_LIST_DIR}/../../tools/test_host/xo_host_run.py;--backend;device;--device;${XO_HOST_DEVICE}")
