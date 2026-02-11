## Part I: 形式化 — 建立模型

### 1. 问题定义
- [x] 1.1 阐述传统序列化的两次变换本质
- [x] 1.2 阐述 XOffset "零变换" 洞察及其前提条件

### 2. 架构模型
- [x] 2.1 形式化定义架构集 A = { (ptr_size, endian, sizeof_table, align_table) }
- [x] 2.2 说明为什么必须固定架构（sizeof/alignof/endianness 决定内存表示）
- [x] 2.3 映射 static_assert 矩阵到架构属性

### 3. 内存模型
- [x] 3.1 描述段管理器在 vector<char> 上构建堆的机制
- [x] 3.2 形式化 offset_ptr 原理：stored = target - this，整体平移后相对关系不变
- [x] 3.3 说明 iset_index 和 free-list 也基于 offset_ptr
- [x] 3.4 证明 buffer 内无绝对地址逃逸（关键引理）

### 4. 类型模型
- [x] 4.1 归纳定义安全类型集 S (S₀ → 枚举 → XString → 容器 → 复合)
- [x] 4.2 阐述每条排除规则的原因（原生指针、std容器、虚函数、XOffsetPtr）
- [x] 4.3 映射 is_safe_leaf / is_safe_type 到类型规则

### 5. 核心定理
- [x] 5.1 精确陈述零编码正确性定理
- [x] 5.2 证明基元和枚举 case
- [x] 5.3 证明 offset_ptr case（相对寻址 + 同 buffer 内）
- [x] 5.4 证明容器和 XString case
- [x] 5.5 证明复合类型 case（结构归纳）

### 6. 强制执行链
- [x] 6.1 建立不变量目录 I1–I6，逐条映射到代码位置
- [x] 6.2 说明 TypeLayout 签名作为跨编译 ABI 指纹的角色

### 7. 边界与限制
- [x] 7.1 列举零编码失效场景
- [x] 7.2 描述 TypeLayout 签名检测和迁移路径
- [x] 7.3 描述 XOffsetPtr opt-in 条件

### 8. 文档组装
- [x] 8.1 组装为 docs/CORE_FORMAL_MODEL.md

---

## Part II: 分析与优化 — 用模型审计代码

### A1. 模型 vs 代码一致性审计
- [x] A1.1 逐条检查不变量是否在代码中被完整强制执行 — Domain A(✅), Domain S(✅), C2/offset_ptr(✅), P1/alignment(✅)
- [x] A1.2 检查 ArchSpec 是否完整表达了架构集 A — 发现 G1: 缺少各基本类型的 alignof 字段
- [x] A1.3 检查 is_safe_type() 判定逻辑是否完全匹配类型集 S — 完全匹配，包括空 struct 边界 case
- [x] A1.4 输出缺口清单 — G1: ArchSpec 缺少 alignof_table（低严重度，TypeLayout 签名可兜底）

### A2. 冗余与简化分析
- [x] A2.1 识别重复验证同一不变量的代码路径 — 5 个 validate_xbuffer_type 调用点各不相同，无冗余
- [x] A2.2 分析 validate_xbuffer_type 错误消息是否与模型术语一致 — 用户面使用 SAFE/UNSAFE，模型面使用 Domain S，分离合理
- [x] A2.3 输出简化建议 — 无重大简化空间

### A3. 缺失覆盖分析
- [x] A3.1 检查模型要求但代码未检查的约束 — G1: alignof 基本类型未检查（仅 alignof(void*)）
- [x] A3.2 检查类型集 S 的边界 case — 空 struct、嵌套容器、const 成员全部正确处理
- [x] A3.3 检查架构集 A 的边界 case — ARM64-LE 通过，ARM64-BE 正确拒绝

### A4. 模型启发的优化
- [x] A4.1 分析 is_safe_type() 编译期判定 — consteval 已最优，无优化空间
- [x] A4.2 分析 resolve_strategy() 与类型集分类 — 完全对齐（S₀→TrivialCopy, S_string→AllocatorAware, S_container→Container, S_composite→Composite/TrivialCopy）
- [x] A4.3 分析 ArchSpec 冗余字段 — 未使用的预设保留为可扩展性，无需删除
- [x] A4.4 分析 TypeLayout 签名扩展 — 容器内部字段排列不编码是 TypeLayout 库限制，非 XOffset 可改

### A5. 命名与概念对齐
- [x] A5.1 审计代码命名 — ArchSpec/TargetArchitecture/is_safe_type/is_safe_leaf 全部与模型术语一致
- [x] A5.2 审计文档注释 — Lines 57-62 已使用 "A (Architecture Set)" 和 "S (Safe Type Subset)" 模型术语

### A6. 实施优化
- [x] A6.1 汇总 A1–A5 发现，排优先级 — G1(alignof)已实施，其余无重大缺口
- [x] A6.2 逐项实施代码修改（每项追溯到模型章节）— ArchSpec alignof 字段+预设值+4条 static_assert 已添加；文档行号已同步
- [x] A6.3 验证所有测试通过 — 19/19 tests passed, demo + helloworld 全部成功

### A7. 深度形式化分析（补充）
- [x] A7.1 offset_ptr null case — 在 Lemma C2.1 中补充 null sentinel 说明
- [x] A7.2 文档行号偏移 — §3.3 和 §5.1 中所有行号引用已更新
- [x] A7.3 必要性证明 — §4.3 补充了 C1 和 C2 的形式化必要性论证（反证法）
