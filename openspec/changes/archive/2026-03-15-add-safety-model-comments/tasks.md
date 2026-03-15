## 1. 添加安全模型注释

- [x] 1.1 在 `DefaultPolicy::accept` 的文档注释中添加安全责任分层说明
- [x] 1.2 在第一分支（opaque）添加注释：外壳安全 = 用户保证，内部元素 = TypeLayout 保证
- [x] 1.3 在第三分支（递归 struct）添加注释：解释为何允许非 trivially_copyable 的 struct
- [x] 1.4 在 `else return false` 分支添加已知限制注释：opaque 容器的 C 数组

## 2. 验证

- [x] 2.1 Docker 构建通过（27/27 测试）