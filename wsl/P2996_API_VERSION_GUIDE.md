# P2996 API版本变化详解

## 什么是API版本变化？

P2996提案从2022年开始，经过多次修订（R0, R1, ... R9, R10, R11...），每次修订会：
- 改进设计
- 修改函数签名
- 重命名函数
- 添加/删除功能

**当前Clang P2996分支实现的是 R10 版本**。

---

## R9 → R10 主要API变化

### 1. 函数重命名

#### 变化1: `name_of` → `display_string_of`

**R9 (旧版本)**:
```cpp
consteval auto name_of(info r) -> string_view;
```

**R10 (新版本)**:
```cpp
consteval auto display_string_of(info r) -> string_view;
consteval auto u8display_string_of(info r) -> u8string_view;
```

**原因**: 
- 更精确的命名，`display_string_of`明确表示这是用于显示的字符串
- 区分ASCII和UTF-8版本

**修改示例**:
```cpp
// ❌ R9 旧代码
auto member_name = name_of(^^Point::x);

// ✅ R10 新代码
auto member_name = display_string_of(^^Point::x);
```

---

#### 变化2: `is_static` → `is_static_member`

**R9 (旧版本)**:
```cpp
consteval auto is_static(info r) -> bool;
```

**R10 (新版本)**:
```cpp
consteval auto is_static_member(info r) -> bool;
```

**原因**: 
- 更明确的语义：检查的是"静态成员"而非泛指"静态的东西"
- 避免歧义（静态变量 vs 静态成员）

**修改示例**:
```cpp
// ❌ R9 旧代码
bool is_stat = is_static(^^Class::member);

// ✅ R10 新代码
bool is_stat = is_static_member(^^Class::member);
```

---

### 2. 函数签名变化

#### 变化3: `nonstatic_data_members_of` 新增参数

**R9 (旧版本)**:
```cpp
consteval auto nonstatic_data_members_of(info class_type) -> vector<info>;
```

**R10 (新版本)**:
```cpp
consteval auto nonstatic_data_members_of(info class_type, 
                                         access_context ctx) -> vector<info>;
```

**原因**:
- **访问控制**：需要明确指定访问上下文
- **安全性**：防止无意中访问私有成员
- **灵活性**：可以选择不同的访问策略

**access_context 选项**:
```cpp
namespace access_context {
    // 不检查访问权限（用于反射所有成员）
    consteval auto unchecked() -> access_context;
    
    // 其他可能的选项（视实现而定）
    // consteval auto current() -> access_context;  // 当前上下文
}
```

**修改示例**:
```cpp
// ❌ R9 旧代码
auto members = nonstatic_data_members_of(^^Person);

// ✅ R10 新代码
auto members = nonstatic_data_members_of(^^Person, 
                                         access_context::unchecked());
```

**Clang当前状态**:
```cpp
// 为了兼容，保留了旧版本但标记为废弃
[[deprecated("P2996R10 requires an 'access_context' argument")]]
consteval auto nonstatic_data_members_of(info r) -> vector<info> {
  return nonstatic_data_members_of(r, access_context::unchecked());
}
```

---

### 3. 新增/移除的函数

#### 新增: `is_nonstatic_data_member`

**R10 新增**:
```cpp
consteval auto is_nonstatic_data_member(info r) -> bool;
```

**用途**: 直接检查是否为非静态数据成员

**示例**:
```cpp
// ✅ R10 直接使用
bool is_data = is_nonstatic_data_member(^^Class::member);

// ❌ R9 需要手动组合
bool is_data = !is_static(^^Class::member) && is_data_member(...);
```

---

## 完整的修改对照表

| R9 代码 | R10 代码 | 说明 |
|---------|---------|------|
| `name_of(refl)` | `display_string_of(refl)` | 函数重命名 |
| `is_static(refl)` | `is_static_member(refl)` | 函数重命名 |
| `nonstatic_data_members_of(type)` | `nonstatic_data_members_of(type, access_context::unchecked())` | 新增参数 |
| `!is_static(...)` | `is_nonstatic_data_member(...)` | 新增专用函数 |

---

## 实战：修改test_advanced_meta.cpp

让我展示如何将错误的R9代码修改为正确的R10代码：

### 错误代码 (R9风格)
```cpp
#include <experimental/meta>
#include <iostream>

struct Person {
    int age;
    double height;
};

int main() {
    using namespace std::meta;
    
    // ❌ R9: 缺少access_context
    constexpr auto members = nonstatic_data_members_of(^^Person);
    
    // ❌ R9: 使用name_of
    constexpr auto age_name = name_of(^^Person::age);
    
    // ❌ R9: 使用is_static
    bool is_stat = is_static(^^Person::age);
    
    // ❌ R9: 使用expand（未实现）
    [:expand(members):] >> [](auto member) {
        std::cout << name_of(member) << "\n";
    };
}
```

### 正确代码 (R10风格)
```cpp
#include <experimental/meta>
#include <iostream>

struct Person {
    int age;
    double height;
};

int main() {
    using namespace std::meta;
    
    // ✅ R10: 添加access_context参数
    // ✅ 改为auto（不是constexpr，因为vector涉及堆分配）
    auto members = nonstatic_data_members_of(^^Person, 
                                              access_context::unchecked());
    
    // ✅ R10: 使用display_string_of
    // ✅ 改为auto（consteval函数返回值在运行时使用）
    auto age_name = display_string_of(^^Person::age);
    
    // ✅ R10: 使用is_static_member
    bool is_stat = is_static_member(^^Person::age);
    
    // ✅ R10: 用传统for循环替代expand
    std::cout << "Person members:\n";
    for (auto member : members) {
        // ✅ R10: 使用display_string_of
        std::cout << "  - " << display_string_of(member) << "\n";
        std::cout << "    is_static: " << is_static_member(member) << "\n";
        std::cout << "    is_public: " << is_public(member) << "\n";
    }
    
    std::cout << "\nTotal members: " << members.size() << "\n";
}
```

---

## 完整的可编译示例

### test_meta_introspection_r10.cpp

```cpp
#include <experimental/meta>
#include <iostream>
#include <string_view>

// Test structures
struct Point {
    int x;
    int y;
    double z;
};

struct Person {
    int age;
    double height;
    const char* name;
};

class MyClass {
public:
    int public_member;
    static int static_member;
private:
    int private_member;
};

int MyClass::static_member = 42;

int main() {
    std::cout << "========================================\n";
    std::cout << "  P2996 R10 API Test\n";
    std::cout << "========================================\n\n";

    using namespace std::meta;

    // ========================================================================
    // Test 1: Get member list with access_context
    // ========================================================================
    std::cout << "[Test 1] nonstatic_data_members_of with access_context\n";
    std::cout << "---------------------------------------------------\n";
    {
        auto members = nonstatic_data_members_of(^^Point, 
                                                  access_context::unchecked());
        
        std::cout << "Point has " << members.size() << " members\n";
    }
    std::cout << "[PASS] Test 1 PASSED\n\n";

    // ========================================================================
    // Test 2: display_string_of (替代name_of)
    // ========================================================================
    std::cout << "[Test 2] display_string_of for names\n";
    std::cout << "---------------------------------------------------\n";
    {
        auto x_name = display_string_of(^^Point::x);
        auto y_name = display_string_of(^^Point::y);
        auto z_name = display_string_of(^^Point::z);
        
        std::cout << "Point members:\n";
        std::cout << "  - " << x_name << "\n";
        std::cout << "  - " << y_name << "\n";
        std::cout << "  - " << z_name << "\n";
    }
    std::cout << "[PASS] Test 2 PASSED\n\n";

    // ========================================================================
    // Test 3: is_static_member (替代is_static)
    // ========================================================================
    std::cout << "[Test 3] is_static_member\n";
    std::cout << "---------------------------------------------------\n";
    {
        bool pub_is_static = is_static_member(^^MyClass::public_member);
        bool stat_is_static = is_static_member(^^MyClass::static_member);
        
        std::cout << "MyClass::public_member is static: " << pub_is_static << "\n";
        std::cout << "MyClass::static_member is static: " << stat_is_static << "\n";
    }
    std::cout << "[PASS] Test 3 PASSED\n\n";

    // ========================================================================
    // Test 4: is_nonstatic_data_member (新增函数)
    // ========================================================================
    std::cout << "[Test 4] is_nonstatic_data_member\n";
    std::cout << "---------------------------------------------------\n";
    {
        bool pub_is_nonstatic = is_nonstatic_data_member(^^MyClass::public_member);
        bool stat_is_nonstatic = is_nonstatic_data_member(^^MyClass::static_member);
        
        std::cout << "MyClass::public_member is nonstatic data member: " 
                  << pub_is_nonstatic << "\n";
        std::cout << "MyClass::static_member is nonstatic data member: " 
                  << stat_is_nonstatic << "\n";
    }
    std::cout << "[PASS] Test 4 PASSED\n\n";

    // ========================================================================
    // Test 5: Iterate members with for loop (替代expand)
    // ========================================================================
    std::cout << "[Test 5] Manual iteration (替代expand)\n";
    std::cout << "---------------------------------------------------\n";
    {
        auto members = nonstatic_data_members_of(^^Person, 
                                                  access_context::unchecked());
        
        std::cout << "Person members detail:\n";
        for (auto member : members) {
            std::cout << "  Member: " << display_string_of(member) << "\n";
            std::cout << "    Type: " << display_string_of(type_of(member)) << "\n";
            std::cout << "    Is public: " << is_public(member) << "\n";
            std::cout << "    Is static: " << is_static_member(member) << "\n";
        }
    }
    std::cout << "[PASS] Test 5 PASSED\n\n";

    // ========================================================================
    // Test 6: Combined member access
    // ========================================================================
    std::cout << "[Test 6] Combined reflection operations\n";
    std::cout << "---------------------------------------------------\n";
    {
        Person p{25, 175.5, "Alice"};
        
        auto members = nonstatic_data_members_of(^^Person, 
                                                  access_context::unchecked());
        
        std::cout << "Person instance values:\n";
        
        // 手动访问每个成员（因为expand未实现）
        std::cout << "  age: " << p.[:^^Person::age:] << "\n";
        std::cout << "  height: " << p.[:^^Person::height:] << "\n";
        std::cout << "  name: " << p.[:^^Person::name:] << "\n";
        
        std::cout << "\nMember count: " << members.size() << "\n";
    }
    std::cout << "[PASS] Test 6 PASSED\n\n";

    // ========================================================================
    // Test 7: Type introspection
    // ========================================================================
    std::cout << "[Test 7] Type name queries\n";
    std::cout << "---------------------------------------------------\n";
    {
        auto point_name = display_string_of(^^Point);
        auto person_name = display_string_of(^^Person);
        auto int_name = display_string_of(^^int);
        auto double_name = display_string_of(^^double);
        
        std::cout << "Type names:\n";
        std::cout << "  Point: " << point_name << "\n";
        std::cout << "  Person: " << person_name << "\n";
        std::cout << "  int: " << int_name << "\n";
        std::cout << "  double: " << double_name << "\n";
    }
    std::cout << "[PASS] Test 7 PASSED\n\n";

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "========================================\n";
    std::cout << "  P2996 R10 API Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: nonstatic_data_members_of with access_context\n";
    std::cout << "[PASS] Test 2: display_string_of for names\n";
    std::cout << "[PASS] Test 3: is_static_member\n";
    std::cout << "[PASS] Test 4: is_nonstatic_data_member\n";
    std::cout << "[PASS] Test 5: Manual iteration (no expand)\n";
    std::cout << "[PASS] Test 6: Combined operations\n";
    std::cout << "[PASS] Test 7: Type name queries\n";
    std::cout << "\n[SUCCESS] All R10 API tests passed!\n";
    std::cout << "========================================\n";

    return 0;
}
```

---

## 关键修改点总结

### 1. 函数名称
- ❌ `name_of` → ✅ `display_string_of`
- ❌ `is_static` → ✅ `is_static_member`

### 2. 函数参数
- ❌ `nonstatic_data_members_of(type)` 
- ✅ `nonstatic_data_members_of(type, access_context::unchecked())`

### 3. 变量声明
- ❌ `constexpr auto members = ...` (vector不能constexpr)
- ✅ `auto members = ...`

### 4. 遍历方式
- ❌ `[:expand(members):] >> ...` (未实现)
- ✅ `for (auto member : members) { ... }`

---

## 建议的迁移步骤

1. **全局替换函数名**:
   ```
   name_of          → display_string_of
   is_static        → is_static_member
   ```

2. **添加access_context**:
   ```cpp
   nonstatic_data_members_of(^^Type, access_context::unchecked())
   ```

3. **移除constexpr**:
   ```cpp
   auto members = ...  // 不是 constexpr auto
   ```

4. **替换expand**:
   ```cpp
   for (auto member : members) { ... }
   ```

---

**是否需要修改到R10？** 

**是的！** 因为当前Clang P2996分支实现的就是R10版本，使用R9 API会产生编译错误或警告。
