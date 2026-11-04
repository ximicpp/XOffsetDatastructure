# Memmove image implementation review

本分支交付实验性的受控构建、可信只读镜像和显式借用接口，供主会话 review 后决定是否合并。核心源代码、方案、两轮原始样本及未通过的结果一起保留。

**收益来自“可信＋只读＋借用”这一组合。没有做可信可变、验证后只读等分项对照，不能证明可信因素贡献最大，也不能把结果称为原完整验证、可变 owning 加载在同等合同下的提速。**

## 基点和提交范围

- 实现与测量基点：`631d6f85520dcd2f40f880a122de38c5dba1dace`。
- 发布准备时 GitHub `main` 已在 `4989e635ab5d411fa822cdda8bbaae4c76d8473b`。本分支保留实际测试的基点；没有把旧测量冒充最新 main 的验证。合并前需由主会话检查差异与兼容性。
- 生产主头 `xoffsetdatastructure.hpp` 的 SHA256 仍为 `c4999e76010d419d231428dd262a329f7ea1d475f9cb4db602572fa8c0c75341`。
- 三个新增头：`xoffset_dense_export.hpp`、`xoffset_image_view.hpp`、`xoffset_image_builder.hpp`。另外修改显式安装/测试清单，加入 `tests/test_dense_image.cpp`。
- Wire 2.1、默认 emission、现有 verified 入口和冻结的 `bench/` 源码未修改。新接口必须显式包含头文件、显式采用。

## 建议阅读顺序

1. [设计方案](../../docs/MEMMOVE_SERIALIZATION_PLAN_20261104.md)。
2. [实现和验证结果](../../docs/MEMMOVE_SERIALIZATION_RESULTS_20261104.md)。
3. [构建不变量、生命周期与接口合同](lifetime-and-contract.md)。
4. [新增测试](../../tests/test_dense_image.cpp)及三个新增头。
5. [第二轮预登记](measurement-v2-borrowed/preregister.json)、[第二轮统计](measurement-v2-borrowed/summary.json)、[第一轮保留结果](measurement-v1/summary-recovered.json)。

## 已完成的验证

- Apple M4 Pro、macOS 26.4.1、SDK 26.4、匹配的 P2996 Clang 21 / libc++。
- 完整 Release CTest 106/106；后续新增接口修改只影响新增测试，该测试的最终版本已再次通过 Release、ASan/UBSan 和安装包独立消费。
- 新增测试包含两组各 1000 个固定种子的有效图差分用例、golden bytes、OOM、别名、嵌套、脏 bool spare、移动、持有/借用、旧句柄拒绝和复用尾部。
- 实际 SLG Report 模型 10000 条事件，240088 字节；原验证器和代理读取 checksum 同为 50995077。A/D schema 属于当前构建器类型域，含 map 的 B/C schema 明确拒绝。
- 第二轮借用型接收 8/8 场景通过 A/A 和门槛。大消息逐场景单侧 95% 耗时比上界最高 1.0246；小消息额外延迟上界最高 1.27 ns。
- 优化 IR 中，借用 reload 只有一次完整 memmove 加固定检查/元数据操作，没有节点循环或原子引用计数。

### 必须保留的限制和失败

- 第一轮持有型 ImageView 接收只有 7/8 达标：64 KiB 上界 1.1653，未过 1.10。第二轮增加独立的借用合同，原持有型 API 和其覆盖保护仍保留，两轮不合并样本。
- 第一轮在全部 24 个进程完成后，汇总遇到诊断查询的零计时而失败；离线恢复只把无法分辨的查询比值置为空，没有重跑、筛选或追加样本。
- 既有 sanitizer 子集为 7/8，`test_verified_paths` 的 OOM 注入断言失败；同一未改 HEAD 测试和生产主头的对照同样失败。没有删除该断言。见 [基线对照](baseline-asan-control/result.json)。新增测试最终 sanitizer 运行通过，整组 sanitizer 不能写成全绿。macOS 未启用 LeakSanitizer。
- 尚未实现通用 ImageEditor、map/set 受控构建、跨进程免验证准入，也没有新的完整 SLG A/B/C/D 端到端性能结论。默认外部输入继续完整验证。
- 六个性能场景没有业务记录，另外两个用 blob 补齐总大小。这些数据适合判断一次复制及结构相关附加成本，不代表实际业务收益上限。

## Review 重点

- 受控构建是否确实维持边界、完整覆盖、单一所有权和有效字段表示；每个动态块只链接一次，失败准备不会留下孤块。
- 构建器拒绝从 vector 存储 trait 混入 map/set，也拒绝任意用户派生 string/vector。排序、隐藏成员或自定义激活不能被绕过。
- 所有读取是固定大小 POD memcpy 和字节偏移代理，没有把复制字节强转为一个已存在的 owning T；动态数组及非平凡对象生命周期不能以性能结果代替证明。
- seal 后旧 builder 句柄拒绝写入；freeze 的既有可写别名静默期仍是调用方合同。
- ImageView 独立持有存储并拒绝覆盖；BorrowedImageBuffer 的旧代理/string_view 在 reload、move、销毁后失效，由调用方同步和管理借用。两个 API 不互相冒充。
- 计时只覆盖已准备目标和可信来源的热路径；首次构建、freeze、pack、分配及查询成本分别保留。

## 核对证据

在完整仓库根目录运行：

```sh
python3 experiments/memmove-serialization-20261104/audit_evidence.py
```

这是只读完整性和离线统计核对，不是平台测试或新的计时窗口。它核对第二候选的已提交源代码、两轮全部原始 JSON、逐进程统计与门槛，并从 `review-evidence.zip` 核对第一候选的冻结源码。旧的 `analyze.py` 绑定第一候选运行时工作树，不应在第二候选工作树上用来重写历史结果。

`review-evidence.zip` 是交付时已生成的原始来源/证据包，保留两轮版本的分离。编译目录、安装前缀、完整 IR 和原生二进制留在 Mac；它们的路径或摘要不等于文件随本分支提供。`review-manifest.json` 固定本次选入 Git 的普通文件身份，自身除外。实验目录的 `.gitignore` 隐藏本机生成物，源码与小型证据显式纳入 Git。原始日志和 JUnit 输出保留捕获时的空白，`.gitattributes` 只对这些证据文件关闭空白格式检查。

## 功能复现

需要完整仓库的固定依赖和支持 P2996 的编译器/标准库。指定实际编译器，使用全新目录：

```sh
cmake -S . -B build_memmove_review_new \
  -DCMAKE_CXX_COMPILER=/absolute/path/to/clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build_memmove_review_new --target test_dense_image -j 4
ctest --test-dir build_memmove_review_new -R '^test_dense_image$' --output-on-failure

cmake -S experiments/memmove-serialization-20261104 -B build_memmove_probe_new \
  -DCMAKE_CXX_COMPILER=/absolute/path/to/clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build_memmove_probe_new --target bench_borrowed slg_image_smoke -j 4
./build_memmove_probe_new/bin/Release/slg_image_smoke
```

Mac 可按实际 SDK 加 `-DCMAKE_OSX_SYSROOT=...`。sanitizer 还需与编译器匹配的 runtime 和头文件；原命令与退出码记录在 [checks.json](checks.json)，其中的绝对路径是该次 Mac 证据，不是通用安装路径。

性能脚本默认拒绝已有输出目录。不要在本目录直接重跑、覆盖已提交窗口。若需要新测量，先把源码和脚本复制到 `experiments/` 下的另一个全新同级目录，在其 `perf-borrowed` 子目录构建对应二进制，预登记新平台和合同后运行新窗口。合并到最新 main 后，旧结果仍然只代表原基点。
