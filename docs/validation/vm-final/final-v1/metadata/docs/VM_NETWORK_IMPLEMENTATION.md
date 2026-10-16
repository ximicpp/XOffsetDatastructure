# N7 TCP 回环参考消费者

日期：2026-10-15。W1 只写入独占实验、实现说明和新验证目录；生产双头保持 F。
本实验使用 POSIX IPv4 TCP 本机回环、复用一条连接、一次一个请求。它不选择生产协议、
网络库、业务尺寸或恢复语义；可注入完成状态模型不等同于真实异步库验证。

## 契约与源码

[contract.json](../experiments/vm_network/contract.json) 冻结 CLI、JSON、编译 profile、
限额、计时边界和精确测试期望；当前合同 gate 状态见
[handoff.json](../experiments/vm_network/handoff.json)。根类型仍为 `IoReferenceRoot`，
schema 名、fixture 值、标记、标签、变换和逐值核验复用只读文件参考。
帧仍是原有 88 字节 V1 头加 `used_bytes` payload，未加入长度前缀或新字段。

实现分为：

- [帧状态模型](../experiments/vm_network/frame_model.hpp)：头部准入、逻辑资源账本、
  分次读写、取消 pending、迟到/重复完成及拥有型输出；由显式 completion 驱动。
- [模型测试](../experiments/vm_network/frame_model_tests.hpp)：18 组、163 个固定子案例，
  每项记录预期和实际原因、进度、接收缓冲物化、发布、终结次数及资源余额。
- [参考程序](../experiments/vm_network/net_reference.cpp)：有界非阻塞 socket 加 poll 的
  同步适配器，后台线程只负责服务端同步请求处理；没有生产异步 completion 适配器。
- [冻结与验证入口](../experiments/vm_network/run_network.py)：准备、双 profile 构建、
  功能 smoke、独立 sanitizer 和可搬迁只读审计；无正式计时矩阵命令。

## 准入、拥有关系与截止时间

读取头时只使用固定本地对象。头经过 magic、版本、大小、endian、flags、类型、schema、
保留字段和 offset/length 检查，再作单帧及 owner 上限检查，之后才分配 owning string。
`reserved_bytes > used_bytes` 的合法历史表示仍接受；显式 owner cap 的语义与 F 一致。
完整图仍由 `load_verified` 验证，只有成功后才能调用处理方。

每次正常交换预留一个请求槽和四个 max-wire 的**逻辑字节额度**，分别覆盖客户端输入、
服务端输入、服务端输出、客户端响应。此额度不是实测 string capacity、宿主分配开销、
RSS 或 VM 页面；两个 owner 最大容量和两个池的空闲预算另计。额度 guard 在成功及异常
退出归还；部分真实发送后断连的测试验证异常归还，smoke 结束核对自有 socket 数为零。

服务端处理后 verified save 产生独立 string，再释放 owner，最后发送。string 保持到发送
完成。客户端收到完整响应后 verified load，并核对每个值。半帧失败关闭自有连接，不重发
整帧，不扫描 magic 恢复。模型取消保留缓冲和额度直到完成被确认，迟到完成不能二次发布；
该模型要求未来真实异步适配器也保持上下文寿命，并未替任何网络库证明这一点。

连接建立有一个绝对 steady-clock 截止时间，客户端一次交换和服务端一次交换各有一个
绝对截止时间；短 I/O、暂不可用及 EINTR 均不重置期限。socket 尝试、poll 及成功返回边界
检查期限；客户端语义校验后再检查。同步图验证/处理不能被抢占，超时只在边界报告，
安全 shutdown/join 可能晚于期限；这不是硬实时取消保证。

## 池和计时边界

服务端和客户端使用两个独立同容量单槽池。none 不使用池；cold 每次交换前清两个池；
warm 在同一连接做一次完整未计时往返。每次 cold 预期两个 miss，warm 预期两个 hit，
none 两者均为零，且三个模式使用相同输入、验证、连接和硬上限。

主指标从客户端第一次请求发送前开始，到完整响应 verified load 和逐值核验结束。
服务端 owner 在发送前释放，因此属于因果路径。客户端 verifier owner 的释放在主计时后
单独记录；随后等待服务端发送完成及服务端输入/输出 string 析构，仅记录残余等待。
服务端清理可能与客户端接收/核验重叠，不把整段发送或清理时间重复加到主指标。

延期回收对照为主指标加 cold clear、final clear、客户端 owner 释放及服务端残余完成等待，
再除以次数。它仍排除连接/线程/池准备、warmup、文件输出、客户端响应和输入 string
析构及少量调度/探针间隙，不是完整应用成本。timing profile 去掉逐阶段探针，保留必要
截止时间时钟；diagnostic profile 提供阶段时间，不能作为正式主指标对照。

## 功能清单与运行

34 组展开为 182 个固定子案例：163 个注入模型检查、17 个真实 TCP 子案例和两个文件
排他创建检查。合同逐项给出 golden 原因及计数；审计器独立比较，不信任 stdout 自报的
expected 字段。header 切分覆盖 0..88，payload 覆盖0/1/7/8/63/64/65/mid/last/end。
真实 TCP 覆盖三池、7 字节短 I/O、连续帧、帧后 EOF、头/体中 EOF、坏头分配前拒绝、
坏图不发布及后续完整加载、在途 owner 清池、处理失败不发送、响应截断、期限、部分发送
后 RST 和可验证 EAGAIN 背压。背压使用同一 V1 schema 的较大 fixture，逐值核验。

```text
python3 experiments/vm_network/run_network.py prepare --out build/vm-network/prepared-NEW
python3 experiments/vm_coordination/run_job.py run --owner W1 --kind build \
  --out build/vm-network/queue-build-NEW --timeout-seconds 240 -- \
  python3 experiments/vm_network/run_network.py build \
  --prepared build/vm-network/prepared-NEW --out build/vm-network/timing-NEW \
  --profile timing --coordinated-execution
```

诊断构建改 `--profile diagnostic`；sanitizer 构建另用 `--sanitizer address-undefined`。
smoke 同样由 run_job 包住，运行与所选构建相同的冻结 `source/run_network.py`，并显式
传默认队列锁的绝对路径；审计只读、不需要持锁。全部输出目录必须新建，失败记录不覆盖。
具体已经执行的命令、返回码、源码/二进制指纹及限制将在本轮验收证据索引列出。

W3 是唯一正式计时执行者。W1 的功能 smoke 含少量往返及阶段记录，不能称为性能窗口、
生产收益或真实业务验证。当前源码、构建、结果和独立 review 完成之前不宣称 G1 通过。

## 本轮实际结果

合同修订2已经统筹接受并冻结，SHA-256 为
`496791aaf988cfe3aa72a8cdc0d120e2e0384434f8fc1bf628a2e79223191718`。
同一 `prepared-v2` 生成 timing 和 diagnostic 构建，两个 smoke 均通过34组、182个
精确子案例。真实同步路径终结计数来自帧/fixture 退出事件或已完成的交换记录，独立与
golden 期望比较，不再把期望常量当作实际值。29项 Python 检查在普通及 `-O` 模式均通过。
补充16项 CLI 检查（含8项预期参数拒绝），12份处理输出与既有文件参考逐字节相同。

首次开发 smoke 曾因非阻塞 EOF 尚未可见而失败；修正 readiness 等待后才取得上述通过。
原失败记录和原源码保留。联合 ASAN/UBSAN 实际尝试编译，但可用 AppleRT21 导出
`___asan_version_mismatch_check_apple_clang_2100`，P2996 编译器需要
`___asan_version_mismatch_check_v8`，链接失败。没有绕过版本检查；**sanitizer 未运行、
未通过**，仍缺兼容的原生运行库前提。

[最终证据索引](validation/vm-network/final-v2/README.md) 列出命令、二进制和源码身份、
独立审查及搬迁审计；6MB归档包含311个成员。归档中的实现说明是打包时的说明快照，
本节为其后补的验收摘要。G1 的功能/冻结双构建请求与 sanitizer 前提分别交统筹裁决，
不据此关闭真实业务、生产异步或整体 N7。
