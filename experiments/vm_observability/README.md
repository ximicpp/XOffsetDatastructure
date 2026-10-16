# W2/N3 diagnostic interface

Read `contract.json`, `coverage.json` and `event_kinds.json` before interpreting records. The contract is accepted externally; its proposed status is preserved to keep the accepted SHA unchanged. `handoff.json` identifies the selected immutable source/build/output.

The generated header includes `n3_runtime.hpp`; compile only diagnostic consumers against that include directory. Original F timing consumers must keep the original include path and no recorder/probes.

```cpp
// Fresh process; every observed resource begins within the recorder lifetime.
#include "xoffsetdatastructure.hpp" // generated diagnostic copy
#include <fstream>
int main() {
    {
        n3::Phase phase("work"); // UTF-8 label at most 63 bytes
        // Construct observed owners/pools, run and release here.
        // Join all emitting threads before leaving to dump.
    }
    std::ofstream output("new-trace.json");
    n3::dump(output); // quiescent only; can allocate while serializing
}
```

Use recorded Phases for warm-up and work; no reset/prefix truncation. If a preregistered warm pool or owner remains alive at dump, W3 must check the exact allowed active owners/request domains/counts/bounds. Preexisting unrecorded resources are unsupported. `n3::Scope(base)` supplies causal attribution for shared calls; direct event semantics still depend on kind. Explicit unowned operations require an inner `Scope(nullptr)`.

`audit_events.validate_trace(trace, event_kinds_path=FROZEN_CATALOG)` validates structure, known kinds, allocation lifetimes and registry generations. It does not prove arbitrary workload semantics or certify timing. The fixed `n3_probe --case CASE_ID` collection has its own exact case assertions. Retained shared buckets are reported rather than treated as a leak. Do not sum pointer/domain/epoch/offset values or sampled capacities as costs.

Build and collection must run as the direct command of W4 `run_job.py` using its default lock. `observe.py prepare` and offline `audit` are lightweight. See the accepted CLI in `contract.json`; all output directories must be new. A collection carries `tool/observe.py` and its imports/catalog for portable read-only offline audit.
