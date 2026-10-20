# R1 + M2 实施与正式验收结果

本轮有限实施计划已完成；正式性能目标未全部达成。完整审计与独立复算一致：Q32、F64各30/30项A/A通过，F64主目标仅3/12项通过，候选不晋升默认实现。默认源仍为C14，已验证代码以独立候选和补丁保留。

正式口径为 **XOffset候选 ÷ 同批各竞品**。下面三列是原12项主目标的联合单侧95%上界，门槛依次为1.05、1.05、0.20；不超过门槛才达标。

| 竞品 | 读取合计上界 | 修改上界 | 独立输出上界 | 通过 |
| --- | ---: | ---: | ---: | ---: |
| cista | 1.184597 | 1.835824 | 0.471482 | 0/3 |
| FlatBuffers | 1.042315 | 1.603829 | 0.419917 | 1/3 |
| Cap’n Proto | 0.405309 | 0.560808 | 0.526898 | 2/3 |
| cereal | 1.144953 | 1.704362 | 0.463736 | 0/3 |

九项失败的点估计本身也超出门槛。读取仅对FlatBuffers、Cap’n Proto达标；修改仅对Cap’n Proto达标；独立输出四项均未达到0.20。

重载保留完整校验，沿用固定竞品模式，不新增比例门槛。以下是原40项辅助族的双侧区间，与主目标上界属于不同族。

| 重载比较 | 点估计 | 原联合区间 |
| --- | ---: | --- |
| XOffset / cista | 1.909181 | [1.885631, 1.933024] |
| XOffset / FlatBuffers | 4.577373 | [4.529286, 4.625971] |
| XOffset / Cap’n Proto | 0.888923 | [0.880238, 0.897694] |
| XOffset / cereal | 3.473098 | [3.433507, 3.513144] |

同批相对C14的收益只用于说明实际改善，不能替代竞品验收，也不是R1与M2两次筛选百分比相加。

| 相对C14辅助指标 | 点估计 | 原联合区间 | 判断 |
| --- | ---: | --- | --- |
| 读取合计 | 0.845786 | [0.839728, 0.851889] | 已建立下降 |
| 修改 | 0.762778 | [0.757190, 0.768408] | 已建立下降 |
| 独立输出 | 0.993742 | [0.986164, 1.001378] | 未建立改善 |
| 重载 | 0.976073 | [0.969270, 0.982924] | 已建立下降 |
| 总量 | 0.886299 | [0.881334, 0.891293] | 已建立下降 |

读取、修改、重载、总量点估计分别降低约15.4%、23.7%、2.4%、11.4%。全部40项辅助结果保留：21项下降、16项上升、3项不确定；三项不确定是对C14的输出、退休、清理。既定5项成本转移否决未触发，不能写成所有阶段无退化。初载、退休、清理与全部辅助数据见RESULTS.json及原审计。

R1保留每次acquire检查，将inactive入口与active helper拆分；M2只复用当前非string owning-vector节点的一次data解析，保留capacity检查先于size、双向owns及递归顺序。真实修改机制移除48,393次canonical解析；相对诊断控制阶段8,421,400次调用仅约0.575%，不能将局部4→1外推为整个修改减少75%。

独立输出仍需物化17,830,872B。固定诊断中payload复制区段约5.672399ms，包含4,352次minor faults与边界开销；它不是纯memcpy带宽或正式计时收益，不能另加一份缺页时间。现有resize_and_overwrite已直接构造最终string，R1/M2没有减少payload字节，也没有建立五倍独立输出的新机制。后续需新的安全结构性依据，不能改demo、预触页、移出准备成本或用可选view/lease接口替代。

组合自身功能门禁已通过：normal73/SAN73，业务normal80/SAN80（S/M/L完整真值），V1主84+额外8、AB4/C4，消费端29、生命周期52；现有demo、完整校验、V1和独立拥有string语义保留。具体源码及作用域见SOURCE_CHANGE.md。历史M1超时和L1否决保持关闭，未重开、补样或改阈值。

F64固定9264次实际调用，4632主样本与4632conditioning，剔除预定24次主warmup后4608进入推断，64宏周期/df63；原始八阶段向量全保留，无过滤或替换。计时4620.436771s≤5400，完整审计1169.531633s≤3600，两个队列exit0/cleanup为空。真实进程恢复、完整队列暂停覆盖、原native PowerWindow与Balanced恢复及审计前因果引用均通过。

完整generated wire为157,351,335,928B，primary为78,675,667,964B；27,792份body proof、7个anchor。逻辑ledger retained为322,928,599,500B，原审计全输出为322,961,182,807B；ledger检查点unique allocated为307,003,392B，这不是peak RSS。原预算全部通过；完整字节量未因退出后验证池化而减少。

结果限于本机WSL Ubuntu22.04/Linux x86_64及既定Clang/libc++完整P2996/C++26工具链；storage-only C++17要求未变。未宣称Windows/ARM原生通过、物理核心独占、频率锁定或全机器静默。竞品固定为cista0.16 trusted offset/NONE、FlatBuffers25.12.19 trusted object API、Cap’n Proto1.5.0 trusted bounded reader/builder、cereal1.3.2 trusted binary/native-std，模式未更换。置信条件沿用原模型条件分族，不把A/A、主目标和辅助族合称整体95%。

基线提交：`1904b9c71230026ac6f2d58f919a9ab76447d264`；候选main SHA：`155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501`；补丁SHA：`02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb`。未提交、推送或发布。各证据中的本机绝对路径用于身份追踪；换机需重新配置和验证，完整性检查不能当成平台测试。

交付：[代码说明](C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/delivery/SOURCE_CHANGE.md) · [补丁](C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/delivery/COMBINED.patch) · [机器可读结果](C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/delivery/RESULTS.json) · [独立统计复核](C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/reviews/F64_STATISTICAL_RESULT_ACCEPTANCE.md) · [独立工程复核](C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/reviews/F64_ENGINEERING_RESULT_ACCEPTANCE.md)。
