# M2 单候选机制实际 freeze 窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**结论：单候选接入及实际冻结桥接通过，未发现新的实质阻断；总体准入归统筹。** 本次复用 C 已有协调器/observer 审查、已审 C_LEDGER 和 M2 精确机制桥接，只核单候选差分。编译、项目native、dummy、性能采样均0，没有重做全runtime或C业务证据审计。

## 精确身份

| 产物 | bytes | SHA256 |
|---|---:|---|
| `validation/diag/M2_MECHANISM_FREEZE_DRAFT.json` | 1293005 | `001ddddf7bf6e83ca6e68dc0de095c517b127d094d2168157c68c7a1084a018b` |
| `validation/diag/M2_MECHANISM_RESOURCES_DRAFT.json` | 1866 | `2d994afe7c80953561f263f8ec0094dd51635406b31e19b5f22d1108aa61a722` |
| `validation/diag/M2_MECHANISM_REQUEST_DRAFT.json` | 2360 | `f3b716b90deedc9111c324e682d80c4243ff07d770cf483ecb50343cd54278ed` |
| `tools/.../diag/m2_mechanism_job.py` | 18452 | `60b0ca20be8fdd0cd5455485e09cc8ddc8d7c63b769e662e5ee147598d21b562` |

请求中的freeze/resource身份、freeze中的resource身份及实际文件相同。freeze绑定当前M2工具，基于已审 `phase_c_job.py` 38059B/`9d7b3585...`，导入前后都要求该SHA，未复制改写C的compiler/native/audit实现。

## 单次执行与原租约

- 实际请求采用 **更紧的300秒**，不是600秒。core桥接的600秒仅为允许上限；本冻结policy、request、leased检查、统一deadline和最终超限判断全部为300秒。没有放宽协议。
- 固定只执行m2：1次compile≤120秒、1次native≤20秒；新控制编译/运行均0，warmup/extra_validate/extra_emit/retry均0。输出为唯一`m2-mechanism-original-run-v1`新prefix，已使用即拒绝，不覆盖或补run。
- 原W3 diagnostic租约直接执行`/usr/bin/python3 -B m2_mechanism_job.py run ...`，wait=0、原lock/cwd/runner SHA准确绑定；实际held锁、supervisor父子关系和session均沿用C方式验证。未调用第二个run_job，不嵌套租约。
- 统一deadline在M2模块早期建立；C.compile_one接收该deadline，native D.supervise也接收同一absolute deadline。失败会停止后续工作并保留prefix和收据，外层runner仍负责实际最终cleanup。若封存前被外kill，不因为缺RESULT获得重试。

## 复用控制和同工作闭包

- 原C freeze为1321251B/`250ce7da...`，C14 control AUDIT为17479B/`7b834986bd69ea3c8ca0eb0fe0e6940da3bf44f52abc8d69922f5e2a0b9af503`；已只读核当前control audit bytes与pin一致。
- C_RESULT记录原C实际2次编译/2次diagnostic、retry0、源/runtime前后一致、26.272397881秒及空cleanup。M2 prepare检查C准确请求、manifest六文件、原control编译/二进制/原始输出身份；本次复用已审C_LEDGER，不重新执行或重建控制。
- M2 freeze的6492个fixed_files完整保留原C全部fixed_files的每个key和身份，再加入新M2工具、请求及已完成控制证据。M2源码早于原C已预登记绑定，未用新测控制替代旧控制。
- 实际M2与原C14的四份共同文件已窄核冻结身份和磁盘bytes全部一致：driver `399282a2...`、adapter `335114f0...`、POD hook `55221d97...`、event header `c4dd064b...`。
- compile argv归一source/output角色路径后与原C14完全相同；native保持同wire、同fixed-M trace及相同六个输入/输出flags，仅新binary/body路径不同。environment除角色TMPDIR外相同。36事件次序、原audit/完整truth、fixture/wire及继承PATH、ps/Python路径均与原C一致。

## 预算及解释

- 时间投影为 **120+2+20.5+60+90=292.5秒≤300秒**，90秒包括身份检查、预登记offline checker和封存。余量紧但没有靠超时后的追加运行补齐；实际启动还检查剩余compile/native/audit/seal预算，超限即失败。
- 新候选body64MiB，旧control新输出0；4GiB新allocated准入加guest/backing各2GiB余量沿用C。当前快照guest602798964736B、C backing53868400640B，均覆盖各6GiB前提；运行前后及关键节点仍重查。这是实际资源快照，不保证未来宿主速度或分配成功。
- 原wait4完整进程wall/CPU/fault和原八阶段/嵌套emit资源口径不变。offline checker只消费已pin的control/candidate AUDIT，不再运行C++。
- `RESULT.status=passed`表示这次机制执行和封存完成；是否命中可采用机制应看独立`mechanism_outcome`/`MECHANISM.json`，不能只凭总体passed晋升。任何结果都保持timing_samples=0，performance/screen/QF不准入，也不替代M2唯一72对筛选。

此文件没有新增执行要求。最终使用同一freeze生成总体admission后，原执行者运行该唯一请求；后续实际成功/失败及原runner cleanup仍需据真实回执验收。
