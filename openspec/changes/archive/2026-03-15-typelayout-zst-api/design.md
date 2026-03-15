## Context

XOffsetDatastructure 使用 TypeLayout 库提供类型签名和安全分类功能。TypeLayout 的核心能力已经成熟：

- **签名引擎**：通过 P2996 反射递归生成任意深度嵌套类型（包括 opaque 容器）的 layout/definition 签名
- **安全分类**：`classify_safety<T>()` (consteval) 和 `classify_safety(string_view)` (runtime) 扫描签名标记
- **跨平台比较**：`TYPELAYOUT_ASSERT_COMPAT` 宏 + `CompatReporter` 对比 .sig.hpp 签名

然而存在以下问题：
1. XOffset 自研了 `classify_for_xoffset<T>()`，与 TypeLayout 的 `is_layout_safe<T>()` 功能重叠
2. 运行期 `classify_safety(string_view)` 缺少 `union[` 检测（consteval 版已有）
3. 没有 C1+C2 联合 constexpr 判定 API — `TYPELAYOUT_ASSERT_COMPAT` 只做 C1，不做 C2
4. XOffset 的 `ArchSpec` 与 TypeLayout 签名系统大量重叠

## Goals / Non-Goals

**Goals:**
- TypeLayout 提供完整的 ZST 判定能力（C1∧C2 联合 constexpr 断言）
- XOffset 的 serialization-free 核心判断 100% 委托给 TypeLayout
- 修复 `classify_safety(string_view)` 的 union 漏检 bug
- 保持 XOffset 公开 API 不变（`is_xbuffer_safe<T>::value` 等）

**Non-Goals:**
- 不修改 TypeLayout 的签名引擎或 opaque 机制
- 不修改 XOffset 的 XBuffer、XCompactor、XVector/XString 等数据结构实现
- 不修改 XOFFSET_REGISTER_* 宏的行为
- 不新增跨平台 CI 管线（仅提供宏，CI 集成由后续 change 处理）

## Decisions

### Decision 1: XOffset 的 C2 判断直接使用 `is_layout_safe<T>()`

**选择**: 删除 `classify_for_xoffset<T>()`，`DefaultPolicy::accept<T>()` 直接调用 TypeLayout 的 `is_layout_safe<T>()`。

**理由**:
- `is_layout_safe<T>()` = `classify_safety<T>() == SafetyLevel::Safe`
- Warning→Risk 升级对 bool 门控结果毫无影响（Warning 已经 ≠ Safe → false）
- `long` 拒绝是 C1 问题（跨平台签名不匹配），不应在 C2 层处理

**替代方案考虑**:
- ❌ 保留 `classify_for_xoffset` 作为薄封装 → 无实际功能，增加维护成本
- ❌ 在 TypeLayout 添加可定制安全策略 → 过度工程，`is_layout_safe` 已足够

### Decision 2: `classify_safety(string_view)` 升级为 constexpr

**选择**: `inline` → `inline constexpr`，使 Phase 2 的 `all_serialization_free()` 能在 static_assert 中调用。

**理由**:
- `string_view` 构造函数、`find()`、`npos` 在 C++17 中均为 constexpr
- `constexpr` 是 `inline` 的严格超集，不破坏现有调用

**风险**: 极少数旧编译器可能不支持 → 缓解：TypeLayout 的 Phase 2 target 是 C++17，主流编译器均支持

### Decision 3: `all_serialization_free()` 的 C1+C2 联合判定

**选择**: 在 `compat_auto.hpp` 中新增 constexpr 函数，对 PlatformInfo 中的每个类型同时检查 C1（layout 签名字符串相等）和 C2（`classify_safety(sig) == Safe`）。

**接口设计**:
```cpp
namespace boost::typelayout::compat::detail {
    inline constexpr bool all_serialization_free(
        const PlatformInfo& a, const PlatformInfo& b);
}
```

**理由**:
- 复用现有 `PlatformInfo` 数据结构（.sig.hpp 已生成）
- Pairwise 检查足够（layout match 具有传递性；safety 只需检查一侧因为签名相等则分类结果相等）
- 放在 `compat_auto.hpp` 中与现有 `all_layouts_match` 并列

### Decision 4: 删除 ArchSpec 结构体

**选择**: 完全删除 `ArchSpec`、`Arch64LE`、`TargetArchitecture` 及其 15 个 static_assert。保留 preprocessor 层面的 64-bit + little-endian 门控（`XOFFSET_64BIT_CHECK` 和 `XOFFSET_LITTLE_ENDIAN`）。

**理由**:
- `sizeof(int8_t) == 1` 等断言恒为真（C++ 标准保证）
- `alignof(int32_t) == 4` 等信息已编码在 TypeLayout 签名中
- `pointer_size` 和 endianness 已有 preprocessor 检查
- ArchSpec 无外部使用者（仅头文件内部引用）

### Decision 5: long(LLP64) 行为变化的处理

**选择**: 不再在 C2 层拒绝 `long`，改由 C1 层（`TYPELAYOUT_ASSERT_SERIALIZATION_FREE`）在 CI 捕获。

**理由**:
- 概念正确性：`long` 在 LLP64 上映射为 `i32[s:4,a:4]`，本地布局安全，属于 C2 Safe
- 跨平台差异（LP64 的 `i64` vs LLP64 的 `i32`）是 C1 问题
- 当前 `classify_for_xoffset` 对 `struct{long x;}` 的 P2996 脱糖成员无法拦截，保护不完整

## Risks / Trade-offs

- **[Risk] long(LLP64) 行为变化** → 用户在 Windows 上用 `long` 不再被编译期 C2 拒绝
  - Mitigation: C1 在 CI 中完整捕获；在文档中说明建议使用固定宽度类型
  - Mitigation: 当前拦截本来就不完整（struct 成员绕过）

- **[Risk] constexpr string_view 在特定编译器下不支持** → `all_serialization_free` 无法编译
  - Mitigation: 回退方案：用 `const char*` 手工 `strstr` 实现 `classify_safety_cstr`
  - Mitigation: Phase 2 target 是 C++17，GCC 7+/Clang 5+/MSVC 19.15+ 均支持

- **[Trade-off] 删除 classify_for_xoffset 导致不再区分 Warning 和 Risk**
  - XOffset 的门控只需要 bool（admit/reject），不需要三级分类
  - 诊断函数 `get_safety_error_message` 仍可调用 TypeLayout 的 `classify_safety<T>()` 获取细粒度信息
