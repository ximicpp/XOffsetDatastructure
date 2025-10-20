# XOffsetDatastructure2 DSL Schema System

## ✅ 完成的功能

### 1. YAML Schema 定义
- 支持所有 XOffsetDatastructure2 类型
- 自动检测哪些字段需要 allocator
- 支持嵌套类型和默认值

### 2. 双类型生成
- **Runtime 类型**：带 allocator 构造函数，用于运行时
- **ReflectionHint 类型**：aggregate 类型，用于 boost::pfr 反射

### 3. 规范保证
- 强制 `alignas(XTypeSignature::BASIC_ALIGNMENT)`
- 自动生成正确的 allocator 初始化列表
- 可选的注释掉的 copy/assignment 删除

---

## 📝 使用示例

### 定义 Schema (YAML)

```yaml
# schemas/game_data.xds.yaml
schema_version: "1.0"

types:
  - name: Item
    type: struct
    fields:
      - name: id
        type: int
        default: 0
      
      - name: name
        type: XString
      
      - name: quantity
        type: int
        default: 1

  - name: Player
    type: struct
    fields:
      - name: playerId
        type: int
        default: 0
      
      - name: level
        type: int
        default: 1
      
      - name: health
        type: float
        default: 100.0
      
      - name: name
        type: XString
      
      - name: inventory
        type: XVector<Item>
      
      - name: achievements
        type: XSet<int>

codegen:
  output_dir: "generated"
  generate_copy_delete: true
```

### 生成代码

```bash
python3 tools/xds_generator.py schemas/game_data.xds.yaml -o generated/
```

### 生成的 Runtime 类型

```cpp
struct alignas(XTypeSignature::BASIC_ALIGNMENT) Item {
    template <typename Allocator>
    Item(Allocator allocator) : name(allocator) {}
    // Item(const Item&) = delete;
    // Item& operator=(const Item&) = delete;
    int id{0};
    XString name;
    int quantity{1};
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
    template <typename Allocator>
    Player(Allocator allocator) 
        : name(allocator), 
          inventory(allocator), 
          achievements(allocator) {}
    // Player(const Player&) = delete;
    // Player& operator=(const Player&) = delete;
    int playerId{0};
    int level{1};
    float health{100.0f};
    XString name;
    XVector<Item> inventory;
    XSet<int> achievements;
};
```

### 生成的 ReflectionHint 类型

```cpp
// ItemReflectionHint: Aggregate version of Item
// - Removes constructor to satisfy boost::pfr aggregate type requirement
// - Keeps identical field layout for type signature generation
struct alignas(XTypeSignature::BASIC_ALIGNMENT) ItemReflectionHint {
    int32_t id;
    XString name;
    int32_t quantity;
};

// PlayerReflectionHint: Aggregate version of Player
struct alignas(XTypeSignature::BASIC_ALIGNMENT) PlayerReflectionHint {
    int32_t playerId;
    int32_t level;
    float health;
    XString name;
    XVector<ItemReflectionHint> inventory;  // 注意：使用 ItemReflectionHint
    XSet<int32_t> achievements;
};
```

---

## 🎯 关键特性

### 1. 自动 Allocator 检测

生成器自动识别需要 allocator 的字段：

| 类型 | 需要 Allocator? |
|------|----------------|
| `int`, `float`, `double` | ❌ 否 |
| `XString` | ✅ 是 |
| `XVector<T>` | ✅ 是 |
| `XSet<T>` | ✅ 是 |
| `XMap<K,V>` | ✅ 是 |
| 自定义 struct | ✅ 是 |

### 2. 类型转换规则

Runtime → ReflectionHint：

```
int              → int32_t
float            → float
MyStruct         → MyStructReflectionHint
XVector<int>     → XVector<int32_t>
XVector<MyStruct> → XVector<MyStructReflectionHint>
XMap<XString, MyStruct> → XMap<XString, MyStructReflectionHint>
```

### 3. 默认值支持

```yaml
fields:
  - name: count
    type: int
    default: 0        # 生成: int count{0};
  
  - name: ratio
    type: float
    default: 1.5      # 生成: float ratio{1.5f};
  
  - name: active
    type: bool
    default: true     # 生成: bool active{true};
```

---

## 🔧 工具链

```
schemas/
  └── *.xds.yaml          # YAML schema definitions

tools/
  ├── xds_generator.py    # Code generator
  └── README.md           # Documentation

generated/
  └── *.hpp               # Auto-generated C++ headers
```

---

## 📦 与 CMake 集成

```cmake
# Find Python
find_package(Python3 REQUIRED)

# Schema files
set(SCHEMA_FILES
    ${CMAKE_SOURCE_DIR}/schemas/game_data.xds.yaml
    ${CMAKE_SOURCE_DIR}/schemas/network_data.xds.yaml
)

# Generate headers
foreach(SCHEMA_FILE ${SCHEMA_FILES})
    get_filename_component(SCHEMA_NAME ${SCHEMA_FILE} NAME_WE)
    set(GENERATED_HEADER ${CMAKE_BINARY_DIR}/generated/${SCHEMA_NAME}.hpp)
    
    add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND ${Python3_EXECUTABLE} 
                ${CMAKE_SOURCE_DIR}/tools/xds_generator.py 
                ${SCHEMA_FILE}
                -o ${CMAKE_BINARY_DIR}/generated/
        DEPENDS ${SCHEMA_FILE}
        COMMENT "Generating ${SCHEMA_NAME}.hpp"
    )
    
    list(APPEND GENERATED_HEADERS ${GENERATED_HEADER})
endforeach()

add_custom_target(generate_schemas ALL DEPENDS ${GENERATED_HEADERS})
include_directories(${CMAKE_BINARY_DIR}/generated)
```

---

## ✨ 优势总结

1. **规范保证**：所有生成的类型自动遵循 XOffsetDatastructure2 约定
2. **减少错误**：不会忘记添加 `alignas` 或 allocator 初始化
3. **双类型生成**：同时支持运行时和编译期反射
4. **易于维护**：修改 YAML 比手动修改 C++ 代码更安全
5. **跨平台一致性**：YAML 定义保证不同平台生成相同代码
6. **类型安全**：生成的代码经过编译器检查

---

## 🚀 下一步

建议的增强功能：

- [ ] Schema 验证器（JSON Schema）
- [ ] 支持 enum 类型
- [ ] 支持继承/组合
- [ ] 生成序列化代码
- [ ] 多文件 schema 和 import
- [ ] 自动生成测试代码
- [ ] IDE 插件（VSCode YAML schema）
