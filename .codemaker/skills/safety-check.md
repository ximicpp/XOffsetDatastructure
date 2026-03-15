# Skill: safety-check

## Description
验证用户定义的数据结构是否满足 XBuffer 的安全性要求，使用 TypeLayout 库进行 C1/C2 安全性分类。

## When to Use
- 新增数据结构到项目后
- 修改现有结构的成员后
- 构建时出现 `validate_xbuffer_type` 静态断言失败时

## Background

### TypeLayout 安全性分级
- **C1 (Binary Layout Match)**: 类型的二进制布局在不同平台/编译器之间完全一致
- **C2 (Local Safety / Pointer-Free)**: 类型不包含原生指针，可以安全地序列化到共享内存

### XBuffer 安全规则
- 所有存储在 XBuffer 中的类型必须通过 `is_xbuffer_safe<T>` 检查
- 基本类型（`int`, `float`, `double` 等）自动满足
- XVector / XString / XMap 等容器类型使用 `offset_ptr`，满足安全要求
- `std::string`, `std::vector`, 原生指针等 **不安全**

## Steps

### Step 1: 编译时检查
在代码中使用静态断言：
```cpp
static_assert(is_xbuffer_safe<MyStruct>::value,
              "MyStruct must be safe for XBuffer");
```

### Step 2: 检查 TypeLayout 签名
```cpp
#include <boost/typelayout.hpp>
using namespace boost::typelayout;

// 获取类型签名
auto sig = type_signature<MyStruct>();

// 检查安全等级
auto safety = classify_safety<MyStruct>();
```

### Step 3: 常见不安全模式检查
```cpp
// ❌ 不安全 — 包含原生指针
struct Bad1 {
    int* data;
};

// ❌ 不安全 — 包含 std::string
struct Bad2 {
    std::string name;
};

// ✅ 安全 — 使用 XString
struct Good1 {
    template<typename Allocator>
    Good1(Allocator alloc) : name(alloc) {}
    XString name;
};

// ✅ 安全 — 纯 POD
struct Good2 {
    int id;
    float value;
};
```

### Step 4: 在测试中验证
参考 `tests/test_type_safety_comprehensive.cpp` 的模式：
```cpp
bool test_safety() {
    static_assert(is_xbuffer_safe<MyStruct>::value);
    std::cout << "[PASS] MyStruct is XBuffer safe\n";
    return true;
}
```

## Diagnostic Output
```
[TypeLayout] classify_safety<MyStruct>:
  C1 (binary match): YES
  C2 (pointer-free): YES
  Overall: SAFE for XBuffer
```

## Important Notes
- 嵌套结构中所有成员都必须是安全的
- `enum` 类型是安全的（底层为整数）
- `std::array<SafeType, N>` 是安全的
- 模板成员需要额外验证 allocator 适配
