#include <experimental/meta>
#include <iostream>
#include <cstddef>

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
    static consteval auto calculate() { return CompileString{"i32"}; } 
};

template <> struct TypeSignature<int64_t> { 
    static consteval auto calculate() { return CompileString{"i64"}; } 
};

template <> struct TypeSignature<double> { 
    static consteval auto calculate() { return CompileString{"f64"}; } 
};

template <> struct TypeSignature<float> { 
    static consteval auto calculate() { return CompileString{"f32"}; } 
};

// ========================================================================
// C++26 反射辅助函数
// ========================================================================
template<typename T>
static consteval std::size_t get_member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

template<typename T, std::size_t Index>
static consteval auto get_member_at() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[Index];
}

// ========================================================================
// 单个字段的类型签名（使用索引序列技术）
// ========================================================================
template<typename T, std::size_t Index>
static consteval auto get_field_signature() {
    using namespace std::meta;
    constexpr auto member = get_member_at<T, Index>();
    
    // ✓ 使用 splice 获取字段类型
    using FieldType = [:type_of(member):];
    
    // 调试：先测试简单的
    constexpr auto offset_str = CompileString<32>::from_number(Index * 8);
    constexpr auto type_str = TypeSignature<FieldType>::calculate();
    
    // 生成格式：@offset:type
    return CompileString{"@"} +
           offset_str +
           CompileString{":"} +
           type_str;
}

// ========================================================================
// 预处理器宏：生成字段累加表达式
// ========================================================================

// 宏：生成单个字段的签名调用（带逗号分隔符）
#define XTYPE_FIELD_SIG(T, n) \
    get_field_signature<T, n>()

#define XTYPE_COMMA() + CompileString{","} +

// 手动定义不同数量字段的累加（0-10 个字段）
#define XTYPE_SIG_BUILD_0(T) \
    CompileString{""}

#define XTYPE_SIG_BUILD_1(T) \
    XTYPE_FIELD_SIG(T, 0)

#define XTYPE_SIG_BUILD_2(T) \
    XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 1)

#define XTYPE_SIG_BUILD_3(T) \
    XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 2)

#define XTYPE_SIG_BUILD_4(T) \
    XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 2) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 3)

#define XTYPE_SIG_BUILD_5(T) \
    XTYPE_FIELD_SIG(T, 0) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 1) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 2) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 3) XTYPE_COMMA() \
    XTYPE_FIELD_SIG(T, 4)

// ========================================================================
// 主函数：生成完整的字段签名
// ========================================================================
template<typename T>
consteval auto get_fields_signature() {
    constexpr std::size_t count = get_member_count<T>();
    
    // 使用 if constexpr 根据字段数量选择对应的宏
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
    } else if constexpr (count == 5) {
        return XTYPE_SIG_BUILD_5(T);
    } else {
        // 不支持超过 5 个字段（可扩展）
        return CompileString{"TOO_MANY_FIELDS"};
    }
}

// ========================================================================
// 测试结构体
// ========================================================================
struct TestStruct1 {
    int32_t x;
    double y;
};

struct TestStruct2 {
    int32_t a;
    int32_t b;
    float c;
    int64_t d;
};

// ========================================================================
// 主函数：测试
// ========================================================================
int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Testing: Preprocessor Macro + C++26 Reflection               ║" << std::endl;
    std::cout << "║  Purpose: Verify compile-time string accumulation works!      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    bool all_passed = true;
    
    // 测试 1: 2 个字段
    {
        std::cout << "Test 1: TestStruct1 (2 fields)" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        constexpr auto sig = get_fields_signature<TestStruct1>();
        
        std::string actual;
        for (size_t i = 0; i < sig.size && sig.value[i] != '\0'; ++i) {
            actual += sig.value[i];
        }
        
        std::string expected = "@0:i32,@8:f64";
        bool passed = (actual == expected);
        all_passed &= passed;
        
        std::cout << "  Expected: " << expected << std::endl;
        std::cout << "  Actual:   " << actual << std::endl;
        std::cout << "  Result:   " << (passed ? "✓ PASS" : "✗ FAIL") << std::endl;
        std::cout << std::endl;
    }
    
    // 测试 2: 4 个字段
    {
        std::cout << "Test 2: TestStruct2 (4 fields)" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        constexpr auto sig = get_fields_signature<TestStruct2>();
        
        std::string actual;
        for (size_t i = 0; i < sig.size && sig.value[i] != '\0'; ++i) {
            actual += sig.value[i];
        }
        
        std::string expected = "@0:i32,@8:i32,@16:f32,@24:i64";
        bool passed = (actual == expected);
        all_passed &= passed;
        
        std::cout << "  Expected: " << expected << std::endl;
        std::cout << "  Actual:   " << actual << std::endl;
        std::cout << "  Result:   " << (passed ? "✓ PASS" : "✗ FAIL") << std::endl;
        std::cout << std::endl;
    }
    
    // 测试 3: 成员数量检查
    {
        std::cout << "Test 3: Member count verification" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        constexpr std::size_t count1 = get_member_count<TestStruct1>();
        constexpr std::size_t count2 = get_member_count<TestStruct2>();
        
        bool passed = (count1 == 2 && count2 == 4);
        all_passed &= passed;
        
        std::cout << "  TestStruct1: " << count1 << " (expected: 2)" << std::endl;
        std::cout << "  TestStruct2: " << count2 << " (expected: 4)" << std::endl;
        std::cout << "  Result:      " << (passed ? "✓ PASS" : "✗ FAIL") << std::endl;
        std::cout << std::endl;
    }
    
    // 最终结果
    std::cout << "╔════════════════════════════════════════════════════════════════╗" << std::endl;
    if (all_passed) {
        std::cout << "║  ✓✓✓ ALL TESTS PASSED! ✓✓✓                                   ║" << std::endl;
        std::cout << "║                                                                ║" << std::endl;
        std::cout << "║  CONCLUSION: Preprocessor macros CAN be used to accumulate    ║" << std::endl;
        std::cout << "║  compile-time strings in C++26 reflection!                    ║" << std::endl;
    } else {
        std::cout << "║  ✗✗✗ SOME TESTS FAILED ✗✗✗                                   ║" << std::endl;
    }
    std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
    
    return all_passed ? 0 : 1;
}
