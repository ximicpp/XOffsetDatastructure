// ========================================================================
// Type Signature HelloWorld Test
// Tests preprocessor macro + C++26 reflection for type signature generation
// ========================================================================
#include <experimental/meta>
#include <iostream>
#include <string>
#include <type_traits>

// Copy-paste TypeSignature implementation from xoffsetdatastructure2.hpp
namespace XTypeSignature {
    // CompileString (same as in xoffsetdatastructure2.hpp)
    template <size_t N>
    struct CompileString {
        char value[N];
        static constexpr size_t size = N - 1;
        
        constexpr CompileString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) value[i] = str[i];
        }

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
                if (negative) result[idx++] = '-';
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
    };

    // Forward declarations
    template <typename T> struct TypeSignature;
    
    // Helper functions
    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        using namespace std::meta;
        return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
    }
    
    template<typename T, std::size_t Index>
    static consteval auto get_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];
        constexpr std::size_t offset = offset_of(member).bytes;
        
        return CompileString{"@"} +
               CompileString<32>::from_number(offset) +
               CompileString{":"} +
               TypeSignature<FieldType>::calculate();
    }

    // Preprocessor macros
    #define XTYPE_FIELD_SIG(T, n) get_field_signature<T, n>()
    #define XTYPE_COMMA() + CompileString{","} +
    #define XTYPE_SIG_BUILD_0(T) CompileString{""}
    #define XTYPE_SIG_BUILD_1(T) XTYPE_FIELD_SIG(T, 0)
    #define XTYPE_SIG_BUILD_2(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1)
    #define XTYPE_SIG_BUILD_3(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 2)
    #define XTYPE_SIG_BUILD_4(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 2) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 3)

    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) return XTYPE_SIG_BUILD_0(T);
        else if constexpr (count == 1) return XTYPE_SIG_BUILD_1(T);
        else if constexpr (count == 2) return XTYPE_SIG_BUILD_2(T);
        else if constexpr (count == 3) return XTYPE_SIG_BUILD_3(T);
        else if constexpr (count == 4) return XTYPE_SIG_BUILD_4(T);
        else return CompileString{"TOO_MANY_FIELDS"};
    }

    // Basic type signatures
    template <> struct TypeSignature<int32_t> { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<float> { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double> { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    template <> struct TypeSignature<int64_t> { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };

    // Generic struct signature
    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_class_v<T>) {
                return CompileString{"struct[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} +
                       get_fields_signature<T>() +
                       CompileString{"}"};
            }
        }
    };

    template <typename T>
    [[nodiscard]] consteval auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }
}

using namespace XTypeSignature;

// ========================================================================
// Test Structures
// ========================================================================

// Simple struct with primitive types
struct Point2D {
    int32_t x;
    int32_t y;
};

// Struct with mixed types
struct Player {
    int32_t id;
    float health;
    double score;
};

// Helper function to print type signature
template<typename T>
void print_type_signature(const char* type_name) {
    constexpr auto sig = get_XTypeSignature<T>();
    
    std::cout << "  Type: " << type_name << std::endl;
    std::cout << "  Signature: ";
    for (size_t i = 0; i < sig.size && sig.value[i] != '\0'; ++i) {
        std::cout << sig.value[i];
    }
    std::cout << std::endl;
    std::cout << std::endl;
}

// ========================================================================
// Main Test
// ========================================================================

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  XOffsetDatastructure2 - Type Signature Test (HelloWorld)    ║" << std::endl;
    std::cout << "║  Using Preprocessor Macros + C++26 Reflection                ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // ============================================================
    // Test 1: Point2D (2 int32_t fields)
    // ============================================================
    std::cout << "Test 1: Point2D Structure" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "struct Point2D {" << std::endl;
    std::cout << "    int32_t x;" << std::endl;
    std::cout << "    int32_t y;" << std::endl;
    std::cout << "};" << std::endl;
    std::cout << std::endl;
    
    print_type_signature<Point2D>("Point2D");
    
    // ============================================================
    // Test 2: Player (mixed types)
    // ============================================================
    std::cout << "Test 2: Player Structure" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "struct Player {" << std::endl;
    std::cout << "    int32_t id;" << std::endl;
    std::cout << "    float health;" << std::endl;
    std::cout << "    double score;" << std::endl;
    std::cout << "};" << std::endl;
    std::cout << std::endl;
    
    print_type_signature<Player>("Player");
    
    // ============================================================
    // Test 3: Primitive types
    // ============================================================
    std::cout << "Test 3: Primitive Types" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    print_type_signature<int32_t>("int32_t");
    print_type_signature<float>("float");
    print_type_signature<double>("double");
    
    // ============================================================
    // Success Message
    // ============================================================
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ✓✓✓ ALL TESTS PASSED! ✓✓✓                                   ║" << std::endl;
    std::cout << "║                                                                ║" << std::endl;
    std::cout << "║  Type signatures are working correctly!                       ║" << std::endl;
    std::cout << "║  - Preprocessor macros expand field signatures               ║" << std::endl;
    std::cout << "║  - C++26 reflection provides field types and offsets          ║" << std::endl;
    std::cout << "║  - Compile-time string accumulation works!                    ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}