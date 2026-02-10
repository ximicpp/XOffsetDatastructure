## 1. 识别冗余代码
- [x] 1.1 确认 `XBufferCompactor::get_member_count_impl<T>()` 的所有调用点
- [x] 1.2 确认 `detail::get_safe_member_count<T>()` 的所有调用点
- [x] 1.3 确认 `boost::typelayout::get_member_count<T>()` 签名完全兼容

## 2. 执行替换
- [x] 2.1 删除 `XBufferCompactor::get_member_count_impl<T>()`，调用点改为 `boost::typelayout::get_member_count<T>()`
- [x] 2.2 删除 `detail::get_safe_member_count<T>()`（死代码，无调用点）

## 3. 验证
- [x] 3.1 在 Docker 中构建并运行所有测试（19/19 通过）
- [x] 3.2 确认无编译警告