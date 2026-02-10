# type-signature Specification

## Purpose
提供类型签名生成、比较和跨平台验证能力，确保 XOffsetDatastructure 的数据结构在不同编译环境、架构和进程间保持二进制兼容性。

## Requirements

### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的**唯一**实现，并在内部代码中优先使用 TypeLayout 提供的工具函数，避免重复实现。不再保留任何遗留兼容层。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`
- 内部代码应使用 `boost::typelayout::get_member_count<T>()` 而非自行实现
- 不再存在 `XTypeSignature` 命名空间

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** CMake 配置能找到 `boost/typelayout.hpp`

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout.hpp>`

#### Scenario: 无遗留兼容层残留
- **WHEN** 用户在代码中使用 `XTypeSignature::` 前缀
- **THEN** 编译失败，提示命名空间不存在
- **AND** 用户应使用 `boost::typelayout::` 替代

#### Scenario: 无冗余反射工具函数
- **WHEN** 项目需要获取类型成员数量
- **THEN** 使用 `boost::typelayout::get_member_count<T>()`
- **AND** 不存在功能重复的内部实现

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

### Requirement: 跨平台签名导出工具

系统 SHALL 提供基于 TypeLayout `SigExporter` 的跨平台签名导出工具，支持为关键数据类型生成可移植的 `.sig.hpp` 签名头文件。

**实现方式**：
- 使用 `TYPELAYOUT_EXPORT_TYPES` 宏自动生成导出程序
- 导出的 `.sig.hpp` 文件可在任何 C++17 编译器上 include

#### Scenario: 导出签名到头文件
- **WHEN** 用户编译并运行 `tools/export_signatures` 工具
- **THEN** 在指定目录生成包含签名常量的 `.sig.hpp` 文件
- **AND** 文件包含 Player, Item, GameData 的 Layout 和 Definition 签名
- **AND** 该文件可在任何 C++17 编译器上 include 并比较

#### Scenario: 跨平台签名比较 (C++17 兼容)
- **WHEN** 用户在 Platform B 上编译包含 Platform A 导出签名的代码
- **THEN** 使用 TypeLayout 的 `compat::layout_match()` 进行比较
- **AND** 不需要 P2996 编译器
- **AND** 正确报告布局是否兼容

---

### Requirement: 跨平台兼容性验证工具

系统 SHALL 提供基于 TypeLayout `CompatReporter` 的跨平台兼容性验证工具，可比较多个平台的签名并生成兼容性矩阵报告。

**实现方式**：
- 使用 `TYPELAYOUT_CHECK_COMPAT` 宏自动生成比较程序
- 编译时 `static_assert` 验证 + 运行时报告输出
- 仅需 C++17 编译器（不需要 P2996）

#### Scenario: 运行时兼容性报告
- **WHEN** 用户运行 `tools/check_compat` 工具
- **THEN** 输出跨平台兼容性矩阵
- **AND** 报告包含 Layout 匹配状态、Definition 匹配状态和 Safety 分级
- **AND** 标注哪些类型可以零拷贝传输、哪些需要序列化

#### Scenario: Safety 分级验证
- **WHEN** 验证 XOffsetDatastructure 的核心类型（Player, Item, GameData）
- **THEN** Safety 分级应为 Safe（不含指针、位域）
- **AND** 在同架构下 Layout 和 Definition 均为 MATCH

---

### Requirement: 集成架构分析与持续评估

系统 SHALL 维护 TypeLayout 集成的架构分析文档，记录职责边界、使用模式和改进建议。

#### Scenario: 分析报告可用
- **WHEN** 开发者需要了解 TypeLayout 与 XOffsetDatastructure 的关系
- **THEN** 可在 `docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md` 找到完整分析
- **AND** 报告包含职责边界、使用合理性评估和改进建议