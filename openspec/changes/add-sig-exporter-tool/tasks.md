## 1. 创建签名导出工具
- [x] 1.1 创建 `tools/export_signatures.cpp`，include Player/GameData/Item 头文件
- [x] 1.2 使用 `TYPELAYOUT_EXPORT_TYPES(Player, Item, GameData)` 宏生成 main()
- [x] 1.3 创建 `tools/sigs/` 目录（.gitkeep）

## 2. 构建集成
- [x] 2.1 在 `CMakeLists.txt` 中添加 `export_signatures` executable target
- [x] 2.2 设置正确的 include paths（TypeLayout tools + examples headers）
- [ ] 2.3 在 `build.sh` 中添加可选的 `--export-sigs` 参数

## 3. 验证
- [x] 3.1 在 Docker 中编译 `export_signatures`
- [x] 3.2 运行 `./export_signatures tools/sigs/` 生成签名文件
- [x] 3.3 验证生成的 `.sig.hpp` 文件内容正确

## 4. 文档
- [x] 4.1 在 `tools/README.md` 中说明签名导出工具用法