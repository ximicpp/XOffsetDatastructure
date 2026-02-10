## REMOVED Requirements
### Requirement: XTypeSignature 兼容层
**Reason**: TypeLayout 集成已完成并通过全面验证（21/21 测试通过）。兼容层的过渡期已结束，继续保留只会增加维护负担和认知混乱。所有使用 `XTypeSignature` 的代码已迁移到 `boost::typelayout`。
**Migration**: 
- `XTypeSignature::get_XTypeSignature<T>()` → `boost::typelayout::get_definition_signature<T>()`
- `XTypeSignature::TypeSignature<T>` → `boost::typelayout::TypeSignature<T>`
- `XTypeSignature::CompileString<N>` → `boost::typelayout::FixedString<N>`
- `XTypeSignature::get_member_count<T>()` → `boost::typelayout::get_member_count<T>()`
- `XTypeSignature::BASIC_ALIGNMENT` → `XOffsetDatastructure2::BASIC_ALIGNMENT`
- `XTypeSignature::ANY_SIZE` → `XOffsetDatastructure2::ANY_SIZE`

## MODIFIED Requirements
### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的**唯一**实现。不再保留任何遗留兼容层。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`
- 不再存在 `XTypeSignature` 命名空间

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** CMake 配置能找到 `boost/typelayout.hpp`

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout.hpp>`

#### Scenario: 无遗留兼容层残留
- **WHEN** 用户在代码中使用 `XTypeSignature::` 前缀
- **THEN** 编译失败，提示命名空间不存在
- **AND** 用户应使用 `boost::typelayout::` 替代
