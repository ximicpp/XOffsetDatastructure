# F64 host 源码独立接受

结论：`passed_static_host_source_only`，未发现真实阻断。本接受仅覆盖最终源码及草稿的静态差分、parser/dataflow 和调用闭合，不签 root 准入，不证明实际 F freeze、host 覆盖、恢复或性能通过。

最终 `core/formal_host_v1/F64_HOST_SOURCE_READY_v1.json` 为4894 B、SHA256 `f5b6b430838c32b2cbf4dda52f78531f06a51478e38b9aa883f5e91a92fffb41`。其11份 artifacts引用全部与实际字节一致；读前后最终源码稳定，接受的是核心已封口的以下身份。

| 文件 | 字节 | SHA256 |
|---|---:|---|
| execute.py | 19409 | `5ead5e942d97698196a4e1a2b99799f3f44e19957c51073dc41e4062d0af9a6c` |
| process_pause.py | 16289 | `b5c4d5890309bab3bdd68e5e271b0c9978834741760dbce55435ce3dd78549b8` |
| pause-control.ps1 | 12670 | `0898a5f0e1be74368be71161be970db6e85ae63c4782df8638aa705f471e5465` |

## 实际差分与参数流

独立核对真实Q→F diff，并在内存逐字节重放76/6/2项 UTF-8 byte edits，全部复原三份实际F源码。Q32三份原文件实际身份不变。独立重hash原bound11共266088 B，全部等于此前已接受身份；没有修改原Q资格覆盖的工具闭包。

execute.py只改变 `pause_module`、`prerequisites`、`prelude`、`main` 四个函数文本，以及对应globals。其余九个函数文本逐字一致，函数集合与顺序一致。process_pause.py严格等于原文件替换独立formal state路径与2820→5520；PS helper严格只有2820→5520两处字面量变化。作者的 AST-only记录已绑定；本审查没有执行Python或重复其AST解析，parser/dataflow由实际源码阅读与上述文本/字节证明核对。

main保留run/prelude两个action、五个必需path flags和两个SHA flags。新PIN初始为None，解析CLI后先要求64位小写hex，再赋给global，随后才加载固定hash的pause模块并执行prerequisites；prelude通过同一完整tail重新绑定相同PIN。没有在global声明前使用PIN。请求必须与原直接runner5400/wait0及bound entry/formal逐项argv一致，actual F manifest、路径、root REVIEW/HOST/QUIET和request身份共同绑定。

草稿status为 `DRAFT_NOT_EXECUTABLE_NOT_ADMISSION`，freeze SHA为null，命令含不可执行placeholder，qualification ref身份为null，execution_admitted=false。它无法通过实际ready状态、SHA、manifest和root-record检查。未来应另建真实请求，保留草稿。

## Q 绑定的分层闭合

新增块在任何host action前要求 REVIEW pin 当前 `COORD/qualification-acceptance.json`，当前文件hash必须等于formal冻结副本，timing request内也必须出现完全相同qref。三个 evidence ref的实际身份必须匹配，路径固定到本次原Q audit和 `qualification-v1/queue/request.json/result.json`。它要求q的passed/qualified/complete以及固定postrun pin `b3db8adafebb4079f65f7557e00917ac108a71184dfcb93ed7bfc2df992449ce`，并要求实际原audit为qualification/pass/complete/qualified、4656 actual/2328 primary、无budget violations和同一个queue-result identity。

该块不是重复实现完整Q verifier。完整30项 A/A、Q queue inventory/wall、run/frozen/queue manifest、复合pin和当前bound11由原formal freeze的 `require_qualified_q()` 检查；F run本身仍不重新读取root Q acceptance。新的host绑定利用实际root接受及冻结副本身份保持这一链条，不能替代原freeze或root工程接受。

## 时长与恢复路径

实际formal timing为5400秒，W1/validation prelude仍为60秒，交接新鲜度仍≤30秒。Python default/upper limit、PS default/ValidateRange及实际guard请求均为5520秒；两次紧邻dispatch的保守剩余量均要求≥5460秒。计时用controller timestamp早于独立Windows guard启动时钟，因此计算保守。所有formal controller/host/prelude/process/power/output/frozen路径与Q分离。

原 `PowerWindow(MAX_SECONDS+60)` 对formal为5460秒，其7200秒上限已支持；原bound power module/helper未变，launcher仅查询power，不setactive，也不改原stop/state。保持原helper拥有临时power和独立恢复。成功后另查实际ready的5460、原方案和目标方案；launcher等待完成最多90秒，完整power interval仍须原offline audit核验。

parent在prelude、交接、dispatch或timing异常后均进入原finally：若process request存在就请求自身token的owned stop，随后等待原PowerWindow恢复并核实际original方案。PS启动失败/异常/finally与独立watchdog、SID/PID/creation/image、持有handle、token和一次owned resume均由原逐字相同主体保留。恢复或coverage失败会拒绝，保存现场，不产生重试；静态检查不代替实际恢复证明。

仍只允许已有明确授权的 `client_ue4-Win64-Shipping`、`TGitCache`，没有扩大目标、kill/restart、旧PID/state复用或机器全静默断言。新OUT/PRELUDE/process-state/power-state/controller须不存在；controller的mkdir及create-only ATTEMPT_CLAIMED在prelude前完成，独立F ledger要求maximum_windows=1/retries=0。W1结束后才以原W3 timing lease直接运行，不嵌套租约。host controller不启动offline audit，实际恢复完成后另循原审计准入。

## 封口状态及范围

本次开始读取时实际Q qualification acceptance尚不存在，封口时观察到它已生成；本审查没有读取或接受其工程结果，不据此签任何实际资格。原Q audit report为441839 B，完整审计/统计通过是父会话通知，工程接受由对应审查负责。

封口时实际F冻结包及建议路径上的F REVIEW、HOST_REVIEW、GLOBAL_QUIET、外部attempt ledger均不存在。原统计、9264 actual/4632 primary/4632 conditioning、24 warmup/4608 inference、64 macrocycles、30 A/A、12 primary和40 auxiliary均未更改。实际F freeze/resources、qualification acceptance pin、新root records/quiet/host/source/request和唯一窗口准入仍需各自真实产物。

本审查项目Python/入口、编译、native、capture、queue、freeze、probe、pause、power、跨库统计均为0；未修改任何源或冻结文件，只新增本审查JSON/Markdown。不得将此源码接受用作实际host/runtime、F64准入或性能目标通过。
