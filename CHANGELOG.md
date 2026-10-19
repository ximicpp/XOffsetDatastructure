# Changelog

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
