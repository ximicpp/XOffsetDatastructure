# M1 business/V1/consumer tool reuse review

Status: source-ready, not a test result. Author executed only source derivation,
AST parsing and the concrete `dispatch.py check` (30 Python / 62 bound files).
Compiler, native, queue, performance executions by this author: zero.

## Concrete entry

Use WSL `Ubuntu-22.04`. Set `H` to the following existing directory:

```text
/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261019/validation/business
```

From outside every lease, the sole heavy executor runs these sequentially:

```sh
/usr/bin/python3 -B "$H/dispatch.py" check
/usr/bin/python3 -B "$H/dispatch.py" prepare
/usr/bin/python3 -B "$H/dispatch.py" bench
/usr/bin/python3 -B "$H/dispatch.py" functional
/usr/bin/python3 -B "$H/dispatch.py" package
/usr/bin/python3 -B "$H/dispatch.py" runtime
```

The entry invokes the existing original
`/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py`.
It has no alternate lock, timing mode, native warmup, Q/F action, or generic
command interface. Failed prefixes and request files are exclusive and preserved.
Do not rerun a failed stage; diagnose it before creating an explicitly revised
input/output identity. The functional coordinator remains outside a lease and
serially dispatches the original workers; no nested lock is attempted.

| Stage | Original lease output relative to validation root | Cap |
|---|---|---|
| prepare | `leases/m1-business-prepare-v2` | 300 s |
| bench | `m1-business-v2/queues/bench-profiles-v2` | 2400 s |
| functional | original normal/SAN functional bundle `/queue` and separate audit/compat queues | 1800 s each functional; 600 s each audit; 1500 s compat |
| package | `m1-business-v2/queues/package-standard-v2` | 2400 s |
| runtime | `m1-business-v2/queues/package-runtime-v2` | 2400 s |

## Exact current inputs and identity

- Candidate source stays `/root/xoffset-c14-iteration/20261019/v1/validation/m1-full-v2-normal/source`.
- New artifacts stay `/root/xoffset-c14-iteration/20261019/v1/validation/m1-business-v2`.
- New coordination records stay C14 `validation/business/`.
- `prepare` requires `M1_FULL_RESULT.json` passed, actual normal73 and SAN73
  receipts, actual full inventories/configure checks/compile databases, both
  source snapshots' complete154 members, `M1_CODEGEN_RESULT.json`14 candidate
  production identities, and current core `REQUESTS.json`/candidate `IDENTITY.json`.
  It uses actual passed r2 full receipts. The earlier staged-path preflight
  rejection ran zero compilers/native processes and is not treated as a source
  or platform result.
- `prepare` records every source and helper binding in `PREPARED.json`;
  business and package workers recheck them. Source label alone is insufficient.
- Candidate label is `xoffset-c14-m1-default-scalar-v2`. Content hash is SHA256
  of sorted, compact JSON of all14 production file IDs. New identity file is
  `validation/business/candidate-source-identity-v2.json`.

The full154 snapshot intentionally contains no benchmark/campaign subtree.
Therefore `prepare` copies the existing old `final-v4/source` benchmark and
campaign subtrees into `B/metadata`, verifying every copied byte first. Production,
test, CMake and old trees are untouched. Only two adapter literals (`library_id`,
`source_content_sha256`) and three protocol assignments (`IDENTITIES`,
`CANDIDATE_CONTENT_SHA256`, `CANDIDATE_CLOSURE_IDS`) change. Closure contains all14
production files plus the new candidate identity. `METADATA_DERIVATION.json`
records the complete before/after map and the two changed files.

Adapter commit remains `7539433ab96d69c4a07b1db5c0a13786da85cdcd` as inherited
legacy benchmark base metadata. It is neither M1's current Git base1904 nor a
claim M1 is committed. Inherited adapter `source_archive_sha256` is historical
metadata, not a new M1 archive assertion. Current provenance is the exact
request,154/14 closure, metadata IDs, compiler/runtime and actual dependencies.

## Reused logic and mandatory new work

| Gate | Action / actual inventory |
|---|---|
| Full correctness | Reference the now-passed M1 normal73/SAN73 receipts, not old C14 passes; no duplicate full rebuild here. |
| Changed driver | Build `sc_xoffset`, `sc_xoffset_contract`, `sc_xoffset_resource` in fresh normal/SAN; run exactly2 related CTests each. Preserve actual `-O3 -DNDEBUG`, reflection/expansion/gnu++26 and matched sanitizer resource. |
| Four competitors | Reuse sealed V5 binary/profile/runtime/SDK/generated-code/dependency identities and byte-check them. Never rebuild or relabel them as M1. |
| S/M/L business | Original functional80 children across15 cells in normal and SAN, fresh encoding, detached inspection, nine mutation stages, complete canonical/query/native/wire/receipt truth, followed by maintained full offline audit and composite postrun pin. These executions are correctness, not samples. |
| Main V1 | Original six historical controls72 + separately sealed directV8 twelve =84 checks, S/M/L initial/modified both directions. Current writer inputs are fresh M1 normal functional outputs. |
| Extras | alpha-S2 (plus its describe action), resize2, string2, new-API2; independent AB2/four CLI actions and C2/four CLI actions. Compile only required historical fixture CLIs against pinned controls; candidate CLIs come from actual full CTest metadata where available. |
| Install/subproject | Existing package worker executes storage3+3/full3+3 consumer tests, version-policy rejections,851 locked dependency headers, relocation/source hiding, exact installed headers, prefix tar audit/extract/move and three separate receive consumers. |
| Shared runtime | Existing runtime worker builds both optional DSOs and executes unchanged six consumers in installed and subproject modes, plus receive consumer in both; checks Ninja actual dependencies, hidden original source/prefix, readelf/ldd and exact14 production identities. |
| Lifecycle/OOM/alias | Actual M1 full73 inventory and dedicated M1 normal4/SAN4 retain these gates. AB/C CLI interoperability does not replace them. |

The new full source snapshots already executed full tests without Git metadata
and with byte-exact fixtures/CMake/locked dependencies. No source-tar/ZIP clean
archive full73+73 is launched again by this worker. This entry claims package
prefix tar audit/extract/move and its consumer checks only; any separately
required new source-tar/ZIP receipt remains an explicit distinct artifact and
must not be inferred from these results. Windows remains separate from WSL.

## Outputs for screening freeze

After actual bench success:

```text
B/bench-normal-v2/bin/sc_xoffset
B/bench-sanitized-v2/bin/sc_xoffset
B/profile-normal-v2.json
B/profile-sanitized-v2.json
B/compiled-source-binding-normal-v2.json
B/compiled-source-binding-sanitized-v2.json
B/metadata/experiments/serialization_comparison/adapters/xoffset/adapter.hpp
B/metadata/tools/serialization_comparison/protocol.py
B/functional-{normal,sanitized}-v2/{manifest,status,runtime-before,runtime-after}.json
B/final-business-functional-v2/result.json
B/package-standard-v2/result.json
B/package-runtime-v2/result.json
```

The profile binds actual Ninja driver dependencies plus all14 production files
and identity. Optional runtime cpp/other addon files are source closure, not
falsely claimed to have been linked into the benchmark driver. Frozen metadata
does not admit timing by itself; the sole executor still verifies remaining
safety/resource/independent screening gates before a single attempt.
