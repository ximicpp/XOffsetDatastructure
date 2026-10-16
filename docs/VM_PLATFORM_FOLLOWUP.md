# W4 平台前提与只读取证

日期：2026-10-15。本项交付平台前提清单与冻结 Windows 包完整性检查；所有编译、测试
和性能状态均为 `unexecuted`。Windows storage/package、Windows 完整 P2996、原生独立
Linux 性能以及最终 G4 矩阵分别保留状态，不能由库存、交叉编译器或历史结果推断通过。

入口为 `experiments/vm_platform_validation/probe_platform.py`，首次证据目录固定为
`experiments/vm_platform_validation/evidence/platform-20261015-01`：

```text
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_platform_validation/probe_platform.py --out experiments/vm_platform_validation/evidence/platform-20261015-01
PYTHONDONTWRITEBYTECODE=1 python3 experiments/vm_platform_validation/probe_platform.py --audit experiments/vm_platform_validation/evidence/platform-20261015-01
```

探测只写新的证据目录；输出已有时拒绝覆盖。`commands/` 保留每条实际 subprocess 的
argv、退出码、标准输出、标准错误、截止时间与执行时间。`inputs/` 保存脚本、计划、
平台说明、工具链清单、F 双头和 Windows 包的输入快照；`result.json` 记录前后身份与
前提状态；`manifest.json` 固定完整证据文件集合、长度和 SHA-256。离线 audit 不运行
任何平台命令、不修改证据。

本机完整库工具链按 `toolchains/vm-validation.json` 核对编译器、libc++、libc++abi 和
TSan runtime 哈希。TSan 搜索同时覆盖完整库编译器的 resource directory、已配置
Developer 目录、CommandLineTools 和已安装 Xcode 的已知 runtime 路径，不以
`/usr/local` 中缺失 runtime 推断全机缺失。冻结 `MacOSX26.5.sdk` 的存在性与系统默认
SDK 分别记录；后续编译须显式传入冻结 SDK，不能继承已变化的 xcrun 默认值。

Docker 只读取本地 Unix socket daemon 的身份、已知 P2996 镜像元数据和当前容器清单；
非 Unix endpoint 不连接。OrbStack 只列出现有机器，不进入、启动、创建或停止机器。
本地 ARM64 Linux VM 属虚拟化环境；既有 Linux amd64 P2996 镜像在 ARM64 宿主执行时
属于跨架构仿真。两者均不能作为本轮原生独立 Linux 性能通过的证据。

本轮另获授权对现有 `sfcdev` SSH 别名做一次有界非交互探测。静态配置审查只判定已
明确配置的 alias 与影响该 alias 的控制指令，不归档配置内容，不读取密钥文件。
出现未审查 Include、Match 或相关 ProxyCommand/ProxyJump/LocalCommand/RemoteCommand
则跳过。实际连接固定 BatchMode、5 秒连接上限、一次尝试、严格既有 host-key 检查，
禁用新 host-key 接受、转发、连接复用、钥匙串和 agent 写入。远端只读取 uname、
`/etc/os-release` 与已知编译/构建工具路径；失败不重试、不获取凭据，成功也不证明
编译、运行或性能。确切结果以首次证据中的命令记录为准。

Windows 包沿用 [既有交接说明](VM_WINDOWS_HANDOFF.md)，核对 ZIP 的精确成员、逐文件
长度/哈希、source manifest 固定 SHA、冻结 archive 身份、F 双头和 CRC。该检查只证明
包内字节一致，不证明 PowerShell、MSVC、VirtualAlloc/VirtualFree 或安装 consumer
实际执行。原生 Windows x64 前提为 MSVC x64 Developer PowerShell、C++ 工作负载和
Windows SDK、Python 3.9+、CMake/CTest 3.21+，以及 NMake 或预装 Ninja。应在包外新结果
目录执行并交回完整证据；预期为 storage 2 项、installed consumer 3 项、subproject
consumer 3 项。完整 Windows P2996 工具链与依赖另行验证，此包不能关闭该项。

交接入口为 `experiments/vm_platform_validation/handoff.json` 和 `HANDOFF.md`。平台前提
评估可交审查，最终 selected source、全量/sanitizer/TSan/wire/安装消费矩阵均等待统筹
下发 G4。本轮不修改生产头、公共构建/迁移文档或历史证据。

## 首次评估的实际结果

上述首次探测只执行一次，退出 0，记录 24 条实际 subprocess；离线 audit 退出 0，
精确复核 38 个证据文件。当前 F 双头、11 个依赖 include tree 和 3 个许可证均匹配，
探测期间输入身份未改变。manifest SHA-256 为
`09b09e30a230ae1033bdd2320ea9e28d7041352f6c72c89f9235c5e58d9aca96`。

本机实际为 macOS 26.6.2 / Darwin 25.6.0、arm64、Mac15,7、12 CPU。P2996 Clang 提交
`d77eff1cbd78fd065668acf93b1f5f400d39134d`、target 和编译器二进制哈希匹配冻结清单；
libc++ 和 libc++abi 也匹配。实际 TSan runtime 位于
`/Library/Developer/CommandLineTools/usr/lib/clang/21/lib/darwin/libclang_rt.tsan_osx_dynamic.dylib`，
SHA-256 `6f7114babe190d1010186c1106e05112e58946502d8cec7bb98beebcab7aa0e9`，匹配清单；
`21.0.0` 入口解析至同一文件。冻结 26.5 SDK 仍存在，xcrun 默认确为 27.0。

Windows ZIP 的 126 个成员、121 个源码文件、1,173,446 源码字节、source manifest、F
双头与 CRC 均匹配。ZIP SHA-256 为
`80adbf3e28a93c5ae320e8444052362343d318544653e0ec633daac77c8d9202`；source manifest SHA
为 `b4db950e010cfe68958360103a5cbfe3330f565c9ec63d2b0f88897daa5d8025`。仅完整性通过，
包仍 `prepared-not-windows-tested`，原生 Windows 与完整 P2996 前提均未满足。

OrbStack 清单列出已运行的 Debian bookworm ARM64 VM `jojocpp-hydro`；没有进入机器。
Docker Unix socket daemon 为 aarch64 Linux OrbStack，已知 P2996 镜像为 linux/amd64。
一次 SSH 探测返回 255，耗时约 5.02 秒，stdout 为空，原因是现有 `sfcdev` 配置 endpoint
连接超时。没有重试，未确认远端 OS 或工具链；独立原生 Linux 性能仍 `not_verified`。

当时 Docker 库存另显示一个无关运行容器 `upbeat_goldstine`，镜像为
`layoutcompare-r2-family-f4-python3137`。本项没有拥有、进入、停止或修改该容器。库存
仅说明可能存在背景活动，不能据此估算重型负载量；本地队列入口验收不建立计时噪声
门槛，正式测量仍由 W3 独立校准。

本结果段在不可变证据生成后补写。证据 `inputs/` 保留探测时的方法说明版本；交接 JSON
分别记录探测输入与当前发布文档哈希，不改写原始输入或重新执行探测。
