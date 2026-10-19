# 2026-10-19 跨机器最小转移清单

本次仅整理交接，**开发仍暂停**。以 `pause-state-20261019-v1.json` 为当前控制状态；
`current-stage-v15.json` 是暂停前状态，不能照其中的 next 自动启动工作。暂停记录声明
旧进程均已恢复、全局租约空闲、旧锁文件保留。新机不能沿用旧 PID、队列、锁状态或
旧一次性执行请求。未获恢复开发指令前，不构建、不运行 runner、不启动新 A/A。

本清单只读了指定源码/状态、小型 metadata、Git 状态和普通文件长度，没有运行项目、
导入 runner、扫描 SDK/原始性能正文、打包或转移文件，也没有重算大归档哈希。

## 路径与当前 Git 身份

以下路径相对旧工作区 `C:\Users\gzsufanchen\Documents\ChatGPT\xoffsetdatastructure开发`：

- `H` = `build/receive-completion-20261019/`
- `D` = `H/delivery-v2/`
- `S` = `docs/coordination/receive-completion-20261019/`
- `X` = `docs/coordination/cross-machine-handoff-20261019/`

只读 Git 结果：

| 项目 | 实际值 |
|---|---|
| branch | `codex/offline-codegen-cpp20-20261018` |
| HEAD | `7539433ab96d69c4a07b1db5c0a13786da85cdcd` |
| origin fetch/push URL | `https://github.com/ximicpp/XOffsetDatastructure.git` |
| HEAD 说明 | `Publish accepted VM handoff for cross-machine continuation [skip ci]` |
| tracked dirty | 11 文件；本次使用 `--untracked-files=no`，未长扫描 untracked |

当前 Git 根 main 为 183050 B；已验收 C14 main 为 278262 B。**仅克隆当前 HEAD 或
读取根 `HANDOFF.md`，不能得到 C14。** 根 HANDOFF 是早期 VM 交接材料；本轮先读 X
中的新交接，再看 S 的暂停与最新状态。当前 branch 是否已存在于远端，本次未查询；
remote URL 不是已推送证明。本次没有提交、推送、合并、发布或触发远端 CI。

11 个 tracked dirty 路径须保护：`CMakeLists.txt`、`HANDOFF.md`、`README.md`、
`cmake/XOffsetDatastructureConfig.cmake.in`、`cmake/XOffsetInstall.cmake`、
`docs/STABLE_VM_STORAGE_MIGRATION.md`、`docs/VM_PACKAGING.md`、
`openspec/changes/vm-next-stage-hardening/tasks.md`、`toolchains/README.md`、
`tools/tests/test_validate_vm_package.py`、`tools/validate_vm_package.py`。
若要继续整个旧工作区，另保留这些变更的 binary diff/文件副本和路径清单；本清单不
代替变更备份。Git diff 也不包含下列被忽略的 `build/` 交付，必须逐项转移。

## 必须携带的 C14 源码闭包

优先选已验收 source tar，解压到新目录；不要覆盖当前根源码或已有用户修改。保留
原相对路径，或在新的转移 manifest 中明确重定位映射。

| 路径 | 字节数 | 身份/用途 |
|---|---:|---|
| `D/artifacts/source/xoffsetdatastructure-0.1.0-alpha.1.tar.gz` | 1276597 | SHA256 `a6dd9efe0648156a8b8db3af1a39b1ff3906ca7430875f703edb356edbff13b7` |
| `D/artifacts/source/xoffsetdatastructure-0.1.0-alpha.1.manifest.json` | 208058 | SHA256 `bf2209111f826b6ea98e26f9ac5e63ccb94bdbcb4f8152026abee9f7e185fb88` |
| `D/artifacts/source/SHA256SUMS` | 223 | 原校验清单 |
| **以上 3 文件** | **1484878** | 不需要同时复制已解码源码和 ZIP |

archive 解码集合是 1008 文件、8038649 B；以 manifest/新机器解码核对为准。
验收的 C14 主头 SHA256 是
`66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。

Windows 若确需 ZIP，可用同目录 `xoffsetdatastructure-0.1.0-alpha.1.zip`
（1939159 B，SHA256 `3ddeb05afa78f566d4673d4ca19933a74f29edb45f1250be30bf69d34b7495e4`）
替代 tar；复制匹配的清单和 `zip-audit.json`。这是格式选择，不必同时保存两个副本。
full 安装前缀包（1026010 B）、storage 包（14695 B）、解码 `review-source/` 可作为
可选验证输入，不属于最小开发源码闭包。不要把安装前缀包当完整源码包。

## 最新交接和小型验收状态

携带 X 整个最终交接文档目录；该目录仍由主统筹完成清单/身份，因此此处不预填其
最终大小。现已读到 `PROJECT_AND_ENVIRONMENT.md` 和 `PERFORMANCE_STATUS.md`；
这两份说明分别提供项目/平台边界和最新性能状态。

另携带下列明确文件，避免搬运整个 delivery 或整个 build：

| 文件组 | 具体相对路径 | 字节数 |
|---|---|---:|
| C14 总体验收 | `S/ACCEPTANCE_20261019.md` | 8074 |
| C14 最终父验收 | `S/final-acceptance-v6.json` | 3216 |
| 交付父验收 | `S/delivery-v2-root-acceptance.json` | 464 |
| 交付结果 | `D/result.json` | 19110 |
| 交付总清单 | `D/delivery-manifest.json` | 248180 |
| 14 文件源码身份 | `D/acceptance/candidate-identity.json` | 2969 |
| 后续补充证据索引 | `H/delivery-supplemental-v1/manifest.json` | 10795 |
| **上述 7 文件合计** | 其被引用的大型正文并未因此复制 | **292808** |
| 当前暂停 | `S/pause-state-20261019-v1.json` | 1626 |
| 暂停前最新阶段 | `S/current-stage-v15.json` | 1615 |
| allocation 关闭决定 | `S/allocation-screen-decision-v1.json` | 8103 |
| bitmap 关闭决定 | `S/bitmap-effect-decision-v1.json` | 4151 |
| scoped-specialized 关闭决定 | `S/scoped-specialized-effect-decision-v1.json` | 3714 |
| **上述暂停/决定 5 文件合计** | 三个独立 screen 均未晋升 | **19209** |

C14 在旧固定 Linux/WSL P2996/libc++ profile 下的功能与干净交付已验收；新机平台
结果仍未产生。性能目标未完成，`AA_status=not_established`，新五库正式比较次数 0。
三个关闭 screen、旧 Q32/F64 都不能因换机器而重开或追加样本。

## 下一 A/A 的工具和已经产生的结果

| 路径 | 文件数/字节数 | 作用 |
|---|---:|---|
| `H/new-AA-runtime-maxima-tools-v2/` 整个小目录 | 11 文件，44774 B | 固定旧 Q32 process-receipt 最大值提取器、3 个小 fixture、纯测试源码、修正来源与 receipt |
| `H/new-AA-qualification-design-v1/` 整个小目录 | 5 文件，18554 B | conditional design、evidence、receipt，以及 `profile-addendum-v1/` 两文件；不要漏掉 profile 修正 |
| `S/AA-runtime-maxima-v2-request.json` | 704 B | 原实际请求 metadata，不能在新机原样调度 |
| `S/AA-runtime-maxima-v2-receipt.json` | 507 B | 原外层完成回执 |
| `S/AA-runtime-maxima-v2-derivation.json` | 638 B | v2 派生记录 |
| `H/AA-runtime-maxima-worker-v2.py` | 876 B | 原实际 worker 源码证据；不是新机可直接使用的入口 |
| **上述 dispatch 4 文件合计** | **2725 B** | 旧路径/导入环境只作引用 |

v2 在暂停前已完成 5 个纯工具检查和 metadata 提取，没有新 native/sample；旧 v1
失败保留。工具作者 receipt 的“作者未执行”与主统筹后来实际执行是不同层次，不要
把其中任何一份改写。提取器固定绑定旧四路径 Q32，不是历史 V5 的另一次成功 Q。

**推荐另转移已生成的完整结果 metadata，而不是重新提取：**

- 旧机 WSL `/root/xoffset-receive-completion/20261019/v1/new-AA-oldQ-process-maxima-v2/result.json`：
  3376551 B，SHA256 `642a17384f0a8472027acdbf4c7303db8f66f50266a6cdca9b01d6f3c323e7ab`。
  此身份取自 X 的最新性能整理，不是本清单对原 WSL 文件的重新读取/散列。它保存
  4656 条 elapsed/来源索引，可支持新机继续复核 maxima；不包含 wire/native 正文。
- 同 B 根的 `AA-runtime-maxima-dispatch-v2/result.json`，以及相邻 authority/请求/
  完成小记录：由主统筹按实际复制范围列入转移 manifest。这里未访问其 WSL 文件，
  大小不猜测。移到 X 下新 evidence 目录即可，保留原路径与 FID 的映射。

若不转移 3376551 B 的完整结果，必须在最终清单明确写“原机引用，未随包转移”；
新机只能读 X 中的派生小摘要，不能宣称独立重算过全部 receipt 来源或完整复核结果。
即使转移了结果，未携带的旧 4656 份原 receipt/manifest/body 审计仍只能引用。

本地已核对的 6 组文件（C14 source 3、C14 metadata 7、暂停/决定 5、AA dispatch 4、
工具 11、设计 5）共 **35 文件、1862948 B**。加上上述完整 maxima result 是
**5239499 B**，再加 X 的最终交接文件、小型 dispatch 结果和需要保护的 Git 变更。
这比复制整个 build/WSL 树小得多，不需要任何旧 native/body 重放。

`extract.py` 不是新 A/A campaign runner；它固定旧 `/root/.../qualification-v1` 输入，
复制工具本身并不能使该数据集在新机出现。下一步只在恢复开发后复核既有 maxima，
明确新机 profile、实际 binary 和预算，再冻结独立新资格方案。profile addendum
明确区分 correctness 的 `-UNDEBUG` 与 normal performance 的 `-O3 -DNDEBUG`；
不能将 correctness 参数直接复制成性能 profile，也不能修改旧封存 binary。

## 只引用、不默认转移的原始大档

- 旧 WSL `/root/xoffset-four-paths/20261018/v1/qualification-v1/`、其 commands/receipts、
  `frozen-qualification-v1/`、report-qualification 和旧正式窗口正文；以及各 screen
  的 command/native JSON、wire、canonical、queries、trace、body pool。小摘要中的
  哈希、passed 或来源位置不等于这些原件已随包转移。
- 旧 `/root/xoffset-receive-completion/20261019/v1/` 下构建树、二进制、计时窗口 body、
  队列运行目录和完整审计中间产物；只选上面列出的 metadata。不要归档整个该根目录。
- `/opt/p2996-toolchain/`、匹配 libc++/libc++abi、Clang 21 sanitizer resource、依赖
  checkout、编译缓存及旧 WSL distro/虚拟磁盘。新机按照源码锁与可验证来源重新准备；
  本次不遍历或估算这些目录大小，不把旧 SDK 路径当成新机能力。
- 旧 `.git/`、`build/vm-coordination/queue.lock` 和 PID/handle：不是自动恢复队列的
  输入。原锁保留在原机；新机使用独立协调状态，不能删除旧锁来伪造释放。

若后续确需某个原始 fixture，再列明其用途、最小文件闭包、FID 和新目标目录后单独
转移；不为使历史脚本“看似能跑”复制全部 build，也不回写历史 manifest。

## 新机接续检查顺序

1. 先核对 X 的最终转移 manifest、当前暂停状态和三份交接说明；保持暂停。
2. 校验 C14 source archive/manifest，解码到隔离新目录，确认 14 文件身份；迁移完整性
   通过不等于平台测试通过。
3. 区分已带文件与原机引用，记录新的路径映射；保留 Git dirty 变更，不 reset。
4. 记录实际 OS/架构、原生/仿真、P2996/libc++/sanitizer 能力；在恢复开发授权前不执行
   构建或 runner。缺失大档明确标注，不假造新机复现/资格结果。
5. 恢复开发后优先处理既有 maxima 的复核和独立新 A/A 前置设计；不要重跑已关闭的
   screen、旧 Q/F，也不要直接开始五库正式排名。提交/推送/合并/发布/远端 CI 仍未授权。
