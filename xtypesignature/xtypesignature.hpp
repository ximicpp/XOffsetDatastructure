#pragma once
#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/string.hpp>
#include <boost/any/basic_any.hpp>
#include <boost/pfr.hpp>
#include <string_view>
#include <type_traits>
#include <iostream>
#include "xtypes.hpp"

// Platform detection
#if defined(_MSC_VER)
    #define TYPESIG_PLATFORM_WINDOWS 1
    #define IS_LITTLE_ENDIAN 1
#else
    #define TYPESIG_PLATFORM_WINDOWS 0
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define IS_LITTLE_ENDIAN 1
    #else
        #define IS_LITTLE_ENDIAN 0
    #endif
#endif

namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

    // Type aliases for the library
    using XString = boost::container::basic_string<char>;
    template <typename T>
    using XVector = boost::container::vector<T>;
    template <typename K, typename V>
    using XMap = boost::container::flat_map<K, V>;
	template <typename T>
	using XSet = boost::container::flat_set<T>;

    // Basic any type
    struct alignas(BASIC_ALIGNMENT) any_equivalent {
        void* man;
        char buf[ANY_SIZE];
    };

    using XAny = any_equivalent;
    using DynamicStruct = XMap<XString, XAny>;

    // Verify boost::any compatibility
    static_assert(sizeof(any_equivalent) == sizeof(boost::anys::basic_any<ANY_SIZE, BASIC_ALIGNMENT>), 
                 "Size mismatch with boost::any");
    static_assert(alignof(any_equivalent) == alignof(boost::anys::basic_any<ANY_SIZE, BASIC_ALIGNMENT>), 
                 "Alignment mismatch with boost::any");

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

        // 新添加的编译期比较操作符
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

    // 前向声明 TypeSignature
    template <typename T>
    struct TypeSignature;

    // 计算字段偏移量的辅助函数
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

    // 通用的 get_fields_signature 函数
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

    // 基本类型的特化必须在通用模板之前声明
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

    // 为指针类型添加特化
    template <typename T>
    struct TypeSignature<T*> {
        static constexpr auto calculate() noexcept {
            return CompileString{"ptr[s:8,a:8]"};
        }
    };

    // 为数组类型添加特化
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

    // 为 void* 添加特化
    template <>
    struct TypeSignature<void*> {
        static constexpr auto calculate() noexcept {
            return CompileString{"ptr[s:8,a:8]"};
        }
    };

    // 为 char[ANY_SIZE] 添加特化
    template <>
    struct TypeSignature<char[ANY_SIZE]> {
        static constexpr auto calculate() noexcept {
            return CompileString{"bytes[s:64,a:1]"};
        }
    };

    // 通用的 TypeSignature 实现
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
                // 添加类型名到错误消息中
                static_assert(always_false<T>::value, 
                    "Type is not supported for automatic reflection. Type name: " __FUNCSIG__);
                return CompileString{""};
            }
        }
    };

    template <>
    struct TypeSignature<any_equivalent> {
        static constexpr auto calculate() noexcept {
            return CompileString{"struct[s:"} +
                   CompileString<32>::from_number(sizeof(any_equivalent)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(any_equivalent)) +
                   CompileString{"]{"} +
                   CompileString{"@0:"} +
                   TypeSignature<void*>::calculate() +
                   CompileString{",@8:"} +
                   TypeSignature<char[ANY_SIZE]>::calculate() +
                   CompileString{"}"};
        }
    };

    // 为 XOffsetDatastructure 中的容器添加类型签名特化
    template <typename T>
    struct TypeSignature<XOffsetDatastructure::XVector<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"vector[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XVector<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XVector<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    template <>
    struct TypeSignature<XOffsetDatastructure::XString> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XString)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XString)) +
                   CompileString{"]"};
        }
    };

    template <typename K, typename V>
    struct TypeSignature<XOffsetDatastructure::XMap<K, V>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"map[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XMap<K, V>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XMap<K, V>)) +
                   CompileString{"]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
        }
    };

    template <typename T>
    struct TypeSignature<XOffsetDatastructure::XSet<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"set[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XSet<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XSet<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 boost::container::basic_string 添加特化
    template <>
    struct TypeSignature<boost::container::basic_string<char>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(boost::container::basic_string<char>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(boost::container::basic_string<char>)) +
                   CompileString{"]"};
        }
    };

    template <typename T>
    [[nodiscard]] constexpr auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }

} // namespace XTypeSignature
