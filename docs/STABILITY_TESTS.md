# 稳定承诺与自动化测试对照（K0–K7）

K0–K7 是 V2 的稳定 API 范围（见 [USER_GUIDE.md](USER_GUIDE.md)）。本表把每条承诺
对应到会在 CI 里失败的测试。测试名就是 CTest 名称；"CI 作业"指
`.github/workflows/ci.yml` 里的作业（GitHub CI 自 2026-10-23 停用，这些检查由本地验证
`tools/local_validate.sh` 执行）。改动任何一条承诺的行为时，对应测试必须先失败。

有两种构建：服务端的 C++26 反射构建，客户端的 C++20/C++23 字段表构建（`XOFFSET_RECORD`）。
标"仅反射构建"的测试在字段表构建里不编译（`test-inventory.json` 记为 `cxx26_only_not_built`），
其余测试两种构建都跑。

| 承诺 | 内容 | 测试 | 测的是什么 |
|---|---|---|---|
| K0 | 可重定位：payload 无绝对地址，任何修改后仍可整块搬到任意地址接着读写 | `test_type_safety`（仅反射构建）、`test_inheritance` | 带裸指针、成员指针、函数指针、虚表或 `std::string`/`std::vector` 的类型被编译期准入（`is_byte_copy_safe_v`）拒绝 |
| | | `test_admission_typelayout`（仅反射构建） | XOffset 自己的准入与 TypeLayout 的 `is_byte_copy_safe_v` 结论相同 |
| | | `test_schema_describe` | 字段表构建下，有字段表的记录照常准入；`std::string`、多态类型这类写不了字段表的类型不准入 |
| | | 配置期检查 `polymorphic_rejection`（`cmake/XOffsetBuildChecks.cmake`） | 多态类型调用 `XBuffer::make<T>` 必须编译失败，且报错是 `Type is not byte-copy safe`；先跑正向对照，排除工具链本身坏掉 |
| | | `test_stability_contract`（K3、K5 部分）、`test_v2_golden` | 输出后在新地址的 buffer 里加载，接着读、改、再发；写-读-写逐字节一致 |
| | | `test_clone`、`test_v2_fuzz`（复制检查） | 修改过、带空洞的 buffer 用 `clone()` / `clone_into()` 整块拷到另一个地址，不经 wire：字节、空闲表、重存相同，之后同样的修改得到同样的字节 |
| K1 | 普通 C++ 聚合体定义 schema，不用 IDL：服务端反射得出布局和指纹，客户端加一行字段表 | `test_zero_boilerplate`、`test_reflection_core`、`test_reflection_advanced`、`test_type_signatures`（都仅反射构建） | 聚合体不写样板代码就能建、读、存；反射遍历成员；签名文本 |
| | | `test_schema_describe` | 同一组 `XOFFSET_RECORD` 记录（含派生、枚举、`std::array`、各容器）：反射构建下宏逐条核对字段表，库自带字段表与反射比对；字段表构建下签名与反射写出的固定文本相同，并能 `save_verified` / `load_verified` 往返 |
| | | `schema_describe_*_fail`（omitted、order、duplicate、array、padding、enum、undescribed；反射构建另有 promise） | 漏写、顺序错、重复、数组或补齐处漏成员、无固定底层类型的枚举、缺字段表、违反普通记录承诺时构建失败，报错与预期一致 |
| | | `describe_dump_slg`（仅反射构建） | `tools/describe_dump` 用反射印出的字段表与 `bench/slg` 提交的字段表逐字相同 |
| | | `test_v2_golden` | 固定模型的 wire 字节固定（大小 + FNV-1a），写-读-写逐字节一致 |
| K2 | 对象建在 arena 里，原地读写（含变长修改），空闲块复用 | `test_modify`、`test_vector`、`test_map_set`、`test_main_containers`、`test_string_replace`、`test_assign_range`、`test_stable_erase_if`、`test_resize_growth` | 各容器的原地修改语义 |
| | | `test_free_table_policy`、`test_allocator_model` | 空闲块复用策略与独立 oracle 一致 |
| | | `test_mutation_failures`、`test_allocator_failures` | 分配失败时单次修改是原子的 |
| K3 | 独立输出：`save_verified` 发出调用方独占的副本，发完可以继续改 | `test_stability_contract`（K3 部分） | 保存后修改、扩容、清空、销毁源 buffer，已发出的 wire 不变且仍能加载出原内容 |
| | | `test_verified_paths` | `save_verified_into` 与源存储不重叠、回调中换主人时拒绝 |
| K4 | 接收时完整结构校验，截断、越界、重叠、未铺满、非法值、schema 不符一律拒绝 | `test_wire_validation`、`test_validation_granules` | 恶意偏移、容器头、空洞表、payload 元数据在解引用前被拒绝 |
| | | `test_v2_fuzz` | 独立规格 oracle 与 `load_verified`、`receive_raw::validate`、verified receive、`reload_verified`、直接接收复用槽（`begin_verified_reload`）对变异 wire 的接受/拒绝一致；直接接收中途中止、少报长度、声明长度与头不符一律拒绝 |
| | | `fuzz_v2_wire_replay`、`fuzz_wire_schemas_replay`（含 `_corpus`；关异常版本 `_replay_noexc`） | 两个 libFuzzer harness 不带 libFuzzer 编译，回放 `tests/fuzz/regressions/` 的崩溃输入和各自的种子语料；ci.yml 的每个作业（P2996 Clang、GCC 16、两种 sanitizer、arm64、Windows）都包含它们，Actions 停用期间由全量 CTest 和每日长跑（GCC 16 ASan+UBSan）执行 |
| | | `test_receive_raw`、`test_verified_receive`、`test_reload_reset` | 原始接收、分段接收与直接接收复用槽路径 |
| K5 | 接收后接着改、再发出，空闲空间随空洞表带过去，不退化 | `test_stability_contract`（K5 部分） | 重新加载后释放再分配复用空洞、尾部不增长；接收方的分配决定与发送方逐字节一致；第二跳同样成立 |
| | | `test_free_table_policy`（`lifecycle_move_swap_reload`） | 空洞表写-读-写字节一致，重新加载后的分配决定与原 buffer 相同 |
| K6 | schema 指纹精确匹配，不同就拒 | `test_schema_envelope`、`test_wire_admission` | schema 哈希、根类型 id、magic、端序标记不符时拒绝；V1 wire 明确拒绝 |
| K7 | 跨编译器、跨平台对象表示一致；用户 schema 固定签名后不一致即构建失败 | `check_compat`、`check_signature_regeneration`、`check_signature_matrix` | 4 个目标平台、3 个测试类型的签名与基线一致 |
| | | `test_cross_compiler_wire` + CI 作业 `cross-compiler-wire` | P2996 Clang 与 GCC 16 写出的 5 个语料 wire 逐字节一致，并互相 `load_verified` 通过 |
| | | CI 作业 `gcc16` | GCC 16 跑全量 CTest |
| | | `test_schema_pin`、`schema_pin_mismatch` | 用户 schema 用 `XOFFSET_PIN_SCHEMA` 固定签名后，布局与固定值一致时编译通过；不一致时构建失败，报错含 `XOffset schema pin mismatch (K7)`。两种构建都跑，所以客户端原生编译器（NDK Clang、MSVC、clang-cl、MinGW GCC）上同样会构建失败 |

## 目标 K8（尚未承诺，实现中）

客户端（iOS、Android、Windows，64 位小端）用字段表后端、各平台原生编译器构建，与服务端用同一套库、
同一 wire。它不是 CTest 里的一项，而是每个客户端目标各跑一遍 `tools/native_targets.sh`：

| 检查 | 测的是什么 | 现状（3.2，未发布） |
|---|---|---|
| 目标上的全量 CTest（Release） | 上面 K0–K7 中两种构建都跑的测试，在该目标的编译器和标准库上通过 | Android arm64（NDK r29/r30）、Windows x64（MSVC 19.44 C++20/C++23、clang-cl 19、MinGW GCC 13）都是 106/106；iOS 等 Xcode |
| 读参照语料 | 服务端 C++26 反射构建写出的 6 个语料，目标全部 `load_verified` 接受 | 上述目标全部接受 |
| 写语料 `diff -r` | 目标写出的语料与参照逐字节一致 | 上述目标逐字节一致 |

升为核心承诺的条件见 [USER_GUIDE.md](USER_GUIDE.md) 第 8 节，逐目标结果见 [CLIENT_TARGETS.md](CLIENT_TARGETS.md)。

## 不在 CTest 里的部分

- 字段表构建与反射构建的等价性：`tools/local_validate.sh` 的 `diff-c++20` 列用同一批变异 wire
  比较 C++26 反射构建和 C++20 字段表构建，wire 字节和接受/拒绝结果（含错误信息）都必须相同。

- K2、K3 的性能（读、改 ≤ 1.05，独立输出 ≤ 0.20）由正式测量协议在固定机器上判定，
  不在 CI 里。结果见仓库外的性能报告。
- 内容完整性不在承诺内：只改一个字段值的 wire 会被接受。不要把 K4 说成防篡改。
- 版本间的 K4 对比：`tools/validation_differential.sh <旧版目录> <新版目录> <例数> <seed>...` 用同一批变异
  wire 比较两个库版本的校验结果（含确切错误信息）。比较按 payload 进行：带 extent 表的 wire 先展开成
  等价的稠密 wire 再校验，因为各版本接受的稀疏写法可以不同（0.3.0-beta.2 起 gap 字节不必落在空洞里）。
  同一版本内"稀疏 wire 与其稠密等价物结果相同"由 `test_v2_fuzz` 检查。
- K4 的每日长跑（威胁模型 F4）：`tools/fuzz_nightly.sh` 在云端 Docker 主机上每天运行 GCC 16
  ASan+UBSan 差分 fuzz（`test_v2_fuzz`，4 个 seed，约 45 分钟）和回归语料回放；发现崩溃时
  `tools/fuzz_report.py` 按签名开 `fuzz-crash` issue 并推出 `fuzz-crash/<签名>` 分支。结果
  不在 CTest 里，记录在项目文件 `fuzz-nightly/`。2026-10-27 起，同一晚接着跑
  `tools/fuzz_libfuzzer.sh`：用云端的 P2996 工具链（自带 libFuzzer 运行库）原生构建两个
  libFuzzer 目标（ASan+UBSan，`-fork=4`，每个目标约 20 分钟），语料跨夜保存在项目文件
  `fuzz-nightly/corpus/`，发现走同一个 `fuzz_report.py`。2026-10-27 晚加入
  `fuzz_schema_evolution`。2026-10-28 起再加两步，覆盖客户端的构建：
  `tools/fuzz_libfuzzer.sh --backend fieldlist` 在 C++20 字段表构建上跑 `v2_wire`、
  `wire_schemas`（语料单独保存为 `fieldlist-<target>.tar.gz`）；`tools/fuzz_outcome_diff.sh`
  把两份语料、回归输入和种子分别交给 C++26 反射、C++20 字段表、关异常三种构建回放
  （ASan+UBSan），每条输入的各入口结果和错误文字必须一致（关异常构建没有 raw 和接收入口，
  比较时去掉这两列）。
