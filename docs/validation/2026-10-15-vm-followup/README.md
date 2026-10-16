# 2026-10-15 VM follow-up validation evidence

The final accepted implementation is **F**, identified by:

- Main header: `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`.
- OS header: `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`.

`accepted_native/` contains F's native macOS build, 46/46 CTest results, updated
example output and source manifest. `final-validation-summary.json` indexes the
remaining F checks: Linux full 46/46, Linux ASan/UBSan+LSan 26/26, macOS TSan
15/15 and V0/V1/D/F wire interchange 420/420. The wire matrix checks full values
after two cross-version mutation rounds and includes reuse/cache-flush workloads.
Logs are under `final-validation/`.

Top-level `native-full-*` and `native-targeted-*` logs describe the earlier **E
candidate**, whose 47-test suite included allocator splitting/coalescing. They
are retained history, not F results. That allocator prototype was rejected after
performance and layout regressions. Its complete source/tests are preserved in
`../../perf/2026-10-15-vm-followup/candidate_e/prototype_source.tar.gz`; the local
`allocator_candidate/` files are supplementary copies. `pool/` records an earlier
OS-only pool/reclaim stage, while `direct_rw_validation/` records the final OS
header's independent native macOS/Linux checks. Exact hashes identify each stage.

`final-source.tar.gz` and `final-source-manifest.json` preserve the full F build
source without dependencies or binaries. `wire-inputs-and-versions.tar.gz`
preserves the four version headers, semantic harness, and generated frames.
The summary includes 479 hashed validation artifacts; the source manifest
contains 76 files. Dependencies and compiler revisions are recorded separately.

## Reproduction

For the current checkout, follow the repository's normal P2996 CMake instructions
and run CTest. To reproduce the frozen suites, extract `final-source.tar.gz` into
a new scratch directory, provide its `source/external` path using the recorded
Boost/typelayout dependencies, then invoke:

```text
final_validate.sh <extracted>/source <repository-root> <new-output-directory>
```

The driver uses the local pinned P2996 Docker image and the preserved compiler-rt
runtime under `<repository-root>/build/linux-vm/runtime`; those large dependencies
are intentionally not embedded. The wire stage also needs the original V0/V1/D
header locations recorded by the script; the companion wire archive supplies
portable copies if reproducing elsewhere. `validate.py` exposes individual
`linux`, `tsan`, and `wire` subcommands and explicit path options for relocated
inputs. Always use a fresh output directory and do not overwrite this evidence.

Linux full/sanitizer integration uses x86_64 CPU emulation on the arm64 host;
independent Linux arm64 OS-layer tests are separate. Apple ASan has no supported
leak detection here, so LSan claims apply to Linux only. Windows has no available
SDK/runtime on this machine and was neither compiled nor executed. These are
functional checks; Linux-emulation timings are not performance evidence.
