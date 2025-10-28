# Splice 操作图解：一图看懂为什么失败

```
┌─────────────────────────────────────────────────────────────────┐
│                    类型签名自动生成的流程                          │
└─────────────────────────────────────────────────────────────────┘

输入：struct Item { uint32_t item_id; uint32_t item_type; ... }
目标：生成 "struct[s:48,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],...}"


┌─────────────────────────────────────────────────────────────────┐
│ 步骤 1: 获取成员列表                                              │
└─────────────────────────────────────────────────────────────────┘

    auto members = nonstatic_data_members_of(^^Item);
         ↓
    std::vector<std::meta::info> {
        info(Item::item_id),      // ← 堆分配的 vector
        info(Item::item_type),    // ← 不是 constexpr 对象
        info(Item::quantity),
        info(Item::name)
    }


┌─────────────────────────────────────────────────────────────────┐
│ 步骤 2: 访问第一个成员                                             │
└─────────────────────────────────────────────────────────────────┘

    auto member = members[0];  // Index = 0 (constexpr)
         ↓
    std::meta::info(Item::item_id)
         ↑
         问题：member 不是 constexpr！
         原因：来自非 constexpr 容器


┌─────────────────────────────────────────────────────────────────┐
│ 步骤 3: 获取成员的类型                                             │
└─────────────────────────────────────────────────────────────────┘

    auto type_info = type_of(member);
         ↓
    std::meta::info(uint32_t)
         ↑
         问题：type_info 也不是 constexpr！
         原因：member 不是 constexpr


┌─────────────────────────────────────────────────────────────────┐
│ 步骤 4: 【关键】Splice - 将 info 转换为类型                        │
└─────────────────────────────────────────────────────────────────┘

    ❌ 尝试 1：直接 splice
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    using FieldType = [:type_info:];
                      ^^^^^^^^^^^^^
                      ❌ 错误！type_info 不是 constexpr
    
    编译器：error: splice operand must be a constant expression
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


    ❌ 尝试 2：强制 constexpr
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    constexpr auto type_info_const = type_info;
                   ^^^^^^^^^^^^^^^^
                   ❌ 错误！type_info 不能是 constexpr
    
    编译器：error: constexpr variable must be initialized 
            by a constant expression
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


    ✅ 对比：如果成员已知（可以工作）
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    constexpr auto type_info = type_of(^^Item::item_id);
                               ^^^^^^^^^^^^^^^^^^^^^^^
                               ✓ 整个表达式是 constexpr
    
    using FieldType = [:type_info:];  // ✓ 可以 splice
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


┌─────────────────────────────────────────────────────────────────┐
│ 步骤 5: 调用 TypeSignature（需要实际类型）                         │
└─────────────────────────────────────────────────────────────────┘

    return TypeSignature<FieldType>::calculate();
                         ^^^^^^^^^
                         需要：uint32_t（类型）
                         实际：无法获得（splice 失败）


┌─────────────────────────────────────────────────────────────────┐
│                         问题根源                                  │
└─────────────────────────────────────────────────────────────────┘

    ┌──────────────────────────────────────────────────────────┐
    │  P2996 API 设计                                           │
    ├──────────────────────────────────────────────────────────┤
    │  std::vector<info> nonstatic_data_members_of(info);      │
    │                    ^^^^                                   │
    │                    堆分配！                               │
    └──────────────────────────────────────────────────────────┘
                            ↓
    ┌──────────────────────────────────────────────────────────┐
    │  在 consteval 函数中                                      │
    ├──────────────────────────────────────────────────────────┤
    │  - 函数在编译期执行 ✓                                     │
    │  - 但 vector 是堆分配 ✗                                   │
    │  - vector 不是 constexpr 对象 ✗                          │
    │  - vector[i] 不是 constexpr 表达式 ✗                     │
    └──────────────────────────────────────────────────────────┘
                            ↓
    ┌──────────────────────────────────────────────────────────┐
    │  Splice 语法要求                                          │
    ├──────────────────────────────────────────────────────────┤
    │  [:expr:] 要求 expr 是 constexpr 常量表达式               │
    │                      ^^^^^^^^                             │
    │                      不满足！                             │
    └──────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────┐
│                  consteval vs constexpr                          │
└─────────────────────────────────────────────────────────────────┘

    consteval 函数                    constexpr 变量
    ━━━━━━━━━━━━━━━━                  ━━━━━━━━━━━━━━━━━━
    • 在编译期执行                    • 编译期常量
    • 可以有运行时变量                • 初始化器必须是常量表达式
    • 可以堆分配                      • 不能堆分配
    
    
    示例：
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    consteval auto f() {
        auto vec = std::vector<int>{1, 2, 3};  // ✓ OK（编译期执行）
        constexpr auto x = vec[0];             // ❌ 错误（vec 不是 constexpr）
        return vec[0];                         // ✓ OK（返回值可以）
    }
    
    constexpr int result = f();  // ✓ OK（f() 的结果是 constexpr）


┌─────────────────────────────────────────────────────────────────┐
│                      关键对比                                     │
└─────────────────────────────────────────────────────────────────┘

    工作的：test_member_iteration.cpp
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    template<typename T, size_t Index>
    consteval auto get_member_info_at() -> MemberInfo {
        auto members = nonstatic_data_members_of(^^T);
        auto member = members[Index];
        
        return MemberInfo{
            display_string_of(member).data(),          // ✓ 返回字符串
            display_string_of(type_of(member)).data(), // ✓ 返回字符串
            is_public(member),                         // ✓ 返回 bool
            is_static_member(member)                   // ✓ 返回 bool
        };
        // ✓ 不需要 splice！
    }
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    
    不工作的：自动类型签名生成
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    template<typename T, size_t Index>
    consteval auto get_field_signature() {
        auto members = nonstatic_data_members_of(^^T);
        auto member = members[Index];
        auto type_info = type_of(member);
        
        using FieldType = [:type_info:];  // ❌ 需要 splice！
        //                ^^^^^^^^^^^^^
        //                type_info 不是 constexpr
        
        return TypeSignature<FieldType>::calculate();
    }
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


┌─────────────────────────────────────────────────────────────────┐
│                  需要的解决方案                                    │
└─────────────────────────────────────────────────────────────────┘

    方案 1：Constexpr-friendly API
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 理想的 API（不存在）
    constexpr std::span<const info> members_constexpr(info);
    //        ^^^^^^^^ 返回 constexpr 范围
    
    constexpr auto members = members_constexpr(^^T);  // ✓ constexpr
    constexpr auto member = members[Index];           // ✓ constexpr
    constexpr auto type_info = type_of(member);       // ✓ constexpr
    using FieldType = [:type_info:];                  // ✓ 可以 splice
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    
    方案 2：Template for 支持
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    template for (constexpr auto member : ^^T) {
        //        ^^^^^^^^ member 是 constexpr
        constexpr auto type_info = type_of(member);   // ✓ constexpr
        using FieldType = [:type_info:];              // ✓ 可以 splice
    }
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    
    方案 3：手动特化（当前唯一可行）
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    template <>
    struct TypeSignature<Item> {
        static constexpr auto calculate() {
            return CompileString{"struct[s:48,a:8]{"} +
                   CompileString{"@0:u32[s:4,a:4],"} +
                   CompileString{"@4:u32[s:4,a:4],"} +
                   // ...
                   CompileString{"}"};
        }
    };
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


┌─────────────────────────────────────────────────────────────────┐
│                         总结                                      │
└─────────────────────────────────────────────────────────────────┘

    需要的 Splice 操作：
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    auto type_info = type_of(members[Index]);
    using FieldType = [:type_info:];  ← 这个 splice
    TypeSignature<FieldType>::calculate();
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    为什么失败：
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    1. nonstatic_data_members_of() 返回堆分配的 vector
    2. members[Index] 不是 constexpr 表达式
    3. type_info 不能是 constexpr
    4. splice 要求 constexpr，但 type_info 不满足
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    当前解决方案：
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    手动为每个类型编写 TypeSignature 特化
    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 参考文档

- **详细说明**: `docs/SPLICE_OPERATIONS_EXPLAINED.md`
- **编译期 vs constexpr**: `docs/COMPILE_TIME_VS_CONSTEXPR.md`
- **类型签名限制**: `docs/TYPE_SIGNATURE_LIMITATION.md`
- **自动生成调研**: `docs/AUTO_TYPE_SIGNATURE_RESEARCH.md`
