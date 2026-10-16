# 独立源码与证据审查

日期：2026-10-16（Asia/Shanghai）。审查者：`/root/existing_coverage`；主代理据其只读结论整理。审查者未编辑文件、编译或运行 C++。

结论：通过本轮冻结 20-anchor 有界网络诊断验收，无未解决阻塞项；不关闭完整 N3。

源码审查在首次 workload 前发现并推动修复预算输出路径、恢复前提复核、ledger 命令绑定、实际 diagnostic 头与 foundation 身份绑定、sidecar/build/input 交叉身份以及 endpoint terminal_count 的实现错误。原 W1 每端点只有 receive_frame 的一个 TerminalEvent，Sample 的完成计数独立；最终审计要求 endpoint=1。封存前预算复核已加入。首构建链接失败保留；根因是解析 clang++ 软链接后丢失 C++ driver 调用名，第二次保留 clang++ 名称后通过。

冻结 runner 对 build-v2 与 collection-v1 的离线审计通过；另直接读取 raw 事件复算：

- 32 attempts 全部 complete、returncode0，16 reference +16 diagnostic，copied ledger 与持久 ledger 一致。
- 16 trace 全部 valid=true、dropped0；最多两个线程，每线程最多129事件，低于2048硬上限。
- 三个实际编译依赖唯一解析到 build-v2/input/source，内容哈希匹配固定 foundation snapshot。
- 四 fixture 的 auto_growth=4、extent_request=8，是相对原基线新增的两种 native 路径；本批实际23种kind。
- 十二网络 case 共28次交换，包括完整4次warm；注册/注销及两种load各68、两种save各28、pool_scan16。
- warm 每case7个registry代次、3段mapping（登记分布1/3/3），没有把mapping复用当成活跃owner并存。
- 每进程唯一保留项为domain2原始16B bucket；fixture分配sequence5@fixture.build，run为sequence6@init.preflight，origin_owner/thread均回溯首代base/thread；domains1/3/4/5结尾为0。
- fixture、全部work、warm相对原始W1 work0 oracle，以及跨pool模式的逐字节比较通过。
- 实际小型搬迁副本的1正控/11负控只读复核，异常类型和原因与controls.json相同；包括重新签名后错误diagnostic头、删除warm前缀、native binary字节变化。

关键身份：

| 对象 | SHA-256 |
|---|---|
| build-v2 result | a4a2da454b1f1ebfbd536cf63aa49d1abbbeb42c00123f1238e193ab91a66c53 |
| collection-v1 result | 9a5924cd9e7297dfa7adc2c0de4a7b98ef8e10ed1ebae2bacb6eef0e35143de2 |
| frozen runner | 9ea3b73645ab8581518d71f03c6689b4b3d0b80d879242ccc62506256da36ffb |
| diagnostic binary | 12d97eb430a8d894a4491944827fce7b169835ebb9dec040b02b92a66f8ba2ed |
| original W1 binary | ae6ce12e8ed06b246e19b73995f774da0d3c74f7a369e9f336b2d7fbc30558c8 |

独立审查时两预算根共380,242,110B，低于536,870,912B；报告/最终小型封存新增后的总量以final-v1/result.json为准。

边界：请求字节、显式调用范围、观察序峰值和生命周期均不是进程总内存、RSS、内核复制或性能收益。5 owned/0 free的区间数来自冻结schema/source；容量轨迹不能区分5–8区间。ledger不提供突然断电的严格持久性保证。保留完整warm诊断开销；不得据此作正式计时结论。
