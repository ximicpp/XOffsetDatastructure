# 阶段复核 2026-11-03-ckpt8-5433b4e 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:C 1.0048

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | c9fbaea2e980 | anchor |  |
| 1 | 583d85de0f05 | reload zero watermark, adaptive D6 regrow step, member swap (edae8c26 + its bench MANIFEST); the last range holds adf72e50 1480b62b 0ce5897d 5433b4eb (reload zero tail) |  |
| 2 | 5433b4ebb7b2 | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 583d85d reload zero watermark, adaptive D6 regrow step, member swap (edae8c26 + its bench MANIFEST); the last range holds adf72e50 1480b62b 0ce5897d 5433b4eb (reload zero tail) | | | | 等待编译这个点（延后编译） | |

**进行中**：回退在 (c9fbaea2e980 anchor, 5433b4ebb7b2 head] 内，等编译 583d85de0f05。
