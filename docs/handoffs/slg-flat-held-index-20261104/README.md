# HeldIndex 平面 ID 索引：跨会话应用说明

此分支 `codex/slg-flat-held-index-20261104` 已包含实现与测试。基线为 `1c528d67739bc94d91205c9dd2bf4bf1260253bf`，实现提交为 **`5b8b9f156792fcb6178fc29ec57ae59aa3a6d629`**。实现提交只有两个生产文件和一个独立测试；当前文档所在的后续提交只提供交接材料。

## 新会话应执行的任务

在目标仓库应用此实现并验证。先读取 AGENTS.md 和节点绑定，检查当前版本与用户已有改动，保留现有开发内容与历史证据。

- 如果当前分支已包含实现提交，检查文件后直接验证，避免重复应用。
- 否则先 fetch 本分支、审阅实现提交，再选择 cherry-pick **实现提交**，或针对最新代码做等价移植。若仅需生产补丁，同目录的 `flat-held-index-only.patch` 可独立应用；测试源码见 `bench/slg/tests/flat_held_index_test.cpp`。
- 不要把整个旧基线分支合并进当前开发分支来覆盖后续开发；冲突应按当前 HeldIndex/apply_delta 语义处理，不 reset 用户改动。

示例（目标为本仓库 origin，具体执行节点和目录遵循项目规则）：

```sh
git fetch origin codex/slg-flat-held-index-20261104
git show 5b8b9f156792fcb6178fc29ec57ae59aa3a6d629 --stat
git cherry-pick 5b8b9f156792fcb6178fc29ec57ae59aa3a6d629
```

## 优化范围与正确性约束

优化位置是 SLG 客户端的 ID → 列表下标索引。HeldIndex.units/marches 从 std::unordered_map<int32_t,int32_t> 改成专用 FlatHeldIndexMap。apply_delta/build_index 的业务更新流程、XOffset 核心、wire/增量格式、字符串复制及计时 harness 保持原样。此索引对其他模型同样适用，收益归因于通用客户端适配层。

实现使用连续 Cell 数组、32 位无符号 Fibonacci 哈希、线性探测、至多 0.5 的负载率、后移删除；容量足够时普通插删不逐节点分配。occupied 独立存储，0、负数和 INT32_MIN/MAX 都是合法 ID。clear 保留容量，reserve 重排所有有效项。

**已有 key 的 emplace/operator[] 必须在考虑扩容前返回。** apply_delta 保留被删除 ID 的指针迭代器，先更新末项 ID 的位置，再 erase 旧项。已有 key 的更新若扩容，会使该指针失效。erase 和真正扩容本身允许失效其他迭代器；此表仅支持 HeldIndex 需要的最小接口，end() 是 nullptr，不提供普通容器遍历。reserve 超过 2^29 个元素时抛 length_error。

## 必须完成的验证

1. 构建目标平台的 SLG，运行独立差分测试；测试依赖 assert，禁止 -DNDEBUG（下列命令显式 -UNDEBUG）。
2. 在支持的编译器下运行 ASan+UBSan。detect_leaks=0 不证明无泄漏。
3. 原版/候选对照 abcd/demo、S/M，使用种子 20261021/20261022（S 至少前者），状态摘要和 report 计数一致。继续对照 M、种子 20261022、20 周期，覆盖持续删除/新增。
4. 执行工程要求的相关检查，报告实际结果。源码/哈希一致不是目标平台测试的替代品。

独立测试对照 std::unordered_map，包含 200000 次随机操作，覆盖插入/更新/查找/删除/扩容/clear、复制、负数和 int32 极值。以下为 POSIX 示例，需先创建新的构建目录；其他平台使用相应编译器与节点命令：

```sh
clang++ -std=c++20 -O2 -UNDEBUG -Ibench/slg/include/slg \
  bench/slg/tests/flat_held_index_test.cpp -o BUILD_DIR/flat-index-test
BUILD_DIR/flat-index-test

clang++ -std=c++20 -O1 -g -UNDEBUG -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Ibench/slg/include/slg \
  bench/slg/tests/flat_held_index_test.cpp -o BUILD_DIR/flat-index-test-san
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  BUILD_DIR/flat-index-test-san
```

该测试是独立编译程序，未接入现有 CMake/CTest；不能只运行已有 CTest 就称已执行此测试。普通 C++20 编译器可构建索引测试；含 XOffset 的完整 SLG 仍需要项目规定的 C++26 工具链。

## 既有实验与适用范围

Mac mini M4 Pro（14 核、24 GiB），macOS 26.4.1，P2996 Clang 21 git 9ffb96e3；Release -O3 -DNDEBUG -gline-tables-only；M、seed=20261021、SLG_REUSE=1、SLG_NATIVE_OFF=1，仅 XOffset。每组 6 对进程 AB/BA 交替，demo 15 周期/进程，丢弃周期 0–3。

| 阶段 | 原版 ms | 仅索引 ms | 耗时变化 |
| --- | ---: | ---: | ---: |
| UP.apply | 3.060 | 1.986 | -35.10% |
| UP.send+recv+apply | 3.771 | 2.696 | -28.52% |
| UC.apply | 6.162 | 4.303 | -30.18% |
| UC.send+recv+apply | 10.647 | 8.798 | -17.37% |
| US.apply | 28.069 | 24.791 | -11.68% |
| US.send+recv+apply | 53.187 | 49.918 | -6.15% |

绝对值为进程阶段均值的算术平均，变化率为配对比值的几何平均。同目录 measurements.json 包括该独立索引实验全部阶段、配对值和 t95 区间（df=5，逐指标、未做多重比较校正）、RSS 和每周期大小对照。其他实验的字符串+索引组合收益 36.9% 不作为本项收益。

单项普通与 ASan+UBSan 的 200000 次差分通过；单项 SLG 在 abcd/demo、S/M、两个种子的逻辑对照通过。此分支交付的是注释整理版头文件，与此前推荐组合实际构建和执行 sanitizer 差分的头文件逐字相同；单项计时使用较早的同算法初稿，没有额外对整理版进行一次索引单项计时。推荐组合另有 M/20 周期状态对照及 SLG sanitizer 结果，不能替代本分支在接收方平台的重新验证。

所有大小指标一致不等于逐字节比较全部 wire。宿主仍有桌面后台负载，未宣称通过正式安静门禁；结论限于此机与负载。本分支没有推送私有本机路径、宿主进程快照或原始归档。

复测时保持两份构建、协议、种子、采样口径一致，AB/BA 至少 6 对进程，丢弃前 4 周期，以进程均值为样本。apply 的实际计时段不应混入压缩、摘要和额外大小统计；如另需隔离 harness，放入独立实验，不混入功能提交。测试程序不使用 -DNDEBUG，正常 Release benchmark 可使用它。

## 输出要求

完成移植后报告修改文件、目标平台构建/测试结果、未能完成的验证及原因。如果复测，报告绝对耗时、配对变化和回退项；不要直接将既有 35.1% 当作新机器测得的结论。
