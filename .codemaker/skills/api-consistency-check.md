# Skill: api-consistency-check

## Description
检查 XOffsetDatastructure 公开 API 的一致性，确保遵循 DRY 原则，消除重复逻辑和不一致行为。

## When to Use
- 新增或修改公开 API 后
- 发现多个函数有类似逻辑时
- 代码审计中的 API 层检查

## Key APIs to Check

### XBuffer 核心 API
```cpp
class XBuffer {
    template<typename T> T* make();           // 创建根对象
    template<typename T> XHandle<T> make_handle(); // 创建并返回 Handle
    template<typename T> T* root();           // 获取根对象指针
    std::string save() const;                 // 序列化
    static XBuffer load(const std::string&);  // 反序列化
};
```

### TypedXBuffer<T> API
```cpp
template<typename T>
class TypedXBuffer : public XBuffer {
    T* operator->();                          // 访问根对象
    T& operator*();                           // 解引用根对象
    static TypedXBuffer load(const std::string&); // 类型化加载
};
```

## Consistency Check Points

### 1. 验证逻辑一致性
- [ ] `make<T>()` 和 `make_handle<T>()` 是否共享相同的验证逻辑
- [ ] `TypedXBuffer::load()` 是否正确委托给 `XBuffer::load()`
- [ ] 所有入口点是否都调用 `validate_xbuffer_type<T>()`

### 2. 错误处理一致性
- [ ] 相同的错误条件是否产生相同的错误消息
- [ ] 异常类型是否统一
- [ ] 边界条件处理是否一致

### 3. DRY 原则检查
- [ ] 是否有两个函数包含几乎相同的实现
- [ ] 公共逻辑是否提取到 helper 函数
- [ ] 模板特化是否必要，还是可以用 `if constexpr` 替代

### 4. 命名一致性
- [ ] 类名 PascalCase, 函数名 snake_case, 成员 mPascalCase
- [ ] 相似功能使用相似命名模式
- [ ] 参数名称有意义且一致

## Steps

### Step 1: 列出所有公开 API
搜索所有 `public:` 区域的方法签名。

### Step 2: 分组比较
将功能相似的 API 分组，逐组检查一致性。

### Step 3: 检查调用链
追踪每个 API 的内部调用链，确认共享逻辑。

### Step 4: 记录不一致项
按 P0/P1/P2 分级记录发现。

## Important Notes
- API 变更可能影响所有使用方（测试 + 示例）
- 修改后必须运行完整测试套件
- 向后兼容性是首要考虑因素
