# R1 screen 前的既有源码、裸 codegen 与成本绑定

这是提交 root 独立审阅的既有 gate 证据收束，不是 screen freeze、执行授权、晋升或性能接受。`R1_SCREEN_GATE_BINDING.json` 机械绑定精确154/14、原请求/实际compile receipt/汇编、当前裸profile、已完成的R1专属正确性和资源记录。本次新compile/native/queue均0，不写validation或reviews，不修改任何现有pin。

## 裸 codegen 与当前候选

独立R1仅改变一个生产成员，main `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`，patch `0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad`；不含M1/M2。当前候选、原codegen staged source和原实际receipt的154成员必须一致。原compile实际returncode0、actual_compiles1、native0；实际argv是P2996/libc++、gnu++26、reflection/expansion、pthread、裸`-O3 -DNDEBUG -S`，没有SAN、`-UNDEBUG`或诊断hook。完整argv和原89项输入记录保留在JSON中；不重新编译控制。

控制`control-driver.s`来自历史m1-v2目录，但它是相同C14控制的汇编，绝不借M1候选的功能通过。实际SHA `110bc1bc494a1d731c563660d5760a630d5bb0da39531eb6e43e83bd04479640`；R1实际candidate-driver.s SHA `af67def8d0bd08676065b905755b83b9fdc2a49710a4554890a3598d961258f3`。本次读取实际字节并逐条核对原记录行和分支：控制query16处outlined canonical调用、helper在ledger load前5个push；候选query14处独立load/test/zero branch跳过14处active-helper调用，active helper仍outlined。16与14是静态发射位置数，不是动态工作次数或按比例加速。当前裸screen二进制属于后续业务profile，不冒充原`-S`产物；实际profile/compileDB/compiled-source binding另行引用，两个阶段均保留其真实身份。

原汇编文本1779270→1840476B、query944→960行，属于汇编文本增长，不能称ELF text增长或周期成本。JSON另外只读解析当前已有裸二进制的ELF executable sections及文件大小，供root接受明确的构建/代码footprint边界；没有新构建。其结果不等于指令缓存成本测量。

历史61B限制如实保留：原R1 admission/receipt/result输入表确实列出outer launcher61B/c8ea06…，但这份既有记录不自动补足所有早期控制/定向B的转发链before/after完整证据；不能把当前wrapper字节或新profile的显式pin追溯为旧控制的历史双时点证明。原汇编、原receipt和对应源码事实仍可直接审阅。后续full/business/current bare profile与capture显式绑定outer61B；不为补旧元数据重跑任何实验。

## 安全与完整验证

每次public canonical调用仍恰好执行原acquire load，zero立即返回；nonzero进入私有noinline helper，不作第二次ledger load，也不把本次零状态留到下一次。旧cold body从anchor转换至return recover(record)逐字节相同：token起点3961 ASCII B，若包含首行12空格则3973B，两个数字的范围差异已明确。anchor优先/one-past、target bounds、canonical origin、TLS四槽及last、mutex/shared-record作用域、owner acquire与退休顺序、cold不安装TLS和terminal callback避免均保留。

没有增加field、host cache、arena/layout、registry/TLS状态或跨调用pointer/negative cache，没有增加注册/维护/分配/free/销毁操作；wrapper不持有新owned对象，局部record在active helper返回前销毁，原相对顺序不变。active非零路径可能承担额外call/frame或分支，不能因主体相同就宣称成本为0。同步所有者支持域保持；不扩大到无同步并发payload退役。SAN与真实failpoint/callback验证是补充证据，不是所有ISO规则的证明。

原独立`REVIEW_R1.md`、当前定向B5normal+5SAN（包括四个canonical/owner程序及同步线程交接）、R1自身full73+73、业务SML80+80、main V1 84+extras8和AB4/C4、安装/子项目/relocated/archive消费端与共享runtime、lifecycle52均有已通过的实际R1证据和最终source/receipt闭包。OOM/new-handler/replaceable-delete、真实owner retirement/move/reuse和完整query/九阶段canonical/wire真值按原测试范围保留。JSON列出原始refs和独立R1_FULL_ACCEPTANCE；M1功能结果不替代任何一项。

## 工程成本和仍待唯一screen的判断

可在screen前收束的边界是：只改变函数组织，无新持久内存/对象布局/owner维护/销毁工作或output契约；已有裸构建成功、功能/生命周期通过；当前完整process+truth审计资源投影在事前wall/RSS/allocated/双卷余量策略内。原policy、实际两role capture与queue、input staging、资源projection均保持独立ref。投影166.75389404699882s、physical2915420513B通过其注册范围；staging1.275457023s/62218240 allocated B已收费，144份wire独立，保留每child0.25s tail与原seal/nonchild余量。这只是有限执行可行性，不是比较速度、长期内存分布或未来尾部保证，也不等于额外active call/frame免费。

完整fresh owning输出、header/payload复制、retire/reload/cleanup、V1/支持域和所有生命周期责任不变。此前diagnostic/capture的单次elapsed、CPU、fault和峰值不用于AB收益，不删除成本，不用native.total代替process wall。code footprint的现有字节规模是已披露的预试验工程成本，instruction-cache/分支/active-path耗时和阶段成本转移仍需要唯一72配对实际结果；未添加新的私自阈值或省略既定门禁。

R1主目标read_sum；九辅助是原八阶段加derived total，原四主/36辅助族与df71保持。只有完整72对、所有metric可定义、primary联合上界严格<1、无任何辅助联合下界>1、且全部预声明工程/语义/资源/恢复/审计门禁成立才可晋升。辅助跨1不代表无退化，任何metric（包括辅助）合法零导致undefined即阻止晋升；失败窗口不补样、不算前缀收益。load、mutate、emit、retire、reload、cleanup与total均不因主read_sum成功而豁免。root对当前三gate的独立接受与最终screen freeze准入另行记录，当前绑定不写这些结论。
