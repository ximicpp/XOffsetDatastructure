# Tasks

- [x] [core] Review both reports against `next_cpp26` / `07d6499`.
- [x] [tests] Build and run the unmodified P2996 baseline.
- [x] [docs] Define the stable-base contract and API migration.
- [x] [core] Implement independent RAII VM ownership and deterministic failures.
- [x] [tests] Query actual OS permissions and validate VM geometry/lifetimes.
- [x] [core] Integrate creation, load, growth, logical shrink, and copying save.
- [x] [core] Retain offset/wire ABI and explicit allocator growth semantics.
- [x] [core] Maintain ownership/schema association across move and swap.
- [x] [core] Synchronize registry bookkeeping over immutable reservation ranges.
- [x] [tests] Migrate direct-vector callers and add stable-storage regressions.
- [x] [tests] Execute full macOS and Linux P2996 suites and signature checks.
- [x] [tests] Verify old/new implementations can read each other's wire output.
- [x] [docs] Migrate examples and document pointer/capacity/copying semantics.
- [x] [tests] Finish sanitizer verification and record its actual results.
- [x] [core] Align actual addresses beyond the OS page size and check arithmetic before narrowing.
- [x] [tests] Prove the alignment regression fails with old code and passes with current code.
- [x] [tests] Exercise three rounds of raw and verified save/load/regrowth with full old-payload checks.
- [x] [tests] Repeat native/Linux complete suites and sanitizers against the audited final sources.
- [x] [docs] Record final coverage, unexecuted platforms, and limitations.

Native Windows execution is unavailable in this environment and must remain
explicitly listed as unexecuted rather than being marked verified.
