## 1. 修改 `is_transfer_safe` 前提条件

- [ ] 1.1 在 `serialization_free.hpp` 顶部添加 `#include <boost/typelayout/admission.hpp>`
- [ ] 1.2 `is_transfer_safe<T>(remote_sig)`: 将 `!is_local_serialization_free_v<T>` 替换为 `!is_byte_copy_safe_v<T>`

## 2. 修改 `SignatureRegistry` static_assert

- [ ] 2.1 `register_local<T>(std::string_view key)`: 将 `is_local_serialization_free_v<T>` 替换为 `is_byte_copy_safe_v<T>`，更新错误消息
- [ ] 2.2 `register_local<T>()`: 同上

## 3. 文档化 opaque tag 匹配假设

- [ ] 3.1 在 `opaque.hpp` 头部注释区域添加 Opaque Tag Matching Assumption 说明
- [ ] 3.2 更新 `serialization_free.hpp` 头部注释，说明 is_transfer_safe 现在接受 relocatable opaque 类型

## 4. 测试

- [ ] 4.1 添加测试: relocatable opaque 类型 `is_transfer_safe` 返回 true（签名匹配时）
- [ ] 4.2 添加测试: relocatable opaque 类型 `is_transfer_safe` 返回 false（签名不匹配时）
- [ ] 4.3 添加测试: 含 opaque 成员的 composite struct `is_transfer_safe` 返回 true
- [ ] 4.4 添加测试: `SignatureRegistry` 可以注册 relocatable opaque 类型
- [ ] 4.5 确认现有测试全部通过（向后兼容）

## 5. XOffset 侧跟进

- [x] 5.1 [needs-xoffset] TypeLayout 变更完成后，更新 XOffset 的 TypeLayout 子模块
- [x] 5.2 [needs-xoffset] 更新 XOffset 的 spec 和文档，说明跨平台验证现在可用
