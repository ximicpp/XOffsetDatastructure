## Context

XOffsetDatastructure 项目当前使用自实现的 `XTypeSignature` (~270 行) 进行编译期类型签名。
TypeLayout 是同一团队维护的独立库，提供更完善的类型布局验证功能。需要将 TypeLayout 
作为外部依赖集成，替换内部签名实现。

**约束**:
- 两个仓库同属 `ximicpp` 组织，版本节奏可协调
- 两者共享 Bloomberg Clang P2996 编译器要求
- TypeLayout 是 header-only 库，无需编译链接
- 现有用户代码使用 `XTypeSignature` API，需要平滑过渡

## Goals / Non-Goals

### Goals
- 使用 Git Submodule 管理 TypeLayout 依赖，与 Boost 保持一致的依赖管理风格
- 通过兼容层实现零中断迁移
- 利用 TypeLayout 的 Definition Signature 增强签名能力

### Non-Goals
- 不在此变更中删除 `XTypeSignature` 命名空间（仅 deprecate）
- 不修改 TypeLayout 库本身的代码
- 不要求用户立即迁移到新 API

## Decisions

### Decision 1: Git Submodule（而非 FetchContent/Subtree）

**选择**: Git Submodule

**理由**:
- 与现有 Boost 依赖管理方式一致（`external/boost` 已是 submodule）
- 版本可精确锁定，避免构建时拉取不确定版本
- 开发者可以本地修改 TypeLayout 并 push
- CI/CD 生态成熟，GitHub Actions 原生支持

**备选方案**:
| 方案 | 优点 | 缺点 |
|------|------|------|
| FetchContent | 自动化好 | 依赖网络，版本不可预测 |
| Subtree | 单仓库体验 | 合并复杂，历史膨胀 |
| 复制头文件 | 简单 | 维护困难，版本脱节 |

### Decision 2: 兼容层策略

**选择**: 保留 `XTypeSignature` 命名空间 + `[[deprecated]]` 标记

**理由**:
- 现有 6+ 个文件使用 `get_XTypeSignature<T>()` 和 `TypeSignature<T>`
- 示例代码中有多处 `static_assert` 使用旧 API
- 一步删除会导致所有下游代码立即失效

**实施方式**:
1. 保留 `XTypeSignature` 命名空间框架
2. 旧 API 标记 `[[deprecated("Use boost::typelayout::get_definition_signature")]]`
3. 底层实现委托到 `boost::typelayout`

### Decision 3: 签名格式兼容

**问题**: XTypeSignature 和 TypeLayout 的签名格式不完全相同

| 特性 | XTypeSignature | TypeLayout |
|------|---------------|------------|
| 格式 | `record{@0:i32[s:4,a:4],@4:f32[s:4,a:4]}` | `[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:f32[s:4,a:4]}` |
| 平台前缀 | 无 | `[64-le]` |
| 结构体 size/align | 无 | `record[s:N,a:M]` |
| 字段名 | 无 | Definition 层包含 |

**选择**: 采用 TypeLayout 新格式

**理由**:
- 新格式信息更完整
- 包含平台信息对跨平台场景必要
- 现有 `static_assert` 中的硬编码签名字符串需要更新（在迁移指南中说明）

### Decision 4: include 路径组织

**选择**: 直接使用 TypeLayout 原生路径 `<boost/typelayout.hpp>`

**理由**:
- TypeLayout 遵循 Boost 命名约定
- 无需额外的路径映射或包装头文件
- 与 Boost 的 `external/boost` include 路径设置一致

**CMake 配置**:
```cmake
set(TYPELAYOUT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/typelayout/include)
target_include_directories(target PRIVATE ${TYPELAYOUT_INCLUDE_DIRS})
```

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 签名格式变化导致现有 `static_assert` 失败 | 编译错误 | 兼容层 + 迁移指南 + CI 验证 |
| Submodule 增加 clone 复杂度 | 新用户体验 | 文档明确 `--recursive` 要求 |
| TypeLayout 版本升级可能引入 breaking change | 构建失败 | 锁定 submodule commit |
| Boost include 路径冲突 | 头文件冲突 | TypeLayout 使用独立子路径 `boost/typelayout/` |

## Migration Plan

### 阶段 1: 集成 (本变更)
- 添加 submodule
- 创建兼容层
- 新代码使用新 API

### 阶段 2: 迁移 (后续变更)
- 更新所有测试/示例到新 API
- 更新所有 `static_assert` 签名字符串
- 移除旧 `TypeSignature<T>` 特化

### 阶段 3: 清理 (未来)
- 删除 `XTypeSignature` 命名空间
- 删除 `CompileString` 等辅助类型（如果 TypeLayout 提供替代）

## Open Questions

- [ ] TypeLayout 的 `CompileString` 和 XOffsetDatastructure 的 `CompileString` 是否有冲突？
- [ ] XVector/XString 等自定义容器类型在 TypeLayout 中如何注册签名？
- [ ] 是否需要为 TypeLayout 指定具体的 tag/commit 而非跟踪 main？
