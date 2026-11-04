# 阶段复核 2026-11-04-ckpt16c-d540148 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0094，slg:B 1.0016，slg:C 1.0020

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | 49ce3c68fa60 | anchor |  |
| 1 | 6ec08cc6ba8e | the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge |  |
| 2 | d5401486e5cc | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 6ec08cc the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge | | | | 等待编译这个点（延后编译） | |

**进行中**：回退在 (49ce3c68fa60 anchor, d5401486e5cc head] 内，等编译 6ec08cc6ba8e。
