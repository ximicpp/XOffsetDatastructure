# Change: Improve API Method Naming Conventions

## Summary
分析 XOffsetDatastructure 所有公共 API 方法名，消除不一致、冗余和语义不清的问题，建立统一的命名规范。

## Current API Inventory

### XBuffer (主用户接口)
| 方法 | 类型 | 功能 |
|------|------|------|
| `make<T>()` | instance | 创建根对象 |
| `root<T>()` | instance | 获取根对象引用 |
| `has_root<T>()` | instance | 检查根对象是否存在 |
| `make_handle<T>()` | instance | 创建根对象 + 返回 Handle |
| `handle<T>()` | instance | 获取现有根对象 Handle |
| `allocator<T>()` | instance | 获取 allocator |
| `used_size()` | instance | 已用字节数 |
| `stats()` | instance | 内存统计 |
| `save_to_string()` | instance | 序列化为 string (shrink) |
| `save_to_string_full()` | instance | 序列化为 string (无 shrink) |
| `save_to_vector()` | instance | 序列化为 vector<char> (shrink) |
| `load_from_string()` | **static** | 反序列化 |
| `load_from_vector()` | **static** | 反序列化 |
| `estimate_buffer_size()` | **static** | 估算所需 buffer 大小 |

### XBufferCore (底层，继承自 XManagedMemory)
| 方法 | 功能 |
|------|------|
| `grow(extra_bytes)` | 扩容 |
| `shrink_to_fit()` | 缩容 |
| `get_buffer()` | 获取底层 vector<char>* |
| `get_address()` | 获取内存地址 |
| `get_size()` | 获取总大小 |
| `epoch()` | 获取修改计数 |

### XCompactor
| 方法 | 功能 |
|------|------|
| `compact_automatic<T>(buf)` | 自动紧凑 |

### XBufferStats
| 方法 | 功能 |
|------|------|
| `get_memory_stats(buf)` | 获取内存统计 |
| `print_stats(buf)` | 打印统计 |

### Traits / Validators
| 名称 | 功能 |
|------|------|
| `is_xbuffer_safe<T>` | 类型安全检查 trait |
| `validate_xbuffer_type<T>()` | 编译期安全校验 |

---

## Analysis & Proposed Changes

### Issue 1: `save_to_*` / `load_from_*` — 命名冗长且不对称

**当前:**
```cpp
buf.save_to_string()       // instance method
buf.save_to_string_full()  // instance method  
buf.save_to_vector()       // instance method
XBuffer::load_from_string(data)  // static method
XBuffer::load_from_vector(data)  // static method
```

**问题:**
- `save_to_string` 中 `save_to` 是冗余动词 — buffer 本身就在做序列化
- `save_to_string_full` 中 `full` 含义模糊（实际指"不压缩"）
- C++ 标准库惯例是 `to_string()`, `from_string()`

**建议:** 改为 `serialize()` / `deserialize()` 或 `save()` / `load()`

```cpp
// 方案 A: serialize/deserialize 风格
std::string serialize()              // 替代 save_to_string
std::vector<char> serialize_bytes()  // 替代 save_to_vector
std::string snapshot()               // 替代 save_to_string_full（含义：快照，不压缩）
static XBuffer deserialize(const std::string& data)
static XBuffer deserialize(const std::vector<char>& data)

// 方案 B: save/load 风格（更简洁）
std::string save()
std::vector<char> save_bytes()
std::string save_raw()               // 不压缩
static XBuffer load(const std::string& data)
static XBuffer load(const std::vector<char>& data)
```

**推荐: 方案 B** — 更简短，且 `save/load` 是游戏/嵌入式领域最常见的序列化动词。

### Issue 2: `compact_automatic` — `automatic` 后缀多余

**当前:**
```cpp
XCompactor::compact_automatic<T>(buf)
```

**问题:**
- 只有一种 compact 方式，`automatic` 没有对照物
- 名称太长

**建议:**
```cpp
XCompactor::compact<T>(buf)
```

### Issue 3: `get_memory_stats` / `get_buffer` / `get_size` / `get_address` — `get_` 前缀

**当前:** Boost 风格的 `get_*` 前缀

**问题:**
- 现代 C++ 偏好无 `get_` 前缀的属性式命名（如 `std::vector::size()`, `std::span::data()`）
- `get_buffer()` 返回 `vector<char>*`，"buffer" 语义模糊

**建议（仅对 XBuffer/XBufferStats 新增方法）:**
- `XBufferStats::get_memory_stats(buf)` → `XBufferStats::memory_stats(buf)`
- XBufferCore 的方法（`get_size`, `get_buffer`, `get_address`）属于 Boost 继承 API，**不修改**

### Issue 4: `is_xbuffer_safe` — 旧名称遗留

**当前:**
```cpp
is_xbuffer_safe<T>::value
validate_xbuffer_type<T>()
```

**问题:** 上一轮重命名后 `XBuffer` 已经从底层变为用户接口，但 trait 名暗示"buffer 安全"，实际是"类型安全"。

**建议:** 保持不变 — `is_xbuffer_safe` 语义是"该类型是否可安全用于 XBuffer"，逻辑正确。

### Issue 5: `estimate_buffer_size` — 命名清晰但位置不当

**当前:**
```cpp
XBuffer::estimate_buffer_size(100)  // static
```

**建议:** 保持不变 — 作为 `XBuffer` 的 static factory helper，位置和命名合理。

### Issue 6: `make<T>()` / `root<T>()` / `has_root<T>()` — 核心 API

**评估:** 命名简洁、语义明确、与 C++ 惯例一致（`std::make_shared`, `nlohmann::json::at()`）。

**建议:** 保持不变。

### Issue 7: `make_handle<T>()` / `handle<T>()`

**评估:** 与 `make<T>()` / `root<T>()` 对称。

**建议:** 保持不变。

---

## Summary of Proposed Changes

| 当前名称 | 新名称 | 理由 |
|----------|--------|------|
| `save_to_string()` | **`save()`** | 简化，save 即序列化 |
| `save_to_string_full()` | **`save_raw()`** | 明确"不压缩"语义 |
| `save_to_vector()` | **`save_bytes()`** | 简化，bytes 表示二进制输出 |
| `load_from_string(data)` | **`load(data)`** | 简化，利用重载区分 |
| `load_from_vector(data)` | **`load(data)`** | 简化，利用重载区分 |
| `compact_automatic<T>()` | **`compact<T>()`** | 去除多余后缀 |
| `XBufferStats::get_memory_stats()` | **`XBufferStats::memory_stats()`** | 去 get_ 前缀 |
| `XBufferStats::print_stats()` | **`XBufferStats::print()`** | 简化 |

保持不变的 API:
- `make<T>()`, `root<T>()`, `has_root<T>()` — 核心 API 命名优秀
- `make_handle<T>()`, `handle<T>()` — 与核心 API 对称
- `stats()`, `used_size()`, `allocator<T>()` — 简洁明确
- `estimate_buffer_size()` — 位置和命名合理
- `is_xbuffer_safe<T>`, `validate_xbuffer_type<T>()` — 语义正确
- XBufferCore 继承方法（`grow`, `shrink_to_fit`, `epoch` 等）— 不改动底层

## Backward Compatibility

对被替换的方法保留 `[[deprecated]]` 别名。

## Risk Assessment

- **低风险**: 仅涉及 XBuffer 和 XCompactor 的方法名
- **影响范围**: 全部测试和示例文件需更新
- **构建验证**: 必须通过全部 25 个测试
