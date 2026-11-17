# Stability promises and the automated tests behind them (K0–K9)

K0–K9 are V2's stable API scope (see [USER_GUIDE.md](USER_GUIDE.md)). This table maps each promise to the
tests that fail in CI when it breaks. Test names are CTest names; "CI job" means a job in
`.github/workflows/ci.yml` (GitHub CI has been off since 2026-10-23; these checks are run by the local
validation `tools/local_validate.sh`). When changing the behavior of any promise, the matching tests must
fail first.

There are two builds: the server's C++26 reflection build and the client's C++20/C++23 field-list build
(`XOFFSET_RECORD`). Tests marked "reflection build only" do not compile in the field-list build
(`test-inventory.json` records them as `cxx26_only_not_built`); all other tests run in both builds.

| Promise | Contents | Tests | What they test |
|---|---|---|---|
| K0 | Relocatable: the payload holds no absolute addresses, and after any change it can be moved as a whole to any address and read and written there | `test_type_safety` (reflection build only), `test_inheritance` | Types with raw pointers, member pointers, function pointers, vtables or `std::string`/`std::vector` are rejected by compile-time admission (`is_byte_copy_safe_v`) |
| | | `test_admission_typelayout` (reflection build only) | XOffset's own admission agrees with TypeLayout's `is_byte_copy_safe_v` |
| | | `test_schema_describe` | In the field-list build, records with a field list are admitted as usual; types that cannot have a field list, such as `std::string` and polymorphic types, are not admitted |
| | | Configure-time check `polymorphic_rejection` (`cmake/XOffsetBuildChecks.cmake`) | Calling `XBuffer::make<T>` with a polymorphic type must fail to compile with the error `Type is not byte-copy safe`; a positive control runs first to rule out a broken toolchain |
| | | `test_stability_contract` (parts of K3 and K5), `test_v2_golden` | Load the output into a buffer at a new address, then read, modify and send again; write-read-write is byte-identical |
| | | `test_clone`, `test_v2_fuzz` (copy check) | A modified buffer with holes is copied as a whole to another address with `clone()` / `clone_into()`, without a wire: bytes, free table and re-save are the same, and the same later changes produce the same bytes |
| K1 | Schemas are plain C++ aggregates, no IDL: the server derives layout and fingerprint by reflection, the client adds one field-list line | `test_zero_boilerplate`, `test_reflection_core`, `test_reflection_advanced`, `test_type_signatures` (all reflection build only) | Aggregates can be created, read and saved with no boilerplate; reflection walks the members; signature text |
| | | `test_schema_describe` | One set of `XOFFSET_RECORD` records (with derivation, enums, `std::array`, each container): in the reflection build the macro checks the field list entry by entry and the library's own field lists are compared with reflection; in the field-list build the signatures equal the fixed text written by reflection, and `save_verified` / `load_verified` round-trip |
| | | `schema_describe_*_fail` (omitted, order, duplicate, array, padding, enum, undescribed; the reflection build also has promise) | Missing, misordered or duplicate entries, members missed at arrays or padding, enums without a fixed underlying type, missing field lists, and violations of the plain-record promise fail the build with the expected error |
| | | `describe_dump_slg` (reflection build only) | The field lists printed by `tools/describe_dump` via reflection equal those committed in `bench/slg` word for word |
| | | `test_v2_golden` | The wire bytes of a fixed model are fixed (size + FNV-1a), and write-read-write is byte-identical |
| K2 | Objects are built in the arena and read and written in place (including variable-length changes), with free blocks reused | `test_modify`, `test_vector`, `test_map_set`, `test_main_containers`, `test_string_replace`, `test_assign_range`, `test_stable_erase_if`, `test_resize_growth` | In-place modification semantics of each container |
| | | `test_free_table_policy`, `test_allocator_model` | The free block reuse policy agrees with an independent oracle |
| | | `test_mutation_failures`, `test_allocator_failures` | A single modification is atomic when allocation fails |
| K3 | Independent output: `save_verified` emits a copy owned by the caller, and the buffer can keep changing after sending | `test_stability_contract` (K3 part) | After saving, modifying, growing, clearing and destroying the source buffer, the sent wire is unchanged and still loads the original content |
| | | `test_verified_paths` | `save_verified_into` does not overlap the source storage, and is rejected when the owner changes in a callback |
| K4 | Receive checks come in three tiers; the full tier's structural validation rejects truncation, out-of-range offsets, overlap, incomplete tiling, invalid values and schema mismatches (the trusted and bounds tier interfaces were added in 3.4, with their tests) | `test_wire_validation`, `test_validation_granules` | Malicious offsets, container headers, hole tables and payload metadata are rejected before dereferencing |
| | | `test_v2_fuzz` | An independent spec oracle agrees with `load_verified`, `receive_raw::validate`, verified receive, `reload_verified` and receiving directly into a reused slot (`begin_verified_reload`) on accepting/rejecting mutated wires; aborting a direct receive midway, under-reporting lengths and declared lengths disagreeing with the header are always rejected |
| | | `fuzz_v2_wire_replay`, `fuzz_wire_schemas_replay` (with `_corpus`; no-exception versions `_replay_noexc`) | The two libFuzzer harnesses compiled without libFuzzer, replaying the crash inputs in `tests/fuzz/regressions/` and their own seed corpora; every ci.yml job (P2996 Clang, GCC 16, two sanitizers, arm64, Windows) includes them; while Actions is off, the full CTest and the daily long run (GCC 16 ASan+UBSan) run them |
| | | `test_receive_raw`, `test_verified_receive`, `test_reload_reset` | Raw receive, segmented receive and receiving directly into a reused slot |
| K5 | After receiving, modify and send again; free space is carried along with the hole table and does not degrade | `test_stability_contract` (K5 part) | After reloading, free-then-allocate reuses holes and the tail does not grow; the receiver's allocation decisions are byte-identical to the sender's; the same holds on the second hop |
| | | `test_free_table_policy` (`lifecycle_move_swap_reload`) | Hole table write-read-write is byte-identical, and allocation decisions after reloading equal the original buffer's |
| K6 | Schema fingerprints must match exactly, otherwise rejected | `test_schema_envelope`, `test_wire_admission` | Mismatched schema hash, root type id, magic or endianness mark is rejected; V1 wires are explicitly rejected |
| K7 | Object representation is the same across compilers and platforms; once a user schema's signature is pinned, a mismatch fails the build | `check_compat`, `check_signature_regeneration`, `check_signature_matrix` | The signatures of 3 test types on 4 target platforms match the baseline |
| | | `test_cross_compiler_wire` + CI job `cross-compiler-wire` | The 5 corpus wires written by P2996 Clang and GCC 16 are byte-identical and pass `load_verified` on each other |
| | | CI job `gcc16` | GCC 16 runs the full CTest |
| | | `test_schema_pin`, `schema_pin_mismatch` | After a user schema's signature is pinned with `XOFFSET_PIN_SCHEMA`, compilation passes when the layout matches the pinned value; otherwise the build fails with an error containing `XOffset schema pin mismatch (K7)`. Both builds run them, so the build also fails on the clients' native compilers (NDK Clang, MSVC, clang-cl, MinGW GCC) |

## K8 (core promise, since 2026-11-04)

Clients (iOS, Android, Windows, 64-bit little-endian) build with the field-list backend on each platform's
native compiler, using the same library and the same wire as the server. It is not a CTest item; each
client target runs `tools/native_targets.sh` once:

| Check | What it tests | Status (3.5.0) |
|---|---|---|
| Full CTest on the target (Release) | The tests of K0–K7 above that run in both builds pass on the target's compiler and standard library | Windows x64 (MSVC 19.44 C++20/C++23, clang-cl 19, MinGW GCC 16) and Android arm64 (NDK r29/r30, cloud qemu-aarch64) all pass on 4.1.0; the passing results for macOS arm64 and the iOS simulator (Apple clang) come from 3.3.0 and were not rerun on 4.1.0 (numbers in [CLIENT_TARGETS.md](CLIENT_TARGETS.md)); real devices (3.2.x): iPhone 15 Pro Max 138/138 and an Android 11 phone (3.2.0-rc.1) pass ([DEVICE_TESTING.md](DEVICE_TESTING.md) section 9) |
| Reading the reference corpus | All 6 corpus files written by the server's C++26 reflection build are accepted by the target's `load_verified` | Accepted by all of the targets above |
| Writing the corpus, `diff -r` | The corpus written by the target is byte-identical to the reference | Byte-identical on all of the targets above |

The scope of the guarantee is in [USER_GUIDE.md](USER_GUIDE.md) section 8, and per-target results in
[CLIENT_TARGETS.md](CLIENT_TARGETS.md). Every client target is run before each release
([RELEASING.md](RELEASING.md)).

## K9 (core promise, since 3.5.0): native delta sync

Only the changed bytes are sent; the receiver merges them and checks per the link's tier (API added in
3.3, promoted to a core promise in 3.5.0). Whole-state deltas (format 1.0): `test_delta_sync` (rejection
cases and random streams, including no-exception builds), `test_delta_format` (golden files of the frozen
delta wire 1.0), `fuzz_delta_sync` (pre-release ASan fuzz, both server and client builds, admit versus full
differential), `test_receiver` (`apply_delta`'s default tier per link). Full conditions in
[USER_GUIDE.md](USER_GUIDE.md) section 8.

Viewport deltas use shared deltas (XS 2.0 since 4.0, `XReceiver::apply_sync`, since 3.5):
`test_shared_delta` (record encoding: changed bodies, whole bodies, tampered bodies, boolean and mask out of
range, writer bytes), `test_shared_delta_sync` (a random stream of 1500 messages, with full and admit
clients both compared with the server model after every message; rejection cases, tampered messages,
duplicate entries; including no-exception builds), `test_shared_delta_format` (byte-exact golden files for
XS 2.0, the sender and both receive tiers), `fuzz_shared_delta` (pre-release ASan fuzz),
`receiver_from_client_sync_fail` (`apply_sync` on `from_client` does not compile); the publisher (since
4.0, `xoffset_shared_publisher.hpp`): `test_shared_delta_publisher` (a random world runs several ticks,
each message is received by full and admit clients and compared with the server; slot allocation order, no
reuse within one message, spilling to the cold region and slot numbers above 255, reuse and growth of the
march ring; including no-exception builds), `test_shared_delta_publisher_errors` (one error case per
contract and slot check).

## What is not in CTest

- Equivalence of the field-list build and the reflection build: the `diff-c++20` column of
  `tools/local_validate.sh` compares the C++26 reflection build and the C++20 field-list build on the same
  set of mutated wires; wire bytes and accept/reject results (including error messages) must be identical.

- The performance of K2 and K3 (read, modify ≤ 1.05, independent output ≤ 0.20) is judged by the formal
  measurement protocol on fixed machines, not in CI. Results are in the performance reports outside the
  repository.
- Content integrity is not promised: a wire with only one field value changed is accepted. Do not describe
  K4 as a way to detect tampering.
- K4 comparison across versions: `tools/validation_differential.sh <old dir> <new dir> <cases> <seed>...`
  compares the validation results (including exact error messages) of two library versions on the same set
  of mutated wires. The comparison is per payload: wires with an extent table are first expanded into the
  equivalent dense wire before validation, because versions may accept different sparse encodings (since
  0.3.0-beta.2 gap bytes need not fall in holes). Within one version, "a sparse wire gives the same result
  as its dense equivalent" is checked by `test_v2_fuzz`.
- Pre-release fuzz for K4 (since 2026-11-04; the nightly long run stopped on 2026-10-28 and will not
  resume): before every release, one round of ASan+UBSan fuzz runs in the cloud on every receive entry
  point (trusted, bounds and full tiers, including delta apply), and a release ships only with no findings
  ([RELEASING.md](RELEASING.md)). The entry point is `tools/fuzz_prerelease.sh` (staged, produces
  `VERDICT.md`), which calls the scripts below.
- Long-run tools for K4 (threat model F4): `tools/fuzz_nightly.sh` runs GCC 16 ASan+UBSan differential
  fuzz (`test_v2_fuzz`, 4 seeds, about 45 minutes) and regression corpus replay on a cloud Docker host;
  on a crash, `tools/fuzz_report.py` opens a `fuzz-crash` issue per signature and pushes a
  `fuzz-crash/<signature>` branch. Results are not in CTest; they are recorded in the project files under
  `fuzz-nightly/`. From 2026-10-27, the same night then runs `tools/fuzz_libfuzzer.sh`: it builds the two
  libFuzzer targets natively with the cloud P2996 toolchain (which ships the libFuzzer runtime)
  (ASan+UBSan, `-fork=4`, about 20 minutes per target); corpora persist across nights in the project
  files under `fuzz-nightly/corpus/`, and findings go through the same `fuzz_report.py`.
  `fuzz_schema_evolution` was added on the night of 2026-10-27. From 2026-10-28 two more steps cover the
  client builds: `tools/fuzz_libfuzzer.sh --backend fieldlist` runs `v2_wire` and `wire_schemas` on the
  C++20 field-list build (corpora saved separately as `fieldlist-<target>.tar.gz`);
  `tools/fuzz_outcome_diff.sh` replays both corpora, the regression inputs and the seeds on the C++26
  reflection, C++20 field-list and no-exception builds (ASan+UBSan), and each input's per-entry results and
  error text must agree (the no-exception build has no raw or receive entry points, so those two columns
  are dropped from the comparison).
