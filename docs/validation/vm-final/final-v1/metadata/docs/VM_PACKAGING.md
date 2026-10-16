# 实验性 CMake 安装包与外部消费

本阶段提供两个可重定位的接口目标，不修改两个实现头、运行时策略或 V1 wire 格式。
完整库仍依赖实验性 P2996 Clang 与配套 libc++；安装成功不表示任意 C++26 编译器可用。

| 目标 | 内容 | 构建要求 |
|---|---|---|
| `XOffset::storage` | 独立 OS 映射、页面回收及池，头文件 `xoffset_virtual_memory.hpp` | C++17，CMake 3.13+，平台线程支持 |
| `XOffset::xoffset` | 完整 XBuffer/容器，两个公开头和固定依赖 | P2996 Clang、配套 libc++，CMake **3.30+** |

完整目标通过 `cxx_std_26` 传播最低语言要求，因此消费项目设置 `CMAKE_CXX_STANDARD=17`
也不会覆盖它。CMake 的 `CXX_STANDARD=26` 属性于 3.25 引入，但可传递的
[`cxx_std_26` 特性于 3.30 引入](https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html)，
两者不能混淆。目标另外传播 reflection、expansion statements、libc++ 和 Threads。
最终语言开关可能随消费项目的 `CXX_EXTENSIONS` 设置使用 `c++26` 或 `gnu++26`。

## 构建和安装

完整库示例：

```sh
cmake -S . -B build/package \
  -DCMAKE_CXX_COMPILER=/path/to/p2996/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX=/path/to/install \
  -DXOFFSET_BUILD_TESTS=OFF \
  -DXOFFSET_BUILD_TOOLS=OFF \
  -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/package --config Release
cmake --install build/package --config Release
```

SDK、标准库 include/library 路径及运行时搜索路径应由调用方 toolchain 文件或显式参数配置。
源码构建仍保留编译器安装目录的便利检测；这些本机目录不进入导出目标。
若编译器不能自行找到匹配的 libc++，调用方需提供相应 include、链接及运行时路径。
不要将实验性 libc++ 全局注入系统工具的加载路径；它可能使 CMake 等宿主工具无法启动。

只需 OS 层时加 `-DXOFFSET_STORAGE_ONLY=ON`，可使用平台普通编译器，不读取 Boost、
TypeLayout 或完整 XBuffer 头。此包只安装 OS 头、`XOffset::storage`、项目许可证及 CMake 配置；
请求完整组件会明确失败。完整包安装两个头，且同时提供两个目标。

默认顶层构建保留 tests/tools/examples；作为 `add_subdirectory` 依赖时，以下四项默认关闭：

- `XOFFSET_BUILD_TESTS`
- `XOFFSET_BUILD_TOOLS`
- `XOFFSET_BUILD_EXAMPLES`
- `XOFFSET_INSTALL`

它们可分别显式开启。`XOFFSET_BUILD_TESTS=OFF` 时，tools 即使开启也不注册测试。
`XOFFSET_BUILD_TOOLS=OFF` 的完整测试配置仍保留 36 个测试程序和 7 个子进程场景；
默认完整配置另有 3 个工具检查，共 46 项。配置时的反射能力检查始终执行；
多态正例和拒绝检查随测试配置执行。storage-only 不构建完整库 tools/examples。

`build.sh` 是完整开发验证入口，会显式恢复 full/tests/tools/examples；即使复用曾关闭这些
选项的构建目录，也必须运行完整测试与签名检查。需要精简消费目标时使用上面的 CMake 入口。

## 消费方式

安装消费：

```cmake
cmake_minimum_required(VERSION 3.30)
project(MyConsumer LANGUAGES CXX)
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

通过 `CMAKE_PREFIX_PATH` 或 `XOffsetDatastructure_DIR` 指向安装目录即可。
只消费 OS 层时使用 `COMPONENTS storage` 和 `XOffset::storage`，最低 CMake 为 3.13。
未指定组件时默认请求 full；没有虚构项目版本，也未生成版本兼容承诺文件。

源码消费：

```cmake
add_subdirectory(path/to/XOffsetDatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

子项目路径以 XOffset 自身为基准，签名工具不再依赖外部主项目的 `CMAKE_SOURCE_DIR`。
完整源码消费同样需要已准备的固定依赖。只消费 storage 时可在加入子目录前设置
`XOFFSET_STORAGE_ONLY=ON`。

## 依赖和许可证

完整包将 851 个依赖头安装到私有 include 根 `include/xoffset-deps/`，保留原有版权标注。
这包括 TypeLayout 的全部 19 个 include 文件，以及当前构建实际开放的十个 Boost 模块
（container、core、config、throw_exception、static_assert、container_hash、type_traits、
winapi、intrusive、predef）。它不是完整 Boost 发行包。

- Boost 源版本：`da041154c6bac1a4aa98254a7d6819059e8ac0b0`。
- TypeLayout 源版本：`9a6de1d73617631a9612ba9f572d6c0844a90b19`。
- `cmake/XOffsetDependencyLock.cmake` 固定各 include 树和许可证的内容哈希。
  配置阶段核对字节，不依赖 `.git` 存在，因此相同内容的源码归档可用；缺失或被修改的
  依赖不能悄悄成为另一个实验版本。哈希锁是受维护的源码，不由构建过程自动刷新。
- `share/XOffsetDatastructure/xoffset-third-party-manifest.txt` 记录版本来源和逐文件 SHA-256。
- `share/XOffsetDatastructure/licenses/` 包含项目 MIT 许可证、Boost 的
  `LICENSE_1_0.txt`、TypeLayout 的 `LICENSE` 与 `LICENSE_1_0.txt`。
- include/lib/share 安装子目录必须位于 prefix 内；导出目标只引用可重定位的安装路径。

## 可复现验收入口

维护入口使用 Python 3.9+；因为要生成 CTest JUnit 证据，storage 验证命令需要 CMake 3.21+，
full 验证需要 CMake 3.30+。这与 storage 接口目标自身的 CMake 3.13 下限不同。

```sh
python3 tools/validate_vm_package.py --suite storage --out build/package-storage
python3 tools/validate_vm_package.py --suite full --out build/package-full \
  --compiler /path/to/p2996/bin/clang++ \
  --library-dir /path/to/matching/libcxx
```

可指定 `--source-dir`、`--sdk`、`--generator` 和 `--jobs`。省略 compiler/generator 时使用平台默认值，
storage 可从 Windows 开发者终端选择原生 MSVC。输出必须是新目录，且不能放在待复制的源码子树中。
入口不下载依赖，也不覆盖历史证据。它会：

1. 固定源码副本并关闭开发目标，构建和安装独立包。
2. 核对头文件、许可证、依赖清单和导出内容，拒绝本机绝对目录泄漏或额外文件。
3. 移动安装 prefix，并使原生产源码路径失效，再从新目录构建三翻译单元消费项目。
4. 运行跨翻译单元 owner 转移、分配、池归还/重用、live owner 下 clear、句柄先销毁、
   跨翻译单元静态析构三项测试；完整消费方故意设置 C++17，验证语言要求正确传递。
5. 从另一个构建目录检验源码 `add_subdirectory` 的默认目标范围，并运行同样三项测试。
6. 验证最终源码和安装包未变化；记录命令、文件哈希、JUnit 及 passed/failed 结果。

这些测试验证同一可执行文件中的多翻译单元共享状态。未验证共享库、动态插件、跨 CRT、
混用不同工具链或不同头版本的边界，不对这些组合宣称支持。

| 验证范围 | 状态与边界 |
|---|---|
| macOS arm64，普通 AppleClang，OS 安装包 | 已验证安装重定位与源码消费，各 3 项 |
| macOS arm64，固定 P2996 Clang，完整安装包 | 已验证安装重定位与源码消费，各 3 项 |
| Linux arm64，GCC，OS 安装包 | 原生容器验证安装重定位与源码消费，各 3 项；本地固定 image ID，非公开 registry 复现承诺 |
| Linux x86_64 固定 P2996 镜像 + 固定 CMake 4.2.3 | 安装重定位与源码消费各 3 项通过；在 arm64 主机模拟执行，仅作功能证据 |
| Windows 原生 OS 安装包 | 已提供可运行入口和 CI 配置；本机未取得原生运行证据 |
| Windows 完整库 | 未验证对应 P2996 工具链、安装消费和完整集成 |

全量、sanitizer、TSan 和 wire 验证仍由 [验证入口](VM_VALIDATION_RUNNER.md) 维护。
包装验证不能替代这些运行时检查，安装包也不改变 [已有策略收益和代价](VM_MEMORY_REUSE_RESULTS.md)。
本阶段完整验证结果及归档见 [第二阶段结果](VM_STAGE2_RESULTS.md)。
