# XOffsetDatastructure2 跨平台测试快速参考

## 📋 平台支持矩阵

```
✅ 完全支持    ⚠️ 部分支持    ❌ 不支持
```

| 平台 | 架构 | 编译 | 运行测试 | 类型签名验证 |
|------|------|------|---------|-------------|
| **macOS** | x86_64, arm64 | ✅ | ✅ | ✅ 完整 |
| **Linux** | x86_64, arm64 | ✅ | ✅ | ✅ 完整 |
| **Windows** | x64 | ✅ | ✅ | ⚠️ 大小/对齐 |
| **iOS** | arm64, x86_64 | ✅ | ✅ | ✅ 完整 |
| **Android** | arm64-v8a, armeabi-v7a, x86_64 | ✅ | ✅ | ✅ 完整 |

## 🚀 快速命令

### 本地测试

```bash
# macOS/Linux
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
ctest -V

# Windows (PowerShell)
mkdir build; cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
ctest -C Release -V
```

### GitHub Actions

```bash
# 推送到 main/develop 分支自动触发
git push origin main

# 或手动触发
# 在 GitHub 网页: Actions → Cross-Platform CI → Run workflow
```

### Android 本地测试

```bash
export ANDROID_NDK_HOME=/path/to/ndk-r25c
mkdir build-android && cd build-android
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30 \
  -DANDROID_STL=c++_static \
  ..
cmake --build . -j$(nproc)

# 部署到设备
adb push bin/test_msvc_compat /data/local/tmp/
adb shell "cd /data/local/tmp && chmod +x test_msvc_compat && ./test_msvc_compat"
```

### iOS 本地测试

```bash
# iOS 模拟器
mkdir build-ios-sim && cd build-ios-sim
cmake -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  ..
cmake --build . --config Release

# 运行在模拟器
xcrun simctl boot "iPhone 15 Pro"
xcrun simctl spawn booted ./bin/Release/test_msvc_compat
```

## 🔍 CI 工作流程

### 触发条件
- ✅ Push 到 `main` 或 `develop` 分支
- ✅ Pull Request 到 `main` 或 `develop`
- ✅ 手动触发 (workflow_dispatch)

### 并行任务
1. **test-macos** (2 jobs: Debug, Release)
2. **test-linux** (4 jobs: gcc×2, clang×2)
3. **test-windows-msvc** (2 jobs: Debug, Release)
4. **test-android** (3 jobs: arm64-v8a, armeabi-v7a, x86_64)
5. **test-ios** (2 jobs: iOS, iOS-simulator)
6. **test-binary-compatibility** (1 job)
7. **code-quality** (1 job)
8. **benchmarks** (1 job)
9. **test-summary** (1 job)

**总计**: 16 个并行任务

## 📦 Artifacts 下载

```bash
# 下载所有平台的构建产物
# GitHub → Actions → 选择运行 → Artifacts 部分

# 可用的 artifacts:
- test-results-macos-{Debug|Release}
- test-results-linux-{gcc|clang}-{Debug|Release}
- test-results-windows-msvc-{Debug|Release}
- android-binaries-{abi}
- ios-binaries-{platform}
```

## 🛠️ 常用调试命令

### 查看平台信息

```bash
# macOS
uname -a
sw_vers
clang --version

# Linux
uname -a
lsb_release -a
g++ --version

# Windows (PowerShell)
systeminfo
cl.exe
```

### 验证 Boost 设置

```bash
# 检查 Boost 头文件
ls external/boost/libs/*/include

# 测试 Boost.PFR
echo '#include <boost/pfr.hpp>' | g++ -std=c++20 -x c++ - -I external/boost/libs/pfr/include -fsyntax-only
```

### 生成详细构建日志

```bash
# CMake 配置详细输出
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..

# 编译详细输出
cmake --build . --verbose

# CTest 详细输出
ctest -V --output-on-failure
```

## 🐛 常见问题速查

### "Submodules not initialized"
```bash
git submodule update --init --recursive
```

### "Python not found"
```bash
# macOS
brew install python3
# Linux
sudo apt-get install python3 python3-pip
# Windows
# 从 python.org 下载安装
```

### "PyYAML not found"
```bash
pip3 install pyyaml
```

### MSVC 类型签名验证失败
```bash
# 预期行为 - 使用以下选项禁用:
cmake -DENABLE_MSVC_TYPE_SIGNATURE_VALIDATION=OFF ..
```

### Android NDK 未找到
```bash
# 下载 NDK
wget https://dl.google.com/android/repository/android-ndk-r25c-darwin.dmg
# 设置环境变量
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c
```

### iOS 构建失败
```bash
# 选择正确的 Xcode
sudo xcode-select -s /Applications/Xcode.app
# 安装命令行工具
xcode-select --install
```

## 📊 CI 状态检查

### Badge 状态

| Badge | 含义 |
|-------|------|
| ![Passing](https://img.shields.io/badge/build-passing-brightgreen) | 所有平台通过 |
| ![Failing](https://img.shields.io/badge/build-failing-red) | 至少一个平台失败 |
| ![Pending](https://img.shields.io/badge/build-pending-yellow) | 正在运行 |

### 查看详细结果

```bash
# 命令行查看最新 CI 状态 (需要 gh CLI)
gh run list --workflow=ci.yml --limit 1

# 查看特定运行的日志
gh run view <run-id> --log

# 下载 artifacts
gh run download <run-id>
```

## 🎯 预期测试时间

| 平台 | 配置时间 | 编译时间 | 测试时间 | 总计 |
|------|---------|---------|---------|------|
| macOS | ~2min | ~3min | ~1min | ~6min |
| Linux | ~2min | ~3min | ~1min | ~6min |
| Windows | ~2min | ~5min | ~1min | ~8min |
| Android | ~2min | ~4min | ~2min | ~8min |
| iOS | ~2min | ~5min | ~2min | ~9min |

**完整 CI 运行时间**: ~10-15 分钟 (并行执行)

## 💡 性能优化提示

### 加速本地构建

```bash
# 使用 ccache (Linux/macOS)
sudo apt-get install ccache  # 或 brew install ccache
export CC="ccache gcc"
export CXX="ccache g++"

# 使用 Ninja (更快的构建系统)
cmake -G Ninja ..
ninja -j$(nproc)

# 使用预编译头 (CMake 3.16+)
target_precompile_headers(mylib PRIVATE xoffsetdatastructure2.hpp)
```

### 仅运行特定测试

```bash
# 运行单个测试
ctest -R test_msvc_compat

# 运行匹配模式的测试
ctest -R "test_.*"

# 排除某些测试
ctest -E "test_comprehensive"
```

## 📚 相关文档链接

- 📘 [Quick Start Guide](docs/QUICK_START.md)
- 🔧 [MSVC Compatibility](docs/MSVC_COMPATIBILITY.md)
- 🧪 [GitHub Actions CI详解](docs/GITHUB_ACTIONS_CI.md)
- 📝 [Type Signature Validation](docs/MSVC_TYPE_SIGNATURE_VALIDATION.md)
- 🏗️ [CMakeLists.txt](CMakeLists.txt)
- ⚙️ [CI Workflow](.github/workflows/ci.yml)

---

**提示**: 将此文件保存为书签，便于快速查阅！
