# Change: Improve API Usability Based on Memory Lifecycle Audit

## Why
The `analyze-memory-lifecycle` audit identified 6 "Important" usability issues that create
significant friction or safety risks for library users. These issues range from missing
convenience APIs (XString assignment) to undocumented safety constraints (thread safety,
bulk deallocation semantics). Addressing them will reduce common mistakes and improve the
developer experience without changing the core zero-encoding architecture.

来源：`docs/MEMORY_LIFECYCLE_ANALYSIS.md` Part II & Part III 的审计发现。

## What Changes

### 1. XString 直接赋值 (U2.1) — 最高优先级
**现状**：赋值一个字符串需要手动构造临时 XString + allocator：
```cpp
player->name = XString("Alice", xbuf.allocator<XString>());
```
**目标**：支持直接赋值：
```cpp
player->name = "Alice";  // XString::operator=(const char*)
```
**实现**：XString 内部已持有 allocator（通过 offset_ptr<SM>），可直接调用 `assign()`。

### 2. make() 裸指针安全 (U1.2)
**现状**：`make<T>()` 返回 `T*`，grow/shrink 后悬挂，无编译期保护。
**目标**：提供 `XHandle<T>` 或类似的安全包装，在 buffer 操作后自动失效或重新查找。
**备选**：至少在 README 添加 "Critical Rules" 章节突出警告。

### 3. save_to_vector() 序列化优化 (U1.3)
**现状**：`save_to_string()` 全量拷贝含空闲区（4096B buffer 可能只有 500B 有效数据）。
**目标**：添加 `save_to_vector()` 返回 `std::vector<char>`，以及 `used_size()` 查询有效字节数。

### 4. Buffer 容量预估 (U2.3)
**现状**：用户无法预估初始 buffer 大小，只能靠猜测（如 4096）。
**目标**：提供 `estimate_buffer_size(user_data_bytes)` 辅助函数，考虑 segment header overhead。

### 5. Bulk Deallocation 文档 (C3.1)
**现状**：segment 释放时不调用对象析构函数，但 Domain S 约束保证安全。用户可能不理解。
**目标**：在头文件和 README 中明确文档化 "no individual destructor" 语义。

### 6. 线程安全文档 (C5.1)
**现状**：使用 `null_mutex_family`（零锁），但无任何文档警告多线程风险。
**目标**：在头文件和 README 中添加 "NOT thread-safe" 警告。

### 7. construct() 绕过安全门 (G1) — 正确性缺口
**现状**：`xbuf.construct<T>(name)(xbuf.get_segment_manager())` 绕过了 `validate_xbuffer_type<T>()` 检查。
`make()` 有此检查但底层 `construct()` 没有。测试中 7 处直接使用 `construct()` 创建对象。
**目标**：要么在 `XManagedMemory::construct()` 层面添加 `validate_xbuffer_type<T>()` 检查，
要么将 `construct()` 标记为内部 API 并在文档中警告用户使用 `make()` 替代。

### 8. XMap/XSet transparent comparator (G2) — 易用性
**现状**：每次 `map.find()` / `operator[]` 都需要构造临时 XString：
```cpp
data->scores[XString("Alice", xbuf.get_segment_manager())] = 95;
```
**目标**：支持 `const char*` 直接查找，无需构造临时 XString：
```cpp
data->scores["Alice"] = 95;
```
**实现**：使用 transparent comparator (`std::less<void>` 或自定义) + `XString` 的 `operator<(const char*)` 重载。

### 9. 命名对象删除 API (G3)
**现状**：`XBufferExt` 有 `make` / `find_ex` / `find_or_make`，但缺少对应的 `remove<T>(name)` 包装。
用户必须降级到底层 API：`xbuf.destroy<T>("name")`。
**目标**：添加 `remove<T>(name)` 方法保持 API 对称性。

### 10. shrink_to_fit() 性能优化 (G5)
**现状**：`shrink_to_fit()` 内部经历 3 次数据拷贝：
1. `base_t::shrink_to_fit()` — 段内压缩
2. `m_buffer.resize()` — 调整 vector 大小
3. `update_after_shrink()` — 新建 vector 拷贝 + swap（第三次拷贝）
**目标**：优化为 2 次拷贝，消除 `update_after_shrink()` 中不必要的额外拷贝。

### 11. static_assert 逐成员诊断 (G8)
**现状**：`validate_xbuffer_type<T>()` 失败时只说 "Type is unsafe"，不指出哪个成员有问题。
**目标**：利用 C++26 reflection 逐成员检查并在编译错误中指出具体不安全成员：
```
error: XBuffer Type Safety Error
  Member 'data' of type 'std::string' is UNSAFE. Use XString instead.
```

### 12. 单对象 Buffer 模型重构 (Breaking Change)
**现状**：API 要求每个对象有一个命名字符串 (`make<T>("name")`、`find<T>("name")`)。
代码审计显示 89% 的实际使用场景每个 buffer 只存一个根对象，`name` 参数成为纯噪音。
**目标**：消除 `name` 参数，改为单对象模型。每个 buffer 绑定一个类型 T，通过 `root()` 直接访问。
```cpp
// 旧 API
auto* player = buffer.make<Player>("player");
buffer.grow(8192);
player = buffer.find<Player>("player").first;  // 必须记名字
// 新 API
auto& player = buffer.make<Player>();
buffer.grow(8192);
auto& player = buffer.root();  // 无需名字
```
**BREAKING**: 所有 `make/find/construct` 的 `name` 参数被移除。~28 个文件需要更新。

## Impact
- Affected specs: `memory-lifecycle` (new capability from audit)
- Affected code: `xoffsetdatastructure2.hpp` (API redesign), ~28 test/example files
- Affected docs: `README.md` (Critical Rules + examples update)
- **BREAKING**: Yes. §12 removes `name` parameter from all public APIs.
