# Change: 分析 TypeLayout 功能缺口并提出增强建议

## Why

XOffsetDatastructure 作为 TypeLayout 的第一个深度集成用户，在实际使用中暴露出 TypeLayout 的多个功能缺口。这些缺口限制了 XOffsetDatastructure 的开发效率和运行时诊断能力。本提案从 XOffsetDatastructure 的**实际需求**出发，系统性分析 TypeLayout 应添加的功能，为 TypeLayout 的下一步开发提供路线图。

## What Changes

本提案是**纯分析性**工作，不涉及代码修改。产出为一份分析文档，列出 TypeLayout 需要添加的功能及优先级。

具体分析的功能缺口：

### 🔴 高优先级（直接影响 XOffsetDatastructure 核心功能）

1. **编译时签名哈希 API**
   - 需求来源：`player.hpp` 和 `game_data.hpp` 中的 `static_assert` 使用完整签名字符串比较，字符串很长（Player 签名 ~200 字符），难以维护
   - 缺口：TypeLayout 没有提供 `consteval uint64_t signature_hash<T>()` 用于编译时哈希比较
   - 价值：可以用短哈希替代长字符串，如 `static_assert(definition_signature_hash<Player>() == 0xABCD1234ULL)`

2. **签名差异诊断 API**
   - 需求来源：当 `static_assert(get_definition_signature<T>() == "...")` 失败时，编译器只报"assertion failed"，无法定位具体哪个字段变化了
   - 缺口：TypeLayout 没有提供签名 diff 功能
   - 价值：可以输出 `Field 'mName' type changed from 'string[s:32,a:8]' to 'string[s:64,a:8]'`

3. **Opaque 容器特化辅助宏/工具**
   - 需求来源：XOffsetDatastructure 为 XString/XVector/XSet/XMap 各写了独立的 `TypeSignature` 特化，重复代码多
   - 缺口：TypeLayout 没有提供简化特化注册的宏或基类
   - 价值：可以用一行宏 `TYPELAYOUT_OPAQUE_CONTAINER(XString, "string", 32, 8)` 替代 7 行特化代码

### 🟡 中优先级（提升工具链和诊断能力）

4. **签名版本标识**
   - 需求来源：签名格式（如 `record[s:N,a:M]{...}`）是隐式的，没有版本号。TypeLayout 升级可能改变签名格式
   - 缺口：没有格式版本前缀或版本查询 API
   - 价值：如 `[v2][64-le]record{...}` 或 `constexpr int signature_format_version = 2;`

5. **Safety 分级与 `is_xbuffer_safe` 集成**
   - 需求来源：XOffsetDatastructure 的 `is_xbuffer_safe<T>` 独立实现了一套类型安全检查（~200 行），而 TypeLayout 的 `classify_safety()` 已经能检测指针、位域、vptr
   - 缺口：TypeLayout 的 `classify_safety()` 是运行时 API（基于签名字符串扫描），不是编译时 API
   - 价值：提供 `consteval SafetyLevel classify_safety<T>()` 可以让 XOffsetDatastructure 复用 TypeLayout 的安全分级逻辑

6. **编译时成员迭代工具**
   - 需求来源：`XBufferCompactor::migrate_members` 使用 P2996 反射遍历所有成员。TypeLayout 内部已有类似的遍历逻辑（`definition_fields<T>`）但未作为 API 暴露
   - 缺口：TypeLayout 只暴露签名结果，不暴露中间的成员迭代能力
   - 价值：提供 `for_each_member<T>(callback)` 工具可以减少 XOffsetDatastructure 中的重复反射代码

### 🟢 低优先级（锦上添花）

7. **签名格式化/美化输出**
   - 需求来源：签名字符串在测试输出中很长，难以阅读
   - 缺口：TypeLayout 没有提供 pretty-print 函数
   - 价值：缩进格式化的签名更易读

8. **枚举类型的 XBuffer 安全检查**
   - 需求来源：XOffsetDatastructure 的 `is_safe_type` 未处理枚举，但枚举在游戏数据中很常见
   - 缺口：TypeLayout 已支持枚举签名，但没有提供 "枚举是否 POD 安全" 的判断工具
   - 价值：`is_trivial_enum<T>()` 可以简化 XOffsetDatastructure 的安全检查

9. **数组类型的 Opaque 签名支持**
   - 需求来源：XOffsetDatastructure 的 `T[N]` 固定数组在签名中显示为完整的 `array[s:N*sizeof(T),a:alignof(T)]<T,N>`，有时过于冗长
   - 缺口：TypeLayout 没有提供可选的紧凑数组签名模式
   - 价值：可选择简化为 `T[N]` 格式

## Impact

- **不影响任何代码**：本提案仅产出分析文档
- **直接影响 TypeLayout 库**：为 TypeLayout 下一步开发提供需求文档
- **间接影响 XOffsetDatastructure**：TypeLayout 添加新功能后，XOffsetDatastructure 可以简化内部代码
- Affected specs: `type-signature`
- Affected code: 无（纯分析）
