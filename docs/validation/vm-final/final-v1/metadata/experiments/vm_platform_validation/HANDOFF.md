# W4 平台前提交接

状态：`ready_for_review`；交付是只读前提评估，G4 等待统筹下发。没有编译、测试矩阵、
性能测量、容器启动、镜像下载或远端构建任务正在运行。没有修改生产头或历史证据。

唯一一次探测：

```text
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_platform_validation/probe_platform.py --out experiments/vm_platform_validation/evidence/platform-20261015-01
```

退出 0，原始结果为 `ready_for_review`，共记录 24 条 subprocess，所有编译、测试和性能
均 `unexecuted`。Windows ZIP 的字节完整性审计 `passed`，不表示 Windows 编译或运行。
证据在 [result.json](evidence/platform-20261015-01/result.json) 和 `commands/001.json`
至 `024.json`；没有覆盖或重跑旧目录。

离线审计：

```text
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_platform_validation/probe_platform.py --audit experiments/vm_platform_validation/evidence/platform-20261015-01
```

退出 0，`audit_passed`，38 个文件的精确集合、大小与 SHA-256 一致。审计没有运行平台
命令或修改证据。manifest SHA-256 为
`09b09e30a230ae1033bdd2320ea9e28d7041352f6c72c89f9235c5e58d9aca96`。

本机为 macOS / Darwin 25.6.0 arm64。P2996 Clang 的提交、target、二进制 SHA，libc++、
libc++abi 与冻结工具链清单一致。TSan 实际可用位置为
`/Library/Developer/CommandLineTools/usr/lib/clang/21/lib/darwin/libclang_rt.tsan_osx_dynamic.dylib`，
SHA-256 `6f7114babe190d1010186c1106e05112e58946502d8cec7bb98beebcab7aa0e9`，匹配冻结清单；
`21.0.0` 路径解析至同一文件。此前只查 `/usr/local` 的负结果不能解释为缺失 TSan。
冻结 `MacOSX26.5.sdk` 存在，默认 xcrun SDK 为 `MacOSX27.0.sdk`；G4 必须显式选择 26.5。
11 个依赖 include tree、3 个许可证和当前 F 双头都匹配；探测期间输入身份未改变。

Windows ZIP 为 356,508 bytes，SHA-256
`80adbf3e28a93c5ae320e8444052362343d318544653e0ec633daac77c8d9202`；精确 126 个成员，
其中源码 121 个、1,173,446 bytes，所有清单/长度/哈希/CRC 均一致。source manifest SHA
为 `b4db950e010cfe68958360103a5cbfe3330f565c9ec63d2b0f88897daa5d8025`。
包状态仍是 `prepared-not-windows-tested`。原生 Windows x64 / MSVC SDK 执行环境为
`prerequisite_unavailable`；具体工具与回交要求见 [平台说明](../../docs/VM_PLATFORM_FOLLOWUP.md)。
完整 Windows P2996 是另一项未满足前提。

OrbStack 只读清单显示已有 `jojocpp-hydro` Debian bookworm ARM64 VM；没有进入机器。
Docker 只查询本地 Unix socket：daemon 为 aarch64 Linux OrbStack；已知 P2996 镜像为
linux/amd64，跨架构执行须标记仿真。独立原生 Linux 性能仍 `not_verified`。
当时另有无关运行容器 `upbeat_goldstine`，镜像为 `layoutcompare-r2-family-f4-python3137`；
本项仅记录库存，没有进入、停止或修改它。背景活动可能存在，库存不用于估算重型负载
量，共享队列入口验收也不证明计时噪声门槛通过；W3 仍须独立校准。

已授权的现有 `sfcdev` 别名经过静态配置审查后，只尝试一次有界非交互 SSH。命令 024
包含 BatchMode、ConnectTimeout=5、ConnectionAttempts=1、StrictHostKeyChecking=yes、
UpdateHostKeys=no、ControlMaster=no、ControlPath=none，并禁用转发和 agent/keychain 写入。
退出 255，约 5.02 秒，stdout 为空，错误是连接现有配置 endpoint 超时。没有重试、接受新
host key 或获取凭据；别名 OS、工具链与原生 Linux 可用性未能确认。

探测时源码与文档身份（当前发布文档哈希另列于 handoff.json）：

```text
probe_platform.py 4bc6080c668b66160c7ac7a2875492807f250eda1e45aa114189dcf3d13629ad
docs/VM_PLATFORM_FOLLOWUP.md b29ef85f7f9e121453438df0a75c7d159230eab74d94c7467ab82dac1ffaf805
result.json 868c7894c3ccfd528c3a9bcb1a4ba5f5bd677acf9b49c9154ffdd701c11be51e
```

下一步由独立审查验收本证据；G4 未下发前不执行最终冻结/全量矩阵。原生 Windows
执行通道和独立 Linux 可达设备仍是明确外部前提，库存审计不关闭 N1 或整体路线图。
平台文档的实际结果段在不可变证据生成后补写；保留探测时输入快照，不重跑或覆盖证据。

独立审查已通过：38 个证据文件、Windows 包及原冻结 archive 的字节身份均已只读复核；
未重新执行平台、SSH 或 Docker 命令，无阻挡发现。记录见 `independent-review.json`。
此结论只验收前提清单和取证完整性，G4 矩阵继续等待统筹。
