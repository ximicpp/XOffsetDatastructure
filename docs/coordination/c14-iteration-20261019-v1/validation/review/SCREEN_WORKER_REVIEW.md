# M1 screen worker 独立代码审查

日期：2026-10-19（Asia/Shanghai）。审查者：`screen_review`。本审查只读 Python 源码及现有资料；无 C++ 编译、native 或计时。

**初审结论：需修订；不准入采样。** 实际 freeze 未到达，最终运行准入仍必须绑定其外部 SHA256。以下是代码审查，不是运行成绩。

## 初审范围及身份

目录：`tools/c14_iteration_20261019/validation/screen/`。当前开发中的工具会继续修改，下列 hash 只表示本次初审快照，不可借给后续 revision：

| 文件 | SHA256 |
|---|---|
| m1_screen.py（已包含 config/compile metadata 绑定的中间版） | `c6b43f9ed3638e70152840d738f1d8e2fcf4d98e9e37854b6fe47f3f5f710de0` |
| campaign.py | `33dfc628a7202d4f08c31e463a4ac0c4fffc27ba5fd04dad3b4a991d402b99c2` |
| core.py | `9ad9063aade365b9dc243f9cf1261df93483e3aa2e8bb7ab443898ae3396cada` |
| protocol.py | `17c3eaf1d880e61e89a29cebad3df33fe4af5b2dc9a44f1c946df4bc125a9537` |

已核对原 `experiments/vm_coordination/run_job.py` 的 manifest 结构及 copied campaign 的 queue_audit/execute/native_result，不调用其旧五角色调度或旧统计。原 queue manifest 的 `files` 是整数 byte 长度的 list，与 core inventory 的 dict 不同，须明确归一化核对而不能直接套 core.check_inventory。

## 初审符合项

- schedule 固定72对/144 children，pair内相邻，AB/BA各36，control/candidate各72；run 依schedule顺序串行执行，无warmup/retry/replacement循环。
- summary 对每pair按角色计算 `log(candidate/control)`；Bessel sample stdev、`se=s/sqrt(72)`、df71、主族4和辅助族36临界值与独立复算相同。全集八阶段+read_sum+total，mutate唯一主目标，其他恰9项双侧辅助；derived量在child内先求和再做比值。主上界严格<1且无辅助下界>1，未设置事后择项。
- native_result 验证精确字段、原八阶段、native/receipt计时关系、输入/trace/wire/canonical/query实际完整文件SHA和oracle、记录数及输出wire尺寸。dedup在其后再逐字节核对canonical/query及oracle，先将首个有效文件hardlink入store，其余有效文件以同inode硬链接替换；wire全部保留，未按SHA或尺寸直接假设字节相等。
- source_snapshot读取154成员的control/candidate期望身份，并独立检查14生产文件。实际profile metadata、adapter字面量、compile_commands和有效argv已有额外绑定；性能要求-O3/-DNDEBUG且排除-UNDEBUG、SAN和强制include。所有实际closure路径仍须在最终freeze验证确为生效runtime/driver/依赖，而不是仅具名文件。
- 外部freeze pin、独立review和HOST quiet admission均要求同freeze SHA；run核对原全局锁真实被占及当前supervisor/run_id。输出只允许新WSL validation路径，已存在prefix不能覆盖。full源码/工具前后identity不符将失败。
- projection保守保留144个独立wire、共享canonical/query及7个file-limit临时槽和64MiB metadata；每child前检查已有physical allocated+7槽+1MiB<=4GiB，guest和/mnt/c分别保留2GiB并覆盖新峰值。allocated使用dev/inode去重及st_blocks*512，不把apparent字节当physical。
- execute施加每child20秒、RLIMIT_FSIZE和保守RLIMIT_AS16GiB，native high-water RSS另核对<=16GiB。RLIMIT_AS是额外虚拟地址空间约束，不能表述成OS提供的RSS硬配额；实际capture须证明原模型能在此约束完整成功，不能缩小合法业务来fit。

## 已报告的修订阻断

| ID | 初审具体缺口 | 必须闭合的条件 |
|---|---|---|
| B1 | audit的外部postrun pin只绑定queue/manifest.json，未把queue实际六文件与该manifest核对；request/result的run_id、owner、cleanup和实际argv/cap也没有完整离线核对。 | 按原runner格式核对精确六文件及bytes/SHA，request/result/run_id/owner/锁/argv/600秒/cleanup和时间一致。不能只pin manifest而允许其指向文件独立变化。 |
| B2 | capture status passed会直接进入projection，未证明其外层functional queue成功且清理完整。 | capture也核对原queue完整manifest、functional120秒、request/command/结果/cleanup；outer timeout或残留child不获得freeze输入资格。 |
| B3 | audit3600秒只在每个child循环开头检查；最后的hash/allocated/summary/write未做完整总cap核对，且初版没有自身外层硬watchdog准入。 | 绑定独立原lease audit-queue及固定3600外硬cap，末尾检查总elapsed；任何超限封存失败，用户进程先恢复。 |
| B4 | native完成后dedup/ledger/资源检查失败可使最后一次attempt有效native但未completed；初版audit只在native校验失败时允许末项break，会把合法失败前缀当作不可审。外层kill发生在seal前也可能没有worker status。 | 单独记录validated-but-not-completed末项和失败原因，不把失败窗口提升为144完整样本；外kill/无seal由独立有限failure receipt记录原prefix和queue事实，不补样、不换名。 |

已直接发给`m1_screen_tools`；其回复正在修订B1–B4，并将新核对过的相同taskset/CPU2 prefix及实际loader/libc纳入闭包。此回复仅是修订意向，尚不是复审通过。

## 实际 freeze 的待审项目

1. 对最终worker及vendor文件重新hash；检查B1–B4实际实现及纯工具检查结果。任何自测不得编译/native，也不能以模拟测试替代真实capture/门禁。
2. 实际config里两角色二进制、build/profile/adapter/driver、真实编译与链接依赖、compiler/wrapper/libc++/libc++abi/loader/libc及taskset、源154/生产14、fixture/wire/trace和所有工具逐项匹配。主头SHA相同不足以绑定M1 v2；`xoffset_core_lifetime.hpp`及IDENTITY文件必须一致。compile_commands本身不能单独证明既有二进制由该命令生成，必须与现有编译/依赖receipt交叉核对。
3. 新追加的相同CPU2 prefix必须来自当前核对，控制和候选一致；`sched_getaffinity`可用集及当前宿主授权/HOST quiet/restoration事实明确。旧profile中的CPU2/PID不能自行继承。
4. 正确性、full normal/SAN、S/M/L、V1双向、消费者及安全门禁完整、同候选身份；gate admission不能只具备status passed标签。
5. 实际capture两角色完整成功且外queue审计通过；按实际尺寸/完整进程与在线audit max进行144child投影，包含setup/identity/seal/恢复余量；600秒fit、4GiB输出physical和guest/C backing各2GiB实际余量均成立。不能用native.total或20秒child cap证明600秒全窗fit。
6. 单一attempt输出路径、144精确schedule、外freeze pin、独立review及HOST quiet gate不可转借；零/缺失时间或失败数据处理事前固定。若出现合法零时间而统计比值不可定义，应保持全部原始样本并拒绝该项晋升，不能补样或事后加epsilon。
7. 外层600秒包括实际worker所有工作；运行后用户恢复先于独立3600秒审计；前后身份、完整原始bytes/receipts、queue及postrun外pin匹配，再独立复算。统计结论仍仅为内部改善，不给Q/F资格。

本文件保留初审发现。修订复审与实际freeze终审应在下文追加明确SHA和结论，不覆盖失败历史。

## 稳定工具版复审

**结论：以下精确工具 revision 的代码审查通过，初审 B1–B4 已闭合；实际 freeze/采样仍未准入。** `m1_screen_tools`已停笔，本审查重新读取修订代码，并用 Python `-B` 做只读独立已知答案计算，执行前后五个工具 hash 一致：

| 文件 | 稳定版 SHA256 |
|---|---|
| m1_screen.py | `8310364077d3c37e01b4ca521932d33c4eb69023ecf645d9a7e6a5b40fe659b7` |
| campaign.py | `33dfc628a7202d4f08c31e463a4ac0c4fffc27ba5fd04dad3b4a991d402b99c2` |
| core.py | `9ad9063aade365b9dc243f9cf1261df93483e3aa2e8bb7ab443898ae3396cada` |
| protocol.py | `17c3eaf1d880e61e89a29cebad3df33fe4af5b2dc9a44f1c946df4bc125a9537` |
| check_tools.py | `d6f65c1c11b3c5e3b4307c5c2e3decbbce0d0d8a96ef191e72ef758b8f1e7d32` |

修订核对：

- B1：新增queue_audit按原runner list格式核对精确六文件/实际bytes/SHA、request/result的run_id/owner/kind/全局锁、cleanup为空、固定cap及时间。offline audit重新构造实际run argv，并与execution-context、freeze路径和外pin、原interpreter、原review/HOST字节身份核对。
- B2：projection重新审capture外层functional120秒queue，并从原receipt/native和实际输出重新核对角色、argv/environment、完整进程elapsed、在线elapsed、值与尺寸；capture本身不进入72对推断。
- B3：audit必须由原global lease下独立validation/3600秒queue直接调用，active检查精确命令及supervisor；末尾另检查总elapsed，外层watchdog覆盖Python末尾写入。audit在读完整样本前要求恢复回执，且采样first/last monotonic位于HOST ready与resumed之间，Balanced电源前后一致。
- B4：失败末项即使native及dedup完整，也可记录validated_uncommitted，保持failed状态且不给完整窗口统计；failure专用入口在恢复后、原validation3600秒lease下，记录outer失败或未seal的原prefix清单、pin及queue事实，明确window_closed/retry0/replacement0，不改变原prefix。
- 后续发现B5（审批JSON字节）：run已改为C.copy_file原freeze/review/HOST，归档原字节与context原identity一致，非canonical但合法JSON不再因重编码于末尾审计失败。
- screen_allocated把实际run、保留capture和frozen metadata目录一起计入4GiB，包含目录inode；projection增加已保留capture allocated，仍保留7临时file-limit槽与64MiB metadata。实际峰值/双卷空余尚须由最终freeze证明。
- control材料化154成员与实际旧编译source的14生产成员分开绑定；candidate同样绑定实际编译14及SOURCE_MANIFEST。两角色使用本轮新核对的相同taskset CPU2，前置再次检查2在allowed affinity，taskset/loader/libc加入闭包。此处不替代root对人类授权和真实宿主目标的核对。

独立轻量已知答案核查（全部passed，编译/native/计时均0）：

1. 直接调用稳定版schedule：144个不同child ID，72对各恰含control/candidate，AB36对、BA36对。
2. 两种先后顺序下每对candidate=80、control=100：十项point/upper均0.8，恰九项存在辅助lower，主项改善判定为true。证明未把second/first误当candidate/control。
3. 36对候选比2、36对比0.5：point=1，独立解析标准误 `ln(2)/sqrt(71)=0.08226143603173261`，与实际summary完全相同；主上界与独立`exp(t*se)`一致。
4. 一个cleanup值为零：原144个child及72个pair完整保留，cleanup标undefined，禁止晋升；无epsilon、删点或补样。
5. 只读比较实际旧functional-tools-v2：core/protocol字节完全相同；campaign仅多一条RLIMIT_AS16GiB，execute/native_result其余内容未弱化。

已读取工具agent的screen/TOOL_CHECK_v2.json（六项通过，0native/0compile/0timing）及TOOL_READY.json。后者所列实际CONFIG.json为601475 bytes、SHA256 `d4e296d277c92dd38fa16ff7ab7af283cbf403a9be9df5a9d6a1e8e1051f1363`，唯一输出`/root/xoffset-c14-iteration/20261019/v1/validation/M1-screen-20261019-v1`。此config可供实际capture/freeze审查，未创建实际freeze。

config候选IDENTITY文件为 `cfee04c9644cd011e8549ff1f01a80de0a210e6eba811cfb0187f36ded6f9745`（44371 bytes），不同于早期回执的IDENTITY文件字节。最终gate必须逐154成员/14生产内容证明同一M1 v2，说明元数据差异，不能只按文件名或相同主头借用旧准入。实际source14、二进制build/dependency receipt、全部正确性/消费者/safety、capture外queue、600秒及physical/双卷资源投影、独立实际freeze review和HOST quiet/restoration仍是运行前必需条件。本工具复审不批准任何采样，也不批准Q/F。

## 实际 freeze 最终独立审查

**指定 freeze 审查通过。** 已写 `screen/REVIEW.json`，kind=`independent-screen-admission`，status=`passed`，只绑定实际文件 `screen/frozen-v1/freeze.json` 的 SHA256 **`e57468ed00157a0154f220b4c67a29879c3fc14bebb9ff04d65f621e4ed11685`**。审批文件自身 SHA256为 `010afee72bbc9d0f5ae4d3ef8329d060abe9d3c27f297acd87e6dcc6b034ab38`（9797 bytes）。此审批仍以root完成当前HOST/跨会话quiet/真实目标授权/恢复watchdog及原全局lease为启动前提；不是性能成绩，也不给Q/F资格。

本次重新核对磁盘真实事实，不只读取passed标签：

- 实际全部source/toolchain/runtime/binary/metadata/driver/dependency closure重新计算，与freeze及两角色capture前后对象完全相同。两角色各154材料化成员、各14实际生产文件。M1主头为`50fc517ad0978b2a4961848870a7d7f1f2d670e6a9e7b9468b687d5622fc9280`，helper为`34d961bf7bb0cc4048f8403ef7b38618c9b124ba7d1866952aba638be6f32b09`；不是只按主头准入。
- 两份IDENTITY原始fileID分别保持fce1…44364B与cfee…44371B，解析全部JSON对象相等；独立按sorted compact JSON重新计算canonical SHA `81889c53bc2731fd15b382ad79c85d198e0ecef9006dfd9561bb3c23f58f8ad5`一致。原候选、staged、full normal及full SAN四份source各154成员逐文件SHA已重新核对。
- normal和SAN各73个实际JUnit testcase与receipt inventory名称一致，无failure/error/skipped，configure/build/ctest实际退出0；十四生产闭包同候选。正确性-UNDEBUG没有转入实际bare性能argv。
- business normal/SAN实际functional bundle均complete，保留各80动作/15业务cell、84主V1、8 extra、AB与C各2双向case/four CLI actions；全新业务编码及sealed旧控制wire的用途分别保留。消费者结果覆盖安装/子工程/移动prefix/archive receive以及两optional shared runtimes；没有把storage-only或独立source ZIP full suite冒充通过。
- 生命周期矩阵52条原始result/receipt pin均匹配、退出0且无timeout/exception，实际结果分类为24个`passed_ABDE_C_not_exercised`、24个`passed_C_direct_receive`、2个`passed`和2个`passed_cache_ledger_and_business_truth`，不把ABDE单项假称C。其真实W1/validation3600秒原queue为complete、203.040537565秒、cleanup三项为空，六文件manifest与真实bytes一致。W1合法历史验证角色未误套本screen专用W3约束。
- candidate实际编译binding的new_binary与冻结candidate二进制`cae2ca5aa22c9da9e1f630640cc5bf10a56ad247d2871dcadbe2fed4be1200a6`一致，production14一致。control实际二进制`7ff88829110ceea397bd9baded724bc1438e6645e3b39a6b67934873de1a11d3`。两角色实际compile_commands唯一driver argv与metadata匹配，真实-O3/-DNDEBUG，未含-UNDEBUG；实际runtime和依赖都在冻结闭包内。
- CONFIG_r2相较agent原CONFIG只新增`host_pause_derivation`、`host_pause_helper`、`host_pause_module`、`identity_serialization_equivalence`四个closure，其他JSON值完全一致。实际请求/fixture/wire/adapter未换；schedule与已独立核对的统计policy完全一致，attempt输出尚不存在。
- 两角色固定old-wire capture真实原queue为complete、11.947760348秒、cleanup为空。receipt与实际canonical/query/wire/body oracle复核，before/after身份一致；不把捕获中的phase值纳入推断。
- 独立重算投影完全相同：144个独立wire，各17830872B；canonical9772159B、query8073722B、7临时槽、64MiB metadata及74072064B保留capture；physical峰值**2851936609B**，低于4GiB。完整child加在线audit max投影**395.867313888秒**，另保留30秒seal，低于600秒。当前重新读取guest空余614821801984B、C backing53301628928B，均覆盖峰值并各保留2GiB；运行前及逐child仍要重查，历史max不保证未来速度。

终审员本次仍未编译、执行native或采样。恢复必须先于独立3600秒审计；失败前缀保留、0 retry/0 replacement和其余候选alpha不回收。此精确freeze外的controller/新源码/工具修改不能借用本审批。

## 唯一窗口的失败封存复审

**独立核对结论：已验证的关闭失败，不晋升。** `screen/FAILURE_REVIEW.json`为`verified_closed_failure`，SHA256 `d591ca3aa1df27210c262bccab4da63df9b853b8ab82a5ad8db3b0ec353df53f`（9279B）。freeze仍为e57468…；postrun pin为`7a6a5811c3d014ed96aacc7972de67d4a64a882af1c70b48088b4ff6252d55d5`，原manifest、queue六文件及当前前缀匹配。独立读取119条receipt、118条native JSON及对应完成值/去重ledger，恰为原schedule前缀：attempted119、completed118、未执行25，无重复或后续child，0 retry/0 replacement。

最后`p059-1-candidate`的完整进程receipt为timeout=true、20.022790410秒、returncode=-9，stdout/stderr为空，无native/wire/canonical/query。原timing queue`c8c4fa7355fe4a48b228d65ed90fe822`以failed关闭，370.219591107秒，cleanup全空；不是600秒总体预算耗尽。源/工具/运行时before-after身份与冻结相同。

两真实宿主目标均已requested-stop恢复，各owned resume=0且仍live，guard token/ready/stop一致，Balanced前后相同；WSL ready-observed≤timing queue interval≤stop，恢复先于原共享W3 validation3600秒离线audit。audit queue`f8c98e4e6ecd4ea49f75c9b158425ab2`complete，58.097674396秒、cleanup为空；结果仅为`preserved_failure`、children118、summary=null。未重复大体积body审计，复用该精确外pin下的完整离线body audit，另独立匹配实际文件名/尺寸、native摘要与manifest、hardlink ledger。当前含capture/frozen metadata的physical allocated为2204110848B，低于4GiB。

未计算或报告部分配对的任何收益、比值或区间；不足72对且窗口失败，禁止用于晋升、Q/F或回收alpha。20秒超时及空输出不足以归因候选正确性、具体阶段或主机噪声。保留失败前缀与未执行后缀，不补样、不改名重开。本复审编译/native/采样均0。
