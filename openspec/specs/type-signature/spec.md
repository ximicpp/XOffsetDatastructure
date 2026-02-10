# type-signature Specification

## Purpose
TBD - created by archiving change integrate-typelayout-library. Update Purpose after archive.
## Requirements
### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的核心实现。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** CMake 配置能找到 `boost/typelayout.hpp`

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout.hpp>`

---

### Requirement: Definition Signature API 暴露

系统 SHALL 暴露 TypeLayout 的 Definition Signature API 作为推荐的类型签名接口。

**核心函数**:
```cpp
namespace boost::typelayout {
    template<class T> consteval auto get_definition_signature();
    template<class T, class U> consteval bool definition_signatures_match();
}
```

#### Scenario: 生成单一类型的 Definition Signature
- **WHEN** 用户调用 `boost::typelayout::get_definition_signature<MyStruct>()`
- **THEN** 返回包含字段名、类型、偏移量、继承结构的完整签名字符串
- **AND** 签名格式为 `[64-le]record[s:N,a:M]{@offset[name]:type...}`

#### Scenario: 比较两个类型的结构兼容性
- **WHEN** 用户调用 `definition_signatures_match<TypeA, TypeB>()`
- **THEN** 若两类型结构完全相同返回 `true`
- **AND** 若字段名/类型/偏移/继承任一不同返回 `false`

---

### Requirement: Layout Signature API 暴露

系统 SHALL 同时暴露 TypeLayout 的 Layout Signature API 用于纯字节布局比较场景。

**核心函数**:
```cpp
namespace boost::typelayout {
    template<class T> consteval auto get_layout_signature();
    template<class T, class U> consteval bool layout_signatures_match();
}
```

#### Scenario: 生成纯字节布局签名
- **WHEN** 用户调用 `boost::typelayout::get_layout_signature<MyStruct>()`
- **THEN** 返回不含字段名的纯布局签名
- **AND** 继承结构被扁平化处理

#### Scenario: 共享内存/IPC 场景的布局比较
- **WHEN** 用户调用 `layout_signatures_match<SenderType, ReceiverType>()`
- **THEN** 仅比较字节布局（偏移、大小、对齐）
- **AND** 忽略字段名差异

---

### Requirement: XTypeSignature 兼容层

系统 SHALL 保留 `XTypeSignature` 命名空间作为兼容层，内部委托到 TypeLayout 实现。

**兼容 API**:
```cpp
namespace XTypeSignature {
    template<class T> [[deprecated("Use boost::typelayout::get_definition_signature")]]
    consteval auto get_XTypeSignature();
}
```

#### Scenario: 现有代码继续编译
- **WHEN** 用户代码使用 `XTypeSignature::get_XTypeSignature<T>()`
- **THEN** 代码正常编译
- **AND** 产生 deprecation warning
- **AND** 返回等效于 TypeLayout Definition Signature 的结果

#### Scenario: 现有 static_assert 检查继续工作
- **WHEN** 现有代码包含 `static_assert(get_XTypeSignature<T>() == "...")`
- **THEN** 断言继续按预期工作
- **AND** 迁移指南说明如何更新到新 API

---

### Requirement: 跨平台签名导出工具

系统 SHALL 提供跨平台签名导出能力，支持在不同架构间验证类型兼容性。

#### Scenario: 导出签名到头文件
- **WHEN** 用户在 Platform A 编译并执行签名导出
- **THEN** 生成包含签名常量的 `.sig.hpp` 文件
- **AND** 该文件可以在 Platform B 上 include 并比较

#### Scenario: 跨平台签名比较 (C++17 兼容)
- **WHEN** 用户在 Platform B 上比较 Platform A 导出的签名
- **THEN** 使用 TypeLayout 的兼容模式（不需要 P2996）
- **AND** 正确报告布局是否兼容

