# 文档索引

本目录包含XOffsetDatastructure项目的核心文档。

## 📚 文档列表

### 🚀 快速开始

| 文档 | 用途 | 适合人群 |
|------|------|---------|
| **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** | 快速查找命令和解决方案 | ✅ 所有开发者 |
| **[BUILD_AND_TEST_GUIDE.md](BUILD_AND_TEST_GUIDE.md)** | 完整构建和测试指南 | ✅ 新手 & AI助手 |

### 📖 详细文档

| 文档 | 描述 |
|------|------|
| **[CORE_FORMAL_MODEL.md](CORE_FORMAL_MODEL.md)** | 🔬 核心形式化模型 — 零编码正确性的理论基础（C1+C2定理与证明） |
| `technical_overview.md` | 技术架构概览 |
| `MIGRATION_TYPELAYOUT.md` | TypeLayout 迁移指南与 API 参考 |
| `README.md` | (本文件) 文档索引 |

## 🎯 使用建议

### 遇到问题时

1. **先查**: [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - 快速定位常见错误
2. **深入**: [BUILD_AND_TEST_GUIDE.md](BUILD_AND_TEST_GUIDE.md) - 获取详细解决方案
3. **参考**: `../AGENTS.md` - 了解开发规范

### AI助手工作流

```
收到构建/测试相关任务
    ↓
查阅 QUICK_REFERENCE.md (获取核心规则)
    ↓
参考 BUILD_AND_TEST_GUIDE.md (详细步骤)
    ↓
执行 (本地测试 → CI验证)
    ↓
记录新知识 (更新相关文档)
```

## 📋 核心原则速记

### 1. 本地测试优先
```bash
./scripts/local-docker-test.sh  # 本地先测试
git push origin next_cpp26       # 再推送
```

### 2. 正确的Docker命令
```bash
# ✅ 正确
docker run ... bash ./build.sh

# ❌ 错误  
docker run ... ./build.sh
```

### 3. CMake配置
```cmake
# ✅ 正确
add_compile_options(-std=c++26 -freflection ...)

# ❌ 错误
set(CMAKE_CXX_STANDARD 26)
```

## 🔗 相关文件

- 项目根目录: `../AGENTS.md` - 开发规范
- CI配置: `../.github/workflows/ci.yml`
- 构建脚本: `../build.sh`
- Docker配置: `../Dockerfile`
- 测试脚本: `../scripts/local-docker-test.sh`

## 📊 测试矩阵

```
27个测试 = 7个基础测试 + 20个反射测试

预期结果:
  Tests Run: 27
  Tests Passed: 27
  Status: ✓ SUCCESS
```

## 🕐 时间预期

| 操作 | 时长 |
|------|------|
| 本地Docker测试 | 5-10分钟 |
| GitHub Actions完整CI | 1.5-2小时 |
| Docker首次构建 | 1-1.5小时 |

---

**维护**: 当发现新的常见问题或最佳实践时，请更新相应文档。
