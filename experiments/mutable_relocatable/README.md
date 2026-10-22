# mutable_relocatable experiments

Prototypes and micro-benchmarks for the "mutable, relocatable data structure"
positioning discussion. The plan and the questions they answer are in
[docs/positioning/MUTABLE_RELOCATABLE.md](../../docs/positioning/MUTABLE_RELOCATABLE.md), section 5.

Everything here runs in the cloud VM (GCC 16 docker) and reports ratios only.
Numbers meant for external use are measured on the Mac under `PERF_GOAL.md` rules.
Nothing here is wired into CMake, CTest or CI.

| File | What |
| --- | --- |
| `reloc_tax.cpp`, `build.sh` | X1 relocation tax vs std, X2 in-process whole copy (`RESULT_X1_X2_20261022.md`) |
| `handoff.cpp`, `build_handoff.sh` | X4 same-host handoff through a sealed memfd, Linux only (`RESULT_X4_20261022.md`) |
