# Windows 原生存储验证交接包

本项为第一阶段 N1 的并行准备工作。包已在 macOS 上完成冻结源码完整性与脚本负向验证，**尚未在 Windows 执行，不能视为 Windows 支持验证通过**。它验证现有 F 的独立 C++17 OS 存储层与安装消费，不验证完整 P2996 库，也不占用 N6 的编译、sanitizer 或性能测量窗口。

## 输入与范围

输入固定为第二阶段最终源码快照：

- `docs/validation/2026-10-15-vm-stage2/final-source/source.tar.gz`，SHA-256 `c1a69f06cf67db2f773368d5828798873a9faac071f40f0e4c5550507897b3a3`。
- 同目录 `source-manifest.json`，SHA-256 `b4db950e010cfe68958360103a5cbfe3330f565c9ec63d2b0f88897daa5d8025`。
- F 的 `xoffset_virtual_memory.hpp`，SHA-256 `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`。

包保留冻结快照的完整 selected surface：121 个文件、1,173,446 字节。包括 OS 头、两个独立存储测试、CMake 支持文件、三个编译单元的 package consumer、现有 package 验证器和许可证。它也包含冻结的完整库头和已有说明/工具源码，但没有 Boost、TypeLayout、`external/`、`.git/` 或构建产物；完整库模式因此不属于此交接包支持范围。

不从当前正在变更的 N6 工作区复制源码，不修改历史证据、公共 CMake/CI、生产头或签名基线。包中既有 `tools/validate_vm_package.py` 已提供 Windows 默认编译器/MSVC 路径；本项只准备调用入口，仍需原生实测确认。

## 准备与传递

在仓库根目录执行（纯 Python，无下载、配置或编译）：

```sh
python3 experiments/vm_windows/prepare.py \
  --source-archive docs/validation/2026-10-15-vm-stage2/final-source/source.tar.gz \
  --out build/windows-f-storage-handoff
```

`--source-manifest` 可显式指定，省略时使用归档旁的 `source-manifest.json`；其内容必须匹配上述固定身份。`--out` 必须不存在，已有目录、文件或悬空链接都会被拒绝。

准备器先检查归档/manifest 身份，再逐项检查文件名、类型、大小和 SHA-256。拒绝目录条目、链接、设备文件、稀疏文件、重复条目、Windows 大小写冲突、路径穿越和 Windows 非法/保留文件名；仅在全部验证成功后写入新目录。包级 `bundle-manifest.json` 包含所有输出文件（自身除外）的精确清单。

已生成可移动示例包：`experiments/vm_windows/artifacts/frozen-f-storage.zip`，其校验值在同目录 `SHA256SUMS`。解压后的目录可直接移动到 Windows，无需仓库或 Git。

## Windows 执行

环境要求：Windows x64，已安装 C++ 工作负载和 Windows SDK 的 MSVC Developer PowerShell，Python 3.9+，CMake/CTest 3.21+。默认生成器为 NMake；也可指定 `Ninja`，需预先安装。脚本不下载或安装任何依赖。

CTest 最低版本来自 `--output-junit` 于 3.21 加入；测试发现使用 `--show-only=json-v1`。参见 [CTest 官方参数文档](https://cmake.org/cmake/help/v3.24/manual/ctest.1.html)。

在 x64 Developer PowerShell 中：

```powershell
Expand-Archive .\frozen-f-storage.zip -DestinationPath .\windows-f-storage
.\windows-f-storage\run-windows.ps1 -OutDirectory .\windows-f-storage-results
```

结果目录必须不存在，且位于包目录外。PowerShell 先确认 Windows 和 Python；Python 驱动在任何配置前核对 CMake、CTest、`cl`、`link`、NMake/Ninja 和 x64 MSVC 开发环境。配置后还检查实际编译器 ID 为 MSVC，防止错误编译器被记录为 MSVC 通过。

验证顺序：

1. 核对包清单和全部冻结源码。
2. 配置并构建 Release、storage-only 测试，保留断言。
3. 比较 CMake 生成清单和 CTest 实际发现清单，必须恰好包含 `test_virtual_memory_storage`、`test_vm_pool`。
4. 运行两项测试，检查退出码与 JUnit：不接受空清单、缺失、重复、跳过、未运行或失败项。
5. 调用冻结的 package 验证器：安装、移动安装目录并隐藏原 producer source，执行外部 consumer 三项；再执行子项目 consumer 三项。验证 storage-only 安装拒绝 full component。
6. 独立复查 package `result.json` 和两份 consumer JUnit，再核对原包未改变，最后才将总状态写成 `passed`。

现有测试覆盖保留/增长、边界和失败注入、尾页回收与重新增长、载入零值语义、移动/交换/释放、池复用/限额/生命周期/共享线程；package consumer 覆盖跨编译单元转移、池 handle 早于 owner 销毁和静态销毁。它不是生产负载基准，也不包含完整库的 wire/反射测试。

## 结果交回与判定

保留整个 `windows-f-storage-results` 目录，至少审查：

- 顶层 `result.json`：原生系统、工具路径、MSVC 环境、命令退出码、冻结来源、最终状态；通过时 `storage_tests = 2`，`package_tests = {installed: 3, subproject: 3}`。
- `storage-discovery.json`、构建内 `test-inventory.json` 和 `storage-tests.xml`。
- 每条命令的日志、编译器配置记录与 `package/result.json`、`package/consumer-tests.xml`、`package/subproject-tests.xml`。
- package 中的 source manifest、安装文件审计、移动后的安装目录及 producer/consumer 构建记录。

失败退出非零并保留已有证据。缺少 Windows/Python 属于 PowerShell 启动失败，会在控制台报错；进入 Python 驱动后的环境、构建或验证失败均写入 `result.json`。配置成功不等于运行通过。当前包不接受重跑到旧结果目录，也不自动删除证据。

## 本机已验证与后续边界

运行 `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s experiments/vm_windows -p 'test_*.py' -v`，18 项轻量检查通过。涵盖固定源码打包、完整性、归档路径/类型/重复/大小、错 F 拒绝、新目录约束、空/重复/失败 JUnit、清单失配，以及非 Windows 执行必须失败且不调用构建工具。首次测试有一处负向测试夹具把结果目录放在包内，被正确拒绝；已修正夹具后全数通过。

尚未验证 PowerShell 实际执行、Windows MSVC 构建和运行、Windows 内存提交/回收行为、Windows 性能、Windows 完整 P2996 工具链。远端 CI 未触发。收到原生结果后再审查并更新支持矩阵；第三阶段若采用改变 OS 层或安装界面的实现，还需对最终源码另建身份明确的包复测。
