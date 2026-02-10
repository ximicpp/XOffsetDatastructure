## 1. 创建兼容性检查工具
- [ ] 1.1 创建 `tools/check_compat.cpp`，include 已导出的 `.sig.hpp` 文件
- [ ] 1.2 添加编译时 `static_assert` 验证（同平台自验证）
- [ ] 1.3 使用 `TYPELAYOUT_CHECK_COMPAT` 宏生成运行时报告

## 2. 构建集成
- [ ] 2.1 在 `CMakeLists.txt` 中添加 `check_compat` target（C++17 标准，不需要 P2996）
- [ ] 2.2 设置 include paths 指向 `tools/sigs/` 和 TypeLayout tools

## 3. 验证
- [ ] 3.1 在 Docker 中编译 `check_compat`（使用 C++17）
- [ ] 3.2 运行并验证兼容性报告输出正确
- [ ] 3.3 验证 Safety 分级（Player/GameData/Item 应为 Safe）

## 4. 文档
- [ ] 4.1 在 `tools/README.md` 中说明兼容性验证工具用法