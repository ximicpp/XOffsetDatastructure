## Context

项目目前有 27 个测试（7 基础 + 20 反射），权威来源是：
- `tests/CMakeLists.txt`（REFLECTION_TESTS 列表 + 7 个基础测试）
- `build.sh`（run_test 调用）
- `tests/README.md`（已同步）

然而 `docs/` 下的 3 个文档各自声称不同的测试数量（18、30），且 `QUICK_REFERENCE.md` 中列出了大量已不存在的测试名。根目录 `README.md` 的 Docker 命令示例也缺少 `bash` 前缀。

## Goals / Non-Goals

**Goals:**
- 将 `docs/QUICK_REFERENCE.md`、`docs/README.md`、`docs/ZERO_BOILERPLATE.md` 中的测试数量和测试列表修正为与实际代码一致的 27 个
- 修正 `README.md` 中的 Docker 命令示例
- 确保所有文档中的测试列表与 `tests/CMakeLists.txt` 完全一致

**Non-Goals:**
- 不重写文档的整体结构或内容风格
- 不修改 `docs/technical_overview.md` 的定位（保留为 CppCon talk outline）
- 不修改代码或测试文件
- 不更新 `AGENTS.md` 或 `tests/README.md`（这两个已经是正确的）

## Decisions

### D1: 权威来源为 `tests/CMakeLists.txt`

所有测试数量和名称以 `tests/CMakeLists.txt` 的 `REFLECTION_TESTS` 列表 + 7 个基础测试为准。不从 `build.sh` 或其他文档派生。

**理由**: CMakeLists.txt 是构建系统的唯一真相来源，build.sh 和 tests/README.md 已与之同步。

### D2: `docs/QUICK_REFERENCE.md` 测试矩阵整段替换

不逐条修补旧列表，而是用与 `tests/README.md` 对齐的新列表整段替换。

**理由**: 旧列表中有 10 个测试名不存在、缺失 7 个实际测试，逐条修补容易遗漏。

### D3: 数字全局搜索替换

在每个目标文件中搜索所有提及测试数量的位置（"18"、"30"等），逐一替换为正确值。

**理由**: 某些文件中测试数量在多处出现（如 QUICK_REFERENCE.md 的正文和检查清单中都有），必须全部修正。

## Risks / Trade-offs

- **[Risk] 未来测试增减时文档再次过期** → 这是结构性问题，本次不解决。可考虑未来自动生成测试列表。
- **[Risk] 遗漏某处数字** → 使用 grep 全局搜索 "18 个测试"、"30 个" 等模式确保无遗漏。
