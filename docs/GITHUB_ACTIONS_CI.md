# GitHub Actions CI Guide

本文档说明如何使用 GitHub Actions 进行跨平台测试。

## 🎯 支持的平台

GitHub Actions CI 自动测试以下平台：

| 平台 | 编译器 | 架构 | 测试类型 |
|------|--------|------|---------|
| **macOS** | Clang (Apple) | x86_64 | 完整测试 + 运行 |
| **Linux** | GCC 11, Clang 14 | x86_64 | 完整测试 + 运行 |
| **Windows** | MSVC 2022 | x64 | 完整测试 + 运行 |
| **Android** | Clang (NDK r25c) | arm64-v8a, armeabi-v7a, x86_64 | 编译 + 模拟器测试 |
| **iOS** | Clang (Xcode) | arm64, x86_64 (sim) | 编译 + 模拟器测试 |

## 🚀 触发方式

### 1. 自动触发

CI 会在以下情况自动运行：

```yaml
- push 到 main 或 develop 分支
- Pull Request 到 main 或 develop 分支
```

### 2. 手动触发

在 GitHub 网页上：
1. 进入 **Actions** 标签
2. 选择 **Cross-Platform CI** workflow
3. 点击 **Run workflow**
4. 选择分支并运行

## 📊 测试任务详情

### macOS Testing
```yaml
matrix:
  build_type: [Debug, Release]

steps:
  - 编译 (Clang)
  - 运行所有 CTest
  - 运行 test_msvc_compat
  - 运行 demo
  - 上传测试结果
```

**预期结果**: ✅ 所有测试通过

### Linux Testing
```yaml
matrix:
  compiler: [gcc, clang]
  build_type: [Debug, Release]

steps:
  - 编译 (GCC 11 或 Clang 14)
  - 运行所有 CTest
  - 运行 test_msvc_compat
  - 运行 demo
  - 上传测试结果
```

**预期结果**: ✅ 所有组合通过 (4个jobs)

### Windows MSVC Testing
```yaml
matrix:
  build_type: [Debug, Release]
  arch: [x64]

steps:
  - 编译 (MSVC 2022)
  - 运行所有 CTest
  - 运行 test_msvc_compat
  - 运行 demo
  - **实验性**: 尝试启用类型签名验证
  - 上传测试结果
```

**预期结果**: 
- ✅ 标准测试全部通过
- ⚠️ 类型签名验证可能失败（预期行为）

### Android NDK Testing
```yaml
matrix:
  abi: [arm64-v8a, armeabi-v7a, x86_64]
  api_level: [30]

steps:
  - 使用 NDK r25c 编译
  - 列出编译的二进制文件
  - (仅 x86_64) 在模拟器上运行测试
  - 上传 Android 二进制文件
```

**预期结果**: ✅ 编译成功，x86_64 在模拟器运行通过

### iOS Testing
```yaml
matrix:
  platform: [iOS, iOS-simulator]

steps:
  - 使用 Xcode 15 编译
  - (仅模拟器) 在 iPhone 15 Pro 模拟器运行测试
  - 上传 iOS 二进制文件
```

**预期结果**: ✅ 编译成功，模拟器测试通过

## 🔍 额外检查

### Code Quality
- 验证所有 YAML schema 文件
- 检查生成的代码是否最新

### Performance Benchmarks
- 使用 `-O3 -march=native` 编译
- 运行性能基准测试
- 测量内存使用

### Binary Compatibility
- 下载所有平台的构建产物
- 验证二进制兼容性

## 📦 构建产物 (Artifacts)

每个平台的测试结果和二进制文件会作为 artifacts 上传：

| Artifact 名称 | 内容 | 保留时间 |
|--------------|------|---------|
| `test-results-macos-{Debug/Release}` | macOS 测试结果 | 30天 |
| `test-results-linux-{gcc/clang}-{Debug/Release}` | Linux 测试结果 | 30天 |
| `test-results-windows-msvc-{Debug/Release}` | Windows 测试结果 | 30天 |
| `android-binaries-{abi}` | Android 可执行文件 | 30天 |
| `ios-binaries-{platform}` | iOS 可执行文件 | 30天 |

### 下载 Artifacts

1. 进入 GitHub Actions 运行页面
2. 滚动到底部的 **Artifacts** 部分
3. 点击下载所需的 artifact

## 🛠️ 本地复现 CI 环境

### macOS
```bash
# 安装依赖
brew install cmake python3
pip3 install pyyaml

# 编译和测试
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(sysctl -n hw.ncpu)
ctest --output-on-failure -V
```

### Linux (Docker)
```bash
# 使用 Ubuntu 22.04
docker run -it --rm -v $(pwd):/workspace ubuntu:22.04 bash

# 在容器内
apt-get update
apt-get install -y build-essential cmake python3-pip git
pip3 install pyyaml

cd /workspace
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
ctest --output-on-failure -V
```

### Windows (PowerShell)
```powershell
# 安装依赖 (需要 Visual Studio 2022)
pip install pyyaml

# 编译和测试
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
ctest -C Release --output-on-failure -V
```

### Android
```bash
# 设置 NDK
export ANDROID_NDK_HOME=/path/to/ndk-r25c

# 配置
mkdir build-android
cd build-android
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30 \
  -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release \
  ..

# 编译
cmake --build . -j$(nproc)

# 部署到设备
adb push bin/test_msvc_compat /data/local/tmp/
adb shell "cd /data/local/tmp && chmod +x test_msvc_compat && ./test_msvc_compat"
```

### iOS
```bash
# 配置
mkdir build-ios-sim
cd build-ios-sim
cmake -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  ..

# 编译
cmake --build . --config Release

# 在模拟器运行
xcrun simctl boot "iPhone 15 Pro"
xcrun simctl spawn booted ./bin/Release/test_msvc_compat
```

## ⚙️ CI 配置选项

### 修改测试的平台

编辑 `.github/workflows/ci.yml`:

```yaml
# 禁用某个平台
test-android:
  if: false  # 跳过 Android 测试
  
# 修改测试矩阵
strategy:
  matrix:
    # 添加更多 ABI
    abi: [arm64-v8a, armeabi-v7a, x86_64, x86]
```

### 修改触发条件

```yaml
on:
  push:
    branches: [ main, develop, feature/* ]  # 添加 feature 分支
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 0 * * 0'  # 每周日运行
```

### 添加 Slack/Email 通知

```yaml
- name: Notify on failure
  if: failure()
  uses: 8398a7/action-slack@v3
  with:
    status: ${{ job.status }}
    text: 'Build failed!'
    webhook_url: ${{ secrets.SLACK_WEBHOOK }}
```

## 🐛 常见问题

### Q: iOS 测试失败 "Xcode not found"
**A**: GitHub Actions macOS runner 预装了多个 Xcode 版本。如果失败，尝试：
```yaml
- name: Select Xcode
  run: sudo xcode-select -s /Applications/Xcode_14.3.app
```

### Q: Android 模拟器超时
**A**: 模拟器启动可能很慢。增加超时时间：
```yaml
- name: Setup Android Emulator
  timeout-minutes: 30  # 增加超时
```

### Q: MSVC 类型签名验证失败
**A**: 这是预期行为。MSVC 默认禁用类型签名验证。如需启用：
```bash
cmake -DENABLE_MSVC_TYPE_SIGNATURE_VALIDATION=ON ..
```

### Q: 如何查看详细日志？
**A**: 
1. 点击失败的 job
2. 展开失败的 step
3. 查看完整输出

## 📈 监控 CI 状态

### 添加 Badge 到 README

```markdown
[![CI Status](https://github.com/username/repo/workflows/Cross-Platform%20CI/badge.svg)](https://github.com/username/repo/actions)
```

### 查看历史趋势

GitHub Actions 提供：
- 成功率统计
- 运行时间趋势
- 资源使用情况

## 🚦 下一步

1. ✅ 提交代码并 push 到 GitHub
2. ✅ 等待 CI 自动运行
3. ✅ 检查所有平台的测试结果
4. ✅ 下载 artifacts 进行本地验证
5. ✅ 根据需要调整 CI 配置

## 💡 最佳实践

1. **频繁提交**: 小改动更容易定位问题
2. **本地测试**: 先在本地验证再 push
3. **查看日志**: 及时修复 CI 失败
4. **保持更新**: 定期更新依赖版本
5. **文档同步**: CI 变更时更新文档

---

**相关文档**:
- [MSVC兼容性](./MSVC_COMPATIBILITY.md)
- [MSVC类型签名验证](./MSVC_TYPE_SIGNATURE_VALIDATION.md)
- [快速开始](./QUICK_START.md)
