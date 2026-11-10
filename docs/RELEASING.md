# 发版流程

本页是 `next_cpp26` 打正式版和预发布版的固定流程。GitHub CI 永久停用（2026-10-23），
每一步的证据都来自云端本地验证，不等任何 GitHub 作业，也不等 Mac（susu 2026-11-05 定：发版前一律
只用云端；Mac 阶段复核在发版后跑，第 7 节）。

分工：云端判定、本地验证和发版 PR 由开发线程做；tag 由 susu 在自己的机器上打，或请 Mac 会话代推
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

## 2. 发版前：云端性能判定

从 3.3.1 起，发版前的性能判定只用云端（MERGE_RULES_V2 补充，2026-11-05），不再跑 Mac 发布批：

- 候选提交（`next_cpp26` 上准备发版的那个提交，下称 C）对上一个发版 tag 跑 GCC 16 和 P2996 两个 Ir 门。
  它们就是第 3 步 `local_validate --release --base <上一个 tag>` 里的 `ir` 和 `p2996-ir` 两步：base 是上一个
  tag，第 7 步又把 `bench/perf/ANCHOR` 移到了上一个 tag，所以 head÷base 和 head÷anchor 都是 C 对上一个 tag。
- 判定（两个编译器各自判）就是工具对 head÷base 的判定行，和 S 类门禁同一套阈值（[PERF_GATE.md](PERF_GATE.md)
  第 3 节）：任何一个合计 > 1.010，或任何一个阶段 > 1.05，是 fail；任何一个合计 > 1.003，或任何一个
  阶段 > 1.02，是警告。发版前不再有 Mac 把关，云端是唯一的发版判定，所以不比工具自己的线更松。fail
  不能发版：要么修，要么带数据找 susu。警告的指标连同函数差异表写进发版 PR，由「性能优化」决定，并写进发布说明。
- 没通过：定位到具体 PR 后撤回或修复，再跑一次；赶不上时 tag 打在最后一个通过的提交上，
  没过的改动挪到下一个版本（第 6 节）。
- 合并规则也只用云端（susu 2026-11-05 01:26Z）：S 类和 L 类 PR 合并前都只过 GCC 16 和 P2996 两个 Ir 门
  （S 类阈值，即上面这套），再在合并结果上跑 `local_validate`；L 类另附 cache-sim 的 D1/LL 缺失对比，
  作为警告项（[PERF_GATE.md](PERF_GATE.md) 第 5a 节）。Mac 只做合并后和发版后的复核，只报，不挡合并。

## 3. 发版前：云端本地验证

在 C 上跑全套本地验证，含 TSan：

```sh
tools/local_validate.sh --release --base <上一个 tag>
```

`--release` 在默认步骤（GCC 16 Release 全量 CTest、ASan+UBSan、`test_v2_fuzz`、
校验差分、bench smoke、两种编译器的 Ir 门禁、P2996 构建与签名核对）之外加上 GCC 16 TSan
全量 CTest。每一步都要是 pass，两个 Ir 门按第 2 节判。客户端列里容器没有工具链、报
「not run」的（例如 Android NDK），在发版 PR 和文档里写明这一版没有重跑。`REPORT.md` 贴进发版 PR。

### 3.1 发版前 fuzz（K4）

同一个 C 上，对每个接收入口跑一轮 ASan+UBSan fuzz（susu 2026-11-04 定；不再每晚跑）。云端后台任务
2 小时会被停，所以按阶段分开跑，每个阶段一个后台任务（GCC 阶段先按 `fuzz-nightly/README.md` 第 1 步
启动 dockerd）：

```sh
OUT=/mnt/project-files/release-X.Y.Z/fuzz
tools/fuzz_prerelease.sh --out $OUT --phase gcc        # GCC 16 Docker 差分 fuzz + 回放，约 50 分钟
tools/fuzz_prerelease.sh --out $OUT --phase libfuzzer  # 服务端反射构建的全部 libFuzzer 目标
tools/fuzz_prerelease.sh --out $OUT --phase client     # 客户端字段表构建 + 三种构建结果比较
tools/fuzz_prerelease.sh --out $OUT --phase verdict    # 写 VERDICT.md，PASS 才往下走
```

预算：默认 `--budget release`（GCC 2700 秒，libFuzzer 每个目标每种构建 1200 秒，合计约 4 小时）。
3.5 起按 susu 2026-11-10 的决定用 `--budget hour`（GCC 900 秒，libFuzzer 每个目标 300 秒，九个目标合计
约 1 小时，另加构建时间），四个阶段的 `--budget` 必须一致；`quick` 只是试跑，不算数。

VERDICT 只有在三个阶段都跑在 C 上（工作树干净）、预算是 `release` 或 `hour`、每个作业成功且没有发现时才是
PASS。有发现：`tools/fuzz_report.py` 开 issue（用法见 `tools/fuzz_nightly.sh` 开头），修复 PR 把最小化
后的输入放进 `tests/fuzz/regressions/<target>/`，合入后在新的 C 上重跑。VERDICT.md 贴进发版 PR。

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
发版 PR 不改库代码，所以 C 的云端判定和本地验证结果原样适用于它的合并提交。期间有别的 PR
合进来，就把 C 换成新的头，重跑第 3 步（第 2 步的两个 Ir 门在同一次运行里）。

## 5. 打 tag（susu 本地，3 条命令）

发版 PR 合并后，在本地仓库执行（`<提交>` 是发版 PR 的合并提交，开发线程会在线程里给出完整哈希）；
也可以在 Mac 线程里请 Mac 会话代为执行（3.2.1、3.3.0 都是这样推的）。推好后开发线程用 `git ls-remote` 核对：

```sh
git fetch origin next_cpp26
git tag -a vX.Y.Z <提交> -m "XOffset vX.Y.Z"
git push origin vX.Y.Z
```

可选：在 GitHub 网页 Releases → Draft a new release，选刚推的 tag，标题 `XOffset vX.Y.Z`，
正文贴 CHANGELOG 里这一版的整段；需要的话附上第 4 步生成的源码包和 `SHA256SUMS`。
预发布版勾选 "Set as a pre-release"。

## 6. 云端判定没过时

- tag 打在最后一个通过第 2、3 步的提交上；那个提交上的 `XOFFSET_VERSION` 和 CHANGELOG 不是新版本号，
  所以要从那个提交拉一个分支做发版 PR（只改第 4 步的内容），合并后对它的合并提交打 tag，
  或者推迟发版。两种做法在线程里写明选了哪个。
- 没过的改动记进下一个版本的计划，CHANGELOG 里留在 `## Unreleased`。

## 7. 发版之后

- 下一个版本所有「不回退」判定以这个 tag 的提交为基线。
- Ir 门禁的 anchor 前移：开一个只改 `bench/perf/ANCHOR`（改成 tag 的提交）的 0 类 PR，
  同时把失效的 ledger 条目移到 `bench/perf/ledger_archive.json`（[PERF_GATE.md](PERF_GATE.md) 第 4 节）。
- Mac 阶段复核：发版后跑一次（head 是这个 tag 的提交，anchor 是上一个 tag），排在发版后的第一个 Mac 空档，
  不能无限期往后推；按 L2 规则判，只报，
  结果写在 project files 的 `perf-gate/mac/<日期>-<批次名>/VERDICT.md`。发现回退就开修复 PR，进下一个补丁
  版本，并写进下一版的发布说明。
- 在 project files 里记下 tag、提交、云端判定（`local_validate` 的 `REPORT.md`）和之后 Mac 复核 VERDICT 的路径。
