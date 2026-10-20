# M2 唯一筛选：工程和恢复结果窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**工程和恢复审查通过。** 精确结果见同名JSON。复用已审M2源码、自己的完整门禁和实际完整body审计；本次未编译、未执行项目native/新queue/新实验、未重hash大body，也未重计72对统计。总体采用决定由统筹结合另一个独立统计审查作出。

## 完整窗口与来源

freeze为 `d74af43ad891b87873b7bdb1bf4ce9bc0fd79eca4fcb4474b58c1c7af386fab2`。唯一窗口恰72对、attempted144、completed144、unexecuted0；job与冻结schedule完整匹配，恰144份receipt，无替换/补run。全部准确argv/environment、exit0、无timeout/launch_error、stderr空，非重叠且各≤20秒；最慢完整进程0.47152953秒。

timing原队列 `96298e1f7ffc4f4194fe948e3e0e8faa` complete/exit0，**91.617711512秒≤600秒**，cleanup三项全空。audit原队列 `ae4bc5c61ce0470399fdccddd7e4180a` complete/exit0，**15.334364290秒**，cleanup全空；AUDIT内部15.045294127秒另列，不能当成完整外队列时间。

已窄核874份小metadata的SHA、两原队列各六metadata文件、postrun及AUDIT pins；1308当前文件名称/尺寸/普通类型匹配manifest，288行dedup当前inode/尺寸与byte-exact记录一致。完整body/oracle读取由原已完成audit承担，本次没有再次hash大body。`runtime-before == runtime-after == freeze.runtime`；准确M2 bare候选和C14控制各自来源、完整功能和资源门禁按此前实际准入复用，不借R1通过替代M2。

## 空间与成本

当前总计费allocated为 **2,732,118,016B<4GiB**，包括输出2,592,727,040B、retained capture75,341,824B、冻结metadata1,830,912B和staging62,218,240B。预声明projection为2,915,424,609B及176.089992063秒，staging的1.275457023秒独立列账，未隐藏保留/准备成本。逻辑apparent与硬链接去重后的allocated明确区分。

两角色after-audit RSS最大均119312KiB，低于16GiB门槛；这不是内存等价证明。现有局部data复用没有引入持久cache、arena字段或额外维护生命周期，源码和实际工程门禁的原接受范围保持。

## fresh guard 与恢复

本次独立plan于19:17:13.9894511Z创建，SHA `52148606...`，token为 `c701106f81e7465593312968ea4171e4`，与R1状态分离。plan/request/ready/completed的两目标完整PID/image/SID/creation身份和pin一致；held handles=2，两个目标各一次owned resume状态0，wait均258、未外部退出。780秒guard按`requested_stop`完成，没有提前watchdog恢复。

完整覆盖链：`ready-observed 19:17:15.713075Z ≤ timing开始19:17:16.417598Z ≤ timing结束19:18:48.038682Z ≤ stop请求19:18:48.117657Z`。guard完成于19:18:48.2065642Z，audit于19:19:20.983002Z才启动；实际恢复先于audit。覆盖使用WSL自身记录，不推断Windows/WSL时钟偏移。工具未改power，前后均为Balanced GUID `381b4222-f694-41f0-9685-ff5bb260df2e`，恢复记录error/power_error为空。

## 不能扩写的性能结论

AUDIT报告主目标mutate point=0.9538474986、联合上界=0.9780941220，属于本窗口 **M2÷C14自身控制**。独立统计计算由另一个审查负责，本文件不重新计算或代替其结论。

九个辅助指标区间全部跨1，没有lower>1，未触发预声明“已确认辅助退化”否决；**不能写成无退化、等价或总量改善**。例如总量区间[0.94685,1.02443]、读取[0.96751,1.02451]、输出[0.86903,1.14399]、重载[0.95586,1.02964]，均仍不确定。

本轮不是M2与R1的比较，不推断二者组合收益；固定M、ordinary-owner、已登记WSL/P2996配置之外不作性能推广。正式四竞品1.05/0.20目标及Q/F仍未验收。
