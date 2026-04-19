#ifndef XOFFSET_SIGNATURE_TARGET_MATRIX_HPP
#define XOFFSET_SIGNATURE_TARGET_MATRIX_HPP

#include <array>
#include <cstddef>
#include <string_view>

namespace xoffset::signature {

struct SignatureTargetSpec {
    std::string_view platform_name;
    std::string_view display_name;
    std::string_view arch_prefix;
    std::size_t pointer_size;
    std::size_t sizeof_long;
    std::size_t sizeof_wchar_t;
    std::size_t sizeof_long_double;
    std::size_t max_align;
    std::string_view data_model;
};

inline constexpr std::array<SignatureTargetSpec, 4> target_matrix = {{
    {
        .platform_name = "x86_64_windows_clang",
        .display_name = "x86-64 Windows (Clang)",
        .arch_prefix = "[64-le]",
        .pointer_size = 8,
        .sizeof_long = 4,
        .sizeof_wchar_t = 2,
        .sizeof_long_double = 8,
        .max_align = 16,
        .data_model = "LLP64",
    },
    {
        .platform_name = "x86_64_linux_clang",
        .display_name = "x86-64 Linux (Clang)",
        .arch_prefix = "[64-le]",
        .pointer_size = 8,
        .sizeof_long = 8,
        .sizeof_wchar_t = 4,
        .sizeof_long_double = 16,
        .max_align = 16,
        .data_model = "LP64",
    },
    {
        .platform_name = "arm64_ios_clang",
        .display_name = "AArch64 iOS (Clang)",
        .arch_prefix = "[64-le]",
        .pointer_size = 8,
        .sizeof_long = 8,
        .sizeof_wchar_t = 4,
        .sizeof_long_double = 8,
        .max_align = 8,
        .data_model = "LP64",
    },
    {
        .platform_name = "arm64_android_clang",
        .display_name = "AArch64 Android (Clang)",
        .arch_prefix = "[64-le]",
        .pointer_size = 8,
        .sizeof_long = 8,
        .sizeof_wchar_t = 4,
        .sizeof_long_double = 16,
        .max_align = 16,
        .data_model = "LP64",
    },
}};

inline constexpr std::size_t target_matrix_count = target_matrix.size();

} // namespace xoffset::signature

#endif // XOFFSET_SIGNATURE_TARGET_MATRIX_HPP
