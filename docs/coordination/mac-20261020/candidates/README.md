# Mac candidates (2026-10-20)

Every `*.vs-r1m2.patch` is `diff -ruN` against the isolated R1+M2 candidate materialized by
`python3 -I -B tools/verify_mac_handoff_20261020.py --materialize-candidate <dir>` (main header
`155f0847…`), excluding `external/`. Apply to a fresh materialized copy. Root C14 is unchanged.

| Name | Contents (lineage) | Full validation (M4) | Screen (pre-registered, `../screens/`) |
| --- | --- | --- | --- |
| G1 | new-page zero skip (`zero_from_`) | normal 76/76, TSan 73/73, ASan platform-9 | G1-vs-R1M2-v1: promoted |
| W1 | fixed-address canonical window, relaxed load in branch | normal 80/80, TSan 77/77, ASan platform-9 | — |
| W2 | W1 + G1 + internal no-load resolver, eager reservation; proof `W2-PROOF.md` | normal 80/80, TSan 77/77, ASan platform-9 | — |
| W3 | W2 + receive storage keeps full zeroing | normal 80/80, TSan 77/77, ASan platform-9 | W2-on-G1-v1 (W3 vs G1): promoted |
| W3-T / OR / N / E / G | single mechanism on W3 | normal 80/80, TSan 77/77 (ASan for T, OR) | T vetoed (load); OR/N/E not promoted; G promoted |
| W4 | W3 + OR + N + E + T | normal 80/80, TSan 77/77, ASan platform-9 | — |
| W4G | W4 + G | normal 80/80, TSan 77/77, ASan platform-9 | — |
| W4G-MV_W5 | W4G + MV ("W5") | normal 80/80, TSan 77/77, ASan platform-9 | MV-on-W4G-v1: promoted; W5-on-R1M2-v1: vetoed (load +6%) |
| W4G-P / NZ / RL / PF | single mechanism on W4G | normal 80/80, TSan 77/77 | exploratory only (P, NZ slower; RL, PF neutral) |
| W6 | W3 + G + MV, Darwin window 32 GiB | normal 80/80, TSan 77/77, ASan platform-9 | W6-on-R1M2-v1: vetoed (load +1.4%) |
| DIAG-* | non-shipping diagnostic variants (D1 ablation, D2 relaxed, X1/X2/X3 guard forms, W1b/W1c) | — | exploratory only |

"ASan platform-9": the same nine allocation-failure-injection tests fail on the C14 baseline under Darwin ASan,
because the ASan runtime interposes `operator new` for libc++ dylib callers (see `../MAC_PLATFORM_VALIDATION_20261020.md`).
Validation `result.json`/`ctest.xml` files are under `../validation/`. `w6-source/` holds W6's changed files in full.
