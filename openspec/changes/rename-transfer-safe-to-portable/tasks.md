## 1. TypeLayout 上游重命名

- [ ] 1.1 在 `serialization_free.hpp` 中将 `is_transfer_safe` 函数重命名为 `is_byte_copy_portable`
- [ ] 1.2 添加 `is_transfer_safe` 作为 `[[deprecated]]` alias，调用 `is_byte_copy_portable`
- [ ] 1.3 将 `SignatureRegistry::is_safe` 重命名为 `SignatureRegistry::is_portable`，保留旧名 deprecated alias
- [ ] 1.4 更新 `serialization_free.hpp` 头部注释，使用新名称
- [ ] 1.5 更新 TypeLayout 测试中所有 `is_transfer_safe` 引用为 `is_byte_copy_portable`
- [ ] 1.6 TypeLayout 全量构建验证通过

## 2. XOffset 下游同步

- [ ] 2.1 更新 TypeLayout 子模块到包含重命名的最新 commit
- [ ] 2.2 更新 `xoffsetdatastructure.hpp` 头部注释中的 `is_transfer_safe` → `is_byte_copy_portable`
- [ ] 2.3 更新 XOffset 测试中所有 `is_transfer_safe` 引用（如有）
- [ ] 2.4 XOffset Docker 构建验证 23/23 测试通过

## 3. Specs 和文档同步

- [ ] 3.1 [needs-docs] 更新 `openspec/specs/type-signature/spec.md` 中的 `is_transfer_safe` 引用
- [ ] 3.2 [needs-docs] 更新 `openspec/specs/type-safety-delegation/spec.md` 中的 `is_transfer_safe` 引用
- [ ] 3.3 [needs-docs] 更新 `docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md` 中的 API 名称（如有引用）
