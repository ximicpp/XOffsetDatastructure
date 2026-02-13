## P0 — 阻塞发布
- [x] F3: `make<T>()` 重复调用保护 — 检查已存在时 throw interprocess_exception
- [x] F4: `root<T>()` Release 安全性 — 替换 assert 为 throw interprocess_exception

## P1 — 高优先级
- [ ] F1: `compact_automatic` 返回 `XBufferExt` — 延期(需重构类声明顺序，风险高)
- [x] F2: `save_to_string()` 改为默认紧凑(shrink) + 新增 `save_to_string_full()`
- [x] F5: 添加错误路径测试 `test_error_paths.cpp` (重复make/root空缓冲区/vector roundtrip/estimate/compact输出/状态)

## P2 — 中优先级
- [x] F6: 注释中 `find<T>()` → `root<T>()` (grow/shrink_to_fit 注释)
- [ ] F7: 在 demo.cpp 中添加 XHandle 使用演示 — 延期(依赖 F1)
- [x] F8: 为 `save_to_vector()` / `load_from_vector()` / `estimate_buffer_size()` 添加测试
- [ ] F9: README 补充 compact_automatic 返回类型说明 — 延期(依赖 F1)
- [x] F10: examples/README.md 添加 buffer sizing 指南
- [x] F11: examples/README.md 添加 XVector 内存耗尽说明
- [x] F15: 头文件中明确声明 single-root-object 模型 (XBufferExt 注释块)

## P3 — 低优先级
- [x] F12: CORE_FORMAL_MODEL.md 行号 → 锚点描述 (§2.1, §3.3, §5.1)
- [x] F13: `XBufferBestFit` 加注释说明
- [x] F14: Arch64BE/Arch32LE/Arch32BE 加注释说明

## 验证
- [x] V1: 全量构建测试通过 (26/26 + Demo + HelloWorld)

## 延期项说明
F1/F7/F9 需要将 `XBufferCompactor::compact_automatic` 的返回类型从 `XBuffer` 改为
`XBufferExt`，但 `XBufferCompactor` 在文件中位于 `XBufferExt` 定义之前。修改需要重构
类的声明顺序或引入前向声明，属于较大的结构性改动，不应与其他修复混在一起。
建议作为独立 proposal 单独处理。