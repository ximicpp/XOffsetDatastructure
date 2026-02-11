## Part I: 逐行内存生命周期分析

### File 1: player.hpp — 数据结构定义
- [ ] F1.1 分析 `alignas(8)` 对 Player 在 buffer 内布局的影响（padding 位置、sizeof）
- [ ] F1.2 分析 template allocator 构造函数 `Player(Allocator allocator)` 的机制：rebind 链、allocator 传播到 XString/XVector 成员
- [ ] F1.3 分析 Full constructor `Player(allocator, id, level, name)` 的成员初始化顺序与段内分配时序
- [ ] F1.4 分析 TypeLayout `static_assert` 签名验证：签名字符串各字段含义、编译期 ABI 指纹工作原理
- [ ] F1.5 分析 `XVector<int32_t> items` 成员：S₀ 元素的容器安全检查路径

### File 2: game_data.hpp — 嵌套复合类型定义
- [ ] F2.1 分析 Item 的成员布局：3 个 int32_t + XString 的 padding 分析（`@16[name]` 为何不是 `@12`）
- [ ] F2.2 分析 GameData 的成员布局：6 个字段的 offset 链（0, 4, 8, 16, 48, 80, 112）
- [ ] F2.3 分析嵌套容器 `XVector<Item>` 的安全类型递归检查路径：is_safe_type<XVector<Item>> → is_safe_type<Item> → 逐成员检查
- [ ] F2.4 分析 `XMap<XString, int32_t>` 的类型安全检查：key 和 value 分别检查
- [ ] F2.5 分析 GameData 的 TypeLayout 签名：嵌套 record 签名的递归编码
- [ ] F2.6 分析 GameData allocator 构造函数中 4 个容器成员的 allocator 传播

### File 3: helloworld.cpp — 完整生命周期
#### Phase 1 — Buffer Creation
- [ ] H1.1 Trace XBufferExt(4096): vector<char> allocation + create_impl internals
- [ ] H1.2 Document segment manager header layout and overhead

#### Phase 2 — Object Construction
- [ ] H2.1 Trace make<Player>("Hero"): validate_xbuffer_type + iset_index entry + placement new
- [ ] H2.2 Trace Player(allocator): XString/XVector default construction with allocator
- [ ] H2.3 Trace trivial scalar assignment (id, level)

#### Phase 3 — Temporary Object & Move/Copy Semantics
- [ ] H3.1 Trace XString("Alice", allocator): stack temporary with segment-allocated char data
- [ ] H3.2 Trace allocator type conversion: allocator<XString,SM> → allocator<char,SM> (rebind)
- [ ] H3.3 Trace move-assignment player->name = <temporary>: offset_ptr recalculation
- [ ] H3.4 Trace temporary destruction: moved-from state
- [ ] H3.5 Document offset_ptr behavior across stack↔segment boundary

#### Phase 4 — Data Mutation & Reallocation
- [ ] H4.1 Trace push_back(101): initial vector allocation from free-list
- [ ] H4.2 Trace push_back triggering reallocation: 1.1x growth, old block → free-list
- [ ] H4.3 Trace pop_back: size decrement only, no memory return
- [ ] H4.4 Document free-list fragmentation created by reallocation cycles

#### Phase 5 — Serialization & Deserialization
- [ ] H5.1 Trace save_to_string: byte-for-byte copy
- [ ] H5.2 Trace load_from_string: vector<char> construction → move → open_impl
- [ ] H5.3 Document open_impl segment rediscovery
- [ ] H5.4 Trace NRVO on load_from_string return path
- [ ] H5.5 Trace find_ex: iset_index lookup via offset_ptr chain

#### Phase 6 — Compaction & Migration
- [ ] H6.1 Trace compact_automatic: new segment creation
- [ ] H6.2 Trace migrate_member TrivialCopy / AllocatorAware / Container
- [ ] H6.3 Trace shrink_to_fit + update_after_shrink
- [ ] H6.4 Trace return value: NRVO / move

#### Phase 7 — Memory Reclamation
- [ ] H7.1 Trace destructor chain: ~XManagedMemory → destroy_impl → vector swap
- [ ] H7.2 Document bulk deallocation vs individual object destruction

### File 4: demo.cpp — 7 个 Demo 函数
#### Demo 1: demo_basic_usage (lines 53–115)
- [ ] D1.1 Trace make<GameData>("player_save"): 复合类型命名对象构造（对比 helloworld 的 Player）
- [ ] D1.2 Trace `game->items.emplace_back(allocator, ...)`: 嵌套容器（XVector<Item>）的段内元素构造
- [ ] D1.3 分析 Item 的 emplace_back 路径：allocator 传播 → Item full constructor → XString 段内分配
- [ ] D1.4 Trace `game->achievements.insert(i)`: flat_set 有序插入的内存分配（底层 x_vector_impl 的 growth）
- [ ] D1.5 Trace `game->quest_progress[XString(...)] = 75`: flat_map operator[] 的键值对插入路径
- [ ] D1.6 分析 XString key 在 flat_map 中的段内存储：XString 临时对象 → move into pair → 段内

#### Demo 2: demo_memory_management (lines 121–155)
- [ ] D2.1 Trace xbuf.grow(4096): resize + close_impl + open_impl + base_t::grow 的完整路径
- [ ] D2.2 分析 grow 后 `game` 指针是否仍然有效（open_impl 重建后地址可能不变，但不保证）
- [ ] D2.3 Trace xbuf.shrink_to_fit(): base_t::shrink_to_fit + resize + update_after_shrink 的双重重建
- [ ] D2.4 分析 shrink_to_fit 后所有旧指针失效的影响链

#### Demo 3: demo_serialization (lines 161–204)
- [ ] D3.1 Trace 完整数据流：src_buf → save_to_string → binary_data(std::string) → load_from_string → dst_buf
- [ ] D3.2 分析 dst_buf.find<GameData>("save"): 反序列化后 iset_index 查找路径（对比 helloworld 的 find_ex）
- [ ] D3.3 分析跨 buffer 数据完整性验证的正确性

#### Demo 4: demo_type_signatures (lines 210–245)
- [ ] D4.1 分析 `get_definition_signature<Item>()` 的编译期求值路径
- [ ] D4.2 分析 signature.value 的运行时输出与编译期验证的关系

#### Demo 5: demo_automatic_compaction (lines 251–335)
- [ ] D5.1 Trace compact_automatic<GameData>: 全量迁移路径（对比 helloworld 的 Player 迁移）
- [ ] D5.2 分析 XVector<Item> 递归迁移：Container 策略 → 逐元素 → Item 含 XString → AllocatorAware
- [ ] D5.3 分析 XSet<int32_t> 迁移：TrivialCopy 策略（元素是 S₀）
- [ ] D5.4 分析 XMap<XString, int32_t> 迁移：key(AllocatorAware) + value(TrivialCopy)
- [ ] D5.5 分析 compaction 前后 free-list 碎片消除效果

#### Demo 6: demo_performance (lines 341–390)
- [ ] D6.1 分析 1000 次 emplace_back 的 1.1x growth 触发次数（理论值 vs 实际）
- [ ] D6.2 分析每次 growth 产生的 free-list 碎片大小
- [ ] D6.3 分析 65536 字节初始 buffer 是否足够 1000 个 Item（估算每个 Item 的段内开销）

#### Demo 7: demo_advanced_features (lines 396–432)
- [ ] D7.1 此 demo 仅输出文本无内存操作，标注为 "无分析需求"

### Cross-cutting & Diagrams
- [ ] X1 Document allocator propagation in Boost.Container（rebind 机制）
- [ ] X2 Document x_seq_fit vs x_best_fit fragmentation behavior
- [ ] X3 Draw ASCII memory layout diagrams for key phases (创建、填充、序列化、compaction)
- [ ] X4 Document offset_ptr mechanics across stack↔segment boundary
- [ ] X5 Assemble into docs/MEMORY_LIFECYCLE_ANALYSIS.md

---

## Part II: 正确性问题审计

### C1. 指针/引用有效性
- [ ] C1.1 审计 grow() 后旧指针失效的风险和文档
- [ ] C1.2 审计 shrink_to_fit() 指针失效链完整性
- [ ] C1.3 审计 compact_automatic 后旧 buffer 指针使用风险

### C2. 异常安全
- [ ] C2.1 审计 XString 临时对象构造失败时的资源泄漏
- [ ] C2.2 审计 push_back 分配失败时 vector 状态一致性
- [ ] C2.3 审计 compact_automatic 中途失败时的 buffer 状态

### C3. 析构完整性
- [ ] C3.1 审计 segment 释放时对象析构函数是否被调用
- [ ] C3.2 审计 XString/XVector 析构是否依赖有效 segment manager

### C4. 跨段操作
- [ ] C4.1 审计 migrate_container 跨段赋值的正确性
- [ ] C4.2 审计栈上临时 XString（allocator 指向 segment）的析构安全性
- [ ] C4.3 审计跨段 allocator 比较对 move/copy 路径的影响

### C5. 并发安全
- [ ] C5.1 审计 null_mutex_family 是否有足够的文档警告

---

## Part III: 易用性改进分析

### U1. API 陷阱
- [ ] U1.1 分析 allocator<XString>() 返回类型的直观性
- [ ] U1.2 分析 make() 返回裸指针的失效风险
- [ ] U1.3 分析 save_to_string() 的拷贝开销

### U2. 缺失便利接口
- [ ] U2.1 分析 XString 直接赋值的可能性（如 name = "Alice"）
- [ ] U2.2 分析 range-based push_back 的可能性
- [ ] U2.3 分析 buffer 容量预估辅助的需求

### U3. 错误诊断
- [ ] U3.1 分析 segment 空间不足时的错误消息
- [ ] U3.2 分析 static_assert 是否指出具体不安全成员
- [ ] U3.3 分析 grow() 失败时的诊断信息

### U4. 文档与心智模型
- [ ] U4.1 评估用户是否需要理解 offset_ptr
- [ ] U4.2 评估指针失效约束的文档可见度

---

## Part IV: 汇总与实施

- [ ] I1 将所有发现分类为 Critical / Important / Nice-to-have
- [ ] I2 Critical 正确性问题直接修复
- [ ] I3 Important 易用性问题创建后续 proposal
- [ ] I4 验证所有测试通过