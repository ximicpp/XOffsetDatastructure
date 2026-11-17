# XOffsetDatastructure

[English](README.md)

**4.0.0** · C++20 / C++26 · 只有头文件 · 64 位小端

XOffsetDatastructure 把 C++ 对象和其中的字符串、数组、集合、映射放在一块连续的、与地址无关的 arena 里。
arena 的字节就是消息：原地改数据，把字节复制到别的进程或机器，接收方直接读、接着改。没有序列化这一步，
也不用 IDL。

它面向游戏后端：Linux 服务端之间交接状态，服务端把状态推给 Windows、iOS、Android 客户端，两端包含同一份
C++ 头文件。

- [核心功能](#核心功能)
- [示例](#示例)
- [原理](#原理)
- [性能对比](#性能对比)
- [快速开始](#快速开始c20)
- [文档](#文档)

## 核心功能

| | 能做什么 |
| --- | --- |
| **不用序列化** | 对象建在 arena 里，彼此用相对偏移相连。发送是把 arena 复制一次，接收是检查加复制一次，没有逐字段的编码和解码。 |
| **原地修改** | 直接在 buffer 里读写字段，对容器追加、插入、删除，包括变长修改；释放的空间会复用。 |
| **收到后接着改** | 收到的 buffer 就是普通的工作 buffer，改完可以再发出去。空闲空间表随消息一起带过去，转几手都不退化。 |
| **任意 64 位小端设备** | 同一份字节在 Linux x86-64/arm64、Windows x64、macOS、iOS、Android 上都有效。用 `XOFFSET_PIN_SCHEMA` 把类型布局固定一次，构建它的每个编译器都会在编译期核对。 |
| **普通 C++ 类型** | schema 就是聚合体结构，容器用库里的 `XString`、`XVector`、`XSet`、`XMap`。C++20 每个类型写一行 `XOFFSET_RECORD`；C++26 反射（GCC 16 或 P2996 Clang）从类型本身拿字段，写了这一行就拿它核对。两种构建写出的字节相同。 |
| **增量同步** | 对方已有副本后只发改过的字节：客户端自己的状态用逐客户端增量，共享世界的视口用共享增量（每个变了的实体只编码一次，所有看得到它的客户端共用）。 |
| **按链路选检查档** | 接收器的类型就是它服务的链路。自己的服务端之间、服务端发往客户端用信任档，只查 wire 头和边界；客户端上行可以做边界检查（只读视图），要保留或修改时做完整校验。 |
| **客户端不需要异常** | 客户端用各平台自己的编译器（MSVC、Android NDK Clang、Apple clang）按 C++20 构建。客户端要用的接收和修改路径用 `XStatus` 或 `buffer.error()` 报错，`-fno-exceptions` 构建可用。 |

不做的事：不跨语言；不压缩（在外面套 zstd 之类）；检查只管结构，不是认证。链路经过不可信网络时配 TLS 或 mTLS。

## 示例

一份头文件，服务端和每个客户端构建都包含它：

```cpp
// game_protocol.hpp
#include "xoffsetdatastructure.hpp"
#include <cstdint>
using namespace XOffsetDatastructure;

struct Unit {
    std::uint32_t id;
    std::int32_t x, y, hp;
    XString name;
};
XOFFSET_RECORD(Unit, id, x, y, hp, name)      // 全局作用域，紧跟类型

struct WorldView {
    std::uint64_t tick;
    XVector<Unit> units;
};
XOFFSET_RECORD(WorldView, tick, units)
XOFFSET_REGISTER_SCHEMA_NAME(WorldView, "game.WorldView")   // 每个根类型一个稳定名字
```

服务端原地建、原地改，然后把 arena 的字节发出去：

```cpp
auto world = TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(1 << 20));

auto units = world.root().units();              // 代理：buffer 扩容搬家后仍然有效
auto u = units.emplace_back();
u.id() = 1;
u.hp() = 100;
u.name() = "Archer Captain";                    // 放在自己的块里；10 字节以内直接放在容器头里

world.edit([](WorldView& v) {                   // 热循环：给普通引用，速度和普通结构体一样
    v.tick += 1;
    for (Unit& unit : v.units) unit.y += 1;
});

std::string wire;                               // 每次发送复用
world.save_verified_into(wire);                 // 复制一次，不逐字段编码
send(wire);
```

客户端收进一个常驻的 buffer，读普通成员，也可以接着改、再发：

```cpp
#include "xoffset_receiver.hpp"

XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(1 << 20)};
auto held = TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(1 << 20));

if (XStatus s = rx.reload(held, wire); !s) {     // 不需要异常
    ask_for_whole_state();
    return;
}
held.read([](const WorldView& v) {
    for (const Unit& unit : v.units) draw(unit.id, unit.x, unit.y, unit.name.view());
});
held.root().tick() += 1;                        // 仍然是普通的、可修改的 buffer
```

能运行的版本：[`examples/helloworld.cpp`](examples/helloworld.cpp)，以及
[GETTING_STARTED.md](docs/GETTING_STARTED.md) 里每个场景一个示例：服务端发往客户端（`server_to_client.cpp`）、
增量（`delta_sync.cpp`、`shared_delta.cpp`）、客户端上行（`client_uplink.cpp`）、服务端之间（`server_to_server.cpp`）。

## 原理

```
 一块 arena（payload），放在任何地址都一样
 ┌────────┬───────────────────────┬──────────┬────────┬──────────────────┐
 │ header │ root WorldView        │ Unit[]   │ hole   │ "Archer Captain" │
 │ 48 B   │ tick │ units{Δ,n,cap} │ Unit ... │ (free) │                  │
 └────────┴───────────────┬───────┴─▲────────┴────────┴─▲────────────────┘
                          └─────────┘                   │
                     rel32 Δ from head to block         │
                          Unit.name{Δ,n,cap} ───────────┘

 wire = 40 B 头 + arena 已用部分 + 空洞表
```

1. **用相对偏移代替指针。** 每个容器头 12 字节：一个从容器头到数据块的 32 位有符号偏移、size、capacity。
   10 字节以内的短字符串直接放在容器头里。根对象用它到 arena 开头的偏移定位。payload 里没有任何绝对地址，
   所以这些字节在任何地址、任何进程、任何 64 位小端机器上含义都一样。
2. **arena 里自带分配器。** 容器从 arena 本身分配 8 字节对齐的块。释放的块变成空洞，buffer 把空洞记在空闲表里
   （小块按大小分箱，大块最佳适配）并复用。原地变长修改靠的就是它：vector 超出原来的块就换到新块，旧块留给
   下一次分配。
3. **wire 就是 arena。** `save_verified` 写一个 40 字节的 wire 头（格式版本、已用大小、根偏移、schema 指纹、
   根类型），然后是 arena 的已用部分和空洞表；大段空闲或大段零可以不上线。接收方把字节复制进自己的 buffer，
   把空洞表装回空闲表，接着用：两端各复制一次，没有编码和解码，空闲空间也跟过去了。
4. **schema 就是 C++ 类型。** 每个记录的字段表（`XOFFSET_RECORD` 写一次，或由 C++26 反射得出）决定布局签名和
   64 位指纹。wire 带着指纹，指纹不同的 wire 会被拒绝。`XOFFSET_PIN_SCHEMA` 把布局文本固定在共用的头文件里，
   某个客户端编译器排出的布局不一样就编译失败。含裸指针、引用或虚函数的类型在编译期就被拒绝。
5. **按链路检查。** 信任档查 wire 头和空洞表边界，不遍历对象图。边界档遍历容器头，保证任何读取都不越出这条
   消息，给出只读视图。完整档校验整张对象图（不重叠、恰好铺满、键有序、值合法），之后才允许修改。任何档都
   不检查内容：结构合法、字段值离谱的消息照样被接受，所以服务端仍要按自己的游戏规则核对客户端输入。
6. **默认性能优先。** 在修改和发送路径上花时间的加固措施（清零删除的元素和填充字节、重置时清零、`edit()` 里延迟复用、
   保存时自检）默认关闭，`-DXOFFSET_HARDENED=1` 一起打开。不打开时，容器删掉的元素字节、复用的 buffer 在
   `reset()` 之前存过的内容，可能随 `save_verified` 发出；接收方不能看到这些字节的地方用 `save_compact` 或加固构建
   （[威胁模型 4.1 节](docs/V2_THREAT_MODEL.md)）。

格式规范见 [V2_FORMAT.md](docs/V2_FORMAT.md)（wire 格式 2.1）。

## 性能对比

两组对比，都在云端 x86-64 Linux 上（Xeon，GCC 16 `-O3`，钉核）。云端机器有噪声，所以每个数都是多个进程的
中位数，比值在 0.95–1.05 之间按持平看。比值是 XOffset ÷ 对手：**小于 1 表示 XOffset 更快**。压缩一律不计时，
字节数按 zstd 3 级压缩后报告。

### CppCon 2024 的 `Character`

CppCon 2024 分享里的游戏角色（64 个物品的 map、路径点、属性、8 件各带属性的装备），用 4.0.0 和现在的规则重跑：
随机数在计时前生成，各家的物品 map 都是同一种有序扁平 map，“接收”指接收方能读也能改。15 个进程取中位数，
PR #292 之后的库（[测法和全部数据](bench/cppcon2024/README.md)）。

**发送 + 接收**（每条消息，复用 buffer，XOffset 用信任档）：

| 库 | ns / 条 | XOffset ÷ 对手 |
| --- | ---: | ---: |
| XOffset | 96 | 1.00 |
| zpp::bits | 343 | 0.28 |
| FlatBuffers（构建 + 读出） | 663 | 0.15 |
| cista（raw / offset，不校验） | 654 / 660 | 0.15 |
| Cap'n Proto | 2,381 | 0.04 |
| MessagePack | 6,510 | 0.015 |
| Protocol Buffers | 8,676 | 0.011 |

换成完整校验档，XOffset 是 232 ns，仍是 zpp::bits 的 0.67。

**读写循环**（每次：改标量和位置，增删装备、路径点、一个属性和一个物品）：

| 布局 | XOffset | std 容器 | cista raw / offset | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: |
| 2024 原样，ns | 239 | 340 | 224 / 245 | 78 |
| XOffset ÷ 对手 | | 0.70 | 1.07 / 0.97 | **3.06** |
| 调优写法\*，ns | 62 | 79 | 46 / 53 | 72 |
| XOffset ÷ 对手 | | 0.79 | **1.34 / 1.16** | 0.86 |

\* 小的属性数组内联存放、按范围追加、按已知位置删除；每个能这样写的库都这样写。

原地读写比同样代码跑在 std 容器上快，2024 原样布局下和 cista 持平，但不是每种情况都最快：2024 原样布局下
Protocol Buffers 赢（它的 `RemoveLast` 留着删掉的对象复用），调优写法下 cista 快 1.16–1.34 倍。XOffset 明显
领先的是一个来回：**读写一次 + 发送一次 + 接收一次**，XOffset 143 ns（调优写法），zpp::bits（std 容器）393 ns
（0.36），cista 688–715 ns（0.20–0.21），Protocol Buffers 8,748 ns（0.016）。

大小：`Character` 的 wire 原始 3,232 字节，zstd-3 后 3,001 字节（zpp::bits 2,926，Protocol Buffers 3,802）。
wire 头让 XOffset 的小消息比 zpp::bits 多约 100 字节；这是设计上接受的，只报告，不为此改格式。

### SLG 游戏服务器计分卡

`bench/slg` 端到端模拟一个策略游戏服务器：登录时下发整份状态、逐客户端增量、共享地图视口、客户端上行指令、
玩家在服务器之间迁移。每格只计数据写入、发送和接收，对手取这一格最强的（zpp_bits 或不校验的 cista，都不做
校验；XOffset 用信任档）。4.0.0 发版时（[PERFORMANCE.md 第 6b 节](docs/PERFORMANCE.md)）：**19 个门禁格，
赢 14、平 5、输 0**，8 批 × 21 周期，压线的共享增量格按三种代码布局复判。

这些数字只对这些负载、这些适配写法和这些机器成立；换负载或换对手的写法，数字会变。
[PERFORMANCE.md](docs/PERFORMANCE.md) 有每个版本的历史（包括更早的 Apple M4 结果）、测法和局限。

## 快速开始（C++20）

需要 Git、CMake 3.20+，以及 GCC 12+、Clang 16+、Apple clang 15+ 或 MSVC 19.38+，目标是 64 位小端：

```sh
git clone https://github.com/ximicpp/xoffsetdatastructure.git
cd xoffsetdatastructure
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20
cmake --build build --config Release --target helloworld
./build/bin/Release/helloworld        # Windows: build\bin\Release\helloworld.exe
```

不写 `-DXOFFSET_CXX_STANDARD` 时，工具链支持反射（CMake 3.30+、GCC 16+ 或 P2996 Clang）就选 C++26，否则按 C++20
构建并提示。接下来看 [GETTING_STARTED.md](docs/GETTING_STARTED.md)，每个场景一个能运行的示例。

| 构建 | 编译器 |
| --- | --- |
| 服务端，C++26 反射 | GCC 16+ 或 P2996 Clang（[toolchains](toolchains/README.md)）；CMake 3.30+ |
| 客户端以及任何 C++20/C++23 构建 | 各平台编译器：MSVC 19.44（`/Zc:preprocessor`）、clang-cl、MinGW GCC、Android NDK Clang、Apple clang（iOS 17+ / macOS 14+）；CMake 3.20+ |
| 只用 OS 存储层 | 任何 C++17 编译器；CMake 3.13+ |

安装、包的使用、验证过的平台矩阵、存储和所有权规则见 [BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md)（英文）。

## 文档

| 文档 | 内容 |
| --- | --- |
| [GETTING_STARTED.md](docs/GETTING_STARTED.md) | 按场景入门，每个场景一个能运行的示例 |
| [USER_GUIDE.md](docs/USER_GUIDE.md) | API：类型、修改、保存加载、存储、错误处理、核心承诺 K0–K9 |
| [DEPLOYMENT.md](docs/DEPLOYMENT.md) | 上线用的上限、链路和检查清单 |
| [DELTA_SYNC.md](docs/DELTA_SYNC.md)、[SHARED_DELTA.md](docs/SHARED_DELTA.md) | 逐客户端增量和共享视口增量 |
| [SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md)、[SCHEMA_PINS.md](docs/SCHEMA_PINS.md) | 两端 schema 版本不同；固定布局 |
| [V2_FORMAT.md](docs/V2_FORMAT.md)、[V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) | wire 格式；检查覆盖什么、不覆盖什么 |
| [ALPHA_COMPATIBILITY.md](docs/ALPHA_COMPATIBILITY.md) | 兼容性和输入约定 |
| [CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md)、[DEVICE_TESTING.md](docs/DEVICE_TESTING.md) | 客户端工具链和真机记录 |
| [PERFORMANCE.md](docs/PERFORMANCE.md)、[bench/cppcon2024](bench/cppcon2024/README.md) | 性能数字和测法 |
| [CHANGELOG.md](CHANGELOG.md)、[RELEASING.md](docs/RELEASING.md) | 发版 |

## 许可证

见 [LICENSE](LICENSE)。
