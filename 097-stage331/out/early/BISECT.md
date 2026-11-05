# 阶段复核 2026-11-05-stage331-ee506d3 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0161

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | 51a4815b7802 | anchor |  |
| 1 | 84243ba56281 | #223 merged (root cache refill, class L; known item from CKPT20; deferred); the last range holds #233 (bench only), #221 and #238 (cloud P2996: same bench binaries as their parent) and #226 |  |
| 2 | ee506d3f54a9 | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 84243ba #223 merged (root cache refill, class L; known item from CKPT20; deferred); the last range holds #233 (bench only), #221 and #238 (cloud P2996: same bench binaries as their parent) and #226 | | | | 等待编译这个点（延后编译） | |

**进行中**：回退在 (51a4815b7802 anchor, ee506d3f54a9 head] 内，等编译 84243ba56281。
