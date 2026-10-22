# Five-library value snapshot experiment

This is an isolated experiment, not part of the alpha release or its installed
API. The comparison contract is `protocols/contract-v1.json`; exact data and
trace encodings are in `model/SPEC.md`. Current acceptance is recorded in
`docs/coordination/competitor-implementation-20261017/` at repository root.
Research, design and the original P0–P11 plan remain historical snapshots.

The common application is a fixed-schema, trusted, detached mutable snapshot:
load and release caller input, query, grow and change native state, serialize an
independent owning checkpoint, destroy the old state, reload, query and destroy.
All adapters run the same logical operations and produce the same canonical
values and query answers. Their layouts, copies, allocators and necessary native
checks intentionally differ and are disclosed in the contract. A passing value
test does not establish equal untrusted-input protection, graph identity,
schema migration, cross-language interoperability or mmap support.

## Components

- `adapters/`: five statically bound native implementations and two schemas.
- `driver.cpp`: shared commands and eight timer phases; expensive full-value
  auditing is outside the main endpoint but inside the child time budget.
- `model/reference.py`: independent Python generator and sequential oracle.
- `fixtures/`: small immutable oracle goldens, including empty and boundary data.
- `tests/`: C++ codec/view/adapter, selected capability and resource lifetime
  checks. Sanitizer positive controls are separate intentional defects.
- `locks/environment-p1-v1.json`: actual sources/compiler/runtime/codegen lock.
- `../../tools/serialization_comparison/`: bounded campaign and offline audits.

Configure this directory independently with the five `SC_*_SOURCE` directories,
`SC_CAPNPROTO_PREFIX`, fixed `SC_FLATC`, and the locked compiler wrapper. CMake
does not download dependencies. All candidate runtime translation units use
GNU++26; only XOffset receives reflection/expansion flags. Cap'n Proto's static
runtime must be built by that same compiler. A separate sanitizer build uses
matching compiler-rt and a fully instrumented Cap'n Proto/KJ runtime; its host
schema generators remain the pinned release tools.

Every build or native command must hold the repository's existing global
`experiments/vm_coordination/run_job.py` lease. Never run heavy tests alongside
formal measurements. The maintained campaign commands and strict profile
format are documented in `../../tools/serialization_comparison/README.md`.

## Acceptance boundaries

Small contract tests cover detached ownership, native growth, exact mutation
statuses, checkpoint survival and independent reload. Stage7 deletion may use
stable batching while preserving the reference's sequential status semantics.
The resource tests inject one C++ `operator new` failure into stage7 bookkeeping
after actual native growth and verify RAII unwind and an unchanged restorable
initial wire. They do not cover every internal allocator, OS failures or a
multi-operation transaction guarantee.

The normal functional campaign requires all S/M/L datasets and five libraries.
Sanitizer and wider capability evidence are additional gates. A timeout remains
a failed gate, even when smaller inputs pass. No adapter may silently replace
verified load with unchecked load, change version, reduce fields or shrink the
frozen workload. Formal measurement starts only after reviewed gates and an
external protocol pin; failed calibration must never be retried to qualify.
