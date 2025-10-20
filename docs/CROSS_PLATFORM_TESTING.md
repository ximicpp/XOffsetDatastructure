# Cross-Platform Testing Guide

This guide explains how to test XOffsetDatastructure2 on multiple platforms from macOS.

## Platform Overview

| Platform | Compiler | Test Method | Binary Compatibility |
|----------|----------|-------------|---------------------|
| **macOS** | Clang/GCC | Native | Reference Platform |
| **Linux** | GCC/Clang | Docker/VM | High |
| **Windows (MSVC)** | MSVC | Docker/VM/Wine | Medium (different ABI) |
| **Android** | Clang (NDK) | Cross-compile | High |
| **iOS** | Clang | Xcode | High |

## 1. macOS (Native) - Reference Platform

### Build and Test

```bash
# Clone and build
git clone --recursive https://github.com/your-repo/XOffsetDatastructure.git
cd XOffsetDatastructure

# Create build directory
mkdir build && cd build

# Configure with Clang
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . --config Release

# Run tests
ctest -C Release -V

# Run MSVC compatibility test (should pass on all platforms)
./bin/test_msvc_compat
```

### Enable Full Type Signature Validation

```bash
# macOS/Linux/iOS/Android use Clang, which supports full validation
cmake -DENABLE_MSVC_TYPE_SIGNATURE_VALIDATION=ON ..
cmake --build . --config Release
```

## 2. Linux Testing (Docker)

### Using Docker

Create `Dockerfile.linux`:

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install PyYAML
RUN pip3 install pyyaml

WORKDIR /workspace

# Copy source
COPY . .

# Build
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release

# Run tests
CMD ["sh", "-c", "cd build && ctest -C Release -V"]
```

### Build and Test

```bash
# Build Docker image
docker build -f Dockerfile.linux -t xoffset-linux .

# Run tests
docker run --rm xoffset-linux

# Interactive testing
docker run --rm -it xoffset-linux bash
cd build/bin
./test_msvc_compat
./demo
```

### Test Multiple Linux Distributions

```bash
# Ubuntu 20.04
docker build -f Dockerfile.ubuntu20 -t xoffset-ubuntu20 .

# Ubuntu 22.04
docker build -f Dockerfile.ubuntu22 -t xoffset-ubuntu22 .

# Alpine Linux
docker build -f Dockerfile.alpine -t xoffset-alpine .

# CentOS/Rocky Linux
docker build -f Dockerfile.rocky -t xoffset-rocky .
```

## 3. Windows/MSVC Testing

### Option A: Docker with Windows Containers (Requires Windows Host or VM)

Not practical from macOS - Windows containers need Windows host.

### Option B: Cross-compile with MinGW (Limited)

```bash
# Install MinGW
brew install mingw-w64

# Configure with MinGW
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake ..
```

**Note**: MinGW uses GCC, not MSVC. Won't test MSVC-specific issues.

### Option C: GitHub Actions (Recommended)

Create `.github/workflows/ci.yml`:

```yaml
name: Cross-Platform CI

on: [push, pull_request]

jobs:
  test-windows-msvc:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      
      - name: Install PyYAML
        run: pip install pyyaml
      
      - name: Configure CMake
        run: cmake -B build -G "Visual Studio 17 2022" -A x64
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Test
        run: cd build && ctest -C Release -V
      
      - name: Run MSVC Compat Test
        run: .\build\bin\Release\test_msvc_compat.exe

  test-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake python3 python3-pip
          pip3 install pyyaml
      
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          cmake --build . --config Release
      
      - name: Test
        run: cd build && ctest -C Release -V

  test-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Install dependencies
        run: |
          brew install cmake python3
          pip3 install pyyaml
      
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          cmake --build . --config Release
      
      - name: Test
        run: cd build && ctest -C Release -V
```

### Option D: Azure Pipelines (Alternative CI)

Create `azure-pipelines.yml`:

```yaml
trigger:
  - main

pool:
  vmImage: 'windows-latest'

steps:
- task: UsePythonVersion@0
  inputs:
    versionSpec: '3.x'

- script: |
    pip install pyyaml
  displayName: 'Install PyYAML'

- task: CMake@1
  inputs:
    cmakeArgs: '-B build -G "Visual Studio 17 2022" -A x64'

- task: CMake@1
  inputs:
    cmakeArgs: '--build build --config Release'

- script: |
    cd build
    ctest -C Release -V
  displayName: 'Run Tests'
```

## 4. Android Testing (NDK)

### Install Android NDK

```bash
# Install Android Studio first, or download NDK directly
brew install --cask android-studio

# Or download NDK standalone
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c
```

### Create CMake Toolchain File

`cmake/android.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 30)  # API Level 30
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_HOME})
set(CMAKE_ANDROID_STL_TYPE c++_static)
```

### Cross-compile for Android

```bash
# Configure
cmake -B build-android \
      -DCMAKE_TOOLCHAIN_FILE=cmake/android.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# Build
cmake --build build-android --config Release

# The binaries are in build-android/bin/
# Deploy to Android device:
adb push build-android/bin/test_msvc_compat /data/local/tmp/
adb shell "cd /data/local/tmp && chmod +x test_msvc_compat && ./test_msvc_compat"
```

### Test on Android Emulator

```bash
# Start emulator
emulator -avd Pixel_5_API_30 &

# Push and run tests
adb push build-android/bin/* /data/local/tmp/
adb shell "cd /data/local/tmp && ./test_msvc_compat"
```

## 5. iOS Testing

### Using Xcode

```bash
# Configure for iOS
cmake -B build-ios \
      -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      ..

# Build
cmake --build build-ios --config Release

# Or open in Xcode
open build-ios/XOffsetDatastructure.xcodeproj
```

### iOS Simulator Testing

```bash
# Configure for iOS Simulator
cmake -B build-ios-sim \
      -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_SYSROOT=iphonesimulator \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 \
      ..

# Build
cmake --build build-ios-sim --config Release

# Run in simulator (requires Xcode command line tools)
xcrun simctl boot "iPhone 14 Pro"
xcrun simctl install booted build-ios-sim/bin/Release/test_msvc_compat
xcrun simctl spawn booted test_msvc_compat
```

### Create iOS Test App

Create a simple iOS app wrapper for running tests:

`ios/TestApp/main.mm`:

```objc
#import <UIKit/UIKit.h>

// Declare test functions
extern "C" int run_all_tests();

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Run tests
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        int result = run_all_tests();
        NSLog(@"Tests %@", result == 0 ? @"PASSED" : @"FAILED");
    });
    
    return YES;
}

@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
```

## 6. Automated Cross-Platform Testing Script

Create `scripts/test_all_platforms.sh`:

```bash
#!/bin/bash
set -e

echo "======================================"
echo "XOffsetDatastructure2 Cross-Platform Test"
echo "======================================"

# Test macOS (native)
echo "\n[1/5] Testing macOS..."
mkdir -p build-macos && cd build-macos
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
ctest -C Release -V
cd ..

# Test Linux (Docker)
echo "\n[2/5] Testing Linux (Docker)..."
docker build -f Dockerfile.linux -t xoffset-linux .
docker run --rm xoffset-linux

# Test Android (if NDK available)
if [ -n "$ANDROID_NDK_HOME" ]; then
    echo "\n[3/5] Testing Android..."
    mkdir -p build-android && cd build-android
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/android.cmake \
          -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . --config Release
    cd ..
else
    echo "\n[3/5] Skipping Android (NDK not found)"
fi

# Test iOS Simulator
echo "\n[4/5] Testing iOS Simulator..."
mkdir -p build-ios-sim && cd build-ios-sim
cmake -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_SYSROOT=iphonesimulator \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 ..
cmake --build . --config Release
cd ..

# Test MSVC (GitHub Actions only)
echo "\n[5/5] MSVC testing requires Windows - use GitHub Actions"
echo "See .github/workflows/ci.yml"

echo "\n======================================"
echo "Cross-platform testing complete!"
echo "======================================"
```

## 7. Binary Compatibility Testing

### Create Serialization Compatibility Test

```cpp
// tests/test_cross_platform_compat.cpp
#include <iostream>
#include <fstream>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

void create_test_data(const char* filename) {
    XBufferExt xbuf(4096);
    auto* game = xbuf.make<GameData>("game");
    
    game->player_name = XString("CrossPlatformTest", xbuf.allocator<XString>());
    game->player_id = 12345;
    game->level = 99;
    game->health = 100.0f;
    
    // Save to file
    std::string data = xbuf.save_to_string();
    std::ofstream ofs(filename, std::ios::binary);
    ofs.write(data.data(), data.size());
    ofs.close();
    
    std::cout << "Created test data: " << filename << " (" << data.size() << " bytes)\n";
}

bool verify_test_data(const char* filename) {
    // Load from file
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open " << filename << "\n";
        return false;
    }
    
    std::string data((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    ifs.close();
    
    // Deserialize
    XBufferExt xbuf = XBufferExt::load_from_string(data);
    auto* game = xbuf.find<GameData>("game").first;
    
    if (!game) {
        std::cerr << "Failed to deserialize\n";
        return false;
    }
    
    // Verify
    bool ok = (
        std::string(game->player_name.c_str()) == "CrossPlatformTest" &&
        game->player_id == 12345 &&
        game->level == 99 &&
        game->health == 100.0f
    );
    
    std::cout << "Verified test data: " << (ok ? "OK" : "FAILED") << "\n";
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <create|verify> <filename>\n";
        return 1;
    }
    
    std::string mode = argv[1];
    const char* filename = argc > 2 ? argv[2] : "test_data.bin";
    
    if (mode == "create") {
        create_test_data(filename);
    } else if (mode == "verify") {
        return verify_test_data(filename) ? 0 : 1;
    }
    
    return 0;
}
```

### Test Binary Compatibility Across Platforms

```bash
# On macOS, create test data
./build-macos/bin/test_cross_platform_compat create test_data_macos.bin

# Verify on Linux (Docker)
docker run --rm -v $(pwd):/data xoffset-linux \
    /workspace/build/bin/test_cross_platform_compat verify /data/test_data_macos.bin

# Verify on Android
adb push test_data_macos.bin /data/local/tmp/
adb shell "/data/local/tmp/test_cross_platform_compat verify /data/local/tmp/test_data_macos.bin"
```

## 8. Continuous Integration Matrix

### GitHub Actions Matrix Strategy

`.github/workflows/ci-matrix.yml`:

```yaml
name: CI Matrix

on: [push, pull_request]

jobs:
  test:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        build_type: [Debug, Release]
        include:
          - os: windows-latest
            compiler: msvc
          - os: ubuntu-latest
            compiler: gcc
          - os: macos-latest
            compiler: clang
    
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Install dependencies
        run: |
          if [ "$RUNNER_OS" == "Linux" ]; then
            sudo apt-get update
            sudo apt-get install -y cmake python3-pip
          elif [ "$RUNNER_OS" == "macOS" ]; then
            brew install cmake python3
          fi
          pip3 install pyyaml
        shell: bash
      
      - name: Configure
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} ..
      
      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }}
      
      - name: Test
        run: cd build && ctest -C ${{ matrix.build_type }} -V
      
      - name: Upload test results
        uses: actions/upload-artifact@v3
        if: always()
        with:
          name: test-results-${{ matrix.os }}-${{ matrix.build_type }}
          path: build/Testing/
```

## Summary

| Platform | Best Test Method | Binary Compat | Effort |
|----------|-----------------|---------------|--------|
| macOS | Native | ✅ High | Low |
| Linux | Docker | ✅ High | Low |
| Windows/MSVC | GitHub Actions | ⚠️ Medium | Low |
| Android | NDK + adb | ✅ High | Medium |
| iOS | Xcode + Simulator | ✅ High | Medium |

**Recommended Workflow**:
1. Develop and test on macOS (native)
2. Use Docker for Linux testing
3. Use GitHub Actions for MSVC testing
4. Cross-compile for Android/iOS as needed
5. Run binary compatibility tests across all platforms
