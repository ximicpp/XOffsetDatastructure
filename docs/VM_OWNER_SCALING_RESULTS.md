# Fixed-request owner scaling：功能与诊断结果

2026-10-16，D018 / D022。已完成26个原生 workload：18个成功样本（6 diagnostic/R256、6原始F/R256语义参照、6原始F/R8192容量功能检查），及两构建各4类精确失败控制。13项Python合成测试在正常与`-O`模式通过；原始记录的3项离线坏副本均拒绝。没有正式计时或性能结论。

每个条件固定全进程总请求数，owner i始终属于worker i%T。直接使用Core与uint64 allocator；四阶段create/allocate/free/destroy，全owner创建后才分配、全请求释放后才销毁。填值、逐值校验、对齐/范围/不重叠、free守恒、线程启动和输出在四段时钟外。功能记录的wall值包含barrier及调度，全部标记timing_evidence=false，不是单次registry lookup延迟。正式计时只由W3另行执行。

| owners | workers | 完整events | lookup hit | registry lock | metadata snapshot | 唯一末桶bytes |
|---:|---:|---:|---:|---:|---:|---:|
| 8 | 1 | 1981 | 512 | 528 | 264 | 88 |
| 64 | 1 | 2891 | 512 | 640 | 320 | 776 |
| 256 | 1 | 5135 | 512 | 1024 | 512 | 3176 |
| 256 | 2 | 5135 | 512 | 1024 | 512 | 3176 |
| 256 | 4 | 5135 | 512 | 1024 | 512 | 3176 |
| 256 | 8 | 5135 | 512 | 1024 | 512 | 3176 |

每条诊断均有256成功allocate与256对应free；register/erase各C、active峰C/结束0，完整generation、phase和固定worker归属通过审计。Core new/return各256，reserve/enable/grow_zero/unmap各C，zero bytes=65536*C。无scan/reuse、extent、pool、wire、growth或失败事件。domain1 vector分配/释放与容量序列完整且最终归零；domain2节点全归还，只保留表中一个共享registry bucket，按完整分配lineage核验。lock wait/hold只作受扰动诊断，不加总冒充阶段wall。

D022更正草稿检查器对齐假设：本Apple arm64 profile有效alignment=8，用户地址base+64+24*j，Q次后used_end=48+24*Q。最大Q1024时used_end24624，低于每owner逻辑/最大65536。harness本身使用真实alignof/sizeof及守恒检查，二进制无需重编。首份原生记录退出0、1981events，其原收集器退出1及failed队列回执保持原样；修正审计器单独冻结并离线补审该记录，然后只执行剩余25个唯一case，首例没有重跑。

失败控制精确覆盖部分线程启动（2个已起线程join、零Core）、第4个Core前失败（3创建/销毁）、第8个allocate前失败（7分配/清理free、8销毁）、主线程值核验失败（validated7、256free、8销毁）。均退出2、精确原因匹配、清理/join完成，不计为成功样本。另保留3次构建失败（依赖路径别名检查两次、引入的8MiB编译文件限额一次）和一次零原生进程的Python预检kind错误，未隐去或改写为通过。

生产F双头及已接受W2生成头/runtime未改；实际传递闭包为timing1018、diagnostic1019文件，证明诊断头未被F遮蔽。保持/usr/local/bin/clang++执行拼写，冻结flags/compiler/SDK/stdlib/unwind/link工具与源文件身份。W2 recorder仍为16 lifetime线程槽、每槽16384events，无reset/采样/截尾；最大实际整进程5135events，低于5631硬上限。

证据目录：`docs/validation/vm-owner-scaling/final-v1/`。`final-audit-v1.json`核验26个case/claim恰好覆盖、两profile同R checksum一致、原首例补充审计、原/新工具身份分离及完整队列回执。`recovery-v2/`明确历史队列failed与新离线passed的区别；`history/`保留原准备/构建/修正工具版本；`receipts/`包含所有构建/收集失败与成功。manifest与归档提供文件完整性校验，不是签名认证。
