# G9R3 result (2026-11-04T07:15:46Z)

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
| hero.mid_erase.ord | std::vector | G9 | 0.724 | 0.725 | 1.001 | 0.999–1.004 | 0.996, 1.007 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.762 | 0.754 | 0.991 | 0.987–0.994 | 0.992, 0.989 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.368 | 0.363 | 0.985 | 0.983–0.988 | 0.979, 0.991 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.938 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.002 | 1.000–1.004 | 1.001, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.869 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| nested.build.ord | std | G9 | 0.339 | 0.327 | 0.966 | 0.957–0.975 | 0.920, 1.015 |  |
| reset | std | G9 | 0.256 | 0.228 | 0.889 | 0.877–0.900 | 0.796, 0.992 |  |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.084 | 0.996 | 0.994–0.998 | 0.994, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.755 | 1.002 | 0.998–1.005 | 1.003, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.999 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| str.append | std::string | G9 | 0.416 | 0.426 | 1.025 | 1.020–1.030 | 1.026, 1.024 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.800 | 0.924 | 0.922–0.927 | 0.924, 0.925 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.344 | 1.003 | 1.001–1.006 | 1.003, 1.004 | worse |
| vec.iterate | std::vector | G9 | 0.999 | 1.000 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.019 | 0.999 | 0.996–1.001 | 0.999, 0.998 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.958 | 1.003 | 1.000–1.006 | 1.002, 1.005 | worse |
| vec.push_back | std::vector | G9 | 1.114 | 1.105 | 0.992 | 0.989–0.995 | 0.994, 0.990 |  |
| vec.push_back_grow | std::vector | G9 | 0.768 | 0.769 | 1.001 | 0.997–1.005 | 1.005, 0.997 |  |
| hero.mid_erase.plain | std::vector | report | 4.061 | 4.017 | 0.989 | 0.986–0.992 | 0.988, 0.990 |  |
| hero.mid_insert.plain | std::vector | report | 4.242 | 4.188 | 0.987 | 0.984–0.991 | 0.993, 0.982 |  |
| hero.push_back_grow.plain | std::vector | report | 0.407 | 0.402 | 0.988 | 0.986–0.991 | 0.986, 0.991 |  |
| map.erase.fresh | std::map | report | 0.747 | 0.739 | 0.989 | 0.986–0.992 | 0.978, 0.999 |  |
| map.erase.reused | std::flat_map | report | 1.073 | 1.080 | 1.006 | 0.995–1.017 | 1.017, 0.995 |  |
| map.erase.reused | std::map | report | 1.625 | 1.611 | 0.991 | 0.980–1.002 | 1.010, 0.972 |  |
| map.find.fresh | std::map | report | 0.324 | 0.326 | 1.005 | 1.001–1.009 | 1.002, 1.007 | worse |
| map.find.reused | std::flat_map | report | 0.808 | 0.809 | 1.002 | 1.000–1.004 | 1.000, 1.003 |  |
| map.find.reused | std::map | report | 0.707 | 0.697 | 0.986 | 0.981–0.991 | 0.981, 0.990 |  |
| map.insert.fresh | std::map | report | 0.743 | 0.746 | 1.004 | 1.001–1.008 | 1.008, 1.000 | worse |
| map.insert.reused | std::flat_map | report | 1.005 | 1.001 | 0.996 | 0.993–1.000 | 0.997, 0.995 |  |
| map.insert.reused | std::map | report | 1.836 | 1.811 | 0.987 | 0.982–0.991 | 0.978, 0.995 |  |
| nested.build.plain | std | report | 0.338 | 0.337 | 0.998 | 0.995–1.001 | 0.986, 1.009 |  |
| set.erase.fresh | std::set | report | 0.680 | 0.663 | 0.976 | 0.973–0.979 | 0.962, 0.990 |  |
| set.erase.reused | std::flat_set | report | 1.140 | 1.137 | 0.998 | 0.993–1.002 | 0.996, 1.000 |  |
| set.erase.reused | std::set | report | 1.329 | 1.341 | 1.010 | 1.002–1.018 | 0.995, 1.025 |  |
| set.find.fresh | std::set | report | 0.300 | 0.297 | 0.993 | 0.988–0.998 | 0.993, 0.993 |  |
| set.find.reused | std::flat_set | report | 0.761 | 0.756 | 0.993 | 0.986–1.000 | 0.984, 1.003 |  |
| set.find.reused | std::set | report | 0.685 | 0.764 | 1.115 | 1.104–1.126 | 1.106, 1.125 | worse |
| set.insert.fresh | std::set | report | 0.654 | 0.654 | 0.999 | 0.997–1.002 | 0.997, 1.002 |  |
| set.insert.reused | std::flat_set | report | 1.079 | 1.079 | 0.999 | 0.993–1.005 | 1.000, 0.999 |  |
| set.insert.reused | std::set | report | 1.372 | 1.421 | 1.035 | 1.021–1.050 | 1.072, 1.000 | worse? |
| str.assign_short | std::string | report | 1.761 | 1.771 | 1.006 | 1.001–1.010 | 1.008, 1.003 | worse |
| str.assign_short.copy | std::string | report | 1.149 | 1.150 | 1.001 | 0.997–1.004 | 0.997, 1.004 |  |

worse at t1 than t0 (by the rule): str.append / std::string (G9) 1.025; str.assign_short.view / std::string (G9) 1.003; vec.mid_insert / std::vector (G9) 1.003; map.find.fresh / std::map (report) 1.005; map.insert.fresh / std::map (report) 1.004; set.find.reused / std::set (report) 1.115; str.assign_short / std::string (report) 1.006

above 1.01 but not worse by the rule: set.insert.reused / std::set (report) 1.035

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.5 | 163.2 | 0.998 | 0.997–0.999 | 0.996, 1.000 |  |
| hero.mid_erase.plain | 914.2 | 910.6 | 0.996 | 0.993–0.999 | 0.993, 0.999 |  |
| hero.mid_insert.ord | 227.9 | 227.1 | 0.997 | 0.995–0.999 | 0.998, 0.995 |  |
| hero.mid_insert.plain | 1264 | 1261 | 0.998 | 0.995–1.000 | 1.001, 0.995 |  |
| hero.push_back_grow.ord | 31.81 | 31.84 | 1.001 | 0.999–1.002 | 0.995, 1.006 |  |
| hero.push_back_grow.plain | 35.13 | 35.31 | 1.005 | 1.003–1.007 | 1.003, 1.007 | worse |
| map.erase.fresh | 33.02 | 33.05 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| map.erase.reused | 31.42 | 29.61 | 0.942 | 0.929–0.956 | 0.891, 0.997 |  |
| map.find.fresh | 5.287 | 5.287 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.find.reused | 5.298 | 5.311 | 1.002 | 1.001–1.004 | 1.002, 1.003 | worse |
| map.insert.fresh | 32.23 | 32.21 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.reused | 29.65 | 29.5 | 0.995 | 0.992–0.998 | 0.992, 0.998 |  |
| nested.build.ord | 302.4 | 271.7 | 0.899 | 0.878–0.920 | 0.800, 1.010 |  |
| nested.build.plain | 279 | 277.9 | 0.996 | 0.994–0.998 | 0.991, 1.001 |  |
| reset | 30.49 | 28.98 | 0.951 | 0.942–0.959 | 0.915, 0.988 |  |
| set.erase.fresh | 29.92 | 29.76 | 0.995 | 0.992–0.997 | 0.992, 0.998 |  |
| set.erase.reused | 23.43 | 23.18 | 0.989 | 0.983–0.996 | 0.980, 0.999 |  |
| set.find.fresh | 4.903 | 4.89 | 0.997 | 0.995–1.000 | 0.993, 1.002 |  |
| set.find.reused | 5.479 | 4.885 | 0.891 | 0.869–0.915 | 0.793, 1.002 |  |
| set.insert.fresh | 28.94 | 28.86 | 0.997 | 0.995–0.999 | 0.995, 0.999 |  |
| set.insert.reused | 25.21 | 22.8 | 0.904 | 0.888–0.921 | 0.819, 0.998 |  |
| str.append | 1.554 | 1.577 | 1.015 | 1.010–1.019 | 1.012, 1.018 | worse |
| str.assign_long | 2.95 | 2.749 | 0.932 | 0.929–0.935 | 0.928, 0.935 |  |
| str.assign_short | 0.9424 | 0.9574 | 1.016 | 1.010–1.021 | 1.015, 1.017 | worse |
| str.assign_short.copy | 0.6259 | 0.6346 | 1.014 | 1.009–1.019 | 0.999, 1.029 | worse? |
| str.assign_short.view | 0.9483 | 0.9554 | 1.008 | 1.003–1.012 | 1.002, 1.013 | worse |
| vec.iterate | 0.03319 | 0.03314 | 0.998 | 0.997–0.999 | 0.997, 0.999 |  |
| vec.mid_erase | 40.33 | 40.27 | 0.999 | 0.996–1.001 | 0.997, 1.000 |  |
| vec.mid_insert | 40.47 | 40.42 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| vec.push_back | 0.2881 | 0.2917 | 1.013 | 1.002–1.024 | 1.013, 1.012 | worse |
| vec.push_back_grow | 0.2748 | 0.2788 | 1.015 | 1.009–1.021 | 1.017, 1.012 | worse |

XOffset slower at t1 than t0 (worse by the rule): hero.push_back_grow.plain 1.005; map.find.reused 1.002; str.append 1.015; str.assign_short 1.016; str.assign_short.view 1.008; vec.push_back 1.013; vec.push_back_grow 1.015

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.723 | 0.995 | 0.991–0.999 | 0.991, 0.999 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.762 | 1.003 | 1.000–1.007 | 0.999, 1.007 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.353 | 0.364 | 1.033 | 1.028–1.037 | 1.020, 1.046 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.941 | 1.003 | 1.000–1.006 | 0.999, 1.007 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.998–1.004 | 0.999, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.870 | 1.002 | 1.000–1.005 | 1.000, 1.005 |  |
| nested.build.ord | std | G9 | 0.326 | 0.323 | 0.993 | 0.990–0.997 | 0.992, 0.995 |  |
| reset | std | G9 | 0.248 | 0.222 | 0.897 | 0.885–0.909 | 0.923, 0.872 |  |
| set.erase.fresh | std::flat_set | G9 | 1.092 | 1.090 | 0.998 | 0.994–1.002 | 0.997, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 1.000 | 1.001 | 0.998–1.003 | 1.001, 1.001 |  |
| str.append | std::string | G9 | 0.432 | 0.422 | 0.978 | 0.973–0.983 | 0.960, 0.997 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.866 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.342 | 0.998 | 0.995–1.001 | 1.000, 0.996 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.019 | 1.000 | 0.998–1.002 | 1.002, 0.999 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.979 | 1.020 | 1.016–1.024 | 1.017, 1.023 | worse |
| vec.push_back | std::vector | G9 | 1.118 | 1.118 | 1.000 | 0.997–1.003 | 1.003, 0.997 |  |
| vec.push_back_grow | std::vector | G9 | 0.774 | 0.767 | 0.991 | 0.987–0.995 | 0.994, 0.987 |  |
| hero.mid_erase.plain | std::vector | report | 4.015 | 4.032 | 1.004 | 0.999–1.009 | 1.009, 0.999 |  |
| hero.mid_insert.plain | std::vector | report | 4.147 | 4.183 | 1.009 | 1.005–1.012 | 1.003, 1.014 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.391 | 0.408 | 1.043 | 1.038–1.047 | 1.030, 1.056 | worse |
| map.erase.fresh | std::map | report | 0.729 | 0.739 | 1.014 | 1.010–1.017 | 1.019, 1.009 | worse |
| map.erase.reused | std::flat_map | report | 1.081 | 1.082 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| map.erase.reused | std::map | report | 1.621 | 1.577 | 0.973 | 0.966–0.980 | 0.969, 0.976 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.995 | 0.991–0.999 | 0.996, 0.995 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| map.find.reused | std::map | report | 0.736 | 0.741 | 1.007 | 1.003–1.011 | 1.013, 1.001 | worse |
| map.insert.fresh | std::map | report | 0.739 | 0.750 | 1.016 | 1.012–1.019 | 1.003, 1.028 | worse |
| map.insert.reused | std::flat_map | report | 1.010 | 1.001 | 0.991 | 0.989–0.994 | 0.994, 0.988 |  |
| map.insert.reused | std::map | report | 1.844 | 1.826 | 0.990 | 0.985–0.995 | 0.966, 1.015 |  |
| nested.build.plain | std | report | 0.334 | 0.337 | 1.007 | 1.004–1.011 | 1.008, 1.006 | worse |
| set.erase.fresh | std::set | report | 0.670 | 0.667 | 0.995 | 0.991–0.999 | 1.005, 0.985 |  |
| set.erase.reused | std::flat_set | report | 1.143 | 1.144 | 1.001 | 0.997–1.004 | 1.000, 1.002 |  |
| set.erase.reused | std::set | report | 1.356 | 1.309 | 0.966 | 0.958–0.973 | 0.966, 0.966 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.997 | 0.993–1.002 | 0.995, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.find.reused | std::set | report | 0.689 | 0.690 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| set.insert.fresh | std::set | report | 0.647 | 0.655 | 1.011 | 1.008–1.015 | 0.998, 1.024 | worse? |
| set.insert.reused | std::flat_set | report | 1.082 | 1.080 | 0.999 | 0.996–1.002 | 1.000, 0.998 |  |
| set.insert.reused | std::set | report | 1.463 | 1.459 | 0.998 | 0.989–1.006 | 0.999, 0.997 |  |
| str.assign_short | std::string | report | 1.765 | 1.768 | 1.002 | 0.999–1.005 | 1.005, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.150 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.033; vec.mid_insert / std::vector (G9) 1.020; hero.mid_insert.plain / std::vector (report) 1.009; hero.push_back_grow.plain / std::vector (report) 1.043; map.erase.fresh / std::map (report) 1.014; map.find.reused / std::map (report) 1.007; map.insert.fresh / std::map (report) 1.016; nested.build.plain / std (report) 1.007

above 1.01 but not worse by the rule: set.insert.fresh / std::set (report) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 165.6 | 163.9 | 0.990 | 0.986–0.994 | 0.990, 0.990 |  |
| hero.mid_erase.plain | 906.5 | 914.8 | 1.009 | 1.006–1.012 | 1.016, 1.003 | worse |
| hero.mid_insert.ord | 229.8 | 228.6 | 0.995 | 0.993–0.997 | 0.995, 0.995 |  |
| hero.mid_insert.plain | 1245 | 1261 | 1.013 | 1.010–1.015 | 1.011, 1.015 | worse |
| hero.push_back_grow.ord | 31.78 | 31.64 | 0.995 | 0.993–0.998 | 0.991, 1.000 |  |
| hero.push_back_grow.plain | 35.26 | 35.31 | 1.001 | 0.999–1.004 | 0.994, 1.009 |  |
| map.erase.fresh | 33.08 | 32.99 | 0.997 | 0.996–0.999 | 0.999, 0.996 |  |
| map.erase.reused | 29.63 | 29.73 | 1.003 | 1.001–1.006 | 1.004, 1.003 | worse |
| map.find.fresh | 5.308 | 5.288 | 0.996 | 0.994–0.999 | 0.998, 0.995 |  |
| map.find.reused | 5.308 | 5.307 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.fresh | 32.22 | 32.28 | 1.002 | 1.000–1.004 | 1.002, 1.002 |  |
| map.insert.reused | 29.61 | 29.46 | 0.995 | 0.994–0.997 | 0.995, 0.996 |  |
| nested.build.ord | 267.6 | 266.9 | 0.997 | 0.995–1.000 | 0.997, 0.997 |  |
| nested.build.plain | 276.9 | 277.3 | 1.001 | 0.999–1.004 | 1.010, 0.993 |  |
| reset | 30.46 | 28.7 | 0.942 | 0.936–0.948 | 0.946, 0.938 |  |
| set.erase.fresh | 29.8 | 29.82 | 1.001 | 0.998–1.004 | 0.998, 1.003 |  |
| set.erase.reused | 23.29 | 23.3 | 1.001 | 0.997–1.004 | 1.000, 1.002 |  |
| set.find.fresh | 4.887 | 4.884 | 0.999 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.88 | 4.881 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| set.insert.fresh | 28.93 | 28.94 | 1.000 | 0.998–1.002 | 1.003, 0.998 |  |
| set.insert.reused | 22.86 | 22.85 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| str.append | 1.571 | 1.561 | 0.994 | 0.989–0.999 | 0.993, 0.995 |  |
| str.assign_long | 2.943 | 2.966 | 1.008 | 1.005–1.011 | 1.007, 1.008 | worse |
| str.assign_short | 0.9413 | 0.957 | 1.017 | 1.012–1.021 | 1.029, 1.005 | worse |
| str.assign_short.copy | 0.6309 | 0.6342 | 1.005 | 1.000–1.010 | 1.014, 0.996 |  |
| str.assign_short.view | 0.9513 | 0.9555 | 1.004 | 1.000–1.009 | 1.013, 0.996 |  |
| vec.iterate | 0.03327 | 0.03325 | 0.999 | 0.999–1.000 | 0.999, 1.000 |  |
| vec.mid_erase | 40.53 | 40.51 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| vec.mid_insert | 40.81 | 40.82 | 1.000 | 0.997–1.003 | 0.998, 1.003 |  |
| vec.push_back | 0.2722 | 0.2719 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.push_back_grow | 0.2746 | 0.2697 | 0.982 | 0.979–0.986 | 0.990, 0.975 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.009; hero.mid_insert.plain 1.013; map.erase.reused 1.003; str.assign_long 1.008; str.assign_short 1.017

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.push_back_grow.plain (XOffset ns/op) | 1.005 | 1.001 | 0.999–1.004 | not worse |
| map.find.reused (XOffset ns/op) | 1.002 | 1.000 | 0.998–1.002 | not worse |
| str.append (XOffset ns/op) | 1.015 | 0.994 | 0.989–0.999 | not worse |
| str.assign_short (XOffset ns/op) | 1.016 | 1.017 | 1.012–1.021 | still worse |
| str.assign_short.view (XOffset ns/op) | 1.008 | 1.004 | 1.000–1.009 | not worse |
| vec.push_back (XOffset ns/op) | 1.013 | 0.999 | 0.997–1.001 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.015 | 0.982 | 0.979–0.986 | not worse |
| map.find.fresh / std::map (report) | 1.005 | 0.995 | 0.991–0.999 | not worse |
| map.insert.fresh / std::map (report) | 1.004 | 1.016 | 1.012–1.019 | still worse |
| set.find.reused / std::set (report) | 1.115 | 1.001 | 0.998–1.004 | not worse |
| str.append / std::string (G9) | 1.025 | 0.978 | 0.973–0.983 | not worse |
| str.assign_short / std::string (report) | 1.006 | 1.002 | 0.999–1.005 | not worse |
| str.assign_short.view / std::string (G9) | 1.003 | 0.998 | 0.995–1.001 | not worse |
| vec.mid_insert / std::vector (G9) | 1.003 | 1.020 | 1.016–1.024 | still worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.49 | 163.16 | 0.998 | 225.33 | 0.998 |
| hero.mid_erase.plain | 912.01 | 907.15 | 0.995 | 224.45 | 1.004 |
| hero.mid_insert.ord | 227.70 | 226.64 | 0.995 | 294.85 | 0.996 |
| hero.mid_insert.plain | 1261.32 | 1253.06 | 0.993 | 296.76 | 1.008 |
| hero.push_back_grow.ord | 31.74 | 31.75 | 1.000 | 86.32 | 0.996 |
| hero.push_back_grow.plain | 35.07 | 35.22 | 1.004 | 86.25 | 0.999 |
| map.erase.fresh | 33.03 | 33.02 | 1.000 | 35.11 | 0.998 |
| map.erase.reused | 32.56 | 29.56 | 0.908 | 30.17 | 1.002 |
| map.find.fresh | 5.29 | 5.28 | 0.999 | 6.55 | 1.000 |
| map.find.reused | 5.29 | 5.30 | 1.001 | 6.55 | 1.001 |
| map.insert.fresh | 32.22 | 32.20 | 1.000 | 37.02 | 1.001 |
| map.insert.reused | 29.48 | 29.47 | 1.000 | 29.37 | 0.995 |
| nested.build.ord | 308.50 | 271.31 | 0.879 | 907.09 | 0.996 |
| nested.build.plain | 278.88 | 277.89 | 0.996 | 826.14 | 1.002 |
| reset | 30.08 | 28.88 | 0.960 | 117.65 | 0.939 |
| set.erase.fresh | 29.77 | 29.76 | 1.000 | 27.41 | 1.001 |
| set.erase.reused | 23.04 | 22.98 | 0.997 | 20.34 | 1.000 |
| set.find.fresh | 4.88 | 4.88 | 1.001 | 6.47 | 1.000 |
| set.find.reused | 5.73 | 4.88 | 0.851 | 7.51 | 1.000 |
| set.insert.fresh | 28.89 | 28.84 | 0.998 | 28.96 | 1.000 |
| set.insert.reused | 25.12 | 22.76 | 0.906 | 23.19 | 1.000 |
| str.append | 1.53 | 1.57 | 1.028 | 3.68 | 1.000 |
| str.assign_long | 2.93 | 2.71 | 0.925 | 3.39 | 1.001 |
| str.assign_short | 0.93 | 0.95 | 1.023 | 0.53 | 1.037 |
| str.assign_short.copy | 0.61 | 0.64 | 1.043 | 0.53 | 1.001 |
| str.assign_short.view | 0.93 | 0.93 | 1.003 | 2.71 | 0.999 |
| vec.iterate | 0.03 | 0.03 | 0.995 | 0.03 | 0.999 |
| vec.mid_erase | 40.14 | 40.17 | 1.001 | 39.49 | 1.004 |
| vec.mid_insert | 40.38 | 40.39 | 1.000 | 42.24 | 1.002 |
| vec.push_back | 0.29 | 0.29 | 1.008 | 0.26 | 1.001 |
| vec.push_back_grow | 0.27 | 0.28 | 1.012 | 0.36 | 0.982 |

## analyze t0 (G9 at the anchor 49ce3c68)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.114 |  1.117 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.768 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.999 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.020 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.91 |  0.724 |  0.726 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.27 |  0.762 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.22 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.81 |     224.61 |  4.061 |  4.074 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1260.42 |     303.39 |  4.242 |  4.256 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.12 |      86.14 |  0.407 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.761 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.69 |  0.416 |  0.418 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.868 |  0.869 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.743 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.36 |  0.324 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.938 |  0.940 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.03 |  0.747 |  0.749 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.997 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.11 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.754 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.088 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.86 |  0.680 |  0.682 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.005 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.836 |  1.846 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.61 |  0.707 |  0.710 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.073 |  1.090 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.42 |  1.625 |  1.649 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.16 |  1.079 |  1.088 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.46 |  1.372 |  1.397 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.761 |  0.769 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.18 |  0.685 |  0.694 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.140 |  1.147 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.01 |  1.329 |  1.342 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     824.22 |  0.339 |  0.343 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.19 |  0.256 |  0.258 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     824.22 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.105 |  1.110 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.769 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.958 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.019 |  1.021 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.61 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     298.19 |  0.754 |  0.757 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      87.02 |  0.363 |  0.364 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    905.59 |     225.25 |  4.017 |  4.029 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1252.61 |     304.69 |  4.188 |  4.204 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      87.04 |  0.402 |  0.403 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.771 |  1.777 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.66 |       0.57 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.57 |       3.68 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.869 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.746 |  0.749 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.938 |  0.940 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.35 |  0.739 |  0.742 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.48 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.51 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.811 |  1.818 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.809 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.71 |  0.697 |  0.700 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.080 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.98 |  1.611 |  1.621 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.079 |  1.081 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.79 |  1.421 |  1.432 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.44 |  0.764 |  0.766 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.137 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.341 |  1.349 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     830.72 |  0.327 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.19 |  0.228 |  0.232 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     825.50 |  0.337 |  0.338 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T07:06:28Z block 1 start: waited 180s load 1.87 WindowServer 42.2% remote 6
2026-11-04T07:09:33Z block 2 start: waited 180s load 1.95 WindowServer 4.8% remote 6
2026-11-04T07:12:37Z block 3 start: waited 180s load 2.00 WindowServer 29.7% remote 6
2026-11-04T07:15:42Z block 4 start: waited 180s load 2.14 WindowServer 4.6% remote 6
```
