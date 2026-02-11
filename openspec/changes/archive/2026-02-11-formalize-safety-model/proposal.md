# Change: Formalize the Core Framework Model and Optimize Implementation

## Why
XOffsetDatastructure2 的核心主张是：**在特定架构和类型约束下，直接内存拷贝等价于正确的序列化/反序列化**。这不只是一个"安全"特性，而是整个框架存在的根本原因。

当前这个核心模型散落在代码注释、static_assert 和 docs 的各处片段中，缺少一个统一的、自顶向下的形式化描述。更重要的是，**没有形式化就无法系统性地审视现有实现是否完备、是否存在冗余、是否有优化空间**。

本提案分两阶段：
1. **形式化**：建立严谨的核心模型（架构、内存、类型、定理、不变量）
2. **分析与优化**：用形式化模型作为标尺，审计现有代码，发现并修复以下问题：
   - 模型与代码的不一致（声明了但未强制执行的不变量）
   - 冗余的检查（多处重复验证同一不变量）
   - 缺失的检查（模型要求但代码未覆盖的约束）
   - 模型启发的优化（如：类型集 S 的判定是否可以更高效？）
   - 命名和结构是否准确反映模型概念（如：ArchSpec 是否完整表达架构集 A？）

## What Changes

### Part I: 形式化 — 建立模型

产出 `docs/CORE_FORMAL_MODEL.md`，覆盖 7 个层次：

**§1 — 问题定义：为什么传统序列化是多余的**
- 传统序列化的本质：内存表示 → 传输格式 → 内存表示（两次变换）
- XOffset 的洞察：如果内存表示本身就是传输格式，变换次数 = 0
- 前提条件：必须约束架构和类型，使得"内存表示在不同时间/地点保持一致"

**§2 — 架构模型 (Architecture Model)**
- 架构集 A 的形式化定义：A = { (ptr_size, endian, sizeof_table, align_table) }
- 当前实例：A = { Arch64LE }
- 架构谓词：CurrentPlatform ∈ A，由 static_assert 强制
- 为什么需要固定架构：sizeof、alignof、endianness 决定了内存表示

**§3 — 内存模型 (Memory Model)**
- 段管理器 (Segment Manager)：在 vector<char> 上构建的堆
- offset_ptr：相对寻址的核心原理 — `stored_value = target_addr - this_addr`
- 为什么 offset_ptr 使内存拷贝正确：整体平移后相对关系不变
- 命名对象索引 (iset_index) 和 free-list：也基于 offset_ptr

**§4 — 类型模型 (Type Model)**
- 安全类型集 S 的归纳定义
- 排除规则及原因（原生指针、std 容器、虚函数、XOffsetPtr）

**§5 — 核心定理：零编码正确性**
- 定理声明 + 按类型层次归纳证明
- 关键引理：Buffer 内无绝对地址逃逸

**§6 — 强制执行链 (Enforcement Chain)**
- 不变量目录 I1–I6 → 每条映射到代码位置

**§7 — 边界与限制**
- 零编码失效场景 + TypeLayout 检测 + 迁移路径

### Part II: 分析与优化 — 用模型审计代码

基于 Part I 建立的形式化模型，系统性审计现有代码：

**A1 — 模型 vs 代码一致性审计**
- 逐条检查不变量 I1–I6 是否在代码中被完整强制执行
- 检查 ArchSpec 是否完整表达了架构集 A 的所有属性
- 检查 is_safe_type() 的判定逻辑是否完全匹配类型集 S 的归纳定义
- 找出"模型说了但代码没做"的缺口

**A2 — 冗余与简化分析**
- 识别重复验证同一不变量的代码路径
- 分析 validate_xbuffer_type 的 static_assert 消息是否与模型术语一致
- 评估错误消息是否引用了正确的模型概念

**A3 — 缺失覆盖分析**
- 模型中定义但代码中未检查的约束（如：是否检查了 padding 一致性？）
- 类型集 S 的边界 case（如：空 struct、嵌套容器、自引用结构）
- 架构集 A 的边界 case（如：ARM64 little-endian 是否也属于 Arch64LE？）

**A4 — 模型启发的优化**
- is_safe_type() 递归判定是否有编译期缓存/剪枝的空间
- 迁移策略 resolve_strategy() 是否可以更直接地从类型集分类推导
- ArchSpec 中是否有冗余字段（如 sizeof_bool, sizeof_char 是否永远是 1？）
- TypeLayout 签名是否可以编码更多模型信息（如不变量违规的具体位置）

**A5 — 命名与概念对齐**
- 代码中的命名是否准确反映模型概念
- 文档注释是否使用一致的模型术语
- 新用户能否通过命名理解设计意图

**A6 — 实施优化**
- 根据 A1–A5 的发现，实施代码修改
- 每个修改都追溯到模型中的具体章节

## Impact
- Affected specs: `core-model` (new capability)
- Affected code: `xoffsetdatastructure2.hpp` (优化和修复), docs
- New artifact: `docs/CORE_FORMAL_MODEL.md`
- Potential code changes from Part II analysis