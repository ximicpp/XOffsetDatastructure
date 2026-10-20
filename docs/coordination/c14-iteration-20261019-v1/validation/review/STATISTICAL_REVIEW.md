# M1 唯一内部筛选：独立统计预审

审查日期：2026-10-19（Asia/Shanghai）。审查者：`screen_review`，只读审查及轻量 Python 数值计算；本审查未编译 C++、执行 native 或采样。

**结论：统计政策通过；实际 freeze/执行准入待审，不得以本文件单独启动筛选。** 阅读时尚无实际 screen freeze、schedule、resource projection 或完整候选门禁终态。本文件不把既有定向 4 normal + 4 SAN 或机制计数转写为 full/S3 通过。

## 输入身份

| 已读取输入 | SHA256 |
|---|---|
| PLAN.md | `b4f9fbeda402ac9f0248483e684a93722134350461c07c348552200f29e89b0d` |
| VALIDATION_MATRIX.md | `6ff2b866d8c0084f1c3e5e15cc1debd6437e3850c470f10c0adf153f11c6ac6a` |
| validation/M1_CODEGEN_RESULT.json | `e8f11983db49f3bcb170e4bf19cfdc7e97ff748f5c3b3d474b3f6c5108d561c2` |
| validation/M1_CORRECTNESS_RESULT.json | `f8914bf0a6745386f0fbdf940460b45493076f86cec4458ce43ba1cc44151167` |
| validation/M1_MECHANISM_RESULT.json | `75bc071dfaa883262bd39e29b59fdedfdd9626f301887b003bcbf758bf6499d1` |

也已读 AGENTS.md、HANDOFF.md、各 M1 dispatch RESULT 和 M1_FINITE_EXECUTION_RESULT.json。后者明确 timing=0、screening_admitted=false，完整业务/V1/消费者门禁待补。codegen 和 correctness 回执绑定的候选 IDENTITY.json 为 `fce1b6d235574613e512c6be5516d2636be11c7cdc676d894bbe068f45cc6966`（44364 bytes）；此处是核对回执，不是重新核对磁盘源码。v1/v2 主头相同，实际筛选必须核对完整 154 成员及十四生产闭包，包括改变的 `xoffset_core_lifetime.hpp`。

## 临界值的独立复算

固定 Bonferroni 族及 df=71，与 PLAN 和矩阵一致：

| 用途 | 分位概率 | 临界值 |
|---|---|---|
| 四候选的四项主目标，单侧联合 95% | `1 - 0.05/4 = 0.9875` | **2.289930854179058** |
| 四候选各九项辅助，36项双侧联合 95% | `1 - 0.05/(2*36) = 0.9993055555555556` | **3.327935133019535** |

环境：WSL Python 3.10.12，标准库 `math`；没有 scipy，未安装新依赖。第一条独立数值路径用 regularized incomplete beta 的 continued fraction 求 Student t CDF，再二分100步求分位数；第二条路径直接用 Student t 密度在 `[0,t]` 做200000等分 composite Simpson 积分核对 CDF，与不完全 beta 实现无调用依赖。

正 t 的 CDF 公式为 `1 - I_(71/(71+t*t))(71/2,1/2)/2`。独立积分得到主项 CDF `0.9875000000000032`、辅助 CDF `0.9993055555555588`，分别偏离目标 `3.1086244689504383e-15` 和 `3.219646771412954e-15`。freeze 可保留上述完整双精度常数；不得使用旧窗口系数、正态 1.96 或未校正 t。

## 统计计算与判定

固定72个配对单位，144个实际计时 children；每对恰有 control 与 candidate，各只执行一次。AB/BA 各36对；必须按角色身份还原 candidate/control，而不是用 second/first 或以顺序决定符号。

对每个指标 j，每对 i 的 `x[i,j] = log(candidate[i,j] / control[i,j])`。每项分别计算：

```text
n = 72
mean = sum(x)/72
s = sqrt(sum((x-mean)^2)/71)
se = s/sqrt(72)
point = exp(mean)
primary_upper = exp(mean + 2.289930854179058*se)
aux_lower = exp(mean - 3.327935133019535*se)
aux_upper = exp(mean + 3.327935133019535*se)
```

输入阶段时间必须是有限正数。`read_sum` 先在每个 child 内做 `query_before+query_after`，`total` 先在每个 child 内对完整八阶段求和，再计算配对 log 比。不得求阶段比均值、把144 children 当144个独立单位、把八阶段当重复样本，或混入机制/输出16/历史样本。

M1 唯一主目标为 mutate；只有其联合单侧上界 **严格小于1**，且完整正确性、独立审计、资源及冻结成本转移门禁全部通过，才有内部改善依据。上界等于或跨1为“未建立改善”，不能写为等价、无退化或确定变慢。

指标全集固定为八阶段、read_sum、total，共10项；M1从中除去 mutate 后剩9项辅助。辅助下界>1构成已确认时间回退；辅助区间跨1只是未确认回退。所有适用的成本转移否决必须预先明确并保留，不得用较小 total 或主项点估计覆盖。内存/资源/语义的冻结工程门槛独立于辅助时间区间。取消 L1/R1/M2 也仍使用主族4、辅助族36；本轮已消费失败窗口不得换名、修订或改 target 重开。

Bonferroni 不要求各指标互相独立；配对 t 推断仍以72个配对 log 值的预设模型为前提，AB/BA平衡本身不证明时间序列独立。应完整披露执行顺序及异常，不看样本后改单位、删离群值、去趋势、改用有利检验或补样。发现执行条件/数据有效性破坏则关闭并保留失败前缀。

## 实际 freeze 终审的必需项目

以下内容须在首个计时 child 启动前写入同一实际 freeze 并可按内容 hash 复核。现阶段均不因本预审自动通过。

1. **身份与门禁**：M1 revision、全154成员/十四生产文件及依赖闭包、控制闭包、请求、实际编译 argv/profile、compiler/wrapper/libc++/runtime、最终 bare 二进制、adapter 元数据、driver、fixture/wire/trace 和所有调度/采集/审计工具身份。性能为真实 `-O3 -DNDEBUG`，不能继承正确性的 `-UNDEBUG` 或机制插桩。所有本候选适用 full normal/SAN、原 S/M/L、V1双向、消费者、安全与机制门禁有相同身份的实际结果；历史 C14 不能冒充 M1。
2. **固定数据政策**：精确八阶段名称/单位、derived 指标公式、主项 mutate、9项辅助列表、上述系数、n=72/df71、候选/控制方向、严格阈值、完整真值、非正/缺失/超时/身份变化/原生失败处理。正确性/全body审计不得只验证成功退出或少量摘要。
3. **唯一调度**：72个 pair ID、144 child ID、恰好 AB36+BA36、固定实际先后顺序和角色、每child argv/输出路径，排除 warmup/conditioner/额外观察。若采用随机顺序，算法和seed也事前冻结。attempt ID和目录不可替换；0 retry、0补样、0删点，失效/超时保留此前缀并终止。
4. **时间预算**：screen命令600秒、每child20秒、独立离线audit3600秒；明确 bounded prelude、采集/交接/seal/恢复余量、当前child最坏上限和实际线上停止条件。144×20=2880秒大于600秒，因此20秒只是单child硬上限，不能作为全窗可fit证明；必须有实际完整进程 max 和非child工作投影，投影不fit时不启动。不得把 native.total 当完整进程耗时，或边跑边放宽cap。
5. **空间与内存**：基于实际产物份数/大小的 logical、apparent、unique allocated、metadata及峰值账本，所有复制与审计路径归属和 WSL guest/C backing 实际空余。在覆盖峰值后两卷分别保留至少2GiB；输出 physical allocated 总上限4GiB、每child RSS上限16GiB（事前明确采集及超限停止实现）。不能假定稀疏、去重或仅17830872-byte单一输出即代表全部产物；不能删除历史证据腾空间。
6. **互斥与恢复**：原共享全局租约的真实当前状态、唯一heavy执行者、计时期间其他agent静默；对当前真实宿主目标和已有授权的核对、bounded恢复路径及前后身份。用户进程恢复优先于离线审计，旧PID/句柄/暂停记录不可复用。
7. **独立终审**：实际 freeze、实际调度及执行器/统计实现的 hash 已由独立review核对；运行前/后所有身份一致，运行后逐对恢复、原始结果/receipt完整、72对与方向核对并独立复算。统计批准不替代安全/资源/功能批准；Q/F仍无启动许可。

待实际 freeze 到达后，本审查可对该精确 hash 追加终审结论；若发生内容改变，应重新审查受影响项，不能把本文件的政策结论转借给新身份。
