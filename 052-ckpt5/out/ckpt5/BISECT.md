# 阶段复核 2026-10-28-ckpt5-70a07a0 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:B 1.0041，slg:C 1.0138

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 3 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | 9ef5ce75d72a | anchor |  |
| 1 | af601ea0372e | CKPT5 head (M2 gate pass 10-28 07:53Z): everything from v3.1.0 to CKPT5 |  |
| 2 | 32ab499d177c | #167 S MSVC noinline macro (the range also holds #160 #161 #165 #164 #166 #163, class 0) |  |
| 3 | 3a06f38e2a65 | #162 S owner_map_table in explicit __DATA,__bss (effective on Apple) (the range also holds #168 #170, class 0) |  |
| 4 | ed0450094ad5 | #169 S no-exceptions receive string check (exceptions build expected unchanged) |  |
| 5 | ffef6f32f134 | #172 S comments and evolution path markers only (the range also holds #173 #174 #175 #171, class 0) |  |
| 6 | 1108193d372e | #177 S portable schema: plain char keys / enum rejected at compile time (the range also holds #176, class 0; the last range is #179, docs) |  |
| 7 | 70a07a059cd8 | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 3a06f38 #162 S owner_map_table in explicit __DATA,__bss (effective on Apple) (the range also holds #168 #170, class 0) | (9ef5ce7, 70a07a0] | 1.00 | slg:B 1.00，slg:C 1.00 | 回退在此点及之前 | 不计时（二进制与 anchor 或 head 相同） |
| 1 | af601ea CKPT5 head (M2 gate pass 10-28 07:53Z): everything from v3.1.0 to CKPT5 | (9ef5ce7, 3a06f38] | 0.24 | slg:B -0.27，slg:C 0.28 | 回退在此点之后 | bisect-1 |
|  |  |  |  | 可能分散在多个合并里：slg:C |  |  |
| 2 | 32ab499 #167 S MSVC noinline macro (the range also holds #160 #161 #165 #164 #166 #163, class 0) | (af601ea, 3a06f38] | 0.23 | slg:B 0.39，slg:C 0.21 | 回退在此点之后 | bisect-2 |
|  |  |  |  | 可能分散在多个合并里：slg:B |  |  |

**结论：回退来自 3a06f38e2a65（#162 S owner_map_table in explicit __DATA,__bss (effective on Apple) (the range also holds #168 #170, class 0)）**。
