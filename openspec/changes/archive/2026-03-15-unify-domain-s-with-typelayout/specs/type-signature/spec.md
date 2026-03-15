## MODIFIED Requirements

### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名和类型安全判定的**唯一**实现。TypeLayout 不仅提供签名生成，还作为 XOffset 的唯一类型安全引擎——所有 Domain S（安全类型集）的准入判定 SHALL 委托给 TypeLayout 的 API。不再保留任何自建类型分类逻辑（`classify_for_xoffset`、`is_layout_safe`、旧三级 `SafetyLevel`、`ArchSpec` 等均须删除）。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支（须包含 `TYPELAYOUT_OPAQUE_*_AUTO` 宏）
- 头文件路径: `external/typelayout/include`
- 内部代码 SHALL 使用 TypeLayout 的 `is_local_serialization_free_v<T>` 作为单平台 C2 判定
- 内部代码 SHALL 使用 TypeLayout 的 `is_transfer_safe<T>(sig)` 作为跨平台 C1+C2 判定
- 内部代码 SHALL 使用 TypeLayout 的 `classify_v<T>` 五级分类作为诊断信息源
- 不再存在任何 XOffset 自建的类型安全判定函数

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** TypeLayout 版本包含 `TYPELAYOUT_OPAQUE_TYPE_AUTO` 等宏定义

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout/tools/serialization_free.hpp>`
- **AND** 项目可以 `#include <boost/typelayout/tools/classify.hpp>`

#### Scenario: 无自建类型分类逻辑残留
- **WHEN** 在 xoffsetdatastructure.hpp 中搜索 `classify_for_xoffset`、`is_layout_safe`、旧三级 `SafetyLevel`、`ArchSpec`
- **THEN** 均不存在
- **AND** DefaultPolicy::accept 直接委托给 TypeLayout API

#### Scenario: 无冗余反射工具函数
- **WHEN** 项目需要获取类型成员数量
- **THEN** 使用 `boost::typelayout::get_member_count<T>()`
- **AND** 不存在功能重复的内部实现

## ADDED Requirements

### Requirement: Opaque 类型自动注册宏

TypeLayout SHALL 提供 `TYPELAYOUT_OPAQUE_TYPE_AUTO`、`TYPELAYOUT_OPAQUE_CONTAINER_AUTO`、`TYPELAYOUT_OPAQUE_MAP_AUTO` 宏，用于自动推导 sizeof/alignof 的 opaque 类型注册。这些宏 SHALL 设置 `pointer_free = true`（默认），适用于使用 offset_ptr 的容器类型。

**宏签名**:
```cpp
TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)                // 普通类型
TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)       // 单参数容器模板
TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)             // 双参数容器模板
```

#### Scenario: 注册 XString 为 opaque 类型
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_TYPE_AUTO(MyLib::XString, "string")`
- **THEN** TypeSignature 特化被生成，签名格式为 `string[s:N,a:M]`
- **AND** `is_opaque == true` 且 `pointer_free == true`
- **AND** `is_local_serialization_free_v<MyLib::XString>` 返回 true

#### Scenario: 注册 XVector 为 opaque 容器
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_CONTAINER_AUTO(MyLib::XVector, "vector")`
- **THEN** TypeSignature 特化对所有 `XVector<T>` 生效
- **AND** 签名包含元素类型签名: `vector[s:N,a:M]<element_sig>`
- **AND** `is_local_serialization_free_v<MyLib::XVector<int32_t>>` 返回 true

#### Scenario: 注册 XMap 为 opaque 双参数容器
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_MAP_AUTO(MyLib::XMap, "map")`
- **THEN** TypeSignature 特化对所有 `XMap<K,V>` 生效
- **AND** 签名包含键值类型签名: `map[s:N,a:M]<key_sig,value_sig>`

### Requirement: 单平台类型安全判定 (C2)

XOffset 的 `DefaultPolicy::accept<T>()` SHALL 直接委托给 TypeLayout 的 `is_local_serialization_free_v<T>` 作为单平台安全判定。

此判定的语义为：`trivially_copyable(T) && !has_pointer(T)`。
- 允许：fixed-width integers, float/double, bool, char, enum, union (trivially_copyable), struct (all members safe), opaque registered types (pointer_free)
- 拒绝：raw pointers, references, polymorphic types, std containers, type-erased types

#### Scenario: DefaultPolicy 接受基础安全类型
- **WHEN** `DefaultPolicy::accept<int32_t>()` 被求值
- **THEN** 返回 true
- **AND** `is_local_serialization_free_v<int32_t>` 为 true

#### Scenario: DefaultPolicy 接受 union 类型
- **WHEN** 定义 `union U { int32_t a; float b; };` 并求值 `DefaultPolicy::accept<U>()`
- **THEN** 返回 true（union 不再被排除）

#### Scenario: DefaultPolicy 接受 platform-variant 类型
- **WHEN** 定义 `struct S { long x; };` 并求值 `DefaultPolicy::accept<S>()`
- **THEN** 返回 true（platform-variant 由 C1 签名比较捕获，不在 C2 拒绝）

#### Scenario: DefaultPolicy 拒绝含指针类型
- **WHEN** 定义 `struct S { int* p; };` 并求值 `DefaultPolicy::accept<S>()`
- **THEN** 返回 false

#### Scenario: DefaultPolicy 拒绝多态类型
- **WHEN** 定义含 virtual 函数的类 `class P { virtual void f(); };` 并求值 `DefaultPolicy::accept<P>()`
- **THEN** 返回 false

### Requirement: 跨平台类型安全判定 (C1+C2)

XOffset 的 `StrictPolicy<Gold>::accept<T>()` SHALL 委托给 TypeLayout 的 `is_transfer_safe<T>(Gold)` 作为跨平台安全判定。

此判定的语义为：`is_local_serialization_free_v<T> && get_layout_signature<T>() == Gold`。

#### Scenario: StrictPolicy 接受签名匹配的类型
- **WHEN** Gold 签名与当前平台 T 的签名一致
- **THEN** `StrictPolicy<Gold>::accept<T>()` 返回 true

#### Scenario: StrictPolicy 拒绝签名不匹配的类型
- **WHEN** Gold 签名与当前平台 T 的签名不一致（例如 long 在不同平台上 sizeof 不同）
- **THEN** `StrictPolicy<Gold>::accept<T>()` 返回 false

### Requirement: 五级诊断分类

`validate_xbuffer_type<T>()` 的错误消息 SHALL 基于 TypeLayout 的 `classify_v<T>` 五级分类提供精确诊断。

#### Scenario: 精确诊断指针风险
- **WHEN** 类型 T 含指针且 `is_xbuffer_safe<T>::value == false`
- **THEN** 错误消息指出 "Contains pointer/reference — absolute address unsafe for buffer"

#### Scenario: 诊断不再提及 union
- **WHEN** 查看 `validate_xbuffer_type` 的 static_assert 消息
- **THEN** 不包含 "Union types" 的排除提示