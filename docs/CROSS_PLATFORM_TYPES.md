# 跨平台类型使用规范

## 🎯 核心原则

**只使用固定大小的标准类型**，避免平台相关的类型。

## ✅ 推荐使用的类型

### 整数类型

| 类型 | 大小 | 说明 | 跨平台 |
|------|------|------|--------|
| `int8_t` | 1字节 | 有符号8位整数 | ✅ |
| `uint8_t` | 1字节 | 无符号8位整数 | ✅ |
| `int16_t` | 2字节 | 有符号16位整数 | ✅ |
| `uint16_t` | 2字节 | 无符号16位整数 | ✅ |
| `int32_t` | 4字节 | 有符号32位整数 | ✅ |
| `uint32_t` | 4字节 | 无符号32位整数 | ✅ |
| `int64_t` | 8字节 | 有符号64位整数 | ✅ |
| `uint64_t` | 8字节 | 无符号64位整数 | ✅ |

### 浮点类型

| 类型 | 大小 | 说明 | 跨平台 |
|------|------|------|--------|
| `float` | 4字节 | 32位浮点数 | ✅ |
| `double` | 8字节 | 64位浮点数 | ✅ |

### 字符和布尔类型

| 类型 | 大小 | 说明 | 跨平台 |
|------|------|------|--------|
| `char` | 1字节 | 字符 | ✅ |
| `bool` | 1字节 | 布尔值 | ✅ |

## ❌ 禁止使用的类型

这些类型在不同平台上大小或定义不同，**绝对不要使用**：

| 类型 | 问题 | Linux/macOS | Windows | Android |
|------|------|-------------|---------|---------|
| `long` | 大小不一致 | 8字节 | **4字节** | 8字节 |
| `unsigned long` | 大小不一致 | 8字节 | **4字节** | 8字节 |
| `long long` | 类型别名不一致 | ≠ int64_t | ≠ int64_t | **= int64_t** |
| `unsigned long long` | 类型别名不一致 | ≠ uint64_t | ≠ uint64_t | **= uint64_t** |
| `size_t` | 可能不同 | 8字节 | 8字节(x64)/4字节(x86) | 8字节 |
| `int` | 虽然都是4字节，但不明确 | 4字节 | 4字节 | 4字节 |
| `long double` | 大小不一致 | 16字节 | **8字节** | 8字节 |

## 🔍 问题案例

### ❌ 错误示例

```yaml
# schemas/bad_example.yaml
types:
  - name: BadExample
    fields:
      - name: count
        type: long  # ❌ 在 Windows 是 4 字节，其他平台 8 字节
      - name: id
        type: long long  # ❌ 在 Android 是 int64_t 别名，其他平台不是
```

编译错误：
```
Android NDK:
  error: static assertion failed due to requirement 'always_false<long long>::value': 
  Type is not supported for automatic reflection

原因: 在 Android 上 int64_t 定义为 long long，
     导致 TypeSignature<int64_t> 和 TypeSignature<long long> 冲突
```

### ✅ 正确示例

```yaml
# schemas/good_example.yaml
types:
  - name: GoodExample
    fields:
      - name: count
        type: int64_t  # ✅ 所有平台都是 8 字节
      - name: id
        type: int64_t  # ✅ 明确的 64 位整数
      - name: value
        type: int32_t  # ✅ 明确的 32 位整数
```

## 📋 类型选择指南

### 选择整数类型

```cpp
// ✅ 推荐
int32_t player_id;      // 玩家ID (4字节足够)
int64_t timestamp;      // 时间戳 (需要大范围)
uint32_t count;         // 计数器 (无符号)
int8_t flag;            // 标志位 (节省空间)

// ❌ 不推荐
int player_id;          // 不明确，虽然通常是4字节
long timestamp;         // Windows: 4字节, Unix: 8字节
unsigned long count;    // 平台不一致
```

### 选择浮点类型

```cpp
// ✅ 推荐
float position_x;       // 位置坐标 (精度够用)
double precise_value;   // 高精度计算

// ❌ 不推荐
long double value;      // 平台大小不一致
```

## 🔧 Schema 定义最佳实践

### 完整示例

```yaml
schema_version: "1.0"

types:
  - name: Player
    fields:
      # 整数字段
      - name: player_id
        type: int32_t      # ✅ 明确的32位整数
        default: 0
      
      - name: score
        type: int64_t      # ✅ 大范围分数
        default: 0
      
      - name: level
        type: uint16_t     # ✅ 等级 (0-65535)
        default: 1
      
      # 浮点字段
      - name: health
        type: float        # ✅ 生命值
        default: 100.0
      
      - name: position_x
        type: double       # ✅ 高精度坐标
        default: 0.0
      
      # 字符和布尔
      - name: team
        type: char         # ✅ 队伍标识 'A'/'B'
        default: 'A'
      
      - name: is_online
        type: bool         # ✅ 在线状态
        default: false
      
      # 容器
      - name: name
        type: XString      # ✅ 字符串
      
      - name: items
        type: XVector<int32_t>  # ✅ 物品ID列表
```

## 🌍 平台类型映射表

### int64_t 的底层类型

| 平台 | int64_t 定义 | long long 关系 |
|------|-------------|---------------|
| **Linux (GCC)** | `long` | `long long` ≠ `int64_t` |
| **macOS (Clang)** | `long` | `long long` ≠ `int64_t` |
| **Windows (MSVC)** | `long long` | `long long` = `int64_t` ✅ |
| **Android (NDK)** | `long long` | `long long` = `int64_t` ✅ |
| **iOS (Clang)** | `long` | `long long` ≠ `int64_t` |

**结论**: 使用 `int64_t`，不要使用 `long` 或 `long long`

## 🧪 验证类型签名

### 检查生成的代码

```bash
# 生成代码后，检查类型
grep "int64_t\|long long\|long " generated/your_types.hpp

# 应该只看到 int64_t，不应该有 long long 或 long
```

### 编译测试

```bash
# 测试所有平台编译
cd build
cmake --build . -j8

# 如果出现 "Type is not supported" 错误，检查你的 schema
```

## 📚 相关文档

- [GitHub Actions CI](./GITHUB_ACTIONS_CI.md) - 跨平台自动测试
- [MSVC Compatibility](./MSVC_COMPATIBILITY.md) - MSVC特殊说明
- [Quick Start](./QUICK_START.md) - 快速开始

## 💡 故障排除

### 错误: `Type is not supported for automatic reflection`

**原因**: 使用了平台不一致的类型

**解决方案**:
1. 检查 schema 中的类型定义
2. 将 `long`, `long long`, `unsigned long` 等改为 `int32_t`, `int64_t` 等
3. 重新生成代码: `python3 tools/xds_generator.py schemas/xxx.yaml -o generated/`
4. 重新编译

### 示例修复

```yaml
# 之前 (❌ 错误)
fields:
  - name: value
    type: long long

# 之后 (✅ 正确)
fields:
  - name: value
    type: int64_t
```

---

**遵循这些规范，确保代码在所有平台上一致工作！** 🚀
