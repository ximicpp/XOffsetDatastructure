# 复用同一个 XBuffer（0.2.0-alpha，实验接口）

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

调用 `reload_verified` 或 `reset` 之后：

- 之前取得的根引用、元素指针或引用、`view()` 得到的 `string_view`、借用的 span 全部失效，
  不能再使用；
- `XHandle` 会按新的内容重新解析（与 move 赋值后的规则相同）；
- buffer 的地址（`get_address()`）不变。

## 失败

`reload_verified` 失败时抛出与 `load_verified` 相同类别的异常，buffer 变成空的、没有可用
的根：`root<T>()`、`save_verified<T>()` 抛 `XException`，`has_root<T>()` 返回 false。之后可以
再次 `reload_verified` 或 `reset<T>()`。旧内容不保留；需要"失败时保留旧内容"时继续用
`load_verified` 构造新 buffer。

## 内存

复用不会自动归还已提交的页：先收一条大消息、再收小消息时，大消息用过的页仍然提交着。
需要归还时调用 `reclaim_tail_pages()`（释放逻辑末尾之后的整页），或者销毁 buffer。

`reload_verified(wire)`（不带资源参数的形式）的校验临时内存来自这个 buffer
自己的小块缓存：最多 8 块、每块不超过 64 KiB，按大小和对齐原样复用，所以循环
reload 时每条消息不再向宿主分配器申请校验内存。这些块只是宿主侧临时内存，
不进 payload，也不进 wire；随 buffer 一起释放，也可以调用
`release_reload_scratch()` 提前归还。`reload_verified_with_resource` 照常使用
调用方给的资源。
