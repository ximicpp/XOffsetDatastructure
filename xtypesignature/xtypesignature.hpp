#pragma once
#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/string.hpp>
#include <boost/any/basic_any.hpp>
#include <boost/pfr.hpp>
#include <string_view>
#include <type_traits>
#include <iostream>

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

    template <typename T>
    struct TypeSignature;

    template <>
    struct TypeSignature<int32_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"i32[s:"} +
                   CompileString<32>::from_number(sizeof(int32_t)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(int32_t)) +
                   CompileString{"]"};
        }
    };

    template <>
    struct TypeSignature<uint32_t> {
        static constexpr auto calculate() noexcept {
            return CompileString{"u32[s:"} +
                   CompileString<32>::from_number(sizeof(uint32_t)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(uint32_t)) +
                   CompileString{"]"};
        }
    };

    template <>
    struct TypeSignature<float> {
        static constexpr auto calculate() noexcept {
            return CompileString{"f32[s:"} +
                   CompileString<32>::from_number(sizeof(float)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(float)) +
                   CompileString{"]"};
        }
    };

    template <>
    struct TypeSignature<double> {
        static constexpr auto calculate() noexcept {
            return CompileString{"f64[s:"} +
                   CompileString<32>::from_number(sizeof(double)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(double)) +
                   CompileString{"]"};
        }
    };

    template <size_t N>
    struct TypeSignature<char[N]> {
        static constexpr auto calculate() noexcept {
            return CompileString{"bytes[s:"} +
                   CompileString<32>::from_number(N) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(char[N])) +
                   CompileString{"]"};
        }
    };

    template <typename T>
    struct TypeSignature<T*> {
        static constexpr auto calculate() noexcept {
            return CompileString{"ptr[s:"} +
                   CompileString<32>::from_number(sizeof(T*)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T*)) +
                   CompileString{"]"};
        }
    };

    template <>
    struct TypeSignature<XString> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(XString)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XString)) +
                   CompileString{"]"};
        }
    };

    template <typename T>
    struct TypeSignature<XVector<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"vector[s:"} +
                   CompileString<32>::from_number(sizeof(XVector<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XVector<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    template <typename K, typename V>
    struct TypeSignature<XMap<K, V>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"map[s:"} +
                   CompileString<32>::from_number(sizeof(XMap<K, V>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XMap<K, V>)) +
                   CompileString{"]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
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

    template <typename T, size_t I>
    struct FieldOffsetHelper {
        static constexpr size_t calculate() noexcept {
            if constexpr (I == 0) {
                return 0;
            }
            else {
                using PrevType = std::tuple_element_t<I - 1, decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;
                using CurrType = std::tuple_element_t<I, decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;

                constexpr size_t prev_offset = FieldOffsetHelper<T, I - 1>::calculate();
                constexpr size_t prev_size = sizeof(PrevType);
                constexpr size_t curr_align = alignof(CurrType);

                return (prev_offset + prev_size + (curr_align - 1)) & ~(curr_align - 1);
            }
        }
    };

    template <typename Struct, size_t Index = 0>
    constexpr auto get_fields_signature() noexcept {
        if constexpr (Index >= boost::pfr::tuple_size_v<Struct>) {
            return CompileString{""};
        }
        else {
            using FieldType = std::remove_cvref_t<decltype(boost::pfr::get<Index>(std::declval<Struct>()))>;
            constexpr size_t offset = FieldOffsetHelper<Struct, Index>::calculate();

            auto field_sig = CompileString{"@"} +
                            CompileString<32>::from_number(offset) +
                            CompileString{":"} +
                            TypeSignature<FieldType>::calculate();

            if constexpr (Index == 0) {
                return field_sig + get_fields_signature<Struct, Index + 1>();
            }
            else {
                return CompileString{","} + field_sig + get_fields_signature<Struct, Index + 1>();
            }
        }
    }

    template <typename T>
    struct TypeSignature {
        static constexpr auto calculate() noexcept {
            if constexpr (std::is_class_v<T>) {
                return CompileString{"struct[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} +
                       get_fields_signature<T>() +
                       CompileString{"}"};
            }
            else {
                static_assert(always_false<T>::value, "Unsupported type");
                return CompileString{""};
            }
        }
    };

    template <typename T>
    [[nodiscard]] constexpr auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }

} // namespace XTypeSignature