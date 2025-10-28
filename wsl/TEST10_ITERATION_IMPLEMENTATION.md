# Test 10: nonstatic_data_members_of 遍历实现说明

## ✅ 新增测试用例

在 `test_advanced_meta.cpp` 中添加了 **Test 10: nonstatic_data_members_of Iteration**，展示如何在 consteval 上下文中使用 `nonstatic_data_members_of` 遍历属性。

---

## 🎯 核心挑战

### 问题
`vector<info>` 是 **consteval-only** 类型，无法在运行时直接遍历：

```cpp
// ❌ 错误：不能在运行时使用 vector<info>
int main() {
    auto members = nonstatic_data_members_of(^^Person, ...);
    for (auto member : members) {  // 编译错误！
        std::cout << display_string_of(member) << "\n";
    }
}
```

### 解决方案
通过以下技术组合实现"伪遍历"：

1. **在 consteval 函数中获取成员信息**
2. **将信息转换为可跨边界的类型**
3. **使用 C++17 折叠表达式在运行时展开**

---

## 🔧 实现方法

### 步骤1: 定义可跨边界的数据结构

```cpp
// 可以跨越编译时/运行时边界的结构
struct MemberInfo {
    const char* name;
    const char* type;
    bool is_public;
    bool is_static;
};
```

**关键点**：
- 使用 `const char*` 而非 `std::string_view`（string_view 的 data() 指向编译时内存）
- 只包含基本类型和指针
- 可以作为 constexpr 返回值

### 步骤2: 在 consteval 中提取特定索引的成员信息

```cpp
template<typename T, size_t Index>
consteval auto get_member_info_at() -> MemberInfo {
    using namespace std::meta;
    
    // ✅ 在 consteval 中可以使用 vector<info>
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    
    if (Index < members.size()) {
        auto member = members[Index];  // ✅ 可以索引访问
        return MemberInfo{
            display_string_of(member).data(),
            display_string_of(type_of(member)).data(),
            is_public(member),
            is_static_member(member)
        };
    }
    return MemberInfo{"", "", false, false};
}
```

**关键点**：
- 在 consteval 函数内部可以遍历 `vector<info>`
- 通过索引访问而非迭代器
- 返回可跨边界的 `MemberInfo`

### 步骤3: 使用折叠表达式展开

```cpp
template<typename T, size_t... Is>
constexpr void print_all_members_impl(std::index_sequence<Is...>) {
    std::cout << "Iterating members via consteval:\n";
    
    // ✅ C++17 折叠表达式：为每个索引调用一次
    ((std::cout << "  [" << Is << "] " 
                << get_member_info_at<T, Is>().name << " : "
                << get_member_info_at<T, Is>().type << "\n"
                << "      public=" << get_member_info_at<T, Is>().is_public
                << ", static=" << get_member_info_at<T, Is>().is_static << "\n"), ...);
}
```

**关键点**：
- `std::index_sequence<Is...>` 生成 0, 1, 2, ... 的索引序列
- 折叠表达式 `(expression, ...)` 展开为多次调用
- 每次调用 `get_member_info_at<T, Is>()` 都在编译时执行

### 步骤4: 包装为易用接口

```cpp
template<typename T>
void print_all_members_via_consteval() {
    // 获取成员数量
    constexpr auto count = get_member_info_count<T>();
    
    // 生成索引序列并调用实现
    print_all_members_impl<T>(std::make_index_sequence<count>{});
}
```

**用法**：
```cpp
// ✅ 简单调用
print_all_members_via_consteval<Person>();
```

---

## 📊 工作原理图解

```
编译时                           运行时
─────────────────────────────────────────────────
                                 │
consteval get_member_info_at<T, 0>()   │
  ├─ nonstatic_data_members_of()      │
  ├─ members[0]                        │
  └─ return MemberInfo{...}  ────────► │ MemberInfo info0
                                 │
consteval get_member_info_at<T, 1>()   │
  ├─ nonstatic_data_members_of()      │
  ├─ members[1]                        │
  └─ return MemberInfo{...}  ────────► │ MemberInfo info1
                                 │
consteval get_member_info_at<T, 2>()   │
  ├─ nonstatic_data_members_of()      │
  ├─ members[2]                        │
  └─ return MemberInfo{...}  ────────► │ MemberInfo info2
                                 │
                                 │
折叠表达式展开:                    │
  (print(info0), print(info1), print(info2), ...)
                                 │
                                 ▼
                            运行时输出
```

---

## ✅ 运行结果

```
Person members:
Iterating members via consteval:
  [0] age : int
      public=1, static=0
  [1] height : double
      public=1, static=0
  [2] weight : float
      public=1, static=0
  [3] name : const char *
      public=1, static=0

Point3D members:
Iterating members via consteval:
  [0] x : float
      public=1, static=0
  [1] y : float
      public=1, static=0
  [2] z : float
      public=1, static=0
```

---

## 🎓 技术要点

### 1. vector<info> 的限制

| 操作 | 编译时 (consteval) | 运行时 |
|------|-------------------|--------|
| 创建 vector<info> | ✅ 可以 | ❌ 不行 |
| 遍历 vector<info> | ✅ 可以 | ❌ 不行 |
| 索引访问 members[i] | ✅ 可以 | ❌ 不行 |
| 获取 size() | ✅ 可以 | ❌ 不行 |
| 返回 vector<info> | ❌ 不行 | ❌ 不行 |

### 2. 可跨边界的类型

| 类型 | 可跨边界？ | 说明 |
|------|----------|------|
| `info` | ✅ 是（constexpr） | 必须是 constexpr 变量 |
| `vector<info>` | ❌ 否 | consteval-only |
| `MemberInfo` | ✅ 是 | 普通结构体 |
| `const char*` | ✅ 是 | 指向字符串字面量 |
| `bool`, `int` | ✅ 是 | 基本类型 |

### 3. 折叠表达式

```cpp
// 一元右折叠 (E op ...)
(E1, E2, E3, E4)  →  (E1, (E2, (E3, E4)))

// 我们的用法
((print(Is), ...))
→ (print(0), (print(1), (print(2), print(3))))
```

---

## 🔑 关键代码

### 完整实现

```cpp
// 1. 定义跨边界结构
struct MemberInfo {
    const char* name;
    const char* type;
    bool is_public;
    bool is_static;
};

// 2. 在 consteval 中提取成员信息
template<typename T, size_t Index>
consteval auto get_member_info_at() -> MemberInfo {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    
    if (Index < members.size()) {
        auto member = members[Index];
        return MemberInfo{
            display_string_of(member).data(),
            display_string_of(type_of(member)).data(),
            is_public(member),
            is_static_member(member)
        };
    }
    return MemberInfo{"", "", false, false};
}

// 3. 使用折叠表达式展开
template<typename T, size_t... Is>
constexpr void print_all_members_impl(std::index_sequence<Is...>) {
    std::cout << "Iterating members via consteval:\n";
    ((std::cout << "  [" << Is << "] " 
                << get_member_info_at<T, Is>().name << " : "
                << get_member_info_at<T, Is>().type << "\n"
                << "      public=" << get_member_info_at<T, Is>().is_public
                << ", static=" << get_member_info_at<T, Is>().is_static << "\n"), ...);
}

// 4. 提供简单接口
template<typename T>
void print_all_members_via_consteval() {
    constexpr auto count = get_member_info_count<T>();
    print_all_members_impl<T>(std::make_index_sequence<count>{});
}
```

### 使用示例

```cpp
// 打印 Person 的所有成员
print_all_members_via_consteval<Person>();

// 打印 Point3D 的所有成员
print_all_members_via_consteval<Point3D>();
```

---

## 💡 为什么这样可行？

### 编译时计算路径

```cpp
constexpr auto count = get_member_info_count<Person>();
// count = 4 (编译时常量)

std::make_index_sequence<4>{}
// 生成类型: index_sequence<0, 1, 2, 3>

print_all_members_impl<Person>(index_sequence<0, 1, 2, 3>{})
// 展开为:
//   get_member_info_at<Person, 0>()
//   get_member_info_at<Person, 1>()
//   get_member_info_at<Person, 2>()
//   get_member_info_at<Person, 3>()

// 每个 get_member_info_at 都是独立的 consteval 调用
// 在编译时执行，返回 MemberInfo 到运行时
```

---

## 📈 性能特点

### 编译时
- ✅ 每个类型只计算一次
- ✅ 结果内联到代码中
- ✅ 零运行时开销（成员信息）

### 运行时
- ✅ 只有打印操作
- ✅ 成员信息已经是常量
- ✅ 无需运行时反射查询

---

## 🎯 对比其他方法

| 方法 | 可行性 | 灵活性 | 性能 |
|------|--------|--------|------|
| **运行时遍历** vector<info> | ❌ 不可能 | - | - |
| **手动列举**每个成员 | ✅ 可行 | ❌ 低（代码重复） | ✅ 最好 |
| **consteval + 折叠表达式** | ✅ 可行 | ✅ 高（自动化） | ✅ 很好 |
| **expand 操作符**（理想） | ❌ 未实现 | ✅ 最高 | ✅ 最好 |

---

## ✅ 总结

### 实现的功能
- ✅ 自动遍历所有成员（无需手动列举）
- ✅ 获取成员名称、类型、属性
- ✅ 在运行时打印成员信息
- ✅ 类型安全、零运行时反射开销

### 技术要点
1. **consteval 上下文**中使用 `nonstatic_data_members_of`
2. **MemberInfo 结构**跨越编译时/运行时边界
3. **索引访问**而非迭代器
4. **折叠表达式**在运行时展开
5. **index_sequence** 生成编译时索引

### 局限性
- 不是真正的运行时遍历
- 需要编译时已知类型
- 每个类型生成独立代码
- 成员数量影响编译时间

---

**这是在 P2996 当前约束下，实现成员"遍历"的最优雅方法！** 🎉
