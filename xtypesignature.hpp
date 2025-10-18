#pragma once
#include <boost/pfr.hpp>
#include <string_view>
#include <type_traits>
#include <iostream>

// Platform detection
#if defined(_MSC_VER)
    #define TYPESIG_PLATFORM_WINDOWS 1
    #define IS_LITTLE_ENDIAN 1
    #define FUNCTION_SIGNATURE __FUNCSIG__
#elif defined(__clang__)
    #define TYPESIG_PLATFORM_WINDOWS 0
    #define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define IS_LITTLE_ENDIAN 1
    #else
        #define IS_LITTLE_ENDIAN 0
    #endif
#elif defined(__GNUC__)
    #define TYPESIG_PLATFORM_WINDOWS 0
    #define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define IS_LITTLE_ENDIAN 1
    #else
        #define IS_LITTLE_ENDIAN 0
    #endif
#else
    #error "Unsupported compiler"
#endif

namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

    // Verify fundamental type sizes
    static_assert(sizeof(char) == 1, "char must be 1 byte");
    static_assert(sizeof(bool) == 1, "bool must be 1 byte");
    static_assert(sizeof(short) == 2, "short must be 2 bytes");
    static_assert(sizeof(int) == 4, "int must be 4 bytes");
    #ifdef _MSC_VER
    static_assert(sizeof(long) == 4, "long must be 4 bytes on Windows");
    #else
    static_assert(sizeof(long) == 8, "long must be 8 bytes on Unix-like systems");
    #endif
    static_assert(sizeof(long long) == 8, "long long must be 8 bytes");
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    static_assert(sizeof(long double) >= 8, "long double must be at least 8 bytes");

    // Verify integer type sizes
    static_assert(sizeof(int8_t) == 1, "int8_t must be 1 byte");
    static_assert(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
    static_assert(sizeof(int16_t) == 2, "int16_t must be 2 bytes");
    static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
    static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");
    static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
    static_assert(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
    static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

    // Verify pointer size and alignment
    static_assert(sizeof(void*) == 8, "Pointer size must be 8 bytes");
    static_assert(alignof(void*) == 8, "Pointer alignment must be 8 bytes");
    static_assert(alignof(max_align_t) >= 8, "Maximum alignment must be at least 8 bytes");

    // Verify platform requirements
    static_assert(sizeof(void*) == 8, "64-bit architecture required");
    static_assert(sizeof(size_t) == 8, "64-bit architecture required");
    static_assert(IS_LITTLE_ENDIAN, "Little-endian architecture required");

    template <typename T>
    struct always_false : std::false_type {};

    template <size_t N>
    struct CompileString {
        char value[N];
        static constexpr size_t size = N - 1;

        constexpr CompileString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) {
                value[i] = str[i];
            }
        }

        template <typename T>
        static constexpr CompileString<32> from_number(T num) noexcept {
            char result[32] = {};
            int idx = 0;

            if (num == 0) {
                result[0] = '0';
                idx = 1;
            }
            else {
                bool negative = std::is_signed_v<T> && num < 0;
                using UnsignedT = std::make_unsigned_t<T>;
                UnsignedT abs_num;

                if (negative) {
                    abs_num = UnsignedT(-(std::make_signed_t<T>(num)));
                }
                else {
                    abs_num = UnsignedT(num);
                }

                while (abs_num > 0) {
                    result[idx++] = '0' + char(abs_num % 10);
                    abs_num /= 10;
                }

                if (negative) {
                    result[idx++] = '-';
                }

                for (int i = 0; i < idx / 2; ++i) {
                    char temp = result[i];
                    result[i] = result[idx - 1 - i];
                    result[idx - 1 - i] = temp;
                }
            }

            result[idx] = '\0';
            return CompileString<32>(result);
        }

        template <size_t M>
        constexpr auto operator+(const CompileString<M>& other) const noexcept {
            constexpr size_t new_size = N + M - 1;
            char result[new_size] = {};

            size_t pos = 0;
            while (pos < N - 1 && value[pos] != '\0') {
                result[pos] = value[pos];
                ++pos;
            }

            size_t j = 0;
            while (j < M) {
                result[pos++] = other.value[j++];
            }

            return CompileString<new_size>(result);
        }

        template <size_t M>
        constexpr bool operator==(const CompileString<M>& other) const noexcept {
            size_t i = 0;
            while (i < N && i < M && value[i] != '\0' && other.value[i] != '\0') {
                if (value[i] != other.value[i]) {
                    return false;
                }
                ++i;
            }
            return value[i] == other.value[i];
        }

        constexpr bool operator==(const char* other) const noexcept {
            size_t i = 0;
            while (i < N && value[i] != '\0' && other[i] != '\0') {
                if (value[i] != other[i]) {
                    return false;
                }
                ++i;
            }
            return value[i] == other[i];
        }

        void print() const {
            for (size_t i = 0; i < N && value[i] != '\0'; ++i) {
                std::cout << value[i];
            }
        }
    };

    // Forward declaration of TypeSignature
    template <typename T>
    struct TypeSignature;

    // Helper function to calculate field offsets
    template<typename T, size_t Index>
    constexpr size_t get_field_offset() noexcept {
        if constexpr (Index == 0) {
            return 0;
        } else {
            using PrevType = std::tuple_element_t<Index - 1, decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;
            using CurrType = std::tuple_element_t<Index, decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;
            
            constexpr size_t prev_offset = get_field_offset<T, Index - 1>();
            constexpr size_t prev_size = sizeof(PrevType);
            constexpr size_t curr_align = alignof(CurrType);
            
            return (prev_offset + prev_size + (curr_align - 1)) & ~(curr_align - 1);
        }
    }

    // Generic get_fields_signature function
    template <typename T, size_t Index = 0>
    constexpr auto get_fields_signature() noexcept {
        if constexpr (Index >= boost::pfr::tuple_size_v<T>) {
            return CompileString{""};
        }
        else {
            using FieldType = std::tuple_element_t<Index, decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;
            
            if constexpr (Index == 0) {
                return CompileString{"@"} +
                       CompileString<32>::from_number(get_field_offset<T, Index>()) +
                       CompileString{":"} +
                       TypeSignature<FieldType>::calculate() +
                       get_fields_signature<T, Index + 1>();
            }
            else {
                return CompileString{",@"} +
                       CompileString<32>::from_number(get_field_offset<T, Index>()) +
                       CompileString{":"} +
                       TypeSignature<FieldType>::calculate() +
                       get_fields_signature<T, Index + 1>();
            }
        }
    }

    // Specializations for basic types
    template <>
    struct TypeSignature<int32_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"i32[s:4,a:4]"};
        }
    };

    template <>
    struct TypeSignature<float> {
        static constexpr auto calculate() noexcept {
            return CompileString{"f32[s:4,a:4]"};
        }
    };

    template <>
    struct TypeSignature<double> {
        static constexpr auto calculate() noexcept {
            return CompileString{"f64[s:8,a:8]"};
        }
    };

    template <>
    struct TypeSignature<int64_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"i64[s:8,a:8]"};
        }
    };

    template <>
    struct TypeSignature<uint32_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"u32[s:4,a:4]"};
        }
    };

    template <>
    struct TypeSignature<uint64_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"u64[s:8,a:8]"};
        }
    };

    template <>
    struct TypeSignature<bool> {
        static constexpr auto calculate() noexcept {
            return CompileString{"bool[s:1,a:1]"};
        }
    };

    template <>
    struct TypeSignature<char> {
        static constexpr auto calculate() noexcept {
            return CompileString{"char[s:1,a:1]"};
        }
    };

    // Only add long long if it's different from int64_t
    #if !defined(__APPLE__) || !defined(__LP64__)
    template <>
    struct TypeSignature<long long> {
        static constexpr auto calculate() noexcept {
            return CompileString{"i64[s:8,a:8]"};
        }
    };
    #endif

    // Specialization for pointer types
    template <typename T>
    struct TypeSignature<T*> {
        static constexpr auto calculate() noexcept {
            return CompileString{"ptr[s:8,a:8]"};
        }
    };

    // Specialization for array types
    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static constexpr auto calculate() noexcept {
            if constexpr (std::is_same_v<T, char>) {
                return CompileString{"bytes[s:"} +
                       CompileString<32>::from_number(N) +
                       CompileString{",a:1]"};
            } else {
                return CompileString{"array[s:"} +
                       CompileString<32>::from_number(sizeof(T[N])) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T[N])) +
                       CompileString{"]<"} +
                       TypeSignature<T>::calculate() +
                       CompileString{","} +
                       CompileString<32>::from_number(N) +
                       CompileString{">"};
            }
        }
    };

    // Specialization for void*
    template <>
    struct TypeSignature<void*> {
        static constexpr auto calculate() noexcept {
            return CompileString{"ptr[s:8,a:8]"};
        }
    };

    // Specialization for char[ANY_SIZE]
    template <>
    struct TypeSignature<char[ANY_SIZE]> {
        static constexpr auto calculate() noexcept {
            return CompileString{"bytes[s:64,a:1]"};
        }
    };

    // Generic TypeSignature implementation
    template <typename T>
    struct TypeSignature {
        static constexpr auto calculate() noexcept {
            if constexpr (std::is_aggregate_v<T> && !std::is_array_v<T>) {
                return CompileString{"struct[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} +
                       get_fields_signature<T>() +
                       CompileString{"}"};
            }
            else if constexpr (std::is_pointer_v<T>) {
                return TypeSignature<void*>::calculate();
            }
            else if constexpr (std::is_array_v<T>) {
                return TypeSignature<std::remove_extent_t<T>[]>::calculate();
            }
            else {
                static_assert(always_false<T>::value, 
                    "Type is not supported for automatic reflection");
                return CompileString{""};
            }
        }
    };

    template <typename T>
    [[nodiscard]] constexpr auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }

} // namespace XTypeSignature