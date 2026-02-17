# Proposal: mmap vs std::vector 存储后端分析

> **Status**: ✅ Implemented — Strategy D (std::vector + reserve + address detection + re-reserve)  
> **Date**: 2026-02-17  
> **Related**: P3 mmap backend (removed), PROPOSAL_ADAPTIVE_MMAP_RESERVATION (superseded)

---

## 1. 两种方案的实现对比

### 1.1 存储层

| 维度 | std::vector\<char\>（原始方案） | VirtualMemoryBuffer / mmap（当前方案） |
|------|-------------------------------|--------------------------------------|
| **内存分配** | malloc/realloc（堆） | mmap PROT_NONE 保留 + mprotect 按需提交 |
| **物理内存** | resize 多少就消耗多少 | committed 页面才消耗，但按页对齐（macOS 16KB / Linux 4KB） |
| **虚拟地址空间** | 仅占用 size() 大小 | 保留 reserved 大小（自适应: initial×16, 最大 256MB） |
| **代码量** | 0 行（标准库） | ~200 行自定义代码（mmap/mprotect/madvise/munmap） |
| **平台依赖** | 无（C++ 标准） | Unix/macOS（`<sys/mman.h>`），需 fallback 给其他平台 |

### 1.2 grow() 行为

| 维度 | std::vector（原始） | mmap（当前） |
|------|-------------------|-------------|
| **地址稳定性** | ❌ 每次 grow 都可能 relocate | ✅ 快路径不变；慢路径 remap（超出保留时） |
| **实现复杂度** | resize → close_impl → open_impl → grow | 快路径: mprotect → grow；慢路径: mmap+memcpy+munmap → close+open → grow |
| **失败恢复** | try/catch + resize 回滚（6 行） | 返回 false（1 行） |
| **epoch 变化** | 每次 grow **都** 递增 epoch | 快路径**不递增**；慢路径递增 |
| **XHandle 影响** | 每次 grow 后 O(log n) 重查 | 快路径 O(1) 直接命中；慢路径 O(log n) 重查 |

**原始 grow() 代码**（`std::vector`，31 行）:
```cpp
bool grow(size_type extra_bytes) {
    const size_type original_size = m_buffer.size();
    try {
        m_buffer.resize(original_size + extra_bytes);      // 可能 relocate
        base_t::close_impl();
        if (!base_t::open_impl(&m_buffer[0], m_buffer.size())) {
            m_buffer.resize(original_size);                // 回滚
            base_t::open_impl(&m_buffer[0], m_buffer.size());
            return false;
        }
        base_t::grow(extra_bytes);
        ++m_epoch;                                         // 总是递增
        return true;
    } catch(...) {
        try { m_buffer.resize(original_size); } catch(...) {}
        try { base_t::open_impl(&m_buffer[0], m_buffer.size()); } catch(...) {}
        return false;
    }
}
```

**当前 grow() 代码**（mmap，25 行）:
```cpp
bool grow(size_type extra_bytes) {
    size_type old_size = m_buffer.size();
    bool address_changed = false;
    if (!m_buffer.grow(extra_bytes, &address_changed))
        return false;
    if (!address_changed) {
        base_t::grow(extra_bytes);                         // 快路径
    } else {
        base_t::close_impl();                              // 慢路径
        if (!base_t::open_impl(m_buffer.data(), old_size)) {
            throw interprocess_exception("...");
        }
        base_t::grow(extra_bytes);
        ++m_epoch;
    }
    return true;
}
```

### 1.3 shrink_to_fit() 行为

| 维度 | std::vector（原始） | mmap（当前） |
|------|-------------------|-------------|
| **操作** | vector::resize → close_impl → open_impl | madvise + mprotect 尾部页面 |
| **地址变化** | ❌ vector 可能 relocate | ✅ 基地址不变 |
| **epoch** | ++m_epoch（地址可能变） | 不递增（地址不变） |
| **内存释放** | 精确到字节（resize） | 按页释放（madvise DONTNEED） |

### 1.4 save() / load()

| 维度 | std::vector（原始） | mmap（当前） |
|------|-------------------|-------------|
| **save 输出** | vector::begin/end（精确字节） | segment_size()（精确字节，修复后） |
| **save 副作用** | ⚠️ 可能使指针失效（shrink relocate） | ✅ 指针不失效 |
| **load 构造** | vector 拷贝 | mmap 保留 + memcpy |

### 1.5 内存效率

| 场景 | std::vector | mmap |
|------|-----------|------|
| **XBuffer(4096)** | 精确 4096 bytes 堆内存 | 16384 bytes committed（macOS 页对齐） |
| **XBuffer(4096) ×10000** | ~40MB 堆 + 0 虚拟浪费 | ~160MB committed + 625MB 虚拟保留 |
| **grow(100)** | realloc，可能碎片化 | mprotect，无碎片 |
| **shrink 后** | 精确释放 | 按页释放，可能保留多余页 |

---

## 2. 各方案的根本性问题

### 2.1 std::vector 的根本问题

**每次 grow 必须 close_impl + open_impl**:
```
vector::resize() → 可能 relocate → 旧地址失效
→ 必须 close_impl() 关闭旧段
→ open_impl(new_addr, new_size) 重新打开
→ ++m_epoch → XHandle 失效
```

这不是 bug，是 `std::vector` 的结构性限制——它不保证地址稳定。每次 grow 都是 O(log n) 重查。

### 2.2 mmap 的根本问题

1. **平台依赖**: `mmap`/`mprotect` 是 POSIX API，Windows 需用 `VirtualAlloc`/`VirtualProtect`（未实现），WASM 无对应 API
2. **页对齐浪费**: committed 按页对齐（macOS 16KB），小 buffer 效率低
3. **代码复杂度**: 200+ 行平台相关代码（vs vector 的 0 行），需要维护两条路径
4. **虚拟空间占用**: 即使是自适应策略（64KB~256MB），10K 实例仍需 625MB 虚拟空间
5. **remap 慢路径**: 需要 close_impl + open_impl，与 vector 方案本质相同

### 2.3 关键洞察

**mmap 的核心价值只在快路径**: grow 在保留范围内时地址不变，XHandle O(1)。一旦触发 remap，退化为与 vector 完全相同的行为（close + open + epoch++）。

**问题是快路径的实际触发率**: 
- 如果用户很少 grow，mmap 的优势无法体现
- 如果用户频繁 grow 但总量不大，快路径覆盖率高，mmap 有优势
- 如果用户 grow 超出保留量，退化为慢路径，无优势

---

## 3. 方案评估

### 方案 A: 纯 std::vector（回退到原始方案）

**优点**:
- 零平台依赖，所有 C++ 平台可用
- 代码最简单，最容易维护
- 内存精确到字节，无页对齐浪费
- 无虚拟空间保留开销

**缺点**:
- 每次 grow 可能使指针失效
- XHandle 每次 grow 后 O(log n) 重查
- save() 可能使指针失效（shrink relocate）

### 方案 B: 纯 mmap（当前方案）

**优点**:
- grow 快路径地址稳定
- XHandle 在快路径下 O(1) 命中
- save() 不使指针失效
- shrink 不使指针失效

**缺点**:
- 200+ 行平台代码
- 需要 fallback（已有 std::vector fallback）
- 页对齐浪费（小 buffer 效率低）
- 虚拟空间占用
- remap 慢路径与 vector 方案无差异

### 方案 C: std::vector + realloc 优化（中间方案）

使用 `std::vector` 但在 grow 时不做 close+open，而是检测地址是否变化：

```cpp
bool grow(size_type extra_bytes) {
    char* old_addr = m_buffer.data();
    m_buffer.resize(m_buffer.size() + extra_bytes);
    if (m_buffer.data() == old_addr) {
        // vector 没有 relocate → 地址稳定
        base_t::grow(extra_bytes);
        // 不递增 epoch
    } else {
        // vector relocate 了 → 需要 reopen
        base_t::close_impl();
        base_t::open_impl(m_buffer.data(), old_size);
        base_t::grow(extra_bytes);
        ++m_epoch;
    }
    return true;
}
```

**优点**:
- 零平台依赖
- 代码极简（比原始方案还简单）
- 当 vector 不 relocate 时（容量足够），享受与 mmap 相同的 O(1) XHandle
- 内存精确到字节
- 无虚拟空间浪费

**缺点**:
- vector 的 relocate 概率比 mmap 的 remap 高（vector capacity 用完就 relocate）
- 无法通过 reserve() 主动控制快路径范围（实际上可以：`m_buffer.reserve(N)` 等效于 mmap 的预留）
- save/shrink 仍可能 relocate（但也可用同样的地址检测逻辑优化）

### 方案 C 的关键改进

可以在构造时 `m_buffer.reserve(initial_size * 16)` 模拟自适应保留:

```cpp
XManagedMemory(size_type size)
    : m_buffer(size, char(0))
{
    // 预留空间，等效于 mmap 的虚拟保留
    m_buffer.reserve(std::min(size * 16, MAX_RESERVE));
    // ...
}
```

这样 grow 在 reserve 范围内时 vector 不会 relocate → 地址稳定 → epoch 不递增。

---

## 4. 量化对比

| 指标 | A (纯 vector) | B (纯 mmap) | C (vector + 地址检测) |
|------|-------------|------------|---------------------|
| 平台支持 | ✅ 全平台 | ⚠️ Unix/macOS | ✅ 全平台 |
| 新增代码量 | 0 行 | ~200 行 | ~10 行 |
| grow 快路径概率 | 0%（总是 reopen） | ~95%（保留范围内） | ~90%（reserve 范围内） |
| 页对齐浪费 | 无 | macOS 最多 16KB | 无 |
| 虚拟空间 (10K 实例) | 仅 used | 625MB 保留 | 仅 used + reserve |
| 小 buffer (512B) 效率 | 512B 精确 | 16KB committed | 512B + 8KB reserved |
| XHandle O(1) 覆盖 | ❌ 从不 | ✅ 快路径 | ✅ reserve 范围内 |
| save 指针失效 | ⚠️ 可能 | ✅ 不失效 | ⚠️ 可能（可优化） |
| 维护复杂度 | 极低 | 高（两条路径） | 低 |

---

## 5. 实测数据（macOS arm64, 36GB）

1000×64KB 分配的物理内存 (RSS) 变化：

| 方式 | RSS 增量 | 等效场景 |
|------|---------|---------|
| malloc 不触碰 | **+0.0 MB** | vector::reserve() |
| malloc + memset | **+62.5 MB** | vector::resize(N, 0) |
| mmap PROT_NONE | **+0.0 MB** | 当前 mmap 保留 |
| mmap RW + memset | **+62.5 MB** | mmap committed 页面 |

虚拟地址空间消耗：
- malloc×10K: +640 MB VSZ
- mmap×10K: +640 MB VSZ

**关键发现**: `malloc(不触碰)` 和 `mmap(PROT_NONE)` 的物理内存消耗完全相同 = **0**。

---

## 6. 各方案详细设计

### 方案 A: 纯 std::vector（原始方案，无优化）

```cpp
class XManagedMemory {
    std::vector<char> m_buffer;

    XManagedMemory(size_type size)
        : m_buffer(size, char(0))    // resize: 消耗 size 物理内存
    {
        base_t::create_impl(m_buffer.data(), size);
    }

    bool grow(size_type extra) {
        auto old_size = m_buffer.size();
        m_buffer.resize(old_size + extra);  // 每次可能 relocate
        base_t::close_impl();
        base_t::open_impl(m_buffer.data(), m_buffer.size());
        base_t::grow(extra);
        ++m_epoch;                          // 总是递增
        return true;
    }

    void shrink_to_fit() {
        base_t::shrink_to_fit();
        m_buffer.resize(base_t::get_size());  // 可能 relocate
        base_t::close_impl();
        base_t::open_impl(m_buffer.data(), m_buffer.size());
        ++m_epoch;
    }
};
```

| 指标 | 值 |
|------|---|
| 代码量 | 0 行新增 |
| grow 地址稳定 | ❌ 从不保证 |
| epoch 递增 | 每次 grow/shrink |
| XHandle O(1) | ❌ 从不 |
| 内存精度 | 精确到字节 |
| 平台依赖 | 无 |
| save 副作用 | ⚠️ 指针失效 |

---

### 方案 B: mmap（当前方案）

```cpp
class VirtualMemoryBuffer {
    char* m_base;          // mmap 固定基地址
    size_t m_reserved;     // PROT_NONE 保留量
    size_t m_committed;    // PROT_READ|PROT_WRITE 已提交

    VirtualMemoryBuffer(size_t init)
        : VirtualMemoryBuffer(init, compute_reservation(init)) {}

    VirtualMemoryBuffer(size_t init, size_t max_reserved) {
        m_reserved = round_up(max_reserved, page_size);
        m_base = mmap(nullptr, m_reserved, PROT_NONE, ...);  // 0 物理内存
        mprotect(m_base, round_up(init, page_size), RW);     // 提交 init 页
        memset(m_base, 0, committed);                         // 消耗物理页
    }

    bool grow(size_t extra, bool* changed) {
        if (committed + extra <= reserved) {
            mprotect(...);   // 快路径：提交新页面，地址不变
            return true;
        }
        return grow_remap();  // 慢路径：新 mmap + memcpy + munmap
    }

    void shrink(size_t sz) {
        madvise(MADV_DONTNEED);  // 释放物理页面
        mprotect(PROT_NONE);    // 地址不变
    }
};
```

| 指标 | 值 |
|------|---|
| 代码量 | ~200 行 + fallback |
| grow 地址稳定 | ✅ 快路径保证，慢路径 remap |
| epoch 递增 | 仅 remap 时 |
| XHandle O(1) | ✅ 快路径下 |
| 内存精度 | 按页对齐（macOS 16KB 粒度） |
| 平台依赖 | Unix/macOS，需 fallback |
| save 副作用 | ✅ 指针不失效 |
| shrink 释放物理页 | ✅ madvise（地址不变） |
| 虚拟空间保留 | 64KB~256MB per buffer |
| 保护模式 | PROT_NONE 防误访问 |

---

### 方案 C: std::vector + 地址检测 + reserve

```cpp
class XManagedMemory {
    std::vector<char> m_buffer;

    XManagedMemory(size_type size)
        : m_buffer(size, char(0))
    {
        // 预留空间 = 自适应策略，与 mmap 等效
        size_t reserve = compute_reservation(size);
        m_buffer.reserve(reserve);    // 0 物理内存（未触碰页面）
        base_t::create_impl(m_buffer.data(), size);
    }

    bool grow(size_type extra) {
        char* old_addr = m_buffer.data();
        size_type old_size = m_buffer.size();
        m_buffer.resize(old_size + extra);

        if (m_buffer.data() == old_addr) {
            // 快路径：reserve 范围内，地址不变
            base_t::grow(extra);
            // 不递增 epoch
        } else {
            // 慢路径：vector relocate 了
            base_t::close_impl();
            base_t::open_impl(m_buffer.data(), old_size);
            base_t::grow(extra);
            ++m_epoch;
        }
        return true;
    }

    void shrink_to_fit() {
        base_t::shrink_to_fit();
        // 不调用 vector::resize/shrink_to_fit
        // 避免 relocate，保持地址稳定
        // save() 用 segment_size() 获取精确大小
    }

    size_type segment_size() const {
        return base_t::get_size();
    }
};
```

| 指标 | 值 |
|------|---|
| 代码量 | ~15 行改动（净减少 ~185 行） |
| grow 地址稳定 | ✅ reserve 范围内保证（C++标准保证） |
| epoch 递增 | 仅 relocate 时 |
| XHandle O(1) | ✅ reserve 范围内 |
| 内存精度 | 精确到字节 |
| 平台依赖 | **无** |
| save 副作用 | ✅ 指针不失效（不做 vector shrink） |
| shrink 释放物理页 | ❌ 不释放（保持 vector 容量） |
| 虚拟空间保留 | 与 malloc 实现相关（实测≈mmap） |
| 保护模式 | 无（reserved 内存可读写） |

**方案 C 的 save() 设计**:

```cpp
std::string save() {
    base_t::shrink_to_fit();   // 只更新 rbtree 内部逻辑大小
    // 不调用 m_buffer 的 shrink — 避免 relocate
    const char* base = m_buffer.data();
    size_t exact = base_t::get_size();  // rbtree 精确逻辑大小
    return std::string(base, exact);
}
```

---

## 7. 是否存在完美方案？

### 7.1 "完美"的定义

| 需求 | 理想值 |
|------|--------|
| 地址稳定 | grow 永不改变基地址 |
| 零平台依赖 | 纯 C++ 标准 |
| 零内存浪费 | 精确到字节，无 padding |
| 零代码量 | 无需自定义存储层 |
| 物理页释放 | shrink 后可归还 OS |
| 误访问保护 | 未 commit 区域触碰即崩溃 |

### 7.2 根本矛盾

**地址稳定 vs 零浪费**:
要保证地址不变，必须预先保留空间（mmap 或 reserve）。这必然浪费虚拟空间。

**物理页释放 vs 地址稳定**:
要释放物理页面但保持地址不变，只有 mmap 的 `madvise(MADV_DONTNEED)` 能做到。
vector 的 `shrink_to_fit` 会释放内存但可能改变地址。

**误访问保护 vs 零平台依赖**:
`PROT_NONE` 保护需要 mmap。C++ 标准没有等效机制。

### 7.3 各方案的不完美之处

| 方案 | 不完美之处 |
|------|-----------|
| A (vector) | 每次 grow 都失效 |
| B (mmap) | 200+ 行平台代码、页对齐浪费、需 fallback |
| C (vector+检测) | shrink 不释放物理页、无误访问保护 |

### 7.4 结论：不存在完美方案，但方案 C 最接近

**方案 C 丢失的两个特性的实际影响分析**:

1. **shrink 不释放物理页**: 
   - 影响：buffer 的 vector capacity 保持高水位
   - 实际场景：用户创建 buffer → 写入数据 → save → 继续使用
   - 分析：高水位 = 用户曾经需要的最大空间。保持它意味着未来 grow 大概率不 relocate
   - 如果用户真的需要释放内存，可以 compact（迁移到新 buffer）

2. **无误访问保护**:
   - 影响：写入 reserve 范围内的未 commit 区域不会崩溃
   - 实际场景：用户通过 segment_manager API 分配内存，不会直接写 buffer 裸地址
   - 分析：Boost.Interprocess 的 allocator 只在段逻辑范围内分配，不会越界

**方案 C 获得的三个关键优势**:

1. **代码减少 ~185 行**（删除整个 VirtualMemoryBuffer 类 + 所有 `#if XOFFSET_HAS_MMAP` 分支）
2. **消除平台依赖**（不再需要 `<sys/mman.h>`, 不再有 fallback 路径）
3. **消除页对齐浪费**（精确到字节）

---

## 8. 最终建议

**推荐方案 C: std::vector + 地址检测 + reserve 预留**

实现总结:
```
构造: vector(size, 0) + reserve(size × 16)
grow:  resize → 检测 data() 是否变化 → 快/慢路径
save:  base_t::shrink_to_fit() + string(data, segment_size()) — 不 shrink vector
shrink: 只更新 rbtree 逻辑大小，不改 vector — 地址永远不变
```

这是在"地址稳定 + 零依赖 + 低复杂度"三角中最优的平衡点。

---

## 9. 方案 D：已实施（C + 慢路径后 re-reserve）

方案 D 是方案 C 的自然改进，在慢路径（vector relocate）后追加一次 `reserve(compute_reservation(new_size))`，
防止连续 relocate 导致频繁 epoch 递增。

### 实施差异（相对方案 C）

```cpp
// 慢路径末尾追加：
++m_epoch;
// ★ 方案 D 的核心改进：
m_buffer.reserve(compute_reservation(m_buffer.size()));
```

### 实施结果

| 维度 | 变化 |
|------|------|
| **VirtualMemoryBuffer** | 已完全删除（~250 行） |
| **`#include <sys/mman.h>`** | 已删除 |
| **`#if XOFFSET_HAS_MMAP`** | 已删除（所有分支） |
| **m_buffer 类型** | `VirtualMemoryBuffer` → `std::vector<char>` |
| **grow() 实现** | 15 行：resize + data() 比较 + 快/慢路径 + re-reserve |
| **shrink_to_fit()** | 只调用 `base_t::shrink_to_fit()`，不修改 vector |
| **save()** | `string(data, segment_size())` — 字节精确，无页对齐 |
| **净代码变化** | **-235 行** |
| **平台依赖** | **0**（纯 C++ 标准库） |
