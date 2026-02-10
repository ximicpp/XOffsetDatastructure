## 1. 创建兼容性检查工具
- [x] 1.1 创建 `tools/check_compat.cpp`，include 已导出的 `.sig.hpp` 文件
- [x] 1.2 添加编译时 `static_assert` 验证（同平台自验证）
- [x] 1.3 使用 `TYPELAYOUT_CHECK_COMPAT` 宏生成运行时报告

## 2. 构建集成
- [x] 2.1 在 `CMakeLists.txt` 中添加 `check_compat` target
- [x] 2.2 设置 include paths 指向 `tools/sigs/` 和 TypeLayout tools

## 3. 验证
- [x] 3.1 在 Docker 中编译 `check_compat`
- [x] 3.2 运行并验证兼容性报告输出正确
- [x] 3.3 验证 Safety 分级（Layout层为Risk因Boost内部位域，Definition层为正确的Opaque签名）

## 4. 文档
- [x] 4.1 在 `tools/README.md` 中说明兼容性验证工具用法
