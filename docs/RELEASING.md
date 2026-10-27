# 发版流程

本页是 `next_cpp26` 打正式版和预发布版的固定流程。GitHub CI 永久停用（2026-10-23），
每一步的证据都来自 Mac 检查点和云端本地验证，不等任何 GitHub 作业。

分工：Mac 检查点、本地验证和发版 PR 由开发线程做；tag 由 susu 在自己的机器上打
（云端推 tag 会被拒绝，403）。susu 每次发版只需要执行第 5 步的 3 条命令。

## 1. 版本号

- 唯一的版本来源是根目录的 `XOFFSET_VERSION`：正式版 `MAJOR.MINOR.PATCH`（如 `3.0.0`），
  预发布版再加一个 `-(alpha|beta|rc).N`（如 `1.1.0-rc.1`），不带前导零。
  `cmake/XOffsetVersion.cmake`、`tools/validate_vm_package.py` 和
  `tools/prepare_release.py` 都从它读，格式不对时配置或打包直接失败。
- tag 名是 `v` 加版本号（`v3.0.0`），注释 tag，打在发版 PR 的合并提交上。仓库里 `v1.0.0`（2024，
  旧 v1 库，带公开 Release）和 `v2.0-*` 属于前两代实现，版本号不能和它们重名，发版前用
  `git ls-remote --tags origin` 核对一次。
- 安装包只接受完整版本字符串：`find_package` 的数字、`EXACT` 和范围请求一律拒绝，
  消费方用 `XOffsetDatastructure_VERSION STREQUAL "<版本>"` 自己核对（README「Build and consume」）。
  任何两个版本之间都不承诺 API、ABI 或 wire 兼容；1.x 内 wire 的冻结范围以 CHANGELOG 为准。

## 2. 发版前：Mac 检查点

按 MERGE_RULES_V2 第 2 节，每次打 tag 之前必须跑一次 Mac 阶段复核（检查点）：

- head 是候选提交（`next_cpp26` 上准备发版的那个提交，下称 C），anchor 是上一个检查点；
  同时编好上次检查点以来每个 S 类合并提交的二进制，失败时当场二分。
- 结果写在 project files 的 `perf-gate/mac/<日期>-<批次名>/VERDICT.md`。只有 VERDICT 判通过才往下走。
- 没通过：定位到具体 PR 后撤回或修复，再跑一次；赶不上时 tag 打在最后一个通过的检查点上，
  没过的改动挪到下一个版本（第 6 节）。

## 3. 发版前：云端本地验证

在 C 上跑全套本地验证，含 TSan：

```sh
tools/local_validate.sh --release --base <上一个 tag 或上一个检查点>
```

`--release` 在默认步骤（GCC 16 Release 全量 CTest、ASan+UBSan、`test_v2_fuzz`、
校验差分、bench smoke、两种编译器的 Ir 门禁、P2996 构建与签名核对）之外加上 GCC 16 TSan
全量 CTest。每一步都要是 pass（Ir 门禁对发版只看是否完成，不判 S 类）。`REPORT.md`
贴进发版 PR。

## 4. 发版 PR（0 类）

一个只改版本和文档的 PR，标题 `Release X.Y.Z`，base 是 `next_cpp26`：

1. `XOFFSET_VERSION` 改成新版本号。
2. `CHANGELOG.md`：把 `## Unreleased` 下的条目整段移到新标题 `## X.Y.Z: <一句话主题>` 下，
   标题下先写两三句这一版的范围（对上一个 tag 的主要变化、已知缺口），上面留一个空的
   `## Unreleased`。
3. 当前文档里的旧版本号改成新版本：`git grep -n '<旧版本号>' -- README.md 'docs/*.md'` 列出的
   README 第一段和消费示例、用户指南等文档标题、`ALPHA_COMPATIBILITY.md` 的消费示例。
   `docs/perf/`、`docs/validation/` 等历史记录不改。
4. 重新生成源码清单 `SOURCE_MANIFEST.json`，并确认打包工具在这个提交上能完整跑通：

   ```sh
   python3 tools/prepare_release.py create --source-dir . --out build/release-source
   cp build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json SOURCE_MANIFEST.json
   python3 tools/prepare_release.py audit \
     --archive build/release-source/xoffsetdatastructure-X.Y.Z.tar.gz \
     --manifest build/release-source/xoffsetdatastructure-X.Y.Z.manifest.json \
     --checksums build/release-source/SHA256SUMS
   ```

   `create` 读的是当前工作树（不读 Git），要先按 [PACKAGING.md](PACKAGING.md#fixed-inputs)
   准备好固定的子模块。`SOURCE_MANIFEST.json` 不在源码包里，所以复制进来不会改变清单本身。
5. 这个 PR 跑 `tools/local_validate.sh --steps gcc,p2996`（版本号进了 CMake 配置和安装元数据），
   再用一个新的 build 目录做一次 storage 安装，确认 `pack-prefix` 通过：

   ```sh
   cmake -S . -B build/release-storage -DXOFFSET_STORAGE_ONLY=ON -DXOFFSET_BUILD_TESTS=OFF \
     -DCMAKE_INSTALL_PREFIX=$PWD/build/release-storage-prefix
   cmake --build build/release-storage && cmake --install build/release-storage
   python3 tools/prepare_release.py pack-prefix --prefix build/release-storage-prefix \
     --component storage --out build/release-storage-pack
   ```

合并条件：第 2、3 步对 C 都通过，并且 C 之后 `next_cpp26` 上除这个 PR 以外没有别的合并。
发版 PR 不改库代码，所以 C 的检查点和本地验证结果原样适用于它的合并提交。期间有别的 PR
合进来，就把 C 换成新的头，第 2、3 步重做（只改文档、tools 的 0 类 PR 可以只重做第 3 步）。

## 5. 打 tag（susu 本地，3 条命令）

发版 PR 合并后，在本地仓库执行（`<提交>` 是发版 PR 的合并提交，开发线程会在线程里给出完整哈希）：

```sh
git fetch origin next_cpp26
git tag -a vX.Y.Z <提交> -m "XOffset vX.Y.Z"
git push origin vX.Y.Z
```

可选：在 GitHub 网页 Releases → Draft a new release，选刚推的 tag，标题 `XOffset vX.Y.Z`，
正文贴 CHANGELOG 里这一版的整段；需要的话附上第 4 步生成的源码包和 `SHA256SUMS`。
预发布版勾选 "Set as a pre-release"。

## 6. 检查点没过时

- tag 打在最后一个通过检查点的提交上；那个提交上的 `XOFFSET_VERSION` 和 CHANGELOG 不是新版本号，
  所以要从那个提交拉一个分支做发版 PR（只改第 4 步的内容），合并后对它的合并提交打 tag，
  或者推迟发版。两种做法在线程里写明选了哪个。
- 没过的改动记进下一个版本的计划，CHANGELOG 里留在 `## Unreleased`。

## 7. 发版之后

- 下一个版本所有「不回退」判定以这个 tag 的提交为基线。
- Ir 门禁的 anchor 前移：开一个只改 `bench/perf/ANCHOR`（改成 tag 的提交）的 0 类 PR，
  同时把失效的 ledger 条目移到 `bench/perf/ledger_archive.json`（[PERF_GATE.md](PERF_GATE.md) 第 4 节）。
- 在 project files 里记下 tag、提交和检查点 VERDICT 的路径。
