# Exploratory diagnostics (NOT formal performance evidence)

Host: Apple M4 mac mini, macOS 26.3, native arm64. Toolchain p2996-d77eff1, Release -O3.
Bench binaries: /Volumes/XOffsetCS/mac-validation/bench/{c14,r1m2}-normal (restore alpha-era adapters).
Runner: explore_bench.py (randomized order per round, sc_* `run` on M fixture), summarize.py (medians).
No CPU pinning, no quiet-host control, no pre-registration. Medians only, no intervals.

- explore_M_r21.jsonl: 21 rounds, 5 libraries + C14/R1M2 xoffset.
- explore_M_d1_r25.jsonl: + D1 (canonical guard ablated: canonical_bytes_at returns nullptr; src-d1) and r1m2ctl (same build script, unmodified).
- explore_M_d2_r25.jsonl: + D2 (guard load relaxed instead of acquire; src-d2).
Apple arm64 default codegen already uses LDAPR for acquire (227 sites), d3rcpc (-mcpu=apple-m4) identical.

NON-SHIPPABLE VARIANTS: every src-* directory here is diagnostic only. In particular src-g1 (G1 alone on R1+M2)
is unsafe for receive storage: for_receive makes the whole maximum read/write before transport writes, so G1's
pristine-page assumption fails there; the shippable form (W3 onward, incl. W7/W8) sets zero_from_ = maximum for
receive storage. D1/D2/X1/X2/X3/W1b/W1c are ablations or rejected guard forms.
