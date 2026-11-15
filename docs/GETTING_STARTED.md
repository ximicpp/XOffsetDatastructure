# 入门：核心使用场景与推荐写法（4.0.0）

本文按使用场景带新用户走一遍：服务端之间、服务端发往客户端、客户端上行。每个场景对应一个能直接
运行的示例，示例就是推荐写法。先按 [README 的 C++20 快速开始](../README.md#quick-start-c20) 跑通
`helloworld`，API 的完整说明在 [用户指南](USER_GUIDE.md)。

## 1. 这个库解决什么问题

服务端（Linux）和客户端（Windows、iOS、Android，全部 64 位小端）用同一份 C++ 类型定义交换数据：

- 对象直接建在一块连续 arena 里，原地读、原地改（含变长修改）。
- arena 的字节就是消息：发送时整块复制，接收时校验后复制，没有逐字段的编码和解码。两端各复制一次，
  不是零拷贝。
- 接收方拿到之后照样原地改、再发出去；空闲空间随消息带过去，接着改不退化。
- 对方已有副本时只发改过的字节：整份状态用增量，视口用共享增量（库里带发布器）。

## 2. 先跑示例

在 README 快速开始的构建目录里（C++20 配置即可，C++26 反射构建同样可用）：

```sh
cmake --build build --config Release --target server_to_client client_uplink server_to_server
./build/bin/Release/server_to_client
./build/bin/Release/client_uplink
./build/bin/Release/server_to_server
```

三个示例另有关异常构建（`server_to_client_noexc` 等）；整个构建之后 `ctest --test-dir build -C Release -L examples`
把六个都跑一遍。

| 场景 | 发送方 → 接收方 | 接收器（链路） | 默认检查档 | 示例 |
| --- | --- | --- | --- | --- |
| 整份状态：加入、重连、握手答 `convert` | 服务端 → 客户端 | `XReceiver<XLink::from_server>` | 信任 | [`server_to_client.cpp`](../examples/server_to_client.cpp) |
| 增量：客户端握着自己的玩家、单位数据 | 服务端 → 客户端 | `XReceiver<XLink::from_server>::apply_delta` | 信任 | [`delta_sync.cpp`](../examples/delta_sync.cpp) |
| 视口：每个客户端看共享世界的一部分 | 服务端 → 客户端 | `XReceiver<XLink::from_server>::apply_sync` | 信任 | [`shared_delta.cpp`](../examples/shared_delta.cpp) |
| 上行指令 | 客户端 → 服务端 | `XReceiver<XLink::from_client>::view` | 边界（只读） | [`client_uplink.cpp`](../examples/client_uplink.cpp) |
| 保留、修改或转发客户端数据 | 客户端 → 服务端 | `XReceiver<XLink::from_client>::reload` | 完整 | [`client_uplink.cpp`](../examples/client_uplink.cpp) |
| 玩家迁移、存档 | 服务端 ↔ 服务端 | `XReceiver<XLink::server_peer>` | 信任 | [`server_to_server.cpp`](../examples/server_to_server.cpp) |
| 连接建立时对 schema 版本 | 双向 | `xoffset_handshake.hpp` | — | [`schema_handshake.cpp`](../examples/schema_handshake.cpp) |

检查档跟着链路走，由接收器的类型决定，没有全局开关（[用户指南第 8 节](USER_GUIDE.md#8-核心承诺与-api-稳定性)）：

- **信任档**：服务端之间、服务端发往客户端的默认档。只查 wire 头（schema 指纹、上限、根偏移）和空洞表边界，
  不遍历对象图。前提是发送方是自己的同构建服务，链路有 TLS、mTLS 或在内网。调试和 sanitizer 构建照样做
  完整校验。
- **边界档**：客户端上行的默认档。保证任何读取都不越出这条消息，数据只读使用。
- **完整档**：要保留、原地修改或转发客户端发来的数据时用。

任何档都只检查结构，不检查内容：结构合法、字段值随意的消息会被接受。客户端发来的字段值由服务端按自己的
状态和游戏规则核对（`client_uplink.cpp` 里的 `on_order`）。

## 3. 第一步：一份协议头，两端共用

[`examples/game_protocol.hpp`](../examples/game_protocol.hpp) 是三个示例共用的消息类型。服务端（C++26 反射）
和每个客户端（C++20）的构建都包含同一份头文件：

```cpp
struct Unit {
    std::uint32_t id;
    std::uint32_t owner;
    std::int32_t x, y, hp;          // 示例里分开写，每个字段一行
    XString name;
};
XOFFSET_RECORD(Unit, id, owner, x, y, hp, name)        // 全局作用域，紧跟类型

struct WorldView {
    std::uint64_t tick;
    XVector<Unit> units;
};
XOFFSET_RECORD(WorldView, tick, units)
XOFFSET_REGISTER_SCHEMA_NAME(WorldView, "game.WorldView")   // 每个根类型一个稳定名字

XOFFSET_PIN_SCHEMA(WorldView, "record[s:24,a:8]{...}");     // 固定布局，见下
```

- 普通聚合体，容器用库里的 `XString`、`XVector`、`XSet`、`XMap`；不用指针、`std::` 容器，标量用定宽类型
  （[用户指南 2.2 节](USER_GUIDE.md#22-可移植标量)）。
- 每个记录一行 `XOFFSET_RECORD`，写在全局作用域（不能放进命名空间）。C++20 构建从它拿字段，C++26 构建
  用反射核对它；两种构建写出的字节相同。
- 每个根类型（作为一条消息的根）注册一个 schema 名字，再用 `XOFFSET_PIN_SCHEMA` 固定布局：任何编译器
  算出的布局和固定文本不一致时编译失败，客户端工具链的差异在构建时就暴露，而不是等到运行时被拒。固定文本
  由 `schema_pin<T>("名字")` 生成，改了类型就重新生成并提交（[SCHEMA_PINS.md](SCHEMA_PINS.md)）。

## 4. 服务端发往客户端

### 4.1 整份状态（`server_to_client.cpp`）

客户端刚加入、重连，或者握手答 `convert`（两端 schema 版本不同）时，服务端发整份状态。

```cpp
// 服务端：原地改，写进复用的字符串。
world.edit([&](WorldView& v) { v.tick = t; for (Unit& u : v.units) u.y += 1; });
world.save_verified_into(wire, XSaveOptions{.emission = XWireEmission::zero_runs});

// 客户端：每条链路一个接收器，一个常驻 buffer 反复 reload。
XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(kViewLimit)};
if (XStatus s = rx.reload(held, wire); !s) ask_for_whole_state();
held.read([](const WorldView& v) { draw(v); });       // 就地读普通成员
```

- 服务端改数据：零散字段用 `root()` 代理（buffer 搬家后照样有效），循环和批量修改用 `edit()`（钉住期间给
  普通引用，速度和普通结构体一样）。
- 发送：`save_verified_into` 复用同一个字符串，字符串够大之后不再分配；跨机器发送加 `zero_runs`，容器余量
  里的零段不占线路字节。
- 每条消息自己加长度帧，并在复制前按上限拒绝过长的帧（库不规定帧格式）。
- 客户端：`reload` 复用 `held` 的映射，每条消息不新建映射；失败时 `held` 变空，向服务端再要一份。
  客户端代码只用返回 `XStatus` 的入口，关异常构建同样可用（第 7 节）。

### 4.2 只发改过的字节（`delta_sync.cpp`、`shared_delta.cpp`）

客户端已有副本之后，服务端不再发整份：

- **客户端自己的状态**（玩家、背包、自己的单位）：增量。服务端每个客户端一个 `XDeltaBaseline`，每 tick
  `save_delta_into` 只写改过的字节；客户端 `rx.apply_delta<T>(held, delta, cursor)` 就地补进副本。丢包或
  重启后返回 `delta_base_mismatch`，服务端 `reset()` 基线发关键增量即可恢复。
  见 [DELTA_SYNC.md](DELTA_SYNC.md) 和[用户指南 4.5 节](USER_GUIDE.md#45-增量同步只发改过的字节)。
- **共享世界的视口**（地图上的单位、行军）：共享增量。服务端用库里的发布器 `XSyncPublisher`，每个变了的
  实体每 tick 只编码一次，看得到它的客户端都收同一段字节；客户端 `rx.apply_sync(msg, cursor, target)`
  写进自己的记录。见 [SHARED_DELTA.md](SHARED_DELTA.md) 和[用户指南 4.6 节](USER_GUIDE.md#46-共享增量视口同步)。

两种增量都要求两端 schema 相同：握手答 `same` 才发增量，答 `convert` 就发整份（4.1）。客户端收到的副本
只读，要回传修改就用客户端自己的 buffer 走上行（第 5 节）。

## 5. 客户端上行（`client_uplink.cpp`）

客户端发来的数据是不可信输入。

```cpp
// 客户端：一个发送 buffer 反复用。
auto o = out.reset();
o.unit_id() = unit; o.to_x() = x; o.to_y() = y;
out.save_verified_into(wire, XSaveOptions{.emission = XWireEmission::dense});

// 服务端：边界档就地只读，不复制；再按游戏规则核对内容。
XReceiver<XLink::from_client> up{XBuffer::maximum_capacity(4096)};
if (auto v = up.view<MoveOrder>(wire)) {
    const MoveOrder& o = v->root();                    // 只有 const 访问
    if (owns(conn_player, o.unit_id) && in_map(o.to_x, o.to_y)) move(o);
}
```

- 上行消息小，服务端给这条链路设小上限（示例里 4 KiB），解析前先按长度拒绝。
- `view` 要求稠密 wire（4 KiB 以下总是稠密，客户端显式写 `dense` 更稳妥），字节从 8 字节边界开始，并且在用完
  视图前不被改动或释放。
- 把需要的字段读进服务端自己的状态；结构检查不担保内容，玩家 id、单位归属、坐标范围这些由服务端核对。
- 要保留客户端的数据并修改，或者转发给另一台服务器：`up.reload<MoveOrder>(kept, wire)`（这条链路上只能是
  完整档），修改后用 `save_compact` 重建再发，不把客户端写在空洞和余量里的字节带给下一跳
  （[DEPLOYMENT.md 4.2 节](DEPLOYMENT.md#42-转发客户端上行数据用-save_compact-重建)）。

## 6. 服务端之间（`server_to_server.cpp`）

玩家从区服 A 迁到区服 B，离开 B 时把状态交给存档服务器。每一跳都是同样三步：保存 arena 字节，收进常驻
buffer，原地修改。

```cpp
XReceiver<XLink::server_peer> rx{player_cap()};       // 信任档
if (XStatus s = rx.reload(player, wire); !s) refuse();
player.root().gold() += 250;                           // 零散字段：代理
player.edit([](PlayerState& p) {                       // 一批会分配的修改：一个钉住作用域
    for (std::uint32_t item = 20; item < 40; ++item) p.items.push_back(item);
    p.quests[100u] = 4;
}, 1024);                                              // expected_growth：预留空间，中途不会失败
player.save_verified_into(out, XSaveOptions{.emission = XWireEmission::zero_runs});
```

- `server_peer` 只用于自己控制的服务端之间，不用于客户端数据。
- 会在原地增长的 buffer 用 `XGrowPolicy::Automatic` 和明确的 `maximum_capacity`。
- 在 `edit()` 里做会分配的批量修改时传 `expected_growth`，并在之后检查 `ok()`；不传时 buffer 不能在钉住期间
  搬家，空间不够会中途停下。
- 两台服务器的构建版本不同（滚动更新）时，先做 schema 握手（`schema_handshake.cpp`、
  [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)）。

## 7. 客户端构建：C++20、原生编译器、关异常

- 客户端（Windows MSVC/clang-cl、Android NDK Clang、iOS Apple clang）用 C++20 和字段表构建，和服务端是同一个
  库、同一种 wire（[CLIENT_TARGETS.md](CLIENT_TARGETS.md)）。CMake 目标 `XOffset::xoffset` 会自动给 MSVC 加上
  `/Zc:preprocessor /Zc:__cplusplus /permissive-`。
- 引擎关异常（`-fno-exceptions`、MSVC `/EHs-c-`）时整个库照样可用，不需要 RTTI。接收用返回 `XStatus` /
  `XResult` 的入口（`XReceiver` 的各个方法、`try_load_verified`、`try_reload_verified`）；修改时空间不足这类
  没有调用方可返回的失败会调用 `set_fatal_handler` 设的处理器后终止（[用户指南 6.2 节](USER_GUIDE.md#62-关异常构建-fno-exceptionsmsvc-ehs-c-)）。
  三个场景示例都另外按关异常编译成 `*_noexc` 并在 CTest 里运行。
- 苹果平台部署目标不低于 macOS 14、iOS 17；只支持 64 位小端（[用户指南第 5 节](USER_GUIDE.md#5-必须遵守的约束)）。

## 8. 推荐写法清单

| 做 | 不做 |
| --- | --- |
| 一份协议头两端共用，每个记录一行 `XOFFSET_RECORD`，根类型注册名字并 `XOFFSET_PIN_SCHEMA` | 两端各写一份类型定义；把记录宏放进命名空间 |
| 接收器按链路建：`server_peer`、`from_server`、`from_client` | 拿服务端之间的信任档去收客户端数据 |
| 每个 buffer、每个接收器都设 `maximum_capacity`；先按长度拒绝过长的消息 | 不设上限就接收（一条合法 wire 也能占满内存） |
| 常驻 buffer 反复 `reload`、发送字符串反复 `save_verified_into` | 热路径上每条消息 `load` 一个新 buffer |
| 零散字段用 `root()` 代理，循环和批量修改用 `edit()` / `read()` | 长期持有 `plain_root()` 的引用（buffer 搬家后悬空） |
| 已有副本后发增量或共享增量，整份只在加入、重连、`convert` 时发 | 每 tick 给每个客户端发整份 |
| 客户端上行用 `view` 只读，内容按服务端状态核对 | 把结构检查当成内容检查 |
| 保留或转发客户端数据：完整档 `reload`，转发前 `save_compact` | 收到客户端数据后原样 `save_verified` 转给别人 |
| 客户端用 `XStatus` / `XResult` 入口，关异常构建同样能编译运行 | 在客户端依赖异常处理接收失败 |

## 9. 接下来读什么

- [用户指南](USER_GUIDE.md)：类型定义、修改、保存与加载的全部规则，引用何时失效，错误处理。
- [部署指南](DEPLOYMENT.md)：同版本发布、不可信输入、容量上限、网络与压缩，接入检查清单。
- [DELTA_SYNC.md](DELTA_SYNC.md)、[SHARED_DELTA.md](SHARED_DELTA.md)：两种增量的格式、检查和保证。
- [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)：两端 schema 版本不同时的握手和转换。
