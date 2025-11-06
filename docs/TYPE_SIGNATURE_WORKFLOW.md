# 类型签名计算流程可视化对比

## 完整工作流程对比

### next_practical (Boost.PFR) 方案

```
┌─────────────────────────────────────────────────────────────────┐
│                      开发阶段                                    │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 1. 定义运行时类型 (有构造函数)                                  │
│                                                                  │
│    struct GameData {                                            │
│        template <typename Allocator>                            │
│        GameData(Allocator allocator)                            │
│            : name(allocator), items(allocator) {}               │
│                                                                  │
│        int32_t player_id;                                       │
│        float health;                                            │
│        XString name;                                            │
│        XVector<int32_t> items;                                  │
│    };                                                           │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. 定义 ReflectionHint (聚合类型)                               │
│                                                                  │
│    struct GameDataReflectionHint {                              │
│        int32_t player_id;    // 必须与 GameData 完全对应       │
│        float health;                                            │
│        XString name;                                            │
│        XVector<int32_t> items;                                  │
│    };                                                           │
│                                                                  │
│    ⚠️  必须手动保持两者内存布局一致!                            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. 注册 reflection_hint trait                                   │
│                                                                  │
│    template<>                                                   │
│    struct reflection_hint<GameData> {                           │
│        using type = GameDataReflectionHint;                     │
│    };                                                           │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│                    编译阶段 - 类型签名生成                       │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ get_XTypeSignature<GameDataReflectionHint>()                    │
│                                                                  │
│  ├─ TypeSignature<GameDataReflectionHint>::calculate()         │
│  │   ├─ 检查: std::is_aggregate_v<T> ✓                         │
│  │   ├─ 获取大小: sizeof(T) = 72                               │
│  │   ├─ 获取对齐: alignof(T) = 8                               │
│  │   └─ 调用 get_fields_signature<T>()                         │
│  │                                                               │
│  ├─ get_fields_signature<T, 0>() // 递归开始                   │
│  │   ├─ boost::pfr::structure_to_tuple() → tuple<int32_t,...> │
│  │   ├─ std::tuple_element_t<0> → int32_t                      │
│  │   ├─ get_field_offset<T, 0>() → 0                           │
│  │   │   └─ 返回 0 (第一个字段)                                │
│  │   ├─ TypeSignature<int32_t>::calculate() → "i32[s:4,a:4]"  │
│  │   └─ "@0:i32[s:4,a:4]" + get_fields_signature<T, 1>()      │
│  │                                                               │
│  ├─ get_fields_signature<T, 1>() // 递归继续                   │
│  │   ├─ std::tuple_element_t<1> → float                        │
│  │   ├─ get_field_offset<T, 1>() → 4                           │
│  │   │   ├─ prev_offset = get_field_offset<T, 0>() = 0         │
│  │   │   ├─ prev_size = sizeof(int32_t) = 4                    │
│  │   │   ├─ curr_align = alignof(float) = 4                    │
│  │   │   └─ (0 + 4 + 4 - 1) & ~3 = 4                           │
│  │   ├─ TypeSignature<float>::calculate() → "f32[s:4,a:4]"    │
│  │   └─ ",@4:f32[s:4,a:4]" + get_fields_signature<T, 2>()     │
│  │                                                               │
│  ├─ get_fields_signature<T, 2>() // 继续                       │
│  │   ├─ std::tuple_element_t<2> → XString                      │
│  │   ├─ get_field_offset<T, 2>() → 8                           │
│  │   │   ├─ prev_offset = 4, prev_size = 4                     │
│  │   │   ├─ curr_align = alignof(XString) = 8                  │
│  │   │   └─ (4 + 4 + 8 - 1) & ~7 = 8                           │
│  │   ├─ TypeSignature<XString>::calculate() → "string[s:32,a:8]"│
│  │   └─ ",@8:string[s:32,a:8]" + get_fields_signature<T, 3>() │
│  │                                                               │
│  ├─ get_fields_signature<T, 3>() // 最后一个                   │
│  │   ├─ std::tuple_element_t<3> → XVector<int32_t>            │
│  │   ├─ get_field_offset<T, 3>() → 40                          │
│  │   │   ├─ prev_offset = 8, prev_size = 32                    │
│  │   │   ├─ curr_align = 8                                     │
│  │   │   └─ (8 + 32 + 8 - 1) & ~7 = 40                         │
│  │   ├─ TypeSignature<XVector<int32_t>>::calculate()          │
│  │   │   → "vector[s:32,a:8]<i32[s:4,a:4]>"                   │
│  │   └─ ",@40:vector[s:32,a:8]<i32[s:4,a:4]>" +               │
│  │       get_fields_signature<T, 4>()                          │
│  │                                                               │
│  └─ get_fields_signature<T, 4>() // 递归终止                   │
│      └─ Index >= tuple_size_v<T>, 返回 ""                      │
│                                                                  │
│  结果拼接:                                                       │
│  "struct[s:72,a:8]{" +                                          │
│    "@0:i32[s:4,a:4]" +                                          │
│    ",@4:f32[s:4,a:4]" +                                         │
│    ",@8:string[s:32,a:8]" +                                     │
│    ",@40:vector[s:32,a:8]<i32[s:4,a:4]>" +                     │
│  "}"                                                            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│                      运行时使用                                  │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  XBufferExt xbuf(8192);                                         │
│  auto* game = xbuf.make<GameData>("save");  // 使用运行时类型  │
│                                                                  │
│  ⚠️  运行时对象是 GameData (有构造函数)                         │
│  ⚠️  编译期检查用 GameDataReflectionHint (聚合类型)            │
└─────────────────────────────────────────────────────────────────┘

════════════════════════════════════════════════════════════════════

### next_cpp26 (C++26 Reflection) 方案

┌─────────────────────────────────────────────────────────────────┐
│                      开发阶段                                    │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ 1. 定义类型 (只需要一个!)                                       │
│                                                                  │
│    struct GameData {                                            │
│        template <typename Allocator>                            │
│        GameData(Allocator allocator)                            │
│            : name(allocator), items(allocator) {}               │
│                                                                  │
│        int32_t player_id;                                       │
│        float health;                                            │
│        XString name;                                            │
│        XVector<int32_t> items;                                  │
│    };                                                           │
│                                                                  │
│    ✓ 不需要 ReflectionHint                                      │
│    ✓ 不需要 reflection_hint trait                              │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│                    编译阶段 - 类型签名生成                       │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ get_XTypeSignature<GameData>()                                  │
│                                                                  │
│  ├─ TypeSignature<GameData>::calculate()                       │
│  │   ├─ 检查: std::is_class_v<T> ✓                             │
│  │   ├─ 获取大小: sizeof(T) = 72                               │
│  │   ├─ 获取对齐: alignof(T) = 8                               │
│  │   └─ 调用 get_fields_signature<T>()                         │
│  │                                                               │
│  ├─ get_fields_signature<GameData>()                           │
│  │   ├─ get_member_count<GameData>() → 4                       │
│  │   │   └─ nonstatic_data_members_of(^^GameData).size() = 4  │
│  │   │                                                           │
│  │   └─ concatenate_field_signatures<GameData>(                │
│  │         std::make_index_sequence<4>{})                       │
│  │       // Fold expression 一次性展开:                         │
│  │       // (field<0>() + field<1>() + field<2>() + field<3>())│
│  │                                                               │
│  ├─ [并行展开] get_field_signature<GameData, 0>()              │
│  │   ├─ auto member = nonstatic_data_members_of(^^GameData)[0]│
│  │   ├─ using FieldType = [:type_of(member):] → int32_t       │
│  │   ├─ offset = offset_of(member).bytes → 0                   │
│  │   │   └─ ✓ 编译器直接提供!                                  │
│  │   └─ "@0:i32[s:4,a:4]"                                      │
│  │                                                               │
│  ├─ [并行展开] get_field_signature<GameData, 1>()              │
│  │   ├─ auto member = members[1]                               │
│  │   ├─ using FieldType = [:type_of(member):] → float         │
│  │   ├─ offset = offset_of(member).bytes → 4                   │
│  │   │   └─ ✓ 编译器直接提供!                                  │
│  │   └─ ",@4:f32[s:4,a:4]"                                     │
│  │                                                               │
│  ├─ [并行展开] get_field_signature<GameData, 2>()              │
│  │   ├─ auto member = members[2]                               │
│  │   ├─ using FieldType = [:type_of(member):] → XString       │
│  │   ├─ offset = offset_of(member).bytes → 8                   │
│  │   │   └─ ✓ 编译器直接提供!                                  │
│  │   └─ ",@8:string[s:32,a:8]"                                 │
│  │                                                               │
│  └─ [并行展开] get_field_signature<GameData, 3>()              │
│      ├─ auto member = members[3]                               │
│      ├─ using FieldType = [:type_of(member):] → XVector<int32_t>│
│      ├─ offset = offset_of(member).bytes → 40                  │
│      │   └─ ✓ 编译器直接提供!                                  │
│      └─ ",@40:vector[s:32,a:8]<i32[s:4,a:4]>"                  │
│                                                                  │
│  结果拼接 (通过 fold expression):                               │
│  "struct[s:72,a:8]{" +                                          │
│    "@0:i32[s:4,a:4]" +                                          │
│    ",@4:f32[s:4,a:4]" +                                         │
│    ",@8:string[s:32,a:8]" +                                     │
│    ",@40:vector[s:32,a:8]<i32[s:4,a:4]>" +                     │
│  "}"                                                            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│                      运行时使用                                  │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  XBufferExt xbuf(8192);                                         │
│  auto* game = xbuf.make<GameData>("save");  // 直接使用!        │
│                                                                  │
│  ✓ 运行时和编译期使用同一个类型                                 │
└─────────────────────────────────────────────────────────────────┘

════════════════════════════════════════════════════════════════════

## 关键差异总结

┌─────────────────────┬─────────────────────┬─────────────────────┐
│      特性           │   next_practical    │    next_cpp26       │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 类型定义数量        │ 2 个 (运行时+Hint)  │ 1 个                │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 偏移量计算          │ 手动递归计算        │ 编译器直接提供      │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 成员遍历            │ 递归模板展开        │ Fold expression     │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 类型限制            │ 必须是聚合类型      │ 任何类类型          │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 维护成本            │ 高 (手动同步)       │ 低 (自动)           │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 编译时间            │ 慢 (深度递归)       │ 快 (并行展开)       │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 精确度              │ 依赖手动计算        │ 编译器保证          │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 可用性              │ 现在 (C++20)        │ 未来 (C++26)        │
└─────────────────────┴─────────────────────┴─────────────────────┘

## 实际计算对比

### 示例: 计算 health 字段的偏移量

#### next_practical (手动):
```
字段: float health
位置: 第 1 个字段 (Index = 1)

步骤:
1. prev_offset = get_field_offset<T, 0>() = 0
2. prev_size = sizeof(int32_t) = 4
3. curr_align = alignof(float) = 4
4. 计算: (0 + 4 + 4 - 1) & ~(4 - 1)
        = 7 & 0xFFFFFFFC
        = 4

递归调用链:
get_field_offset<T, 1>()
  └─ get_field_offset<T, 0>()
      └─ return 0
```

#### next_cpp26 (编译器):
```
字段: float health
位置: 第 1 个字段 (Index = 1)

步骤:
1. auto members = nonstatic_data_members_of(^^T)
2. auto member = members[1]
3. offset = offset_of(member).bytes  // 直接获取
4. return 4

✓ 无递归
✓ 编译器保证正确
✓ 一行代码
```

## 错误处理对比

### next_practical:
```cpp
// 如果忘记定义 ReflectionHint
struct NewType {
    template <typename Allocator>
    NewType(Allocator a) {}
    int value;
};

// 编译错误:
// error: static assertion failed: Type is not supported for automatic reflection
// note: 'std::is_aggregate_v<NewType>' evaluates to false

// 需要手动添加:
struct NewTypeReflectionHint { int value; };
template<> struct reflection_hint<NewType> { 
    using type = NewTypeReflectionHint; 
};
```

### next_cpp26:
```cpp
// 直接使用即可
struct NewType {
    template <typename Allocator>
    NewType(Allocator a) {}
    int value;
};

// ✓ 编译成功
auto sig = get_XTypeSignature<NewType>();
// "struct[s:4,a:4]{@0:i32[s:4,a:4]}"
```
