# Maintained VM validation entry point

`tools/validate_vm.py` validates a snapshot of an explicitly selected source tree
or source archive. It preserves every command and result in a **new** output
directory. Run it with Python 3.9 or later, CMake and CTest 3.21 or later (3.30+
for the current full-library source). A full
build needs the P2996 compiler and matching libc++; storage alone needs C++17.
The pinned inputs and platform limits are recorded in
[`toolchains/vm-validation.json`](../toolchains/vm-validation.json).

The runner never fetches dependencies or images. Supply an external dependency
directory containing `boost/libs/<component>/include` and `typelayout/include`,
and pull the pinned Linux image before validation. The compiler, SDK, library
directories and optional runtime must belong to the intended toolchain profile.
`--compiler` is mandatory for native runs; Linux defaults to the P2996 compiler
inside its explicitly selected image. Docker `--platform` defaults to
`linux/amd64`; choose it explicitly for other platforms. The output records the
host and requested platform, so emulated functional runs cannot be mistaken for
native performance measurements.

## Current source and frozen source

Choose exactly one of `--source-dir` and `--source-archive`. The current source
snapshot includes top-level non-hidden files and the `tests`, `tools`,
`examples`, `cmake` and `include` directories. Dependencies are a separate input.
The archive can have a flat root (as the preserved F archive does) or one wrapper
directory. Symlinks, hardlinks, special files, duplicate paths and path escapes
in archives are rejected. Archives containing dependencies must first be
separated into source and explicit dependency inputs.

For the historical F archive, add its exact manifest to verify every source file:

```sh
python3 tools/validate_vm.py native \
  --source-archive docs/validation/2026-10-15-vm-followup/final-source.tar.gz \
  --source-manifest docs/validation/2026-10-15-vm-followup/final-source-manifest.json \
  --deps /path/to/external --compiler /path/to/p2996/bin/clang++ \
  --sdk /path/to/MacOSX26.5.sdk --library-dir /path/to/p2996/lib \
  --jobs 2 --out /path/to/new-frozen-full-result
```

Paths above are explicit inputs, not discovered author-machine locations. Use the
SDK and matching compiler from the intended profile. In particular, selecting a
newer default macOS SDK can fail with an older linker; supply `--sdk` explicitly.
On Linux native runs omit `--sdk` and provide matching library directories if
the toolchain needs them. Additional configure arguments use
`--cmake-arg=-DNAME=VALUE` and are recorded verbatim. Only additional cache data
is accepted: `CMAKE_*`, `CTEST_*`, `XOFFSET_*`, `VM_SOURCE`, `BUILD_TESTING` and
non-`-D` arguments are rejected so they cannot override the requested compiler,
suite, sanitizer or test selection.

## Storage and full suites

The independent storage suite neither reads nor requires Boost or TypeLayout:

```sh
python3 tools/validate_vm.py native --suite storage --source-dir . \
  --compiler /path/to/c++ --jobs 2 --out /path/to/new-storage-result
```

This uses `XOFFSET_STORAGE_ONLY=ON` in the maintained source. For older source
archives which predate that option, the runner writes a separate, recorded
C++17 CMake driver for `test_virtual_memory_storage` and `test_vm_pool`. The
archive's source and CMake files remain intact. Storage results establish only
OS-layer coverage. This runner supports Clang/GNU-style compiler drivers.
Windows MSVC storage validation uses the independent CMake entry point
(`-DXOFFSET_STORAGE_ONLY=ON`) and Windows CI workflow; the Python runner does
not claim an established MSVC invocation path. Windows full-library support
requires separate P2996 integration evidence.

Ordinary full validation does **not** require a sanitizer runtime:

```sh
python3 tools/validate_vm.py linux --suite full --source-dir . \
  --deps /path/to/external \
  --image ghcr.io/ximicpp/typelayout-p2996@sha256:fab845dadf9c7171d104cd34f8a3941119809e49baa7d0cd7ce1a7a2d71ad999 \
  --compiler /opt/p2996-toolchain/bin/clang++ --platform linux/amd64 \
  --cmake-root /path/to/prepared-cmake/root \
  --jobs 2 --out /path/to/new-linux-full-result
```

The pinned compiler image contains CMake 3.28.3. Current full-library targets
propagate `cxx_std_26`, which requires CMake 3.30+. Prepare the separate, pinned
official CMake input before an offline run:

```sh
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out /path/to/prepared-cmake
```

The preparation step downloads and verifies the archive hash from
`toolchains/vm-validation.json`; `--archive PATH` accepts previously downloaded
bytes without network. The validation runner itself stays offline. Its Linux-only
`--cmake-root` mounts a complete materialized distribution read-only, records
every file hash and checks it again before passing. Native runs can select
`--cmake` and `--ctest`. Historical sources can still use the image's older CMake.

Images must be registry references pinned by digest, already present locally.
The runner inspects and records the image and executes containers with networking
disabled. A local image ID or mutable tag is rejected. `--jobs` bounds build
parallelism and the container's CPU allocation. To test Linux storage with an
ordinary compiler, use `--suite storage`, an appropriate pinned image and
`--compiler g++` (or another available C++17 compiler).

## Sanitizers and TSan

`--sanitizer address-undefined` runs ASan/UBSan; Linux also enables leak detection.
It selects 21 VM-related test programs (including the two Stage III allocator tests) and their subprocess cases
in a full source tree, or both programs in a storage suite. Missing selected
targets are errors. `--targets name1,name2` explicitly selects another subset.
The Linux compiler resource directory must contain nonempty `lib/` and
`include/` directories; it is
**not** the parent directory of `resource/`:

```sh
python3 tools/validate_vm.py linux --source-dir . --deps /path/to/external \
  --image ghcr.io/ximicpp/typelayout-p2996@sha256:fab845dadf9c7171d104cd34f8a3941119809e49baa7d0cd7ce1a7a2d71ad999 \
  --sanitizer address-undefined --runtime-dir /path/to/compiler-rt/resource \
  --cmake-root /path/to/prepared-cmake/root \
  --jobs 2 --out /path/to/new-linux-sanitizer-result
```

The directory must be self-contained under the container's narrow `/runtime`
mount. External, absolute, broken or cyclic symbolic links are rejected before
Docker execution. Relative links wholly within the directory are supported and
their files are included in the runtime manifest. The original F resource tree
used links to a sibling extracted package and compiler headers inside the
container. Materialize a **new** directory with the recorded runtime package's
libraries and matching pinned compiler's builtin headers before using this
entry point. Keep the historical tree and evidence unchanged; passing only its
linked `resource/` directory cannot reproduce the former wider mount.

`--sanitizer thread` selects eight VM concurrency/lifecycle programs and their
subprocess cases. On macOS, the historical toolchain needs an explicit directory
containing `libclang_rt.tsan_osx_dynamic.dylib`; the runner records and links it:

```sh
python3 tools/validate_vm.py native --source-dir . --deps /path/to/external \
  --compiler /path/to/p2996/bin/clang++ --sdk /path/to/MacOSX26.5.sdk \
  --library-dir /path/to/p2996/lib --sanitizer thread \
  --runtime-dir /path/to/clang/lib/darwin --jobs 2 \
  --out /path/to/new-tsan-result
```

Native sanitizer runs without `--runtime-dir` use the compiler's bundled
runtime, whose resource path is recorded. Linux container sanitizer runs require
the explicit runtime input. Apple ASan runs disable leak detection and make no
LSan claim. Any sanitizer build/runtime failure remains a failed validation.

## Historical wire interchange

Wire validation uses the semantic C++ harness extracted from the explicit
historical bundle. It compiles V0/V1/D plus the chosen source (labelled
`current`), checks every preserved phase-0/1/2 frame with `current`, then runs
the two-round cross-version mutation matrix:

```sh
python3 tools/validate_vm.py wire --source-dir . --deps /path/to/external \
  --compiler /path/to/p2996/bin/clang++ --sdk /path/to/MacOSX26.5.sdk \
  --library-dir /path/to/p2996/lib \
  --wire-bundle docs/validation/2026-10-15-vm-followup/wire-inputs-and-versions.tar.gz \
  --out /path/to/new-wire-result
```

The preserved bundle produces 84 saved-frame checks plus 420 new matrix
operations. `--wire-variants v0,v1,d,final` also includes the frozen F headers
alongside the selected source and therefore increases the matrix size. Every
requested variant and saved-frame input must exist. Wire mode uses native full
compilation without sanitizers; it needs the matching native P2996 toolchain.

## Evidence and failure reporting

Every created output directory includes `result.json`, the invoked runner and
`source-manifest.json`. Full/wire runs also include `dependencies.json` with
hashes of active dependency include trees. Completed toolchain setup produces
`toolchain.json`; container runs add `container-image.json`. `commands.json`
records argument arrays, timestamps, return codes and log names. CTest runs
save the exact selected `test-inventory.json`, JUnit results and last test log.
The report identifies whether all registered tests or an explicit/default
sanitizer target subset ran, including selected and total registered counts.
If the source produces a build inventory, its mode, names and counts must agree
with the requested suite and CTest registration; it is retained separately as
`build-test-inventory.json`.
Passing requires a nonempty inventory, successful execution, exact matching
result names and no skipped cases. Wire runs record each operation and frame
hashes. Runtime files are hashed when an explicit runtime directory is supplied.

Exit codes: `0` means passed; `1` means failed; `2` means prerequisites were
unavailable or invalid; `130` means interrupted. The corresponding status and
reason are written to `result.json`, including failed runs. An existing output
directory is rejected before any file is changed. An output path inside a source
directory that would be copied is also rejected. No `SUCCESS` marker from
historical scripts is used, and missing inputs never become skipped passes.

Run the runner's focused prerequisite and evidence-integrity checks with:

```sh
python3 -m unittest discover -s tools/tests -p 'test_validate_vm.py' -v
```

Historical `docs/validation/2026-10-15-vm-followup/validate.py`,
`final_validate.sh` and their results remain immutable evidence. New runs use
this maintained entry point and new output directories.
