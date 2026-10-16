# G3 三方向：实现可行性与寿命静态审查

2026-10-16。结论：三方向都有可描述的实验落点，但现有提案仍需补齐下述事务/完成与资源契约；
本审查不批准实现、不判定实验拒绝，也不估计收益。成本是否足够及是否启动，仍由统筹结合
W2 精确网络成本和 W3 校准裁决。未编译、测试、计时、运行网络或接触执行队列。

输入身份见 [source-identities.json](source-identities.json)。下文 H 指
[固定 F 主头](../../../xoffsetdatastructure.hpp)，V 指 [F VM 头](../../../xoffset_virtual_memory.hpp)，
N 指 [冻结 W1v2](../../../build/vm-network/prepared-v2/source/net_reference.cpp)。
F 双头为 `9bc8f1f5…7640bb2`/`22f181b5…8239ad`，N 为 `d4235d10…9617dcf`。
审查的是决议 `12554395…c92899f` 和预登记 `d4b4c8a4…cba9e9`，不改 W3 的文件。

| 方向 | 可能省掉的工作，均待成本证据 | 仍在或新增的工作 | 公开 API 判断 |
|---|---|---|---|
| 复用 owner | 真正保留同一 reservation/Core 时，可能减少池查询、剩余映射及 owner 注册/注销 | 输入完整验证、业务处理、输出仍在；事务暂存/回滚、旧 owner 保留、宿主状态准备和提交同步可能抵消收益 | 现有 `load_verified + swap` 已可安全替换，但不消除新 owner；真正就地 verified reuse 没有现成受支持入口。先做私有副本，不必先发布 API |
| 直接读新 staging owner | 服务端或客户端一次 string→owner 的 P 字节复制，及相应接收 string；一次只选一端 | 头部/进度、映射或池、页面准备、registry、全部图验证/语义校验仍在；普通 grow 会新增 P 字节清零 | F 没有“填入后原位 verified finalize”的受支持能力。需隔离 staging/finalize seam；生产是否公开后定 |
| 减少输出复制 | owning response string 分配及 P 字节 payload 复制/拼接 | 相同规范化/头构造/现有校验、传输、offset bookkeeping；owner 延迟释放、独占和背压保留 | 固定消费者的内部 helper/发送作用域即可试验，不必发布库 API；直接返回裸 spans 不足 |

P 为该帧 `used_bytes`，不是名义 vector 有用字节。不能把整个 load/save 阶段或重叠阶段之和
当作可消除成本，也不能重复计算 F warm pool 已消除的映射成本。

## 1. 复用 owner：先明确复用对象及事务终点

**落点。** 新加载实际经过 `H:3720 load_verified<T>` → `H:477 init_from_bytes` →
`V:191 from_bytes`。registry 注册/注销/转移在 `H:505–550`，存储及宿主 free-block 状态为
`H:745–750` 的私有成员。现有 `H:306 swap`/`H:505 move_from` 更新 registry 并递增双方 epoch。

用现有 API 先加载临时 owner，成功后 `old.swap(staged)`，无需新 API，失败能不触碰旧值；
但仍走完整新 owner 加载，且多一次 swap/锁/epoch 工作。复用同一 C++ 变量、复用池的
映射和真正就地复用 Core 是三种不同成本，不能共用“owner reuse”结论。

真正覆盖旧 reservation 前，必须选定一种完整事务方案：

- 新 owner 先验证后 swap：旧值保留，但仍新建/注册 staging，峰值同时保留旧、新两份。
- 两个已注册 owner 交替，published 旧值加未发布 spare：可能省稳态 mapping/registry churn，
  但须内部 refill/finalize、宿主状态重置、失败 dirty spare 处理及最终回收；第二个持久
  reservation 和 swap/epoch/registry 锁成本必须计入。这与每次新 load 后 swap 不同。
- 新 owner 验证后再复制到旧 reservation：可能保留旧 Core/registry，却额外复制一次 P；
  必须先准备足够页面/元数据，使覆盖之后的提交步骤不再抛出。
- 就地覆盖并回滚：需要旧 payload、logical extent、宿主 `free_blocks_`、growth/schema/type 状态
  等的备份/恢复策略。只备份 wire 字节不覆盖 Core 宿主状态；备份分配必须早于破坏旧值。
  取消后仍可能写入的操作必须先真正完成，再恢复/复用旧区，防止迟到写破坏回滚结果。

普通 `grow_to` 失败允许新增可访问页留下（V:238–249）；“旧值保留”不能被扩写为全部资源
占用回滚。失败必须保留原值和可用性；是否还要求 epoch/逻辑长度/资源余额完全不变应单列。

**事务范围缺口。** N:219–223 的 publication 在 load 后、process 前，且每次请求没有一个
持续对外发布的旧 owner。提案的“所有 allocation/cancellation 失败保留旧值”必须明确只覆盖
load transaction，还是也覆盖 process、save/发送失败；后者不能由一次 load 的强保证推出。
原地 process 可能已改了一部分值，输出分配也可能失败；TCP 已发出的字节无法撤回。
若要求持久旧值，应增加同样保留旧值的 F pooled `load+swap` 对照；不能直接把现有无旧值
W1 路径与多一项业务保证的候选作单变量收益比较。

**指针。** 成功替换即使数值基址相同，也不能承诺原 root/元素引用继续表示原值。
新帧 root offset 可不同；必须按提交契约使 epoch 缓存失效。`XHandle` 保存原 C++ owner 指针
（H:3410–3423），不随 storage 自动获得共享所有权；epoch 不是锁，已有裸引用和缓存 handle
也不能在覆盖期间并发读取。失败前后的旧引用可用性与成功后的失效范围需分开规定。
成功后 handle 可能重解析到同一 C++ owner 的新根，不能把“旧引用失效”误写成全部 handle 自动变空。

## 2. 直接读新 staging：复制消除不能变成额外清零

**最小落点。** N:171–177 当前先收 owning string，N:219 再调用 verified load；可只替换
服务端这一路，保留客户端 F load、process/save、协议、连接和三池对照。内部 seam 应围绕
`V:191–208` 的 reserve/enable/填入/发布 logical length，以及 `H:477–503` 的 arena 接纳，
复用 `H:3720–3815` 完整验证后半段；不要另写一个漏项的“快速验证”。

当前 `from_bytes` 在页面准备后直接 memcpy，不先清零；`VirtualMemoryStorage(logical, cap)`/
`grow_to`（V:241–249）会 memset 新逻辑区。直接收进普通 grow 出来的区，可能只是把一次
P memcpy 换成 P memset 再 recv，并未省去同量级内存遍历。这个保守版本可以存在，但不能
与“未发布新区跳过预清零”的另一变体混为一个候选或偷偷升级。

需要的隔离顺序是：

1. 拥有并对齐固定 88 字节头；沿用 W1/F 的 magic/version/header/endian/flags/type/schema、
   保留字段、offset/length、owner/wire/总准入检查，先检查再分配；合法历史
   `reserved_bytes > used_bytes` 仍接受，显式 cap 语义不变。
2. 只取得未发布 staging 的可写 payload 区，按实际偏移收取恰好 P 字节。部分字节不能
   触发 arena/容器解析或 typed root 访问；头及操作状态也必须保持拥有关系。
3. 完成后可在隔离状态内部设置已完整填入的 logical extent，仍不开放 typed access；依次保留
   arena magic/header/used_end、精确 normalized extent、root/allocator
   的存在和真实 offset、allocator/整个容器图验证（H:3786–3813）。之后才置 typed-access、
   bind schema（3814–3815）并在未取消的单一同步点发布。registry 插入可失败，也算未发布失败。

F 没有现成的受支持 public finalize。`TypedXBuffer::create()`/`make()` 会构造活跃根/allocator；
直接用 `data()` 覆盖，再跳过 finalize 或调用 unsafe root，不能算 verified 接收。
可先在生成 F 副本内部引入不可导出 typed root 的 staging capability；不要为了实验先增加
生产公共 API。若将来向外部调用方开放，就必须提供可审查的填入/提交能力，而不是任意 span。

短读/EOF/超时/取消失败只销毁新状态、不发布新值；同步 POSIX 路径退出后没有未决用户缓冲
借用。真实异步路径则须持有 reservation、页面、header、进度和队列额度至实际 completion，
即使已请求 close/cancel。未填区域不能读或发布；失败的脏 mapping 若归池，下一次加载须
完全覆盖，后续 grow 暴露区仍按 F 规则清零。不能用“不可见”取消未来清零契约。

## 3. 输出复制：保留 owner 的代价属于候选

**落点。** `H:3619–3679 save_verified<T>` 的准备/头构造与 string materialization 可在
独立副本中分离，保留原 string API 作为控制；仅替换 N:216–226 的响应持有/发送。
固定消费者也可用内部 helper 组合现有公开 shrink/root/allocator/bytes 能力，代价是维护
与 F 相同的准备逻辑；这不是必须发布新 public API 的理由。

有一个必须修正的成本前提：**F 的 save_verified 没有完整图扫描。** 它做 typed/schema gate、
shrink、root/allocator 查找、V1 header 和复制；完整图扫描在 `H:3813 load_verified`，W1 两端
均保留。不能把不存在的 save-time 扫描计作节省；额外增加发送前扫描是新增语义/成本。

规范化仍须调用 `shrink_to_fit`（H:283–289/556–573）。只去掉 spare tail，不压实或重新清零
payload 内部空闲区/容器容量/填充。发送字节必须仍为同一原生布局的 88 字节 V1 header
加同一 payload，`reserved_bytes=used_bytes`，offset/type/schema/flags/endian/CRC/保留字段同 F，
不加前缀、不换网络序、不省略内部字节。`bytes/get_buffer` 只是 payload 借用，不自动满足这些条件。

N 在发送前就释放 owner，保留独立 string；候选必须保留**稳定 owned header、独占 payload
owner、描述符/操作状态**直到真实 send/cancel completion。`const span` 不阻止其他别名写入，
epoch 不冻结内容；不得在发送期间修改容器、swap/replace、归池或销毁 owner。含 header 的
frame 移动后 header 地址会变，iovec/span 必须在稳定归属后生成或重建，不能缓存栈地址。

短 scatter/gather 进度需跨 header/payload 边界只推进一次，不能重发 header；EAGAIN/EINTR
不推进，零进度失败，正数短写不重置期限。两段传输不改变 V1 字节，也不等于内核零复制。
开始发送前准备失败不得发成功结果；部分发送失败只关闭实验连接，不重发整帧。

背压会延长 live owner、registry/pool handle 及 reservation/accessible pages 的持有期，延迟
归池/解除映射，并可能与客户端 owner 重叠。主计时止于客户端验证；若候选把服务端释放
移到该边界以后，必须在残余发送完成/释放的校正视野计入，不能把搬出计时的成本称为收益。

## 共同 gate 与建议的副本边界

- 先补全每候选的 `commit point / failure preserved state / completion point / resource ledger`。
  `hard_capacity_increase_bytes=0` 需明确是每 owner 上限还是总活跃预算；额外旧、新、rollback
  storage 不能藏在各自相同 cap 后面。W1 的 `4*max_wire` 只计逻辑 string 准入，不计 VM owner/
  pool 实占；池限额仅管 idle mapping（V:92–101），不能代替背压的活跃条数和总字节限额。
- 新 staging 和输出可先各限服务端一处，保持另端及 F none/cold/warm 不变；reuse 必须先固定
  “对象/映射/Core”含义及旧状态工作量。每方向最多一个独立生成副本，不组合三项，不改正式 F、
  frozen W1、wire、验证或硬预算。副本的源码、契约、额外保留成本和失败口径另行冻结。
  首个直读候选若还改客户端，会失去 N:245 的 `Sample::output` 原始 wire；为输出归档补作的复制
  及其持有量必须显式计入，不能移到计时外后继续套用相同完成工作量。
- W1 G1 已证明当前同步回环及独立注入模型，不证明上述新路径，更不证明具体异步库。
  真实库须说明取消、close、迟到完成、描述符借用和 buffer 最后借用点；清池不会撤销 live owner。
- 本审查没有成本比例或实验结果。按原预登记，仍需统筹认可 W2 可消除成本、W3 合格校准及
  独立比较设计后，才能批准实现；5%目标、2%默认预算及独立窗口要求不因静态可行性放宽。

独立分项复核：[owner/staging](reuse-staging-independent.md)、[输出复制](output-copy-independent.md)。
