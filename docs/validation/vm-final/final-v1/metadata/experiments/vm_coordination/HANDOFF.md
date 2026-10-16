# W4 shared execution entry point handoff

Status: **D033 accepted; preparing single final G4 seal and independent audit**, updated 2026-10-16. Parent accepted the frozen runner for
local POSIX use; acceptance is recorded in
`docs/coordination/vm-followup/G0_REVIEW.md`. G1 prerequisite assessment is also
accepted; G4 matrix/identity preparation and a restricted Docker design reserve
are now published below. This is not G4 or whole-route completion. No C++,
sanitizer, stress or timing workload has been launched by W4.

Use `python3 experiments/vm_coordination/run_job.py status` and the `run` CLI in
[README.md](README.md). The default queue is `build/vm-coordination/queue.lock`.
Do not use `--lock-dir` for participating workloads, replace the lock inode, or
reuse an existing receipt directory. Initial build parallelism remains at most 2.

The final candidate lightweight run is:

```text
python3 experiments/vm_coordination/test_run_job.py --evidence-root build/vm-coordination/tests-20261015-05
17/17 tests passed; exit 0; 7.545 seconds
```

The report lists every exact test ID, source hashes, failures/errors, and each
case retains command argv/stdout/stderr/result/manifest. It covers real OS
mutual exclusion, stable inode, bounded busy/wait, nonzero/spawn failure,
timeouts, TERM-ignoring children, leader-exit orphans, children changing process
group within the owned session, stdin isolation, SIGINT/SIGTERM cancellation,
SIGKILL of the public client, unrelated-process survival, existing-output
protection, offline audit, nested results and tamper detection. All test queues
are independent of the default queue. Earlier immutable test directories remain
at their original hashes and do not validate later source revisions.

The stable lock is held by a separate supervisor until the owned session is
quiescent and evidence is finalized. Commands receive stdin `/dev/null`. The
recursive receipt manifest covers only files inside that receipt; each role
must audit separate build/results directories itself. Raw process probes are
outside the ordinary command timing loop. F headers are read-only and their
hashes are recorded; the runner's request and supervisor source identity agree.

A separate final-source synthetic refusal suite passed 4/4 tests (10 rejected
commands), report `build/vm-coordination/docker-synthetic-20261015-02/test-results.json`.
It proves that unsupported daemon commands are refused before execution, output
creation or lock acquisition. It does not execute fake/real Docker and does not
prove container cleanup.

Supported scope is local POSIX commands whose descendants remain in their new
session. `setsid`, escaped daemons, remote execution and externally scheduled
work are unsupported. The public client may be interrupted or killed; forcibly
killing the supervisor is outside the guarantee. Unknown cleanup/inspection
state retains the lock; cleanup can exceed the requested command deadline.

**Docker is not enabled.** Review identified that killing a `docker run` client
and observing an absent name once cannot prove a queued daemon start will not
occur later. Direct Docker/Podman/nerdctl/ctr and reserved `--docker` are rejected
before output creation; wrapping them in scripts is forbidden. No actual
container was launched/stopped. A future adapter needs a create-without-start
handshake, verified immutable ID/label/daemon identity, start only after that
handshake, owned stop/kill and postcondition evidence. Synthetic refusal tests
are separate from real-container cleanup verification. Parent has accepted
this POSIX scope; all daemon and remote workload restrictions remain.

Independent final-candidate review passed within the stated POSIX scope:
all 18 original run-05 receipt manifests passed offline audit, and both
intentional tampered copies were rejected. No tested command was re-executed. Machine-readable source hashes, current review status and next gate
are in [handoff.json](handoff.json). Platform prerequisites are published
under `experiments/vm_platform_validation/` and `docs/VM_PLATFORM_FOLLOWUP.md`.
Full final validation awaits an explicit G4
dispatch from the orchestration chat.

平台前提评估已通过独立审查并被统筹接受，记录
`docs/coordination/vm-followup/platform-acceptance.json`，不关闭 N1。见
`experiments/vm_platform_validation/handoff.json` 与 `docs/VM_PLATFORM_FOLLOWUP.md`：
24 条只读命令、38 文件取证、Windows 包完整性均有原始记录；没有执行平台矩阵。
TSan 和冻结 SDK 已找到；原生 Windows 与独立 Linux 访问仍缺前提。
最终 G4 的 selected-source 冻结、适用验证和最终版交付报告均等待统筹派发；文档草稿另行授权。

G4 准备的最终只读补证已经完成：

- [矩阵与冻结要求](../vm_platform_validation/g4-preparation/G4_MATRIX.md) 与
  [机器清单](../vm_platform_validation/g4-preparation/g4-matrix.json) 绑定最终候选 inventory、身份映射和来源限制。
- [身份映射说明](../vm_platform_validation/g4-preparation/identity-notes.md) 保留实际 profile；
  当前无条件基础 C++ 新任务为 **0**，没有剩余廉价补证。
- 原生 full 48（签名 3 已含其中）、wire 504、原生 full/storage package 各 6 项的相关
  闭包继承基础已获统筹接受。Linux sanitizer 26+2 是逐 case 并集 28，不能写成一次新跑。
- TSan 15 项保留实际 macOS 26.6.2 arm64 及记录中的 10.15 deployment 编译选项；
  当前 unset deployment profile 没有运行。Apple C++17 standalone/storage-package 与
  P2996 full 子目标分别记录；Linux package full 为 amd64/P2996，storage 为 ARM64/GNU。
- 历史未保存的可执行文件、编译器或完整 SDK 字节 seal 不补造。映射保留实际
  source/runner/command/JUnit 来源链及缺失字段，不因字段缺失自动重跑全量。
- 条件增量只限统筹明确选择的主张：普通 Linux 新增 allocator 两项、W1 新网络源码
  sanitizer，或另选当前 unset deployment TSan 配置。均未派发，不重复已知 Apple ASan ABI 失败。

[Docker 提案](docker-adapter-design/PROPOSAL.md) 已被统筹接受为受限设计储备；
没有 adapter 实现、测试或容器工作。后端实例连续性、独立 supervisor 生命周期及未知
create/start 结果的持锁收尾仍是实施前提；只有确需 Linux 增量时才重新评估实施。

W3 一次 F/F 校准的 78 个进程执行成功，但 4 项指标及 anchor 均噪声不合格，统筹已
接受该限制；不重试、不进入 D/F 或归因消融，N3 保持开放。D019 有界网络诊断与
D021 的 112 进程校准/可移动归档现已接受，仅 1 MiB 完整合格；新的 D025 88 进程
池窗口结果仍待统计/资格独立验收。G4 等待池比较、固定请求量 owner 争用、G3 及
最终采用身份。W4 当前无运行任务，也不占用共享队列。

统筹 D015 已接受历史配置补证，记录
`docs/coordination/vm-followup/G4-supplement-acceptance.json`，不可变副本位于
`docs/coordination/vm-followup/platform-supplement-reviewed-v1/`。本轮新授权交付为
[VM_FINAL_DELIVERY.md 草稿](../../docs/VM_FINAL_DELIVERY.md)；
[draft-status.json](../../docs/validation/vm-final/draft-status.json) 记录文档来源、复核和待项。
草稿汇总已接受的 W1/W2 基础与历史 profile，保留默认 F/F 噪声未达标、平台和真实
consumer 前提，并单列 W2/W3 网络结果、G3 与 D016 owner 研究。它没有最终冻结源码
或声称 G4 完成；没有改全局 tasks/迁移说明/示例、运行矩阵或实现 Docker。README 的
后续 D024 授权窗口另记如下。
首版[草稿审阅](../../docs/validation/vm-final/draft-review.json) 保留为历史观察，不能用它
验证后来修订。当前 v2 文稿的独立审阅与字节身份见 `draft-status.json` / `draft-review-v2.json`；
本次小检查为草稿 44 个本地链接、建议说明 6 个，均存在。后续结果统一填表，不反复封存草稿，
也不把文档来源观察当作最终源码冻结。

本次 v2 已纳入 D019/D021/D022/D023：网络诊断为 32 进程有界证据，网络校准为真实
112 进程且只有 1 MiB 完整合格；26 进程 owner 功能尚未完整验收，首记录保留并离线
重审，只补剩余项。D023 是 W2 准备、W3 sole 执行的 78 进程描述性协议，尚无原生
测量执行授权，与已经完成的 default F/F 78 进程分开。

D024 已授予且完成仅 README.md 的独占窗口。三处替换严格等于审阅补丁，README
postimage 为 `aeb54c4d8428842982911ae4f944754f5894d21fdf755c8da50bd20e82dbfaad`。
[窗口交回记录](../../docs/validation/vm-final/readme-window-return.json) 固定 preimage、补丁、
postimage 和链接检查；窗口已交回，没有持续写权限。迁移说明与 C++ 示例身份未变，
无公共 API、C++、容器或基线运行改动。

G4 最后收尾已获统筹授权。D026 网络88及 D029 档案、D027 owner26及 D029 档案、
D028 三方向前提未满足保留 F 的决议均已接受。D030 唯一 owner78 已执行；D031
approval 副本重序列化造成离线哈希拒绝，须保留原工具/样本/失败档案并补证，不能算
native 测量失败或重试。当前 live owner 测量工具及最后 handoff 仍未冻结。

默认 validation 队列已暂存 225 个稳定文件（2,970,586 字节），21 个相关基线输入均匹配；
Stage III 129 项只差已接受的 README 与两个离线审计工具。另已保留五份实际生成源码
及四份原 D030 工具/测试，共九项接受身份。元数据路径预检通过，原始巨包只作身份引用。
见 `docs/validation/vm-final/final-v1/source-differences.json`、`preserved-inputs.json` 和
`build-identities.json`。没有新 C++ 任务，最终 capsule 尚未 seal。

下一步只在最后 owner78/D031 接受后补入最终 live 工具、结果、交接及索引，经默认
validation 队列一次封存，再独立审计；外部平台、默认 D/F 2% 归因、兼容 ASan ABI
和真实 business/API 前提继续开放。本轮 G4 仅请求验收明确的本地研究交付。

### Final accepted inputs and G4 cut

D033 owner research and D032 tool/offline supplement are accepted. W3 has
completed final RESULTS and handoff documents without further execution.
All 78 original processes, 7/24 qualified cells and 2/24 qualified comparisons
(including one self comparison) remain explicit; all multithread comparisons are
uncertain. The original approval-copy failure and original/derived sole
approval.json byte difference remain preserved; no native retry or rebuild.

Final owner authority:
`docs/coordination/vm-followup/owner-measurement-acceptance.json`, SHA
`d87332b22406e9077ab68a44cb02ba14ac611a8fba78447c6498458bf584eddd`.
Tool authority: D032 `contracts/owner-measurement-v1/supplement-tool-acceptance.json`.
The accepted-input cut contains 225 stable files, 9 preserved inputs, 9 final owner files and 2 packaging
helpers. The final 9 small source identities were recorded through the default
validation queue in `docs/validation/vm-final/final-increment.json`: eight
accepted historical/source constraints plus one unexecuted auxiliary template.
The unmodified 110 B comment placeholder `tools/sigs/.gitkeep` was not selected;
129 historical comparison records do not mean 129 files all entered the archive.

This live handoff records the preseal stage. The next authorized operations are
one final seal and one independent audit of only the new capsule. Final
postcut results will be bound outside it by `docs/validation/vm-final/final-seal.json`
and an immutable final handoff, preventing metadata/report hash cycles.
No unconditional C++/native/container jobs or historical statistics reruns are
added. G4 parent acceptance and external roadmap prerequisites remain separate.
