# Changelog

## 0.2.0-alpha.1: V2 storage and wire format

Experimental prerelease from next_cpp26. Same toolchain requirements as
0.1.0-alpha.1 (pinned clang-p2996). Schema evolution is not supported: a wire is
accepted only when the reader's type layout matches the writer's exactly.

Breaking: the V2 format replaces V1. V1 wires are rejected with
"unsupported wire format version 1"; there is no V1 reader. See
`docs/V2_FORMAT.md`.

- Blocks carry no headers: extents are derived from container heads
  (8-byte aligned, multiples of 8); `XAllocatorStateV1` and in-arena free
  lists are removed.
- Host-side binned free table with exact reuse and splitting; it travels as
  the wire hole table, so free space survives reloads and reloads never scan.
- Validators require live blocks plus holes to tile the payload exactly.
- Strings of up to 10 bytes are stored inline in their 16-byte head.
- The owner is found through a host page before the payload (no registry or
  thread-local lookup on allocate/retire).
- Sparse wires skip free runs of at least 4 KiB; Darwin outputs of at least
  1 MiB are prefaulted in 256 KiB chunks.
- `XFixedVector::assign(first, last)`; move-assignment ownership walk only
  in debug builds (`XOFFSET_MOVE_OWNERSHIP_CHECKS`).
- Public API changes from V1 are listed in `docs/V2_FORMAT.md` section 6.
- Documentation: README, `docs/ALPHA_COMPATIBILITY.md` and `docs/VM_PACKAGING.md`
  describe V2; new `docs/USER_GUIDE.md` (usage, constraints, intended-stable API).
- CI: the main workflow also runs on pushes and pull requests to next_cpp26, and
  adds a Linux ASan+UBSan job (P2996 image, pinned compiler-rt).
- Tests: `test_assign_range` covers the iterator form of `assign(first, last)`.
- `docs/USER_GUIDE.md` section 8 lists the core guarantees K1–K7 (K8, the
  client cross-compile, is a goal) and the optional interfaces E1–E5; new
  `docs/DEPLOYMENT.md` (same-version release, untrusted input, default receive
  limits, client capacity, client-link compression).

## 0.1.0-alpha.1

First alpha packaging of the accepted XOffsetDatastructure implementation.
The production headers and V1 wire format are unchanged by the release packaging.

- Reflected root objects and offset containers in an address-stable VM arena,
  with bounded growth, verified transport, schema registration and handles.
- Explicit growth by default; opt-in Automatic growth, idle-reservation pooling
  and explicit tail-page reclamation.
- Independent C++17 OS storage target and full P2996 C++26 target.
- Relocatable CMake full/storage packages, exact prerelease metadata, bundled
  dependency headers/licenses and multi-translation-unit consumer validation.
- Source and installed-prefix archive tooling, per-file manifests and archive
  integrity audit; compiler, SDK and runtime remain external inputs.

Recorded baseline validation includes native Windows storage, WSL storage/full/
package profiles, WSL network ASan/UBSan and historical macOS evidence. The initial
WSL full 48-test profile and final WSL package/network profile are separate results.
See the source distribution's `docs/VM_PACKAGING.md` for counts and boundaries.
Release-archive validation is reported separately from these baseline results.

This is an experimental prerelease. It does not promise stable source API/ABI
between alphas, general cross-toolchain/schema wire compatibility, Windows full
P2996 support, production asynchronous transport or N7 I/O extensions. The current
WSL F/F calibration did not establish the required noise qualification; no D/F
performance, causal attribution or optimization adoption is claimed. WSL and
Docker remain eligible for subsequent qualified Linux measurements.

Save and load copy bytes. Verified format checks and integrity hashes are not
cryptographic authentication. See `docs/ALPHA_COMPATIBILITY.md` in the source
distribution, or `ALPHA_COMPATIBILITY.md` beside this file in an installed package,
for the type domain, lifetime rules and resource limits.
