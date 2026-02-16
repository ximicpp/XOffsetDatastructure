# Proposal: Adaptive mmap Reservation Strategy

> **Status**: Analysis Complete — Awaiting Decision  
> **Date**: 2026-02-16  
> **Related**: P3 mmap backend implementation

## 1. Problem Statement

Current `VirtualMemoryBuffer` **unconditionally reserves 256MB** virtual address space per
XBuffer instance. This works perfectly for the single-buffer or few-buffer use case,
but becomes problematic when applications create many independent XBuffer instances.

### 1.1 Quantitative Impact (实测数据)

Test platform: macOS arm64, 36GB RAM, page size = 16KB.

| 实例数 | 当前方案 (256MB/个) | 自适应 (64KB/个) | 自适应 (1MB/个) |
|--------|---------------------|-------------------|-----------------|
| 1      | 256MB 虚拟          | 64KB 虚拟         | 1MB 虚拟        |
| 100    | 25.6GB              | 6.4MB             | 100MB           |
| 1,000  | 256GB               | 64MB              | 1GB             |
| 10,000 | 2.5TB               | 640MB             | 10GB            |
| 100,000| 25TB                | 6.1GB             | 100GB           |

> **物理内存不受影响** — PROT_NONE 页面不消耗物理 RAM。  
> 以上全部为虚拟地址空间消耗。

### 1.2 System Limits (系统硬限制)

| 平台 | 用户态虚拟地址空间 | VMA 条目限制 | 实测 mmap 上限 |
|------|-------------------|-------------|----------------|
| macOS arm64 | ~256TB (48-bit) | ~200,000+ (实测) | 15,000 × 256MB = 3.7TB |
| macOS arm64 | ~256TB | ~200,000+ | 100,000 × 64KB = 6.1GB |
| Linux x86_64 | 128TB (47-bit) | `vm.max_map_count` = 65,530 (default) | ~65K regions |
| Linux x86_64 (tuned) | 128TB | 可调至 1M+ | 根据需求 |

### 1.3 Performance (实测)

创建 10,000 个 mmap 区域的耗时（macOS arm64）：

| 保留大小 | 总耗时 | 每个耗时 |
|---------|--------|---------|
| 256MB   | 3.1ms  | 0.3μs   |
| 1MB     | 2.5ms  | 0.3μs   |
| 64KB    | 2.5ms  | 0.3μs   |

> **结论**: mmap 创建性能与保留大小无关。优化目标纯粹是虚拟地址空间效率。

### 1.4 核心矛盾

| 需求 | 少量大 buffer | 海量小 buffer |
|------|-------------|-------------|
| 实例数 | 1-10 | 10,000+ |
| 典型 initial_size | 1MB-100MB | 512B-16KB |
| grow 频率 | 高 | 低 (多为一次构建后只读) |
| 地址稳定性 | **关键** — 长时间持有指针 | 不太关键 — 通常通过 root<T>() 访问 |
| 256MB 保留合理性 | ✅ 完全合理 | ❌ 浪费 >99.99% 虚拟空间 |

---

## 2. Proposed Solution: Adaptive Reservation

### 2.1 核心公式

```
max_reserved = clamp(initial_size × GROWTH_HEADROOM, MIN_RESERVE, MAX_RESERVE)
```

其中：
- `GROWTH_HEADROOM = 16` — 允许 buffer 增长到初始大小的 16 倍
- `MIN_RESERVE = 64KB` — 最低保留（= 4 × macOS 16KB page 或 16 × Linux 4KB page）
- `MAX_RESERVE = 256MB` — 最大保留（与当前一致）

### 2.2 行为表

| initial_size | 计算过程 | max_reserved | 增长上限 |
|-------------|---------|-------------|---------|
| 512B        | 512 × 16 = 8KB → clamp → 64KB | **64KB** | 可增长到 64KB |
| 4KB         | 4K × 16 = 64KB | **64KB** | 可增长到 64KB |
| 16KB        | 16K × 16 = 256KB | **256KB** | 可增长到 256KB |
| 64KB        | 64K × 16 = 1MB | **1MB** | 可增长到 1MB |
| 256KB       | 256K × 16 = 4MB | **4MB** | 可增长到 4MB |
| 1MB         | 1M × 16 = 16MB | **16MB** | 可增长到 16MB |
| 4MB         | 4M × 16 = 64MB | **64MB** | 可增长到 64MB |
| 16MB        | 16M × 16 = 256MB | **256MB** | 可增长到 256MB |
| 32MB+       | clamp at MAX | **256MB** | 可增长到 256MB |

### 2.3 海量对象场景对比

**10,000 个 XBuffer(4096)（典型小对象场景）：**

| 指标 | 当前方案 | 自适应方案 | 改善倍数 |
|------|---------|-----------|---------|
| 虚拟地址空间 | 2.5TB | 640MB | **4,000×** |
| VMA 条目数 | 10,000 | 10,000 | 1× (不变) |
| 物理内存 | ~160MB | ~160MB | 1× (不变) |
| 每个实例增长上限 | 256MB | 64KB | — |

**100,000 个 XBuffer(512)（极端微对象场景）：**

| 指标 | 当前方案 | 自适应方案 |
|------|---------|-----------|
| 虚拟地址空间 | 25TB | 6.1GB |
| 能否创建成功 | ❌ 超出 macOS 3.7TB 实测上限 | ✅ 轻松 |

### 2.4 grow() 超出保留量时的处理

当 `grow()` 请求的空间超出 `max_reserved` 时，不能简单失败。需要一个 **fallback 慢路径**：

```
grow(extra_bytes):
  if (committed + extra ≤ reserved):
    mprotect → commit 新页面          // 快路径：O(1)，地址不变
  else:
    new_reserved = max(reserved × 2, committed + extra)
    new_region = mmap(new_reserved)   // 慢路径：重新映射
    memcpy(new_region, old_region)
    munmap(old_region)
    base_address = new_region         // 地址改变！
    ++epoch                           // 通知 XHandle 重新查找
```

**为什么这是安全的：**
- Boost.Interprocess 内部全部使用 `offset_ptr`（相对偏移），移动内存块后偏移关系不变
- 外部持有的 `T*` 裸指针会失效，但 epoch 机制会让 `XHandle<T>` 自动重新查找
- 这与 mmap 之前 `std::vector` 的行为一致 — 退化而非崩溃

### 2.5 用户可控制的 max_capacity

对于知道自己需求的高级用户，允许显式指定：

```cpp
// 自适应（默认）
XBuffer buf(4096);                          // reserved = 64KB

// 显式指定最大容量
XBuffer buf(4096, XBuffer::max_capacity(64 * 1024 * 1024));  // reserved = 64MB

// 无限制（等价于当前行为）
XBuffer buf(4096, XBuffer::max_capacity(256 * 1024 * 1024)); // reserved = 256MB
```

---

## 3. Implementation Design

### 3.1 VirtualMemoryBuffer Changes

```cpp
class VirtualMemoryBuffer {
public:
    // Adaptive: compute max_reserved from initial_size
    static constexpr std::size_t GROWTH_HEADROOM = 16;
    static constexpr std::size_t MIN_RESERVE = 64ULL * 1024;        // 64KB
    static constexpr std::size_t MAX_RESERVE = 256ULL * 1024 * 1024; // 256MB

    static std::size_t compute_reservation(std::size_t initial_size) {
        std::size_t r = initial_size * GROWTH_HEADROOM;
        if (r < MIN_RESERVE) r = MIN_RESERVE;
        if (r > MAX_RESERVE) r = MAX_RESERVE;
        return r;
    }

    // Default: adaptive reservation
    explicit VirtualMemoryBuffer(std::size_t initial_size)
        : VirtualMemoryBuffer(initial_size, compute_reservation(initial_size))
    {}

    // Explicit: user-specified reservation
    VirtualMemoryBuffer(std::size_t initial_size, std::size_t max_reserved)
    { /* existing mmap logic */ }

    // grow with fallback remap
    bool grow(std::size_t extra_bytes) {
        std::size_t new_committed = m_committed + extra_bytes;
        if (new_committed <= m_reserved) {
            return grow_in_place(extra_bytes);  // fast path
        } else {
            return grow_remap(new_committed);   // slow path
        }
    }
};
```

### 3.2 XBuffer API Extension

```cpp
class XBuffer : public XBufferCore {
public:
    struct max_capacity_t {
        std::size_t value;
    };
    static max_capacity_t max_capacity(std::size_t bytes) {
        return {bytes};
    }

    // Existing: adaptive reservation (default)
    XBuffer(std::size_t size);

    // New: explicit reservation
    XBuffer(std::size_t size, max_capacity_t cap);
};
```

### 3.3 XManagedMemory Changes

```cpp
// grow() updated to handle remap
bool grow(size_type extra_bytes) {
    bool address_changed = false;
    if (!m_buffer.grow(extra_bytes, &address_changed))
        return false;
    if (address_changed) {
        // Re-bind base_t to new address
        base_t::open_impl(m_buffer.data(), m_buffer.size());
        ++m_epoch;  // Invalidate XHandle caches
    } else {
        base_t::grow(extra_bytes);
        // No epoch change — address stable
    }
    return true;
}
```

---

## 4. Risk Assessment

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|---------|
| grow remap 慢路径损失地址稳定性 | 中 (仅超出保留时) | 低 (epoch 自动处理) | 用户可显式增大 max_capacity |
| 自适应保留过小，频繁触发 remap | 低 (16x headroom) | 中 | 监控：log warning on remap |
| API 向后兼容 | 无 | 无 | 默认行为仅减小保留量，不改变语义 |
| Linux vm.max_map_count | 中 (>65K 实例) | 高 | 文档提示调参；未来考虑 pool |

---

## 5. Future: Pool-Based Backend (备选，适用于 100K+ 场景)

当实例数超过 65,000（Linux VMA 限制），单个 mmap 区域的方案都不够。
需要引入 **VMA Pool**：

```
┌──────────────────────────────────────────────┐
│          Single 4GB mmap region               │
│  ┌──────┐ ┌──────┐ ┌──────┐     ┌──────┐    │
│  │ Buf0 │ │ Buf1 │ │ Buf2 │ ... │ BufN │    │
│  │ 64KB │ │ 64KB │ │ 64KB │     │ 64KB │    │
│  └──────┘ └──────┘ └──────┘     └──────┘    │
│  Slot 0    Slot 1    Slot 2      Slot N      │
└──────────────────────────────────────────────┘
VMA count: 1 (instead of N)
```

但这需要自定义 slot allocator，复杂度高，建议作为后续优化。
自适应保留策略已能覆盖 99% 的实际场景（≤ 50K 实例）。

---

## 6. Decision Matrix

| 方案 | 工作量 | 覆盖场景 | 推荐度 |
|------|--------|---------|--------|
| A. 自适应保留 + remap fallback | 中 (2-3小时) | ≤ 50K 实例 | ⭐⭐⭐⭐⭐ |
| B. 仅自适应保留（无 fallback） | 小 (1小时) | ≤ 50K 实例 | ⭐⭐⭐⭐ |
| C. 固定保留 + 用户参数 | 极小 (30分钟) | 用户自己负责 | ⭐⭐⭐ |
| D. A + Pool 后端 | 大 (1-2天) | 100K+ 实例 | ⭐⭐⭐（过度设计） |