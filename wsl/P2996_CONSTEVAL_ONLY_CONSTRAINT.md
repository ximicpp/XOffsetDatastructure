# 关键发现：vector<info> 是 consteval-only 类型

## ❗ 重大发现

在修改 `test_advanced_meta.cpp` 时，发现了P2996实现的一个**关键限制**：

```
vector<info> is consteval-only type
```

这意味着 **`vector<info>` 只能在编译时（consteval）上下文中使用，不能在运行时使用**。

---

## 🔍 问题表现

### 错误信息

```
error: variable 'members' of consteval-only type must either be 
       constexpr or in a constant-evaluated context

error: expressions of consteval-only type are only allowed in 
       constant-evaluated contexts
```

### 触发代码

```cpp
// ❌ 错误：vector<info> 不能在运行时使用
int main() {
    auto members = nonstatic_data_members_of(^^Person, 
                                             access_context::unchecked());
    
    // ❌ 错误：不能在运行时遍历 vector<info>
    for (auto member : members) {
        std::cout << display_string_of(member) << "\n";
    }
}
```

---

## 📚 技术原因

### 1. `vector<info>` 的特性

```cpp
// 从 <experimental/meta> 头文件
consteval auto nonstatic_data_members_of(info r, access_context ctx) 
    -> vector<info>;  // 返回 vector<info>
```

- `vector<info>` 是特殊的编译时容器
- **只能在 consteval 函数中使用**
- **不能跨越编译时/运行时边界**

### 2. 为什么这样设计？

1. **编译时反射** - P2996的反射是完全编译时的
2. **零运行时开销** - 所有反射信息在编译后消失
3. **类型安全** - 防止在运行时误用编译时信息
4. **内存安全** - `vector<info>` 使用编译器管理的内存

---

## ✅ 正确的使用方式

### 方式1: consteval 函数中使用

```cpp
// ✅ 在 consteval 函数中使用 vector<info>
template<typename T>
consteval auto get_member_count() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, 
                                              access_context::unchecked());
    return members.size();  // 可以返回编译时常量
}

int main() {
    // ✅ 在运行时使用返回的编译时常量
    constexpr auto count = get_member_count<Person>();
    std::cout << "Member count: " << count << "\n";
}
```

### 方式2: constexpr 变量

```cpp
int main() {
    // ✅ info 类型可以是 constexpr
    constexpr auto age_refl = ^^Person::age;
    constexpr auto age_type = type_of(age_refl);
    
    // ✅ 在运行时使用 info（通过 constexpr）
    std::cout << "Type: " << display_string_of(age_type) << "\n";
}
```

### 方式3: 手动展开（无迭代）

```cpp
int main() {
    // ✅ 手动访问每个成员，不使用 vector<info>
    std::cout << display_string_of(^^Person::age) << "\n";
    std::cout << display_string_of(^^Person::height) << "\n";
    std::cout << display_string_of(^^Person::name) << "\n";
}
```

---

## ❌ 无法实现的功能

### 1. 运行时成员遍历

```cpp
// ❌ 不可能！vector<info> 不能在运行时使用
int main() {
    auto members = nonstatic_data_members_of(^^SomeType, ...);
    for (auto member : members) {  // 编译错误！
        // ...
    }
}
```

### 2. 动态反射

```cpp
// ❌ 不可能！无法在运行时决定反射哪个类型
void print_members(std::string type_name) {
    // 无法将运行时字符串转换为编译时类型
    auto members = nonstatic_data_members_of(^^???);
}
```

### 3. 运行时类型发现

```cpp
// ❌ 不可能！类型必须在编译时已知
template<typename T>
void process(T obj) {
    auto members = nonstatic_data_members_of(^^T, ...);
    // 虽然T是模板参数，但vector<info>仍不能在运行时使用
    for (auto member : members) {  // 编译错误！
        // ...
    }
}
```

---

## 🎯 设计影响

这个限制意味着P2996的反射是**完全编译时**的：

### 优点 ✅
- 零运行时开销
- 类型安全
- 编译器优化友好
- 不需要RTTI

### 缺点 ❌  
- 不能动态遍历成员
- 必须手动展开代码
- 需要 `expand` 操作符（尚未实现）
- 代码重复

---

## 🔧 实际解决方案

### 当前可行的做法

由于 `expand` 操作符未实现，我们必须：

#### 1. 手动列举成员

```cpp
// 必须手动写出每个成员
std::cout << display_string_of(^^Person::age) << "\n";
std::cout << display_string_of(^^Person::height) << "\n";
std::cout << display_string_of(^^Person::weight) << "\n";
std::cout << display_string_of(^^Person::name) << "\n";
```

#### 2. 使用 consteval 辅助函数

```cpp
template<typename T>
consteval auto get_member_count() {
    return nonstatic_data_members_of(^^T, 
                                     access_context::unchecked()).size();
}

// 使用
constexpr auto count = get_member_count<Person>();
```

#### 3. constexpr 单个成员操作

```cpp
constexpr auto age_refl = ^^Person::age;
std::cout << "Name: " << display_string_of(age_refl) << "\n";
std::cout << "Type: " << display_string_of(type_of(age_refl)) << "\n";
std::cout << "Public: " << is_public(age_refl) << "\n";
```

### 未来的解决方案（需要 expand）

```cpp
// 🔮 理想情况（需要 expand 操作符）
consteval {
    auto members = nonstatic_data_members_of(^^Person, 
                                              access_context::unchecked());
    [:expand(members):] >> [](auto member) {
        std::cout << display_string_of(member) << "\n";
    };
}
```

---

## 📊 P2996 限制总结

| 特性 | 状态 | 原因 |
|------|------|------|
| 基础反射 (`^^`, `[: :]`) | ✅ 可用 | 核心语法 |
| `display_string_of` | ✅ 可用 | 返回 string_view |
| `type_of` | ✅ 部分可用 | 返回 info（constexpr） |
| `nonstatic_data_members_of` | ⚠️ 受限 | 返回 vector<info>（consteval-only） |
| 运行时遍历成员 | ❌ 不可用 | vector<info> 是 consteval-only |
| `expand` 操作符 | ❌ 未实现 | 编译器未实现 |
| 动态反射 | ❌ 不可能 | 设计上完全编译时 |

---

## 🎓 学习要点

### 1. consteval vs constexpr

```cpp
// constexpr - 可以在编译时或运行时求值
constexpr int x = 42;

// consteval - 必须在编译时求值
consteval int get_value() { return 42; }

// consteval-only type - 只能在编译时存在
// vector<info> 是 consteval-only
```

### 2. 编译时/运行时边界

```cpp
// ✅ 编译时 → 编译时（允许）
consteval auto f() {
    auto members = nonstatic_data_members_of(...);  // OK
    return members.size();  // OK，返回编译时常量
}

// ❌ 编译时 → 运行时（不允许）
int main() {
    auto members = nonstatic_data_members_of(...);  // 错误！
    // vector<info> 不能跨越边界
}

// ✅ 编译时 → constexpr → 运行时（允许）
int main() {
    constexpr auto count = get_member_count<T>();  // OK
    std::cout << count;  // OK，count 是编译时常量
}
```

---

## ✅ 结论

**vector<info> 是 consteval-only 类型**是P2996实现的核心约束：

1. **必须在 consteval 上下文中使用**
2. **不能在运行时遍历**
3. **需要 expand 操作符来生成代码**（尚未实现）
4. **当前只能手动展开成员**

这解释了为什么之前的代码会编译失败，也说明了为什么需要完全重写 `test_advanced_meta.cpp`。

**P2996 的反射是完全编译时的，这是设计特性，不是bug。**
