# Character benchmark validation, 2026-11-16

Baseline `2fcce3f64045951fd92e27642582133ea67329d1` is the merge of [PR #286](https://github.com/ximicpp/XOffsetDatastructure/pull/286). Both final campaigns measured candidate `9d1b56feb5017b12f251397da8639f669a042907`. Entries are nanoseconds per message or iteration. Each campaign alternated nine baseline and candidate process pairs. Each process reports medians of five rounds with 50,000 messages and 100,000 read/write iterations per round. Ratios use the fastest rival within the same candidate process.

| Platform | Metric | PR #286 XOffset | Audited original | Optimized XOffset | Fastest rival | Median ratio | Wins |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Linux x86-64 | Send + receive, reuse | 199.9 | 201.6 | 152.5 | zpp::bits 808.1 | 0.190 | 9/9 |
| Linux x86-64 | Send + receive, fresh | 520.8 | 515.1 | 447.9 | cista offset / cista raw 1902.6 | 0.235 | 9/9 |
| Linux x86-64 | Read/write | 717.6 | 699.0 | 138.7 | Protocol Buffers 154.6 | 0.889 | 8/9 |
| macOS arm64 | Send + receive, reuse | 71.1 | 71.9 | 59.7 | zpp::bits 194.6 | 0.308 | 9/9 |
| macOS arm64 | Send + receive, fresh | 198.3 | 198.4 | 176.7 | zpp::bits 667.8 | 0.266 | 9/9 |
| macOS arm64 | Read/write | 234.6 | 234.3 | 60.8 | Protocol Buffers 80.5 | 0.755 | 9/9 |

The gate requires a median time ratio at most 0.95 and at least eight wins in nine runs per metric. The initial, directly saved, and compacted zstd-3 images must each fit within 1.15 times the smallest rival. The score files include the complete size checks.

## Reproduction and logs

- [Linux score](linux-score.md) and [raw runs](linux-raw.tar.gz) include the Linux x86-64 kernel and CPU details, vCPU 2 affinity, exact image platform digest, compiler, package versions, dependency and binary hashes, and regression logs. The GCC 16.2.0 and Debian 13 filesystem comes from the digest pinned in PR #286. Both builds used `-std=c++26 -freflection -O3 -DNDEBUG`. The node runs the same image filesystem in an unprivileged chroot with user, mount, PID, and network namespaces.
- [Mac score](macos-score.md), [raw runs](macos-raw.tar.gz), and [regression logs](macos-correctness.tar.gz) cover the same candidate on Apple M4 Pro with native arm64 Clang 21 and the field-list backend. The archive records the native package versions and binary hashes. [Cross-platform logs](macos-cross-platform.tar.gz) cover the preceding commit's same Character schema exchanging mutable images between macOS and Windows. The [Windows container test](windows-containers.log) covers the final candidate, including in-capacity and growing self-appends.
- [Initial Linux score](linux-before-range-score.md) and [raw runs](linux-before-range-raw.tar.gz) show why the additional iteration was needed. Its read/write ratio was 1.044 with three wins. The final candidate retains the sorted item map and wire schema, appends path points as a range, and includes one complete edit scope per timed round. Temporary writes are observable before removal.

```sh
mkdir linux-check macos-check
tar -xzf linux-raw.tar.gz -C linux-check
tar -xzf macos-raw.tar.gz -C macos-check
python3 score.py linux-check/raw
python3 score.py macos-check/paired-refined
```
