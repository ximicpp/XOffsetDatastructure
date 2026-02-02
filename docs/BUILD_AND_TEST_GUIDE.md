# XOffsetDatastructure 构建与测试指南

> **更新时间**: 2026-02-02  
> **目的**: 记录本项目的本地测试和CI/CD最佳实践，避免重复踩坑

---

## 📋 目录

1. [构建方法总览](#构建方法总览)
2. [本地测试流程](#本地测试流程)
3. [远端CI/CD流程](#远端cicd流程)
4. [常见问题与解决方案](#常见问题与解决方案)
5. [关键配置文件](#关键配置文件)

---

## 🏗️ 构建方法总览

### 项目依赖

| 组件 | 要求 | 说明 |
|------|------|------|
| **编译器** | Clang P2996 | 支持C++26反射特性 |
| **C++标准** | C++26 | 使用 `-std=c++26` |
| **反射标志** | `-freflection -fexpansion-statements` | LLVM实验特性 |
| **标准库** | libc++ | Clang的C++标准库 |
| **构建系统** | CMake 3.10+ | 推荐3.22+ |
| **Boost** | Boost.Interprocess | 共享内存支持 |

### 测试矩阵

```
总测试数: 18
├── 基础测试 (6个)
│   ├── test_basic_types       - 基本类型序列化
│   ├── test_vector            - 动态数组
│   ├── test_map_set           - 关联容器
│   ├── test_nested            - 嵌套结构
│   ├── test_compaction        - 内存压缩
│   └── test_modify            - 修改操作
└── 反射测试 (12个) - 需要Clang P2996
    ├── test_reflection_operators
    ├── test_member_iteration
    ├── test_reflection_type_signature
    ├── test_splice_operations
    ├── test_type_introspection
    ├── test_reflection_compaction
    ├── test_reflection_serialization
    ├── test_reflection_comparison
    ├── test_field_limit_fix
    ├── test_class_type_signatures
    ├── test_type_safety
    └── test_vptr_layout
```

---

## 🧪 本地测试流程

### 方法1: 使用WSL (Windows用户推荐)

#### 前置条件
```bash
# 1. 启用WSL2
wsl --install

# 2. 安装Ubuntu 22.04
wsl --install -d Ubuntu-22.04

# 3. 进入WSL
wsl
```

#### 本地Docker测试 (推荐)
```bash
# 1. 构建Docker镜像 (首次需要1-1.5小时)
cd /mnt/g/workspace/XOffsetDatastructure
./scripts/docker-build.sh

# 2. 运行测试
docker run --rm \
  -v $(pwd):/workspace \
  -w /workspace \
  xoffset-clang-p2996:latest \
  bash ./build.sh

# 3. 验证结果
# 应该看到:
#   - Tests Run: 18
#   - Tests Passed: 18
#   - Status: ✓ SUCCESS
```

#### 本地直接测试 (需要安装Clang P2996)
```bash
# 1. 检查Clang P2996是否可用
which clang++ && clang++ --version | grep P2996

# 2. 运行构建脚本
./build.sh

# 3. 可选参数
./build.sh --debug              # 调试模式
./build.sh --no-reflection      # 跳过反射测试 (只运行6个基础测试)
./build.sh -j 8                 # 使用8线程并行构建
```

### 方法2: 使用Docker Compose

```bash
# 1. 一次性构建并测试
docker-compose run --rm xoffset-dev ./build.sh

# 2. 进入交互式Shell调试
docker-compose run --rm xoffset-dev bash

# 3. 手动执行单个测试
docker-compose run --rm xoffset-dev bash -c "cd build && ./bin/Release/test_basic_types"
```

---

## 🚀 远端CI/CD流程

### GitHub Actions配置

**文件**: `.github/workflows/ci.yml`

#### 工作流程

```
1. 触发条件
   - Push到 next_cpp26 分支
   - Pull Request到 next_cpp26

2. Docker镜像构建 (1-1.5小时)
   - 使用多阶段构建
   - 从源码编译Clang P2996
   - 安装Boost和CMake

3. 测试执行 (5-10分钟)
   - 挂载代码到 /workspace
   - 执行 bash ./build.sh
   - 运行18个测试
```

#### 关键配置

```yaml
# 正确的测试执行方式
- name: Run tests in Docker
  run: |
    docker run --rm \
      -v $(pwd):/workspace \
      -w /workspace \
      xoffset-clang-p2996:latest \
      bash ./build.sh  # ✅ 使用 bash 明确指定解释器
```

**⚠️ 常见错误**:
```yaml
# ❌ 错误: 直接执行可能因权限失败
./build.sh

# ❌ 错误: 使用sh可能不支持bash特性
sh ./build.sh

# ✅ 正确: 明确使用bash
bash ./build.sh
```

---

## 🔧 常见问题与解决方案

### 问题1: CMake无法识别C++26标准

**症状**:
```
CMake Error: CMAKE_CXX_COMPILER does not support -std=c++26
```

**原因**: CMake 3.10不认识`CMAKE_CXX_STANDARD 26`

**解决方案**:
```cmake
# ❌ 错误做法
set(CMAKE_CXX_STANDARD 26)

# ✅ 正确做法
add_compile_options(-std=c++26 -freflection -fexpansion-statements -stdlib=libc++)
```

### 问题2: Docker中执行build.sh失败

**症状**:
```
./build.sh: Permission denied
```

**原因**: 
- WSL/Windows文件系统挂载到Docker时可能丢失执行权限
- 容器内shebang解释器路径不一致

**解决方案**:
```bash
# ❌ 错误
docker run ... ./build.sh

# ✅ 正确: 明确使用bash
docker run ... bash ./build.sh
```

### 问题3: 反射测试编译失败

**症状**:
```
error: unknown type name 'std::meta::info'
```

**原因**: 
- 使用了系统默认编译器而非Clang P2996
- 未启用反射标志

**解决方案**:
```bash
# 检查编译器
which clang++ && clang++ --version

# 确保Clang P2996在路径中
export PATH="$HOME/clang-p2996-install/bin:$PATH"

# 或使用Docker (已内置正确编译器)
docker run ... xoffset-clang-p2996:latest bash ./build.sh
```

### 问题4: CI构建超时

**症状**: GitHub Actions在1.5小时后超时

**原因**: Docker镜像从源码编译LLVM

**解决方案**:
```yaml
# 使用缓存或预构建镜像
- name: Cache Docker layers
  uses: actions/cache@v3
  with:
    path: /tmp/.buildx-cache
    key: ${{ runner.os }}-buildx-${{ github.sha }}
    restore-keys: |
      ${{ runner.os }}-buildx-
```

### 问题5: 本地测试通过，CI失败

**症状**: 本地Docker测试成功，GitHub Actions失败

**排查步骤**:
```bash
# 1. 确保本地使用相同的Docker镜像
docker build -t xoffset-clang-p2996:latest .

# 2. 清除本地缓存重新构建
docker build --no-cache -t xoffset-clang-p2996:latest .

# 3. 使用完全相同的命令
docker run --rm -v $(pwd):/workspace -w /workspace \
  xoffset-clang-p2996:latest bash ./build.sh

# 4. 检查构建脚本的环境依赖
docker run --rm -v $(pwd):/workspace -w /workspace \
  xoffset-clang-p2996:latest bash -c "env && pwd && ls -la"
```

---

## 📄 关键配置文件

### 1. Dockerfile

**位置**: `Dockerfile`

**关键配置**:
```dockerfile
# 多阶段构建
FROM ubuntu:22.04 AS builder

# 编译Clang P2996
RUN git clone https://github.com/bloomberg/clang-p2996.git /tmp/llvm-project && \
    cmake -S /tmp/llvm-project/llvm -B /tmp/build \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_TARGETS_TO_BUILD="X86" && \
    cmake --build /tmp/build -j$(nproc)

# 运行环境
FROM ubuntu:22.04
COPY --from=builder /usr/local /usr/local
```

### 2. CMakeLists.txt

**位置**: `CMakeLists.txt`

**关键配置**:
```cmake
# C++26标准 + 反射标志
add_compile_options(
    -std=c++26
    -freflection
    -fexpansion-statements
    -stdlib=libc++
)

# 条件编译反射测试
if(ENABLE_REFLECTION_TESTS)
    add_executable(test_reflection_operators tests/test_reflection_operators.cpp)
endif()
```

### 3. build.sh

**位置**: `build.sh`

**关键逻辑**:
```bash
# 自动发现Clang P2996
find_clang_p2996() {
    local search_paths=(
        "/usr/local/bin/clang++"
        "$HOME/clang-p2996-install/bin/clang++"
        "/opt/clang-p2996/bin/clang++"
    )
    # 验证是否支持 -freflection
}

# 运行18个测试
run_test "test_basic_types" 1 18
# ... (省略)
run_test "test_vptr_layout" 18 18
```

### 4. GitHub Actions Workflow

**位置**: `.github/workflows/ci.yml`

**关键配置**:
```yaml
- name: Build Docker image
  run: |
    docker build -t xoffset-clang-p2996:latest .

- name: Run tests in Docker
  run: |
    docker run --rm \
      -v $(pwd):/workspace \
      -w /workspace \
      xoffset-clang-p2996:latest \
      bash ./build.sh
```

---

## ✅ 最佳实践清单

### 开发阶段
- [ ] 本地先用Docker测试: `docker-compose run --rm xoffset-dev ./build.sh`
- [ ] 确保18个测试全部通过
- [ ] 检查代码是否符合AGENTS.md规范
- [ ] 提交前运行: `git diff` 检查修改

### 提交前
- [ ] 提交信息清晰: `git commit -m "fix: resolve reflection test failure"`
- [ ] 推送到远端: `git push origin next_cpp26`
- [ ] 等待CI完成 (不要立即连续提交)

### CI监控
- [ ] 使用脚本监控: `powershell scripts/fetch-ci-status.ps1`
- [ ] Docker构建阶段: 预计1-1.5小时
- [ ] 测试执行阶段: 预计5-10分钟
- [ ] 失败时查看日志: 访问GitHub Actions详情

### 调试失败
- [ ] 先在本地Docker复现: `docker run ... bash ./build.sh`
- [ ] 检查是否使用正确的编译器
- [ ] 验证CMake配置是否正确
- [ ] 查看完整的构建日志

---

## 📊 性能基准

| 环境 | Docker构建 | 测试执行 | 总时长 |
|------|-----------|---------|--------|
| **本地WSL (8核)** | 60-90分钟 | 5-8分钟 | ~95分钟 |
| **GitHub Actions (2核)** | 80-100分钟 | 5-10分钟 | ~110分钟 |
| **预缓存镜像** | 0分钟 | 5-10分钟 | ~10分钟 |

---

## 🔍 快速诊断命令

```bash
# 1. 检查本地环境
docker --version
wsl --version
git status

# 2. 验证Docker镜像
docker images | grep xoffset-clang-p2996

# 3. 快速测试
docker run --rm -v $(pwd):/workspace -w /workspace \
  xoffset-clang-p2996:latest bash -c "clang++ --version"

# 4. 检查CI状态
powershell -File scripts/fetch-ci-status.ps1

# 5. 查看最近的提交
git log --oneline -5
```

---

## 📝 总结

### 核心原则
1. **本地优先**: 始终先在本地Docker测试通过
2. **明确解释器**: 使用 `bash ./build.sh` 而非 `./build.sh`
3. **手动标志**: CMake配置使用 `add_compile_options(-std=c++26)` 
4. **缓存利用**: 利用Docker层缓存加速构建

### 工作流
```
本地修改 → Docker测试 → 提交代码 → CI验证 → 合并分支
   ↓          ↓           ↓          ↓         ↓
  VSCode   docker-compose  git push  Actions  main
```

---

**维护者**: AI Assistant  
**最后更新**: 2026-02-02  
**版本**: v1.0
