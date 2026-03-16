## Why

TypeLayout 内部存在概念矛盾：

- `admission.hpp` 提供 `is_byte_copy_safe_v<T>`，接受 relocatable opaque 类型（XVector、XString 等）
- `opaque.hpp` 提供 `TYPELAYOUT_OPAQUE_*_RELOCATABLE` 宏，声明这些类型是 byte-copy safe
- `layout_traits<T>::has_pointer` 对这些类型返回 false（offset_ptr 不是原生指针）

但 `serialization_free.hpp` 中的 `is_transfer_safe<T>(remote_sig)` 使用 `is_local_serialization_free_v<T>` 作为前提条件——后者要求 `trivially_copyable`。所有 relocatable opaque 类型（XVector、XString 等）都不是 trivially_copyable，因此 `is_transfer_safe` 对它们**永远返回 false**。

这导致 TypeLayout 自己提供的跨平台验证 API 对自己认可的 byte-copy safe 类型完全不可用。

## What Changes

**目标仓库**: TypeLayout (https://github.com/ximicpp/TypeLayout)

### 1. `is_transfer_safe<T>` 统一本地安全前提

`serialization_free.hpp` 第 54-63 行：

```cpp
// BEFORE
template <typename T>
[[nodiscard]] inline bool is_transfer_safe(std::string_view remote_sig) noexcept {
    if constexpr (!is_local_serialization_free_v<T>) {  // ← 要求 trivially_copyable
        return false;
    } else {
        constexpr auto local_sig = get_layout_signature<T>();
        return std::string_view(local_sig) == remote_sig;
    }
}

// AFTER
template <typename T>
[[nodiscard]] inline bool is_transfer_safe(std::string_view remote_sig) noexcept {
    if constexpr (!is_byte_copy_safe_v<T>) {             // ← 接受 relocatable opaque
        return false;
    } else {
        constexpr auto local_sig = get_layout_signature<T>();
        return std::string_view(local_sig) == remote_sig;
    }
}
```

需要在 `serialization_free.hpp` 中添加 `#include <boost/typelayout/admission.hpp>`。

### 2. `SignatureRegistry::register_local<T>` 统一 static_assert

`serialization_free.hpp` 第 72-88 行，两个 `register_local` 重载中的 `static_assert`：

```cpp
// BEFORE
static_assert(is_local_serialization_free_v<T>,
    "Only locally serialization-free types can be registered.");

// AFTER
static_assert(is_byte_copy_safe_v<T>,
    "Only byte-copy safe types can be registered. "
    "Type must be either locally serialization-free (trivially_copyable + no pointer) "
    "or a registered relocatable opaque type with safe elements.");
```

### 3. 文档化 opaque tag 匹配假设

在 `opaque.hpp` 头部注释区域添加：

```
Opaque Tag Matching Assumption:
  When two endpoints compare opaque type signatures for cross-platform
  transfer safety (via is_transfer_safe), matching is based on:
    - tag name (e.g. "vector", "string")
    - sizeof
    - alignof
    - element type signature (for container templates)

  This assumes that types registered with the same tag, sizeof, and alignof
  have identical internal binary layout. This is the user's responsibility
  to guarantee — typically ensured by using the same library version
  (e.g. Boost.Interprocess) and the same compiler ABI on both endpoints.

  TypeLayout does NOT verify internal field layout of opaque types.
```

### 4. 保留 `is_local_serialization_free_v`

不删除。它退为 "strict C++ POD safety" 的独立查询，仍可用于不涉及 relocatable 容器的场景。

## Capabilities

### Modified Capabilities
- `type-signature`: `is_transfer_safe` 现在接受所有 `is_byte_copy_safe_v<T> == true` 的类型

## Impact

- **文件**: `serialization_free.hpp`（API 语义变更）、`opaque.hpp`（文档注释）
- **API 变更**: `is_transfer_safe<T>` 语义放宽——从 "trivially_copyable + no pointer + sig match" 到 "byte-copy safe + sig match"
- **向后兼容**: 之前返回 true 的类型仍返回 true（`is_local_serialization_free_v ⊂ is_byte_copy_safe_v`）。新增返回 true 的类型是 relocatable opaque 类型（之前永远 false）。
- **测试**: 需要添加测试验证 relocatable opaque 类型的 `is_transfer_safe` 返回 true
