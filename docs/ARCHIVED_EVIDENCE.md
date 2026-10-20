# 已归档的交接与 C14 迭代证据

默认实现切换为 W9a（提交 `14b57637d6fa57f8521138a0997fb3c8736db4f1`，经 PR #2 合并到 `next_cpp26` = `9e3602f977e984b5d4d87bee68c2f369efbf09e8`）后，以下交接与 C14 迭代证据已从 `next_cpp26` 工作树移除。内容未修改、未删除历史，仍可从 Git tag 原样取回。

## 移除内容

| 路径 | 文件数 | 字节 |
| --- | ---: | ---: |
| `handoff/` | 26 | 176,729,761 |
| `docs/coordination/` | 1,131 | 72,826,504 |
| `tools/c14_handoff_20261020/` | 2 | 13,769 |
| `tools/c14_iteration_20261019/` | 104 | 643,015 |
| `tools/c14_iteration_20261020_v2/` | 379 | 3,243,759 |
| `accepted-executed/` | 4 | 119,619 |
| `accepted-generated/` | 5 | 282,840 |
| **合计** | **1,651** | **253,859,267（约 254 MB）** |

保留未动：`docs/validation/`（`tools/tests/test_validate_vm.py` 读取其中的 `2026-10-15-vm-followup`）、`experiments/`、`docs/perf/`、`docs/handoff-history/`、`openspec/`，以及 `tools/*.py`（含各 `verify_*handoff*` 脚本）。

## 存放位置

- **`archive/codex-mac-continuation-20261020`**（指向 `9e3602f`）：包含上表全部 7 个路径，推荐使用。
- `archive/claude-mac-candidates-20261020`：同样包含全部 7 个路径。
- `archive/codex-cross-machine-handoff-20261019`、`archive/codex-vm-handoff-20261016`：仅包含当时已有的 `handoff/`、`docs/coordination/`、`accepted-executed/`、`accepted-generated/`（内容为当时版本）。

## 恢复

在仓库根目录执行（只恢复文件到工作树，不切换分支）：

```sh
git fetch origin --tags
git checkout archive/codex-mac-continuation-20261020 -- \
  handoff docs/coordination \
  tools/c14_handoff_20261020 tools/c14_iteration_20261019 tools/c14_iteration_20261020_v2 \
  accepted-executed accepted-generated
```

如只需查看，可用 `git worktree add <新目录> archive/codex-mac-continuation-20261020` 单独检出，避免改动当前分支。

## 依赖这些路径的工具

以下脚本仍保留在 `tools/`，但运行前需先按上文恢复对应路径，否则会因找不到输入而失败：

- `tools/verify_mac_handoff_20261020.py`（`handoff/mac-20261020/TRANSFER_MANIFEST.json`、`handoff/github-20261019/C14_MATERIALIZATION.json`）
- `tools/verify_c14_evidence_bundle_20261020.py`（`handoff/mac-20261020/evidence/BUNDLE.json`）
- `tools/restore_handoff_20261019.py`（`handoff/XOffset-continuation-20261019-v1.zip` 等）
- `tools/verify_vm_handoff.py`（`handoff/manifest.json`）

各 `HANDOFF*.md` / `PROMPT_MAC_20261020.md` 中指向上述路径的链接同理，需恢复后才能打开。
