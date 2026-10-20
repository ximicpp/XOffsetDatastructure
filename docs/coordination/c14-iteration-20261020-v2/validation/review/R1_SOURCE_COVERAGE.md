# R1 v2 native 请求前的源码与测试覆盖审查

日期：2026-10-20（Asia/Shanghai）。审查者`screen_review`。已读v2 PLAN；本次不编译、不执行native/诊断/计时，不重编R1 codegen，不改v1历史。

**源码事实已核对，实际native请求尚未到达，因此尚无执行准入。** v1的R1 review只准入当时codegen；其后已完成的codegen按v2计划引用，不产生当前native或性能成绩。

## 实际身份和变化

- 候选`r1-canonical-hotcold-v1`的IDENTITY SHA256：`52570679ffd1a479bdf39d3854c8424aa8b29a51e67339d2f93e6fd3c6072948`。
- 候选主头SHA256：`b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`；patch SHA256：`0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad`。
- 独立逐项hash154候选成员与C14基线成员，均匹配IDENTITY中的candidate/baseline SHA；14生产成员集合由原C14材料化清单确定。仅`xoffsetdatastructure.hpp`变化，没有混入M1。
- 独立提取原active body（从anchor数值转换至`return recover(record);`），3973字符完全相同。public helper仍逐调用acquire-load，零返回null，非零进入private noinline helper；未持久缓存零结论，未改注册/退役顺序、cold TLS规则或ownership契约。

## 四个既有程序实际覆盖

| 程序 | SHA256 | 已有真实控制 | 不能替代的同步混合场景 |
|---|---|---|---|
| test_canonical_zero_target.cpp | `10a2ec0f580e147e4a4453370c63d19c9c529850351130562eae22e492629091` | 两个确定性相邻数组的C/C与C/ordinary origin、cold anchor优先、0字节one-past和正宽target-only、TLS未冷安装/驱逐；fixture清理恢复ledger/registry/cache。 | 这是私有registry合成fixture，没有外线程真实finish_verified owner发布/持有/退役。 |
| test_canonical_cold_no_callback.cpp | `22864fbf55e2a84b129a29fe81fde68363e1dfb5728b0ec5894b43dab4b505cb` | 真实C receive owner；四terminal TLS记录、cold无allocation/delete/eviction；真实双block drain由new_handler安装terminal slots，随后故意精确delete控制移动并销毁原Core，验证触发点确实有效。 | 同线程；不能证明foreign canonical owner与ordinary owner同时存在时的global ledger/TLS交接。 |
| test_canonical_registry_v2.cpp | `c77f6dd256ae135b1f67bdd9b01e55400158ddc258250492a58ef85b13a2be9b` | 七真实C owner、冷热精确恢复、one-past、移动/swap、退役/重建、pending注册OOM保token、new_handler重入注册/查询/移动；最后释放所有C后的普通读取与canonical=null。 | 普通owner的显式canonical=null断言位于释放全部C之后，没有证明无关foreign C活动时该ordinary查询仍正确。 |
| test_owner_identity.cpp | `4b7efff5ef72a6b2592532aa51f1cdaa42f8b13f1ecb5332bee3911df7eaa1f9` | 热/冷host owner、pending注册OOM、move/swap/release/reentry、精确池base复用、registry锁外new_handler、replaceable-delete eviction/完整move、TLS退出与late global destructor。 | 八worker线程创建的是ordinary owner，各自独占；不能把这些线程等同为foreign真实canonical owner。 |

以上四个程序与R1候选内对应文件字节完全相同。存在这些程序或C14历史通过都不能写成R1本轮normal/SAN通过。

## 新同步混合owner用例的真实风险

1. **发布与mapping寿命**：foreign线程构造真实verified-receive C owner后，必须通过明确release/acquire同步把只读地址/输入与阶段状态发布给reader。owner/mapping必须持续持有到reader确认结束。保留OwnerRecord或看到global active count非零，不等于pin住mapping。
2. **无关C导致ordinary路径进入active helper**：普通owner在global count零、foreign C活动非零、回零后均验证完整值和`canonical_bytes_at`的null结果，不能仅比较ledger数值。普通owner registry非空不应自身增加canonical count。
3. **跨线程cold/hot与来源域**：在未由本线程创建C owner的线程上恢复真实C地址，确保foreign场景确实经过本线程cold/TLS域；重复恢复与正宽/one-past行为保持。不能靠预先读同一owner把所谓cold条件暖化，也不能只手造record冒充真实C发布。
4. **退役时仍有其他active C**：至少一个退役查询阶段保留另一live C，使global count仍非零；否则零快路会掩盖dead-record校验缺陷。最后C退役回零、再发布新C也须有明确阶段断言，避免跨调用缓存零值。旧地址只用于既有flat-address恢复接口的数值查找，禁止解引用已释放payload。
5. **移动/复用/线程退出**：真实move保持相同mapping的归属结果；退休/释放/新发布与reader明确串行，禁止同时退役reader正在读的mapping。新线程TLS起止与主线程TLS相互独立；既有确定性池复用和terminal TLS回调程序作为补充，不扩大并发合法域。
6. **重入是真实边界**：若新例声明覆盖mixed-owner重入，必须使实际new_handler/replaceable-delete/注册路径触发，并有独立控制证明callback确实执行；不能直接手调callback后宣称生产重入。回调只能改变已证明独立或已同步的owner，不在reader持裸指针期间并发释放其mapping。四既有测试的相关控制需按同身份normal/SAN实际执行。
7. **fixture也必须无死锁/UB**：阶段通信采用锁/CV、barrier或release/acquire协议而非sleep猜测；异常路径也要释放等待者并join，避免fixture hang伪装库故障。SAN不证明这些生命周期前提或ISO对象寿命，源码同步/所有权证明仍需独立成立。

## 实际请求到达后的准入检查

按PLAN B，只允许四旧程序normal/SAN共8执行，加新同步混合程序normal/SAN共2执行，合计恰10。请求必须事前给出其精确SHA、不可覆盖输出、14/154完整候选身份、基线/固定依赖/profile/SAN资源、所有新测试/工具/输入和完整argv/预算。不得夹带diag、fixed-M、describe、warmup、codegen重编或screen。实际wrapper接入原run_job/原锁，其真实当前状态、资源和工具自测由root/唯一执行者核对；本源码审查不转移执行权。

当前缺少实际REQUESTS_R1_NATIVE.json、新同步fixture/其源码review、最终wrapper工具闭包及固定argv预算，故不能给可执行批准。它们到达后只对该精确请求SHA复审，不凭候选主头相同转借准入。

## 请求与同步fixture到达后的审查进展

已读实际请求`bcdef196cb2e7f3baa6439c16a145ac79528afd972f544207ef6785269de6833`、新fixture`d0b1a99c244247cee70ddf9d6db812915367546197bc6182a3b5f7c70979640b`及core worker`35b8d1928b2cb166935fa83e438970b22ce1e8eab7d87f8ae7521b4d5ded6f2a`。完整387项注册输入与实际文件匹配、实际复用WSL source154成员匹配；20个命令恰为5normal+5SAN编译/运行，compile120秒/native20秒，最坏child总1400秒、outer1500秒，无额外diag或计时。此记录是中间版本事实，最终准入将重新绑定核心最终请求/fixture/review身份。

新fixture所有权证明已读：worker构造真实C，在mutex下向optional发布并等待main确认；main独占读、move/swap及活跃分配复用后在mutex下归还；worker检查并退役、核对count=0，再通知main。main在通知之后且下一round尚未获准创建新C之前查询旧数值anchor，未解引用已释放payload。两轮重复0/1/0；ordinary copy只由main读写，wire在线程启动前完成并只读。error/cancel/phase/optional均在相同mutex保护下访问，失败通知并join，最终error在join后读取。本源码证明不依赖SAN结果。

集合覆盖按四legacy与新fixture组合评价，**不要求新fixture重复legacy已有控制**：多C仍active时退休一个的dead-record路径由`test_canonical_registry_v2`七owner场景覆盖；真实new_handler/replaceable-delete与TLS退出由既有三个相关程序覆盖。新fixture补foreign真实C发布、同步跨线程独占交接、ordinary在global非零时仍非canonical、last-C归零及再发布。

**TLS措辞限制**：canonical cold body明确不安装cold结果。`check`中的repeat=2本身不能证明第二次是canonical TLS热命中；应记为foreign首次及重复查询，热路径另按legacy实际控制核对。core正在固定最终测试/文档/请求措辞，当前仍不写passed。

validation外层曾将SHA-only inherited runtime项与包含bytes的完整fid字典直接比较，已向root报告；root当前实现改为SHA必检、bytes仅在注册时检，且在最终review中要求core review绝对path+identity。core worker本身只观察“某个lease被占”，因此执行准入必须绑定root的实际原runner/W1 validation/固定leaseout/完整argv，并在退出后核对source154、input、真实queue身份与cleanup，不能单独运行core worker。此项是外层必要执行条件，不是给core worker增加第二锁。

## 最终v2请求准入

最终`REQUESTS_R1_NATIVE_v2.json` SHA **`edcf2fb2d29f5d5249d8d1ebc2d7727bafcd2d44a503ff1bacd839018edf7d4a`**已审查通过，`R1_NATIVE_REVIEW.json`现为passed，仅准入B批次。新fixture`test_r1_thread_handoff_v2.cpp` SHA为`4ede309cf78c7e62faa16960470298c174ac4418512897a2a6d8b42994ff2cd6`，最终core review为9905B/`fabe8f031143eb87516475c44338b0780c6897b3992f108fdd2bfaafcab702a1`，root entry为`bca349f8be2b32a31c23b59055e9717b48575a78ebc174351d57364459dde88c`。

独立逐项核对390项输入、实际WSL source154/14、完整20命令。v2相较未执行的v1仅更正fixture注释/成功输出及换行，并绑定新请求/测试与fresh `r1-native-v2`输出；实际同步、断言与所有权动作不变，预算没有增加。恰10编译/10执行，5normal+5SAN，120/20/1500秒。旧请求superseded且未执行，不构成第二批。

重复读取只声明稳定foreign domain结果，不宣称自动TLS热命中；四legacy真实cold/hot/多owner退休/回调/TLS退出与新同步交接按集合覆盖。SAN不替代寿命证明。root须按指定原supervisor/lease运行、后验核完整source/input/queue和cleanup；无C/diag/full/screen/QF准入。本审查没有编译或native执行，native正确性仍为not_run。
