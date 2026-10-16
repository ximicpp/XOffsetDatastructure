# Stage 1 runner completion protocol correction

Date: 2026-10-15. This note supplements the immutable
[stage 1 evidence](perf/2026-10-15-vm-stage1/README.md). It does not change any
archived source, sample, summary, interpretation, log or hash.

Review found a recovery flaw in the schema 1 runner: it published compressed raw
samples and finished-window metadata before the final runtime identity check.
If that check failed and the runtime was subsequently restored, a later invocation
could accept the failed window and continue. The existence of complete-looking
files was therefore insufficient proof that the final check had succeeded.

The maintained [runner](../tools/benchmark_vm_stage1.py) now creates schema 2
experiments. A window starts with `status: running` and
`runtime_identity_validated: false`. Its final runtime check occurs before
publishing the compressed samples and summary. A failure records `status: failed`
and does not publish a finished timestamp or accepted sample archive. Successful
completion publishes `status: completed` and `runtime_identity_validated: true`
last, after the samples and summary have been written.

Reading, resuming and auditing inspect every window directory. They reject failed,
unfinished, missing-status or missing-archive windows; restoring runtime files
does not rehabilitate a failed run. Preserve such a directory for diagnosis and
prepare a new experiment. The final console message now says "measurement
window"; it does not certify that the host was quiet.

Schema 1 archives are deliberately not upgraded or silently accepted by the new
runner. Audit them with the exact runner inside their own source archive:

```text
tar -xzf docs/perf/2026-10-15-vm-stage1/source_snapshots.tar.gz -C <scratch>
python3 <scratch>/snapshots/benchmark_vm_stage1.py audit \
  --output docs/perf/2026-10-15-vm-stage1
```

This supersedes the archived README's command that names the current maintained
runner for auditing old evidence. New measurements use the current runner's
`prepare` command and require schema 2 completion records. There is no continuation
of old schema 1 timing runs through the new runner.

The three archived windows completed normally in the original session and passed
their frozen auditor again after this correction; all 15 files in their existing
`SHA256SUMS` still match. They retain the independently documented
`background-contention/diagnostic-only` limitation. New completion markers are not
retroactively asserted for them. No timing was repeated and no performance
conclusion changed.

The [runner tests](../tools/test_benchmark_vm_stage1.py) now include a simulated
final identity-check failure, restoration followed by rejected continuation,
rejected auditing/reading of the failed window, unfinished-window rejection,
schema 1 rejection by the maintained auditor, and successful publication only
after the final check. These tests mock benchmark execution and run no real
timing workload.
