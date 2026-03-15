# Skill: update-docs

## Description
更新 `docs/` 目录下的技术文档，保持文档与代码实现同步。

## When to Use
- 架构变更后
- 新增重要功能后
- 发现文档与实际实现不一致时
- 用户要求更新文档时

## Document Inventory
```
docs/
├── proposals/                         # 设计提案
├── ARCHITECTURE_REVIEW.md             # 架构审查报告
├── AUDIT_TODO.md                      # 审计待办事项
├── BUILD_AND_TEST_GUIDE.md            # 构建和测试指南
├── COMPILETIME_SAFETY_API_ANALYSIS.md # 编译时安全 API 分析
├── CORE_FORMAL_MODEL.md               # 核心形式化模型
├── core-features-analysis.md          # 核心功能分析
├── DEEP_ARCHITECTURE_ANALYSIS.md      # 深度架构分析
├── IMPLEMENTATION_REVIEW.md           # 实现审查
├── LONG_PORTABILITY_GUIDE.md          # long 类型可移植性指南
├── LONG_TYPE_ANALYSIS.md              # long 类型分析
├── MEMORY_LIFECYCLE_ANALYSIS.md       # 内存生命周期分析
├── MIGRATION_TYPELAYOUT.md            # TypeLayout 迁移指南
├── QUICK_REFERENCE.md                 # 快速参考
├── README.md                          # docs 目录说明
├── technical_overview.md              # 技术概述
├── TYPE_SUBSET_MODEL.md               # 类型子集模型
├── TYPELAYOUT_FEATURE_GAPS.md         # TypeLayout 功能缺口
├── TYPELAYOUT_INTEGRATION_ANALYSIS.md # TypeLayout 集成分析
├── ZERO_BOILERPLATE.md                # 零样板代码说明
└── *.pdf                              # 演示文稿/论文
```

## Steps

### Step 1: 识别需要更新的文档
根据代码变更影响范围确定相关文档：
- 类型安全变更 → `COMPILETIME_SAFETY_API_ANALYSIS.md`, `TYPE_SUBSET_MODEL.md`
- 架构变更 → `ARCHITECTURE_REVIEW.md`, `technical_overview.md`
- TypeLayout 集成变更 → `TYPELAYOUT_INTEGRATION_ANALYSIS.md`, `TYPELAYOUT_FEATURE_GAPS.md`
- 构建流程变更 → `BUILD_AND_TEST_GUIDE.md`

### Step 2: 更新文档内容
- 确保描述与最新代码一致
- 更新版本号（如有）
- 添加变更日期
- 更新代码示例

### Step 3: 检查交叉引用
确保文档之间的引用链接仍然有效。

### Step 4: 提交变更
```bash
git add docs/
git commit -m "docs: update <document-name> for <change-description>"
```

## Writing Guidelines
- 使用 Markdown 格式
- 代码块使用 ```cpp 标注
- 保持文档简洁、结构清晰
- 重要变更标注日期

## Important Notes
- 文档更新应与代码变更同步提交（或紧随其后）
- PDF 文件通常不需要更新（会议演示等静态内容）
- `AUDIT_TODO.md` 是活跃的待办清单，随审计进度更新
