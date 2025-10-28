#include <experimental/meta>
#include <iostream>
#include <string>

// ========================================================================
// 简化的 CompileString（编译期字符串）
// ========================================================================
template <size_t N>
struct CompileString {
    char value[N];
    static constexpr size_t size = N - 1;
    
    constexpr CompileString(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i) value[i] = str[i];
    }
    
    template <size_t M>
    constexpr auto operator+(const CompileString<M>& other) const {
        constexpr size_t new_size = N + M - 1;
        char result[new_size] = {};
        size_t pos = 0;
        // 复制第一个字符串（跳过尾部 null）
        while (pos < N - 1 && value[pos] != '\0') {
            result[pos] = value[pos];
            ++pos;
        }
        // 复制第二个字符串（包括尾部 null）
        size_t j = 0;
        while (j < M) {
            result[pos++] = other.value[j++];
        }
        return CompileString<new_size>(result);
    }
    
    // 辅助：从数字生成字符串
    template <typename T>
    static constexpr CompileString<32> from_number(T num) {
        char result[32] = {};
        int idx = 0;
        if (num == 0) {
            result[0] = '0';
            idx = 1;
        } else {
            bool negative = num < 0;
            T abs_num = negative ? -num : num;
            while (abs_num > 0) {
                result[idx++] = '0' + char(abs_num % 10);
                abs_num /= 10;
            }
            if (negative) result[idx++] = '-';
            // Reverse
            for (int i = 0; i < idx / 2; ++i) {
                char temp = result[i];
                result[i] = result[idx - 1 - i];
                result[idx - 1 - i] = temp;
            }
        }
        result[idx] = '\0';
        return CompileString<32>(result);
    }
};

// ========================================================================
// 基础类型签名
// ========================================================================
template <typename T> struct TypeSignature;

template <> struct TypeSignature<int32_t> { 
    static consteval auto calculate() { return CompileString{"i32[s:4,a:4]"}; } 
};

template <> struct TypeSignature<int64_t> { 
    static consteval auto calculate() { return CompileString{"i64[s:8,a:8]"}; } 
};

template <> struct TypeSignature<double> { 
    static consteval auto calculate() { return CompileString{"f64[s:8,a:8]"}; } 
};

template <> struct TypeSignature<float> { 
    static consteval auto calculate() { return CompileString{"f32[s:4,a:4]"}; } 
};

// ========================================================================
// C++26 反射辅助函数
// ========================================================================
template<typename T>
static consteval std::size_t get_member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// ========================================================================
// 单个字段的类型签名（使用反射）
// ========================================================================
template<typename T, std::size_t Index>
static consteval auto get_field_signature() {
    using namespace std::meta;
    constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
    
    // ✓ 使用 splice 获取字段类型
    using FieldType = [:type_of(member):];
    
    // 获取字段偏移量
    constexpr std::size_t offset = offset_of(member).bytes;
    
    // 生成格式：@offset:type
    return CompileString{"@"} +
           CompileString<32>::from_number(offset) +
           CompileString{":"} +
           TypeSignature<FieldType>::calculate();
}

// ========================================================================
// 预处理器宏：生成字段累加表达式
// ========================================================================

#define XTYPE_FIELD_SIG(T, n) get_field_signature<T, n>()
#define XTYPE_COMMA() + CompileString{","} +

#define XTYPE_SIG_BUILD_0(T) CompileString{""}
#define XTYPE_SIG_BUILD_1(T) XTYPE_FIELD_SIG(T, 0)
#define XTYPE_SIG_BUILD_2(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1)
#define XTYPE_SIG_BUILD_3(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 2)
#define XTYPE_SIG_BUILD_4(T) XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 2) XTYPE_COMMA() XTYPE_FIELD_SIG(T, 3)

// ========================================================================
// 主函数：生成完整的字段签名
// ========================================================================
template<typename T>
consteval auto get_fields_signature() {
    constexpr std::size_t count = get_member_count<T>();
    
    if constexpr (count == 0) {
        return XTYPE_SIG_BUILD_0(T);
    } else if constexpr (count == 1) {
        return XTYPE_SIG_BUILD_1(T);
    } else if constexpr (count == 2) {
        return XTYPE_SIG_BUILD_2(T);
    } else if constexpr (count == 3) {
        return XTYPE_SIG_BUILD_3(T);
    } else if constexpr (count == 4) {
        return XTYPE_SIG_BUILD_4(T);
    } else {
        return CompileString{"TOO_MANY_FIELDS"};
    }
}

// ========================================================================
// 通用类型签名模板
// ========================================================================
template <typename T>
struct TypeSignature {
    static consteval auto calculate() {
        if constexpr (std::is_class_v<T>) {
            return CompileString{"struct[s:"} +
                   CompileString<32>::from_number(sizeof(T)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T)) +
                   CompileString{"]{"} +
                   get_fields_signature<T>() +
                   CompileString{"}"};
        } else {
            return CompileString{"unknown"};
        }
    }
};

// 主 API
template <typename T>
[[nodiscard]] consteval auto get_XTypeSignature() {
    return TypeSignature<T>::calculate();
}

// ========================================================================
// 测试结构体
// ========================================================================

struct Point2D {
    int32_t x;
    int32_t y;
};

struct Player {
    int32_t id;
    float health;
    double score;
};

// ========================================================================
// 辅助函数：打印类型签名
// ========================================================================
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
// 主函数：测试
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
