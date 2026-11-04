# XOffset 编解码一次内存搬运优化方案

日期：2026-11-04。状态：设计，尚未实施或验收性能。

目标是恢复 CppCon 2024 分享所追求的成本结构：数据在最终布局中构建，传输时不逐字段转换，整体换址后不修复相对偏移。建议分两步推进：先用现有 Wire 2.1 和快照接口减少复制与重复准备；再为严格的一次搬运接收引入可证明生命周期的平凡描述符表示。外部输入的完整验证继续保留。

## 1 基线与范围

本方案依据工程绑定的 **Mac mini 当前源码**，不是 Windows 控制目录里的旧 V1 副本。

| 项目 | 本轮核对值 |
| --- | --- |
| 节点 | `mac-mini` |
| 工程 | `/Users/netease/XOffsetDatastructure-20261027/development/next-cpp26-latest-20261027` |
| Git HEAD | `631d6f85520dcd2f40f880a122de38c5dba1dace` |
| 版本文件 | `XOFFSET_VERSION` = `3.1.0` |
| wire | 2.1，40 字节 wire 头，48 字节 arena 头不发送，容器头 12 字节 |
| 工作区 | 分析开始时 `git status --short` 为空 |
| 主头 SHA256 | `c4999e76010d419d231428dd262a329f7ea1d475f9cb4db602572fa8c0c75341` |

已有 `MEMMOVE_SERIALIZATION_DESIGN_20261104.md` 以本地旧版为基线，保留作为历史方案。本文件是面向当前 Mac 源码的后续设计；本地同名文件为审阅副本，不代表两机源码同步。

这里的“只需 memmove”定义为：**在源已完成发布准备、目标已有足够且已提交的存储、schema、ABI 和字节序匹配的前提下，热路径为一次完整数据区复制加固定大小的检查与发布。** 分配、冷页缺页、首次验证、快照打包和业务修改分别计费，并同时报告端到端成本。系统调用内部如何复制不在此承诺内。

## 2 当前已经具备什么

| 现有入口 | 已有能力 | 距离严格目标的剩余成本 |
| --- | --- | --- |
| `save_verified_into(out, {.emission = dense})` | 复用输出容量，连续复制 payload，不逐字段编码 | 检查及逻辑规范化；输出空洞表；空洞表不完整时重新遍历图 |
| `freeze_verified(std::move(buf))` | 接管原 arena；快照持有稳定的头、payload、空洞表 | 首次准备分配 control/空洞表并完整验证；准备后重复 `segments()` 无图遍历 |
| `VerifiedSnapshot::segments()` / `IoLease` | 三段发送，库内不复制 payload | 传输层须持有 lease 到真实完成；跨段拼成连续 wire 仍需复制 |
| `reload_verified` | 复用目标存储、owner 登记与验证 scratch | 完整图验证、空洞表装载及必要的页面处理 |
| `begin_verified_reload` | 传输直接写入目标 payload，省去中间 wire 到 arena 的复制 | 完整验证仍执行；不能据此称接收全过程 O(1) |
| `begin_verified_receive` / `finish_verified` | 原始字节验证后建立对象生命周期，再发布 typed owner | 对非平凡容器头和动态数组进行恢复，成本随结构规模增加 |
| `clone` / `clone_into` | 同进程可信对象整块复制，无 wire 验证 | 复制宿主空闲表可能分配并随空洞数增长；不能作为任意网络输入的可信加载器 |

当前 `shrink_to_fit()` 只缩逻辑长度，不做对象压缩、宿主 realloc 或整块搬迁。不能把它当作主要性能问题。当前验证已使用游标/位图和可复用 scratch，旧 V1 的两棵临时 `std::map` 结论也不能套用。

Wire 2.1 的稠密输出为：`40 字节 header + payload[48, used_bytes) + 8 × hole_count 字节空洞表`。即使 payload 一次复制，遍历/写出空洞表仍是 O(H)。因此“整个 wire 一次 memmove”与“payload 一次复制”是两个验收项。

## 3 路径选择与成本模型

令 B 为已用 payload 字节数，N 为需要遍历的结构节点数，H 为空洞数，A 为需要显式恢复的对象/数组数，R 为同一快照的消费次数。

| 路径 | 首次准备 | 每次发送或接收 | 使用条件 |
| --- | --- | --- | --- |
| 三段快照发送 | 验证 O(N)，准备 O(H) | O(1) 取 span；库内 payload 零复制 | 不可变 owner，传输支持 scatter/gather |
| 稠密连续输出 | 准备头和空洞表 | 一次 payload 复制 + O(H) 表复制 | 需要独占连续 wire |
| 缓存连续 wire 后重复发送 | 一次打包 O(B+H)，另占一份 wire | 整条 wire 一次 `memmove` | 同一内容重复消费足以摊销准备 |
| 当前 verified 接收 | 准备/复用目标和 scratch | 复制或直接接收 + 完整验证 + 空闲表恢复 | 外部字节，保持现有保证 |
| 当前安全 raw receive | 接收前存储准备 | 完整原始字节验证 + O(A) 激活 | 需要现有 owning 容器及 typed 引用 |
| 新平凡表示的可信接收 | 源有效性/布局证明，目标预备 | 一次数据区复制 + O(1) 发布 | 第 5 节的受限类型域及可信来源 |

统一报告 `T_total = T_prepare + R × T_hot + T_first_read + T_mutation + T_cleanup`。对首次收到的新外部消息，完整验证必须计入；相同不可变快照的验证结果才可复用。带宽只说明复制部分，不替代整个流程的延迟。

## 4 第一阶段 使用现有表示减少热路径工作

### 4.1 发送端

先将三种用途明确路由，不改现有默认 emission：小消息/本地连续输出显式选 `dense`；同一快照广播复用 `freeze_verified` 与 `IoLease`；带宽敏感的跨机传输保留 `free_runs`、`zero_runs` 或 compact，按端到端结果选择。

新增接口应建立在现有 `VerifiedSnapshot<T>` 上，不再设计一套 snapshot owner：

```cpp
// 拟新增接口，名字及参数是设计草案。
template<class T>
std::size_t dense_wire_size(const VerifiedSnapshot<T>& snapshot);
template<class T>
void emit_dense_into(const VerifiedSnapshot<T>& snapshot,
                     std::span<std::byte> destination);
template<class T>
PackedWire<T> pack_dense(const VerifiedSnapshot<T>& snapshot);
template<class T>
void copy_packed_into(const PackedWire<T>& packed,
                      std::span<std::byte> destination);
```

`emit_dense_into` 在已经准备好的三段上写 header、复制一次 payload、复制已连续保存的空洞表；不遍历对象、不查 owner、不规范化源、不重新验证。它仍可能有三次块复制，不能宣称整条 wire 只有一个调用。`pack_dense` 首次把三段合为独占连续 wire，之后 `copy_packed_into` 才只有一次整条 wire 的搬运。

单次发送优先直接分段或直接写入最终输出，避免为了凑成单次函数调用先多复制一遍。打包缓存按显式请求创建，生命周期与快照相同，不引入无上限全局缓存。输出别名、长度溢出及容量不足在写入前检查；禁止与快照的 arena、header、空洞表重叠，以免破坏不可变源。

对已提交的热目标，新增输出路径不逐页 `madvise`，与裸复制使用相同的页面条件。当前大输出的分块预取策略保留在原入口，作为独立候选对照后再决定是否调整。普通 `std::string::resize` 可能先清零；应使用现有 `resize_and_overwrite` 或调用方已准备的可写 span，不能产生“先清零整块、再复制整块”的额外写流量。

### 4.2 接收端

现有不可信入口继续走完整验证。先用复用型 `reload_verified` 消除每条消息的存储和 scratch 分配；传输适配具备完整生命周期管理时，用现有 direct receive 消除中间复制。两者都复用现有检查集合和失败语义。

冻结后的同进程只读消费直接持有 `VerifiedSnapshot<T>`，无需重新序列化和加载。同进程需要独立可变副本时使用 `clone_into` 作为对照，同时单独统计空闲表复制的节点、分配和时间，不把它标为严格 O(1) 发布。

`freeze_verified` 仍要求所有可写别名停止使用。移动源对象和移除宿主 owner 不能撤销以前返回的 `T&`、元素指针或 span；epoch 也不能拦截经旧指针直接写入。第一阶段继续采用明确的独占/静默期合同，新接口持有 owner 和 I/O lease；不把这种合同描述为运行时强制不可变。

## 5 第二阶段 使接收发布真正接近一次搬运

### 5.1 当前 owning 表示的限制

完整结构验证和 C++ 生命周期是独立问题。`memcpy`/`memmove` 可以隐式创建满足条件的对象，但不会为所有非隐式生命周期的子对象自动开始生命周期。当前 raw receive 因此显式重建 owning 容器头及数组；仅删除验证调用或 `reinterpret_cast<T*>` 不能替代这些工作。

目标应分成两个明确产品合同：现有 `TypedXBuffer<T>` 保留其 owning、复制/移动、修改与销毁语义；新增一次搬运类型域不承诺自动获得这套语义。**不以 `is_v1_wire_admitted_v<T>`、TypeLayout 的字节安全性或 `std::is_trivially_copyable_v<T>` 单项判断为准入证明。**

### 5.2 平凡描述符与外置 owner

严格快路径原型采用如下表示：

```cpp
// 拟新增表示；不替换现有 XFixedVector / XFixedString。
template<class T>
struct RelSpan32 {
    std::int32_t data_delta;
    std::uint32_t size;
    std::uint32_t capacity;
};

struct ImageOwner;        // 存储、寿命、类型/布局绑定、可选分配器在 payload 外
template<class T> struct ImageView;
template<class T> struct ImageEditor;
```

`RelSpan32` 只保存相对位置和长度，没有用户析构、深复制、绑定 owner 或回收动作。根、记录、容器描述符及其递归元素均必须通过 implicit-lifetime、平凡复制、标准布局及无重叠字段的联合准入；初版排除原始指针/引用、继承、union、虚函数、自定义激活、const/volatile 存储成员及外部资源。

一次完整搬运同时移动描述符和其指向的所有数据，rel32 数值不变。实现前必须完成整块创建的逐类证明：根和嵌套对象、每个动态 `T[capacity]` 数组、活元素、空容量、spare slots、对齐、指针来源及后续销毁。尤其不能用可能解释未用 bool/enum 位模式的 API 草率启动整个容量区。只有这份证明和所用工具链能力核对通过，才开放复制后直接形成 typed 引用的分支。

若数组/对象创建证明无法覆盖目标平台，提供同一 wire 表示上的只读 byte proxy：用边界已证明的偏移和固定大小 POD 读取访问字段，不把字节冒充现有 owning `T`。代理访问成本单独测量。它是显式接口选择，不能在 typed API 内静默切换实现或承诺。

资源释放归 `ImageOwner`。记录复制是描述符的浅值复制；业务深复制、删除子图、扩容和嵌套回收只能通过 `ImageEditor` 显式执行。旧版直接赋值、自动 RAII 退还嵌套存储等行为需要适配，不能用相同类型名掩盖差异。此改造与收发优化分别验收。

### 5.3 信任与接收流程

可信源初版只接纳同进程受控构建并持有不可变 owner 的镜像，或由完整验证器对指定不可变字节产生的凭证。凭证绑定类型、布局、长度、存储身份和寿命；不提供任意 `span<byte>` 加 `trusted=true` 的入口。

跨进程/跨机的任意新字节继续完整验证。若以后支持受控服务间免结构验证的生产者协议，必须另行定义完整不变量、版本协商及传输完整性；认证身份本身不能证明偏移和容器结构合法。该协议不作为本阶段的隐含前提。

```text
受控镜像或已验证且不可变的镜像
    -> 类型/布局/长度/目标容量固定检查
    -> 准备目标并撤销其旧 typed 访问
    -> 一次 memcpy/memmove 覆盖完整数据区
    -> 固定大小 owner/视图发布
```

复制成功后无需图遍历、逐容器激活、offset 修复、复制宿主空闲树或清零整个目标。复制前所有可能失败的准备完成，提交阶段无分配、无用户回调、无抛异常。目标复用期间的旧引用失效；禁止源/目标重叠及写源。失败不得发布部分根，复用型 API 沿用失败后目标为空的合同；需要保留旧状态时由独立 staging owner 完成后交换。

拟提供 `copy_image_into(const SealedImage<T>&, PreparedImageSlot<T>&)`：`SealedImage` 只能由受控 builder 或完整验证器在新类型域中产生；`PreparedImageSlot` 持有对齐存储和容量，返回视图持有目标寿命。没有无凭证的原始 span 重载。若沿用 Wire 2.1，payload 仍落在目标 arena 的偏移 48，头部在固定位置建立；不能把 40 字节 wire 头后的连续 string 直接当作 arena，绕过对齐和存储协议。

第一版只读 owner 不恢复可变分配器。后续 `ImageEditor` 可延迟建立空闲表，但首次编辑的 O(H) 工作必须计入修改和端到端；立刻编辑的业务不因延迟计费而被认为减少了总成本。重复使用目标的脏尾部要沿用“暴露前清零”的机制，不能留下旧内容，也不能为了省掉本次 memset 在首次访问时读出旧数据。

### 5.4 格式与迁移

第一阶段继续 Wire 2.1，稠密新输出与现有 dense 输出逐字节一致。新旧读写互通只针对同一 schema、同一 emission 比较；不能要求 dense 与默认 sparse 的 wire 字节相同，应比较加载后的值与 payload。

第二阶段使用独立类型名和 schema/表示标识，拒绝把 descriptor 镜像重新标成旧 owning 类型。若保持当前块布局、覆盖规则和头部编码，可以研究 Wire 2.1 的独立 schema；若省略空洞表、改变覆盖或对齐规则，则另立 wire 版本并显式协商。版本选择在原型证明后决定，首阶段不改 wire。

## 6 验证与性能验收

先建立新目录 `experiments/memmove-serialization-20261104/`，冻结源码、编译器/标准库、编译参数、schema、页面条件和输入。实现及测量输出不写入已有 SLG 冻结树或历史档案。当前版本性能文档作为场景参考，本轮没有新计时结论。

| 验收面 | 必需覆盖 |
| --- | --- |
| 同等工作量基线 | 现有 dense save/reload、快照三段、direct receive、clone_into，以及同尺寸预分配 memcpy/memmove |
| 消息与形状 | 128 B、1 KiB、4 KiB、64 KiB、1 MiB、16 MiB；固定 B 改变 N/H；无洞、碎片化、嵌套、空容器、SSO、spare capacity |
| 生命周期 | 首次准备、重复 1/10/100 次、源销毁、首次读、接收后修改、清理；SLG A/B/C/D 的实际复用方式 |
| 页面与存储 | 热页/冷页分开；目标未提交/已提交分开；VM/钉住槽位/可搬槽位分别记录，不额外强制原入口失去其存储策略 |
| 操作量 | 复制总字节和次数、memset/扫描字节、验证节点、激活头/数组、空闲表节点、宿主分配、系统调用、页故障 |
| 资源 | wire 字节、虚拟预留、实际提交、阶段峰值、缓存额外内存；区分整进程 RSS 与库独占内存 |

“一次搬运”有结构门槛：热路径仅一次完整数据区复制；没有随 N/H/A 增长的额外遍历、分配、恢复或元数据重建。相同 B 改变结构规模的诊断必须验证这一点。连续 wire 的单次复制门槛单独用于已打包 `PackedWire`，初次 pack 不能排除出单发端到端数字。

建议预登记的性能目标：对热页、容量已备好的 64 KiB/1 MiB/16 MiB 候选，配对进程统计的 `T_fast / T_same_size_memmove` 单侧 95% 上界不超过 1.10；小消息报告绝对纳秒差，拟定额外开销不超过 50 ns。阈值是待实现目标，不是当前结果。A/A 必须先证明噪声足以分辨目标差异；若未满足，本窗口结果记为不确定，不追加样本直到过关。

与原路径同合同的 SLG 端到端比较，候选/原版单侧 95% 上界拟不超过 1.05；同时报告首次使用和稳态。新 descriptor/proxy 合同先报告收益与业务适配成本，不用其免验证结果给 verified API 宣称加速。计时窗口仅一个执行者，不与编译、重测试或归档竞争。

正确性复用并扩展 `test_verified_receive`、`test_receive_raw`、`test_clone`、`test_core_lifetime`：覆盖 golden bytes、新旧读取、schema/ABI/版本拒绝、重复消费、容量/溢出/对齐、别名、过期凭证、lease 的完成/取消、传输中断、OOM、失败不发布、接收后修改及源销毁。

外部验证入口继续运行截断、坏 offset、重叠、错误长度、bool/排序、空洞覆盖、稀疏 gap、资源上限及 fuzz 回归；比较完整拒绝集合和必要的首错顺序。ASan/UBSan 和跨编译器运行支持正确性判断，不能替代 C++ 对象生命周期证明。

## 7 实施顺序与停止条件

| 阶段 | 交付 | 通过条件 |
| --- | --- | --- |
| P0 基线 | 当前源码成本分解、操作量计数、A/A 能力、生命周期类型矩阵 | 确认实际热点和类型闭包，历史与当前数字不混用 |
| P1 现有发送路径 | 复用 snapshot 的 dense span 输出；按需 PackedWire；最终输出缓冲适配 | Wire 2.1 字节兼容，单次 payload 复制/重复发送零遍历，lease 通过 |
| P2 现有接收路径 | 业务适配 reload/direct receive，优化有证据的验证和空闲表热点 | 检查集合、错误及失败合同保持，端到端包含首次成本 |
| P3 严格镜像原型 | 平凡描述符类型域、对象/数组证明、受控来源 copy/view、显式编辑适配 | 没有逐对象恢复，可信接收只有一次数据区复制和固定发布，性能门槛通过 |
| P4 业务取舍 | SLG 场景验证、缓存预算、类型/格式迁移说明 | 明确哪些场景采用现有 owning，哪些采用镜像，结果可复现 |

P0/P1/P2 可以在当前表示上带来收益。若目标是现有任意 owning 类型从任意外部输入加载后仍有完整验证和全部修改语义，不能承诺整个接收只做 memmove。P3 若无法证明生命周期或业务不接受表示/API 调整，应保留 P1/P2 的结果，明确严格目标仍未完成。

## 8 源码依据与参考

以下行号均对应第 1 节 **Mac HEAD**，不对应 Windows 本地旧主头：

| 位置 | 本方案使用的事实 |
| --- | --- |
| `xoffsetdatastructure.hpp:562` | shrink 仅变更逻辑长度 |
| `xoffsetdatastructure.hpp:1932` | clone 复制宿主空闲表 |
| `xoffsetdatastructure.hpp:5850`、`:6258` | Wire 2.1 头和 dense/sparse 写出 |
| `xoffsetdatastructure.hpp:7357`、`:7418`、`:7465`、`:7628` | load、复用 reload、direct reload 和完整验证发布 |
| `xoffset_verified_paths.hpp:20`、`:58` | 发送准备、输出容量复用和空洞表修复 |
| `xoffset_verified_paths.hpp:791` | 快照、三段、IoLease 和首次 freeze 验证 |
| `xoffset_verified_receive.hpp:262` | 原始验证、激活、typed 发布的顺序 |
| `receive_raw.hpp:765` | owning heads、容量数组和嵌套元素的激活 |
| `docs/V2_FORMAT.md`、`docs/BUFFER_REUSE.md` | 当前格式、复用、失败和失效合同 |

辅助头 SHA256：`xoffset_verified_paths.hpp` = `7e7ebb0f47eb200e708403e1fa22011b368e156a163b5b1765f4e7c5b96349f9`；`xoffset_verified_receive.hpp` = `af433508e9878d98b306da7485ecf7279f8a3b67d1b1fe66ab36dc3fafa9b30b`；`receive_raw.hpp` = `cdb28a6a58c1ea701baa850537ea22da80495736756d73050c47cef39fe8386d`。

原始主题见 [CppCon 2024 官方材料索引](https://github.com/CppCon/CppCon2024/blob/main/README.md)中的 Fanchen Su 分享及其[幻灯片](https://github.com/CppCon/CppCon2024/blob/main/Presentations/Using_Modern_Cpp_to_Build_XOffsetDatastructure.pdf)。本文件中的 API 与架构为本轮设计，不是对演讲内容的逐项转述。

生命周期依据见 C++ 工作草案 [cstring.syn](https://eel.is/c++draft/cstring.syn)、[intro.object](https://eel.is/c++draft/intro.object)、[basic.types.general](https://eel.is/c++draft/basic.types.general) 与 [obj.lifetime](https://eel.is/c++draft/obj.lifetime)。草案规则需与项目实际采用的标准版本和工具链逐项对照。
