# C 实际 freeze 的执行边界窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**结论：实际freeze与已审协调器的桥接通过，未发现新的实质阻断。** 本文件只审身份接入、角色/预算/租约与资源口径；不重复全runtime、producer或业务源码审查，不替代统筹的总体准入。编译、项目native、dummy、性能采样均0；仅静态JSON核对和所列少量文件的只读SHA复核。

## 精确产物身份

| 产物 | bytes | SHA256 |
|---|---:|---|
| `validation/diag/C_FREEZE_DRAFT.json` | 1321251 | `250ce7daf7e1fb20513c8977928524c008aa6079211dbb66b9f5ef7efc4c7f0c` |
| `validation/diag/C_RESOURCES_DRAFT.json` | 1562 | `c542130c32939553062dac3f24cee991ce40098892b83844bdd17991a36bca79` |
| `tools/.../diag/phase_c_job.py` | 38059 | `9d7b3585cdc05a2c3444b931702da27bad9c914944478fcf0b61a878f86e60bb` |

freeze、`REQUEST_ADMISSION.json`的freeze/resource pins、`C_COORDINATOR_READY.json`的tool身份及当前实际协调器均一致。freeze包含6459个fixed_files成员；此处只复核协调器、原runner及C1指定launcher路径，不将未重新遍历的全runtime说成再次完整审计。初次PowerShell默认JSON解析不支持大小写不同Linux头文件key，随后采用保留精确大小写的Python JSON只读解析，未改动freeze。

## 两角色与时间边界

- policy精确为`c14`后`r1`，roles对象也仅这两项；两份source各记录154成员/14生产文件。m2仅保留预登记身份，不在本次执行roles中。
- 两份compile argv仅有各自角色路径差异，归一化后完全相同：固定Clang/libc++ profile，-O3/-DNDEBUG，最终materialized driver，独立输出及-MD依赖产物。compile预算2，各120秒；无额外probe/build循环。
- native argv各为自己新二进制的`run`，同一old-V8初始wire、同一fixed-M trace；仅wire/canonical/query/native四输出路径不同，参数清单一致。native预算2，各20秒；warmup/retry/extra_validate/extra_emit均0。
- whole policy为600秒。已审协调器在原W3 diagnostic600队列中直接执行compile和`D.supervise`，无嵌套租约，并把统一absolute deadline传给native wrapper。控制失败停止R1，已使用prefix不覆盖、不补run。
- READY的180秒是之前source-only prepare入口预算，不是C执行窗口。REQUEST记录prepare只读准备6.118776186秒、compile/native均0；它不代表本次C已经执行或通过。

## C1 的实际桥接

freeze明确记录继承PATH、`runner_ps_path=/usr/bin/ps`、`runner_python_lexical_path=/usr/bin/python3`。fixed_files收录实际请求路径及解析Python实体，已只读复核当前SHA：

- `/usr/bin/python3`及`/usr/bin/python3.10`：`a2f33a6e006989270f4340528eb61f8f97366e00a5d1b602ac8672ea44fc56ae`。
- `/usr/bin/ps`：`207df9d438f75185ab3af2ab1173d104831a6631c28ef40d38b2ab43de27b40f`。
- 原共享`run_job.py`：16898B，`6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909`，当前文件与freeze相同。

实际dispatch必须使用冻结的`/usr/bin/python3 -B phase_c_job.py run ...`请求和原shared queue路径；协调器启动时会核对准确argv/600秒/held lock/session及继承PATH、ps解析。当前是执行前freeze，因此不存在可据此宣称已经合格的真实C队列cleanup；运行后仍以原runner回执证明live/process_groups/zombies为空。不修改旧runner，不增加全OS闭包或新工具层。

## 资源与结论边界

- 资源文件与freeze policy一致，固定时间投影为 **505秒≤600秒**；包含两次编译及清理、两个native、各角色audit和100秒身份/封存预算。它是启动投影，实际统一deadline仍生效，不能保证宿主未来运行时间。
- 准入峰值4GiB，guest/backing各保留2GiB；记录guest可用603326906368B、C backing54748459008B，均满足各6GiB前提。实际启动及角色间继续重查。
- retained两角色body预算128MiB；各角色业务body64MiB由audit核对。compile单文件128MiB、每次wrapper合并日志1MiB、16GiB AS及观察RSS门限沿用冻结policy。它们不是内核提供的全进程RSS或全卷aggregate硬quota，4GiB allocated仍以实际产物检查。
- 精确36事件、完整进程wall/CPU/fault、原八阶段和嵌套emit/replay资源保持分别解释。产物明确`performance_admitted=false`、timing_samples=0；不用于加速比、置信区间、72对或Q/F结论。

统筹可结合其他独立freeze/source/资源审查形成总体准入。若实际source/tool/freeze再变动，应桥接新SHA；本结论不授权补run，也不把随后失败改写为可重试。
