# 阶段复核 2026-11-03-ckpt10-3e02607 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0276，slg:C 1.0002

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | cc5b28eed718 | anchor |  |
| 1 | 3278287dd46a | delta sync merged with next_cpp26 d9db04d (library = head without 4889cdd4: free table, a change marks the generation stale); the last range holds 4889cdd4 and the merge of cc5b28ee (bench only) |  |
| 2 | 3e026071a28e | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 3278287 delta sync merged with next_cpp26 d9db04d (library = head without 4889cdd4: free table, a change marks the generation stale); the last range holds 4889cdd4 and the merge of cc5b28ee (bench only) | (cc5b28e, 3e02607] | -0.47 | slg:A -0.44，slg:C -500.37 | 回退在此点之后 | bisect-1 |
|  |  |  |  | **不单调**：slg:A 此点之后，slg:C 此点之后 |  |  |

**结论：不单调，不给单一来源。** 各失败指标在第一个不单调的步里各自的方向：

- slg:A：m/r -0.44，来源在 (3278287dd46a delta sync merged with next_cpp26 d9db04d (library = head without 4889cdd4: free table, a change marks the generation stale); the last range holds 4889cdd4 and the merge of cc5b28ee (bench only), 3e026071a28e head]
- slg:C：m/r -500.37，来源在 (3278287dd46a delta sync merged with next_cpp26 d9db04d (library = head without 4889cdd4: free table, a change marks the generation stale); the last range holds 4889cdd4 and the merge of cc5b28ee (bench only), 3e026071a28e head]

按 f 走到的点是 3e026071a28e（head），只作参考；交「性能优化」决定。
