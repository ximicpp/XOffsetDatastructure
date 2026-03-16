## Context

TypeLayout `serialization_free.hpp` 暴露 `is_transfer_safe<T>(remote_sig)` 函数，
用于跨平台零编码传输验证。该函数与同库的 `is_byte_copy_safe_v<T>` 构成
"本地安全 → 跨平台安全" 的递进关系，但命名风格不统一，语义模糊。

当前 API 谱系：

| 谓词 | 形状 | 含义 |
|---|---|---|
| `is_byte_copy_safe_v<T>` | 编译期 `constexpr bool` | 本地 byte-copy 安全 |
| `is_transfer_safe<T>(sig)` | 运行时 `bool` | 跨平台传输安全 |
| `is_local_serialization_free_v<T>` | 编译期 `constexpr bool` | Strict C++ POD 安全 |

前两者构成递进关系 (`is_transfer_safe = is_byte_copy_safe_v + 签名匹配`)，
但命名上 "byte_copy" 与 "transfer" 没有任何词汇关联。

## Goals / Non-Goals

**Goals:**
- 将 `is_transfer_safe` 重命名为 `is_byte_copy_portable`，与 `is_byte_copy_safe_v` 形成统一前缀
- 保留 `is_transfer_safe` 作为 deprecated alias，确保向后兼容
- 更新 TypeLayout 和 XOffset 中所有引用

**Non-Goals:**
- 不改 `is_byte_copy_safe_v`（名字已足够精确）
- 不改 `is_local_serialization_free_v`（独立概念）
- 不改 `classify_v` / `SafetyLevel`（正交概念）
- 不改函数签名或行为（纯重命名，零语义变更）

## Decisions

### Decision 1: 新名称选择 `is_byte_copy_portable`

**选择**: `is_byte_copy_portable<T>(remote_sig)`

**理由**:
- `byte_copy` 前缀与 `is_byte_copy_safe_v` 统一
- `portable` 暗示跨平台/跨机器可移植性
- `safe` vs `portable` 清晰表达递进: "安全" → "安全 + 可移植"
- 不与 C++ 标准术语冲突

**备选方案**:
- `is_relocatable_across` — 拒绝，`relocatable` 在 C++ 中已有不同含义 (P1144)
- `is_byte_portable` — 拒绝，丢失了 "copy" 的语义
- `is_cross_platform_safe` — 拒绝，太长且不与 `byte_copy` 前缀对齐

### Decision 2: Deprecated alias 保留策略

**选择**: 保留 `is_transfer_safe` 作为 inline wrapper + `[[deprecated]]` 属性

```cpp
template <typename T>
[[deprecated("Use is_byte_copy_portable instead")]]
inline bool is_transfer_safe(std::string_view sig) noexcept {
    return is_byte_copy_portable<T>(sig);
}
```

**理由**: TypeLayout 可能有其他下游用户，直接删除会 break 他们。

### Decision 3: SignatureRegistry 方法同步重命名

**选择**: `SignatureRegistry::is_safe` → `SignatureRegistry::is_portable`

**理由**: 内部 API 一致性。保留旧名 deprecated alias。

## Risks / Trade-offs

- **[Risk] 下游用户使用 `is_transfer_safe`** → deprecated alias 保证编译通过，编译器发出 deprecation warning
- **[Trade-off] 名字更长** → `is_byte_copy_portable` (22 字符) vs `is_transfer_safe` (16 字符)，可接受
- **[Risk] `portable` 可能被误解为 "可移植编译"** → 函数签名要求 `remote_sig` 参数，上下文足够明确
