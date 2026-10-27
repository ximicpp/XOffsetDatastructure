# 0.2.0-alpha.1 源码包、安装包与外部消费

本 alpha 面向能够固定实验性编译器、依赖与 schema 的试用者。核心功能已完成，
但 alpha 之间不承诺源码 API、ABI 或任意工具链/layout 的长期 wire 兼容。
先阅读 [兼容与输入边界](ALPHA_COMPATIBILITY.md)。本版本使用 V2 存储与 wire 格式，不读取 V1 wire；
save/load 仍复制字节，不是零复制接口。

## 三类交付物

| 交付物 | 内容 | 不包含的环境输入 |
| --- | --- | --- |
| `xoffsetdatastructure-<VERSION>.tar.gz` 源码包 | 两个生产头、CMake、tests/examples、必要工具与用户文档、固定的 851 个依赖头及许可证 | 编译器、libc++/系统 runtime、SDK、构建输出、实验目录与历史大型证据 |
| `xoffsetdatastructure-<VERSION>-full.tar.gz` 安装包 | 两个生产头、`XOffset::xoffset` 与 `XOffset::storage`、851 个私有依赖头、CMake 配置、版本/兼容文档与许可证 | 编译器、SDK、runtime、原生程序二进制 |
| `xoffsetdatastructure-<VERSION>-storage.tar.gz` 安装包 | OS 头 `xoffset_virtual_memory.hpp`、`XOffset::storage`、CMake 配置、版本/兼容文档与项目许可证 | 完整 XBuffer/容器头、Boost、TypeLayout、编译器、SDK、runtime |

源码包的依赖闭包可在没有 `.git` 的解压目录中通过内容锁检查。它不是完整 Boost
发行版，也不是包含全部历史原始证据的离线研究档案。源码归档中的
`VM_VALIDATION_RUNNER.md` 保留部分历史入口说明；本包包含的运行入口是 native
full/storage 及 package 验证，历史 wire fixture、日期专用 Windows/network runner
和实验工具不在发行范围内。

## Requirements

| 目标 | 最低要求 |
| --- | --- |
| `XOffset::storage` | C++17，CMake 3.13+，Linux/macOS/Windows VM API，线程支持 |
| `XOffset::xoffset` | P2996 Clang、配套 libc++、CMake 3.30+，64-bit little-endian |
| 维护的 Python 验证入口及其测试 | Python 3.10+；storage 的 JUnit 验证需要 CMake/CTest 3.21+，full 需要 3.30+；C++ 库没有 Python 运行依赖 |

完整目标传播 `cxx_std_26`、reflection、expansion statements、libc++ 与 Threads。
消费项目自行设置 C++17 不会覆盖完整目标的要求；普通 C++26 编译器也不能替代 P2996。
Windows 当前只对原生 storage 宣称已验证，完整 P2996 不在此 alpha 的已验证支持范围。

## Fixed inputs

[toolchains/README.md](../toolchains/README.md) 和
[vm-validation.json](../toolchains/vm-validation.json) 记录固定镜像、编译器来源、
依赖和官方 CMake 4.2.3 archive 的 SHA-256。历史 Linux Docker 输入为 immutable
registry digest，编译器 revision 是 `a5270822dede40aaaf2c62381b89f110d2cfda4c`；
新 WSL 接续编译器是 `9ffb96e3ce362289008e14ad2a79a249f58aa90a`，二者不是同一 profile。
Dockerfile 是固定编译器源码的构建配方，但其 Ubuntu base tag/apt 输入仍可变，
不能将重建镜像说成已验证 registry digest 的字节副本。

源码包已包含固定依赖，不需要下载子模块。Git 检出则使用父 gitlink，保持 LF 字节：

```sh
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost external/typelayout
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
```

Boost 固定于 `da041154c6bac1a4aa98254a7d6819059e8ac0b0`，TypeLayout 固定于
`fe295929d0a5bfa7809fb737a6eb3db279599346`。完整配置按
`cmake/XOffsetDependencyLock.cmake` 核对 11 个 include tree 与 3 个许可证的内容，
不依赖 Git 元数据。不要使用 `--remote` 升级依赖，也不要改锁来接受 CRLF 或其他差异。
已有检出的字节不匹配时，先保护修改并诊断，重复 update 不代表已修复。

准备固定 CMake 的例子（输出必须为新目录）：

```sh
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-alpha
```

该工具下载并核对锁定 archive，也可用 `--archive` 指向已下载文件；CMake 位于
`build/cmake-alpha/root/bin`。固定 Linux 镜像自带的 CMake 3.28.3 不足以构建 full。
例如在 Linux shell 中以固定镜像运行 full 检查：

```sh
XOFFSET_IMAGE=$(python3 -c 'import json; print(json.load(open("toolchains/vm-validation.json"))["profiles"]["linux-p2996"]["image"])')
docker run --rm --platform linux/amd64 -v "$PWD":/workspace -w /workspace \
  -e PATH=/workspace/build/cmake-alpha/root/bin:/opt/p2996-toolchain/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  "$XOFFSET_IMAGE" bash ./build.sh --compiler /opt/p2996-toolchain/bin/clang++ \
  --build-dir build/docker-alpha-full -j 2
```

`tools/prepare_vm_linux_current.py --compiler-root /opt/p2996-toolchain --out <新目录>`
是另一条恢复已存在 `9ffb96e...` 编译器的本机配方：校验编译器并准备固定的私有
runtime/link wrapper；它不下载该编译器，也不将宿主系统头冒充完整 sysroot。
选择该路径后，以输出的 `bin/clang++` 显式构建；不要用历史 `linux-p2996` 身份
检查把新 revision 重标为旧镜像。macOS 的固定 compiler/SDK 输入也单独列于 toolchain 锁。

SDK、标准库 include/library 和运行时搜索路径由调用方 toolchain 文件或显式参数提供。
本机便利检测路径不会写进导出目标。不要把实验性 libc++ 全局加入系统工具加载路径。

## 创建、审计与解压归档

在已经准备好固定依赖的源码树执行（下文 `<VERSION>` 是 `XOFFSET_VERSION` 的内容，`prepare_release.py` 从同一源码树的这个文件读版本）：

```sh
python3 tools/prepare_release.py create --source-dir . --out build/release-source
python3 tools/prepare_release.py audit \
  --archive build/release-source/xoffsetdatastructure-<VERSION>.tar.gz \
  --manifest build/release-source/xoffsetdatastructure-<VERSION>.manifest.json \
  --checksums build/release-source/SHA256SUMS
python3 tools/prepare_release.py extract \
  --archive build/release-source/xoffsetdatastructure-<VERSION>.tar.gz \
  --out build/release-source-extracted
```

创建输出含 archive、同名 `.manifest.json` 及 `SHA256SUMS`；归档根目录为
`xoffsetdatastructure-<VERSION>/`，内含 `SOURCE_MANIFEST.json`。
`audit` 可不传外部 manifest/checksums，但对外分发时应另行固定 archive/manifest 身份。
`extract` 先审计再提取，源码位于
`build/release-source-extracted/xoffsetdatastructure-<VERSION>/`。
完整性哈希不是签名或来源认证。

已验证的安装 prefix 可分别包装为 full/storage 归档：

```sh
python3 tools/prepare_release.py pack-prefix --prefix /path/to/verified-full-prefix \
  --component full --out build/release-full
python3 tools/prepare_release.py pack-prefix --prefix /path/to/verified-storage-prefix \
  --component storage --out build/release-storage
python3 tools/prepare_release.py extract \
  --archive build/release-full/xoffsetdatastructure-<VERSION>-full.tar.gz \
  --out build/release-full-extracted
```

安装归档同样带外部 `.manifest.json`/`SHA256SUMS`，内部为 `INSTALL_MANIFEST.json`。
full 解压后 prefix 为 `build/release-full-extracted/xoffsetdatastructure-<VERSION>-full`，
将它传给消费项目的 `CMAKE_PREFIX_PATH`；storage 的根名以 `-storage` 结尾。
`audit`/`extract` 同时支持三类 archive。包装/哈希通过不能替代实际解压后的消费构建。

所有输出使用新目录；source/out 不接受 symlink。create 输出不能等于或包围源码，
也不能位于待复制的 cmake/tests/tools/examples/include/external/docs/toolchains 内；
`build/release-source` 这类独立目录可用。工具不会覆盖已有产物或触发 CI/发布。

## 构建和安装

在源码根目录构建 full：

```sh
cmake -S . -B build/alpha-package \
  -DCMAKE_CXX_COMPILER=/path/to/p2996/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX=/path/to/xoffset-prefix \
  -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/alpha-package --config Release
cmake --install build/alpha-package --config Release
```

storage-only 加 `-DXOFFSET_STORAGE_ONLY=ON`，使用普通平台编译器，跳过完整头、依赖和
P2996 检查。full 安装同时提供 full/storage，storage 安装请求 full 会明确失败。
源树顶层默认构建 tests/tools/examples；作为 `add_subdirectory` 依赖时，以下四项
默认关闭，可分别显式开启：`XOFFSET_BUILD_TESTS`、`XOFFSET_BUILD_TOOLS`、
`XOFFSET_BUILD_EXAMPLES`、`XOFFSET_INSTALL`。完整配置始终执行反射 compile/link 探测。

## 安装与源码消费

full 安装消费的最小 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.30)
project(MyConsumer LANGUAGES CXX)
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
if(NOT XOffsetDatastructure_VERSION STREQUAL "0.2.0-alpha.1")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 0.2.0-alpha.1")
endif()
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

```sh
cmake -S /path/to/consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/path/to/xoffset-prefix \
  -DCMAKE_CXX_COMPILER=/path/to/p2996/bin/clang++
cmake --build build/consumer --config Release
```

storage 使用 `COMPONENTS storage`、`XOffset::storage` 和 CMake 3.13+。未指定组件默认
请求 full。安装配置暴露 `XOffsetDatastructure_VERSION="0.2.0-alpha.1"`、
`XOffsetDatastructure_VERSION_NUMERIC="0.2.0"`、
`XOffsetDatastructure_VERSION_PRERELEASE="alpha.1"`。
**任何数字、EXACT 或范围版本请求均保守拒绝**，避免 CMake 把 alpha 当作稳定版本。
不要写 `find_package(XOffsetDatastructure 0.2.0 ...)`，也不要把带 alpha 的字符串放在
CMake 数字版本实参中；采用无版本查找再核对完整字符串。

源码消费：

```cmake
add_subdirectory(path/to/XOffsetDatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

需要 storage 时，在 `add_subdirectory` 前设置 `XOFFSET_STORAGE_ONLY=ON` 并改链
`XOffset::storage`。根 `project()` 的版本为数字 `0.2.0`；源码树的完整 prerelease
变量是 `XOFFSET_VERSION_FULL`，不要把根 project 数字变量当成安装配置的完整版本。

## 安装内容和许可证

full 的私有 include 根为 `include/xoffset-deps/`，包含 TypeLayout 的 19 个 include
文件和 Boost container/core/config/throw_exception/static_assert/container_hash/
type_traits/winapi/intrusive/predef，共 851 个依赖头，保留原版权标注。

- `share/XOffsetDatastructure/xoffset-third-party-manifest.txt` 记录来源版本和逐文件 SHA-256。
- 两种安装都包含 `share/XOffsetDatastructure/VERSION`、`CHANGELOG.md` 和
  `ALPHA_COMPATIBILITY.md`。
- `share/XOffsetDatastructure/licenses/` 包含项目 MIT 许可证；full 另含 Boost
  `LICENSE_1_0.txt`、TypeLayout `LICENSE` 与 `LICENSE_1_0.txt`。
- include/lib/share 子目录必须位于 prefix 内；导出目标只使用可重定位安装路径。

## 消费验收与平台边界

```sh
python3 tools/validate_vm_package.py --suite storage --out build/alpha-package-storage
python3 tools/validate_vm_package.py --suite full --out build/alpha-package-full \
  --compiler /path/to/p2996/bin/clang++ --library-dir /path/to/matching/libcxx
```

Windows 从 x64 MSVC 开发者终端用 `python` 执行第一条命令即可；此处不依赖日期专用
Windows runner。可指定 `--source-dir`、`--sdk`、`--generator`、`--jobs`。
使用新输出目录，避免放入验证器待复制的源码目录范围。工具不下载依赖。

验证器固定源码副本、构建安装、核对精确包内容/许可证/版本策略，然后移动 prefix，
使原生产源码路径不可用，运行 installed 3 项和源码 subproject 3 项多翻译单元消费。
覆盖 owner 转移、池归还/重用、live owner 下 clear、handle 先销毁及跨翻译单元静态析构，
并核对源码/安装包前后不变。它不代替 full、sanitizer、TSan 或 wire 验证。
共享库、动态插件、跨 CRT、混合工具链或混合头版本未获此验收。

以下为 0.1.0-alpha.1（V1 格式）时期的已记录基线，不代表 V2；V2 的验证记录见 README 的 Validation scope 与 CI。归档自身的验收另行报告，不从基线自动继承：

| 环境/配置 | 已记录结果 | 口径 |
| --- | --- | --- |
| Windows x64 MSVC 19.44 | storage 2/2；installed 3/3、subproject 3/3 | 2026-10-16 原生运行；full P2996 未支持/未验 |
| WSL2 x86_64 GCC 11.4 | storage 2/2；installed 3/3、subproject 3/3 | 当前同架构虚拟机功能证据 |
| WSL2 P2996，初始恢复 profile | full 48/48，另 3 个 configure 检查 | `linux-current-20261016` |
| WSL2 P2996，最终完整 profile | full installed 3/3、subproject 3/3；网络 ASan/UBSan 34 组/182 子场景；2 个受控缺陷检出 | `linux-current-complete-v2-20261016`；不是最终 profile 重跑全部 full 48 项 |
| 历史 macOS arm64，固定 P2996/AppleClang storage | full 48/48；full 与 storage 各 installed 3/3、subproject 3/3 | 以前记录，非本机新跑 |
| 历史 Linux x86_64 固定 Docker 镜像 | full 安装与源码消费各 3 项 | 在 arm64 宿主模拟执行，仅功能证据 |

`docs/VM_FINAL_DELIVERY.md` 保留上述证据与身份（Windows/Linux 当期结果文件已不在仓库中），
精简源码包不含这些研究记录。
WSL 或 Docker 可用于后续 Linux 性能工作，不要求独立物理机作为当前准入条件；
仍须注明虚拟化类别并先取得噪声资格。本次 WSL 唯一 78 进程 F/F 校准的四指标均未通过
局部 anchor 和 drift 门槛，未建立 2% 比较能力，没有 D/F、ablation 或 adoption 结论。
背景负载/平衡电源观测不构成因果归因，历史原记录不因此改写。

本 alpha 不承诺 N7 三项 I/O 扩展、生产异步 transport 或业务网络整合。当前已验
网络范围是同步 TCP 回环参考与独立生命周期模型；缺少真实业务前提仍保留 F。
