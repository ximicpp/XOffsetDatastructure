# M1 v2 原始 driver 代码生成独立审查

日期：2026-10-19，北京时间。审查者：`plan_delivery_review`。本审查只读取已有源码、汇编及回执，运行标准库只读哈希/文本检查；没有调用编译器、项目原生程序、sanitizer 或测量，没有创建工具或取得执行租约。仅写本文件。

## 结论

**未发现阻断此次代码生成机会准入的问题。可以继续已登记的定向正确性与固定 M 机制验证；不能据此进入 S3、集成默认 C14，或宣称性能改善。**

原始完整 driver 实际生成的 uint8/uint32 fresh suffix 循环确实逐元素调用 `binding()`，该 callee 内仍有实际 `canonical_bytes_at` 调用。候选将这段循环换成一次由标准值初始化算法生成的 `memset`。这不是只比较包装函数长度，也不是从源码中的 lambda 推测编译器行为。

尚未由本审查证明：固定 M 的实际 fresh 命中次数、被删除工作的周期占比、完整功能通过、端到端阶段收益或竞品目标。它们各自保留后续门禁。

## 实际产物与身份

审阅的 WSL 产物根是 `/root/xoffset-c14-iteration/20261019/v1/validation/m1-v2/codegen/`。读取了 `INPUTS.json`、两份完整 `.s`、`commands.json`、`REVIEW_REQUIRED.json`，以及本轮 validation 的 admission/result/dispatch、staging receipt、codegen-exit 和原租约最终 result。

| 内容 | 独立核对结果 |
|---|---|
| control 生产源码 | `/root/xoffset-receive-completion/20261019/v1/final-v4/source`，14 文件逐一重算，匹配记录；main SHA256 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3` |
| candidate 生产源码 | validation-owned `m1-v2/staged/source`，14 文件逐一重算，匹配 v2 记录；全154成员还与 `M1_IDENTITY.json` 独立逐一核对 |
| candidate main | SHA256 `50fc517ad0978b2a4961848870a7d7f1f2d670e6a9e7b9468b687d5622fc9280` |
| candidate lifetime helper | SHA256 **`34d961bf7bb0cc4048f8403ef7b38618c9b124ba7d1866952aba638be6f32b09`**；不是 v1 的 `125293c8919dc2b360a83fb8dd06f1b860f98495f93f5a6f76c156f64d3c7fc3` |
| control 汇编 | 1,779,270 B，SHA256 `110bc1bc494a1d731c563660d5760a630d5bb0da39531eb6e43e83bd04479640` |
| candidate 汇编 | 1,777,770 B，SHA256 `8948580e7ae5ae87f5fdde77c1185c026ee776bd5bf0ed025d8a48c005ed205b` |
| 工具与 profile | 记录中的17项工具、编译器、wrapper、loader、runtime 文件逐一重算，全部一致 |
| 原实验输入 | 记录中的62个 experiment 文件逐一重算，全部一致；包含完整原 driver、原 adapter、views/model 等 |

原 `driver.cpp` SHA256 为 `11aa76107eedb0fcdc1ffcf79893d2cdaf2a844f95f9a49d8682bbc434b80380`，原 XOffset adapter 为 `f1901685bd78e0348d63ab129749272aebb3d71c39538933eb60b86f55073578`。两份编译命令与保存的 `commands.json` 及 result 完全一致，使用同一实际 P2996/libc++ wrapper、`-std=gnu++26 -freflection -fexpansion-statements -stdlib=libc++ -pthread -O3 -DNDEBUG -S`；没有 `-UNDEBUG`、诊断宏或独立微基准替换。两角色仅按原设计选择各自源码 include 路径和输出文件，vendor 路径相同。

实际 compiler SHA256 为 `c7fa55437d34c43201da37960251d7d3dab1e06fc92273efe6f6d88f6dae7c6e`。已读取两层 wrapper，确认 libc++ include、resource-dir、loader/library-path 的实际转发；没有把61字节 shell wrapper 的哈希误称为编译器本体身份。Windows UNC 不能直接跟随 Linux 的 compiler symlink，因此工具链哈希改由 WSL 系统 Python 标准库只读计算，未运行该 compiler。

原 lease 记录 run_id 为 `993e2f9853794161be8db186247d31c2`，build 命令退出0，cleanup 的 live/process_groups/zombies 均为空。两条 `-S` 命令各退出0。它证明该次已有编译任务的完成，不是本审查的新执行或 native/performance 结果。

## 实际调用闭包

以下行号均为已核对 SHA 的汇编文本行号。

1. 原 timed `run` 在两角色第6474行调用 `XOffsetAdapter::mutate`。
2. control 的 mutate 在41107行调用 `assign_record`，41152/41201/41318行直接调用 uint8 `assign_sequence`；candidate 有对应调用，未改变业务算法。
3. `assign_record` 在两角色13034行调用 uint32 `assign_sequence`，13038/13087行调用 uint8 版本。
4. 两角色 `assign_sequence` 在24489/24618行调用对应 `resize_default_scalar`；control 在25409/26163行继续调用原 `resize_construct`，candidate 则调用 `<true>` 实例。
5. fresh suffix 的 callee 内部存在下表所列真实变化。

| 实例 | control fresh 区段 | candidate fresh 区段 |
|---|---|---|
| uint32 | 25737–25763，`.LBB128_28` 循环：每元素 `callq ...XFixedVectorIjE7bindingEv` 后 `movl $0` | 25734–25745，计算 `(count-old_size)*4` 与 `fresh+old_size*4`，一次 `memset@PLT` |
| uint8 | 26491–26517，`.LBB131_28` 循环：每元素 `callq ...XFixedVectorIhE7bindingEv` 后 `movb $0` | 26468–26478，计算 `count-old_size` 与 `fresh+old_size`，一次 `memset@PLT` |

进一步读取 binding callee，而非只计这一个 call：uint8 函数从20566行开始，uint32 从20722行开始。对非零、非 segment-tag 的普通 allocator-state binding，实际依次恢复 allocator-state、再次恢复该 state、恢复 state 的 segment；该分支上存在三次 `canonical_bytes_at` 调用。tagged segment 与空 binding 分支保持原来的不同闭包，不能把“三次”宣传为全部调用的固定次数。

`canonical_bytes_at` 在19299行读取 active count，19301行在零时转向返回。**旧普通 copy-load demo 不因此逐元素进入 map/锁；常见零 active-owner 情形仍承担真实函数调用、保留寄存器、count load 和分支。** 本机会成立的理由是实际重复调用被移除，不是预设原子 load 或 registry 锁占用了多少时间。

## 初始化、事务与未变化路径

已对照实际两文件源码 diff 及 fresh 汇编的前后路径：

- 仅 `default_scalar_bulk_supported<T>()` 可选择 `<true>`；v2 显式排除 `future_custom_lifetime_supported<T>()`，并排除 activation-only、future-only 等任一 enabled frontier。泛型合法 scalar/enum 自定义调度不能因 `is_scalar` 被绕过。
- 源码使用 `std::uninitialized_value_construct_n`，没有加入通用手写 `memset`。本 profile 的 uint8/uint32 被编译器合法降为零字节初始化，不构成对 pointer/member-pointer 等其他 scalar 表示的假设。
- fresh 的分配、whole-capacity array lifetime 准备和 rel32 范围检查仍在 suffix 初始化之前；初始化仅覆盖新 suffix，没有跳过 resize 后 copy 之前的必需值初始化。
- 两份 fresh 汇编在 suffix 之后仍复制旧 prefix，然后写 data_delta/size/capacity，最后调用 `retire_dynamic_bytes`。旧 prefix 未被批量清零；分配或 rel32 失败的异常路径仍保留。
- capacity/general construction 区段仍可见逐元素 binding 调用。该观察防止把局部 fresh 优化误写为消除了所有 resize 工作；原 same-address tail 路径和其他公开调用没有在本补丁中被替换。
- 没有引入新的跨回调 data/owner 缓存或放宽 canonical lifetime。本审查不新认证原有 custom scalar capacity/tail 行为，也不代替实际 OOM、new_handler、Core move、receive/canonical 反例测试。

## 下一道门与停止条件

准许继续登记的定向 normal/SAN 与固定 M control/candidate 各一次机制轨迹。它们仍需证实 fresh elements/命中阶段、移除的实际 binding 次数、原九阶段 canonical/query/wire 真值及不变化路径一致；不能用本汇编里的静态循环存在代替动态命中。

若固定 M 没有足够命中、语义或回调/lifetime 反例成立、计数不守恒、或新增工作抵消机制机会，则关闭 M1，不为此启动72对筛选。进入 S3 前，另须完成本候选真实闭包的完整业务/V1/消费端等既定门禁与冻结统计/资源协议。

后续任务必须继续绑定**完整 v2 两个变更文件及14文件闭包**。`run_m1.py` 内部仅比较 main 哈希的条件不能单独区别 v1/v2；它只能在外层 validation admission 已校验完整输入与 staging 身份的前提下使用。两版本 main 相同，不能仅凭 main 哈希复用本准入。

本报告没有根据汇编指令数、文件大小、编译用时或 `memset` 次数推断速度，也没有给出相对自身或四竞品的性能结论。
