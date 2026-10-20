# 0.1.0-alpha.1 兼容与输入边界

此 alpha 面向固定编译器、依赖、schema 并自行验收的试用者。核心实现可用，
但 alpha 之间不承诺源码 API 或 ABI 稳定，也不承诺任意编译器、标准库、schema
演进与布局组合之间的长期 wire 兼容。本次版本包装不修改生产头或 V1 格式。

## 版本和工具链

`VERSION` 是完整版本 `0.1.0-alpha.1` 的唯一文本来源；CMake project 数字版本是
`0.1.0`。安装消费采用无版本查找，再检查完整字符串：

```cmake
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
if(NOT XOffsetDatastructure_VERSION STREQUAL "0.1.0-alpha.1")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 0.1.0-alpha.1")
endif()
```

storage 消费改用 `COMPONENTS storage` 和 `XOffset::storage`。
数字、EXACT 和版本范围查找全部拒绝，避免 alpha 被当作兼容的稳定版本。
源码构建中的完整版本变量是 `XOFFSET_VERSION_FULL`。

完整库要求 64-bit little-endian、P2996 Clang 与匹配 libc++；普通 C++26 不是替代品。
固定配方与依赖来源见源码分发中的 `docs/VM_PACKAGING.md`、`toolchains/README.md`
及 `toolchains/vm-validation.json`；这些配方不随安装 prefix 复制。
支持结论按具体 profile 给出，不能从 OS API 存在推导完整编译器支持。Windows 已验
storage，完整 P2996 尚未验证。WSL、Docker 的结果必须保留实际虚拟化/仿真类别。

同一可执行文件内应使用一致的生产头、TypeLayout/Boost 内容、工具链和编译选项。
`XOFFSET_DEFAULT_MAXIMUM_CAPACITY` 等宏在所有翻译单元中保持一致。
已验证的多翻译单元安装/源码消费不扩大为共享库、动态插件、跨 CRT、混合标准库
或混合头版本的 ABI 保证。

## V1 wire 的准入和兼容

V1 是当前固定格式，不提供通用 schema migration 或任意 ABI 转换。加载要求匹配的
root/schema/type/layout 身份及有效的 allocator/object graph；注册相同 schema 名称
并不能让不同布局兼容。需要持久化跨版本读取时，应保留能够读取原格式的工具链、
schema 与转换程序，并单独验证迁移结果。

- 存储类型必须满足 `boost::typelayout::is_byte_copy_safe_v<T>`。
- 固定 schema verified 路径还要求 `is_v1_wire_admitted_v<T>`；这是额外的 wire
  准入约束，不等于“所有可平凡复制的 C++ 类型均可用”。
- 使用 library 的 XString/XVector 等 arena 容器。普通进程地址、原生指针/引用、
  member pointer、union 不能进入此 admitted wire 域；普通 `std::string/std::vector`
  不是 offset 容器的替代品。
- 记录类型必须符合实际 admission 检查，包括 standard-layout、非多态、无基类、
  对齐不超过 8，且所有成员递归 admitted。数组、标量、枚举和容器元素也递归检查。
- 每种 root 用 `XOFFSET_REGISTER_SCHEMA_NAME(T, "唯一且稳定的名称")` 注册。
  不应把名称、单个 signature 或一次 roundtrip 当作任意目标平台的兼容证明。

仓库 `tools/sigs/` 的四目标签名是显式 wire catalog 与目标 ABI metadata 生成的
基线；它们不等于对应目标原生导出/运行证据，也不建立任意用户 schema 的兼容性。

`save()`/`save_verified()` 使用规范化 transport 语义：保存 admitted 对象图并支持
加载后继续变更，不承诺保留原 spare capacity 或 allocator 碎片形态。
save 会形成 owning byte copy；load 将输入复制到新 reservation。
这不是外部只读映射或通用零复制加载 API。

## Verified 不是认证协议

`TypedXBuffer<T>::load()` 使用 verified 路径；`XBuffer::load_verified<T>()` 在发布
typed root 前验证 wire、schema、allocator 和对象图。`save_verified()` 检查 typed
root/schema/allocator 并规范化输出，但不应被描述为完整对象图扫描或恶意输入认证。
`load_unverified()` 仅适合调用方已经信任的 raw payload。

格式中的 signature、FNV 和文件 SHA-256 清单用于身份/完整性核对，不提供发送者认证、
机密性、MAC 或数字签名。当前 V1 的 `crc32c` 字段写入并要求为 0，不能宣称它已经
提供 CRC 检验。若业务需要来源真实性或防篡改，应在外部协议中进行认证并保护对应
版本/schema/长度信息；库的 verified 标记不替代该协议。

接受不可信输入时，调用方须在分配/加载前限制总 wire 长度，并设置合理的
`XBuffer::maximum_capacity(bytes)`。另行限制并发 buffer 数、聚合内存/虚拟地址预算、
队列长度、执行时间及失败后的清理；单个 buffer 的合法性不能限制总业务资源消耗。
payload 上限为 `0x7ffffff8`，但这不是建议给每个请求预留的预算。

## 生命周期、增长和并发

成功创建/加载后的 arena base 在 grow、逻辑 shrink 和 save 中保持稳定；达到硬上限
或 OS 失败时不会转为搬迁分配。容器扩容、插入/擦除、赋值及替换仍可能移动或销毁
元素；不能据 arena base 稳定长期保存元素指针。compaction 创建独立 buffer。

容器对象本身应在所属 arena 内；传 native string/view 给插入接口，不要在栈上构造
绑定 arena 的 XString 临时对象。无 per-object delete，由 buffer 统一拥有对象。
`get_buffer()` 是借用视图；`copy_bytes()` 才是显式 owning copy。

`XHandle<T>` 跟随 C++ buffer 对象，不能延长它的生命周期；buffer 对象必须晚于 handle
销毁。move/swap/替换及类型/verified 状态变化遵循 handle 检查规则，不能把 handle
当作跨 owner 的 shared ownership。写入需调用方同步。

默认 Explicit 增长可能因当前逻辑区不足抛 `XBadAlloc`；Automatic 仍受硬上限与 OS
失败约束。库容器变更在分配失败时保留逻辑内容，但容量与 allocator bookkeeping
不必回滚；多步业务操作、普通 record assignment、外部构造副作用不因此成为事务。

默认 maximum 是 64 MiB，或较大的初始长度。`maximum_capacity()` 是硬上限；旧
`max_capacity(bytes)` 参数只是 reservation hint。`reserved_bytes()` 表示页舍入的
虚拟地址范围；`accessible_bytes()` 表示可读写页面，不是 RSS。逻辑 shrink 保留已启用
页面；显式 reclaim 可部分完成，应检查返回结果。pool 限额仅约束 idle reservation，
不限制 live owner 或全进程内存；policy/pool 不序列化。

## 本 alpha 不包含的承诺

生产异步 transport、真实业务网络整合、N7 三项 I/O 扩展与性能归因不属于此 alpha
承诺。现有网络验收是同步 TCP 回环参考及独立注入生命周期模型。
当前 WSL F/F 校准未取得预定的 2% 噪声资格，不能推导加速、等价、回退或候选 adoption。
后续 Linux 测量允许 WSL 或 Docker；仍须事前固定输入、记录环境并通过噪声资格。
本版本文档不改写既有历史实验的范围、失败证据或当时的启动条件。

## 直接接收数据的地址模型（W9）

直接接收（verified receive）得到的 buffer 与普通 load 一样，按平坦地址模型解析内部
相对偏移（`origin + delta`），依赖相同的、已披露的实现定义行为。库不再承诺接收数据的
指针由接收存储的字节数组派生；`XBufferCore::canonical_bytes_at` 作为公开接口保留以兼容
源码，但不再参与库内解析。
