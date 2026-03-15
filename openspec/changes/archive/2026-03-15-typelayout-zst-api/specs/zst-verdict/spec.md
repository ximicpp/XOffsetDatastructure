## ADDED Requirements

### Requirement: 编译期 ZST 联合判定 API

TypeLayout SHALL 提供 `all_serialization_free(PlatformInfo&, PlatformInfo&)` constexpr 函数，对两个平台的所有注册类型同时执行 C1（layout 签名匹配）和 C2（安全分类 == Safe）检查，返回 `bool`。

**语义定义**:
- C1: 对每个类型 i，`a.types[i].layout_sig` 与 `b.types[i].layout_sig` 字符串相等
- C2: 对每个类型 i，`classify_safety(a.types[i].layout_sig) == SafetyLevel::Safe`
- 当 `a.type_count != b.type_count` 时返回 `false`

#### Scenario: 两个相同平台的安全类型集合
- **WHEN** 两个 PlatformInfo 包含相同的安全类型签名（无 ptr/union/bits/wchar/f80/vptr 标记）
- **THEN** `all_serialization_free(a, b)` 返回 `true`

#### Scenario: layout 签名不匹配
- **WHEN** 两个 PlatformInfo 中某类型的 layout 签名不同（如 `i32` vs `i64`）
- **THEN** `all_serialization_free(a, b)` 返回 `false`（C1 失败）

#### Scenario: 签名匹配但含不安全标记
- **WHEN** 两个 PlatformInfo 的签名相同，但签名中包含 `ptr[`、`union[`、`,vptr]` 等标记
- **THEN** `all_serialization_free(a, b)` 返回 `false`（C2 失败）

#### Scenario: 类型数量不匹配
- **WHEN** `a.type_count != b.type_count`
- **THEN** `all_serialization_free(a, b)` 返回 `false`

#### Scenario: 可在 static_assert 中使用
- **WHEN** 在编译期上下文中调用 `all_serialization_free`
- **THEN** 编译器可在 constexpr 求值中执行该函数
- **AND** 可用于 `static_assert(all_serialization_free(a, b), "...")`

---

### Requirement: TYPELAYOUT_ASSERT_SERIALIZATION_FREE 宏

TypeLayout SHALL 提供 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE(first, ...)` 宏，接受两个或多个平台命名空间名，展开为 pairwise `static_assert(all_serialization_free(...))` 调用。

**约定**: 平台命名空间名对应 `.sig.hpp` 中的 `boost::typelayout::platform::<name>`。

#### Scenario: 两个平台的 ZST 断言
- **WHEN** 用户写 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE(x86_64_linux, aarch64_linux)`
- **THEN** 展开为 `static_assert(all_serialization_free(x86_64_linux::get_platform_info(), aarch64_linux::get_platform_info()), ...)`
- **AND** 如果所有类型 C1∧C2 通过则编译成功，否则 static_assert 失败

#### Scenario: 三个平台的 ZST 断言
- **WHEN** 用户写 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE(a, b, c)`
- **THEN** 展开为 pairwise 检查 `(a,b)` 和 `(a,c)`
- **AND** 利用 layout match 的传递性，不需要检查 `(b,c)`

#### Scenario: 与现有 TYPELAYOUT_ASSERT_COMPAT 共存
- **WHEN** 代码中同时使用 `TYPELAYOUT_ASSERT_COMPAT` 和 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE`
- **THEN** 两个宏独立工作，不互相干扰
- **AND** `ASSERT_COMPAT` 只检查 C1，`ASSERT_SERIALIZATION_FREE` 检查 C1+C2

---

### Requirement: CompatReporter 运行期 ZST 查询

TypeLayout 的 `CompatReporter` SHALL 提供 `all_serialization_free()` 和 `is_type_serialization_free(name)` 方法，在运行期判定 ZST。

#### Scenario: 所有类型均 serialization-free
- **WHEN** 所有注册平台的所有类型 layout 匹配且安全分类为 Safe
- **THEN** `reporter.all_serialization_free()` 返回 `true`

#### Scenario: 部分类型不满足 ZST
- **WHEN** 某类型 layout 不匹配或安全分类非 Safe
- **THEN** `reporter.all_serialization_free()` 返回 `false`

#### Scenario: 查询单个类型的 ZST 状态
- **WHEN** 调用 `reporter.is_type_serialization_free("Player")`
- **THEN** 返回该类型是否在所有平台间满足 C1∧C2

#### Scenario: 查询不存在的类型名
- **WHEN** 调用 `reporter.is_type_serialization_free("NonExistent")`
- **THEN** 返回 `false`
