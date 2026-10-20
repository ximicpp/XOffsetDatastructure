# 最终六角色源派生交付

2026-10-20，`v2_stats_review`。**已完成有限source-only派生；唯一未绑定的STAGE_BINDINGS字段为 `fixed_input_captures`。入口对此明确拒绝freeze/run/audit，没有假造capture、profile或六角色max。**

实际入口：`tools/c14_iteration_20261020_v2/final_formal_v1/six-role-campaign.py`，89763B，SHA `92c79e792f666b8700981d05bbc71c52045cebc8090716484e2c2ef57d6f451f`。native parser：`final_formal_v1/native_helpers/protocol.py`，11739B，SHA `8a1a6cf4cf32d8a7ea4d2c29e938b0afe82e26f3a7945c6706898c5c587e1c3a`。

机器可审清单：`reviews/FINAL_FORMAL_SOURCE_DERIVATION.json`，29942B，SHA `75b0b8332612d43295c6146287dc8c70ec7e74b099d54a88c926d31aaf316132`；两份精确unified diff位于新source目录。原冻结11源码before/after hash相同；复制9份原件，只有上述2份派生。未运行任何项目源码、编译/native/queue、profile生成、探针、freeze或Q/F，也未读取/hash body或binary。

## 已绑定与最小变更

- C14实际main66818…、14文件digest26299…、actual binary `7ff888…`；真实组合main155f0847…、14文件digest8cd90345…、actual binary `8b52729c014885a4f4df24dac1a8fc7d1e989e4c3903ea1792a4fde5795f57b8`。值来自已落盘bench index、normal/SAN profile和compiled-source-binding；只读取实际15个singleton源码/identity，完整profile仍保留重复dependency aliases。未以legacy commit替代实际源身份。
- STAGE控制/候选的binary/content/closure_ids/adapter identity及原四竞品binary/模式、五种sealed输入身份已绑定。native protocol只替换IDENTITIES中的xoffset、CANDIDATE_CONTENT_SHA256和CANDIDATE_CLOSURE_IDS；其余AST不变。
- executor仅改身份/路径常量以及 `configure`、`freeze`、`registration_source` 的路径接入：明确actual profile-normal-v2、C14 fixture、原global runner和旧同源pure-resource-test receipt；live native_helpers布局与未来11源码archive布局分开，避免误入archive模式。其余原函数AST完全相同。
- 原phase/comparison数学、schedule/seed、conditioner、时间边界、成本veto、资源/power/verified pooling源码逐字相同；32/64宏周期、30资格、12主+40辅助、2700/5400/3600/20秒、H300前置投影与4/8GiB、双卷2GiB规则保持。H300及六角色max仍须实际注册，不能从本source文件存在推断资源合格。

CLI保留原 `freeze/run/audit --campaign qualification|formal --out` 及原frozen/queue/pin/report参数；没有虚构`--profile/--bindings`。组合完整门禁、当前proposal/coordinator注册、host/quiet及六角色资源投影是后续执行前提，未在此伪造成passed。

## capture接续：一个字段，不新增框架

原executor `bind_stage:115–132` / `capture_argv:280–297` / `verify_input_captures:299–365` 要求normal与sanitized各六角色。每record为 `{prefix,binary:{path,identity},files}`，files精确20成员：8种body/结果suffix，另有 **inspect-input→validate→run→inspect-final** 四动作各自receipt/stdout/stderr。每个原生动作必须使用该prefix、该mode实际binary、对应environment及固定M trace；`.input.wire`必须等于sealed五类输入，三种canonical与queries匹配原完整truth，normal/SAN最终wire尺寸相同。原capture收据合同为每动作 `0<timeout<=60秒`；Q/F实际conditioner/primary仍每child20秒。

当前组合business coordinator明确声明fresh encoding，不能只凭80+80通过直接登记其M结果。**若其完整20文件、实际argv/prefix、binary/environment、sealed输入bytes及动作先后全部满足上述原条件，可以直接引用原完整record，不新增native；fresh encoding本身既不证明也不否定bytes相等。** 不改历史receipt、不把旧argv复制成新prefix，也不把资源capture的单次run凑成四动作。

四竞品binary/模式未变，其normal/SAN共8份旧sealed capture可在相同profile/输入合同核对后复用。仅当前C14和组合的normal/SAN共4份record待最终登记；若某份没有完整可复用record，最小是为那一份新prefix登记原4动作，四份全缺时最多16个native动作，而非重跑六角色整套或完整业务矩阵。执行前由root另定有限outer及精确request；本交付没有发这些执行或预算授权。原 `capture_inputs` 已提供完整native/validate/inspect truth与文件身份审计，复用它，不新写capture框架。

`maxC/maxP` 必须是同实际profile/输入/envelope的conditioner/primary完整process收据时长，含进程内准备与事后校验；八阶段total、诊断或单个business M-run不能代替这两类max。当前六角色max仍pending，本次不为其创建探针或宣称Q/F可完成。原wire/canonical/query均保留 **fresh独立生成并完整验证退出后** 的verified pooling，累计generated wire、logical、allocated及临时峰值分别计费；不搬用内部screen的wire永久独立规则。

上述路径除入口外均相对工程新目录或v2协调根；下一步只需独立源差分审查并补真实capture索引。交付后idle，不等待、轮询或扩大扫描。
