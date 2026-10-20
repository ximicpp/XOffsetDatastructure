# Mac 原生平台验证结果（2026-10-20）

本文件记录本次 Mac 接续的交接完整性与平台功能验证。没有任何性能测量；这里的通过只代表 macOS arm64 原生功能，不代表性能、Windows 或 Linux。

## 1. 宿主与身份

| 项目 | 值 |
| --- | --- |
| 宿主 | Mac mini，Apple M4（4P+6E），16 GB，macOS 26.3（25D125），原生 arm64，无 Rosetta/容器 |
| 文件系统 | 系统盘 APFS 大小写不敏感；新建大小写敏感稀疏镜像 `/Volumes/XOffsetCS`（`~/workspace/xoffset-mac-cs-20261020.sparseimage`），所有源码快照、构建和结果都在其中 |
| 仓库 | `~/workspace/XOffsetDatastructure-mac-20261020`，分支 `codex/mac-continuation-20261020`，提交 `a6fd1ecb8da94306e1e3970ce56d0ab72c6db7e1` |
| C14 主头 | `66818de4…`（根目录，未修改） |
| R1+M2 候选 | `/Volumes/XOffsetCS/candidate-r1m2-20261020`，用 `--materialize-candidate` 生成，154 成员已核对，主头 `155f0847…`；为安装消费检查加了指向仓库 `external` 的符号链接 |
| 依赖 | Boost `da041154…`（含 10 个组件子模块）、TypeLayout `9a6de1d7…` |
| Python / CMake | 系统 Python 3.9.6；Homebrew Python 3.13（业务套件需要）；CMake/CTest 4.4.4；Ninja |
| storage 编译器 | AppleClang 21.0.0（clang-2100.1.1.101） |
| full 编译器（新 profile） | 本机源码构建的 bloomberg/clang-p2996 `d77eff1cbd78fd065668acf93b1f5f400d39134d`（clang 21.0.0git），SDK `MacOSX26.5.sdk`，安装在 `/Volumes/XOffsetCS/toolchain/p2996-d77eff1`。主要文件哈希：`clang-21` `a3a779ee…`，`libc++.1.0.dylib` `f4e4394b…`，`libc++abi.1.0.dylib` `d4c3f89d…`，`libclang_rt.tsan_osx_dynamic.dylib` `50c53e13…`，`libclang_rt.asan_osx_dynamic.dylib` `6299ab27…`。与旧 `macos-p2996` profile 记录的哈希不同（同提交、不同构建），旧 profile 未修改 |
| 竞品源码 | cista `015b94e6`、FlatBuffers `7e163021`、Cap'n Proto `373e61ec`、cereal `ebef1e92`，提交和 tree 均与 `environment-p1-v1.json` 锁一致 |

Homebrew Clang 23.1.3 不支持 P2996（不认 `-fexpansion-statements`，没有 `<meta>`），没有使用。

## 2. 交接完整性（不是平台测试）

- `verify_mac_handoff_20261020.py`：`passed_integrity_only`，1568 个转交文件、154 基线成员、14 生产文件、两个 gitlink、候选主头。
- `verify_c14_evidence_bundle_20261020.py`：`passed`，4 卷 126,548,323 B，28,022 个 CAS blob，未展开 485 GB 逻辑别名。
- `restore_handoff_20261019.py --out /Volumes/XOffsetCS/restore-20261019`：`github_handoff_integrity_passed`。

## 3. 平台功能验证

结果目录都在 `/Volumes/XOffsetCS/results/`（业务套件在 `/Volumes/XOffsetCS/mac-validation/`）。

| 验证 | C14 | R1+M2 | 入口 |
| --- | --- | --- | --- |
| storage normal | 2/2 | — | `validate_vm.py native --suite storage`（AppleClang） |
| storage 安装消费 | 15 项通过 | — | `validate_vm_package.py --suite storage` |
| 反射能力 | 独立反射程序编译、链接、运行正确；`cmake/reflection_probe.cpp` 编译链接通过，运行返回 1 | 同 | 手工；见 §4.1 |
| full normal | 76/76 | 76/76 | `validate_vm.py native --suite full` |
| full ASan+UBSan，全部 66 个程序 | 64/73 | 64/73（同一组 9 项） | `validate_vm.py --sanitizer address-undefined --targets <全部>` |
| full UBSan-only，全部用例 | 76/76 | 76/76 | 新目录手工 CMake，动态 libc++，与入口同 flags |
| full TSan，全部 66 个程序 | 73/73 | 73/73 | `validate_vm.py --sanitizer thread --targets <全部>` |
| VM 历史 wire 交换 | 84 帧 + 504 项通过 | 同 | `validate_vm.py wire` |
| full 安装消费 | 14 项通过 | 14 项通过 | `validate_vm_package.py --suite full` |
| 业务 normal（5 库 × S/M/L 真值 + 审计） | 80/80，15 cell，audit-passed | 80/80，15 cell，audit-passed | Mac 适配层，见 §4.3 |
| 业务 SAN（ASan+UBSan） | 80/80，15 cell，audit-passed | 80/80，15 cell，audit-passed | 同上 |
| 对比工具 ctest | 16/16（normal、SAN） | 16/16（normal、SAN） | `build_bench.sh` |

固定输入在本机由 `reference.py` 重新生成，prepared 审计通过，S 档 5 个文件与 golden-S 逐字节一致。

## 4. 偏差与限制（据实记录）

### 4.1 反射探针返回 1
`reflection_probe.cpp` 期望 `value == 7`。库的 `reflect_init_all` 会先 `memset` 清零，再逐成员 `default_construct`，不应用 `int value = 7` 这类默认成员初始化，所以读出 0（直接访问字段也是 0）。这是库语义，不是工具链问题；CMake 检查只要求编译和链接。

### 4.2 ASan 下 9 项分配失败注入测试
失败的是 `test_scalar_resize`、`test_validation_v4`、`test_wire_bitmap`、`test_validation_resource`、`test_wire_range_failures`、`test_range_hint`、`test_vm_transport`、`test_owner_identity`、`test_verified_paths`，报错都是“分配失败没有被触发”。

原因：Darwin 的 ASan 运行库接管了 libc++ 动态库内部对 `operator new` 的调用，测试替换的 `operator new` 调不到。最小复现：同一程序在无 sanitizer、UBSan、TSan 下 pmr 分配会经过替换的版本，在 ASan 下不会。

这 9 项的 ASan 内存检查记为“受平台限制未覆盖”，不算通过；它们的 UB 检查由 UBSan-only 全量覆盖。

曾尝试静态链接 libc++ 绕过，但 Darwin 上静态 libc++ 与 libSystem 自带的 libc++ 冲突，连不开 sanitizer 时 ostream 输出数字都失败，该配置无效，相关目录保留并标注 `INVALID_CONFIG.txt`。

### 4.3 业务套件
- 用的是旧包中 alpha 时期的对比工具元数据（适配器、driver），配新头文件运行；历史 F64 所用 final-v4 元数据的 C++ 部分没有转交。内容身份由适配层另行校验。
- 去掉 WSL 队列租约、taskset、ld-linux loader，`ldd/readelf` 改为 `otool`；Darwin 没有 LSan，不做泄漏结论；`ru_maxrss` 在 Darwin 是字节。
- Cap'n Proto 1.5.0 在 ASan 且关闭 fibers 时编译失败（`kj/async.c++` 引用 `FiberStack::impl`），ASan 版改为开启 fibers；普通版关闭。失败构建目录保留。

### 4.4 未复现的历史验收
- **V1 兼容 84+8、AB4/C4**：需要 6 个旧版本加 V8 的二进制或源码、历史 wire、four-paths 基线 14 文件，交接清单写明不随包。仅能部分验证（alpha 读取方向、一条旧 V8 M wire）。上表的 wire 交换是另一项兼容性证据，不能替代。
- **生命周期 52**：作者的 `production-matrix-v2` 与 `receive-pool-probe` 源码不在迁移物中，无法复现，不重写冒充。
- **消费端 29**：维护入口覆盖 12 项；其余 17 项（receive、归档搬迁、shared runtime）需另写 Mac 版脚本，尚未做。

## 5. 结论

C14 和 R1+M2 在 macOS arm64 原生环境下，full normal、UBSan、TSan、VM wire 交换、安装消费和业务 80（normal/SAN）全部通过；ASan 下 9 项受平台限制未覆盖；V1 84+8、生命周期 52 和消费端其余 17 项未复现。正式性能窗口尚未启动。
