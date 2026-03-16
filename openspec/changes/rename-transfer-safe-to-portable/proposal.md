## Why

`is_transfer_safe<T>(remote_sig)` 的命名含糊：
- "transfer" 没有暗示"跨平台"和"签名匹配"这两个关键要素
- 与 `is_byte_copy_safe_v<T>` 的命名风格不统一，用户难以推断两者关系

## What Changes

**目标仓库**: TypeLayout (upstream)

### 重命名

```cpp
// BEFORE
template <typename T>
bool is_transfer_safe(std::string_view remote_sig);

// AFTER
template <typename T>
bool is_byte_copy_portable(std::string_view remote_sig);
```

保留 `is_transfer_safe` 作为 deprecated alias 一段时间。

### 命名关系

```
is_byte_copy_safe_v<T>            — 本类型的字节可以被安全复制（编译期）
is_byte_copy_portable<T>(sig)     — 安全复制 + 跨平台布局一致（运行时）
is_local_serialization_free_v<T>  — 不改（独立的 strict C++ POD 概念）
```

### 不改的部分

- `is_byte_copy_safe_v<T>` — 名字已足够精确
- `is_local_serialization_free_v<T>` — 独立概念，语义清晰
- `classify_v<T>` / `SafetyLevel` — 正交概念，不受影响

## Capabilities

### Modified Capabilities
- `type-signature`: API 重命名 `is_transfer_safe` → `is_byte_copy_portable`

## Impact

- **TypeLayout**: `serialization_free.hpp` 中的函数重命名 + deprecated alias
- **XOffset**: 更新 `using` 声明和注释（如有引用）
- **测试**: 全部测试中的 `is_transfer_safe` 引用需更新
- **向后兼容**: deprecated alias 确保旧代码仍可编译
