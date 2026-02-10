# Implementation Tasks

## 1. 类型擦除容器检测 ✅ 已完成
- [x] 1.1 添加 std::function 检测
- [x] 1.2 添加 std::any 检测
- [x] 1.3 添加 std::shared_ptr/unique_ptr/weak_ptr 检测
- [x] 1.4 更新 is_xbuffer_safe 集成黑名单检查
- [x] 1.5 创建测试用例 (test_type_erased_detection.cpp)

## 2. 结构体填充验证 (待定)
- [ ] 2.1 设计填充验证机制
- [ ] 2.2 实现编译时填充检查
- [ ] 2.3 添加填充警告/错误信息

## 3. Schema 版本控制 (待定)
- [ ] 3.1 设计版本号格式
- [ ] 3.2 实现版本兼容性检查
- [ ] 3.3 添加版本迁移指南文档

---

## 已拒绝的任务

### ~~平台指纹系统~~
**原因**: 假设同构环境，不需要跨平台指纹验证

### ~~std::variant 安全性检查~~
**原因**: std::variant 在同构环境下内存布局固定，可安全使用