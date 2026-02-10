# Change: 集成 TypeLayout 库替换 XTypeSignature 实现

## Why

当前 `XTypeSignature` 命名空间 (~270 行) 提供基础的类型签名生成能力，但功能有限：
- 仅支持 Layout 层级签名，缺少字段名/继承结构信息
- 不支持跨平台签名导出和比较
- 缺少形式化证明保证（Soundness, Injectivity）
- 代码维护在单一仓库，难以复用

[TypeLayout](https://github.com/ximicpp/TypeLayout) 是一个成熟的编译期类型布局验证库，提供：
- 两层签名系统（Layout + Definition）
- 跨平台签名导出工具
- 形式化正确性证明
- 更完整的类型支持（继承、多态、联合体等）

## What Changes

### 新增
- 添加 TypeLayout 作为 Git Submodule (`external/typelayout`)
- 在 CMake 中集成 TypeLayout 头文件路径
- 暴露 `boost::typelayout` API 供用户使用
- 提供从 `XTypeSignature` 到 `boost::typelayout` 的迁移指南

### 修改
- **BREAKING**: `XTypeSignature::get_XTypeSignature<T>()` 标记为 deprecated
- 推荐使用 `boost::typelayout::get_definition_signature<T>()`
- 更新测试和示例使用新 API

### 保留 (兼容性)
- `XTypeSignature` 命名空间保留，底层委托到 TypeLayout
- 现有 `static_assert` 检查继续工作
- 渐进迁移，不破坏现有用户代码

## Impact

- **Affected specs**: `type-signature` (新建)
- **Affected code**:
  - `xoffsetdatastructure2.hpp` (lines 51-323, 1148-1178)
  - `tests/test_reflection_type_signature.cpp`
  - `tests/test_class_type_signatures.cpp`
  - `examples/game_data.hpp`, `examples/player.hpp`
- **Dependencies**: 新增 `external/typelayout` submodule
- **CI/CD**: 需要更新 Docker 镜像和构建脚本
