# Cross-target schema compile gate

This is a compile-only gate, not a platform runtime or wire interoperability test.
The built-in catalog pins the repository test fixture pin::Root, not game schemas.
Each target compiler must satisfy its target macro, TypeLayout byte-copy safety,
XOffset wire admission, registered root name and an explicit XOFFSET_PIN_SCHEMA.
The bundled Boost/TypeLayout include trees and licenses are checked against
the repository content lock before compilation. Do not disable platform
checks. Missing targets never count as passed.

Run with the Mac P2996 compiler and a new, non-existing output directory:

    python3 tools/schema_guard/verify.py --clang /absolute/path/to/clang++ --out /absolute/new/result-dir

Defaults: Linux uses digest-pinned GCC 16 Docker images (add --pull to permit
pulling), macOS and iOS use the supplied P2996 Clang and local Apple SDKs.
Android uses --ndk or ANDROID_NDK_HOME and the supplied Clang; Windows requires
an explicit cross-compiler profile. All targets are listed even when --target
selects only some. Exit code 2 means a selected target did not pass.

A custom catalog must be a header defining XSG_SCHEMA_ROOTS(X), where each
entry is X(registered_root_type, "reviewed wire signature", "reviewed TypeLayout signature"). Alias types
containing commas first. The header must also define XSG_RECORD_FIELDS(X), with X(record_type, "field1|field2|...") entries, and include schema definitions and
XOFFSET_REGISTER_SCHEMA_NAME. Supply --catalog /absolute/path/to/header.
Review baseline signatures independently on all targets before adopting pins.
A missing pin, empty catalog, or disabled platform check fails compilation.
The catalog must be in the project tree to be mounted into Docker.

Overrides use --profiles JSON, for example:
    {"windows-x64": {"kind": "host", "compiler": "/path/to/cross-clang++",
                     "flags": ["-std=c++26", "-freflection",
                               "-fexpansion-statements", "--target=x86_64-w64-windows-gnu",
                               "... target sysroot and libc++ flags ..."]}}
A Docker profile uses kind=docker, image, platform, compiler and flags. The
probe independently checks target macros so a host compiler cannot silently
pass as another target. Record toolchain, SDK and headers externally; a compiler
flag alone does not certify the actual environment.

Scope limitations: only source-level layout pins, declared field names, catalog assertions and TypeLayout safety are
checked. No runtime application is executed, no wire corpus is exchanged, no
complete enum-value or business-semantic comparison is performed, and
unused/unregistered business roots are not detected. Matching a pin is not
proof of target runtime availability or application interoperability.

## Target execution and wire exchange

Build the repository's existing test_cross_compiler_wire target separately on
each target, using the actual target toolchain. When both executables can be
started from the same host (optionally with runner argv prefixes), run:

    python3 tools/schema_guard/wire_check.py --reference-binary /path/to/reference \
        --target-binary /path/to/target --out /absolute/new/result-dir

The tool runs both --write operations, each binary's --read of the other
corpus, and compares the set of .xwire files and their bytes. Supplying the
same binary twice tests only the harness, not platform interoperability.
The runtime matrix uses a separate fixture. Android adb and Docker command-line
adapters are provided but need working target environments; an iOS device app
adapter and signing are not provided. The supplied binary determines which schema cases are covered:
the repository's existing binary does not cover arbitrary business schemas.

Compile gate negative tests on a Mac with the reflection compiler:

    XSG_TEST_CLANG=/absolute/path/to/clang++ python3 tools/schema_guard/test_guard.py


The SLG benchmark catalog covers four benchmark roots and 13 declared record
field lists; it is not a production game schema. Run it with:
    --catalog tools/schema_guard/catalog_slg.hpp
Its pins originated from macOS arm64 and are checked on every selected target.
Different compilation profiles are represented by separate invocations; do
not infer that one target result covers other OS versions or compilers.

## Business catalog adoption

Use a shared header for business schema definitions and registered root names.
Provide three entries per root: the type, a candidate wire_abi_signature, and a
candidate TypeLayout get_layout_signature. signature_dump.cpp can print both
from a native toolchain using a temporary catalog. Independently compare each
candidate on every declared target before checking in the pins. Enumerate every
reachable business record in XSG_RECORD_FIELDS; every registered root must
be present and include explicit enum-value
static assertions in the shared catalog. Unlisted types and non-layout business
semantics are not automatically discovered.

For the local October 2026 Mac run, the NDK sysroot was partially fetched from
Google's r28c archive; each extracted ZIP entry passed its CRC check, but the
full archive checksum was not verified. The Mac P2996 wrapper uses its own
runtime-v2 libc++ headers. Passing this compile check does not establish an
Android/iOS target libc++ ABI, linking, binary execution, or shipping support.

## Pre-run gate

The repository's build.sh invokes the complete compile matrix and runtime/wire
matrix before CTest and before the subsequent signature tools. A failed, unavailable or missing target
stops that entry point. Each invocation produces a new report directory under
$BUILD_DIR/schema-guard-runs (override with XSG_REPORTS).

Set XSG_PROFILES to a compile profile file, XSG_WIRE_PROFILES to a runtime
profile file, XSG_NDK (or ANDROID_NDK_HOME)
to the Android NDK, XSG_CATALOG currently must resolve to the SLG benchmark catalog.
The default catalog for this run gate is catalog_slg.hpp; it describes the
benchmark, not production game schemas. Set XSG_PULL=1 to allow pulling
digest-pinned Linux Docker images. Profiles can replace Docker with explicit
cross compilers, including Windows. The gate compiles every listed target; it
does not accept --target and refuses to start the command on any non-pass.

To gate another application or test entry point:
    python3 tools/schema_guard/run_checked.py --clang /path/to/p2996/clang++ \
        --profiles /path/to/profiles.json --ndk /path/to/ndk \
        --wire-profiles /path/to/wire-profiles.json \
        --reports /path/to/report-parent -- /path/to/program arguments...

Only invocations routed through build.sh or run_checked.py are guarded.
Direct execution of a built binary or direct ctest bypasses this integration.
Report directories and logs are retained, including on failure. The compile and runtime reports describe distinct checks; every runtime target
must pass before the command runs. Runtime coverage is limited to the fixture
and target configurations explicitly listed in the report.

## Preparing Windows cross-target headers

prepare_windows_headers.py fetches pinned MSYS2 MinGW header and winpthreads
packages for x64 and arm64, verifies their SHA-256 values and emits a manifest
and profiles.json in a new directory. It can extend existing Linux profiles:
    python3 tools/schema_guard/prepare_windows_headers.py --clang /path/to/p2996/clang++ \
        --base-profiles /path/to/linux-profiles.json --out /path/to/new/windows-tools

Pass --zstd /path/to/zstd when it is not on PATH. Repeat --archive-dir DIR to
reuse previously downloaded packages; checksums are still verified. The pinned
URLs may eventually be retired by the upstream mirror; a missing package is a
failure requiring an explicitly reviewed version/checksum update. The generated
manifest records package hashes, not ongoing integrity checks of extracted files.

These profiles exercise Clang's Windows GNU targets and MinGW C headers using
the supplied compiler's C++ headers. They are compile-only evidence, not a
Microsoft SDK/MSVC STL validation or proof that a Windows binary can link or run.
The current Mac wrapper supplies its own libc++ headers. The full matrix report
includes the profile manifest when it is next to profiles.json.

## Runtime and wire matrix

The runtime fixture contains the six existing cross-compiler corpus cases plus
four populated SLG benchmark roots (PlayerState, BattleMsg, ShardState, Report).
Each target reports its compiled OS, architecture, pointer width, endianness
and source fingerprint. The matrix requires the current fingerprint, an exact
10-file corpus, a passing reference self-check, and each target reading the
reference corpus while the reference reads that target's corpus. The two
corpora must be byte-identical. This covers those benchmark fixtures, not
arbitrary production schemas; custom catalogs are therefore blocked by
run_checked.py until they have an appropriate runtime fixture.

On a Mac with the P2996 compiler, matching libc++ headers, resource directory
and a booted arm64 iOS simulator, create new runnable test binaries with:
    python3 tools/schema_guard/build_wire_apple.py --clang /path/to/clang++ \
        --libcxx-include /path/to/include/c++/v1 --resource-dir /path/to/clang-resource \
        --simulator SIMULATOR-UDID --out /path/to/new/apple-wire-build

The output profiles.json contains macOS arm64/x64 and iOS simulator arm64.
The x64 build uses reflection Clang to emit target-specific LLVM IR and Apple's
Clang x64 backend to compile and link it. This profile is incomplete: merge
entries for every remaining target into a new JSON file before using it as
XSG_WIRE_PROFILES. An entry has:
    {"binary": "/absolute/path/to/target-fixture", "runner": ["runner", "args"]}

Each platform binary must compile tools/schema_guard/wire_runtime.cpp with the
current source fingerprint, obtained with:
    python3 tools/schema_guard/wire_inputs.py
Pass that hex value as the quoted string macro XSG_WIRE_INPUT_HASH. The target
runner receives the binary, then --xsg-identify or --write/--read and a local
corpus directory. The provided adb_wire_runner.py translates those operations
for an explicitly selected Android serial; docker_wire_runner.py does so for a
digest-pinned Linux container. Android adapter argument/transport tests use mocks; the adapter was also
exercised with the Windows Android x64 emulator and its arm64 translation.
The Docker adapter was exercised in the Mac Linux VM. These do not establish
physical-device compatibility. For either adapter, end
the configured runner prefix with "--" before the automatically appended binary.
For iOS simulator use ["xcrun", "simctl", "spawn", "SIMULATOR-UDID"].
Native iOS devices need a signed app-based runner; one is not supplied.

Run the matrix alone with:
    python3 tools/schema_guard/runtime_matrix.py --profiles /path/to/wire-profiles.json \
        --out /path/to/new/runtime-report

The runtime matrix never treats an unconfigured target or self-test as a
cross-platform pass. build.sh and run_checked.py require all 11 runtime targets;
the configured simulator/device must be available at each run. If the fixture
sources or locked dependencies change, rebuild target binaries. Direct CTest
or direct program execution still bypasses build.sh/run_checked.py.

For Linux, build_wire_linux.py can build both fixtures with the pinned GCC 16
Docker images, merging the generated entries into existing Apple runtime
profiles:
    python3 tools/schema_guard/build_wire_linux.py --pull \
        --base-profiles /path/to/apple/profiles.json --out /path/to/new/linux-wire-build

The Docker daemon must be available and support both Linux architectures
(natively or through an explicitly configured emulator). A successful build
is not a runtime or interoperability result; run the resulting profiles
through runtime_matrix.py. The Mac October 2026 run exercised Linux arm64 in
a VM and Linux x64 through emulation; results are in separate runtime reports.
The default builder covers both targets; --target can restrict the build while
the runtime gate still requires the complete matrix.

If Docker Hub is unavailable, --registry-prefix mirror.gcr.io/library/ uses
the same pinned image digests via that registry mirror.

See VALIDATION_20261028.md for the recorded platform results and limitations.

## Windows-hosted Android emulator relay

A Mac gate can use an Android emulator on a Windows node without exposing ADB
on the network. Start a separate Windows worker while the gate runs. It polls a
private Mac queue through the configured nodecli connection, executes only
pre-allowlisted local fixture binaries, and returns each fresh result and corpus.
There is no fallback to a historical result. Each request has a unique id and
the Mac runner checks the binary SHA-256 against the Windows worker allowlist.
The queue must be an existing private directory with mode 0700. Restrict access
to the queue and worker machine: anyone who can write the queue can cause an
allowlisted fixture to run. Use a fresh queue for each worker session.

Copy nodecli_wire_worker.py, adb_wire_runner.py and the exact Android binaries
to the Windows node using nodecli copy. Verify that the binary hashes on both
nodes match. Start the emulator with an explicit ADB serial, then on the Windows
node run (PowerShell backticks indicate line continuation):

    python nodecli_wire_worker.py --mac-cwd /absolute/mac/project --queue /absolute/mac/private-queue `
        --adapter C:\path\adb_wire_runner.py --adb C:\path\adb.exe --serial emulator-5568 `
        --android-fixture C:\path\android-x64.bin --android-fixture C:\path\android-arm64.bin

The worker also accepts repeated --native-fixture paths for Windows executables.
It does not build them. The worker calls nodecli and must be started from a
directory whose nodecli project mapping can reach the Mac and local nodes.
In the Mac runtime profiles add, for each target, an entry such as:

    {"binary": "/absolute/mac/android-x64.bin",
     "runner": ["python3", "/absolute/mac/project/tools/schema_guard/relay_wire_runner.py",
                "--queue", "/absolute/mac/private-queue", "--"]}

Pass the complete profiles file as XSG_WIRE_PROFILES or --wire-profiles to
run_checked.py. The configured target binary on Mac and allowlisted Windows
binary must be byte-identical. The normal matrix performs target identity,
source fingerprint, exact corpus, both read directions and byte comparison.
A stopped worker, missing device, wrong binary hash, failed transfer, or
timeout fails the matrix. The worker does not make the remaining unavailable
targets pass. The October 2026 live run used the Android x64 emulator with
arm64 translation; it is not evidence of physical Android hardware.
