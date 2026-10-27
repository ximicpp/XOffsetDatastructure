# 客户端目标可行性（K8，3.1.0）

结论：服务端与客户端之间使用 XOffset，在 **Windows x64（MinGW-w64 GCC 16）** 上已验证，
**Linux arm64** 作为 Android 的旁证也已验证；记录是本地平台矩阵（3.1.0 RC 五列全过，见下一节）。
**Android** 和 **iOS** 在 3.1.0 上不可行，缺的是工具链前提，不是库本身的问题。
服务端与客户端必须用同一 schema、同时发布（见 [DEPLOYMENT.md](DEPLOYMENT.md) 第 2 节）。

## 已验证：本地平台矩阵（3.1.0 起的记录）

GitHub CI 自 2026-10-23 停用，`.github/workflows/` 里的文件不再运行。现在的记录是
`tools/platform_matrix.sh TARGET`：它构建、跑全量 CTest，并拿 Linux x86-64 GCC 16 写出的参照语料做
`--read` 和 `diff -r`，结果写进构建目录的 `PLATFORM_RESULT.md`。

| 目标 | 在哪跑 | 编译器 | 最近结果 |
| --- | --- | --- | --- |
| Windows x64 | 云端 Linux，CTest 经 wine 运行 | 自建 MinGW-w64 GCC 16.2.0（`tools/build_mingw_toolchain.sh`，`cmake/toolchains/mingw-w64-x86_64.cmake`） | 3.1.0 RC（e913d22）：107 项 105 过、2 项跳过，V2.1 语料逐字节一致 |
| Linux arm64 | Mac M4 的 colima Linux 虚拟机（Linux 6.8 aarch64，4 KiB 页），随 Mac 检查点批次 | GCC 16（gcc:16 arm64 镜像按 digest 固定）、P2996 d77eff1 | 3.1.0 RC（e913d22）：两者都 107/107，V2.1 语料逐字节一致 |
| Linux x86-64 | 云端 | GCC 16（写参照语料）、P2996 | 3.1.0 RC（e913d22）：两者都 107/107，P2996 语料与参照逐字节一致 |

Windows 上跳过的两项是 `test_canonical_cold_no_callback` 和 `test_canonical_zero_target`：它们测的是
每线程 owner 缓存，而 GCC 在 MinGW 上用 emutls，线程退出时访问 `thread_local` 会拿到新块，所以这类
缓存在 MinGW GCC 上整体关闭（`XOFFSET_THREAD_CACHES=0`），这两项以退出码 77 报跳过。

Windows 上需要注意的三点（都已在仓库里处理）：

- 依赖锁按子模块字节做哈希，checkout 前要 `core.autocrlf false`、`core.eol lf`，否则 Boost 的
  `* text=auto` 会把文件转成 CRLF，锁校验失败。
- 顶层构建在 MinGW 上静态链接运行时（`-static`）。动态 `libstdc++-6.dll` 里的 `std::pmr` 等
  内部分配不经过测试替换的 `operator new`，分配失败注入类测试会测不到。这只影响本仓库自己的
  测试和工具，不进入导出的接口。
- 签名导出工具以二进制方式写文件，提交的签名基线在所有主机上都是 LF。

## 历史记录：GitHub CI（2026-10-21，wire 格式 2.0）

CI 停用前 `.github/workflows/client-targets.yml` 的结果，格式是 2.0，测试数也比现在少；只作历史参考。

| 目标 | 编译器 | 验证内容 | 结果 |
| --- | --- | --- | --- |
| Linux x86-64（参照） | GCC 16（gcc 镜像按 digest 固定） | 写出参照 wire 语料 | 作为其他目标的比对基准 |
| Linux arm64（GitHub `ubuntu-24.04-arm` 原生） | GCC 16（arm64 平台 manifest digest 固定） | Debug 全量 CTest；读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过，wire 逐字节一致 |
| Windows x64（`windows-latest` + MSYS2 UCRT64） | MinGW-w64 GCC 16.2（要求 ≥ 16） | Debug 全量 CTest 87/87（Ninja）；Release 构建读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过；5 个语料（empty、small、medium_holes、reloaded_mutated、sparse）逐字节一致 |

## 3.1.0 不可行

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

**Windows x64，MSVC 或 clang-cl**：两者都没有反射，编不了完整库。MinGW 和 MSVC 的 C++ ABI
不同，用 MSVC 的引擎只能通过 C 接口使用 MinGW 编的 XOffset 模块。

客户端（Android、iOS、MSVC）的构建方式是 3.2 的工作，进展以本文后续版本为准；3.1.0 不包含它。

## 握手（3.2，未发布）

客户端与服务端的 schema 版本不同时，3.2 的服务端负责转换（[SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)）。
客户端只多一次握手，用 `xoffset_handshake.hpp`：连接建立后发
`make_schema_hello<...>()`，收到服务端的 ack 后用 `read_schema_ack<...>()` 读出结果，
被拒绝时提示更新。这两个函数不抛异常；关异常的客户端构建要等 SC-8 的
`try_load_verified` 合入。客户端不包含 `xoffset_evolution.hpp`，没有转换代码。

## 范围

只支持 64 位目标。32 位客户端、大端平台不在 3.1.0 范围内。
