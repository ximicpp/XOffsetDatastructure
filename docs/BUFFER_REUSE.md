# 复用同一个 XBuffer（0.3.0-beta.1，实验接口）

大量 KB 级消息逐条收发时，每条消息都新建、销毁一个 `XBuffer` 的固定开销（映射、owner
登记、空闲表）会超过序列化本身。除了用 `XBufferPool` 复用映射，还可以让收发两端各自
长期持有一个 buffer，逐条复用：

```cpp
// 接收方
XBuffer in = XBuffer::create<Msg>(16 * 1024, cap);
for (const std::string& wire : incoming) {
    in.reload_verified<Msg>(wire);          // 完整校验，复用 in 的映射和登记
    handle(in.root<Msg>());
}

// 发送方
XBuffer out = XBuffer::create<Msg>(16 * 1024, cap);
std::string frame;
for (...) {
    Msg& m = out.reset<Msg>();              // 清空，重建默认构造的根
    fill(m);
    out.save_verified_into<Msg>(frame);     // 写进 frame，复用其容量
    send(frame);
}
```

`TypedXBuffer<T>` 有同名的 `reload_verified(wire)`、`reset()`、`save_verified_into(out)`。

## 直接接收进复用槽

`reload_verified(wire)` 要先有一整段连续的 wire，库再把 payload 拷进 buffer。传输层可以
省掉这次拷贝，直接把收到的字节写进 buffer：

```cpp
XWireHeaderV2 h = read_header(conn);         // 先收 wire 头，拿到三个长度
auto t = in.begin_verified_reload<Msg>(h.used_bytes, h.hole_count, h.extent_count);
std::memcpy(t.header_bytes().data(), &h, sizeof h);
recv_exact(conn, t.extent_bytes());          // 稀疏 wire 的 extent 表
recv_exact(conn, t.payload_bytes());         // 稠密：整段；稀疏：每个 extent 写到它的偏移
recv_exact(conn, t.hole_bytes());            // 空洞表
t.complete(t.header_bytes().size(), t.payload_bytes().size(),
           t.hole_bytes().size(), t.extent_bytes().size());
t.finish();                                  // 与 reload_verified 同一个校验器
handle(in.root<Msg>());
```

- 接纳、校验、拒绝集合和结果与对同一条 wire 调用 `reload_verified<T>()` 完全相同。
- `complete()` 报的是实际写入的字节数；稀疏 wire 的 payload 计数包括 gap，即全部 extent
  落地后的逻辑长度。消息比头里声明的短或长、没有调用 `complete()`、计数不符、声明的
  长度与头不符，都拒绝。
- token 在 `finish()` 之前被销毁（例如连接中途断开）等同于一次失败的 reload：buffer
  变成空的，没有可用的根。
- 从 begin 到 `finish()` 成功之前，buffer 没有可用的根；期间不能使用、移动或销毁 buffer，
  同一个 buffer 同时只能有一个 token。
- begin 不清零：稠密 payload 由传输层整段覆盖；稀疏 wire 的 gap 只在 finish 时清掉旧内容
  弄脏的部分（从未暴露过的页本来就是零）。
- header、extent 表和空洞表放在 buffer 保留的暂存里（与校验位图一起由
  `release_reload_scratch()` 归还），稠密 wire 每条消息不向宿主申请内存。
- 传输层按未校验的 extent 表写入时要自己检查每个 extent 落在 `payload_bytes()` 之内；
  越界的表在 finish 时一定会被拒绝。

## 语义

- `reload_verified<T>(wire)` 的接纳和校验与 `load_verified<T>(wire, 该 buffer 的 MaximumCapacity)`
  完全相同（同一个校验器，同样的拒绝集合）。超过该 buffer 的 `maximum_capacity` 的 wire
  以 `XBadAlloc` 拒绝。
- `reset<T>()` 等价于 `create<T>(当前大小, 当前 MaximumCapacity)`。
- `save_verified_into<T>(out)` 与 `save_verified<T>()` 输出逐字节相同，`out` 是调用方独占的副本，
  之后继续修改 buffer 不影响它。
- 复用后，之前内容留下的字节不会出现在新内容里：wire 里的空隙、尾部和之后增长出来的
  字节都读为零。

## 失效规则

调用 `reload_verified`、`begin_verified_reload` 或 `reset` 之后：

- 之前取得的根引用、元素指针或引用、`view()` 得到的 `string_view`、借用的 span 全部失效，
  不能再使用；
- `XHandle` 会按新的内容重新解析（与 move 赋值后的规则相同）；
- buffer 的地址（`get_address()`）不变；例外是可搬槽位上的 buffer 收到放不下的内容时会先换存储
  （见 USER_GUIDE 第 3.4 节），这时地址改变，`epoch()` 照样加一。

## 失败

`reload_verified` 失败时抛出与 `load_verified` 相同类别的异常，buffer 变成空的、没有可用
的根：`root<T>()`、`save_verified<T>()` 抛 `XException`，`has_root<T>()` 返回 false。之后可以
再次 `reload_verified` 或 `reset<T>()`。旧内容不保留；需要"失败时保留旧内容"时继续用
`load_verified` 构造新 buffer。

## 内存

复用不会自动归还已提交的页：先收一条大消息、再收小消息时，大消息用过的页仍然提交着。
需要归还时调用 `reclaim_tail_pages()`（释放逻辑末尾之后的整页），或者销毁 buffer。

校验用两张位图（每 8 字节一位，活块一张、空洞一张）。payload 不超过 32 KiB 时
位图在栈上，不申请任何宿主内存。更大的 payload：`reload_verified(wire)`（不带资源
参数的形式）用这个 buffer 自己保留的一块位图内存，按收到过的最大 payload 增长、
之后原样复用，所以循环 reload 时每条消息不再向宿主分配器申请校验内存。这块内存
只是宿主侧临时内存，不进 payload，也不进 wire；随 buffer 一起释放，也可以调用
`release_reload_scratch()` 提前归还。`load_verified_with_resource` 和
`reload_verified_with_resource` 向调用方给的资源申请一块。
规范布局的 wire（没有空洞，块按访问顺序首尾相接，例如 `XCompactor::compact` 的
结果）走游标路径，只记一个偏移，不碰位图，任何大小都不申请校验内存。
