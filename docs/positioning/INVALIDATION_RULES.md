> X3 of `MUTABLE_RELOCATABLE.md`：按"数据结构"定位要有的失效规则表草稿。逐条按 next_cpp26 @ 4bc922b 的代码核对，另加本分支的 `clone()` 原型和 R1 落地后的变化。这是讨论稿，定下来后再进 `docs/USER_GUIDE.md`。

# XOffset 失效规则（草案）

2026-10-22 · 「可修改可重定位定位」线程 · 状态：草案，未进用户文档

现在的用户指南只有一句话："容器扩容、插入、删除会移动元素，不能长期保存元素指针；arena 基址和根对象地址在增长时保持不变"（`docs/USER_GUIDE.md` 第 3 节）。按数据结构定位，用户会拿 cppreference 上 std 容器的失效表来对照，所以要写成同样的形式。

## 1. 三种地址

| 层 | 指什么 | 例子 |
| --- | --- | --- |
| 元素 | 容器里的元素对象，包括元素里嵌套的容器头 | `heroes[3]`、`heroes[3].name` 这个头本身 |
| 子数据 | 元素拥有的块 | `heroes[3].name` 的字符、`heroes[3].skills` 的元素数组 |
| 区域 | 整个 payload：根对象和它到达的一切 | `root<PlayerState>()` |

另有一个计数：**epoch**。区域里的内容整体换掉或搬走时，epoch 加一；`XHandle` 看到 epoch 变了就重新解析根。

## 2. 容器操作：只影响这个容器自己

任何一个容器操作都不会移动别的容器的元素或子数据。分配只从空闲表或尾部取新块，释放只把块记进空闲表，不搬动任何活块（代码：`allocate_block`、`retire_block`；`docs/V2_FORMAT.md` 第 4 节）。

| 操作 | 本容器元素的引用、迭代器 | 和 std 比 |
| --- | --- | --- |
| `push_back` / `emplace_back` | 没扩容：都有效。扩容：全部失效 | 同 `std::vector` |
| `insert(pos, …)` | `pos` 及之后失效；扩容时全部失效 | 同 `std::vector` |
| `erase(pos)` | `pos` 及之后失效 | 同 `std::vector` |
| `stable_erase_if(pred)` | 第一个被删的位置及之后失效 | 同 `std::erase_if` |
| `reserve(n)`、`resize(n)` 超过容量 | 全部失效 | 同 `std::vector` |
| `clear()` | 全部失效；容量保留，旧字节清零 | `std::vector` 不清零 |
| `XString` 任何赋值 | 之前取到的 `view()`、`data()` 不要再用 | 同 `std::string` |
| `XSet` / `XMap` 插入（`operator[]` 插入新键、`insert`） | 插入位置及之后失效；扩容时全部失效 | **不同于 `std::map` / `std::set`**（它们插入不影响别的元素）；同 `std::flat_map` |
| `XSet` / `XMap` 删除 | 删除位置及之后失效 | **不同于 `std::map`**；同 `std::flat_map` |
| 读、改已有元素的值（不改长度） | 都有效 | 同 std |

几点说明：
- **扩容可能原地完成。** 容器的块正好在区域尾部时，`reserve` 和 `push_back` 会原地扩展（`try_expand_dynamic_bytes`），这时元素地址不变。这不在承诺里，用户应按"扩容即全部失效"来写。
- **元素搬家时子数据通常不动。** 扩容、中间插入删除搬的是元素本身（含嵌套的容器头），走 `relocate_construct` 或 SHIFT（memmove 加 rel32 rebase）；头被重建在新位置，指向原来的块，所以 `heroes[3].name` 的字符没有挪。例外是不满足 `can_relocate_without_allocation` 的类型，它们走 `rebuild`，子数据也会被复制。内联的短字符串（不超过 10 字节）在头里，跟着头走。建议也不写进承诺，和 `std::vector<std::string>` 里的短字符串优化一样，只作为实现细节。
- **XMap 是有序平铺数组。** 这是失效规则和 `std::map` 不同的根本原因，复杂度也不同：插入、删除 O(n)，查找 O(log n)。定位文档里要写清楚它对标的是 `std::flat_map`。

## 3. buffer 操作：影响整块区域

| 操作 | 区域地址 | 指进 payload 的 `T&`、span、迭代器 | epoch | 现状 |
| --- | --- | --- | --- | --- |
| 容器操作触发的 Automatic 增长 | 不变（VM 预留里开页） | 只按第 2 节失效 | 不变 | 代码 |
| 显式 `grow()`，VM 存储 | 不变 | 有效 | 不变 | 代码 |
| 显式 `grow()`，**R1 可搬槽位** | 可能改变 | **全部失效**，要重新取 root 或用 `XHandle` | 加一 | susu 15:09Z 已定，I1 落地后生效 |
| `save_verified`、`save_verified_into`、`save` | 不变（只做逻辑收缩） | 有效 | 不变 | 代码 |
| `reload_verified`、`reset<T>()`、`begin_verified_reload` | 不变（复用存储） | 全部失效 | 加一 | 代码；`begin_verified_reload` 是 PR #50 |
| `load_verified`、`load_unverified`、`XCompactor::compact` | 结果是新 buffer | 源不受影响 | 新 buffer 从 0 开始 | 代码 |
| `clone()` | 结果是新 buffer | 源不受影响 | 新 buffer 从 0 开始 | 本分支原型 |
| `src.clone_into(dst)` | dst 不变（复用存储） | dst 全部失效；src 不受影响 | dst 加一 | 本分支原型 |
| `XBuffer` 移动构造、移动赋值 | 不变，存储随对象转移 | 仍然有效 | 两边都加一 | 代码（`move_from`） |
| `swap(a, b)` | 两块各自不变，换主人 | 仍然有效，但现在属于另一个对象 | 两边都加一 | 代码 |
| 析构 | 释放 | 全部失效 | 不涉及 | 代码 |

说明：
- **`XHandle` 绑在 C++ 对象上，不跟着移动走。** 移动之后，原对象变空，绑在它上面的 `XHandle` 解析为空；要在新对象上重新建。这和"buffer 对象要比它的 XHandle 活得久"（`USER_GUIDE.md` 第 5 节）是同一条约束。
- **"区域地址不变"只对 VM 存储成立。** R1 之后，默认（Explicit）buffer 放在可搬槽位上，显式 `grow()` 是唯一会让存活区域换地址的点；需要地址永远不变的写 `with_storage(XStorage::Pinned)`。
- **K0 保证的是字节，不是地址。** 任何两次操作之间，区域的字节都可以整块复制到别处接着用；但"复制"产生的是一个新 buffer，旧地址上的引用不会自动指过去。

## 4. 还要核对的

- `freeze_verified`、`begin_verified_receive` / `finish_verified` 两条进阶传输路径的失效规则。
- `XCompactor` 失败重试时（容量按 2、2.5、3、4 倍试）源 buffer 是否完全不动。
- R1、I1 落地后的槽位升级为 VM（超过 SLOT_MAX 时）这一步，要单独列一行。
- 这张表要不要进 K0–K7 的稳定层：建议第 2 节的 std 式规则进（它是数据结构契约的一部分），第 2 节"说明"里的实现细节不进。
