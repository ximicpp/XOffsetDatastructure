# 客户端目标可行性（K8，1.0.0）

结论：服务端与客户端之间使用 XOffset，在 **Linux arm64** 和 **Windows x64（MinGW-w64 GCC 16）**
上已由 CI 验证；**Android** 和 **iOS** 目前不可行，缺的是工具链前提，不是库本身的问题。
服务端与客户端必须用同一 schema、同时发布（见产品定位）。

## 已验证（`.github/workflows/client-targets.yml`）

| 目标 | 编译器 | 验证内容 | 结果 |
| --- | --- | --- | --- |
| Linux x86-64（参照） | GCC 16（gcc 镜像按 digest 固定） | 写出参照 wire 语料 | 作为其他目标的比对基准 |
| Linux arm64（GitHub `ubuntu-24.04-arm` 原生） | GCC 16（arm64 平台 manifest digest 固定） | Debug 全量 CTest；读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过，wire 逐字节一致 |
| Windows x64（`windows-latest` + MSYS2 UCRT64） | MinGW-w64 GCC 16.2（要求 ≥ 16） | Debug 全量 CTest 87/87（Ninja）；Release 构建读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过；5 个语料（empty、small、medium_holes、reloaded_mutated、sparse）逐字节一致 |

Windows 上需要注意的三点（都已在仓库里处理）：

- 依赖锁按子模块字节做哈希，checkout 前要 `core.autocrlf false`、`core.eol lf`，否则 Boost 的
  `* text=auto` 会把文件转成 CRLF，锁校验失败。
- 顶层构建在 MinGW 上静态链接运行时（`-static`）。动态 `libstdc++-6.dll` 里的 `std::pmr` 等
  内部分配不经过测试替换的 `operator new`，分配失败注入类测试会测不到。这只影响本仓库自己的
  测试和工具，不进入导出的接口。
- 签名导出工具以二进制方式写文件，提交的签名基线在所有主机上都是 LF。

## 目前不可行

**Android（arm64-v8a）**：NDK 自带的 Clang 没有 P2996 反射，NDK 自 r18 起也不再提供 GCC，
所以现在没有能编译 XOffset 的 Android 工具链。Linux arm64 的结果说明同一 ABI（LP64、小端、
AArch64 对齐规则）下 wire 一致，但这只是旁证，不等于 Android 已验证。前提：P2996（或 C++26
反射）进入 NDK Clang，或者自行构建一套面向 Android 的 P2996 Clang + libc++。

**iOS / iOS 模拟器**：在 Apple M4 Mac 上用 clang-p2996 d77eff1 探测（2026-10-21），结果是：

- 没有 Xcode，没有 iOS / iOS 模拟器 SDK（`xcrun --sdk iphoneos` 找不到 SDK）；
- P2996 工具链只有 macOS 版 libc++（`LC_BUILD_VERSION` platform 1）和 `*_osx` compiler-rt，
  没有 ios / iossim 版，也没有按 target 分开的 `__config_site`。

所以即使装上 Xcode 也只能走到编译，链接仍会失败。前提：Xcode + iOS SDK，并为 P2996 工具链
单独构建 iOS 与 iOS 模拟器版的 libc++ 和 compiler-rt。

## 范围

只支持 64 位目标。32 位客户端、大端平台不在 1.0.0 范围内。
