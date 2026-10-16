# N7 并行前置：文件 I/O 参考消费者与实验准备

日期：2026-10-15。用户确认与 N6 并行推进 N7 前置及轻量平台准备。
本工作独立于正在运行的第三阶段：只新增 `experiments/vm_io/` 与本文，不修改生产头、
公共 CMake、N6 工具或任务总表。N6 继续负责分配器和最终代码整合。

后续状态：N6 已完成并保留 F；父会话接管后续本地实验调度。用户选择网络收发为
实际目标，但消息大小、同步/异步和协议/网络库均未定。下面的文件参考保留为合成
对照，不替代网络或业务测量。新的计时/分段诊断构建分离与固定矩阵见
[measurement-preregister.json](../experiments/vm_io/measurement-preregister.json)。
该矩阵现已执行，功能与离线审计通过，但四尺寸 A/A 全部超过噪声门槛；结果见
[文件对照筛选](VM_IO_SCREENING_RESULTS.md)。网络后续见 [网络研究计划](VM_NETWORK_PLAN.md)。

## 已确认的问题与范围

已有 `tools/bench_vm_compare.cpp` 的 load 先读取文件，再开始 load_verified 计时；
`tools/bench_vm_improvements.cpp` 也把文件读取和写出放在 load/save 计时之外。
现有示例主要是内存往返。这些数据能评价库内成本，不能证明一次完整文件或网络请求
能从省略某次复制获益多少。

先建立同步、可定位的普通文件参考消费者：

```text
同一固定 V1 输入文件
  → 打开/读入/关闭
  → load_verified 创建拥有型 owner 并校验
  → 相同的确定性数据处理
  → save_verified 规范化并生成拥有型字符串
  → 写入新临时文件/关闭
  → 释放 owner
  → 计时外逐值读回核验，再发布输出文件名
```

这不是用户业务 trace。当前没有业务调用方、消息分布、延迟目标和吞吐目标，因此
暂不能作 N7 生产 API 采用结论。后续可在同一实验框架换入实际 schema 和输入。

本轮保留所有输入/输出复制与原有 V1 格式，不新增 load_into、外部借用输入、
直接读 arena 或分段保存接口，也不引入新传输协议。N7 的前置工作可并行；生产优化
原型需先有成本证据，最终采用结论需在 N6 选定的版本上重新确认。

## 实现与指标

参考程序位于 [io_pipeline.cpp](../experiments/vm_io/io_pipeline.cpp)。根对象包含
marker、uint32 vector 和 string，schema 名为 `n7.reference.file-pipeline.v1`。
输入生成和处理均可确定重放；每个输入/输出都逐元素验证，checksum 只是辅助标识。

三种模式保持相同 wire、硬上限、处理和验证：

| 模式 | 空闲 owner 状态 | 单独记录的开销 |
|---|---|---|
| none | 不使用池 | 创建和释放在主流程内 |
| cold | 每次处理前显式清池 | 清池时间在主流程外另报 |
| warm | 先完整预热一次，再重复使用 | 预热时间及命中/未命中与正式迭代分开 |

主计时覆盖上述读取至 owner 释放。阶段计时含探针开销，所有结果均标
`diagnostic=true`、`timing_evidence=false`，仅供确定正式实验边界。host 字符串析构、
读回验证、文件名发布、pool 准备和最终 clear 均在主计时外，不能把此数值说成应用
全部工作耗时；输出 JSON 明确列出这些排除项。

文件缓存不受控，预检、读回及预热会影响后续缓存状态。cold/warm 指 pool，不代表
冷盘/暖盘。普通 stdio close 不代表持久化完成，本轮没有 fsync；未来涉及持久化需求
应单列同步写入实验。输出采用同目录 `.pending` 加排他创建和硬链接发布，要求文件
系统支持硬链接；失败不出现完成命名，不覆盖旧文件，不作崩溃一致性保证。

程序提供三个入口：

```sh
io_pipeline fixture --output NEW_FILE --elements 257
io_pipeline run --input FILE --out-dir NEW_DIR --pool warm --iterations 2 --max-frame 1048576
io_pipeline smoke --out-dir NEW_DIR
```

`run` 是探索性参考流程，不是正式性能实验入口。当前仅用 smoke 验证功能，所有路径
在新输出目录中，输入长度在字符串分配前检查。初版实际编译环境限定为 macOS P2996；
不得据此声称 Windows 或 Linux 上的 N7 消费者已通过。

## 故障与生命周期

smoke 设计覆盖三种池模式、成功的短块读写重试，以及十个拒绝场景：短读、读取消、
读失败、写入无进展、写失败、坏头、截断 wire、超出帧上限、既有输出和既有临时文件。
故障来自参考消费者 I/O 适配层；按预期失败原因和已发生的部分读写字节确认注入生效。
不是用系统磁盘耗尽模拟错误，也不冒充 OS 故障或新的库 API 保证。

本消费者只在 load_verified 成功后访问 root，处理新 owner；失败通过 RAII 清理本次
拥有的临时输出，原始输入保持不变。它没有替换现有存活 owner，所以尚不能决定
将来的 load_into 是否保留旧值、如何使旧 handle/span 失效。

后续每个生产候选必须单独回答：

| 候选方向 | 必须先确定的契约 | 必须包含的对照 |
|---|---|---|
| 复用 owner 的加载 | 旧值/引用/epoch 在部分读取或校验失败后的状态 | F 无池与池化加载 |
| 直接读入新 staging owner | 输入上限、取消/短读、完整校验后发布、失败释放 | 当前有界读 string＋verified load |
| 减少输出副本 | 相同规范化 wire、头部拥有权、发送期间冻结/寿命 | 当前 save_verified＋完整输出 |

原用户排除了新的稀疏 wire 和分段协议；后续任何减少输出副本的研究都须保持该边界。
异步网络、外部只读输入和快照不在这个同步文件参考程序的验证范围内。

## 冻结、复现与资源协调

[run_io.py](../experiments/vm_io/run_io.py) 从第二阶段不可变归档逐文件校验后提取 F 两头，
同时冻结本次消费者、预登记清单和 runner。它不读取 N6 正在变化的生产头作为基线。
`prepare` 离线完成；构建核对固定 P2996、SDK、运行时和依赖。每次输出必须是新目录。

```sh
python3 experiments/vm_io/run_io.py prepare --out build/n7-prepared
python3 experiments/vm_io/run_io.py build --prepared build/n7-prepared \
  --out build/n7-build --lease build/parallel-coordination/n7-window.json
python3 experiments/vm_io/run_io.py smoke --build build/n7-build \
  --out build/n7-smoke --lease build/parallel-coordination/n7-window.json
python3 experiments/vm_io/run_io.py audit build/n7-smoke
python3 -m unittest discover -s experiments/vm_io -p 'test_run_io.py' -v
```

前置阶段共享执行窗口由 N6 负责人在不计时阶段授予，格式包含 `granted: true`、带时区的
`expires_at` 和 `finished: false`。build/smoke 检查剩余窗口足够执行有界命令；过期或
未授予即拒绝。执行后主会话写 finished=true 归还窗口，不自行延长授权时间。
这个约定依赖两个负责人合作，不是跨进程强制锁，也不适用于脱离本机的正式测量调度。
N6 完成后，父会话使用新的协调记录串行安排本地构建和测量；不延长已归还的 N6 窗口。

证据状态与文件清单显式校验，普通 Python 和 `python -O` 都执行校验；失败、中断、
输入变化或缺失不能标通过。准备/构建/功能验证与性能采用证据分别命名。

## 后续顺序与验收边界

1. 本轮完成参考消费者、冻结/运行/审计工具、针对性失败检查和 Windows 验证准备。
2. 明确实际消费者及目标；若仍无业务数据，继续将该流程标为合成参考。
3. 将文件完整流程的无池/冷池/暖池测量加入统一队列，冻结相同输入和计时边界，
   先做 A/A、记录环境，再逐段判断复制、映射、校验和 I/O 的占比。
4. 仅为有端到端收益空间的方向制定生产 API 和失败/寿命契约，实验前登记采用预算。
5. N6 采用结果整合后，候选在新的冻结源码上复测，最终兼容/安装/sanitizer 验收统一执行。

## 本轮执行结果

原生 macOS arm64 的固定 P2996 工具链编译通过；257 个元素的 smoke 通过 **14 项
功能检查**，含三种池模式各两次、成功的短块读写和十个失败场景。运行器 **17 项
Python 检查**通过，冻结输入、构建和 smoke 的完整性审计均通过。
独立复审后补强了检查集合、运行器身份、父结果及执行回执校验。

一次过期窗口正确拒绝启动编译；随后在 N6 授予的新窗口内完成构建与功能验证并归还
资源。没有进行正式性能测量，未修改生产 API。

可移植的日志、冻结源码、二进制、输入/输出和审计清单见
[验证证据](validation/2026-10-15-vm-io-prestudy/README.md)。Windows 并行交接项见
[Windows 原生验证准备](VM_WINDOWS_HANDOFF.md)，其原生执行仍待完成。

不能将准备完成标为 N7 已完成或路线图整体交付。共同状态表由 N6 负责人整合，
避免两个会话同时修改任务清单。
