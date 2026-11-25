# Memory per process (2026-11-25)

SLG demo cases at M scale, one process per format, the `slg_bench_all` binary of
[slg_summary.md](slg_summary.md) (library v4.3.0, tree `5a54d9d1`, GCC 16.2 `-O3`):

```sh
docker run --rm --network none -e SLG_REUSE=1 -e SLG_CASES=demo -e SLG_NATIVE_OFF=1 \
  -v <bin>:/b:ro -v <out>:/out xo-cppcon24 /b/slg_bench_all --mode proc --scale M --seed 20261021 \
  --formats <format> --out /out/proc_<format>.jsonl
```

RSS and peak RSS (HWM) after the run, page tables (`VmPTE`), and RSS ÷ the smallest rival RSS. Gates
(goal 3.6): RSS and peak at most 1.10x the smallest rival, page tables at most 1% of RSS.

| Format | RSS MiB | Peak MiB | Page tables MiB | Page tables ÷ RSS | RSS ÷ smallest rival |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset_trusted | 538.3 | 538.3 | 2.25 | 0.42% | 0.92 |
| protobuf | 583.9 | 583.9 | 1.37 | 0.23% | 1.00 |
| zpp_bits | 595.8 | 595.8 | 1.48 | 0.25% | 1.02 |
| flatbuffers_noverify | 595.9 | 595.9 | 1.40 | 0.23% | 1.02 |
| cereal | 597.6 | 597.6 | 1.48 | 0.25% | 1.02 |
| cista_unchecked | 698.5 | 698.5 | 1.73 | 0.25% | 1.20 |
