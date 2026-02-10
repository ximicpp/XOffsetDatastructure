## MODIFIED Requirements
### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的**唯一**实现，并在内部代码中优先使用 TypeLayout 提供的工具函数，避免重复实现。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`
- 内部代码应使用 `boost::typelayout::get_member_count<T>()` 而非自行实现

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** CMake 配置能找到 `boost/typelayout.hpp`

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout.hpp>`

#### Scenario: 无冗余反射工具函数
- **WHEN** 项目需要获取类型成员数量
- **THEN** 使用 `boost::typelayout::get_member_count<T>()`
- **AND** 不存在功能重复的内部实现