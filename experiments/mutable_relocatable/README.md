# mutable_relocatable experiments

Prototypes and micro-benchmarks for the "mutable, relocatable data structure"
positioning discussion. The plan and the questions they answer are in
[docs/positioning/MUTABLE_RELOCATABLE.md](../../docs/positioning/MUTABLE_RELOCATABLE.md), section 5.

Everything here runs in the cloud VM (GCC 16 docker) and reports ratios only.
Numbers meant for external use are measured on the Mac under `PERF_GOAL.md` rules.
Nothing here is wired into CMake, CTest or CI.
