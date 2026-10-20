# 首轮实际结果

本回合已完善计划、阶段矩阵和任务账本，并启动两个持久实施会话及独立agent审查。所有新原生工作由验证会话在原全局共享租约执行。此处记录有限批次事实，非正式性能验收。

- S0通过：根154成员、14生产文件、固定依赖及实际compiler/runtime身份核对；同profile原证据按范围复用，未重复全库验收。
- M1 v2已隔离实现，根C14未修改；合法custom scalar/enum/frontier原调度、标准值初始化、完整数组lifetime和原事务顺序保留。
- 原driver真实`-O3 -DNDEBUG -S`控制/候选各一次，独立审查实际call closure：控制uint8/uint32 fresh循环每元素binding，普通state binding继续canonical恢复；候选为编译器生成的批量初始化。完整14闭包包括v2 helper已经核对，不只比较主头。
- 4个normal程序、4个SAN程序通过；固定M控制/候选各一次通过原9阶段canonical、首尾query、独立输出及完整reload真值。
- 实际命中24576次fresh resize、1703936个新增scalar元素；逐元素binding为1703936→0，批量初始化元素为0→1703936，所需值初始化仍在。计数不等于周期或墙钟收益。
- 另一次固定M轨迹后，16次独立owning输出均17830872B、一次C++ new请求、header及payload各一次复制。这一验证暖分配条件中均无minor fault，且没有计时；不能借此认定冷分配成本、首次触页主导、DRAM带宽或0.20目标可达。保持16次上限，不滚动补做有利批次。
- 有限执行批次共13次C++编译、11次native运行、正式/筛选计时样本0；原始队列、实际身份和退出结果见validation各RESULT。统计筛选尚未准入，整体性能目标未完成。
- 后续设计迭代：L1跨验证上下文地址缓存因scratch/new_handler可退役owner的反例被否决，没有消耗原生或计时预算；不把取消名额转给其他候选。R1已形成独立源及安全review，一次候选汇编请求交唯一验证执行者排队，尚无计时准入。

下一阶段已授权持续推进：M1相关完整normal/SAN、业务S/M/L、V1双向和消费端，之后独立冻结唯一72对内部筛选。只有源/安全/功能/资源和统计审查全部通过才执行；固定主族4/辅助族36、AB/BA各36、无补样，screen命令600秒/每child20秒、离线审计3600秒，guest与C backing峰值余量均实核。Q/F仍后置。其他独立候选按可证明机会推进，不自动合入C14，不commit/push。

主要证据：validation/S0_RESULT.json、M1_CODEGEN_RESULT.json、M1_CORRECTNESS_RESULT.json、M1_MECHANISM_RESULT.json、S1_OUTPUT16_RESULT.json、M1_FINITE_EXECUTION_RESULT.json，以及reviews/M1_CODEGEN.md。
