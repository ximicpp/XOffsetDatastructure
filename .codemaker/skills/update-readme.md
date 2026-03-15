# Skill: update-readme

## Description
更新项目各级 README 文件，保持对外文档的准确性和完整性。

## When to Use
- 新增功能或测试后
- 项目结构变化后
- 构建方式变化后
- 用户要求更新 README 时

## README File Locations
```
README.md          # 项目主 README（GitHub 首页展示）
docs/README.md     # 文档目录说明
tests/README.md    # 测试目录说明（如存在）
examples/          # 示例目录（如有 README）
```

## Steps

### Step 1: 确认需要更新的内容
常见更新项：
- 功能列表
- 安装/构建说明
- API 使用示例
- 测试覆盖说明
- 依赖关系
- 许可证信息

### Step 2: 更新主 README.md
```bash
# 查看当前内容
cat README.md
```

需要保持以下章节最新：
- 项目简介
- 快速开始
- 构建说明
- 功能特性
- 测试
- 文档链接

### Step 3: 更新测试 README
新增测试后，更新测试列表和说明：
```bash
# 列出所有测试文件
ls tests/test_*.cpp
```

### Step 4: 提交变更
```bash
git add README.md docs/README.md tests/README.md
git commit -m "docs: update README for <change-description>"
```

## README Template (Test Section)
```markdown
## Tests

| Test File | Description |
|-----------|-------------|
| `test_basic_types.cpp` | 基本类型序列化测试 |
| `test_vector.cpp` | XVector 容器测试 |
| `test_complex_nesting.cpp` | 复杂嵌套结构测试 |
| ... | ... |

### Running Tests
\```bash
./build.sh
\```
```

## Important Notes
- README.md 是项目的 "门面"，保持清晰简洁
- 代码示例必须可编译运行
- 避免过时的信息（如旧的 API、已弃用的特性）
- 使用 GitHub Markdown 格式（支持表格、折叠块等）
