# Skill: reflection-audit

## Description
检查 C++26 P2996 反射代码路径的正确性，包括成员迭代、偏移计算、聚合构造等。

## When to Use
- 修改反射相关代码后
- 新增使用反射的数据结构时
- P2996 编译器版本升级后

## P2996 Core Constructs

### 反射操作符
```cpp
#ifdef __cpp_reflection
using namespace std::meta;

// 获取类型的非静态数据成员
auto members = nonstatic_data_members_of(^T);

// 获取成员偏移量
size_t offset = offset_of(member);

// 获取成员名称
auto name = identifier_of(member);

// Splice（拼接）语法 — 将反射信息转回代码
auto& value = obj.[:member:];
```

### 关键函数审计目标
1. **`reflect_init_all<T>`** — 通过反射初始化所有成员
2. **`aggregate_construct<T>`** — 使用 allocator 构造聚合类型
3. **`detail::construct_root<T>`** — 在 XBuffer 中构造根对象
4. **`migrate_element<T>`** — XCompactor 中的元素迁移

## Check Points

### 1. 成员迭代完整性
- [ ] `nonstatic_data_members_of(^T)` 是否遍历了所有成员
- [ ] 是否正确处理了继承的成员
- [ ] 是否跳过了静态成员和函数

### 2. XVector 成员检测
- [ ] 是否正确识别 `XVector<U>` 类型的成员
- [ ] 是否对 XVector 成员使用了 allocator 构造
- [ ] 嵌套的 XVector（`XVector<XVector<U>>`）是否处理

### 3. Allocator 传播
- [ ] allocator 是否正确传递给所有需要的成员
- [ ] 聚合构造时是否区分 POD 和容器成员
- [ ] `has_allocator_constructor<T>` 检测是否准确

### 4. 偏移计算
- [ ] `std::meta::offset_of` 与 `offsetof` 宏的结果是否一致
- [ ] padding 和 alignment 是否正确处理

### 5. 编译条件
- [ ] `#ifdef __cpp_reflection` 是否正确包围所有反射代码
- [ ] 非反射路径（fallback）是否可编译
- [ ] 编译标志 `-freflection -fexpansion-statements` 是否都需要

## Testing
关键测试文件：
- `tests/test_reflection_operators.cpp` — 反射操作符测试
- `tests/test_complex_nesting.cpp` — 嵌套结构反射构造
- `tests/test_long_reflection_probe.cpp` — `long` 类型反射探测

## Important Notes
- P2996 语法仍在演进中，注意编译器版本差异
- `[: ... :]` splice 语法是 P2996 特有的
- 反射代码只在 Clang P2996 编译器下编译
- 非反射路径必须保持可用作为 fallback
