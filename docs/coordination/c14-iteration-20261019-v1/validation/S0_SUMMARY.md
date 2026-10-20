# S0 结果

S0 passed：交接HEAD `1904b9c71230026ac6f2d58f919a9ab76447d264`，154根成员与14生产文件一致；主头 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。固定gitlinks、11个vendor include内容树与3份许可证通过。

实际环境为 Windows 11/WSL2 Ubuntu-22.04、Linux x86_64；32逻辑CPU，约62.6GiB WSL内存。原 `9ffb96e3…` P2996 compiler、wrapper、匹配libc++/libc++abi、独立host loader/glibc及兼容SAN resource仍存在，固定toolchain清单的全部文件SHA一致。复用CMake 4.2.3，无重新配置工具链。

旧WSL final-v4 153个代码/构建/测试/文档成员与新根一致，只有旧 `SOURCE_MANIFEST.json` 是发行前元数据；新根154成员受最终材料化清单约束。历史 normal73/SAN73 的14文件绑定、实际inventory和JUnit逐项相符，依既有规则复用其原profile下通过记录，不重复执行146个测试。业务80/V1 84/生命周期52/安装及干净包仍以原验收文件为来源，S0没有重新审计其全部body。

工作树submodule目录为空，不修改它们。新候选验证快照将从已通过同一内容锁的WSL final-v4/vendor复制；CMake仍执行原内容锁。Windows原生C14 storage和full均没有本轮新验证结果，不能用WSL代替。

实际全局租约核验为空闲，仍用旧根 `build/vm-coordination/queue.lock` 的同一inode。WSL逻辑空闲约582GiB，但VHDX backing在C盘，C盘实际空闲约73.6GiB。定向验证先保留8GiB空间；72对/Q/F实际capture、宿主控制及峰值空间预算尚未准入。未暂停进程或改变电源。

初次盘点出现三项工具适配问题，原始输出保存在 `S0_INITIAL_DIAGNOSTIC.json`：WSL Git无法解读Windows worktree `.git`绝对路径（改用Windows Git的真实HEAD/gitlinks）；Python Path对象排序与CMake相对字符串排序不同（依原CMake算法核验后11树全通过）；发行前后SOURCE_MANIFEST元数据不同（明确排除平台代码差异）。没有修改依赖或旧归档，也没有C++失败重跑。

可执行定向入口已完成：`candidate_job.py check|run --request ...`。最终版本5项实际预检全部通过，见 `entry-check-v2/result.json`；工具只支持明确normal/SAN目标，不支持计时。具体请求/回执规范见 `RECEIPTS.md`。

下一请求：核心冻结M1 v2 codegen/定向反例请求及精确源码、worker、输入身份。现已只读看到M1 v2源码和run_m1草稿，尚未看到最终REQUESTS登记，未擅自运行。先核查真实 `-O3 -DNDEBUG` 原driver机器码；机制存在才进行后续一次固定M和适用normal/SAN。S1 owning输出16次诊断独立登记，不阻塞M1代码生成准备。

本有限交付新增C++编译0、C++执行0、计时样本0；整体性能目标仍未完成。
