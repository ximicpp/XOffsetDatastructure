# R6 archive dependency repair v2 — source preparation

This independent derivative preserves package-execution-v1, package-v1, their
receipts, successful v3 package/runtime gates and the failed archive-v1 evidence.
The archive-v1 default full normal tests passed before the audit incorrectly
required all eleven own headers in those tests' union of includes. Two opt-in
scratch headers are not default full-test dependencies. This is an audit scope
repair; it changes no production header, CMake install list, payload, ABI or test.

The author has performed source reads/writes/copies and small file hashes only.
No imports, Python tests, parser execution, apply, configure, compiler, native
program, queue, measurement or release action were performed by the author.

## Root's first minimal check, before a new archive build

The following entries use only stdlib and this local parser. Root may run the
synthetic checks and the parser over the existing v1 actual dependency report:

`/usr/bin/python3 -B -m unittest discover -s H/package-archive-repair-v2 -p test_deps_audit.py`

`/usr/bin/python3 -B H/package-archive-repair-v2/deps_audit.py --deps B/package-clean-archive-v1/normal-deps.stdout --source B/package-clean-archive-v1/zip-extracted/xoffsetdatastructure-0.1.0-alpha.1 --own-headers-json B/final-source-identity-v3.json --out B/package-archive-default-deps-parser-v2.json`

These examples intentionally reference historical v3 metadata only for the
already-created v1 report. They prove neither current v4 gates nor header probes.
The parser rejects empty, stale, malformed, duplicate or count-inconsistent
Ninja records and missing required default headers. It records the observed own
headers. It does not manufacture usage evidence for the two absent headers.

## Final source spec and dispatch

After the explicit final source's necessary new full normal/SAN gates pass,
root supplies all five paths; no newest-directory search or source fallback:

`/usr/bin/python3 -B H/package-archive-repair-v2/prepare-spec.py --source ABS_FINAL_SOURCE --materialization ABS_MATERIALIZATION --candidate ABS_CANDIDATE --normal-gate ABS_NORMAL_INVENTORY_ACCEPTANCE --sanitized-gate ABS_SANITIZED_INVENTORY_ACCEPTANCE`

The source must be B/final-vN/source. The spec defaults to
C/package-archive-repair-spec-v2.json and is write-once. Its fourteen production
file identities, eleven own headers, actual complete full names/count (at least
the old 71, with all new registered cases retained), normal/SAN agreement and
separate configure3 come from the supplied source-specific gate records. This
allows the known v4 regression test without relabeling v3 results as v4 passes.
Both maintained package-tool tuples and the CMake install list must exactly
match those eleven declared headers. Source packaging tools, metadata,
execution closure, fourteen runtime fixture files and shared helpers are pinned.

Root dispatches OUTSIDE any active lease:

`/usr/bin/python3 -B H/package-archive-repair-v2/dispatch.py --stage archive`

The original common.queue gives archive-worker one W1 validation lease, outer
3600 seconds, B/queues/package-clean-archive-v2, output
B/package-clean-archive-v2. All worker commands retain bounded original child
timeouts and stop at the first failure. No retry or automatic cleanup of prior
outputs exists. The archive still runs maintained source tar create/audit/
extract, exact equivalent ZIP validation, clean normal/SAN full tests with the
spec's exact inventory, separate configure3 and clean C consumer. It retains
the maintained dependency lock/851 audit and exact metadata/version policies.

The only new compile work is eleven OBJECT TUs per profile, each containing
exactly one first #include of its declared public own header. No object is
linked or executed. They use the clean decoded ZIP source, fixed compiler,
Release O3 with assertions, C++26 reflection, libc++ and exact compatible SAN
resource. Every compile row, input bytes, output object, requested header in
that object's VALID Ninja section and source exclusion is checked. The default
full report requires its real nine headers; the independent objects prove all
eleven compile, and neither report claims the other's scope.

The unchanged standard package/runtime workers are also available under fresh
labels package-standard-v2/runtime-v2 (2400 seconds each), via --stage package,
runtime or all. They permit root's necessary consumer revalidation after the
production change; no v3 result is reused as a current-source pass. Archive
repair alone does not require rerunning already-valid v3 consumers. The
dispatcher writes C/package-repair-STAGE-acceptance-v2.json only on real success.

The result adds public_first_include_objects.normal/sanitized, each with 11
object records and native_executions: 0; full.normal/sanitized records the
exact current test count/JUnit plus default dependency scope separately. The
existing clean C consumer remains a real native gate. No formal performance
sample or claim is made.
