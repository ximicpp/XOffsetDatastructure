# XOffsetDatastructure Tests

27 个测试文件，覆盖数据结构、内存管理、API、类型安全、反射和零样板功能。

## 运行测试

```bash
# 推荐：Docker 构建（含 P2996 编译器）
docker run --rm --platform linux/amd64 \
  -v $(pwd):/workspace -w /workspace \
  ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh

# 或本地构建
./build.sh
```

## 测试文件一览

### 基础测试（7 个）

| # | 文件 | 说明 |
|---|------|------|
| 1 | `test_basic_types.cpp` | POD 类型读写、序列化/反序列化 |
| 2 | `test_vector.cpp` | XVector 操作、持久化 |
| 3 | `test_map_set.cpp` | XMap/XSet 操作 |
| 4 | `test_nested.cpp` | 多层嵌套对象 |
| 5 | `test_compaction.cpp` | 内存压缩、数据完整性 |
| 6 | `test_modify.cpp` | 就地修改操作 |
| 7 | `test_xbuffer_api.cpp` | XBuffer make/root/save/load/grow API |

### 反射测试（20 个，需要 P2996 Clang）

| # | 文件 | 说明 |
|---|------|------|
| 8 | `test_reflection_core.cpp` | `^^` 反射、`members_of` 迭代、`[: :]` splice |
| 9 | `test_reflection_advanced.cpp` | 反射序列化、比较、版本兼容性 |
| 10 | `test_type_signatures.cpp` | TypeLayout 签名生成（class/struct/template/组合） |
| 11 | `test_type_introspection.cpp` | 类型名查询、成员类型分析 |
| 12 | `test_reflection_compaction.cpp` | 反射辅助内存分析 |
| 13 | `test_field_limit_fix.cpp` | 大字段数结构体签名 |
| 14 | `test_type_safety.cpp` | 全面类型安全验证（Domain S） |
| 15 | `test_typelayout_integration.cpp` | TypeLayout 集成（签名匹配、classify） |
| 16 | `test_enum_support.cpp` | 枚举类型安全和签名 |
| 17 | `test_xstring_direct_assign.cpp` | XString 直接赋值 |
| 18 | `test_xhandle.cpp` | XHandle 稳定句柄 API |
| 19 | `test_error_paths.cpp` | 错误路径和边界条件 |
| 20 | `test_memory_efficiency.cpp` | 内存效率分析 |
| 21 | `test_zero_boilerplate.cpp` | 零样板 API（自动分配器） |
| 22 | `test_zero_boilerplate_vector.cpp` | 零样板 XVector 聚合 |
| 23 | `test_complex_nesting.cpp` | 深层嵌套、多容器组合 |
| 24 | `test_inheritance.cpp` | 继承、多重继承、组合 |
| 25 | `test_adaptive_reservation.cpp` | 自适应缓冲区预留 |
| 26 | `test_policy_trait.cpp` | DefaultPolicy/StrictPolicy/自定义策略 |
| 27 | `test_remediation_fixes.cpp` | 安全审计修复验证（C1/C2/M2/L2） |