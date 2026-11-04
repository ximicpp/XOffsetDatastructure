# G9R3 result (2026-11-04T09:05:11Z)

t0 49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479 (next_cpp26 49ce3c68 (#212; mainline merged into PR #213)); t1 daf56e89cd4c4ec8a8d838e52846864e24bf82bd (CKPT15 head (PR #213 keep free table daf56e89)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479 tree 9b9544a872e52b29c154df3e2940c7ce93bef908 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 49ce3c68 (#212; mainline merged into PR #213))
t1 daf56e89cd4c4ec8a8d838e52846864e24bf82bd tree 585971e0ef7c8c9fbe851c4a9ff997451f120235 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT15 head (PR #213 keep free table daf56e89))
t1: diff over t0 sha256 3e461319274ea3c0eb3f97b5d56f167da7841e6af564eb9adb1b0e40a48faab9 ( 3 files changed, 116 insertions(+), 50 deletions(-))
t1: library header diff over t0:  1 file changed, 67 insertions(+), 37 deletions(-)
t1: first-parent commits over t0:
  daf56e89 Merge next_cpp26 into claude/keep-free-table-20261104
  4a54734d Keep a reloaded buffer's free table instead of recycling it
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 cbdab37ce5213b313f9cbae2f81d6a569d1fe515171ead440165e3d8731521d1
reloc_bench t1 sha256 bda36a5d999b89dce10307362d737ffe0ad36216ea0b0e06898a549563196d65
reloc_bench t0a sha256 997947e5a22513ed50575bc5e308ad08ee0c568181d4dee72419e74f025ee8bc
reloc_bench t1a sha256 c896031c33e1abe6059c1fbb5b546e01a1cc160794aeef9122cc4d3978e1a4b4
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.726 | 0.725 | 0.998 | 0.995–1.000 | 0.995, 1.000 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.756 | 0.996 | 0.992–0.999 | 0.997, 0.994 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.367 | 1.002 | 0.999–1.005 | 1.005, 0.999 |  |
| map.erase.fresh | std::flat_map | G9 | 0.936 | 0.940 | 1.004 | 1.001–1.007 | 1.007, 1.002 | worse |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.998–1.004 | 0.999, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.869 | 0.869 | 1.000 | 0.997–1.003 | 0.993, 1.007 |  |
| nested.build.ord | std | G9 | 0.327 | 0.318 | 0.972 | 0.967–0.977 | 1.000, 0.944 |  |
| reset | std | G9 | 0.254 | 0.237 | 0.934 | 0.926–0.941 | 0.948, 0.919 |  |
| set.erase.fresh | std::flat_set | G9 | 1.085 | 1.087 | 1.002 | 0.999–1.005 | 1.004, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.752 | 0.996 | 0.994–0.998 | 0.995, 0.997 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.999 | 1.004 | 1.001–1.006 | 1.005, 1.002 | worse |
| str.append | std::string | G9 | 0.419 | 0.425 | 1.014 | 1.009–1.019 | 1.020, 1.008 | worse |
| str.assign_long | std::string | G9 | 0.867 | 0.800 | 0.922 | 0.921–0.924 | 0.923, 0.922 |  |
| str.assign_short.view | std::string | G9 | 0.344 | 0.344 | 1.000 | 0.996–1.004 | 1.000, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.022 | 1.005 | 1.003–1.007 | 1.008, 1.003 | worse |
| vec.mid_insert | std::vector | G9 | 0.956 | 0.957 | 1.001 | 0.998–1.004 | 1.001, 1.001 |  |
| vec.push_back | std::vector | G9 | 1.109 | 1.100 | 0.992 | 0.988–0.996 | 1.001, 0.983 |  |
| vec.push_back_grow | std::vector | G9 | 0.767 | 0.769 | 1.003 | 1.001–1.006 | 1.012, 0.995 |  |
| hero.mid_erase.plain | std::vector | report | 4.062 | 4.041 | 0.995 | 0.991–0.998 | 0.998, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.224 | 4.192 | 0.992 | 0.989–0.996 | 0.991, 0.993 |  |
| hero.push_back_grow.plain | std::vector | report | 0.404 | 0.408 | 1.010 | 1.008–1.013 | 1.017, 1.004 | worse |
| map.erase.fresh | std::map | report | 0.746 | 0.737 | 0.989 | 0.986–0.992 | 0.980, 0.998 |  |
| map.erase.reused | std::flat_map | report | 1.083 | 1.081 | 0.998 | 0.994–1.001 | 0.995, 1.000 |  |
| map.erase.reused | std::map | report | 1.683 | 1.625 | 0.966 | 0.960–0.972 | 0.960, 0.971 |  |
| map.find.fresh | std::map | report | 0.325 | 0.325 | 1.002 | 0.998–1.007 | 1.001, 1.003 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.807 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.find.reused | std::map | report | 0.718 | 0.701 | 0.977 | 0.973–0.981 | 0.982, 0.971 |  |
| map.insert.fresh | std::map | report | 0.741 | 0.747 | 1.008 | 1.005–1.012 | 1.001, 1.016 | worse |
| map.insert.reused | std::flat_map | report | 1.000 | 1.001 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| map.insert.reused | std::map | report | 1.821 | 1.772 | 0.973 | 0.968–0.978 | 1.006, 0.941 |  |
| nested.build.plain | std | report | 0.337 | 0.331 | 0.982 | 0.976–0.988 | 0.992, 0.972 |  |
| set.erase.fresh | std::set | report | 0.677 | 0.663 | 0.979 | 0.976–0.983 | 0.976, 0.983 |  |
| set.erase.reused | std::flat_set | report | 1.142 | 1.144 | 1.002 | 0.998–1.006 | 1.000, 1.004 |  |
| set.erase.reused | std::set | report | 1.344 | 1.354 | 1.008 | 0.999–1.016 | 1.013, 1.002 |  |
| set.find.fresh | std::set | report | 0.299 | 0.297 | 0.991 | 0.987–0.996 | 0.991, 0.992 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.002 | 0.998, 1.002 |  |
| set.find.reused | std::set | report | 0.680 | 0.755 | 1.110 | 1.107–1.114 | 1.135, 1.086 | worse |
| set.insert.fresh | std::set | report | 0.654 | 0.654 | 1.001 | 0.999–1.004 | 1.005, 0.997 |  |
| set.insert.reused | std::flat_set | report | 1.079 | 1.079 | 1.000 | 0.997–1.003 | 0.998, 1.002 |  |
| set.insert.reused | std::set | report | 1.432 | 1.393 | 0.972 | 0.964–0.981 | 1.002, 0.944 |  |
| str.assign_short | std::string | report | 1.769 | 1.764 | 0.997 | 0.993–1.001 | 1.001, 0.994 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.149 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |

worse at t1 than t0 (by the rule): map.erase.fresh / std::flat_map (G9) 1.004; set.insert.fresh / std::flat_set (G9) 1.004; str.append / std::string (G9) 1.014; vec.mid_erase / std::vector (G9) 1.005; hero.push_back_grow.plain / std::vector (report) 1.010; map.insert.fresh / std::map (report) 1.008; set.find.reused / std::set (report) 1.110

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.6 | 163.4 | 0.999 | 0.997–1.000 | 0.996, 1.001 |  |
| hero.mid_erase.plain | 916.2 | 907.7 | 0.991 | 0.988–0.994 | 0.998, 0.983 |  |
| hero.mid_insert.ord | 228.5 | 227.6 | 0.996 | 0.994–0.998 | 0.995, 0.997 |  |
| hero.mid_insert.plain | 1270 | 1253 | 0.987 | 0.985–0.989 | 0.992, 0.982 |  |
| hero.push_back_grow.ord | 31.94 | 31.92 | 0.999 | 0.997–1.002 | 0.992, 1.007 |  |
| hero.push_back_grow.plain | 35.27 | 35.3 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| map.erase.fresh | 33.05 | 33.06 | 1.000 | 0.998–1.002 | 1.001, 1.000 |  |
| map.erase.reused | 29.77 | 29.66 | 0.996 | 0.994–0.998 | 0.995, 0.997 |  |
| map.find.fresh | 5.289 | 5.298 | 1.002 | 0.999–1.004 | 1.002, 1.001 |  |
| map.find.reused | 5.298 | 5.299 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| map.insert.fresh | 32.25 | 32.28 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| map.insert.reused | 29.5 | 29.49 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| nested.build.ord | 271.8 | 271 | 0.997 | 0.994–1.000 | 0.994, 1.001 |  |
| nested.build.plain | 279.4 | 279.9 | 1.002 | 0.997–1.006 | 0.999, 1.005 |  |
| reset | 30.38 | 29.13 | 0.959 | 0.955–0.963 | 0.939, 0.979 |  |
| set.erase.fresh | 29.79 | 29.84 | 1.002 | 1.000–1.004 | 1.002, 1.001 |  |
| set.erase.reused | 23.31 | 23.32 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| set.find.fresh | 4.885 | 4.881 | 0.999 | 0.998–1.000 | 0.999, 0.999 |  |
| set.find.reused | 4.879 | 4.885 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| set.insert.fresh | 28.92 | 28.87 | 0.998 | 0.997–1.000 | 0.998, 0.998 |  |
| set.insert.reused | 22.85 | 22.81 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| str.append | 1.56 | 1.561 | 1.001 | 0.996–1.005 | 1.006, 0.995 |  |
| str.assign_long | 2.959 | 2.716 | 0.918 | 0.916–0.920 | 0.923, 0.912 |  |
| str.assign_short | 0.9464 | 0.9354 | 0.988 | 0.983–0.993 | 1.001, 0.976 |  |
| str.assign_short.copy | 0.6281 | 0.6249 | 0.995 | 0.990–1.000 | 1.006, 0.984 |  |
| str.assign_short.view | 0.9487 | 0.9465 | 0.998 | 0.993–1.003 | 1.001, 0.994 |  |
| vec.iterate | 0.03317 | 0.03322 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.mid_erase | 40.35 | 40.31 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.mid_insert | 40.45 | 40.44 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| vec.push_back | 0.2931 | 0.2893 | 0.987 | 0.977–0.998 | 0.995, 0.979 |  |
| vec.push_back_grow | 0.2753 | 0.2761 | 1.003 | 0.998–1.008 | 1.008, 0.998 |  |

XOffset slower at t1 than t0 (worse by the rule): none

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.724 | 0.728 | 1.005 | 1.002–1.008 | 1.003, 1.006 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.756 | 0.995 | 0.991–0.998 | 0.997, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.358 | 0.359 | 1.003 | 0.999–1.007 | 0.982, 1.024 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.939 | 0.997 | 0.994–1.000 | 0.995, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.809 | 1.001 | 0.998–1.003 | 1.001, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.868 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| nested.build.ord | std | G9 | 0.310 | 0.307 | 0.991 | 0.984–0.997 | 0.977, 1.004 |  |
| reset | std | G9 | 0.239 | 0.210 | 0.878 | 0.864–0.892 | 0.821, 0.939 |  |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.088 | 1.000 | 0.997–1.003 | 0.995, 1.005 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.998 | 0.995–1.000 | 0.999, 0.997 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.997 | 1.001 | 0.998–1.004 | 0.998, 1.004 |  |
| str.append | std::string | G9 | 0.434 | 0.430 | 0.991 | 0.986–0.997 | 0.980, 1.003 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.867 | 1.003 | 1.001–1.006 | 1.003, 1.003 | worse |
| str.assign_short.view | std::string | G9 | 0.343 | 0.340 | 0.993 | 0.990–0.997 | 0.999, 0.988 |  |
| vec.iterate | std::vector | G9 | 1.002 | 1.003 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.023 | 1.006 | 1.004–1.009 | 1.004, 1.009 | worse |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.976 | 1.017 | 1.014–1.019 | 1.022, 1.012 | worse |
| vec.push_back | std::vector | G9 | 1.117 | 1.118 | 1.001 | 0.996–1.006 | 0.996, 1.006 |  |
| vec.push_back_grow | std::vector | G9 | 0.771 | 0.763 | 0.990 | 0.986–0.993 | 0.991, 0.988 |  |
| hero.mid_erase.plain | std::vector | report | 4.019 | 4.052 | 1.008 | 1.004–1.012 | 1.011, 1.006 | worse |
| hero.mid_insert.plain | std::vector | report | 4.161 | 4.174 | 1.003 | 1.000–1.007 | 1.005, 1.002 |  |
| hero.push_back_grow.plain | std::vector | report | 0.400 | 0.399 | 1.000 | 0.996–1.004 | 0.985, 1.014 |  |
| map.erase.fresh | std::map | report | 0.740 | 0.744 | 1.006 | 1.003–1.009 | 0.997, 1.015 |  |
| map.erase.reused | std::flat_map | report | 1.084 | 1.080 | 0.996 | 0.993–1.000 | 0.996, 0.997 |  |
| map.erase.reused | std::map | report | 1.613 | 1.570 | 0.973 | 0.966–0.981 | 0.958, 0.989 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 0.999 | 0.995–1.003 | 1.000, 0.998 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.003 | 1.001, 0.999 |  |
| map.find.reused | std::map | report | 0.719 | 0.718 | 0.997 | 0.993–1.002 | 0.978, 1.017 |  |
| map.insert.fresh | std::map | report | 0.743 | 0.749 | 1.009 | 1.005–1.013 | 0.993, 1.025 |  |
| map.insert.reused | std::flat_map | report | 1.006 | 1.000 | 0.994 | 0.992–0.997 | 0.993, 0.996 |  |
| map.insert.reused | std::map | report | 1.781 | 1.810 | 1.016 | 1.010–1.023 | 1.043, 0.990 | worse? |
| nested.build.plain | std | report | 0.328 | 0.318 | 0.971 | 0.965–0.977 | 0.977, 0.965 |  |
| set.erase.fresh | std::set | report | 0.669 | 0.675 | 1.009 | 1.006–1.012 | 1.003, 1.014 | worse |
| set.erase.reused | std::flat_set | report | 1.138 | 1.137 | 0.999 | 0.995–1.003 | 0.998, 1.000 |  |
| set.erase.reused | std::set | report | 1.338 | 1.288 | 0.962 | 0.955–0.970 | 0.948, 0.977 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.999 | 0.994–1.004 | 0.998, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.757 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| set.find.reused | std::set | report | 0.685 | 0.684 | 0.999 | 0.996–1.003 | 0.979, 1.021 |  |
| set.insert.fresh | std::set | report | 0.653 | 0.656 | 1.005 | 1.001–1.008 | 0.997, 1.012 |  |
| set.insert.reused | std::flat_set | report | 1.080 | 1.084 | 1.004 | 1.002–1.007 | 1.006, 1.003 | worse |
| set.insert.reused | std::set | report | 1.387 | 1.394 | 1.005 | 0.996–1.014 | 1.038, 0.973 |  |
| str.assign_short | std::string | report | 1.766 | 1.767 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.152 | 1.153 | 1.001 | 0.997–1.005 | 1.000, 1.002 |  |

worse at t1a than t0a (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; str.assign_long / std::string (G9) 1.003; vec.mid_erase / std::vector (G9) 1.006; vec.mid_insert / std::vector (G9) 1.017; hero.mid_erase.plain / std::vector (report) 1.008; set.erase.fresh / std::set (report) 1.009; set.insert.reused / std::flat_set (report) 1.004

above 1.01 but not worse by the rule: map.insert.reused / std::map (report) 1.016

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.4 | 164.3 | 0.999 | 0.998–1.001 | 0.995, 1.004 |  |
| hero.mid_erase.plain | 905.7 | 911.7 | 1.007 | 1.004–1.009 | 1.007, 1.006 | worse |
| hero.mid_insert.ord | 230.1 | 228.9 | 0.995 | 0.993–0.997 | 0.993, 0.996 |  |
| hero.mid_insert.plain | 1254 | 1255 | 1.001 | 0.999–1.003 | 1.004, 0.998 |  |
| hero.push_back_grow.ord | 32.01 | 31.69 | 0.990 | 0.988–0.992 | 0.984, 0.996 |  |
| hero.push_back_grow.plain | 35.48 | 35.13 | 0.990 | 0.988–0.992 | 0.988, 0.992 |  |
| map.erase.fresh | 33.04 | 33.01 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| map.erase.reused | 29.73 | 29.67 | 0.998 | 0.996–1.000 | 0.995, 1.001 |  |
| map.find.fresh | 5.293 | 5.294 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.find.reused | 5.306 | 5.308 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| map.insert.fresh | 32.25 | 32.3 | 1.001 | 0.999–1.003 | 1.000, 1.003 |  |
| map.insert.reused | 29.53 | 29.55 | 1.001 | 0.998–1.003 | 0.999, 1.002 |  |
| nested.build.ord | 268.6 | 267.2 | 0.995 | 0.993–0.997 | 0.990, 0.999 |  |
| nested.build.plain | 276.4 | 276.3 | 1.000 | 0.997–1.002 | 0.994, 1.006 |  |
| reset | 30.83 | 27.98 | 0.908 | 0.903–0.912 | 0.889, 0.927 |  |
| set.erase.fresh | 29.74 | 29.81 | 1.002 | 1.001–1.004 | 1.001, 1.003 | worse |
| set.erase.reused | 23.24 | 23.23 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| set.find.fresh | 4.885 | 4.887 | 1.000 | 0.998–1.002 | 1.001, 1.000 |  |
| set.find.reused | 4.886 | 4.886 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.insert.fresh | 28.9 | 28.97 | 1.002 | 1.000–1.005 | 1.002, 1.002 | worse |
| set.insert.reused | 22.79 | 22.89 | 1.004 | 1.002–1.006 | 1.003, 1.005 | worse |
| str.append | 1.572 | 1.565 | 0.995 | 0.990–1.000 | 0.992, 0.999 |  |
| str.assign_long | 2.952 | 2.949 | 0.999 | 0.996–1.002 | 1.001, 0.997 |  |
| str.assign_short | 0.9411 | 0.9482 | 1.008 | 1.003–1.012 | 1.011, 1.005 | worse |
| str.assign_short.copy | 0.6278 | 0.6326 | 1.008 | 1.002–1.013 | 1.009, 1.007 | worse |
| str.assign_short.view | 0.9484 | 0.9536 | 1.005 | 1.001–1.010 | 1.010, 1.001 | worse |
| vec.iterate | 0.03326 | 0.0334 | 1.004 | 1.002–1.007 | 1.005, 1.003 | worse |
| vec.mid_erase | 40.37 | 40.62 | 1.006 | 1.005–1.008 | 1.003, 1.009 | worse |
| vec.mid_insert | 40.67 | 40.69 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| vec.push_back | 0.2725 | 0.2722 | 0.999 | 0.996–1.002 | 0.998, 1.000 |  |
| vec.push_back_grow | 0.272 | 0.2697 | 0.991 | 0.989–0.994 | 0.991, 0.992 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.007; set.erase.fresh 1.002; set.insert.fresh 1.002; set.insert.reused 1.004; str.assign_short 1.008; str.assign_short.copy 1.008; str.assign_short.view 1.005; vec.iterate 1.004; vec.mid_erase 1.006

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.push_back_grow.plain / std::vector (report) | 1.010 | 1.000 | 0.996–1.004 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.004 | 0.997 | 0.994–1.000 | not worse |
| map.insert.fresh / std::map (report) | 1.008 | 1.009 | 1.005–1.013 | not worse |
| set.find.reused / std::set (report) | 1.110 | 0.999 | 0.996–1.003 | not worse |
| set.insert.fresh / std::flat_set (G9) | 1.004 | 1.001 | 0.998–1.004 | not worse |
| str.append / std::string (G9) | 1.014 | 0.991 | 0.986–0.997 | not worse |
| vec.mid_erase / std::vector (G9) | 1.005 | 1.006 | 1.004–1.009 | still worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.49 | 162.93 | 0.997 | 225.05 | 0.998 |
| hero.mid_erase.plain | 910.92 | 904.43 | 0.993 | 224.56 | 1.007 |
| hero.mid_insert.ord | 228.15 | 227.13 | 0.996 | 297.76 | 0.995 |
| hero.mid_insert.plain | 1261.27 | 1250.02 | 0.991 | 303.71 | 1.000 |
| hero.push_back_grow.ord | 31.86 | 31.78 | 0.997 | 86.97 | 0.991 |
| hero.push_back_grow.plain | 35.12 | 35.23 | 1.003 | 87.20 | 0.991 |
| map.erase.fresh | 33.02 | 33.01 | 1.000 | 35.20 | 1.000 |
| map.erase.reused | 29.69 | 29.57 | 0.996 | 27.36 | 0.998 |
| map.find.fresh | 5.28 | 5.28 | 1.000 | 6.55 | 1.001 |
| map.find.reused | 5.29 | 5.29 | 0.999 | 6.55 | 1.000 |
| map.insert.fresh | 32.23 | 32.22 | 1.000 | 37.00 | 1.002 |
| map.insert.reused | 29.46 | 29.41 | 0.998 | 29.38 | 1.000 |
| nested.build.ord | 270.98 | 270.81 | 0.999 | 828.24 | 0.994 |
| nested.build.plain | 278.15 | 279.11 | 1.003 | 826.08 | 0.998 |
| reset | 30.26 | 29.03 | 0.959 | 118.88 | 0.908 |
| set.erase.fresh | 29.78 | 29.76 | 1.000 | 27.42 | 1.001 |
| set.erase.reused | 23.06 | 23.09 | 1.001 | 20.32 | 1.000 |
| set.find.fresh | 4.88 | 4.88 | 1.001 | 6.46 | 1.001 |
| set.find.reused | 4.87 | 4.88 | 1.001 | 6.46 | 1.000 |
| set.insert.fresh | 28.89 | 28.85 | 0.999 | 28.97 | 1.001 |
| set.insert.reused | 22.83 | 22.78 | 0.998 | 21.14 | 1.005 |
| str.append | 1.55 | 1.53 | 0.990 | 3.69 | 1.003 |
| str.assign_long | 2.94 | 2.71 | 0.924 | 3.39 | 1.000 |
| str.assign_short | 0.94 | 0.92 | 0.975 | 0.53 | 0.999 |
| str.assign_short.copy | 0.62 | 0.61 | 0.982 | 0.54 | 0.995 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 1.001 |
| vec.iterate | 0.03 | 0.03 | 1.000 | 0.03 | 1.002 |
| vec.mid_erase | 40.21 | 40.21 | 1.000 | 39.62 | 1.007 |
| vec.mid_insert | 40.37 | 40.37 | 1.000 | 42.24 | 1.000 |
| vec.push_back | 0.29 | 0.29 | 0.985 | 0.26 | 1.000 |
| vec.push_back_grow | 0.28 | 0.27 | 0.997 | 0.36 | 0.990 |

## analyze t0 (G9 at the anchor 49ce3c68)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.109 |  1.112 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.769 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.956 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.55 |  1.017 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     224.61 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     298.17 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      87.32 |  0.366 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     224.61 |  4.062 |  4.075 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1262.38 |     304.69 |  4.224 |  4.241 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.16 |      87.32 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.775 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.867 |  0.869 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.67 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.13 |  0.741 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.24 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.746 |  0.748 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.19 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.95 |  0.677 |  0.678 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.000 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.821 |  1.829 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.29 |  0.718 |  0.721 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.70 |      27.34 |  1.083 |  1.087 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.70 |      17.42 |  1.683 |  1.693 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.16 |  1.079 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.62 |  1.432 |  1.445 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.23 |  0.680 |  0.682 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.142 |  1.145 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.344 |  1.356 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     828.12 |  0.327 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     119.14 |  0.254 |  0.256 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     825.53 |  0.337 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.26 |  1.100 |  1.104 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.24 |      39.47 |  1.022 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.95 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.22 |  0.756 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      87.12 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.30 |     224.61 |  4.041 |  4.056 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1250.00 |     304.03 |  4.192 |  4.206 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.83 |  0.408 |  0.409 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.53 |  1.149 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.66 |  0.425 |  0.427 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.89 |  0.747 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.08 |  0.940 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.43 |  0.737 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.95 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.752 |  0.754 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.48 |  0.297 |  0.298 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.087 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.60 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.38 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.38 |      16.36 |  1.772 |  1.781 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.55 |  0.701 |  0.704 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.081 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.625 |  1.635 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.079 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.95 |  1.393 |  1.406 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.44 |  0.755 |  0.758 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.144 |  1.150 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.354 |  1.365 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     835.94 |  0.318 |  0.320 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     118.48 |  0.237 |  0.240 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     830.72 |  0.331 |  0.334 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T08:51:21Z block 1 start: waited 180s load 1.33 WindowServer 1.2% remote 6
2026-11-04T08:54:25Z block 2 start: waited 180s load 1.27 WindowServer 1.2% remote 6
2026-11-04T09:02:03Z block 3 start: waited 450s load 1.70 WindowServer 42.1% remote 6
2026-11-04T09:05:07Z block 4 start: waited 180s load 1.93 WindowServer 43.6% remote 6
```
