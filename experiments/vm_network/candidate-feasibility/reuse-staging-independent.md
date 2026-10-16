# Owner 复用与新 staging 直读：独立只读可行性审查

2026-10-16；范围仅源码分析。未编译、测试、计时、实现候选或修改冻结证据。以下是候选前提与边界，不是实现授权、功能通过或收益结论。

## 固定输入与定位约定

- `F` = `build/vm-network/prepared-v2/source/xoffsetdatastructure.hpp`，SHA256 `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`。
- `VM` = 同目录 `xoffset_virtual_memory.hpp`，SHA256 `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`。
- `W1` = 同目录 `net_reference.cpp`，SHA256 `d4235d100ca1687e7834ad0383d20864b19347dc254e09b6ae2a2649b9617dcf`；另读冻结 `frame_model.hpp`。
- 提案 `experiments/vm_io_candidates/candidate-preregister.json` SHA256 `d4b4c8a4326a66c4e1258bcc8058902e36fc582bbd7265168dce5959a8cba9e9`；决策文档 `docs/VM_IO_CANDIDATE_DECISIONS.md` SHA256 `1255439546b60a6fcc6232d286d060c26135adcf04aeb7d8b643c0bf0c92899f`。
- 已读统筹方案与 W1 G1 接受记录。G1 接受的是真实本机同步回环及单独的完成状态模型，不覆盖生产异步、持久旧值事务或任何候选。

## 共同验证底线

当前完整加载不是“复制后直接 cast”。替代入口必须保留以下全部检查及先后关系：

| 阶段 | 固定源码落点与不可省操作 |
|---|---|
| 分配前 | `frame_model.hpp:81–107` 的 `admit_header`：88 字节有主、对齐的头；magic/version/header size/endian/flags/type/schema；used/reserved 上界与 reserved 字段；root/allocator offset；无溢出的 wire/owner 容量准入。显式 `MaximumCapacity` 不把历史 `reserved_bytes > used_bytes` 当容量请求。还需先取得队列、总在途及所有额外 owner/rollback 预算。 |
| 完整帧 | `W1:150–177` 的精确 header/body 读满与绝对期限；完整 payload 必须正好 `used_bytes`。短读、EOF、取消或错误不能把已接收前缀当完整 payload。 |
| arena | `F:477–503` 的 magic/header size/used_end 范围；`F:3786–3812` 的 normalized extent、根/allocator 可访问性和对齐、wire 与 arena 实际 offset 一致。 |
| allocator/图 | `F:2838–2853`：arena flags/reserved，arena/root/allocator 所属区间与 allocation header；`F:2783–2833` 的 allocator segment binding、边界、free-list 类别/范围/计数；`F:2572–2724` 的所有 range、对齐、relative binding、allocation metadata、live/free/动态区间不重叠、字符串容量/终止符、容器容量及递归元素校验。校验的宿主 vector 仍可能分配/抛出。 |
| 发布 | `F:3813–3815` 完成图校验后才允许 typed access 并绑定 schema/root type；`W1:81–100` 的逐值检查与处理语义仍保留。规范化 V1 字节、schema、处理和输出核对不能为省操作而删减。 |

图校验中的部分类型遍历由 `validate_wire_value` 分派（`F:2745–2779`）；上表不是新增验证规则。`load_verified` 的结构验证及 `save_verified` 的规范化不可被误称为一个可删除的输入复制阶段。

## 方向一：复用 owner

**可实现性的核心是先定义提交对象与时点。** `W1:213–226` 每次创建局部 owner，完整加载后处理、保存，并在发送前销毁；它没有供失败保留的持久旧 owner。提案的“所有 parse/validation/allocation/cancellation 失败均保留旧值”比 W1 当前语义更强。需明确是 load-only 提交，还是 receive/process/save 全事务；若加载后即提交，随后 `process` 的逐元素修改或 `save_verified` 分配失败无法保留旧值。若涵盖发送失败，还必须说明本地提交与对端已收到前缀之间不存在跨网络回滚保证。

| 路径 | API 与真正成本 |
|---|---|
| `load_verified` 新 owner 后 `swap` 提交 | 现有公开 API 已足够实现 load-only 强失败保证：所有可能失败的加载先在局部变量完成，再调用 `XBuffer::swap`（`F:3469–3474`）。旧 owner 失败时不变；但这只是复用变量，不能消除新 VM owner、payload copy、注册/注销，不能作为 owner-reuse 收益证据。 |
| 两个已注册 owner 交替，旧 published + 未发布 spare | 能在独立头副本内设计，保留 spare mapping/registry 可能消除稳态 acquire/release/register churn。当前没有安全覆盖 spare、重置宿主状态并 finalize 的公开 API；现有 `data()` 只是裸可写指针。需增加**内部** quarantine/refill/validate/commit 路径；研究副本不要求先增加公开 API。代价是第二个持久 reservation、初始化、失败 dirty spare 恢复及最终回收。与 F warm 池比较时不能把已由池消除的映射重复计为收益。 |
| 原 published mapping 原地覆写 | 不满足现有强失败承诺。先验证别处再复制回来仍需暂存与复制；完整备份后覆写需复制旧 logical bytes、保留旧 logical/accessibility 状态和宿主元数据，失败时恢复，且恢复前必须等真实 I/O 完成。仅备份 arena payload 不足。OS remap/COW 另引入平台操作、失败路径和地址保证，不能作为免费回滚。 |

内部落点应围绕 `F:477–503`、`F:3720–3816` 与 `VM:184–208` 分离 prepare/validate/commit，而不是调用 `TypedXBuffer::create` 先造根后用 `data()` 覆写。重用时必须处理 `F:745–750` 的 `free_blocks_`、growth policy、root identity，以及 `F:738–743` 的 typed/schema 状态；新图不能继承旧宿主 free-block 索引，失败又不能丢失旧状态。表面上只保留 VM 地址，仍有元数据重置、校验、epoch、锁和失败事务成本。

**指针与注册：** `F:306–322` 的 swap 交换存储/宿主状态、对双方递增 epoch 并在 registry 锁内修正两个 base→owner；`F:505–522` move 同样更新 epoch/owner，但会先释放目标原存储。`XHandle` 缓存的是 owner 对象指针与 epoch（`F:3387–3424`），成功替换后可重新解析同一个 owner 上的新根，并非自动变空；它不延长 owner 寿命。旧裸 root/容器/allocator 指针和引用必须声明失效，即使映射地址碰巧相同。`data()` 覆写不会递增 epoch；仅翻 typed flag 不能撤销已借出的引用。失败前若承诺旧值和 handle 不变，应避免对 published owner 做任何早期 swap/move/epoch bump。registry 查找有锁，但不使 payload 并发读写安全；不得跨网络等待长期持有该全局锁。

## 方向二：直接读新的 staging owner

准确目标是 `W1:171–177` 的 receiving owning string，再经 `F:3781–3785` → `init_from_bytes` → `VM:206` 复制 payload。新 staging 直读可设计为保有头 scratch，头/预算准入后取得独立未发布 owner，分块读入 payload；读满、期限与图校验成功后一次性 finalize typed/schema。旧已发布 owner不参与接收，失败可直接保持旧值；失败 staging 在真实完成后回收。

**当前公开 API 不具备安全的 adopt/finalize 接口。** `load_verified(span)` 和 `load_unverified(span)` 都会复制；`bytes()`/`get_buffer()`/`data()` 只提供原始区间。`VirtualMemoryStorage` 属于 detail，`enable_prefix` 与 `XBufferCore::buffer_` 私有；`bind_schema` 也是 `XBuffer` 私有。最小研究改动可在独立副本中做内部 staged factory，复用原完整验证主体，消费已完成、独占的 staging storage，最后只在同一个 commit 点打开 typed access。不能用 `unsafe_root` 或 fresh typed root 规避这个边界；能否形成公开 API 留给真实消费者契约。

**清零是隐含成本。** F `VM:191–208` 的 `from_bytes` 取得 logical=0 的 storage，enable 后用完整 memcpy 初始化，每个 logical byte 只由复制初始化；它不先做 P 字节 memset。若直读使用常规 `VirtualMemoryStorage(P,cap)` 或 `grow_to(P)`，`VM:241–249` 会先清零 P，再由网络覆盖 P；省掉的用户态 memcpy 不能直接算成净节省。内部无预清零 staging 必须保留准确 accessible extent、暂不发布 logical bytes，并证明读满才可验证/暴露，所有失败路径只回收而不暴露未初始化尾部。pool hit 可能含旧字节，完整新读必须覆盖全部将发布范围；部分失败后重试也必须从头完整重载。当前 W1 dirty-pool retry 只证明 F 的完整复制路径，不证明新的直读入口。

可能省掉接收 owning string 的分配/初始清零，以及 string→owner 的一次 payload copy；保留头 scratch、真实 recv/内核复制、短读记账、mapping/enable/页错误、注册、全部结构/逐值验证和处理、输出保存及发送。若仍创建 staging 后再复制成 XBuffer，就没有达成该目标。完整读取使用到 owner 容量小于输入时必须在分配前拒绝；不能先借默认 64 MiB 再声称硬容量一致。

## 真实完成、背压与预算

- W1 真实适配器是非阻塞 syscall + `poll` 的同步循环（`W1:150–177`），`serve_one` 发送的是独立 saved string，owner 发送前已释放（`W1:223–226`）。`Worker` 的 shutdown/join 与 `finish`（`W1:233–248`）提供本实验线程完成边界；它不证明真实异步库取消/回调合约。
- `ReceiveModel` 的 cancel_pending/token/late completion（`frame_model.hpp:156–177,237–246`）是逻辑模型。未来异步读必须保持 staging owner、头/进度/context、socket 与预算直到**真实 pending operation 完成**；收到取消请求或忽略旧 token 不能阻止底层仍向该地址写入。回滚、复用、pool return、析构均须等待该边界。`SendModel:310–384` 只证明有主 string 的模型；owner/spans 发送须另证明 header 与 payload 在真正发送/取消完成前稳定、不可写且有主。
- 若复用 owner 与“减少输出复制”组合，发送背压期间不能覆写、swap 或归池该 owner；单在途限制仅限制请求数量，不自动释放它。必须把该保留时间、背压导致的额外 live reservation、清池/最终释放计入结果。不要将延期回收移到主指标之后当收益。
- W1 的 `4*max_wire`（`W1:259–299`）只是四个 owning wire buffer 的逻辑准入；`frame_model.hpp:125–127` 明确不含 VM owner、页舍入、pool、metadata、header scratch。`owner_capacity` 是每 owner 上限，pool 限额只约束 idle mapping（`VM:92–94`），不是所有 live owner 总预算。复用方案的旧值+spare/backup、直读在途 owner、验证宿主分配须另外列逻辑量、实际 reservation/accessibility 与峰值并准入；“每个 cap 未增加”不等于总容量约束未放宽。

## 最小独立副本边界建议

若后续获准，先保持冻结 F/W1、fixture/process、V1、双 profile、连接、计时边界和现有证据只读。单变量研究副本仅包含两头、所需 frame 支持与新的 server-only `receive + verified load` 适配，保留客户端原接收与输出归档路径，可隔离一次 server 输入复制。客户端也改成 staging 会失去当前 `Sample::output` 原始 owning wire（`W1:245–246,291–293`），应另明确定义保存字节与资源成本，不能把补复制移到计时外而仍套用旧指标。

owner-reuse 与 new-staging-direct-read 是两个独立机制，不宜在首个副本混合。前者在 W1 中缺持久旧值状态，需先由统筹冻结 load-only/whole-operation 提交范围及旧值消费者，再确定两 owner 或回滚方案；后者可在新未发布 owner 内验证，不要求先设计“覆盖已发布旧 owner”的公开 API。两者均仍是前提未满足，当前只建立可审查的成本与寿命边界。
