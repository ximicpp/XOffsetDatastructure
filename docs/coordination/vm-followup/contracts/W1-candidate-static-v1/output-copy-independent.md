# 减少输出复制：独立静态可行性与寿命审查

2026-10-16。仅只读源码审查；未实现、构建、测试或计时，不构成候选批准或收益证据。

审查身份：F `xoffsetdatastructure.hpp` SHA256 `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`；`xoffset_virtual_memory.hpp` `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`；W1v2 `build/vm-network/prepared-v2/source/net_reference.cpp` `d4235d100ca1687e7834ad0383d20864b19347dc254e09b6ae2a2649b9617dcf`。所读决议/预登记 SHA256 分别为 `1255439546b60a6fcc6232d286d060c26135adcf04aeb7d8b643c0bf0c92899f`、`d4b4c8a4326a66c4e1258bcc8058902e36fc582bbd7265168dce5959a8cba9e9`。

## 实际落点与不可混淆的校验

F `save_verified<T>`（主头 3619–3679）依次做类型准入、typed/schema gate、`shrink_to_fit`、root/allocator 查找、构造 V1 header，再分配 owning string 并复制 header 与完整 payload。C++26 分支在 3669–3672 使用 `resize_and_overwrite`，后备分支在 3675–3677 用 reserve/append；不能把其他分支可能存在的初始化成本自动归给当前构建。

**这里没有完整图扫描。** `verify_structural_wire_graph` 位于 2838，真正调用点为 `load_verified<T>` 3813，之后 3814–3815 才允许 typed access/bind schema。W1 的服务器加载和客户端加载分别在 219、246 行，均保留该扫描；客户端语义验证在 247 行。预登记“排除 mandatory normalization/graph validation”不能解释成 save 内已有图扫描，也不能借此删除接收端扫描。若额外增加发送前扫描，这是新增安全语义和成本，需要单列，不能算基线已有工作。

`shrink_to_fit`（283–289，`prune_free_blocks` 556–573）只收缩逻辑 payload 尾部；VM `resize_logical`（VM 254–263）不解除映射或归还物理页。候选不能绕过这一步、发送 owner 的最大容量或给 wire 添加新前缀。相同字节须为原生布局的 **88 字节现有 V1 header + 同一规范化 payload 范围**：`reserved_bytes=used_bytes`；root/allocator 相对偏移、schema/type、magic/version、native-endian tag 不变；flags、CRC、reserved 字段维持零。Header 当前由值初始化及逐字段赋值生成（3640–3660；布局 2942–2964），不能改成网络序编码。所谓无 slack 是不传 spare tail，**不是把 payload 内部空闲区/容器容量/填充重新清零或压实**；这些仍按 F 字节原样发送。原生 payload 的大小端或 ABI 限制未因此消失。

## 最小独立候选边界

可在一个经批准的独立 F 副本内，把 3620–3660 的准备过程抽为内部 helper：同时供原有 string save 和私有 owned-output frame 使用；网络副本只替换 W1 `serve_one` 的 216–226 行及其发送适配器。Frame 必须拥有稳定 header 和该响应的独占 owner，不能只有两个 borrowed spans。生产双头和现有公开 API 不必改变；这只是可审查的副本边界建议。现有公开 `bytes()`（3596）/`get_buffer()`（295）只给借用视图，不承诺 typed/schema gate、规范化、header 构造、冻结或寿命；将其直接返回不满足提案。

可省的直接工作是 owning response string 的分配、payload 到它的完整复制和微小的 header 拼接/复制。准备/gate/offset 查找、必要规范化、现有两端图校验、语义处理/校验、输入接收 string 及其到输入 owner 的复制、客户端 response string/owner、socket 实际传输均不因此消失。POSIX scatter/gather 不等于内核零复制，也不能据此声称有端到端收益。持有 frame、registry/epoch 转移和发送 bookkeeping 自有成本。

## 事务、指针与真正完成

1. W1 219–223 的 owner 在 225 明确早于发送释放；复制后的 `saved` 独立且不可变。候选把 owner 留到发送结束，必须在准备成功之前零发送；处理/准备失败不得发送成功响应。部分帧已送出后发生超时/断连/取消，关闭本实验连接，不从头重发或把余字节解释成新帧。输入 owner 已被 process 原地修改，输出候选并不提供业务旧值回滚；客户端仍须完整接收、图校验与语义验证后才接受新值。
2. Root 指针在 growth/shrink/save 下保址（3490–3491），不代表数据快照或独占。`data/get_buffer/root` 仍可写；发送期间必须禁止其他别名修改、swap、replace、destroy、pool return，不能靠 `const span` 或 epoch 数值代替锁定/独占契约。
3. owner move/swap 在 306–321、505–522 更改 registry 并递增 epoch。Payload 地址可随 storage 所有权转移保持，但旧 `XHandle` 保存的是原 `XBufferCore*`（3421），不会自动跟随新 owner；旧裸引用/handle 的可用性必须明确失效。Header 若内嵌 frame，移动 frame 会改变 header 地址；不得在稳定归属前缓存 header/iovec 指针。移动 owner 的 registry lock、epoch 以及稳定 frame 归属开销须计入。
4. 单调字节偏移必须跨越 header/payload 边界：短写可停在任一 header 字节、边界或 payload；下一次只发送剩余子范围，不重发 header；返回字节不得超过剩余量。EINTR/EAGAIN 不推进，零进度失败，正数短写不延长绝对截止时间。描述符数组若被异步库借用，也须保持到该库规定的真正完成点。
5. W1 `send_exact`（157–167）是同步控制流中的非阻塞 socket 循环：每次 POSIX send 返回后不再借用该次用户缓冲，函数结束/异常退出没有未决用户缓冲借用；Worker 错误关闭连接（234），结束等待在 248。它未证明真实异步库的 cancel-complete 契约。真实异步发送必须让 header、payload owner、操作状态/描述符及预算在最后借用完成或取消完成前存活；cancel requested、close requested、晚到回调不能各自提前释放或重复终结。W1 SendModel 的 pending/token 模型只能作为注入证明，不能升级成生产异步证明。

## 隐藏资源和计时成本

背压时 F 保留 response string，owner 已经归池/解除映射；候选保留 owner 的 reserved/accessible pages、registry entry 和 pool handle。VM release（444–464）仅到 owner 析构才归池或解除映射；pool clear（414–423）只清闲置缓存，不撤销活跃 owner，clear 后活跃 owner 稍后仍可能重新入池。不能把 cache 为零解释为资源已释放，也不能把闲置池块、活跃 owner、输入 string、header/frame、客户端 owner/string 重叠漏计。`4*max_wire` 是 W1 的逻辑准入额度（contract budget_meaning），不覆盖 VM owner/pool 实占，不是 RSS；候选需要分别列逻辑额度与活跃/缓存/保留 bytes/owner 数，不得默增硬预算。单在途限制不能推广成流水并发下的资源证明。

基线 server owner release 已在客户端 verified 主计时的因果前置路径；候选 release 可能晚于客户端主计时结束，**不能把释放移到主计时外便称收益**。必须包含实际 send 完成和残余 server release 的校正视野，并保留 client owner release、cold/final pool clear 的对应口径。W1 `worker.finish` 在客户端 owner reset 后残余等待（248），可覆盖 worker 返回前的 send/局部对象析构；不能把整段 send 和主计时相加，因为有重叠。现有指标仍不是总应用成本，候选额外 frame 清理若在 worker 返回之后必须显式纳入，不能沿用字段名而漏算。上述是应满足的可行性条件，不是已实现/已验证的候选性质。
