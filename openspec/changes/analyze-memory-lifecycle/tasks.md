## Part I: 逐行内存生命周期分析

### 1. Buffer Creation
- [ ] 1.1 Trace XManagedMemory(4096): vector<char> allocation + create_impl internals
- [ ] 1.2 Document segment manager header layout and overhead

### 2. Object Construction
- [ ] 2.1 Trace make<Player>("Hero"): iset_index entry + placement new in segment
- [ ] 2.2 Trace Player(allocator): XString/XVector default construction with allocator
- [ ] 2.3 Trace trivial scalar assignment (id, level)

### 3. Temporary Object & Move/Copy Semantics
- [ ] 3.1 Trace XString("Alice", allocator): stack temporary with segment-allocated char data
- [ ] 3.2 Trace allocator type conversion: allocator<XString,SM> → allocator<char,SM> (rebind)
- [ ] 3.3 Trace move-assignment player->name = <temporary>: offset_ptr recalculation
- [ ] 3.4 Trace temporary destruction: moved-from state
- [ ] 3.5 Document offset_ptr behavior across stack↔segment boundary

### 4. Data Mutation & Reallocation
- [ ] 4.1 Trace push_back(101): initial vector allocation from free-list
- [ ] 4.2 Trace push_back triggering reallocation: 1.1x growth, old block → free-list
- [ ] 4.3 Trace pop_back: size decrement only, no memory return
- [ ] 4.4 Document free-list fragmentation created by reallocation cycles

### 5. Serialization & Deserialization
- [ ] 5.1 Trace save_to_string: byte-for-byte copy
- [ ] 5.2 Trace load_from_string: vector<char> construction → move → open_impl
- [ ] 5.3 Document open_impl segment rediscovery
- [ ] 5.4 Trace NRVO on load_from_string return path
- [ ] 5.5 Trace find_ex: iset_index lookup via offset_ptr chain

### 6. Compaction & Migration
- [ ] 6.1 Trace compact_automatic: new segment creation
- [ ] 6.2 Trace migrate_member TrivialCopy / AllocatorAware / Container
- [ ] 6.3 Trace shrink_to_fit + update_after_shrink
- [ ] 6.4 Trace return value: NRVO / move

### 7. Memory Reclamation
- [ ] 7.1 Trace destructor chain: ~XManagedMemory → destroy_impl → vector swap
- [ ] 7.2 Document bulk deallocation vs individual object destruction

### 8. Cross-cutting & Diagrams
- [ ] 8.1 Document allocator propagation in Boost.Container
- [ ] 8.2 Document x_seq_fit vs x_best_fit fragmentation
- [ ] 8.3 Draw ASCII memory layout diagrams for all 7 phases
- [ ] 8.4 Assemble into docs/MEMORY_LIFECYCLE_ANALYSIS.md

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