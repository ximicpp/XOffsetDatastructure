# 3.2.0-rc.1 跨平台功能测试记录（2026-10-28）

**源码身份**：`v3.2.0-rc.1`（注释 tag 对象 `6bced0b6a493a50f078916b74f64df747b5e893c`）→ 提交
`39de6364563d89f0ef745a23d1937c7f212f9a3c`。本页的结果只属于这个提交。本页写入时 `next_cpp26` 已在
39de636 之后合入 #166（59012329）、#163（964b2bc0）、#167（32ab499d）等，这些提交没有跑过本轮测试，
不能沿用这里的结论。

只测功能，不计时：没有性能结论，没有 iPhone，没有裸机 Linux（见第 5 节）。

## 1. 证据和核验

原始证据是交接包 `xoffset-3.2.0-rc1-handoff-20261028.zip`（12,591,750 字节，SHA-256
`44a368e49c4a581ad72fd3573e7f6f2717b44120358306d1fb76bd2b68a6d484`），放在 project files 根目录。
包里有设备序列号和测试机的本地路径，所以原始日志不进仓库。包内文件（`SHA256SUMS` 逐个列出）：

| 路径 | 内容 | SHA-256（前 16 位） |
| --- | --- | --- |
| `platform-rc1-20261028/REPORT.md` | 测试方的结论 | `4246f3c60dbe4128` |
| `platform-rc1-20261028/windows-results.tar.gz` | Windows：失败的 `windows-cxx20-run2`，通过的 `windows-cxx20-run3`、`windows-cxx23-run3` | `705b4c91b2a29cf4` |
| `platform-rc1-20261028/macos-results.tar.gz` | macOS C++20、C++23 | `ce0539491d9deb52` |
| `platform-rc1-20261028/macos-reflection-results.tar.gz` | macOS C++26 反射 | `e6560cfa89626044` |
| `platform-rc1-20261028/linux-remote-results.tar.gz` | 远端 Linux C++20 | `45253508bccbb469` |
| `platform-rc1-20261028/linux-server-wsl-results.tar.gz` | WSL2 Docker GCC 16 C++26 反射 | `1ac248b88e65764a` |
| `android-3.2.0-rc1-20261028/out-20261028T082542Z.tar.gz` | Android 第 1 次（demo 未跑） | `e9204ad95b914508` |
| `android-3.2.0-rc1-20261028/out-20261028T082851Z.tar.gz` | Android 第 2 次（demo 回放失败） | `15a914b0565448a4` |
| `android-3.2.0-rc1-20261028/out-20261028T083435Z.tar.gz` | Android 最终（全部通过） | `800ad40358f364dc` |
| `supplement/ref-corpus-rc1.tar.gz` | 6 个参照 wire（WSL2 GCC 16.2 C++26 反射写出） | `770b272e62a4abd8` |
| `supplement/*-supplement.tar.gz`、`android-build-evidence.tar.gz` | 各构建的 CMakeCache、`LastTest.log`，Android 构建日志 | |
| `supplement/source-used-wsl-export.tar.gz` | WSL2 和 Android 构建实际用的源码副本（无 `.git`，带 `.xo-commit`） | `2c6d60da3e156932` |

2026-10-28 在 Mac 上的核验（只读，包里的脚本没有执行）：

- `sha256sum -c SHA256SUMS`：27 个文件全部 OK；`REPORT.md` 列出的 6 个结果包哈希与之相同。
  Android 最终包内自带的 `SHA256SUMS` 229 条全部 OK，两次失败包分别 8 条、223 条全部 OK。
- 7 组构建写出的语料（Windows ×2、macOS ×3、远端 Linux、WSL2）与参照 wire 用 `cmp` 逐个比较，
  6 个文件全部相同；Android 最终包的 `corpus.sha256` 与参照哈希相同，`corpus-read.log` 6 个都是
  "accepted and byte-identical"。
- 各 `ctest.log` 的摘要行与下表一致；8 份 `test-inventory.json` 按标准分成两组，组内完全相同。
- 源码副本与 `git archive 39de636` 比较：除 `.xo-commit`（内容是 39de636）、`tools/__pycache__` 和
  子模块外没有差异；`external/typelayout` 与子模块提交 fe295929 的内容相同。Boost 子集没有逐文件核对。
- 包内 `android_device.sh`、`native_targets.sh`、`platform_matrix.sh` 与 39de636 的同名文件逐字节相同。

## 2. 结果

| 环境 | 编译器与标准 | CTest | 参照 wire | demo | 结果包 |
| --- | --- | --- | --- | --- | --- |
| Windows 11 x64 实机 | MSVC 19.44.35229（工具集 14.44.35207），Ninja，Release，`/MT`，`/utf-8`；C++20、C++23 | 各 129/129 | 读入 6 个全部接受，写出逐字节相同 | 未跑 | windows-results |
| macOS arm64 实机（SDK 26.4，Darwin 25.4.0） | Apple clang 21.0.0（clang-2100.0.123.102），部署目标 14.0；C++20、C++23 | 各 129/129 | 同上 | 未跑 | macos-results |
| 同一台 Mac | clang-p2996 9ffb96e3（clang 21.0.0git），C++26 反射 | 158/158 | 同上 | 未跑 | macos-reflection-results |
| 远端 Linux x86-64（测试方记为 KVM 虚拟机） | Zig 提供的 Clang 20.1.2，C++20 | 129/129 | 同上 | 未跑 | linux-remote-results |
| WSL2 Linux x86-64（6.6.87.2-microsoft-standard-WSL2），Docker | GCC 16.2.0（镜像按 digest 固定），C++26 反射 | 158/158 | 写参照的构建自己读写，逐字节相同 | 作为服务端 A/B | linux-server-wsl-results |
| Xiaomi M2102K1C，Android 11（API 30），arm64-v8a，4 KiB 页 | NDK r29（29.0.14206865）Clang 21，API 28，C++20 | 手机上 114/114，电脑上编译失败测试 15/15 | 手机读入 6 个全部接受，写出逐字节相同 | android、android-noexc 都 PASS | out-20261028T083435Z |

Android demo（最终包 `demo/E2E_RESULT.md`）：服务端 A/B 是 WSL2 Docker 里的 GCC 16 C++26 反射构建，
两个客户端在手机上运行，经 Windows adb 的 `adb reverse` 连到 A。两个客户端都是自检 23/23、回放
60/60 相同摘要（dense、compact、zstd 各 20）、1/100/10000 单位各 500 步与恶意输入通过
（android 74 条判定，android-noexc 75 条），android-noexc 三档的步 wire 与参照主机相同。
表中的时间是这台手机上的单次功能运行，不作性能数据。

环境列里的操作系统版本和机器类型，凡日志里没有的（Windows 11、远端 Linux 的发行版和虚拟化），
都取自测试方的 `REPORT.md`。

## 3. 测试数量不能相加

| 构建 | 测试程序 | 子进程场景 | 工具检查 | CTest | 配置期检查（另列） |
| --- | ---: | ---: | ---: | ---: | ---: |
| C++20 / C++23 字段表 | 110 | 18 | 1 | 129 | 3 |
| C++26 反射 | 133 | 20 | 5 | 158 | 3 |

两组不是包含关系：C++26 构建有 35 项是 C++20/23 没有的，C++20/23 有 6 项
（`receive_raw_nonfixed_*_fail`）是 C++26 构建没有的。`cxx26_only_not_built` 列的是字段表构建
不编的 31 个程序和工具（含不进 CTest 的 `export_signatures`），不等于 CTest 的差。所以 129 和 158
各自只说明对应后端的全量通过，不能加成一个覆盖率。Android 的 114 + 15 是同一份 129 项清单
（`android-build-evidence/android/test-inventory.json`）拆成手机上跑的和电脑上跑的两部分。

## 4. 未通过的记录（保留，没有覆盖）

- **Windows `windows-cxx20-run2`**：configure 通过，build 退出码 2。`tests/test_memory_efficiency.cpp`
  第 35 行的 `≈` 在代码页 936 下被当成 GBK 读，报 C2001/C3688；库头文件同时有 C4819 警告。
  `run3` 在新目录里加 `-DCMAKE_CXX_FLAGS=/utf-8` 重跑 C++20 和 C++23，全部通过。包里没有 `run1`
  目录，测试方报告把 run2 称作第一次构建，编号原因未说明。
- **Android 第 1 次（08:25Z）**：库和 wire 通过；`adb reverse tcp:48024` 失败，手机上这个端口被
  占用（47800–48049 落在手机的临时端口范围里），demo 没跑，退出码 3。
- **Android 第 2 次（08:28Z）**：端口改到 17800 起；库和 wire 通过；回放的 60 个快照全部缺失：
  Windows adb 的路径转换丢了目录末尾的 `/.`，61 个语料文件推到了手机上多一层的目录里，客户端报
  `cannot read /data/local/tmp/xo/corpus/corpus.csv`，退出码 2。自检、三档和
  恶意输入全部通过，失败的 2 条判定都属于回放（报告里写成 "tiers: 2 of 74 verdicts failed"，
  措辞不准）。
- **Android 最终（08:34Z）**：用修好的 adb 包装重跑全部三步，退出码 0。

适配只在测试环境里：`android_device-wsl-ports.sh` 与仓库脚本相比只改了 3 行端口（47800 → 17800），
`adb-windows-fixed` 把 WSL 路径转成 Windows 路径并保留 `/.`。库代码和测试没有改。Android demo
汇总里的 "commit unknown" 和 WSL2 平台报告里的 "exported"，是因为脚本复制源码时排除了 `.git`；
源码身份见第 1 节。

## 5. 没有覆盖的范围

- 性能、内存、包体积：没有测。
- iPhone 真机：没有测（iOS 只有模拟器记录，见 `docs/CLIENT_TARGETS.md`）。
- 裸机 Linux：远端 Linux 是虚拟机；Linux 上的 C++26 反射只在 WSL2 Docker 里跑过，远端没有反射构建。
  Linux arm64 本轮没有跑。
- Windows 和 macOS 的跨设备 demo：没有跑，只有库 CTest 和 wire 语料。Windows 只测了 MSVC，
  clang-cl、MinGW 在真机上没有跑；Windows ARM64、macOS x86-64 没有跑。
- Android：只有一台手机（Android 11、4 KiB 页）；16 KiB 页设备、其他 Android 版本、NDK r30
  没有在真机上跑。demo 的服务端在 WSL2 Docker，不是独立的 Linux 服务器。
- 没有 sanitizer 运行；没有 Mac 检查点（预览版，见发版记录）。
- 硬件型号（Windows 的 i9-13900K、Mac 的 M4 Pro、远端 Linux 的 Debian 与 KVM）只来自测试方的
  `REPORT.md`：`native_targets.sh` 的结果页没有记机器，原始日志里找不到这些信息。

## 6. 发现的问题和后续

| 问题 | 处理 |
| --- | --- |
| MSVC 在非 UTF-8 代码页上编不过测试（`/utf-8` 不在项目里） | 项目内对 MSVC 加 `/utf-8`；`xoffsetdatastructure.hpp` 的 9 行注释改成 ASCII，公开头文件不再依赖源码字符集 |
| `android_device.sh` 的 demo 端口 47800–48049 在 Android 临时端口范围内 | 默认改为 17800，可用 `XO_PORT_BASE` 指定 |
| Android 11 上 `device.txt` 的 SoC 为空（`ro.soc.*` 从 API 31 才有） | 为空时改记 `ro.board.platform` 和 `ro.hardware` |
| 导出源码（无 `.git`）时结果页写不出提交 | `platform_matrix.sh`、`native_targets.sh`、`run_tier.sh` 读 `.xo-commit` |
| `native_targets.sh` 的结果页不记机器 | 加 `machine: uname -srm` |
| `e2e_report.py` 把所有失败判定记成 "tiers" | 改为 "verdicts: N of M failed (scope …)" |
| Windows 主机（WSL2 + Windows adb）不是 `android_device.sh` 支持的主机 | 本轮靠包里的 adb 包装绕过；是否正式支持待定，`docs/DEVICE_TESTING.md` 先写明 |
| 测试方报告的硬件信息无原始记录 | 见第 5 节；以后的结果页带 `machine` 行 |

以上修改都在 39de636 之后，要在下一个 rc 上重跑才算验证过。
