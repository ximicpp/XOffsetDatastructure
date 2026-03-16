## Context

`xoffsetdatastructure.hpp` 的代码和注释已面向 TypeLayout main 最新 API 编写，
但子模块指针停留在旧 commit `99b78ec`（`v0.2.0-structural` 分支），
落后 `origin/main` 约 20 个 commit。这导致当前子模块缺少以下关键头文件和符号：

| XOffset 引用 | 当前子模块 | main 最新 |
|---|---|---|
| `serialization_free.hpp` | ❌ | ✅ |
| `classify.hpp` | ❌ | ✅ |
| `layout_traits<T>` | ❌ | ✅ |
| `is_local_serialization_free_v<T>` | ❌ | ✅ |
| `is_transfer_safe<T>(sig)` | ❌ | ✅ |
| `classify_v<T>` | ❌ | ✅ |
| `SafetyLevel` (5级) | ❌ (旧3级在 `compat`) | ✅ |
| `TYPELAYOUT_OPAQUE_*_RELOCATABLE` | ❌ | ✅ |

同时，注释区存在三类过时问题：
1. 引用不存在的宏名 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE`（第 64 行）
2. 两个紧邻的注释块（第 45-60 行和第 61-65 行）内容重复
3. 上述问题虽然不影响功能逻辑，但会误导维护者

## Goals / Non-Goals

**Goals:**
- 将 TypeLayout 子模块升级到 `origin/main` HEAD，使所有 `#include` 和 `using` 声明可正确解析
- 修正 `xoffsetdatastructure.hpp` 中引用不存在 API 名称的注释
- 合并/精简重复注释块
- Docker 构建验证 23/23 测试通过

**Non-Goals:**
- 不修改任何 C++ 代码逻辑（仅注释文本）
- 不修改测试文件
- 不引入新的 API 或功能
- 不处理 TypeLayout 可能引入的 breaking changes（如有则回退并另建 change）

## Decisions

### Decision 1: 子模块直接追踪 main HEAD

**选择**: `git checkout origin/main` 然后 `git add external/typelayout`

**理由**: `type-signature` spec 已明确要求 "分支: 跟踪 `main` 分支"。
TypeLayout main 的每个 commit 都有 CI 验证，风险可控。

**替代方案**: 选择特定 tag（如 `v0.3.0`）—— 拒绝，因为目前 main 尚无新的
release tag，且 XOffset 代码已面向 main HEAD 编写。

### Decision 2: 注释修改范围最小化

**选择**: 仅修改以下三处注释：
1. 第 64 行 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` → `serialization_free_assert<T>`
2. 第 45-65 行合并为一个注释块，删除重复的第 61-65 行块
3. 不改动其他注释（如 Domain S 公式），因为升级后它们已经准确

**理由**: 最小改动原则。Domain S 注释、API 列表注释（第 33-38 行）
在子模块升级后都能与实际 API 对应，无需修改。

### Decision 3: 构建验证策略

**选择**: 升级子模块后立即 Docker 构建验证全部 23 个测试

**理由**: TypeLayout main 可能有签名格式变化，需要确认 XOffset 的
`is_xbuffer_safe<T>` 等编译期检查仍能通过。

## Risks / Trade-offs

- **[Risk] TypeLayout main 的 API 签名不兼容** →
  Docker 构建会立即暴露编译错误；如果出现，回退子模块并另建 change 处理
- **[Risk] 签名格式变化导致 `.sig.hpp` 文件过时** →
  `tools/sigs/` 中的签名文件可能需要重新生成；在构建验证中检查
- **[Trade-off] 注释精简可能丢失有价值的上下文** →
  仅删除重复内容，保留 Domain S 公式和 API 列表等有价值的说明
