# 阶段复核 2026-11-03-ckpt7-c4925fa 二分

失败指标（复核点 head÷anchor 合并几何均值）：formal:emit_after 1.0122

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 3 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | c9fbaea2e980 | anchor |  |
| 1 | 583d85de0f05 | reload zero watermark, adaptive D6 regrow step, member swap in the SLG demo installs (edae8c26; 583d85de adds its bench MANIFEST, the first point whose bench/slg passes the manifest check) |  |
| 2 | 5433b4ebb7b2 | reload zero tail: shrinking loads only, out-of-line reinit, D6 step reset keyed on the epoch (the range also holds adf72e50 1480b62b 0ce5897d) |  |
| 3 | 683b8b365b5b | free table: small bins keep their first holes inline; cheaper first hole and XVector element access (the range also holds b251226b; the last range holds 6b151ac0 prefetch_for_update and c4925fa7 bench A adapter) |  |
| 4 | c4925fa7e7eb | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 5433b4e reload zero tail: shrinking loads only, out-of-line reinit, D6 step reset keyed on the epoch (the range also holds adf72e50 1480b62b 0ce5897d) | | | | 等待编译这个点（延后编译） | |

**进行中**：回退在 (c9fbaea2e980 anchor, c4925fa7e7eb head] 内，等编译 5433b4ebb7b2。
