#include <experimental/meta>
#include <iostream>

// 简化的 CompileString
template <size_t N>
struct CompileString {
    char value[N];
    static constexpr size_t size = N - 1;
    
    constexpr CompileString(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i) value[i] = str[i];
    }
    
    template <size_t M>
    constexpr auto operator+(const CompileString<M>& other) const {
        char result[N + M - 1] = {};
        for (size_t i = 0; i < N - 1; ++i) result[i] = value[i];
        for (size_t i = 0; i < M; ++i) result[N - 1 + i] = other.value[i];
        return CompileString<N + M - 1>(result);
    }
};

// 基础类型签名
template <typename T> struct TypeSignature;
template <> struct TypeSignature<int32_t> { 
    static consteval auto calculate() { return CompileString{"i32"}; } 
};
template <> struct TypeSignature<double> { 
    static consteval auto calculate() { return CompileString{"f64"}; } 
};

struct TestStruct {
    int32_t x;
    double y;
};

// 尝试使用索引序列方法
template<typename T, std::size_t Index>
static consteval auto get_member_at() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[Index];
}

template<typename T, std::size_t Index>
static consteval auto get_field_sig() {
    using namespace std::meta;
    constexpr auto member = get_member_at<T, Index>();
    
    // 关键测试：这行能工作吗？
    using FieldType = [:type_of(member):];
    
    return TypeSignature<FieldType>::calculate();
}

int main() {
    std::cout << "Testing index sequence for TypeSignature..." << std::endl;
    
    // 测试
    constexpr auto sig0 = get_field_sig<TestStruct, 0>();
    constexpr auto sig1 = get_field_sig<TestStruct, 1>();
    
    std::cout << "Field 0: ";
    for (size_t i = 0; i < sig0.size; ++i) std::cout << sig0.value[i];
    std::cout << std::endl;
    
    std::cout << "Field 1: ";
    for (size_t i = 0; i < sig1.size; ++i) std::cout << sig1.value[i];
    std::cout << std::endl;
    
    return 0;
}
