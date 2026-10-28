# 客户端目标可行性（K8）

最核心的场景是服务端之间、服务端到客户端：服务端 Linux，客户端 Windows / iOS / Android，
都是 64 位小端，不依赖具体的客户端引擎。服务端与客户端必须用同一 schema、同时发布
（见 [DEPLOYMENT.md](DEPLOYMENT.md) 第 2 节）。库有两条构建路径，wire 逐字节相同：

- **C++26**：P2996 反射描述每个记录，需要 clang-p2996 或带反射的 GCC。服务端走这条路。
- **C++20 / C++23**：每个记录写一行 `XOFFSET_RECORD` 字段表（`-DXOFFSET_CXX_STANDARD=20|23`），
  不需要反射，原生客户端编译器都能用。客户端走这条路；同一行字段表在服务端的 C++26 构建里
  会和反射逐项核对。

K8（客户端与服务端同一套库、同一 wire）是目标，尚未承诺，实现中：字段表后端已合入（#149，3.2.0 起），
下表 Android、Windows、macOS 和 iOS 模拟器各工具链已通过；3.2.0-rc.1 在 Android 真机、Windows 和 macOS 实机上
也已通过（下一节），iOS 真机待测。三个客户端目标都原生构建、CTest 通过（iOS 含
地址空间实测并定下最低部署目标）、跨目标 wire 语料一致、签名与 `tools/sigs/` 基线一致之后，才升为
核心承诺（[USER_GUIDE.md](USER_GUIDE.md) 第 8 节，测试对照见 [STABILITY_TESTS.md](STABILITY_TESTS.md)）。

## C++20 / C++23 字段表构建

运行入口：`tools/native_targets.sh TARGET --std 20|23 --ref <参照语料>`（工具链文件在
`cmake/toolchains/`）。每个目标都跑 Release 全量 CTest，然后做 wire 语料检查：写出本机语料，
读入参照 wire（必须全部接受，重存逐字节相同），把参照 wire 按固定步骤改过后再存（`save_verified`
和 `save_compact` 各一份），必须与参照端改出的字节相同，最后与参照语料 `diff -r`。参照语料由 Linux
x86-64 GCC 16 C++26（反射）写出：8 个用例（elided、empty、medium_holes、reloaded_mutated、small、
sparse，以及 `save_compact` 存出的 compact_slack0、compact_slack8），每个再带改过的两份
（`<用例>.edit.xwire`、`<用例>.edit-compact.xwire`），共 24 个文件。3.2.0-rc.1 及以前只有前 6 个。

防止回退：`tools/local_validate.sh` 带这些列（`msvc-c++20`、`msvc-c++23`、`clang-cl-c++20`、
`mingw-c++20`、`ndk29-c++20`、`ndk30-c++20`），参照语料由同一提交的 GCC 16 C++26 构建现写。
CTest 里包括关异常的 `*_noexc` 程序。默认运行里的 `client-c++20` 步骤保证每个 PR 至少跑一个
C++20 客户端列：按 MSVC、clang-cl、NDK r29、NDK r30、MinGW 的顺序取本机能跑的第一个；一个都跑不了
时这一步失败。`--clients` 和 `--release` 跑全部客户端列。云端新容器装 MSVC 用
`tools/msvc-wine/fetch_msvc.py /opt/msvc`（再跑 `tools/msvc-wine/case_links.py /opt/msvc` 给 clang-cl 用）。

| 目标 | 编译器 | 标准 | 实际运行方式 | 结果 |
| --- | --- | --- | --- | --- |
| Android arm64-v8a | NDK r29 Clang 21.0.0，API 28，`c++_static` | C++20 | 全静态链接，云端 x86-64 上用 `qemu-aarch64-static` 跑 CTest | 129/129（含 21 个关异常测试）；语料全部接受，逐字节一致 |
| Android arm64-v8a | NDK r30 Clang 21.0.0 | C++20 | 同上 | 129/129（含 21 个关异常测试）；语料全部接受，逐字节一致 |
| Windows x64 | MSVC 19.44（VS 2022 17.14 工具集 14.44.35207，Windows SDK 10.0.26100），`/MT` | C++20、C++23 | 云端用 wine 9.0 运行 cl/link（`cmake/toolchains/msvc-wine-x86_64.cmake`）和 CTest | 两种标准都是 129/129（含 21 个关异常测试）；语料全部接受，逐字节一致 |
| Windows x64 | clang-cl 19 + 同一套 MSVC STL 和 SDK，`/MT` | C++20 | `cmake/toolchains/clang-cl-x86_64.cmake`，CTest 经 wine | 129/129（含 21 个关异常测试）；语料全部接受，逐字节一致 |
| Windows x64 | MinGW-w64 GCC 13.2（posix 线程），`-static` | C++20 | CTest 经 wine | 129/129（含 21 个关异常测试）；语料全部接受，逐字节一致 |
| macOS arm64 | Apple clang 21.0.0（Xcode 27.1，MacOSX 27.0 SDK，系统 libc++），部署目标 macOS 14.0 | C++20、C++23 | M4 Mac（macOS 27.2）上原生运行，`tools/native_targets.sh macos` | 提交 1108193d：两种标准都是 137/137（含 21 个关异常测试）；24 个参照 wire 全部接受，改出的字节与 Linux 相同，`diff -r` 一致 |
| iOS 模拟器 | Apple clang 21.0.0（Xcode 27.1，iPhoneSimulator 27.1 SDK），部署目标 iOS 17.0 | C++20 | M4 Mac 上的 iOS 27.0 模拟器（iPhone 17），`ios-simulator.cmake`，CTest 经 `simctl spawn` | 129/129；语料全部接受，逐字节一致。模拟器进程用的是 Mac 的地址空间，真机上的地址空间实测仍待做 |
| Linux x86-64 | GCC 13 / 16、Clang 18、P2996 Clang（各自 C++20，GCC 16 另有 C++23） | C++20、C++23 | `tools/local_validate.sh` 的矩阵列，含 ASan 列 | 都是 106/106；字段表构建与反射构建的校验结果逐条相同（差分 2×200000 例） |

各目标需要注意的地方：

- **Android**：测试里的分配失败注入用 `aligned_alloc`，bionic 从 API 28 起才有，所以测试构建
  默认 API 28；库本身不依赖它。没有 KVM 时只能跑全静态的 bionic 程序，工具链文件在
  `XOFFSET_ANDROID_QEMU=ON` 时加 `-static`。bionic 的 `__cxa_thread_atexit_impl` 用
  `operator new` 分配线程析构节点（glibc 用 calloc），所以线程第一次使用 thread_local 缓存时，
  替换了 `operator new` 的测试会多看到一次分配。
- **Windows（MSVC、clang-cl）**：
  - `XOFFSET_RECORD` 需要符合标准的预处理器 `/Zc:preprocessor`。CMake 接口目标会自动加上；
    不用 CMake 时，传统预处理器会在编译期直接报错说明。
  - 库在包含 `<windows.h>` 时临时定义 `WIN32_LEAN_AND_MEAN` 和 `NOMINMAX`（只在用户没定义时），
    所以 `rpcndr.h` 的 `small` 宏不会进来。`near`、`far` 仍是宏，不要用作标识符。
  - MSVC 的 ABI 把 `alignas` 限制在 8192 以内，单个类型不能超过 0x7fffffff 字节。
  - `[[no_unique_address]]`：MSVC 忽略它。GCC/Clang 上它让成员重叠时 `XOFFSET_RECORD` 直接拒绝
    （成员不能重叠）；只用反射的服务端会接受并打出 8 字节的 pin，MSVC 客户端照这个 pin 写的
    `XOFFSET_PIN_SCHEMA` 会编译失败（MSVC 布局是 12 字节）。共享类型不要用它。
  - `std::string::resize_and_overwrite` 在 MSVC 上只在 `/std:c++latest`（CMake 的 C++23）下可用，
    C++20 下库走普通的 `resize` 路径。
- **Windows（MinGW GCC）**：GCC 在 MinGW 上用模拟 TLS（emutls），库里的每线程缓存在线程退出时
  会出错，所以 GCC + MinGW/Cygwin 下默认关闭这些缓存（`XOFFSET_THREAD_CACHES=0`）。
- **macOS / iOS（std::pmr）**：库用系统 libc++ 的 `std::pmr`。系统 libc++ 从 macOS 14 / iOS 17 起
  才有这些符号，而 libc++ 头文件没有给 pmr 加可用性标注，部署目标更低时照样能编译、链接，到旧
  系统上加载时才失败。所以 C++20/23 构建的默认部署目标是 macOS 14，更低的部署目标在编译期报错。
  NDK、MinGW 和 MSVC 都把标准库静态链进程序，没有这个系统版本门槛。

## 真机与实机：3.2.0-rc.1（39de636，2026-10-28）

测试方在自己的机器上拉取 `v3.2.0-rc.1`，用 `tools/native_targets.sh`、`tools/platform_matrix.sh` 和
`tools/device/android_device.sh` 跑出下表；参照语料是同一提交在 WSL2 Docker 里由 GCC 16.2 C++26 反射写出
的 6 个 wire。结果只属于提交 39de636。核验、未通过的记录和证据索引见
[validation/2026-10-28-rc1-platform/README.md](validation/2026-10-28-rc1-platform/README.md)。

| 目标 | 编译器 | 标准 | 实际运行方式 | 结果 |
| --- | --- | --- | --- | --- |
| Android arm64-v8a 真机（Xiaomi M2102K1C，Android 11） | NDK r29 Clang 21，API 28 | C++20 | 手机上 adb 运行 CTest，编译失败测试在电脑上 | 114/114 + 15/15；语料全部接受，逐字节一致；demo 两个客户端 E2E PASSED |
| Windows x64 实机（Windows 11） | MSVC 19.44.35229，`/MT`，`/utf-8` | C++20、C++23 | 原生 | 两种标准都是 129/129；语料全部接受，逐字节一致 |
| macOS arm64 实机（SDK 26.4） | Apple clang 21.0.0 | C++20、C++23 | 原生 | 两种标准都是 129/129；语料全部接受，逐字节一致 |
| macOS arm64 实机（同一台） | clang-p2996 9ffb96e3 | C++26 反射 | 原生 | 158/158；语料逐字节一致 |
| Linux x86-64 远端虚拟机（KVM） | Zig 提供的 Clang 20.1.2 | C++20 | 原生 | 129/129；语料全部接受，逐字节一致 |
| Linux x86-64，WSL2 Docker | GCC 16.2.0 | C++26 反射 | 写参照语料，也是 Android demo 的服务端 | 158/158 |

129 和 158 是两种后端各自的全量清单，两者互不包含，不能相加。Windows 的第一次构建在代码页 936 下因测试源码
里的非 ASCII 字符失败，加 `/utf-8` 后通过（之后项目对 MSVC 自动加 `/utf-8`）。本轮没有性能数据，没有
iPhone，没有裸机 Linux；Windows 和 macOS 只跑了库和语料，没有跨设备 demo。

## C++26 反射构建：本地平台矩阵（3.1.0 起的记录）

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

NDK Clang 没有 P2996 反射，NDK 自 r18 起也不再提供 GCC；P2996 工具链只有 macOS 版 libc++ 和
`*_osx` compiler-rt，没有 iOS / iOS 模拟器版；MSVC 和 clang-cl 也没有反射。所以 Android、iOS 和
MSVC 客户端都走上面的 C++20 / C++23 字段表构建。

## 历史记录：GitHub CI（2026-10-21，wire 格式 2.0）

CI 停用前 `.github/workflows/client-targets.yml` 的结果，格式是 2.0，测试数也比现在少；只作历史参考。

| 目标 | 编译器 | 验证内容 | 结果 |
| --- | --- | --- | --- |
| Linux x86-64（参照） | GCC 16（gcc 镜像按 digest 固定） | 写出参照 wire 语料 | 作为其他目标的比对基准 |
| Linux arm64（GitHub `ubuntu-24.04-arm` 原生） | GCC 16（arm64 平台 manifest digest 固定） | Debug 全量 CTest；读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过，wire 逐字节一致 |
| Windows x64（`windows-latest` + MSYS2 UCRT64） | MinGW-w64 GCC 16.2（要求 ≥ 16） | Debug 全量 CTest 87/87（Ninja）；Release 构建读入 x86-64 语料并全部接受；写出本机语料与 x86-64 语料 `diff -r` | 全量通过；5 个语料（empty、small、medium_holes、reloaded_mutated、sparse）逐字节一致 |

## 握手（3.2.0 起）

客户端与服务端的 schema 版本不同时，3.2 的服务端负责转换（[SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)）。
客户端只多一次握手，用 `xoffset_handshake.hpp`：连接建立后发
`make_schema_hello<...>()`，收到服务端的 ack 后用 `read_schema_ack<...>()` 读出结果，
被拒绝时提示更新。这两个函数不抛异常；关异常的构建里握手消息经 `try_load_verified` 装载
（下一节）。客户端不包含 `xoffset_evolution.hpp`，没有转换代码。

## 关异常构建（SC-8）

不开异常的引擎（`-fno-exceptions`，MSVC `/EHs-c- /D_HAS_EXCEPTIONS=0`）可以直接包含 XOffset，
接收时用 `try_load_verified` / `try_reload_verified` / `try_begin_verified_reload` 拿错误码和错误
文字，其他不可返回的失败走 `set_fatal_handler`（`docs/USER_GUIDE.md` 第 6.2 节）。CTest 里
`*_noexc` 的 24 个程序和 `test_vm_storage_noexc`（C++17 OS 层）都按关异常编译；
`tools/no_exceptions_differential.sh` 比较开、关异常两种构建对同一批变异 wire 的结果。
C++26 反射构建（P2996、GCC 16）和 C++20 字段表构建（GCC 16、P2996 Clang）都在云端 Linux x86-64
上跑过这些测试；其他客户端目标的关异常构建随各自平台列验证。

## 范围

只支持 64 位小端目标。客户端目标是 Windows x64、Android arm64-v8a、iOS arm64（真机和模拟器）和 macOS arm64，
服务端是 Linux x86-64 和 arm64。32 位客户端、大端平台不在范围内。

## 手机上运行：测试宿主（3.2）

iOS 不能运行命令行程序，Android 真机也不方便逐个推送测试。`-DXOFFSET_TEST_HOST=ON` 把每个测试、演示客户端和
测量程序（`xo_slg_bench` SC-6、`xo_mem_probe` SC-12）各编成一个共享库（`cmake/XOffsetTestHost.cmake`），
由一个宿主程序加载运行：iOS 是 App（`tools/test_host/host_ios.mm`，库在 App 的
`Frameworks/` 里），其他平台是命令行宿主。默认每次启动只运行一个程序。`XO_HOST_SERVE=1` 时宿主常驻（`xoserve`）：只启动一次，电脑把程序一个个送进去，
在同一个进程里依次运行；App 界面显示累计数据（收到的任务包和 demo 快照字节数、运行和通过个数、最近几个程序），
slg 计时和内存探测期间界面不刷新，这两类程序各在一个新启动的 App 里运行。iPhone 上常驻模式还不能跑完整套测试：
每个加载过的程序库都留着一个 64 MiB 的表，约 50 个就占满进程地址空间（2026-10-28 第 6 次真机运行）。程序的 `exit`/`_Exit`、信号和输出都由宿主接住，结果写进宿主的
数据目录（iOS 是 App 的 `Documents/xo`），电脑上的 `tools/test_host/xo_host_run.py` 负责安装、启动、
传文件和取回结果：

| backend | 目标 | 用的工具 |
| --- | --- | --- |
| `local` | Linux / macOS 上检查同一套流程 | 直接启动命令行宿主 |
| `sim` | iOS 模拟器 | `xcrun simctl` |
| `device` | iPhone（USB 或同一 Wi-Fi） | `xcrun devicectl`；签名用 `tools/test_host/sign_ios_app.sh` |

`xo_host_run.py` 可以当 CTest 的 `CMAKE_CROSSCOMPILING_EMULATOR`（`ios-simulator.cmake` 在
`XOFFSET_TEST_HOST=ON` 时就这样设，`ios-device.cmake` 用 `XO_HOST_DEVICE`），也可以给
`demo/mapstate/tools/run_e2e.sh` 当客户端包装命令。它把输入文件和输出目录在电脑和设备之间来回拷贝，
程序看到的路径和在电脑上一样；退出码是程序自己的，被信号杀掉时是 128+信号。程序替换的 `operator new` / `delete`
（分配计数、失败注入）经宿主转发，C++ 运行库内部的分配也会走到它们，和普通可执行文件一样
（`test_host_new_forwarding` 检查这一点）。
云端 Linux 上用 `local` backend 检查过：129/129 个 CTest 通过，演示端到端（自检 23/23、语料回放 60/60、
三档压力和恶意输入）全部通过。
