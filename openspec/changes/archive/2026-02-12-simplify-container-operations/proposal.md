# Change: Simplify Container Operations — Unified Allocator Propagation

## Why

当 XVector/XMap/XSet 中存放 **XString** 或**自定义 allocator-aware 类型**时，用户必须手动传递
`get_segment_manager()` 或 `allocator`。根因是 C++ 标准容器的 `construct()` 默认使用 placement new，
不将容器持有的 allocator 传播到元素构造函数。

统计：37 处容器操作需要手动传 allocator，其中 95% 涉及 XString。

## What Changes

### 统一方案：两层机制，一个原则

> **原则：容器知道自己的 allocator，它应该在任何需要的地方自动使用它。**

#### 层次 1：scoped_allocator_adaptor（construct 路径）

将容器 allocator 从 `allocator<T, SM>` 改为 `scoped_allocator_adaptor<allocator<T, SM>>`。
这是 C++ 标准对"allocator propagation in nested containers"问题的精确解答（N2554）。

覆盖：所有 `emplace` / `emplace_back` / `emplace_hint` 路径（通过 `allocator_traits::construct`）。
包括 `flat_map` 的 pair 分别注入（Boost.Container `dispatch_uses_allocator` pair 特化）。

#### 层次 2：Wrapper class 泛型重载（签名路径）

将 XVector/XMap/XSet 从 type alias 改为继承自底层容器的 wrapper class，添加泛型重载。
覆盖签名要求已构造对象的 API（push_back、insert、operator[]、erase 等）。

检测逻辑统一为 `requires` 约束：
- XVector：如果参数不能直接构造 T，但加上 SM* 就能 → 自动注入
- XMap/XSet：如果 key 参数不能转为 K/T → 通过 find + emplace 组合处理

### 需要 Wrapper 覆盖的 12 个接口

**XVector（5 个）**：push_back, insert(pos,val), insert(pos,n,val), resize(n,val), assign(n,val)
**XMap（4 个）**：operator[], erase(key), try_emplace(key), insert_or_assign(key)
**XSet（3 个）**：insert(val), insert(pos,val), erase(key)

### 用户自定义类型要求

自定义类型需添加一行 `using allocator_type = ...;` 以启用自动传播（C++ 标准 allocator-aware 约定）。

## Impact

- **Breaking Change**：现有 `emplace_back("str", sm)` 写法编译失败（scoped_alloc 会双重注入），
  需简化为 `emplace_back("str")`。约 25 处需修改，每处都是删除多余参数。
- Affected code: `xoffsetdatastructure2.hpp`（allocator 别名 + 容器 wrapper class）
- Affected tests: 所有容器操作相关测试（简化）
- 子系统影响：`is_safe_leaf`/`migrate_as`/TypeLayout 特化代码不变（仍匹配 XVector/XMap/XSet 名字）
- sizeof 不变（scoped_alloc 零额外成员，wrapper 无额外数据成员）