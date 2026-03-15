# Skill: update-agents-md

## Description
维护 `AGENTS.md` 文件，这是 AI 编码助手的开发指南，包含构建命令、代码风格、开发实践等关键信息。

## When to Use
- 构建流程变更时（如新增构建选项、Docker 镜像更新）
- 代码风格约定变更时
- 新增测试/示例的标准模板变更时
- CI/CD 流程变更时
- 依赖关系变更时

## AGENTS.md Structure
```
AGENTS.md
├── ⚡ Local Testing Quick Reference
│   ├── Option A: Native P2996 Compiler
│   └── Option B: Docker
├── Build System
│   ├── Docker Build
│   ├── Primary Build Commands
│   ├── CMake Commands
│   ├── Running Tests
│   └── TypeLayout Library
├── Code Style Guidelines
│   ├── File Organization
│   ├── Include Order
│   ├── Namespace Conventions
│   ├── Naming Conventions
│   ├── Error Handling
│   ├── Test Structure
│   └── Platform-Specific Code
├── Development Practices
│   ├── Compiler Requirements
│   ├── Adding New Tests
│   ├── Adding New Examples
│   ├── Memory Management
│   ├── Reflection Code (C++26)
│   └── Performance Considerations
├── Documentation
└── Common Issues
```

## Steps

### Step 1: 确认需要更新的章节
根据具体变更，定位 AGENTS.md 中的对应章节。

### Step 2: 更新内容
```bash
# 查看当前 AGENTS.md
cat AGENTS.md
```

### Step 3: 保持一致性
确保 AGENTS.md 中的指令与以下文件一致：
- `build.sh` — 构建命令
- `.github/workflows/ci.yml` — CI 配置
- `CMakeLists.txt` — CMake 配置
- `docker-compose.yml` — Docker 配置

### Step 4: 提交变更
```bash
git add AGENTS.md
git commit -m "docs: update AGENTS.md - <change-description>"
```

## Key Sections to Watch

### 构建命令变更
如果 `build.sh` 新增了选项或改变了默认行为，必须同步更新：
- "Primary Build Commands" 章节
- "Local Testing Quick Reference" 章节

### Docker 镜像变更
如果更换了 Docker 镜像或标签：
- "Docker Build" 章节
- "Option B: Docker" 章节

### 新增测试模板
如果测试结构规范变更：
- "Test Structure" 章节
- "Adding New Tests" 章节

## Important Notes
- AGENTS.md 是 AI 助手的 "圣经"，信息必须准确
- 所有构建命令必须经过实际验证
- 代码示例必须与当前代码库一致
- 这是 `.codemaker/` 系统之外的独立指南文件
