# Phase C work ledger and M2 control bridge review

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `phase_c_source_review`.

## Decision and scope

**No blocking finding in the reviewed ledger or conditional M2 bridge.** The ledger's numbers and interpretation preserve the distinction between nested emit observations, the primary eight phases, post-primary replay and the complete child. The bridge binds the actual completed control rather than a proposed path. It remains a proposal requiring independent finite M2 registration, resource/identity checks and admission, not execution authorization.

Read the ledger Markdown/JSON and M2 bridge, and narrowly inspected the accounting source. Independently read the existing C14/R1 AUDIT and observer receipts, rechecked selected counts/samples/events and the referenced control identities. No native, compiler, synthetic process, materializer, performance window or new workload was run. Producer/source audits are not repeated. Only this review document is written.

## Stable reviewed revisions

Initial and final reads agree:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `C_WORK_LEDGER.md` | 11820 | `eca3279c573055592b9f0949031377dca089129b9ac2b38c6913f86916946d64` |
| `C_WORK_LEDGER.json` | 37544 | `7d74c5048f5700104092750b1a85b9759fd517e3166746f4921309fc1d951143` |
| `M2_MECHANISM_EXACT_BRIDGE.json` | 37354 | `09d831163747bd2c09e29c7f2685fcbf5ca07e8f2fe4d8f216ec72a0fe466cd4` |

## Independent numerical checks

Recomputed from the existing audited emit samples:

| Observation | C14 | R1 |
| --- | ---: | ---: |
| sample0 to sample8 | 5.741193 ms | 5.769483 ms |
| sample3 to sample4 | 60.724 us / 2 minor faults | 49.043 us / 2 minor faults |
| sample5 to sample6 | 5.672399 ms / 4352 minor faults | 5.711476 ms / 4352 minor faults |
| sample5 to sample6 user+system | 5648 us | 5784 us |
| whole launch-to-wait4 wall | 2.328594917 s | 2.653630022 s |
| event t0 to event t8 | 87.135628 ms | 75.492739 ms |
| original input-read event interval | 887.414204 ms | 1101.559283 ms |

All agree with the ledger. The complete adjacent-boundary partition and selected disjoint summary both sum to the respective whole-child wall. The original internal native t0-to-t8 total is separately retained (87.134121/75.491460 ms); its slightly different boundaries are not silently substituted for event timestamps.

The CPU/fault residual arithmetic is also correct: C14 CPU 1091909−86734−648610=356565 us and minor faults 54923−16726−10678=27519; R1 CPU 1161907−76429−693341=392137 us and minor faults 54933−16726−10678=27529. These remain an unsplit remainder, not attributed to input reads, audit, writes or any individual wall interval without corresponding resource samples. Whole-child wait4 CPU excludes the Python observer's CPU. The native/replay boundaries and small clock/rusage sampling skew are disclosed; CPU values exceeding an adjacent small wall interval are not converted to utilization.

## Interpretation checks

- sample0-to8 is inside the original emit, which is inside primary and whole child. Adjacent samples partition that nested emit observation; these totals are not added together. The 4352 minor faults observed during payload copy already occur inside its 5.672399/5.711476 ms; no second fault-time estimate is added. A minor fault is not individually attributed to an output address without further evidence.
- The output remains one independent 17830872-byte result, including the 88-byte header and 17830784-byte payload. The first owning output is observed in original run order. The string-to-callback interval is not mislabeled as pure malloc, and normalization/root/allocator work is not presented as a measured removable fivefold opportunity.
- Primary CPU/fault data are only aggregate and the emit subrange samples; the ledger does not invent per-phase resource shares. The t4-to-t5 retirement observation includes post-t4 diagnostic flush. Added replay is separately bracketed and contains its own input/load/initial truth check, nine mutations/exports/hashes and two query calls, with zero owning emits and disabled primary observation. Complete-child peak RSS includes that work and observer/audit allocations and is not an uninstrumented benchmark peak.
- Independently recomputed read_sum inactive=2439240, primary canonical=12081262, mutate canonical=8421400 and mutate owns/vectors/nontrivial/nonempty/data=320465/154575/28652/16131/228010 in both roles. For the frozen source model K=228010−154575−2*28652=16131, predicted removed data calls=73435 and canonical calls=48393. The canonical count fraction is 0.5746431709691975%, matching the added opportunity-scale note.
- That 0.575% is a control-side source/count model, not a candidate observation, elapsed fraction, upper bound on time benefit or reason to reject the predeclared M2 test. Local 4-to-1 parsing does not mean the full modification path becomes 75% faster. R1 and M2 savings can overlap and are not summed into a speed forecast.
- Neither the single C14/R1 diagnostic pair nor its raw phase differences are claimed as comparative performance, a fivefold output result, equivalence, noise qualification, historical M1 timeout root cause or absence of long tails. Input wall intervals alone do not distinguish disk wait, scheduling and memory work. This is the appropriate stopping point for the finite diagnosis; no extra run is requested by the ledger.

## Actual M2 control bridge

Independently verified the original conditional request, successful C result, actual C freeze, actual M2 final source identity and retained materialization identity. The existing C14 AUDIT, observer, compile receipt and binary all match their recorded byte counts/hashes, including binary `339f36b62dc4c5b439349dbbd57b8346da8562d8527b957a116d48211df0f4e0`.

Compared the candidate proposal directly with the actual C freeze's C14 role. Both compile argv arrays contain 31 arguments and native argv arrays contain 14. Changes are confined to the declared C14-to-M2 final source/include prefix and fresh candidate binary/dependency/body paths; the sole environment change is TMPDIR. Compiler flags, original wire/trace paths, experiment include roots and public run operation remain unchanged.

The bridge correctly preserves zero new control compile/run, one candidate compile (120 s), one candidate native (20 s), outer 600 s, no warmup/retry and the same source/observer/audit/runtime/input conditions. Current freshness, resources, shared lease, actual candidate dependencies/binary and successful full audit still require validation-side registration and admission. This review does not assert a future candidate result or allow substitution of an incomplete control.
