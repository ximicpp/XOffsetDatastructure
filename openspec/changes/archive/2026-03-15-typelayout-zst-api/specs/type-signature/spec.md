## MODIFIED Requirements

### Requirement: 跨平台兼容性验证工具

系统 SHALL 提供基于 TypeLayout `CompatReporter` 的跨平台兼容性验证工具，可比较多个平台的签名并生成兼容性矩阵报告。

**实现方式**：
- 使用 `TYPELAYOUT_CHECK_COMPAT` 宏自动生成比较程序
- 编译时 `static_assert` 验证 + 运行时报告输出
- 仅需 C++17 编译器（不需要 P2996）
- 运行期 `classify_safety(string_view)` SHALL 为 `constexpr`，正确检测所有风险标记包括 `union[`

#### Scenario: 运行时兼容性报告
- **WHEN** 用户运行 `tools/check_compat` 工具
- **THEN** 输出跨平台兼容性矩阵
- **AND** 报告包含 Layout 匹配状态、Definition 匹配状态和 Safety 分级
- **AND** 标注哪些类型可以零拷贝传输、哪些需要序列化

#### Scenario: Safety 分级验证
- **WHEN** 验证 XOffsetDatastructure 的核心类型（Player, Item, GameData）
- **THEN** Safety 分级应为 Safe（不含指针、位域）
- **AND** 在同架构下 Layout 和 Definition 均为 MATCH

#### Scenario: classify_safety 对含 union 签名的分类
- **WHEN** 对包含 `union[` 的签名调用 `classify_safety(string_view)`
- **THEN** 返回 `SafetyLevel::Warning`
- **AND** 与 consteval 版 `classify_safety<T>()` 的分类结果一致

#### Scenario: classify_safety(string_view) 可在 constexpr 上下文使用
- **WHEN** 在 constexpr 或 static_assert 上下文中调用 `classify_safety(string_view)`
- **THEN** 编译器可在编译期求值
- **AND** 返回与运行期相同的结果

## ADDED Requirements

### Requirement: XOffset 的 serialization-free 判断完全委托 TypeLayout

XOffsetDatastructure 的类型安全门控 SHALL 直接使用 TypeLayout 的 `is_layout_safe<T>()` 作为 C2（本地安全）判断的唯一来源，不再维护自研的分类逻辑。

**约束**:
- `DefaultPolicy::accept<T>()` SHALL 调用 `boost::typelayout::compat::is_layout_safe<remove_cv_t<T>>()`
- `StrictPolicy<Gold>::accept<T>()` SHALL 使用 `get_layout_signature<T>() == Gold && is_layout_safe<T>()`
- 不 SHALL 存在 `classify_for_xoffset<T>()`
- 不 SHALL 存在 `using boost::typelayout::compat::is_serialization_free_local`

#### Scenario: 安全类型通过 DefaultPolicy
- **WHEN** 类型 T 的 layout 签名不含任何 risk/warning 标记
- **THEN** `is_xbuffer_compatible<T>()` 返回 `true`
- **AND** 底层调用 `is_layout_safe<T>()` 而非自研函数

#### Scenario: 含指针的类型被 DefaultPolicy 拒绝
- **WHEN** 类型 T 含指针成员（签名含 `ptr[`）
- **THEN** `is_xbuffer_compatible<T>()` 返回 `false`
- **AND** 底层由 TypeLayout 的 `is_layout_safe<T>()` 返回 `false` 驱动

#### Scenario: 无冗余 ArchSpec
- **WHEN** 编译 xoffsetdatastructure.hpp
- **THEN** 不存在 `ArchSpec` 结构体、`Arch64LE` 常量、`TargetArchitecture` 常量
- **AND** 64-bit 和 little-endian 检查通过 preprocessor 宏实现

#### Scenario: 公开 API 不变
- **WHEN** 用户使用 `is_xbuffer_safe<T>::value`
- **THEN** 行为与重构前一致（同一组类型的 admit/reject 决策不变）
- **AND** `XOFFSET_REGISTER_*` 宏的使用方式不变
