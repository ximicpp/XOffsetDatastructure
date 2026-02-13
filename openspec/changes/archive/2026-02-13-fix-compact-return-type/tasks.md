## 1. 核心库重构
- [ ] 1.1 将 `XBufferCompactor` 移至 `XBufferExt` 定义之后（或用前向声明）
- [ ] 1.2 `compact_automatic<T>()` 返回类型改为 `XBufferExt`
- [ ] 1.3 验证 `XBufferCompactor` 内部仍可使用 `XBuffer` 基类操作

## 2. Examples 清理
- [ ] 2.1 helloworld.cpp: 移除 `XBufferExt compacted_ext(...)` wrapper hack
- [ ] 2.2 demo.cpp: 移除 `XBufferExt compacted_ext(...)` wrapper hack
- [ ] 2.3 demo.cpp: 添加 XHandle 使用演示（新 Demo section 或融入现有 section）

## 3. 文档
- [ ] 3.1 README Rule 1 表格: 补充 `compact_automatic` 返回 `XBufferExt` 说明
- [ ] 3.2 examples/README.md: 更新相关代码示例

## 4. 验证
- [ ] 4.1 全量构建测试通过
