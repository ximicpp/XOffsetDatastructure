# Q32 statistical result acceptance

**Passed: all 30/30 predeclared within-role A/A intervals establish equivalence.** No unavailable metric, missing inference row, outlier deletion or extra sample was found. The original complete offline audit also passed.

The independent reconstruction read all **4,656 raw native results and 4,656 receipts**, matched the complete frozen native schedule and exact primary samples, and retained 2,328 primary results including the fixed 24 warmups. Inference uses the remaining **2,304 primary children → 192 blocks → 32 six-block macrocycles**, with **df31**. The conditioning results establish execution coverage and do not enter the A/A inference. All eight integer phases sum to each native total; read_sum is formed by integer addition before ratios. Receipts have the fixed role/argv/environment, 20-second ceiling, successful exit and no timeout.

Each ratio is chronological first/second **within the same role and block**. The following range only summarizes the smallest lower and largest upper endpoint of that role's five individually tested intervals; it is not another confidence interval or cross-library comparison.

| Role | A/A passed | Envelope of five canonical intervals |
|---|---:|---:|
| old_xoffset | 5/5 | 0.983676–1.014157 |
| new_xoffset | 5/5 | 0.984336–1.013716 |
| cista | 5/5 | 0.982737–1.028049 |
| flatbuffers | 5/5 | 0.972875–1.043336 |
| capnproto | 5/5 | 0.980958–1.014986 |
| cereal | 5/5 | 0.977127–1.014557 |

The retained rule is the simultaneous two-sided 95% family for 30 tests / 60 tails: t(1−.05/60,31), factor 1.05, original log roundoff 1e-12. Six log ratios are averaged per macrocycle; children and blocks are not treated as independent observations. Zero positions would remain undefined and prevent qualification; no measured AA zero occurred. No epsilon, filtering, retry, optional stopping or changed rule was introduced.

An independent **90-digit Decimal** odd-df Student-tail integral and bisection gives t=**3.4435072406790646**, versus original canonical **3.4435072406790512** (30 ULP; absolute 1.33e-14). Independent Windows CPython 3.11.7 raw reconstruction gives exactly the same ratio point estimates, and ratio interval endpoints differ at most **1 ULP / 2.22e-16**. All decisions agree. The largest near-zero log-mean discrepancy is 1,024 ULP for cereal/read_sum, only **5.42e-20** absolute; the JSON reports field-specific absolute/ULP ranges and both independent/canonical values. Canonical numbers were preserved, not replaced or used to relax the tests.

Original offline audit **c704b28b…785814** is passed, complete, measurement_qualified=true, 4,656/2,328/2,328 successful actual/primary/conditioning children and no budget violations. Its entire analysis object equals the sealed canonical analysis **a3567add…fba5c3** exactly. Its separate W3 functional3600 queue completed in **581.537568 s**, exit 0, empty cleanup; all six small queue-manifest members match. Original timing completed in **2,308.301671 s**, exit 0, empty cleanup. Frozen manifest is **b8e2eaa5…1aa3f23** and external admission **a20b7e1b…2a8d2b4**.

The independent native/receipt ordered chain is `6f871d512f4dbfa9df110fe86c931b68cdcc5fe30f387c10418bc547007558c6`; its unambiguous length-prefixed construction and small metadata pins are in the JSON. No wire/canonical/query/binary body was hashed by this reviewer, and no project executor, benchmark, compiler, queue or new sample was run.

**This establishes Q32 measurement qualification only.** No candidate-versus-competitor contrast or formal performance conclusion was computed. F64 and the performance target remain unaccepted here.
