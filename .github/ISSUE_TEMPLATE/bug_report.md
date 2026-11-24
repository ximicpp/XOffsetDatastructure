---
name: Bug report
about: Report a crash, miscompile, wire mismatch, or unexpected behavior
title: "[bug] "
labels: bug
---

## Summary

A clear and concise description of what happened and what you expected to happen.

## Environment

- **Commit SHA or `XOFFSET_VERSION`:**
- **OS & Architecture:** (e.g., Linux x86_64, Windows 11 x64, macOS arm64, Android arm64-v8a, iOS arm64)
- **Compiler & Version:** (e.g., GCC 16.2, Clang 19, AppleClang 21, MSVC 19.44)
- **C++ Standard:** (`20`, `23`, or `26`)
- **CMake Version & Flags:** (e.g., `-DXOFFSET_HARDENED=ON`, `-DXOFFSET_USE_PMR=0`, `-fno-exceptions`, `/EHs-c-`)

## Steps to Reproduce

Provide a minimal standalone reproducer (or exact CMake + test command) and any failing wire bytes or
sanitizer output.

```cpp
// minimal reproducer
```

## Observed Output

Paste compiler errors, test output, or ASan/UBSan/TSan backtraces here.

## Additional Context

If this is a suspected security vulnerability on untrusted input (`load_verified` / `XLink::from_client`),
please do **not** submit a public issue; follow `SECURITY.md` instead.
