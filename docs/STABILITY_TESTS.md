# 稳定承诺与自动化测试对照（K1–K7）

K1–K7 是 V2 的稳定 API 范围（见 [USER_GUIDE.md](USER_GUIDE.md)）。本表把每条承诺
对应到会在 CI 里失败的测试。测试名就是 CTest 名称；"CI 作业"指
`.github/workflows/ci.yml` 里的作业。改动任何一条承诺的行为时，对应测试必须先失败。

| 承诺 | 内容 | 测试 | 测的是什么 |
|---|---|---|---|
| K1 | 普通 C++ 聚合体定义 schema，反射得出布局和指纹，不用 IDL | `test_zero_boilerplate`、`test_reflection_core`、`test_reflection_advanced`、`test_type_signatures` | 聚合体不写样板代码就能建、读、存；反射遍历成员；签名文本 |
| | | `test_v2_golden` | 固定模型的 wire 字节固定（大小 + FNV-1a），写-读-写逐字节一致 |
| K2 | 对象建在 arena 里，原地读写（含变长修改），空闲块复用 | `test_modify`、`test_vector`、`test_map_set`、`test_main_containers`、`test_string_replace`、`test_assign_range`、`test_stable_erase_if`、`test_resize_growth` | 各容器的原地修改语义 |
| | | `test_free_table_policy`、`test_allocator_model` | 空闲块复用策略与独立 oracle 一致 |
| | | `test_mutation_failures`、`test_allocator_failures` | 分配失败时单次修改是原子的 |
| K3 | 独立输出：`save_verified` 发出调用方独占的副本，发完可以继续改 | `test_stability_contract`（K3 部分） | 保存后修改、扩容、清空、销毁源 buffer，已发出的 wire 不变且仍能加载出原内容 |
| | | `test_verified_paths` | `save_verified_into` 与源存储不重叠、回调中换主人时拒绝 |
| K4 | 接收时完整结构校验，截断、越界、重叠、未铺满、非法值、schema 不符一律拒绝 | `test_wire_validation`、`test_validation_granules` | 恶意偏移、容器头、空洞表、payload 元数据在解引用前被拒绝 |
| | | `test_v2_fuzz` | 独立规格 oracle 与 `load_verified`、`receive_raw::validate`、verified receive 三条路径对变异 wire 的接受/拒绝一致 |
| | | `test_receive_raw`、`test_verified_receive` | 原始接收与分段接收路径 |
| K5 | 接收后接着改、再发出，空闲空间随空洞表带过去，不退化 | `test_stability_contract`（K5 部分） | 重新加载后释放再分配复用空洞、尾部不增长；接收方的分配决定与发送方逐字节一致；第二跳同样成立 |
| | | `test_free_table_policy`（`lifecycle_move_swap_reload`） | 空洞表写-读-写字节一致，重新加载后的分配决定与原 buffer 相同 |
| K6 | schema 指纹精确匹配，不同就拒 | `test_schema_envelope`、`test_wire_admission` | schema 哈希、根类型 id、magic、端序标记不符时拒绝；V1 wire 明确拒绝 |
| K7 | 跨编译器、跨平台对象表示一致；用户 schema 固定签名后不一致即构建失败 | `check_compat`、`check_signature_regeneration`、`check_signature_matrix` | 4 个目标平台、3 个测试类型的签名与基线一致 |
| | | `test_cross_compiler_wire` + CI 作业 `cross-compiler-wire` | P2996 Clang 与 GCC 16 写出的 5 个语料 wire 逐字节一致，并互相 `load_verified` 通过 |
| | | CI 作业 `gcc16` | GCC 16 跑全量 CTest |
| | | `test_schema_pin`、`schema_pin_mismatch` | 用户 schema 用 `XOFFSET_PIN_SCHEMA` 固定签名后，布局与固定值一致时编译通过；不一致时构建失败，报错含 `XOffset schema pin mismatch (K7)` |

## 不在 CTest 里的部分

- K2、K3 的性能（读、改 ≤ 1.05，独立输出 ≤ 0.20）由正式测量协议在固定机器上判定，
  不在 CI 里。结果见仓库外的性能报告。
- 内容完整性不在承诺内：只改一个字段值的 wire 会被接受。不要把 K4 说成防篡改。
