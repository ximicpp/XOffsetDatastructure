# 旧机材料取回清单（2026-10-20）

用途：补齐 Mac 上无法复现的三项——业务套件的 final-v4 对比工具元数据、V1 兼容 84+8 与 AB4/C4、生命周期 52。路径来自仓库与恢复包中的历史记录，旧机上未核实是否仍全部存在。

旧机发行版 Ubuntu-22.04。`/root/X` 在 Windows 侧为 `\\wsl.localhost\Ubuntu-22.04\root\X`；`R` = `/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发`。

## 必需

| # | 旧路径 | 用途 | 已记录身份 | 估计大小 |
| --- | --- | --- | --- | --- |
| 1 | `/root/xoffset-receive-completion/20261019/v1/final-v4/source/{experiments,tools}/serialization_comparison` | final-v4 对比工具元数据（五个适配器、driver.cpp、协议、campaign/protocol.py），用于业务 80 与正式对比 | 67 文件 1,364,554 B；逐文件 SHA 见 `docs/coordination/c14-iteration-20261020-v2/validation/combined_full/business/METADATA_DERIVATION.json` 的 `before`（如 driver.cpp 9708 B `9bb22e39…`，xoffset adapter 9619 B `95df972f…`） | ≈1.4 MB |
| 2 | 7 个对照的 `functional-normal-v1/`（只取 manifest/runtime/profile/status、`queue/manifest.json`、`commands/xoffset-*`）：`/root/xoffset-performance-v5/20261018/v1`、`/root/xoffset-performance-v4/20261018/v1`、`/root/xoffset-performance-v3/20261017/v1`、`/root/xoffset-performance-roadmap/20261017/v2`、`/root/xoffset-load-mutate-optimization/20261017/v1`、`/root/xoffset-verified-ranges/20261017/candidate-v1/repaired-campaign-v2`、`/root/xoffset-performance-v8/20261018/v1` | V1 main84 旧版本写出的 wire | V8 M input wire `9cd8ab46…`（13,702,104 B）；其余 pin/manifest 见 `tools/c14_iteration_20261020_v2/core/combined_full/business/workers/v1-compatibility.py` | 合计约 2 GB（未压缩） |
| 3 | 上述 7 个根目录下 `bench-normal-v1/{bin/sc_xoffset,CMakeCache.txt,compile_commands.json}`，以及 CMakeCache 中 `SC_XOFFSET_SOURCE`/`CMAKE_HOME_DIRECTORY` 指向的旧源码目录 | 新版本写、旧版本读的方向（旧 ELF 不能在 Mac 上跑，要在 Mac 用旧源码重编） | 见 `business_common.py`、`v1-compatibility.py` | 每份源码约 22 MB |
| 4 | `R/docs/coordination/{performance-v3-20261017,performance-v4-20261018,performance-v5-20261018,performance-v8-20261018,performance-roadmap-v2-20261017,load-mutate-optimization-20261017,verified-ranges-repaired-campaign-20261017}` | 上面 7 个对照的依赖锁和验收记录 | — | MB 级 |
| 5 | `/root/xoffset-receive-completion/20261019/v1/baseline/source`（或 `/root/xoffset-four-paths/20261018/v1/final-v1/source`） | four-paths 基线，AB4/C4 与 6 个 extra 需要 | 主头 264,452 B `b0608024fc8d…`，9 个生产文件 326,482 B | 约 28 MB（含 external 6 MB） |
| 6 | `R/build/receive-completion-20261019/production-matrix-v2`、`R/build/receive-completion-20261019/receive-pool-probe` | 生命周期 52 的作者源码 | receipt 2491 B `3eb479ea…`、3824 B `e3d1a430…` | <1 MB |
| 7 | `/root/xoffset-receive-completion/20261019/v1/production-C-matrix-v2/{author-matrix,author-pool}` | 第 6 项的实跑快照（备份） | 按第 6 项 receipt 核对 | <1 MB |

## 可选

`/root/xoffset-receive-completion/20261019/v1/` 下的 `*compatibility*/result.json`、`production-C-matrix-v2/result.json`、`profile-*-v2.json`、`final-source-identity-v4.json`；`/root/xoffset-serialization-comparison/20261017/implementation/integration-v1/configure.request.json`；`/root/xoffset-serialization-comparison/20261017/p1-v1/inputs/`；`R/build/four-path-implementation-20261018/*.py`、`R/build/receive-completion-20261019/*.py`。

不要带：485 GB 逻辑证据、`prepared/`、其他四个库的 wire、`*.canonical`、`*.queries`、build 树、external、.git。

## 打包（在旧机 WSL 里以 root 执行）

```sh
cd / && CTRL="root/xoffset-performance-v5/20261018/v1 root/xoffset-performance-v4/20261018/v1 root/xoffset-performance-v3/20261017/v1 root/xoffset-performance-roadmap/20261017/v2 root/xoffset-load-mutate-optimization/20261017/v1 root/xoffset-verified-ranges/20261017/candidate-v1/repaired-campaign-v2 root/xoffset-performance-v8/20261018/v1"; R='mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发'; RC=root/xoffset-receive-completion/20261019/v1
SRC=$(for r in $CTRL; do sed -n 's#^\(SC_XOFFSET_SOURCE\|CMAKE_HOME_DIRECTORY\):[A-Z]*=/##p' /$r/bench-normal-v1/CMakeCache.txt; done | sort -u); echo "$SRC"
tar --ignore-failed-read -czf /mnt/c/Users/gzsufanchen/Desktop/xoffset-mac-retrieval-20261020.tar.gz \
  --exclude='*/external/*' --exclude='*/.git/*' --exclude='*/prepared/*' --exclude='*.canonical' --exclude='*.queries' \
  --exclude='*/commands/cista-*' --exclude='*/commands/flatbuffers-*' --exclude='*/commands/capnproto-*' --exclude='*/commands/cereal-*' \
  $RC/final-v4/source/experiments/serialization_comparison $RC/final-v4/source/tools/serialization_comparison \
  $(for r in $CTRL; do echo $r/functional-normal-v1 $r/bench-normal-v1/bin/sc_xoffset $r/bench-normal-v1/CMakeCache.txt $r/bench-normal-v1/compile_commands.json; done) $SRC \
  $R/docs/coordination/{performance-v3-20261017,performance-v4-20261018,performance-v5-20261018,performance-v8-20261018,performance-roadmap-v2-20261017,load-mutate-optimization-20261017,verified-ranges-repaired-campaign-20261017} \
  $RC/baseline/source $R/build/receive-completion-20261019/production-matrix-v2 $R/build/receive-completion-20261019/receive-pool-probe \
  $RC/production-C-matrix-v2/author-matrix $RC/production-C-matrix-v2/author-pool \
  $RC/*compatibility*/result.json $RC/production-C-matrix-v2/result.json $RC/profile-*-v2.json $RC/final-source-identity-v4.json \
  root/xoffset-serialization-comparison/20261017/implementation/integration-v1/configure.request.json root/xoffset-serialization-comparison/20261017/p1-v1/inputs \
  $R/build/four-path-implementation-20261018/*.py $R/build/receive-completion-20261019/*.py 2> /mnt/c/Users/gzsufanchen/Desktop/xoffset-mac-retrieval-20261020.tar.log
sha256sum /mnt/c/Users/gzsufanchen/Desktop/xoffset-mac-retrieval-20261020.tar.gz | tee /mnt/c/Users/gzsufanchen/Desktop/xoffset-mac-retrieval-20261020.tar.gz.sha256
```

说明：
- 第二行会先打印 `$SRC`，确认它列出的是旧源码目录。
- 缺失的路径会记进同目录的 `.tar.log`（"Cannot stat"），不会中断打包；这个日志请一起带回。
- 预计压缩包 1–2 GB。只是复制文件，不在旧机启动任何开发或性能工作。

## 带回 Mac 之后

把 `.tar.gz`、`.sha256`、`.tar.log` 三个文件放到这台 Mac 的 `~/Downloads/`（或告诉我实际路径）。我会解到新的空目录 `/Volumes/XOffsetCS/retrieval-20261020/`，按上表逐项核对 SHA，只读引用，不覆盖恢复包或仓库里的原件。
