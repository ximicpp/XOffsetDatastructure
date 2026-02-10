## 1. 识别冗余代码
- [ ] 1.1 确认 `XBufferCompactor::get_member_count_impl<T>()` 的所有调用点
- [ ] 1.2 确认 `detail::get_safe_member_count<T>()` 的所有调用点
- [ ] 1.3 确认 `boost::typelayout::get_member_count<T>()` 签名完全兼容

## 2. 执行替换
- [ ] 2.1 删除 `XBufferCompactor::get_member_count_impl<T>()`，调用点改为 `boost::typelayout::get_member_count<T>()`
- [ ] 2.2 删除 `detail::get_safe_member_count<T>()`，调用点改为 `boost::typelayout::get_member_count<T>()`

## 3. 验证
- [ ] 3.1 在 Docker 中构建并运行所有测试（确保全部通过）
- [ ] 3.2 确认无编译警告