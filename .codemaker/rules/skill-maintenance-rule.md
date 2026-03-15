# Rule: Skill Maintenance — 自动检查与更新

## 规则概述
每次执行 `.codemaker/skills/` 中定义的操作后，必须对照相关 skill 文件检查实际执行情况，如有差异则立即更新 skill 文件。

---

## 触发条件与对应 Skill

### 🔴 强制触发（操作完成后必须检查）

| 触发操作 | 检查的 Skill 文件 | 检查内容 |
|----------|-------------------|----------|
| 在 Docker 中构建测试 | `docker-build-test.md` | 镜像名、命令、输出格式是否一致 |
| 本地构建测试 | `native-build-test.md` | 编译器路径、命令选项是否一致 |
| `git commit` + `git push` | `git-commit-push.md` | 分支名、commit 规范是否一致 |
| `git pull` | `git-pull-latest.md` | 分支名、子模块更新提示是否一致 |
| 子模块操作 | `submodule-*.md` | URL、路径、版本是否一致 |
| CI 失败排查 | `ci-debug.md` | 失败模式、修复方式是否需要补充 |
| 签名文件检查 | `signature-verify.md` | 签名文件路径、格式是否变化 |

### 🟡 条件触发（修改基础设施文件后必须检查）

| 修改的文件 | 检查的 Skill 文件 |
|-----------|-------------------|
| `build.sh` | `docker-build-test.md`, `native-build-test.md`, `ctest-verbose.md` |
| `.github/workflows/ci.yml` | `check-ci-status.md`, `ci-debug.md` |
| `CMakeLists.txt` / `tests/CMakeLists.txt` | `run-single-test.md`, `ctest-verbose.md` |
| `.gitmodules` | `submodule-update.md`, `submodule-verify.md` |
| `AGENTS.md` | `update-agents-md.md` |
| `docker-compose.yml` / `Dockerfile` | `docker-build-test.md` |

### 🟢 择机触发（积累经验后批量更新）

| 场景 | 操作 |
|------|------|
| 完成一轮完整的代码审计 | 更新 `deep-code-audit.md` 的审计方法论和输出模板 |
| 多次执行安全检查后 | 更新 `safety-check.md` 的常见模式列表 |
| 发现新的 CI 失败模式 | 补充到 `ci-debug.md` 的常见失败模式 |

---

## 更新流程

### Step 1: 检查
操作完成后，对比实际执行的命令/步骤与 skill 文件中的描述。

### Step 2: 判断
- **完全一致** → 不更新，跳过
- **有差异** → 进入 Step 3

### Step 3: 更新
修改 skill 文件中不一致的部分。更新范围包括：
- 命令/路径/参数变更
- 新增步骤或注意事项
- 补充常见问题或最佳实践
- 移除已过时的内容

### Step 4: 通知
在任务汇报中附带说明：
> "顺便更新了 skill `xxx`：补充了 yyy 步骤 / 修正了 zzz 命令"

---

## 约束

1. **不要过度更新**：如果只是时间戳或无关紧要的格式差异，不需要更新
2. **不要遗漏更新**：如果命令、路径、镜像名等关键信息有变，必须更新
3. **保持原子性**：每次只更新受影响的 skill 文件，不要无关联地批量修改
4. **跨会话持久**：此规则文件确保即使新建对话，skill 维护行为仍然生效
