// typelayout.hpp
// Compile-time layout signature generator using C++26 static reflection (P2996)
//
// Constraints: 64-bit, little-endian, IEEE 754 floats
// Guarantees: identical signature => identical memory layout

#ifndef TYPELAYOUT_HPP
#define TYPELAYOUT_HPP

// Platform detection
#if defined(_MSC_VER)
    #define TYPELAYOUT_PLATFORM_WINDOWS 1
    #define TYPELAYOUT_LITTLE_ENDIAN 1
#elif defined(__clang__) || defined(__GNUC__)
    #define TYPELAYOUT_PLATFORM_WINDOWS 0
    #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
    #error "Unsupported compiler"
#endif

// Architecture detection
#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    #define TYPELAYOUT_ARCH_64BIT 1
#else
    #define TYPELAYOUT_ARCH_64BIT 0
#endif

// Endianness detection fallback
#ifndef TYPELAYOUT_LITTLE_ENDIAN
    #if defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        #define TYPELAYOUT_LITTLE_ENDIAN 1
    #else
        #define TYPELAYOUT_LITTLE_ENDIAN 0
    #endif
#endif

// Platform requirement checks
#ifndef TYPELAYOUT_DISABLE_PLATFORM_CHECKS
    #if !TYPELAYOUT_ARCH_64BIT
        #error "typelayout requires 64-bit architecture"
    #endif
    #if !TYPELAYOUT_LITTLE_ENDIAN
        #error "typelayout requires little-endian architecture"
    #endif
#endif

#include <experimental/meta>
#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>

namespace typelayout {

    // Type size/alignment requirements
    static_assert(sizeof(int8_t) == 1,   "int8_t must be 1 byte");
    static_assert(sizeof(uint8_t) == 1,  "uint8_t must be 1 byte");
    static_assert(sizeof(int16_t) == 2,  "int16_t must be 2 bytes");
    static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
    static_assert(sizeof(int32_t) == 4,  "int32_t must be 4 bytes");
    static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
    static_assert(sizeof(int64_t) == 8,  "int64_t must be 8 bytes");
    static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
    static_assert(sizeof(float) == 4,    "float must be 4 bytes");
    static_assert(sizeof(double) == 8,   "double must be 8 bytes");
    static_assert(sizeof(char) == 1,     "char must be 1 byte");
    static_assert(sizeof(bool) == 1,     "bool must be 1 byte");
    static_assert(sizeof(void*) == 8,    "Pointer size must be 8 bytes (64-bit required)");
    static_assert(alignof(void*) == 8,   "Pointer alignment must be 8 bytes");
    static_assert(sizeof(size_t) == 8,   "size_t must be 8 bytes");

    // IEEE 754 floating-point verification
    #ifdef __STDC_IEC_559__
        static_assert(__STDC_IEC_559__, "IEEE 754 required");
    #else
        static_assert(std::numeric_limits<float>::is_iec559 || 
                      (std::numeric_limits<float>::digits == 24 && 
                       std::numeric_limits<float>::max_exponent == 128),
                      "float must be IEEE 754");
        static_assert(std::numeric_limits<double>::is_iec559 ||
                      (std::numeric_limits<double>::digits == 53 && 
                       std::numeric_limits<double>::max_exponent == 1024),
                      "double must be IEEE 754");
    #endif

    // Platform-dependent type detection
    template <typename T>
    struct is_platform_dependent : std::false_type {};
    
    // wchar_t: 2 bytes (Windows) vs 4 bytes (Linux)
    template <> struct is_platform_dependent<wchar_t> : std::true_type {};
    // long double: 8/12/16 bytes depending on platform
    template <> struct is_platform_dependent<long double> : std::true_type {};
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows LLP64: long is 4 bytes, int64_t is 8 bytes (long long)
    template <> struct is_platform_dependent<long> : std::true_type {};
    template <> struct is_platform_dependent<unsigned long> : std::true_type {};
#endif
    // Linux LP64: long = int64_t, cannot distinguish, so don't flag
    
    // CV-qualified variants
    template <typename T> struct is_platform_dependent<const T> : is_platform_dependent<T> {};
    template <typename T> struct is_platform_dependent<volatile T> : is_platform_dependent<T> {};
    template <typename T> struct is_platform_dependent<const volatile T> : is_platform_dependent<T> {};
    
    // Arrays of platform-dependent types
    template <typename T, std::size_t N> 
    struct is_platform_dependent<T[N]> : is_platform_dependent<T> {};
    
    // Helper variable template
    template <typename T>
    inline constexpr bool is_platform_dependent_v = is_platform_dependent<T>::value;

    // Compile-time string
    template <size_t N>
    struct CompileString {
        char value[N];
        static constexpr size_t size = N - 1;
        
        constexpr CompileString() : value{} {}
        
        constexpr CompileString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) {
                value[i] = str[i];
            }
        }

        constexpr CompileString(std::string_view sv) {
            for (size_t i = 0; i < N - 1 && i < sv.size(); ++i) {
                value[i] = sv[i];
            }
            value[N - 1] = '\0';
        }

        // Convert number to compile-time string
        template <typename T>
        static constexpr CompileString<32> from_number(T num) noexcept {
            char result[32] = {};
            int idx = 0;
            
            if (num == 0) {
                result[0] = '0';
                idx = 1;
            } else {
                bool negative = std::is_signed_v<T> && num < 0;
                using UnsignedT = std::make_unsigned_t<T>;
                UnsignedT abs_num;
                
                if (negative) {
                    abs_num = UnsignedT(-(std::make_signed_t<T>(num)));
                } else {
                    abs_num = UnsignedT(num);
                }
                
                while (abs_num > 0) {
                    result[idx++] = '0' + char(abs_num % 10);
                    abs_num /= 10;
                }
                
                if (negative) {
                    result[idx++] = '-';
                }
                
                // Reverse the string
                for (int i = 0; i < idx / 2; ++i) {
                    char temp = result[i];
                    result[i] = result[idx - 1 - i];
                    result[idx - 1 - i] = temp;
                }
            }
            result[idx] = '\0';
            return CompileString<32>(result);
        }

        // String concatenation
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

        // Equality comparison with another CompileString
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

        // Equality comparison with C-string
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

        // Get C-string view
        constexpr const char* c_str() const noexcept {
            return value;
        }

        // Get actual length (excluding null terminator)
        constexpr size_t length() const noexcept {
            size_t len = 0;
            while (len < N && value[len] != '\0') {
                ++len;
            }
            return len;
        }
    };

    template <typename T>
    struct always_false : std::false_type {};

    template <typename T>
    struct TypeSignature;

    // Reflection helpers
    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        using namespace std::meta;
        auto all_members = nonstatic_data_members_of(^^T, access_context::unchecked());
        return all_members.size();
    }

    // Get base class count of a type
    template <typename T>
    consteval std::size_t get_base_count() noexcept {
        using namespace std::meta;
        auto bases = bases_of(^^T, access_context::unchecked());
        return bases.size();
    }

    // Check if a type has base classes
    template <typename T>
    consteval bool has_bases() noexcept {
        return get_base_count<T>() > 0;
    }

    // Get field offset at compile time
    template<typename T, size_t Index>
    consteval size_t get_field_offset() noexcept {
        using namespace std::meta;
        auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
        return offset_of(members[Index]).bytes;
    }

    // Build signature for a single field (includes field name for readability)
    // Supports bit-fields with bit offset and width
    template<typename T, std::size_t Index>
    static consteval auto get_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        
        using FieldType = [:type_of(member):];
        constexpr std::string_view name = identifier_of(member);
        constexpr size_t NameLen = name.size() + 1;

        // Check if this is a bit-field
        if constexpr (is_bit_field(member)) {
            // Bit-field: use bit offset and bit width
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_offset = bit_off.bytes;
            constexpr std::size_t bit_offset = bit_off.bits;
            constexpr std::size_t bit_width = bit_size_of(member);
            
            // Format: @byte.bit[name]:bits<width,underlying_type>
            return CompileString{"@"} +
                   CompileString<32>::from_number(byte_offset) +
                   CompileString{"."} +
                   CompileString<32>::from_number(bit_offset) +
                   CompileString{"["} +
                   CompileString<NameLen>(name) +
                   CompileString{"]:bits<"} +
                   CompileString<32>::from_number(bit_width) +
                   CompileString{","} +
                   TypeSignature<FieldType>::calculate() +
                   CompileString{">"};
        } else {
            // Regular field: use byte offset
            constexpr std::size_t offset = offset_of(member).bytes;
            
            return CompileString{"@"} +
                   CompileString<32>::from_number(offset) +
                   CompileString{"["} +
                   CompileString<NameLen>(name) +
                   CompileString{"]:"} +
                   TypeSignature<FieldType>::calculate();
        }
    }

    // Build field signature with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto build_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_field_signature<T, Index>();
        } else {
            return CompileString{","} + get_field_signature<T, Index>();
        }
    }

    // Concatenate all field signatures
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_field_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    // Get all fields signature for a struct/class
    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_field_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    // Base class signature
    template<typename T, std::size_t Index>
    static consteval auto get_base_signature() noexcept {
        using namespace std::meta;
        // Use constexpr with direct indexing to ensure constant expression
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        
        using BaseType = [:type_of(base_info):];
        constexpr std::size_t base_offset = offset_of(base_info).bytes;
        constexpr bool is_virtual_base = is_virtual(base_info);

        if constexpr (is_virtual_base) {
            // Virtual inheritance: offset is typically via vbptr, mark as virtual
            return CompileString{"@"} +
                   CompileString<32>::from_number(base_offset) +
                   CompileString{"[vbase]:"} +
                   TypeSignature<BaseType>::calculate();
        } else {
            // Regular inheritance
            return CompileString{"@"} +
                   CompileString<32>::from_number(base_offset) +
                   CompileString{"[base]:"} +
                   TypeSignature<BaseType>::calculate();
        }
    }

    // Build base signature with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto build_base_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_base_signature<T, Index>();
        } else {
            return CompileString{","} + get_base_signature<T, Index>();
        }
    }

    // Concatenate all base signatures
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_base_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_base_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    // Get all bases signature for a class
    template <typename T>
    consteval auto get_bases_signature() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_base_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    // Get combined bases and fields signature
    template <typename T>
    consteval auto get_layout_content_signature() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();
        
        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return get_fields_signature<T>();
        } else if constexpr (field_count == 0) {
            return get_bases_signature<T>();
        } else {
            // Has both bases and fields
            return get_bases_signature<T>() + CompileString{","} + get_fields_signature<T>();
        }
    }

    // Fixed-width integer types
    
    // 8-bit types
    template <> struct TypeSignature<int8_t>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <> struct TypeSignature<uint8_t>  { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    
    // 16-bit types
    template <> struct TypeSignature<int16_t>  { static consteval auto calculate() noexcept { return CompileString{"i16[s:2,a:2]"}; } };
    template <> struct TypeSignature<uint16_t> { static consteval auto calculate() noexcept { return CompileString{"u16[s:2,a:2]"}; } };
    
    // 32-bit types
    template <> struct TypeSignature<int32_t>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    
    // 64-bit types
    template <> struct TypeSignature<int64_t>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };

    // Standard integer types (conditional to avoid redefinition)
    
    // signed char / unsigned char - only define if not same as int8_t/uint8_t
    #if !defined(__GNUC__) && !defined(__clang__)
    template <> struct TypeSignature<signed char>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <> struct TypeSignature<unsigned char> { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    #endif
    
    // On Windows LLP64, long is 4 bytes (different from int64_t which is long long)
    // On Linux LP64, long is 8 bytes and same as int64_t
    #if defined(_WIN32) || defined(_WIN64)
    // Windows: long is 4 bytes, separate from int32_t (which is also 4 bytes but different type)
    template <> struct TypeSignature<long> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"i32[s:4,a:4]"};  // LLP64 (Windows)
        } 
    };
    template <> struct TypeSignature<unsigned long> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"u32[s:4,a:4]"};  // LLP64 (Windows)
        } 
    };
    #endif
    
    // long long - define only on Linux where it's different from int64_t (which is long)
    #if !defined(_WIN32) && !defined(_WIN64)
    template <> struct TypeSignature<long long>          { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<unsigned long long> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };
    #endif

    // Floating point types
    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    // long double - platform dependent, typically 8, 12, or 16 bytes
    template <> struct TypeSignature<long double> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"f80[s:"} +
                   CompileString<32>::from_number(sizeof(long double)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(long double)) +
                   CompileString{"]"};
        } 
    };
    
    // Character types
    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    template <> struct TypeSignature<wchar_t>  { 
        static consteval auto calculate() noexcept { 
            // wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
            return CompileString{"wchar[s:"} +
                   CompileString<32>::from_number(sizeof(wchar_t)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(wchar_t)) +
                   CompileString{"]"};
        } 
    };
    template <> struct TypeSignature<char8_t>  { static consteval auto calculate() noexcept { return CompileString{"char8[s:1,a:1]"}; } };
    template <> struct TypeSignature<char16_t> { static consteval auto calculate() noexcept { return CompileString{"char16[s:2,a:2]"}; } };
    template <> struct TypeSignature<char32_t> { static consteval auto calculate() noexcept { return CompileString{"char32[s:4,a:4]"}; } };
    
    // Boolean type
    template <> struct TypeSignature<bool>     { static consteval auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    
    // nullptr_t
    template <> struct TypeSignature<std::nullptr_t> { static consteval auto calculate() noexcept { return CompileString{"nullptr[s:8,a:8]"}; } };

    // std::byte
    template <> struct TypeSignature<std::byte> { static consteval auto calculate() noexcept { return CompileString{"byte[s:1,a:1]"}; } };

    // Function pointer types
    
    // Generic function pointer (R(*)(Args...))
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...)> {
        static consteval auto calculate() noexcept {
            return CompileString{"fnptr[s:8,a:8]"};
        }
    };
    
    // Noexcept function pointer (R(*)(Args...) noexcept)
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...) noexcept> {
        static consteval auto calculate() noexcept {
            return CompileString{"fnptr[s:8,a:8]"};
        }
    };
    
    // C-style variadic function pointer (R(*)(Args..., ...))
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...)> {
        static consteval auto calculate() noexcept {
            return CompileString{"fnptr[s:8,a:8]"};
        }
    };

    // Const types - strip const and delegate
    template <typename T>
    struct TypeSignature<const T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // Volatile types - strip volatile and delegate
    template <typename T>
    struct TypeSignature<volatile T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // Const volatile types - strip both and delegate
    template <typename T>
    struct TypeSignature<const volatile T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // Pointer types - all pointers have same size/alignment on 64-bit
    template <typename T> 
    struct TypeSignature<T*> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"ptr[s:8,a:8]"}; 
        } 
    };
    
    template <> 
    struct TypeSignature<void*> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"ptr[s:8,a:8]"}; 
        } 
    };

    // Reference types - treated as pointer-like (same memory footprint)
    template <typename T> 
    struct TypeSignature<T&> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"ref[s:8,a:8]"}; 
        } 
    };
    
    // Rvalue reference types
    template <typename T> 
    struct TypeSignature<T&&> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"rref[s:8,a:8]"}; 
        } 
    };

    // Member pointer types - size varies by platform and class type
    // For data members: typically sizeof(void*) on 64-bit
    // For function members: typically 2*sizeof(void*) on 64-bit (can be larger for virtual)
    template <typename T, typename C>
    struct TypeSignature<T C::*> {
        static consteval auto calculate() noexcept {
            return CompileString{"memptr[s:"} +
                   CompileString<32>::from_number(sizeof(T C::*)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T C::*)) +
                   CompileString{"]"};
        }
    };

    // Array types
    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_same_v<T, char>) {
                // Character arrays treated as byte buffers
                return CompileString{"bytes[s:"} +
                       CompileString<32>::from_number(N) +
                       CompileString{",a:1]"};
            } else {
                // Regular arrays include element signature
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

    // Generic type signature (for structs/classes/enums/unions)
    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                // Enum type: include underlying type in signature
                using UnderlyingType = std::underlying_type_t<T>;
                return CompileString{"enum[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]<"} +
                       TypeSignature<UnderlyingType>::calculate() +
                       CompileString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                // Union type: show size and alignment, but cannot introspect members safely
                return CompileString{"union[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]"};
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                constexpr bool is_poly = std::is_polymorphic_v<T>;
                constexpr bool has_base = has_bases<T>();
                
                if constexpr (is_poly && has_base) {
                    // Polymorphic class with inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",polymorphic,inherited]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else if constexpr (is_poly) {
                    // Polymorphic class without inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",polymorphic]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else if constexpr (has_base) {
                    // Non-polymorphic class with inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",inherited]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else {
                    // Regular struct/class without inheritance
                    return CompileString{"struct[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{"]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                }
            }
            else if constexpr (std::is_pointer_v<T>) {
                return TypeSignature<void*>::calculate();
            }
            else if constexpr (std::is_array_v<T>) {
                return TypeSignature<std::remove_extent_t<T>[]>::calculate();
            }
            else {
                static_assert(always_false<T>::value, 
                    "Type is not supported for layout signature generation");
                return CompileString{""};
            }
        }
    };

    // Public API

    /**
     * @brief Get the compile-time layout signature for a type
     * @tparam T The type to generate signature for
     * @return A CompileString containing the layout signature
     */
    template <typename T>
    [[nodiscard]] consteval auto get_layout_signature() noexcept {
        return TypeSignature<T>::calculate();
    }

    /**
     * @brief Compare two layout signatures at compile time
     * @tparam T1 First type
     * @tparam T2 Second type
     * @return true if signatures match, false otherwise
     */
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool signatures_match() noexcept {
        return get_layout_signature<T1>() == get_layout_signature<T2>();
    }

    /**
     * @brief Get the layout signature as a C-string (for runtime use)
     * @tparam T The type to generate signature for
     * @return Pointer to null-terminated C-string
     */
    template <typename T>
    [[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
        static constexpr auto sig = get_layout_signature<T>();
        return sig.c_str();
    }

} // namespace typelayout

// Smart pointer specializations

namespace typelayout {

    // std::unique_ptr - treat as pointer
    template <typename T, typename Deleter>
    struct TypeSignature<std::unique_ptr<T, Deleter>> {
        static consteval auto calculate() noexcept {
            return CompileString{"unique_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::unique_ptr<T, Deleter>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::unique_ptr<T, Deleter>)) +
                   CompileString{"]"};
        }
    };

    // std::shared_ptr - treat as pointer
    template <typename T>
    struct TypeSignature<std::shared_ptr<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"shared_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::shared_ptr<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::shared_ptr<T>)) +
                   CompileString{"]"};
        }
    };

    // std::weak_ptr - treat as pointer
    template <typename T>
    struct TypeSignature<std::weak_ptr<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"weak_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::weak_ptr<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::weak_ptr<T>)) +
                   CompileString{"]"};
        }
    };

} // namespace typelayout

// Boost.Interprocess offset_ptr specialization (if header included)

#ifdef BOOST_INTERPROCESS_OFFSET_PTR_HPP

namespace typelayout {

    template <typename T, typename DifferenceType, typename OffsetType, std::size_t Alignment>
    struct TypeSignature<boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>> {
        static consteval auto calculate() noexcept {
            return CompileString{"offset_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>)) +
                   CompileString{"]"};
        }
    };

} // namespace typelayout

#endif // BOOST_INTERPROCESS_OFFSET_PTR_HPP

// Portability checking

namespace typelayout {

    // Forward declaration for recursive checking
    template <typename T>
    [[nodiscard]] consteval bool is_portable() noexcept;

    // Check if a single member type is portable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_member_portable() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];
        
        // Use is_portable for recursive checking of nested structs
        return is_portable<FieldType>();
    }

    // Check all members for portability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_members_portable(std::index_sequence<Indices...>) noexcept {
        return (check_member_portable<T, Indices>() && ...);
    }

    // Check if a single base class is portable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_base_portable() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];
        
        // Recursively check the base class
        return is_portable<BaseType>();
    }

    // Check all base classes for portability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_bases_portable(std::index_sequence<Indices...>) noexcept {
        return (check_base_portable<T, Indices>() && ...);
    }

    // Check if a type is portable (no platform-dependent members)
    // Recursively checks nested structs AND base classes
    template <typename T>
    [[nodiscard]] consteval bool is_portable() noexcept {
        // Strip CV qualifiers
        using CleanT = std::remove_cv_t<T>;
        
        // Check if it's a platform-dependent primitive
        if constexpr (is_platform_dependent_v<CleanT>) {
            return false;
        }
        // Check arrays recursively
        else if constexpr (std::is_array_v<CleanT>) {
            using ElementType = std::remove_extent_t<CleanT>;
            return is_portable<ElementType>();
        }
        // Unions: we cannot know which member is active at runtime, but
        // we CAN check all members for portability at compile time.
        // Conservative strategy: if ANY member is non-portable, the union
        // is considered non-portable (safer than missing a problem).
        else if constexpr (std::is_union_v<CleanT>) {
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_portable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // Check structs/classes recursively (including base classes)
        else if constexpr (std::is_class_v<CleanT>) {
            // First check all base classes
            constexpr std::size_t base_count = get_base_count<CleanT>();
            bool bases_portable = true;
            if constexpr (base_count > 0) {
                bases_portable = check_all_bases_portable<CleanT>(
                    std::make_index_sequence<base_count>{});
            }
            
            if (!bases_portable) {
                return false;
            }
            
            // Then check all direct members
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_portable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // All other types (primitives, pointers, etc.) are portable
        else {
            return true;
        }
    }

    // Helper variable template for is_portable
    template <typename T>
    inline constexpr bool is_portable_v = is_portable<T>();

    // Fixed string for NTTP (Non-Type Template Parameter)
    template<size_t N>
    struct fixed_string {
        char data[N];
        constexpr fixed_string(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) data[i] = str[i];
        }
        constexpr operator const char*() const { return data; }
    };
    template<size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;

    // Concepts

    /// Type contains no platform-dependent members
    template<typename T>
    concept Portable = is_portable<T>();

    /// Two types have compatible memory layouts
    template<typename T, typename U>
    concept LayoutCompatible = signatures_match<T, U>();

    /// Type layout matches expected signature string
    template<typename T, fixed_string ExpectedSig>
    concept LayoutMatch = (get_layout_signature<T>() == static_cast<const char*>(ExpectedSig));

} // namespace typelayout

/// Bind type to expected signature - fails if layout differs
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

#endif // TYPELAYOUT_HPP
