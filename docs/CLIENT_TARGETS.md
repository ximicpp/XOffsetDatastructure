# 客户端目标可行性（K8）

服务端与客户端必须用同一 schema、同时发布（见产品定位）。库有两条构建路径：

- **C++26**：P2996 反射描述每个记录，需要 clang-p2996 或带反射的 GCC。
- **C++20 / C++23**：每个记录写一行 `XOFFSET_RECORD` 字段表（`-DXOFFSET_CXX_STANDARD=20|23`），
  不需要反射，原生客户端编译器都能用。wire 与 C++26 构建逐字节一致。

## C++20 / C++23 字段表构建（去 C++26 探索分支，2026-10-27）

运行入口：`tools/native_targets.sh TARGET --std 20|23 --ref <参照语料>`（工具链文件在
`cmake/toolchains/`）。每个目标都跑 Release 全量 CTest，然后做 wire 语料检查：写出本机语料，
读入参照 wire（必须全部接受），再与参照语料 `diff -r`。参照语料由 Linux x86-64 GCC 16 C++26
（反射）写出，共 6 个：elided、empty、medium_holes、reloaded_mutated、small、sparse，
与 clang-p2996 写出的语料逐字节相同。

| 目标 | 编译器 | 标准 | 实际运行方式 | 结果 |
| --- | --- | --- | --- | --- |
| Android arm64-v8a | NDK r29 Clang 21.0.0，API 28，`c++_static` | C++20、C++23 | 全静态链接，云端 x86-64 上用 `qemu-aarch64-static` 跑 CTest | 两种标准都是 101/101；语料全部接受，逐字节一致 |
| Android arm64-v8a | NDK r30 Clang 21.0.0 | C++20 | 同上 | 见 `native/results-b3fe2a9/` |
| Windows x64 | MinGW-w64 GCC 13.2（posix 线程） | C++20、C++23 | `-static`，云端用 wine 9.0 跑 CTest | 两种标准都是 101/101（需要 #119，见下）；语料全部接受，逐字节一致 |
| macOS arm64 | Apple clang（Command Line Tools，系统 libc++） | C++20、C++23 | 待 Mac 队列非计时槽位（`run_native_macos.sh`） | 未运行 |
| iOS 模拟器 | Apple clang + iOS SDK | C++20 | 工具链文件已写（`ios-simulator.cmake`，经 `simctl spawn` 跑） | **阻塞：需要 Xcode**，Mac 上只有 Command Line Tools，没有 iOS SDK 和 simctl |
| Windows x64 | MSVC | C++20 | 没有环境 | **未验证**，风险见下 |

各目标需要注意的地方：

- **Android**：测试里的分配失败注入用 `aligned_alloc`，bionic 从 API 28 起才有，所以测试构建
  默认 API 28；库本身不依赖它。没有 KVM 时只能跑全静态的 bionic 程序，工具链文件在
  `XOFFSET_ANDROID_QEMU=ON` 时加 `-static`。bionic 的 `__cxa_thread_atexit_impl` 用
  `operator new` 分配线程析构节点（glibc 用 calloc），所以线程第一次使用 thread_local 缓存时，
  替换了 `operator new` 的测试会多看到一次分配。
- **Windows（MinGW GCC）**：GCC 在 MinGW 上用模拟 TLS（emutls），库里的每线程缓存在线程退出时
  会崩溃或写坏内存。#119 在 GCC + MinGW/Cygwin 下默认关闭这些缓存（`XOFFSET_THREAD_CACHES=0`），
  不带 #119 时 C++20 是 94/101，C++23 是 93/101。Clang 面向 mingw 用原生 TLS，没有这个问题。
  `windows.h` 定义了 `near` 宏，不要用它作标识符。GCC 16 的 MinGW 包在云端拿不到，所以跑的是 13.2。
- **macOS / iOS（std::pmr）**：库用系统 libc++ 的 `std::pmr`（`new_delete_resource`、
  `monotonic_buffer_resource`、`memory_resource` 虚表）。系统 libc++ 从 macOS 14 / iOS 17 起才有这些
  符号，而 libc++ 头文件（上游和 Apple SDK 都一样）没有给 pmr 加可用性标注，部署目标更低时
  照样能编译、链接，到旧系统上加载时才失败。所以 C++20/23 构建的默认部署目标是 macOS 14，
  更低的部署目标在编译期报错（探索分支 067b112）。C++26 构建带 clang-p2996 自己的 libc++，不受影响。
  NDK 和 MinGW 把标准库静态链进程序，没有这个系统版本门槛。
- **MSVC（未验证）**：`XOFFSET_RECORD` 依赖 `__VA_OPT__`，需要符合标准的预处理器
  `/Zc:preprocessor`（CMake 接口目标应自动加上）；字段表的编译期检查和签名计算可能超过 MSVC
  默认的常量求值步数上限，而用户不能被要求加大这个上限；MSVC 忽略 `[[no_unique_address]]`，
  布局不同时 schema 哈希不同、对端拒收，不会静默出错。Windows 客户端不依赖 MSVC：MinGW 这条路已验证。

## C++26 反射构建（0.3.0-beta.1）

以下是 GitHub CI 停用前 `.github/workflows/client-targets.yml` 的验证结果。

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

C++26 路径下 Android 和 iOS 不可行：NDK Clang 没有 P2996 反射，NDK 自 r18 起也不再提供 GCC；
P2996 工具链只有 macOS 版 libc++ 和 `*_osx` compiler-rt，没有 iOS / iOS 模拟器版。这两个目标
只能走 C++20 / C++23 字段表构建。

## 范围

只支持 64 位小端目标。32 位客户端、大端平台不在范围内。
