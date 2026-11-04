# 阶段复核 2026-11-04-ckpt14-6ec08cc 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0188，slg:B 1.0038

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | eabb583b033d | anchor |  |
| 1 | 7157488f5ea5 | #191 merged with next_cpp26 c2ec7ee3 (before #207 #208 #209 #210, report only); the last range holds the reconciliation 6ec08cc6 |  |
| 2 | 6ec08cc6ba8e | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 7157488 #191 merged with next_cpp26 c2ec7ee3 (before #207 #208 #209 #210, report only); the last range holds the reconciliation 6ec08cc6 | (eabb583, 6ec08cc] | 0.58 | slg:A 0.69，slg:B -1.98 | 回退在此点及之前 | bisect-1 |
|  |  |  |  | 可能分散在多个合并里：slg:A |  |  |
|  |  |  |  | **不单调**：slg:A 此点及之前，slg:B 此点之后 |  |  |

**结论：不单调，不给单一来源。** 各失败指标在第一个不单调的步里各自的方向：

- slg:A：m/r 0.69，来源在 (eabb583b033d anchor, 7157488f5ea5 #191 merged with next_cpp26 c2ec7ee3 (before #207 #208 #209 #210, report only); the last range holds the reconciliation 6ec08cc6]
- slg:B：m/r -1.98，来源在 (7157488f5ea5 #191 merged with next_cpp26 c2ec7ee3 (before #207 #208 #209 #210, report only); the last range holds the reconciliation 6ec08cc6, 6ec08cc6ba8e head]

按 f 走到的点是 7157488f5ea5（#191 merged with next_cpp26 c2ec7ee3 (before #207 #208 #209 #210, report only); the last range holds the reconciliation 6ec08cc6），只作参考；交「性能优化」决定。
