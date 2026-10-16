# D031：批准原字节修复与离线补充工具

当前交接为小写 `handoff.json`，详细工具身份、51项普通/优化模式检查、原件绑定和待父任务审阅的 W3 validation 命令见 `docs/perf/vm-owner-scaling/D031_TOOL_REVIEW.json`。旧交接原字节保存在 `build/vm-owner-measurement/d031/handoff-before.json`，也作为历史对象保留在当前交接中。

D030 的唯一窗口已由 W3 完成78个原生进程。冻结 collector 将批准 JSON 重新序列化，导致保存的批准 SHA 与父批准原字节 SHA 不同；旧审计器正确拒绝了原归档尝试。D031 只修 live collector，使其解析、校验和保存同一份原字节，并增加 `test_approval_bytes.py` 与 `supplement_approval_bytes.py`。冻结提案、旧工具、原窗口和回执均保持原样。

补充工具保留完整原窗口与失败归档回执，证明批准对象相同且旧副本恰为旧 `dump` 输出；派生窗口仅恢复 `approval.json`。它使用冻结 `audit_window`、`verify_receipt` 审核派生窗口和原计时回执，并审计包含最终封存文件的完整搬迁副本。派生窗口不会称为原始字节证据。原窗口的首次完整清单将在 D031 W3 validation 封包时创建，不冒充 D030 已有的清单绑定。

本轮尚未执行实际补充封包；须由父任务审阅工具后交给 W3 默认 validation 队列。新增 native、构建和测量窗口均为0。以下保留 D023 准备说明，其中“未执行/未批准”是当时的历史状态。

## D023：固定请求数的 owner 粗粒度描述测量（历史）

日期：2026-10-16（Asia/Shanghai）。本目录为 W2 独占 collector/auditor；原生实现和构建属于 W1，正式窗口仅由 W3 在父会话精确批准后执行。当前准备代码、合成测试及未批准提案，**没有运行或重编 C++，没有测量样本**。D027已接受W1原26个功能/诊断进程和build-v4/D022 supplement，允许精确producer绑定；这26个进程不是本测量的计时样本。

冻结设计：`docs/coordination/vm-followup/contracts/owner-measurement-v1/design.json`，SHA256 `9465f8bfbed6fcc9557e935501834f31f17eed340674a1e18600ad0047f1d243`。只用该设计的一个窗口、固定78进程，不增加请求量、样本、确认窗口或候选实现。

## 调度与统计

A/B/C为8/64/256 owners、1线程；D/E/F为256 owners、2/4/8线程。每个allocate/free阶段固定8192请求，control=none，每个进程四阶段create/allocate/free/destroy。

六个新进程prerun顺序ABCDEF；随后b=0..5，ABCDEF左旋b位，依次执行该顺序和精确逆序。每block12进程，共72个measured，每条件12个样本、六个同条件pair。prerun仍完整语义校验并归档，但不进入统计，也不预热后续子进程内部registry/Core。

每阶段独立计算 `g_b=sqrt(t_b1*t_b2)`；AA为六pair最大正反比，block dispersion=`max(g)/min(g)`，三anchor=`sqrt(g_2e*g_2e+1)`，anchor drift=`max(a)/min(a)`。三个资格条件都≤1.10才标qualified，否则保留为uncertain。边界用整数产品精确判定；浮点只显示结果。

每条件相对C（o256t1）列六个block ratio及median/min/max。只有case与C对应cell都qualified才有比较资格。各ratio和median在含边界的[0.9,1.1]内均方向未定；六个方向标签不一致时记mixed。全部24cell与24comparison保留，不删异常值、不bootstrap、不推断尾延迟；min/max不是置信区间。

create/destroy报告batch_ns与ns/owner；allocate/free报告batch_ns、batch_ns/8192和8192/秒。现有主时钟包括barrier、调度、指针/计数记账及相应library工作，排除thread startup、填充/验证、快照、join/output；Core destroy不销毁进程终生registry bucket。不得命名为lookup latency、pure registry cost、完整应用生命周期或生产尾延迟。owner数也改变每owner请求数、工作集和Core元数据，线程改变同步与调度，不能单独归因哈希复杂度或锁。

## 身份、批准与一次窗口

`prepare`默认产生`producer=null`的`unapproved_template`，native_execution_authorized=false。最终binding模板为本目录`producer-binding.template.json`，八个ref都必须是仓库相对path+SHA：D027最终父验收、build result、完整26-case功能结果、独立修正checker、revision2 contract、D022 correction、toolkit result及recovery result。旧build冻结checker是旧版本，不能从它取最终语义审计器。D027父验收SHA固定为9e0ea9ed42921997fbe1f7cf9ace698d5268f0abcc765b4ee47faa9e78f739db，并交叉核对其四份reviewed evidence、26个唯一case、18成功/8预期失败、source/binary/checker/contract/supplement；不把设计派发当功能接受。完整source/binary/include/runtime/compiler身份从被绑定build派生，不重新编译。

有binding的prepare会读取完整build，因此必须用已接受默认队列的W2/W3 validation job。模板准备与离线小测试不启动原生。最终proposal会冻结两工具、队列auditor、设计、78-slot schedule、Python executable/version/direct模块身份，以及精确执行路径。它仍是unapproved_proposal；独立父批准文件必须用`owner-measurement-approval-v1`，显式绑定proposal/design/producer binding/最终父验收SHA与run_id，确认producer_functional_parent_accepted=true、W3唯一执行、一个窗口，并由调用参数给出独立已知批准SHA。D018/D022/D023本身均不是此窗口批准。

collect仅接受默认W3 timing queue：900秒、原F双头、冻结queue脚本、proposal中的绝对Python及完整collector argv、cwd/out/默认锁路径都须精确一致。不能以`python3`等替代写法通过。首child前以O_EXCL+fsync消耗唯一`build/vm-owner-measurement/native-window.json`；没有resume/retry或替换样本路径。子进程串行、30秒上限，逐次先记slot/argv，再保留退出/超时/stdout/stderr/audit；失败停下并保留部分证据，最多78个原生进程。

原生环境固定PATH、LC_ALL=C、LANG=C、TZ=UTC，拒绝collector的DYLD_/LD_ override。每个child前后核原binary，每个child前核runtime，窗口前后核完整binding闭包。报告保持原生`timing_evidence=false`，独立wrapper仅证明这个批准窗口的测量资格，不改producer声明，不把所有cell都标为噪声通过。

窗口退出后才能审完整W4 receipt。归档用独立W3 **validation** queue（W4无archive kind），核exact timing receipt、复制完整本窗口/工具/producer linkage、执行一次完整搬迁副本离线审计，再封存wrapper；不复制无关历史归档。新证据总计≤512MiB。归档和离线audit不启动C++。

## 当前复核入口

```
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s experiments/vm_owner_measurement -p test_owner_measurement.py -v
python3 experiments/vm_owner_measurement/collect_owner_measurement.py audit --kind proposal --record build/vm-owner-measurement/proposal-template-v1
```

测试全部是合成记录/文件、真实W1纯语义checker和W4纯receipt auditor；所有Popen控制为mock，不是原生或正式测量证据。早期unbound模板不包含producer身份，保留历史；D027绑定后的proposal-v1仍未批准，不能执行正式窗口。
