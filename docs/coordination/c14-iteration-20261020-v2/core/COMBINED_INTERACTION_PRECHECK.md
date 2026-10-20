# R1+M2有限交互安全预审与相关门禁并集

本次只读两份既有已审patch、独立安全证明、精确targeted请求及现有功能job源。未创建组合源码/identity/最终request，未编译/native/queue/采样或读取大body。M2唯一72的实际结果由root另行统计和工程终审，**尚未正式retained**；本文件不作保留或组合决定。只有root实际接受M2并明确选择组合后，才生成一个新组合身份，执行有限相关门禁。完成后idle。

## 有限静态结论

**在两份patch保持原样、同一C14基底和原支持域前提下，未发现R1 canonical hot/cold拆分与M2当前节点data捕获之间新增的语义冲突。** 这是patch级交互预审，不是实际组合编译/正确性通过或组合性能证据。文本区域不相交只是必要条件，以下调用与寿命论证才是依据。

- R1：基底main `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`，候选main `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`，patch `0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad`。只在canonical函数入口/出口拆出private noinline active helper，原cold主体相同，public恢复点保留。
- M2：同一C14基底，候选main `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f`，patch `c5c3d7bf6e1993fae1bb129b58e918fa3f9bfa9777f17e1b668a966cf56b7497`。只改nonstring owning vector的本次data捕获/原capacity containment/size后live遍历。R1区域原基线371–443附近，M2区域4361附近，未修改对方分支。
- 两候选均独立C14，不含M1；组合以后仍只能包含这两项既定生产差异。不能将某个独立头冒充组合，不能覆盖基线/已封存候选，也不能以组合名恢复M1/L1资格。

## 相交调用闭包与不变量

| 范围 | 实际相交关系及组合必须保留的性质 |
|---|---|
| acquire与inactive | M2本节点`data()`仍走原`resolve_rel32_const`与public canonical。R1 wrapper每次实际canonical调用仍执行原acquire load，zero立即返回；无跨调用zero/negative cache、无pointer保存或load hoist承诺。M2减少重复data调用，不改变每个仍发生调用的ledger规则。 |
| active/cold | nonzero调用进入R1 private helper；旧anchor转换至recover(record)主体、owner acquire、anchor优先/one-past、target/canonical bounds、origin fallback均相同。M2使用data返回值的时点在整个data调用返回之后；不在R1 wrapper/active helper中插入操作。 |
| callback与record释放 | 旧canonical不安装cold TLS结果、不驱逐terminal TLS record；原mutex/shared-record作用域和release相对顺序不变。R1把本地record释放移到active helper结束、仍在canonical返回caller前，wrapper无owned对象和新callback。M2在第一次data返回后至原begin/end捕获点之间仅builtin containment及scalar descriptor读取，没有新增callback/分配/退役。properly synchronized live-owner闭包下没有新last-owner/PMR/new_handler边界。 |
| pointer寿命与mapping | M2仅automatic `const auto* elements`及null-safe last；无owner/record lease、无mapping pin。原range-for本就先保存begin/end并跨child递归使用；R1不延长其存储有效期，组合也不新增可跨未知操作使用指针的保证。仍需原live container/array/provenance与外部owner同步；保留OwnerRecord不是pin mapping。 |
| ADL与递归 | 保留unqualified `move_assignment_owns_address(child,address)`、const lvalue、child early true、array与reflection/base/member次序；ADL可以进入用户同名重载，不能泛称generic recursion无callback或以noexcept作证明。原循环也持有父begin/end跨该递归，提前捕获区间无新callback的差分论证仍成立。不得顺手qualify调用、禁止原可用类型或新增trait短路。 |
| ownership与move | `!owns(destination,&source)`后`!owns(source,&destination)`的原短路顺序、same-arena/allocator前提、ancestor/descendant copy-before-retire fallback及moved-from状态都保留。R1只改变canonical函数组织；M2不缓存predicate、跳 sibling、删除容量内嵌head或反射边。 |
| null/capacity/size | string分支原样；nonstring先data、原完整capacity byte containment，hit立即返回，未提前读size。仅nontrivial miss读live size；null产生null last，不作nullptr+0；empty allocated保持begin==end；不遍历capacity spare或对trivial element新增递归。R1 data恢复语义不变，不能据其inactive null-return将合法原numeric fallback误作空vector。 |
| 线程/TLS/退休 | ledger注册release、unregister/acq_rel、owner clear/extract、四槽TLS与last、冷查找和终端record规则均为原实现。同步线程交接可测试；无同步同时释放同一payload没有新保证，ADL破坏正在遍历父对象的有效性也不因组合变为合法。 |
| 表示与owning输出 | 无field/layout/arena/registry/TLS状态、wire/schema/rel32/public API变化；无新持续缓存、注册维护、分配/free/销毁操作。原完整fresh owning header/payload输出、normalization、load/reload/首错误/生命周期责任保留。 |

R1 active路径仍可能有额外call/frame/branch，M2 counted-loop test/LEA及register使用有成本互换；两项优化可能减少同一批canonical解析。已有独立裸asm与计数用于机制依据，不能推出组合机器码等于两者拼接、收益相乘/相加，或整个mutate下降75%。既有SAN/动态结果也不证明所有ISO规则、任意ADL重入或无同步销毁；真实组合仍须以下实证。

## 已注册targeted的精确并集

读取`REQUESTS_R1_NATIVE_v2.json`的5源码和`REQUESTS_M2_NATIVE.json`的4源码，共同项是**test_owner_identity.cpp**，故源码并集为8项。每项normal与SAN各需同一真实组合source身份的实际证据；若单独运行即16个program executions，**只是准备清单，当前没有执行预算或request**。

| 既有源码 | 真实组合需要保留的验证范围 |
|---|---|
| `tests/test_canonical_zero_target.cpp` | anchor优先、相邻reservation、zero target/one-past及ordinary-domain分支 |
| `tests/test_canonical_cold_no_callback.cpp` | 真实cold路径、无cold eviction、terminal/delete callback与drain边界 |
| `tests/test_canonical_registry_v2.cpp` | cold/hot记录、active ledger、owner move/retire及既有allocation/new_handler失败 |
| `tests/test_owner_identity.cpp` | 两请求共有；stable owner、callback安全reclamation及move/生命周期断言，只合并相同source/profile/断言口径的重复项 |
| `tools/c14_iteration_20261020_v2/core/test_r1_thread_handoff_v2.cpp` | 同步混合ordinary/canonical owner与线程交接；不允许并发退休同payload |
| `tests/test_same_arena_move.cpp` | 双向ancestor/descendant/public赋值、same-arena、copy-before-retire和原OOM/VM故障边界 |
| `tests/test_locator_soundness.cpp` | 容量内合法head/opaque byte storage、跨操作ownership变化，防persistent阴性cache和删完整capacity检查 |
| `tools/c14_iteration_20261020_v2/core/test_m2_owns.cpp` | 后续child命中、unrelated miss、null/empty allocated/spare capacity、非trivial递归、owner move后重新获取、retire/reuse相关断言 |

实际组合full normal/SAN中的同名六个repo用例若真实fixture字节、compile flags、assertions、sanitizer、subprocess/failpoint设置均满足原targeted合同，可用其对应JUnit/compileDB/receipt/source154作为证据，不因旧请求label重复跑；否则补精确缺项。两个core外部fixture不在原154 full源码中，不能由full73这个总数自动代替。用例并集不覆盖一切泛型ADL用户代码，源表达式和差分寿命证明必须同时保留；不临时编写新并行实现oracle或新试验。

## 真实组合的最小相关完整闭包

root实际接受M2并选择组合后，只生成一个新154/14身份，验证diff仅两份既定patch，原CMake/source/vendor与fixture其余字节相同。随后采用现有有限工具完成该真实组合自己的normal73/SAN73、上述targeted缺项、SML80+80的原query/9阶段canonical/owning emit/reload真值、V1 main84+extras8/AB4/C4、安装/subproject/relocated/archive消费端、两共享runtime、OOM/new_handler/replaceable-delete和lifecycle52。不能借独立R1或M2的结果把新source map标passed；可以复用同身份未变的控制/竞品/reference，只重建组合candidate。

组合bare profile/binary及实际依赖必须在这些实际构建中固定outer61B、inner/toolchain/libc++/runtime、原driver/adapter/public模式和输入。correctness保持原`-UNDEBUG`与SAN断言；未来timing裸`-O3 -DNDEBUG`不含诊断/SAN/全局new计量。不存在自动获批的组合72配对或新control诊断预算；若需额外机制/机器码验证由root另行明确有限范围。Q32/F64只在最终真实闭包、六角色实际资源投影和独立freeze准入之后，保持原预算/模式，不能以本文件启动。

## 复用现有工具，未来仅做必要身份派生

以下只是root选择后可复用的源路径，不在本次执行，也不修改当前稳定工具：

- `tools/c14_iteration_20261020_v2/core/run_native_targets.py`：已按request的完整source_members/argv/input pin执行并要求原held lease；将既有8源码并集绑定新source/输出即可，保留原normal/SAN flags、断言、20s native和120s单compile等有限合同，不新增scheduler/lock。
- `core/m2_full_jobs.py`及`core/m2_full/full_entry.py`（或等价已审R1 full薄入口）：复用原CMake/CTest、依赖锁、test inventory、实际JUnit/compileDB/source154前后检查。新派生需改IDENT、Q、DOC、kind允许值、job/output/receipt标签和14文件overlay；当前M2专用前置校验不能照搬为“组合已通过”。
- `core/m2_full/business/prepare.py`与`dispatch.py`：保留fixture/oracle/protocol，仅派生新candidate label、content/closure literals、source/full请求与PREPARED绑定、profile及新输出路径。其`M2_FULL_SOURCE_BINDING`和原M2 targeted/static receipt的精确source前提不能作为组合证明；需要引用root两独立接受+本交互审查，并绑定真实组合targeted/full证据的诚实source/input桥接。
- 同目录既有business/package/runtime工具和`core/m2_full/business_matrix/dispatch.py`：保持全部C++fixture/oracle/动作与预算，candidate source/profile/production maps及DOC/输出前缀更换。完整控制/竞品不因改路径重编。
- `core/finalize_m2_full.py`（或R1 bound finalizer）：复用实际request/tool/launcher/full child receipt/JUnit/inventory/compileDB/source154、PREPARED、business/V1/consumer/runtime/lifecycle refs闭包检查，改为新组合身份和真正targeted并集计数/refs，不能沿用M2-only native8或R1-only B10作为同源结果。
- `core/derive_m2_full.py`、`write_m2_full_request.py`和已审DERIVATION/TOOLS.patch提供身份派生模式参考；脚本本身硬编码M2且输出已存在，**不能直接重跑**。未来新namespace薄派生需记录完整before/after/diff/manifest并独立审阅，全部C++fixture保持字节相同，不再造功能框架。

最小改身份点归纳为：新candidate/source154/production14与patch集合；新source/doc/output/receipt路径；profile/adapter/protocol的候选metadata和完整closure literals；真实targeted/full前置与最终refs；原global runner/lease与explicit61B保持；正确性/模式/预算不变。任何工具字节变动都须进入新工具清单，不能改已审R1/M2工具、请求、结果或历史before/after pin。本次没有物化这些修改。

本文件不接受M2、选择组合或宣称组合性能/正式输出5倍达标；下一步仅等root最终候选选择，保持idle。

## 本次有限只读引用身份

下列为既有patch/小型source metadata的实际SHA；未生成组合，也未读取大body。

| 路径 | bytes | SHA256 |
|---|---:|---|
| `build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/R1.patch` | 1415 | `0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad` |
| `build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/M2.patch` | 1167 | `c5c3d7bf6e1993fae1bb129b58e918fa3f9bfa9777f17e1b668a966cf56b7497` |
| `docs/coordination/c14-iteration-20261019-v1/core/REVIEW_R1.md` | 8375 | `0ad7333d054be4247918307bdf86fb0453a463e4fc1a7e3d6cb19db562cf95bf` |
| `docs/coordination/c14-iteration-20261020-v2/core/REVIEW_M2.md` | 16470 | `0981720db0c638718361e11d7d9b1ccadc7be79b7a4ee00df41f0d2830ce475f` |
| `docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_NATIVE_v2.json` | 167092 | `edcf2fb2d29f5d5249d8d1ebc2d7727bafcd2d44a503ff1bacd839018edf7d4a` |
| `docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_M2_NATIVE.json` | 163031 | `ed3de3d994c346071e8c381b16b469e70df73d9e2b05840553c0c1ac187f6618` |
| `docs/coordination/c14-iteration-20261020-v2/core/R1_SCREEN_GATE_BINDING.json` | 106959 | `21d9ad5def7ead829692f3bd16b67a2cd010236aca389127fcff111a1900585d` |
| `docs/coordination/c14-iteration-20261020-v2/core/M2_SCREEN_GATE_BINDING.json` | 51810 | `1d24709eaaa8384e6ee672575e1758dbf7f65382824b64d3547302239acc068c` |
