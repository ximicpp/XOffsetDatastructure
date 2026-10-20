# M2 有限资源 capture 准备静态审查

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。本次只读/hash小源码、profile/编译元数据及独立review；没有执行任何项目Python源码、编译、queue、native、样本或大body扫描，也没有复算R1已完成窗口。

**结论：v3裸profile/输入及v4有限capture准备源码通过，source readiness passed。精确request/config生成和root执行准入仍待完成；本文件不是实际capture或screen准入。** 不因v3旧pending字段否决已独立通过的M2完整功能和机制。

## 本次版本范围

| 文件 | bytes | SHA256 |
|---|---:|---|
| `M2_RESOURCE_CAPTURE_PLAN_v3.json` | 5016 | `da4b8a188e1dfd89ecbb6da9b724ab9dd1b8e8af1b7f6c0efa8b432e16aa885a` |
| `M2_REQUEST_DRAFT_v3.json` | 102159 | `371a0b32a070c86b514c26c8e6331f6d663dcdbf269704aeb0d1b2e62c6ca9eb` |
| `M2_BARE_PROFILE_OBSERVATION_v3.json` | 34695 | `813b9133450f8cc2a6eb3780007973ede72ec7114e13fe606551d9b647dd2747` |
| `M2_TOOL_MANIFEST_v3.json` | 4721 | `3f1104ceab40f2cbe119a3efd17b47bafa67d500d35867fecb9da06d229c6044` |
| `M2_CAPTURE_PREPARATION_READY_v4.json` | 6259 | `01c9d71054369e7ca37dc2b8120d8aacd02291a99202781b00dc234d9e1053d7` |
| `M2_CAPTURE_REVIEW_REQUIREMENTS_v4.json` | 6235 | `18a1e704aeceac5693bb104b2e490af9f34f81bc1be0eedae9dab0e4ef88f9aa` |
| `prepare_metadata_v3.py` | 14565 | `650dd589516f7d05494e464cf89a7563d0e5740cfb8e24a72295f328d5c8beb2` |
| `prepare_capture_v4.py` | 16055 | `3b922974dbc6b0b97b57a157a4520107e13e3868733caff0d2ee78eaa44b0f79` |

JSON位于 `validation/screen/M2/`；Python位于工程 `tools/c14_iteration_20261020_v2/validation/screen/m2/`。本审查复用此前已审generic capture/request_capture/project_capture与M2 worker，未重新运行这些工具。

## 已核对内容

- 九个实际执行文件与已审v2 manifest逐一hash相同；M2 worker仍59817 bytes/SHA `9dc7c7808c3cb8e5fc9c77ed5ed4cf34e6f51b82a5ca490c6751ba17a7b07aaf`。producer是新metadata入口，不修改worker、协议、统计族、seed或旧R1输入。
- 实际M2裸profile SHA `2cd7e9b61d59157e85cbf5e9da98948efc6bc5f6c05fd11bb3d7f28f0bca8846`，对应225856B binary SHA `f155d5fd360eea38f38b99a7ca45c63a4342c1a136d5ad5b260f57f22c83710e`。读取/hash实际profile、binary、compile_commands与compiled-source-binding；其相互引用一致。
- 实际编译main为 `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f`，与M2 identity一致；14文件按ordinal键排序独立计算canonical digest为 `2810fca77c737987a0d48af518a214c04dba57468744d171824c6496afd5f128`，与profile相同。未借R1候选或用legacy commit字面量充当M2源码身份。
- actual compile argv含裸 `-O3 -DNDEBUG` / P2996 flags，实际编译记录无 `-UNDEBUG`、SAN、diag或forced include。driver与原C14 control实际字节相同；adapter在仅归一化library_id/source_content_sha256两metadata literal后逐字一致。独立owning输出、原run阶段、原业务调用和完整事后truth没有改变。
- M2 INPUT_POLICY的实际hash、staging receipt及parent独立接受引用一致，两角色选择同一已批准WSL staged wire/fixed-M。方案保留staging1.275457023秒及62,218,240B retained成本。本次不重新读取原/副本wire、canonical或业务body，也不把诊断时间或R1资源capture当M2速度证据。
- v4 prepare只生成新M2 metadata，不调度queue或执行native。已有bare binary复用，新增compile预算0；未来capture明确control/candidate各一次、总2 child、每child20秒、原共享W3 functional120秒、0warmup/重试/替换、推断样本0。原20秒/600秒screen与独立audit3600秒未变，资源capture不进入144项推断列表。
- proposal保留64MiB单body、16GiB RSS、4GiB allocated、双卷各2GiB余量；原run完整process加native_result/byte-oracle审计用于projection，事前另留0.25秒/child的dedup/ledger余量、60秒nonchild及30秒seal。没有只用native.total代替完整process，也不按capture结果滚动补样或调节门槛。

## review schema接入已由新桥接文件闭合

首次静态读取发现v4 `validate_reviews`期待的typed字段与已有原review schema不同。root随后新增两份只做字段映射的桥接文件，未改原review、执行工具或重跑门禁；本次已读内容并核SHA：

- `M2_CAPTURE_FUNCTIONAL_ACCEPTANCE.json`：1880B，SHA `dc43c3e27c650d241d1dbb4ef002635fe99280277a9866431eb07d2d9eec5dca`。candidate、mismatches、functional_gates、source154与73/73、80/80、V1/AB/C/lifecycle52 counts满足v4；精确引用已pass原 `M2_FULL_ACCEPTANCE.json` SHA `312d7d965f2c039d89eada390de7cf2c10b8a15c1599961c1297d4226ee793b7`。
- `M2_MECHANISM_EXECUTION_ACCEPTANCE.json`：1778B，SHA `4d8deaf53cfbaf22c244a7ec3f1d738b1191e5b34dfa96f81f46166bc4866e70`。same candidate、exact mechanism/dynamic结果、candidate1/control0和逻辑工作守恒字段满足v4；精确引用已pass原 `M2_MECHANISM_ACCEPTANCE.json` SHA `7ca0a36ed401a3c8769774cb70e3a201bebd6e5259225c2fcb28e3e9aab6891e`。

该schema阻断已关闭；不要求新增业务测试、重编bare binary或重审已通过原始门禁。

## 后续只需精确request准入

读取v4 ready时PRECONDITIONS/config/RESOURCE_CAPTURE_REQUEST/DISPATCH仍列为待生成；其absence不构成本审查源码拒绝。使用上述桥接identity执行metadata prepare后，root只需核生成的精确config/request/dispatch、当前resource/power/affinity及两角色预算，按原共享租约作独立capture准入。本文件不授权执行producer或native。

新producer须由新M2 metadata链/精确request绑定；若producer或输出再变，复核相应新身份。九执行文件和冻结design既然未变，无需重开旧source/性能窗口、改alpha或重复之前统计测试。capture成功后也只是资源可行性证据，实际72对screen仍需十一typed gate、独立实际freeze和host/quiet准入。

以上review相对路径均以 `docs/coordination/c14-iteration-20261020-v2/reviews/` 为根，JSON相对位置另有说明。交付后idle，不轮询新产物。
