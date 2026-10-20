# R1 唯一筛选：工程与恢复结果窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**工程和恢复审查通过。** 本文件复用已审源码、完整门禁和原完整body审计；没有编译、项目native、新实验或独立重算72对统计。精确机器结果见同目录 `R1_SCREEN_ENGINEERING_RESULT_REVIEW.json`；总体采用决定由统筹结合独立统计复算作出。

## 完整窗口与证据

- 实际freeze为 `c168eed40879c5339e91a390506eb6c32cce73fc47ea407ab7ae51cb6a437cf3`。唯一窗口恰attempted144、completed144、unexecuted0；全部job与冻结schedule顺序相同，恰144份receipt，没有替换、补run或后缀重开。
- 原timing队列 `0162bed5befc4b45837fbc81f1250fed` complete/exit0，93.779881469秒≤600，cleanup三项全空。144进程均exit0、无timeout/launch_error、stderr空，真实argv/environment与冻结相同；全部非重叠且≤20秒，最大完整进程0.426200806秒。
- 独立audit队列 `836e4f6831584965be06a124ce0bf5a1` complete/exit0，15.034238107秒，cleanup全空；其实际已完成完整body/oracle审计保留。
- 本次另外只读核874份小型元数据的SHA、两原队列各六文件manifest、postrun与audit pins；1308当前文件的名称/尺寸/普通类型匹配manifest，288份dedup记录的byte-exact声明、当前inode和尺寸一致。没有重新hash大body；不能将复用原完整body审计表述成再次独立读取了2.5GB。
- `runtime-before.json == runtime-after.json == freeze.runtime`。source/tool/runtime未发生前后闭包漂移；复用此前审过的154/14、bare产物和完整正确性门禁，不把诊断二进制当作screen产物。

## 资源与真实成本

当前计费allocated为 **2,732,113,920B<4GiB**：screen输出2,592,727,040B、保留functional capture75,337,728B、冻结metadata目录1,830,912B、staged inputs62,218,240B。包含staging和保留成本；`allocation.json`是最终status/manifest封存之前的快照，不拿它较小的数字代替当前总量。逻辑/apparent字节量较大来自canonical/query硬链接去重，不按apparent误判allocated门槛。

预登记资源投影2,915,420,513B、166.753894047秒；已单列input staging的1.275457023秒和62,218,240B，不隐藏为库优化或从旧接口计时中删去。两角色before/after条件相同，staging本身的端到端I/O效果不是R1阶段收益。

两角色after-audit RSS最大均119312KiB，均远低16GiB门槛；这是观测值，不是内存等价证明。已披露bare binary文件225856→233448B及原代码/I-cache风险；没有新增持久cache/index或arena维护状态，前置完整功能/生命周期门禁仍按原接受范围成立。

## 宿主身份和恢复

真正prelude plan为 `8b84ea89...`，区别于预审plan；request/ready/completed中的两个PID、image、SID、creation FILETIME及start_time一致，held handles=2，token均为 `970abf2233c2462f98aea141207af066`。目标仍只有TGitCache和游戏client；两者各一次owned resume状态0、前后process wait均258且未外部退出。guard为780秒上限，正常`requested_stop`完成，没有watchdog提前恢复。

实证覆盖链为：

`ready-observed 18:31:11.032025Z ≤ timing开始18:31:11.789693Z ≤ timing结束18:32:45.573923Z ≤ stop请求18:32:45.657347Z`。

guard在18:32:45.8900338Z完成，audit在18:34:01.435294Z才启动；恢复先于audit。覆盖比较采用WSL observed/queue/stop记录，Windows原始时间保留而不假设跨时钟偏移。工具未切power，前后均为Balanced GUID `381b4222-f694-41f0-9685-ff5bb260df2e`；恢复记录无error/power_error。

## 成本转移与结论范围

按预声明规则，AUDIT没有任何辅助项lower>1，未触发“已确认辅助退化”否决；但load、emit、retire、reload、cleanup的区间都跨1，**不能写成无退化或等价**。特别emit区间约[0.9044,1.1531]、reload约[0.9354,1.0058]，输出和重载的变化仍不确定。

AUDIT报告read_sum比值0.8428788467、联合上界0.8546632724；这是R1相对自身C14控制的固定M结果，独立统计计算由另一审查负责。本次只证明该工程/恢复证据可以参与总体验收，不把它扩写为所有owner、active canonical路径、生产全部负载或竞品优势。

读取≤1.05、修改≤1.05、独立输出≤0.20仍须同批逐竞品正式验收；当前没有Q/F或竞品达标结论。
