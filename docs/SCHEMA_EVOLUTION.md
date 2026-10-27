# schema 演进（3.2，未发布）

新旧版本的程序互通：服务端先发新版本，线上还在用的旧客户端照常收发。转换全部在
服务端做，客户端只多一次握手。wire 格式 2.1 不变，旧版本的 wire 就是旧版本程序写出
的 wire。

| 头文件 | 用在哪一端 | 内容 |
| --- | --- | --- |
| `xoffset_handshake.hpp` | 两端 | 握手消息 `XSchemaHello` / `XSchemaHelloAck`，`peek_wire_fingerprint` |
| `xoffset_evolution.hpp` | 服务端 | 转换、版本链、按对端收发 |
| `xoffset_schema_freeze.hpp` | 服务端构建 | 发版时冻结版本，CTest 里检查锁文件 |

依赖 C++26 反射（GCC 16、P2996 Clang）。字段表后端（客户端编译器）接入后，客户端
只需要 `xoffset_handshake.hpp`。

## 1. 模型

- 一个根类型的各个版本组成**版本链**，按发版顺序排列；程序以其中一个版本工作
  （工作版本），其余版本只用于收发。
- wire 头里的 `schema_hash` 指出 wire 是链上哪个版本。收到旧版本：先按**旧版本类型**
  完整校验（`load_verified`），再逐级升级到工作版本。发给旧版本的对端：逐级降级，
  再用 `save_compact` 写出。
- 字段按**名字**对应，偏移和声明顺序无关。

| 变化 | 默认行为 |
| --- | --- |
| 新增字段 | 取 `make<T>()` 给的初值 |
| 删除字段 | 丢弃 |
| 重排、中间插入 | 按名字对应 |
| 改名 | 等于删除加新增，需要写钩子 |
| 同名不同类型（包括 int32 → int64） | 编译错误，除非钩子处理 |
| enum 值 | 目标 enum 有这个枚举值就复制，没有则转换失败 |
| 容器元素类型 | 逐个元素转换 |
| set / map 的键 | 同类型直接插入；enum 键按值检查；转换后键重复则失败 |
| 容器种类、数组长度、字符串与其他类型互换 | 编译错误，除非钩子处理 |

转换失败（enum 值不存在、`checked_narrow` 越界、键重复、目标 buffer 超过上限、钩子
调用 `ctx.fail`）一律抛 `XException`，消息里带字段路径，不产生半成品。

## 2. 写一个新版本

1. 发版时用冻结工具把当前版本存成快照头文件（第 5 节），例如 `game_v12::Player`。
   没变的记录和 enum 不复制，快照里写成 `using Item = game_v11::Item;`。
2. 修改工作版本的定义。
3. 登记版本链（全局作用域）：

   ```cpp
   XOFFSET_SCHEMA_LINEAGE(game::Player, game_v11::Player, game_v12::Player)
   ```

   工作版本不在列表里时排在最后。链上所有版本必须注册同一个 schema 名称，且两两
   `schema_hash` 不同，否则编译失败：只改字段名、不改布局不是新版本，要用
   `XOFFSET_SCHEMA_NAME_ALIAS`（第 4 节）。
4. 默认规则处理不了的字段写钩子，每对相邻版本一个：

   ```cpp
   template <> struct XOffsetDatastructure::xevolve<game_v12::Player, game::Player> {
       static constexpr std::string_view handles[] = {"gold", "nick", "display_name"};
       static void upgrade(const game_v12::Player& from, game::Player& to, XEvolveContext& ctx) {
           to.gold = ctx.widen<std::int64_t>(from.gold);
           to.display_name.assign(from.nick.view());
       }
       static void downgrade(const game::Player& from, game_v12::Player& to, XEvolveContext& ctx) {
           to.gold = ctx.checked_narrow<std::int32_t>(from.gold, "gold");
           to.nick.assign(from.display_name.view());
       }
   };
   ```

   默认复制先跑，跳过 `handles` 里的字段，然后调用钩子。`handles` 里的名字两个版本都
   没有时编译失败。嵌套记录也可以有自己的钩子。
5. 把 `evolve_plan<game_v12::Player, game::Player>()` 的输出（每个字段是复制、转换、
   检查 enum、新增、丢弃还是钩子）和快照一起提交，评审时看一步会丢掉哪些值。

`evolve<To>(buffer, options)` 把一个已校验的 buffer 转成另一个版本，单独使用也可以
（读旧快照、离线迁移）。`XEvolveOptions::with_base(&obj)` 把服务端自己的对象交给
最后一步的钩子（`ctx.base<T>()`），用来保留旧版本带不回来的字段。

## 3. 握手与收发

每条连接上，客户端先发 hello，服务端回 ack，之后才是普通消息。帧格式由应用决定，
`examples/schema_handshake.cpp` 是最简单的长度前缀写法。

客户端（不抛异常；关异常构建等 SC-8 的 `try_load_verified` 合入后支持）：

```cpp
send(make_schema_hello<game::Player, game::Chat>("client-1.4.2"));
XSchemaAckResult r = read_schema_ack<game::Player, game::Chat>(receive());
if (!r) show_update_prompt(r.message);   // 服务端不认识这个版本
```

服务端：

```cpp
std::string ack;
XPeerSchemas peer = XPeerSchemas::accept<game::Player, game::Chat>(hello, ack, "server-12");
send(ack);
if (!peer.accepted()) close();

auto msg = peer.load_verified<game::Player>(wire, XBuffer::maximum_capacity(1 << 20));  // 任何链上版本
send(peer.save(state));                                                               // 对端的版本
```

- hello 里每个根类型带三项：根类型 id、`schema_hash`、字段名摘要 `name_digest`。
  服务端对每项给出 `same`、`convert`、`reject_unknown`（链上没有）或 `reject_names`
  （布局相同但字段名不同，见第 4 节）。
- 一条消息发给很多对端时用 `XBroadcast<W>`：每个版本只转换、写出一次。
- 没有握手的场合：`XPeerSchemas::from_fingerprints` 或 `learn(peek_wire_fingerprint(wire).fingerprint)`。
- 只读旧快照：`evolve_load_verified<W>(wire)`。

服务端自己的 hello（服务端之间）用 `make_server_schema_hello<Roots...>`：它把链上其他
版本也列为 `known`。较旧的服务端看到对方认识自己的工作版本，就判 `convert`，由较新
的一方负责转换。

## 4. 字段名摘要

`schema_hash` 只覆盖布局，不含字段名。两个版本只交换了两个同类型字段的名字时
`schema_hash` 相同，按字节读会把 hp 读成 mp。三处挡住它：

- 握手比较 `name_digest`（`schema_name_digest_v<T>`），布局相同但名字不同判 `reject_names`；
- 版本链要求两两 `schema_hash` 不同（编译期）；
- 锁文件检查（第 5 节）。

真的只是改名、字节含义不变时，用别名让新版本也接受旧名字的构建：

```cpp
XOFFSET_SCHEMA_NAME_ALIAS(game::Player, ::XOffsetDatastructure::schema_name_digest_v<game_v12::Player>)
```

摘要的文本格式见 `xoffset_schema_names.hpp`，它是握手兼容承诺的一部分。

## 5. 冻结工具与锁文件

项目自己链接一个小程序：

```cpp
#include "game_schema.hpp"
#include "xoffset_schema_freeze.hpp"
int main(int argc, char** argv) {
    return XOffsetDatastructure::schema_freeze_main<game::Player, game::Chat>(argc, argv);
}
```

- `schema_freeze freeze <label> <namespace> <header> <lock>`：发版时运行，写出快照头文件
  （带 schema 名称注册和 `XOFFSET_PIN_SCHEMA`，改动冻结的类型会编译失败），并在锁文件
  末尾追加这个版本。
- `schema_freeze check <lock>`：放进 CTest，每次构建都跑。布局与某个冻结版本相同但字段名
  不同（且没有别名）时失败；布局变了而上一个冻结版本不在版本链里时失败。

测试里的完整例子：`tests/schema_freeze/`。

## 6. 发版规则

1. **服务端先于客户端发版。** 客户端只认识自己的版本，转换都在服务端。
2. **服务端的版本链覆盖线上还在用的所有客户端版本。** 保留几个版本由业务决定；去掉
   一个版本后，用它的客户端在握手时得到 `reject_unknown`，应提示更新。
3. **要能回滚服务端时分两步发。** 先发一个认识新版本、但仍以旧版本工作的服务端
   （新版本在链上，工作版本不变）；下一次再切工作版本。这样回滚到上一个服务端时，它
   仍然认识已经升级的客户端。工作版本不必是链上最新的。

## 7. 代价与上限

- 收旧版本：一次旧版本的完整校验，加上每一级一次转换（逐字段，读源对象、在新 buffer
  里构造）。同形状、同字段名、无填充、无 enum 的记录整块复制。
- 发旧版本：每一级一次转换，加一次 `save_compact`；广播时每个版本一次。
- 转换后的对象可能比源对象大（新增字段、更宽的类型）。目标 buffer 的上限默认等于源
  buffer 的 `maximum_capacity`，可用 `XEvolveOptions::maximum_capacity` 设定；超过时
  转换失败。接收不可信数据时一定要给 `load_verified` 传上限，它同时限制了转换。

## 8. 测试

| 测试 | 内容 |
| --- | --- |
| `test_schema_handshake` | hello / ack 编解码、各种判定、畸形输入 |
| `test_schema_evolution` | 默认规则、钩子、enum、容器、容量上限、`evolve_plan` |
| `test_schema_lineage` | 一个 v3 服务端对 v1/v2/v3 客户端、混合版本服务端、广播、别名、两步发版 |
| `test_schema_freeze` | 冻结工具的快照和锁文件与提交的版本逐字节一致；检查的各种失败 |
| `tests/schema_evolution/*_fail.cpp` | 应当编译失败的写法及其错误信息 |
| `fuzz_schema_evolution` | libFuzzer：任意 wire 的接收、随机对象发给每个版本、任意握手字节（见 `tests/fuzz/regressions/README.md`） |
