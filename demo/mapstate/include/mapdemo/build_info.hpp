// Map state demo: one line naming how this program was built, printed at
// start-up and stored with the results (client_build.txt), so every result
// row says which compiler, platform and schema backend produced it.
#pragma once
#include <string>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "mapdemo/codec.hpp"
#include "mapdemo/schema.hpp"

namespace mapdemo {

inline std::string build_info() {
    std::string s = "compiler=";
#if defined(__clang__) && defined(_MSC_VER)
    s += "clang-cl-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__apple_build_version__)
    s += "apple-clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__clang__)
    s += "clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(_MSC_VER)
    s += "msvc-" + std::to_string(_MSC_FULL_VER);
#elif defined(__GNUC__)
    s += "gcc-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    s += "unknown";
#endif
    s += " os=";
#if defined(__ANDROID__)
    s += "android-" + std::to_string(__ANDROID_API__);
#elif defined(__APPLE__) && defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
    s += "ios-simulator";
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    s += "ios";
#elif defined(__APPLE__)
    s += "macos";
#elif defined(_WIN32) && defined(__MINGW32__)
    s += "windows-mingw";
#elif defined(_WIN32)
    s += "windows";
#elif defined(__linux__)
    s += "linux";
#else
    s += "unknown";
#endif
    s += " arch=";
#if defined(__aarch64__) || defined(_M_ARM64)
    s += "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    s += "x86_64";
#else
    s += "other";
#endif
#if defined(_MSVC_LANG)
    s += " cxx=" + std::to_string(_MSVC_LANG);
#else
    s += " cxx=" + std::to_string(__cplusplus);
#endif
    s += XOFFSET_SCHEMA_REFLECTION ? " schema=reflection" : " schema=field-lists";
    s += codec::zstd_available() ? " zstd=1.5.7" : " zstd=no";
    return s;
}

} // namespace mapdemo
