# VM validation inputs

`vm-validation.json` records the accepted F headers, pinned Boost/TypeLayout
revisions and separate Linux/macOS P2996 toolchains. CI obtains its immutable
Linux registry reference from this file. On 2026-10-15 that digest was pulled
successfully and reported compiler revision `a5270822dede40aaaf2c62381b89f110d2cfda4c`.
The local Docker image ID is not used as a remotely pullable reference.

Stage 2 adds the official CMake 4.2.3 archive as a separate pinned input; the
compiler image remains unchanged. Its bundled CMake 3.28.3 cannot propagate
`cxx_std_26`, which was added in CMake 3.30 ([CMake feature reference](https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html)).
`build_tools.cmake` pins Linux x86_64/aarch64 archive hashes from the official
[4.2.3 release](https://github.com/Kitware/CMake/releases/tag/v4.2.3).
Use `tools/prepare_vm_cmake.py --platform linux-x86_64 --out <fresh-directory>`
before offline validation, then pass `<fresh-directory>/root` as `--cmake-root`.
The helper also accepts `--archive` for a previously downloaded archive; it
verifies bytes before extraction. CI and the Dockerfile use the same lock.

Validate a local profile before running a comparison:

```sh
python3 tools/check_vm_toolchain.py --profile macos-p2996 \
  --compiler /usr/local/bin/clang++ --deps external \
  --sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  --output build/toolchain.json
```

Paths are caller inputs; those shown are examples from the validated host.
The checker verifies compiler revision/target, SDK contents and clean dependency
revisions and records observed hashes. CMake separately compiles and links an
actual reflection probe. Toolchain identity alone never means tests passed.

Ordinary full Linux validation requires no external sanitizer runtime. Sanitized
runs must explicitly supply a compatible extracted compiler resource directory;
the lock records the package hash used for F. macOS TSan uses an explicitly
selected runtime directory. Never silently use the newest SDK/runtime for a
historical performance comparison.

The repository Dockerfile/compose now pin the compiler source revision and reject
non-SHA references. This is a source-build recipe, not a byte-reproducible image:
its Ubuntu base tag and apt repositories remain mutable. The canonical CI input
is the validated registry digest, including its compiler and standard library,
plus the separately hash-pinned CMake distribution for current full targets.
Rebuilding the source recipe or changing the lock requires fresh validation;
it must not be represented as the already tested image. The recipe does not
currently build compiler-rt; use the explicit runtime input for sanitizer runs.
