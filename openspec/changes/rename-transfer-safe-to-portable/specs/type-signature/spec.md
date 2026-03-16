## MODIFIED Requirements

### Requirement: TypeLayout 依赖集成

系统 SHALL 通过 Git Submodule 方式集成 [TypeLayout](https://github.com/ximicpp/TypeLayout) 库作为类型签名的**唯一**实现，并在内部代码中优先使用 TypeLayout 提供的工具函数，避免重复实现。不再保留任何遗留兼容层。

**约束**:
- Submodule 路径: `external/typelayout`
- 分支: 跟踪 `main` 分支
- 头文件路径: `external/typelayout/include`
- 内部代码应使用 `boost::typelayout::get_member_count<T>()` 而非自行实现
- 不再存在 `XTypeSignature` 命名空间
- 子模块 SHALL 指向 `origin/main` 的最新 commit，确保 `layout_traits.hpp`、`serialization_free.hpp`、`classify.hpp`、`safety_level.hpp` 和 `TYPELAYOUT_OPAQUE_*_RELOCATABLE` 宏全部可用
- `xoffsetdatastructure.hpp` 中的注释 SHALL 仅引用 TypeLayout 实际暴露的 API 名称，不引用不存在的宏或函数名
- 跨平台传输验证 API SHALL 使用 `is_byte_copy_portable<T>(remote_sig)` 名称（不再使用 `is_transfer_safe`）

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

#### Scenario: 无冗余反射工具函数
- **WHEN** 项目需要获取类型成员数量
- **THEN** 使用 `boost::typelayout::get_member_count<T>()`
- **AND** 不存在功能重复的内部实现

#### Scenario: 子模块包含完整的安全分类 API
- **WHEN** 项目 `#include <boost/typelayout/tools/classify.hpp>`
- **THEN** `boost::typelayout::classify_v<T>` 可用
- **AND** `boost::typelayout::SafetyLevel` 包含 5 级分类（TrivialSafe, PaddingRisk, PlatformVariant, PointerRisk, Opaque）

#### Scenario: 子模块包含 serialization-free 检查 API
- **WHEN** 项目 `#include <boost/typelayout/tools/serialization_free.hpp>`
- **THEN** `boost::typelayout::is_local_serialization_free_v<T>` 可用
- **AND** `boost::typelayout::is_byte_copy_portable<T>(remote_sig)` 可用

#### Scenario: 子模块包含 relocatable opaque 宏
- **WHEN** 项目 `#include <boost/typelayout/opaque.hpp>`
- **THEN** `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`、`TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE`、`TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` 宏可用

#### Scenario: 注释中不引用不存在的 API
- **WHEN** 审查 `xoffsetdatastructure.hpp` 中的注释
- **THEN** 不存在对 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` 的引用
- **AND** 不存在对 `is_transfer_safe` 的引用（使用 `is_byte_copy_portable` 替代）
- **AND** 所有注释中引用的 TypeLayout API 名称在 `external/typelayout/include` 中均可找到对应声明

#### Scenario: 旧 API 名称触发 deprecation warning
- **WHEN** 用户代码调用 `boost::typelayout::is_transfer_safe<T>(sig)`
- **THEN** 编译成功但产生 `[[deprecated]]` 警告
- **AND** 警告提示使用 `is_byte_copy_portable` 替代
