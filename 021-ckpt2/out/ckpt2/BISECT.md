# 阶段复核 2026-10-27-ckpt2-b50fc8a 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:C 1.0001

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 3 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | f8eebf46565c | anchor |  |
| 1 | 55c06b5121d8 | #91 S POD assign fast path |  |
| 2 | 33b198268257 | #95 S expand tail precheck (+ #92 #94, class 0) |  |
| 3 | e46cf38190e1 | #104 S shift rebase inline (+ #96 #97 #98 #100 #101 #102 #105, class 0) |  |
| 4 | 12866015d8d8 | #114 S Z2 indeterminate validation context |  |
| 5 | b50fc8abbbe0 | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 33b1982 #95 S expand tail precheck (+ #92 #94, class 0) | (f8eebf4, b50fc8a] | 92.57 | slg:C 92.57 | 回退在此点及之前 | bisect-1 |
|  |  |  |  | **不单调**：slg:C 此点及之前 |  |  |
| 2 | 55c06b5 #91 S POD assign fast path | (f8eebf4, 33b1982] | 68.04 | slg:C 68.04 | 回退在此点及之前 | bisect-2 |
|  |  |  |  | **不单调**：slg:C 此点及之前 |  |  |

**结论：不单调，不给单一来源。** 各失败指标在第一个不单调的步里各自的方向：

- slg:C：m/r 92.57，来源在 (f8eebf46565c anchor, 33b198268257 #95 S expand tail precheck (+ #92 #94, class 0)]

按 f 走到的点是 55c06b5121d8（#91 S POD assign fast path），只作参考；交「性能优化」决定。
