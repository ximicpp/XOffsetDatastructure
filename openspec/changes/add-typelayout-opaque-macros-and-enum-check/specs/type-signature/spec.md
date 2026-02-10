## MODIFIED Requirements
### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的**唯一**实现，并在内部代码中优先使用 TypeLayout 提供的工具函数和辅助宏，避免重复实现。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`
- 内部代码应使用 `boost::typelayout::get_member_count<T>()` 而非自行实现
- 容器类型签名特化应使用 `TYPELAYOUT_OPAQUE_*` 辅助宏注册
- 不再存在 `XTypeSignature` 命名空间

#### Scenario: 初始化项目时自动拉取依赖
- **WHEN** 用户执行 `git clone --recursive` 或 `git submodule update --init`
- **THEN** TypeLayout 代码被正确拉取到 `external/typelayout`
- **AND** CMake 配置能找到 `boost/typelayout.hpp`

#### Scenario: 构建时包含 TypeLayout 头文件
- **WHEN** CMake 配置项目
- **THEN** `external/typelayout/include` 被添加到 include 路径
- **AND** 项目可以 `#include <boost/typelayout.hpp>`

#### Scenario: 使用 Opaque 宏注册容器类型签名
- **WHEN** 项目需要为自定义容器类型注册签名
- **THEN** 使用 `TYPELAYOUT_OPAQUE_TYPE`、`TYPELAYOUT_OPAQUE_CONTAINER` 或 `TYPELAYOUT_OPAQUE_MAP` 宏
- **AND** 宏生成的签名与手写特化完全等价

#### Scenario: 无冗余反射工具函数
- **WHEN** 项目需要获取类型成员数量
- **THEN** 使用 `boost::typelayout::get_member_count<T>()`
- **AND** 不存在功能重复的内部实现

## ADDED Requirements
### Requirement: 枚举类型 XBuffer 安全支持

系统 SHALL 支持固定底层类型的枚举类型作为 XBuffer 安全类型，允许在共享内存数据结构中使用枚举字段。

#### Scenario: Scoped enum 被识别为安全类型
- **WHEN** 用户定义 `enum class Status : uint8_t { Active, Inactive }`
- **THEN** `is_xbuffer_safe<Status>::value` 为 `true`
- **AND** 包含该枚举字段的结构体也通过安全检查

#### Scenario: 固定底层类型的 unscoped enum 被识别为安全类型
- **WHEN** 用户定义 `enum Color : int32_t { Red, Green, Blue }`
- **THEN** `is_xbuffer_safe<Color>::value` 为 `true`

#### Scenario: 枚举字段的 Definition Signature 包含完整信息
- **WHEN** 结构体包含枚举字段
- **THEN** 签名中枚举显示为 `enum<QualifiedName>[s:N,a:M]<underlying_type>`
