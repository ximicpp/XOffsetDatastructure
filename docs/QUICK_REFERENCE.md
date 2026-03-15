# XOffsetDatastructure 快速参考

> **给AI助手的提示**: 在处理构建和测试相关问题时，请参考此文件

## 🎯 核心规则

### 1. 本地测试优先原则

```bash
# ✅ 推荐流程
1. 本地WSL测试
2. 确认全部通过
3. 推送到GitHub
4. 监控CI状态

# ❌ 错误流程
1. 直接推送代码
2. 等待CI (1.5小时)
3. 失败后再修复
4. 重复浪费时间
```

### 2. Docker执行命令规范

```bash
# ✅ 正确
docker run --rm -v $(pwd):/workspace -w /workspace \
  xoffset-clang-p2996:latest \
  bash ./build.sh

# ❌ 错误 - 权限问题
docker run ... ./build.sh

# ❌ 错误 - shell不兼容
docker run ... sh ./build.sh
```

### 3. CMake C++26 配置

```cmake
# ✅ 正确 - 手动指定标志
add_compile_options(-std=c++26 -freflection -fexpansion-statements -stdlib=libc++)

# ❌ 错误 - CMake 3.10不识别
set(CMAKE_CXX_STANDARD 26)
```

## 📋 常用命令速查

### 本地测试

```bash
# WSL环境下
cd /mnt/g/workspace/XOffsetDatastructure

# 方式1: 使用本地测试脚本 (推荐)
./scripts/local-docker-test.sh

# 方式2: 手动Docker运行
docker run --rm -v $(pwd):/workspace -w /workspace \
  xoffset-clang-p2996:latest bash ./build.sh

# 方式3: Docker Compose
docker-compose run --rm xoffset-dev bash ./build.sh

# 交互式调试
./scripts/local-docker-test.sh -i
docker-compose run --rm xoffset-dev bash
```

### CI监控

```powershell
# Windows PowerShell
powershell -File scripts/fetch-ci-status.ps1

# 自动监控 (每分钟刷新)
while ($true) {
    cls
    powershell -File scripts/fetch-ci-status.ps1
    Start-Sleep -Seconds 60
}
```

### Git操作

```bash
# 检查状态
git status
git diff

# 提交推送
git add -A
git commit -m "fix: resolve reflection test failure"
git push origin next_cpp26

# 查看最近提交
git log --oneline -5
```

## 🔍 故障排查流程

### 本地测试失败

```bash
# 1. 进入交互式Shell
./scripts/local-docker-test.sh -i

# 2. 手动构建查看详细错误
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4 VERBOSE=1

# 3. 单独运行失败的测试
./bin/Release/test_basic_types
```

### CI失败处理

```bash
# 1. 先在本地复现
./scripts/local-docker-test.sh

# 2. 如果本地通过但CI失败，检查:
# - Docker镜像是否一致
# - 文件权限是否正确
# - 工作流配置是否正确

# 3. 重新构建Docker镜像
./scripts/docker-build.sh --no-cache
```

### 编译器问题

```bash
# 检查Clang P2996
docker run --rm xoffset-clang-p2996:latest clang++ --version

# 验证反射支持
docker run --rm xoffset-clang-p2996:latest \
  bash -c "echo 'int main(){}' | clang++ -std=c++26 -freflection -x c++ -"
```

## 📊 测试矩阵

```
总测试: 27个
├── 基础测试 (7个) - 核心功能
│   ├── test_basic_types
│   ├── test_vector
│   ├── test_map_set
│   ├── test_nested
│   ├── test_compaction
│   ├── test_modify
│   └── test_xbuffer_api
└── 反射测试 (20个) - 需要Clang P2996
    ├── test_reflection_core
    ├── test_reflection_advanced
    ├── test_type_signatures
    ├── test_type_introspection
    ├── test_reflection_compaction
    ├── test_field_limit_fix
    ├── test_type_safety
    ├── test_typelayout_integration
    ├── test_enum_support
    ├── test_xstring_direct_assign
    ├── test_xhandle
    ├── test_error_paths
    ├── test_memory_efficiency
    ├── test_zero_boilerplate
    ├── test_zero_boilerplate_vector
    ├── test_complex_nesting
    ├── test_inheritance
    ├── test_adaptive_reservation
    ├── test_policy_trait
    └── test_remediation_fixes

预期结果:
  Tests Run: 27
  Tests Passed: 27
  Status: ✓ SUCCESS
```

## 🚨 常见错误及解决方案

### 错误1: Permission denied

```bash
# 症状
./build.sh: Permission denied

# 解决
docker run ... bash ./build.sh  # 使用bash明确指定
```

### 错误2: CMake CXX26 unknown

```bash
# 症状
CMake Error: CMAKE_CXX_STANDARD 26 not supported

# 解决
# 编辑 CMakeLists.txt
add_compile_options(-std=c++26 ...)  # 不用 set(CMAKE_CXX_STANDARD 26)
```

### 错误3: Docker image not found

```bash
# 症状
Error: No such image: xoffset-clang-p2996:latest

# 解决
./scripts/docker-build.sh  # 构建镜像 (1-1.5小时)
```

### 错误4: Reflection tests fail

```bash
# 症状
error: unknown type name 'std::meta::info'

# 解决
# 1. 确认使用Clang P2996
docker run ... clang++ --version

# 2. 如果只测试基础功能
./build.sh --no-reflection
```

### 错误5: CI timeout

```bash
# 症状
GitHub Actions超时 (>2小时)

# 解决
# 使用Docker层缓存 (在ci.yml中配置)
- uses: actions/cache@v3
  with:
    path: /tmp/.buildx-cache
    key: ${{ runner.os }}-buildx-${{ github.sha }}
```

## 📦 项目依赖

```
Clang P2996 (custom build from bloomberg/clang-p2996)
  └─ LLVM with reflection support
     ├─ -freflection
     ├─ -fexpansion-statements
     └─ -stdlib=libc++

CMake 3.10+ (推荐 3.22+)
Boost.Interprocess (共享内存)
Ubuntu 22.04 (Docker基础镜像)
```

## 🕐 时间预期

| 操作 | 预计时长 | 说明 |
|------|---------|------|
| Docker首次构建 | 1-1.5小时 | 从源码编译LLVM |
| Docker缓存构建 | 5-10分钟 | 使用缓存层 |
| 本地测试运行 | 5-10分钟 | 27个测试 |
| GitHub Actions | 1.5-2小时 | 完整流程 |
| 单个测试 | 5-30秒 | 取决于测试 |

## 🔗 相关文档

- 详细指南: `docs/BUILD_AND_TEST_GUIDE.md`
- 开发规范: `AGENTS.md`
- CI配置: `.github/workflows/ci.yml`
- 构建脚本: `build.sh`
- Docker配置: `Dockerfile`

## ✅ 检查清单

### 提交前
- [ ] 本地Docker测试通过
- [ ] 27个测试全部通过
- [ ] 代码符合风格规范
- [ ] 提交信息清晰明确

### CI监控
- [ ] Docker构建成功 (1.5h)
- [ ] 测试执行完成 (10m)
- [ ] 全部测试通过
- [ ] 无错误日志

### 调试失败
- [ ] 本地先复现问题
- [ ] 使用交互式Shell
- [ ] 查看详细构建日志
- [ ] 单独运行失败测试

---

**提示**: 遇到问题时，优先查看 `docs/BUILD_AND_TEST_GUIDE.md` 获取详细解决方案。
