# Follow-up platform verification, 2026-10-28

The full run_checked.py gate was executed with a fresh compile matrix and
live runtime profiles. All eleven compile checks passed within the previously
documented cross-compiler and header limitations.

The runtime matrix reported PASS_REFERENCE for macOS arm64, PASS_WIRE for
macOS x64, Linux x64 and arm64, iOS simulator arm64, and Android x64 and
arm64. Android was executed on the Windows Android 35 x64 emulator, with
arm64 translation for the arm64 fixture. The Mac node contacted a Windows
nodecli worker through a private queue; the worker used allowlisted binary
hashes and ADB. Both Android targets freshly wrote and read the reference
corpus and matched all ten files byte for byte. This is simulator evidence,
not a physical-device or supported shipping Android toolchain validation.

The gate returned 2 and did not start its target command: Windows x64,
Windows arm64, iOS arm64 device and iOS simulator x64 were BLOCKED_RUNTIME.
The tested entry points are build.sh and run_checked.py; direct CTest or binary
execution bypasses the gate. The wire fixture covers six generic cases and
four SLG benchmark roots, not arbitrary production schemas.

For Windows, the P2996 compiler produced x64 and arm64 Windows GNU LLVM IR and
Apple Clang 21 produced COFF objects. On x64 Windows, a link attempt using
the locally installed NDK libc++ and Visual Studio/Windows SDK libraries
failed with missing MinGW startup, thread and stack/runtime symbols and a
libc++ codecvt symbol. No Windows executable was produced and no Windows
runtime or wire pass is claimed. The available host is x64 and no Windows
arm64 execution environment is configured.

Preserved Mac evidence:
- results/schema-wire-relay-gate-20261028-v1/schema-b309be992f344af2b4acd3c7580ff607/
- results/schema-wire-windows-ir-20261028-v1/

The original 2026-10-28 record remains historical; this follow-up supersedes
its Android runtime and gate status only. The Windows and iOS gaps remain.
