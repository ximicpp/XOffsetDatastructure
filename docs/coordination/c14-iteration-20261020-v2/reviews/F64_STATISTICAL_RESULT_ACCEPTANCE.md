# Final F64 statistical result review

**Formal performance target NOT achieved: only 3/12 primary conditions pass.** Statistical reconstruction and the complete original offline integrity/semantic audit pass; those outcomes do not constitute performance-target acceptance.

The independent standard-library reconstruction read **9264 native results and 9264 receipts**, matched the exact frozen schedule/primary samples and verified successful fixed-role argv/environment, eight-phase integer totals and child bounds. Inference retains **4632 primary rows minus the predetermined 24 warmups = 4608**, grouped into **384 blocks / 64 six-block macrocycles, df63**. Conditioning runs establish execution coverage and never enter inference. There was no deletion, epsilon, resampling, new window or project-module import.

**30/30 A/A tests pass**, all 82 inferences are defined, and all twelve competitive primary and forty auxiliary results were independently recomputed. Original separate simultaneous 95% families remain: AA t(1−.05/60,63); primary one-sided t(1−.05/12,63); auxiliary two-sided t(1−.05/80,63). Original 1e-12 log roundoff, zero positions, macrocycle units, fixed thresholds, no filtering/retries and old-control cost veto remain unchanged. No combined overall or conditional-on-AA confidence claim is added.

## Competitive primary results

Each ratio is **combined candidate ÷ the same-window fixed competitor**. Thresholds are 1.05 for read_sum/mutate and 0.20 for independent owning emit. Canonical numbers are retained; full precision and independent numeric crosschecks are in JSON.

| Competitor | Metric | Point | Joint one-sided upper | Threshold | Result |
|---|---|---:|---:|---:|---|
| cista | read_sum | 1.177668 | 1.184597 | 1.05 | not met |
| cista | mutate | 1.823647 | 1.835824 | 1.05 | not met |
| cista | emit_after | 0.467715 | 0.471482 | 0.20 | not met |
| flatbuffers | read_sum | 1.036374 | 1.042315 | 1.05 | pass |
| flatbuffers | mutate | 1.589154 | 1.603829 | 1.05 | not met |
| flatbuffers | emit_after | 0.416119 | 0.419917 | 0.20 | not met |
| capnproto | read_sum | 0.403022 | 0.405309 | 1.05 | pass |
| capnproto | mutate | 0.557131 | 0.560808 | 1.05 | pass |
| capnproto | emit_after | 0.522677 | 0.526898 | 0.20 | not met |
| cereal | read_sum | 1.137245 | 1.144953 | 1.05 | not met |
| cereal | mutate | 1.689372 | 1.704362 | 1.05 | not met |
| cereal | emit_after | 0.459728 | 0.463736 | 0.20 | not met |

All nine rejected conditions have point estimates already above their limits; none is an undefined metric or a point-pass/upper-bound-only failure. All four independent-output conditions miss 0.20. Passing C14 self-comparisons cannot replace these competitor conditions, and independent R1/M2 gains are not added or attributed from this six-role experiment.

## Reload and auxiliary outcomes

The frozen XOffset detached owning reload/full-validation contract and original timing boundaries are retained. Reload has no newly added hard competitor threshold; the following are its original forty-family diagnostic intervals.

| Comparison | Point | Joint diagnostic interval | Direction |
|---|---:|---:|---|
| new_xoffset/old_xoffset | 0.976073 | [0.969270, 0.982924] | confirmed_decrease |
| new_xoffset/cista | 1.909181 | [1.885631, 1.933024] | confirmed_increase |
| new_xoffset/flatbuffers | 4.577373 | [4.529286, 4.625971] | confirmed_increase |
| new_xoffset/capnproto | 0.888923 | [0.880238, 0.897694] | confirmed_decrease |
| new_xoffset/cereal | 3.473098 | [3.433507, 3.513144] | confirmed_increase |

The forty auxiliaries contain **21 confirmed decreases, 16 confirmed increases and 3 unknowns**. None of the original five C14 cost-transfer metrics triggers the lower>1 veto. That does not prove no regression: the three unknown C14 comparisons are emit_after **[0.986164, 1.001378]**, retire_modified_state **[0.984216, 1.004104]**, cleanup **[0.991632, 1.013501]**. All forty results, including increases and uncertainty, remain in JSON.

## Reproducibility and complete audit

With the original frozen critical values, all **82 block/macrocycle vectors, moments, point and interval fields match canonical exactly (0 ULP)**. A separate 90-digit Decimal Student-tail integral/bisection crosscheck differs in critical values by 41/32/50 ULP for AA/primary/auxiliary, at most 2.22e-14 absolute. It changes ratio endpoints by at most **1/1/6 ULP**, respectively (maximum auxiliary absolute 1.71e-13), and changes no verdict. It neither replaces canonical numbers nor widens any test.

The complete original audit is **2425003 B**, SHA `f206dfc4a33d659e1310ca42a9ffd5d82e2a445526e99ae0019aa5f44963efbf`: passed, complete, 9264/4632/4632 actual/primary/conditioning children, no budget violation, target_achieved=false and promotion_eligible=false. Its entire analysis and top primary/auxiliary objects equal the sealed canonical analysis. The latter remains **1540706 B / 118b688b…ec050fe** throughout the reconstruction and final index check.

Original timing completed in **4620.436771 s ≤5400**, exit0/empty cleanup. The separate original W3 functional3600 audit completed in **1169.531633 s**, exit0/empty cleanup; its six queue-manifest members and exact command/composite postrun pin match. Final small index **83984 B / 29df100c…0458b8** has all 82 flattened rows exactly equal to the original audited analysis, with the same nine rejections. Frozen manifest is `5c5a81530b808ff5420bfed36b00b838ef6c147987542012acb4057f9af132d8`; exact execution admission is **0e24cad5…114a4e**.

Independent raw native/receipt ordered chain: `423c988ebdecb1ef27c3ce78ff93ee4bc1c53c75406776438803524232db0264`. The JSON specifies its length-prefixed construction and all small metadata pins. No wire/canonical/query/binary/runtime body or large run manifest was rehashed by this reviewer; no benchmark, compile, lease or new sample was executed. Actual external identity/semantic closure is supplied by the exact original freeze/runtime/offline audit, not by changing the mathematical record's external-identity flag.

**Accept the numeric reconstruction and preserve the sole complete window. Do not accept formal target completion or candidate promotion on this result; no retry, sample extension, changed threshold or optional-interface substitution is authorized.**
