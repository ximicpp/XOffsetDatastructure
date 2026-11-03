# 阶段复核 2026-11-03-ckpt11-827943c 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0294，slg:B 1.0090，slg:C 1.0098

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | b6e067881028 | anchor |  |
| 1 | 0bbbaa079f4f | the pre-fix head: CKPT10's library (3e026071) merged with next_cpp26 b6e06788 (#204); the last range holds 827943cf (free table field order, no redundant bump_gen) |  |
| 2 | 827943cfb972 | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 0bbbaa0 the pre-fix head: CKPT10's library (3e026071) merged with next_cpp26 b6e06788 (#204); the last range holds 827943cf (free table field order, no redundant bump_gen) | | | | 等待编译这个点（延后编译） | |

**进行中**：回退在 (b6e067881028 anchor, 827943cfb972 head] 内，等编译 0bbbaa079f4f。
