## Why

`docs/` 目录下的多个文档中，测试数量、测试名称列表与实际代码严重不一致。`QUICK_REFERENCE.md` 列出了 10 个已不存在的测试名、缺失 7 个实际存在的测试；`docs/README.md` 声称 18 个测试，`ZERO_BOILERPLATE.md` 声称 30 个，而实际是 27 个。此外 `README.md` 的 Docker 示例命令也存在错误。这些不一致会误导新用户和 AI Agent。

## What Changes

- **`docs/QUICK_REFERENCE.md`**: 完全重写测试矩阵部分，将测试列表替换为与 `tests/CMakeLists.txt` 一致的 27 个测试；修正所有提及测试数量的位置（"30" → "27"，"18" → "27"）
- **`docs/README.md`**: 将 "6个基础测试 + 12个反射测试 = 18个" 修正为 "7个基础测试 + 20个反射测试 = 27个"
- **`docs/ZERO_BOILERPLATE.md`**: 将 "30 tests total" 修正为 "27 tests total"（仅数字更正）
- **`README.md`**（项目根目录）: Docker 示例中 `./build.sh` 改为 `bash ./build.sh`

## Capabilities

### New Capabilities

（无新功能）

### Modified Capabilities

（无 spec 级别的行为变更 — 本次仅为文档同步修正，不涉及功能需求变化）

## Impact

- **文件**: `docs/QUICK_REFERENCE.md`, `docs/README.md`, `docs/ZERO_BOILERPLATE.md`, `README.md`
- **用户影响**: 文档一致性提升，新用户/AI Agent 不再被误导
- **代码影响**: 无
- **API 影响**: 无
- **依赖影响**: 无
