# G9R3 result (2026-11-11T16:56:38Z)

t0 cf9b63e584247ca6c61f524591557d468062bb48 (next_cpp26 cf9b63e5 (first parent of #266)); t1 bb537d0db6a7d629f10165e9cc68aadc745b5171 (PR266 head (bb537d0d, #266 merged)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 cf9b63e584247ca6c61f524591557d468062bb48 tree aa3418684e6eb23af02981d9556a115f0262cd21 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 cf9b63e5 (first parent of #266))
t1 bb537d0db6a7d629f10165e9cc68aadc745b5171 tree a52e26a113f33216c2a5009a95bb164dc2275468 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (PR266 head (bb537d0d, #266 merged))
t1: diff over t0 sha256 3233cad543b8a9c06f9253ccae7ac3d8427ec36601b328bd2ee28cfb27327b3f ( 6 files changed, 113 insertions(+), 14 deletions(-))
t1: library header diff over t0:  1 file changed, 41 insertions(+), 9 deletions(-)
t1: first-parent commits over t0:
  bb537d0d Merge #266: fix #265 free-table load growth (SLG M D3) + root<T>() cache refill, redo of #223 (L class)
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 68e3fd020ff33c782a01b02c56b7971de54da6b6c90a4d7ee37193305097b8e3
reloc_bench t1 sha256 e8ee9dd2c490589797a5c4c9803817eb63db0e9d17d92ccc0202fa68d8897021
reloc_bench t0a sha256 b36f3ab4d44bd7247743cc9572d5cdcb05b36f99b815d08a00f0fbc820ad4cf9
reloc_bench t1a sha256 d5b8500dbfafd44e2f8cbf4e12062c6126d4a6f500ab00c841f98356a8155a69
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.738 | 0.735 | 0.997 | 0.994–1.000 | 0.996, 0.998 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.760 | 1.001 | 0.998–1.005 | 1.006, 0.996 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.357 | 0.347 | 0.971 | 0.969–0.973 | 0.974, 0.968 |  |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.940 | 1.003 | 1.000–1.006 | 1.004, 1.002 | worse |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.808 | 1.000 | 0.997–1.003 | 1.001, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.846 | 0.846 | 1.000 | 0.998–1.003 | 0.998, 1.003 |  |
| nested.build.ord | std | G9 | 0.309 | 0.316 | 1.022 | 1.014–1.029 | 1.007, 1.036 | worse |
| reset | std | G9 | 0.196 | 0.207 | 1.056 | 1.044–1.069 | 1.041, 1.072 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.086 | 1.088 | 1.001 | 0.998–1.005 | 1.002, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.757 | 0.755 | 0.998 | 0.995–1.000 | 0.997, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.985 | 0.989 | 1.004 | 1.002–1.006 | 1.003, 1.005 | worse |
| str.append | std::string | G9 | 0.420 | 0.428 | 1.019 | 1.014–1.023 | 1.015, 1.022 | worse |
| str.assign_long | std::string | G9 | 0.799 | 0.865 | 1.082 | 1.080–1.084 | 1.079, 1.085 | worse |
| str.assign_short.view | std::string | G9 | 0.344 | 0.343 | 0.997 | 0.994–1.000 | 0.998, 0.997 |  |
| vec.iterate | std::vector | G9 | 0.982 | 1.001 | 1.019 | 1.016–1.023 | 1.022, 1.017 | worse |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.029 | 1.009 | 1.007–1.011 | 1.009, 1.009 | worse |
| vec.mid_insert | std::vector | G9 | 0.996 | 0.990 | 0.995 | 0.992–0.997 | 0.995, 0.994 |  |
| vec.push_back | std::vector | G9 | 1.143 | 1.118 | 0.978 | 0.975–0.981 | 0.971, 0.985 |  |
| vec.push_back_grow | std::vector | G9 | 0.765 | 0.761 | 0.995 | 0.992–0.999 | 0.996, 0.995 |  |
| hero.mid_erase.plain | std::vector | report | 4.075 | 4.054 | 0.995 | 0.992–0.998 | 0.993, 0.997 |  |
| hero.mid_insert.plain | std::vector | report | 4.250 | 4.205 | 0.990 | 0.987–0.993 | 0.994, 0.985 |  |
| hero.push_back_grow.plain | std::vector | report | 0.394 | 0.391 | 0.992 | 0.990–0.995 | 0.996, 0.988 |  |
| map.erase.fresh | std::map | report | 0.732 | 0.735 | 1.004 | 1.001–1.007 | 1.008, 0.999 |  |
| map.erase.reused | std::flat_map | report | 1.079 | 1.077 | 0.998 | 0.994–1.003 | 0.999, 0.998 |  |
| map.erase.reused | std::map | report | 1.653 | 1.646 | 0.996 | 0.989–1.002 | 0.989, 1.002 |  |
| map.find.fresh | std::map | report | 0.326 | 0.319 | 0.980 | 0.976–0.984 | 0.981, 0.979 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.find.reused | std::map | report | 0.693 | 0.697 | 1.007 | 1.002–1.012 | 1.037, 0.977 |  |
| map.insert.fresh | std::map | report | 0.741 | 0.734 | 0.990 | 0.987–0.993 | 0.993, 0.987 |  |
| map.insert.reused | std::flat_map | report | 1.015 | 1.022 | 1.007 | 1.003–1.011 | 1.002, 1.011 | worse |
| map.insert.reused | std::map | report | 1.817 | 1.891 | 1.041 | 1.034–1.047 | 1.042, 1.039 | worse |
| nested.build.plain | std | report | 0.316 | 0.324 | 1.026 | 1.020–1.032 | 1.010, 1.042 | worse |
| set.erase.fresh | std::set | report | 0.668 | 0.668 | 1.000 | 0.996–1.004 | 0.996, 1.005 |  |
| set.erase.reused | std::flat_set | report | 1.135 | 1.137 | 1.002 | 0.998–1.006 | 1.001, 1.003 |  |
| set.erase.reused | std::set | report | 1.356 | 1.346 | 0.993 | 0.985–1.000 | 0.976, 1.010 |  |
| set.find.fresh | std::set | report | 0.301 | 0.301 | 0.998 | 0.993–1.003 | 0.997, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.999–1.004 | 1.002, 1.000 |  |
| set.find.reused | std::set | report | 0.760 | 0.784 | 1.032 | 1.029–1.035 | 1.039, 1.025 | worse |
| set.insert.fresh | std::set | report | 0.646 | 0.647 | 1.002 | 1.000–1.005 | 1.001, 1.004 |  |
| set.insert.reused | std::flat_set | report | 1.107 | 1.108 | 1.001 | 1.000–1.003 | 1.004, 0.999 |  |
| set.insert.reused | std::set | report | 1.437 | 1.524 | 1.061 | 1.051–1.070 | 1.058, 1.063 | worse |
| str.assign_short | std::string | report | 1.780 | 1.766 | 0.992 | 0.990–0.994 | 0.993, 0.991 |  |
| str.assign_short.copy | std::string | report | 1.148 | 1.150 | 1.001 | 0.999–1.004 | 1.002, 1.000 |  |

worse at t1 than t0 (by the rule): map.erase.fresh / std::flat_map (G9) 1.003; nested.build.ord / std (G9) 1.022; reset / std (G9) 1.056; set.insert.fresh / std::flat_set (G9) 1.004; str.append / std::string (G9) 1.019; str.assign_long / std::string (G9) 1.082; vec.iterate / std::vector (G9) 1.019; vec.mid_erase / std::vector (G9) 1.009; map.insert.reused / std::flat_map (report) 1.007; map.insert.reused / std::map (report) 1.041; nested.build.plain / std (report) 1.026; set.find.reused / std::set (report) 1.032; set.insert.reused / std::set (report) 1.061

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 166.9 | 165.8 | 0.993 | 0.991–0.995 | 0.991, 0.995 |  |
| hero.mid_erase.plain | 913.5 | 913.2 | 1.000 | 0.997–1.002 | 0.998, 1.001 |  |
| hero.mid_insert.ord | 228.1 | 227.8 | 0.998 | 0.997–1.000 | 1.001, 0.996 |  |
| hero.mid_insert.plain | 1265 | 1255 | 0.992 | 0.991–0.994 | 0.995, 0.990 |  |
| hero.push_back_grow.ord | 30.79 | 30.34 | 0.985 | 0.984–0.987 | 0.983, 0.987 |  |
| hero.push_back_grow.plain | 34.06 | 34.09 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| map.erase.fresh | 33.03 | 33.04 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.erase.reused | 29.78 | 29.62 | 0.995 | 0.992–0.997 | 0.994, 0.995 |  |
| map.find.fresh | 5.289 | 5.296 | 1.001 | 0.999–1.004 | 1.003, 0.999 |  |
| map.find.reused | 5.304 | 5.305 | 1.000 | 0.999–1.002 | 1.001, 0.999 |  |
| map.insert.fresh | 31.77 | 31.68 | 0.997 | 0.996–0.999 | 0.998, 0.996 |  |
| map.insert.reused | 30.22 | 30.11 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| nested.build.ord | 261.9 | 261 | 0.996 | 0.991–1.002 | 0.991, 1.002 |  |
| nested.build.plain | 265.9 | 266.8 | 1.003 | 1.000–1.006 | 1.006, 1.000 | worse |
| reset | 25.04 | 25.52 | 1.019 | 1.014–1.024 | 1.040, 0.998 | worse? |
| set.erase.fresh | 29.87 | 29.76 | 0.996 | 0.994–0.999 | 0.994, 0.998 |  |
| set.erase.reused | 23.17 | 23.16 | 0.999 | 0.996–1.003 | 0.997, 1.002 |  |
| set.find.fresh | 4.892 | 4.881 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| set.find.reused | 4.879 | 4.887 | 1.002 | 1.000–1.004 | 1.001, 1.002 |  |
| set.insert.fresh | 28.51 | 28.63 | 1.004 | 1.003–1.006 | 1.004, 1.005 | worse |
| set.insert.reused | 23.4 | 23.44 | 1.002 | 1.000–1.003 | 1.004, 1.000 |  |
| str.append | 1.52 | 1.526 | 1.004 | 1.000–1.007 | 0.992, 1.015 |  |
| str.assign_long | 2.714 | 2.936 | 1.082 | 1.080–1.083 | 1.082, 1.082 | worse |
| str.assign_short | 0.9289 | 0.9245 | 0.995 | 0.993–0.997 | 0.996, 0.995 |  |
| str.assign_short.copy | 0.6089 | 0.6108 | 1.003 | 0.999–1.007 | 0.991, 1.015 |  |
| str.assign_short.view | 0.933 | 0.9344 | 1.002 | 0.998–1.005 | 1.001, 1.002 |  |
| vec.iterate | 0.03255 | 0.03318 | 1.019 | 1.017–1.022 | 1.020, 1.018 | worse |
| vec.mid_erase | 40.27 | 40.62 | 1.009 | 1.007–1.010 | 1.010, 1.008 | worse |
| vec.mid_insert | 40.58 | 40.45 | 0.997 | 0.995–0.999 | 0.998, 0.996 |  |
| vec.push_back | 0.2951 | 0.2906 | 0.985 | 0.974–0.995 | 0.985, 0.984 |  |
| vec.push_back_grow | 0.2745 | 0.2735 | 0.996 | 0.991–1.002 | 0.996, 0.997 |  |

XOffset slower at t1 than t0 (worse by the rule): nested.build.plain 1.003; set.insert.fresh 1.004; str.assign_long 1.082; vec.iterate 1.019; vec.mid_erase 1.009

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.735 | 0.734 | 0.998 | 0.995–1.001 | 1.000, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.757 | 0.758 | 1.001 | 0.998–1.005 | 1.000, 1.002 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.346 | 0.346 | 1.002 | 0.997–1.006 | 0.986, 1.018 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.942 | 1.004 | 1.002–1.006 | 1.002, 1.006 | worse |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.847 | 0.846 | 0.998 | 0.993–1.003 | 1.003, 0.993 |  |
| nested.build.ord | std | G9 | 0.312 | 0.299 | 0.956 | 0.950–0.963 | 0.989, 0.925 |  |
| reset | std | G9 | 0.196 | 0.169 | 0.859 | 0.846–0.872 | 0.904, 0.816 |  |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.092 | 1.002 | 0.999–1.005 | 1.003, 1.002 |  |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.756 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| set.insert.fresh | std::flat_set | G9 | 0.987 | 0.987 | 1.001 | 0.998–1.004 | 0.998, 1.003 |  |
| str.append | std::string | G9 | 0.431 | 0.437 | 1.016 | 1.011–1.020 | 1.028, 1.004 | worse |
| str.assign_long | std::string | G9 | 0.867 | 0.812 | 0.936 | 0.933–0.939 | 0.936, 0.936 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 0.999 | 0.997–1.002 | 1.000, 0.999 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.020 | 1.003 | 1.001–1.005 | 1.002, 1.003 | worse |
| vec.mid_insert | std::vector | G9 | 0.984 | 0.994 | 1.009 | 1.007–1.012 | 1.009, 1.010 | worse |
| vec.push_back | std::vector | G9 | 1.115 | 1.123 | 1.008 | 1.005–1.011 | 1.006, 1.010 | worse |
| vec.push_back_grow | std::vector | G9 | 0.762 | 0.764 | 1.002 | 0.999–1.005 | 0.999, 1.005 |  |
| hero.mid_erase.plain | std::vector | report | 4.034 | 4.031 | 0.999 | 0.996–1.003 | 1.001, 0.998 |  |
| hero.mid_insert.plain | std::vector | report | 4.196 | 4.188 | 0.998 | 0.995–1.001 | 0.998, 0.998 |  |
| hero.push_back_grow.plain | std::vector | report | 0.384 | 0.387 | 1.007 | 1.003–1.011 | 0.996, 1.018 |  |
| map.erase.fresh | std::map | report | 0.737 | 0.733 | 0.995 | 0.992–0.998 | 0.995, 0.995 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.080 | 1.000 | 0.995–1.005 | 1.011, 0.989 |  |
| map.erase.reused | std::map | report | 1.608 | 1.527 | 0.950 | 0.941–0.959 | 0.985, 0.916 |  |
| map.find.fresh | std::map | report | 0.327 | 0.322 | 0.985 | 0.978–0.992 | 0.979, 0.991 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.808 | 0.999 | 0.996–1.003 | 1.001, 0.998 |  |
| map.find.reused | std::map | report | 0.720 | 0.723 | 1.004 | 0.999–1.009 | 0.962, 1.048 |  |
| map.insert.fresh | std::map | report | 0.738 | 0.738 | 1.001 | 0.996–1.005 | 0.997, 1.004 |  |
| map.insert.reused | std::flat_map | report | 1.019 | 1.023 | 1.004 | 1.000–1.008 | 1.003, 1.005 | worse |
| map.insert.reused | std::map | report | 1.848 | 1.842 | 0.997 | 0.989–1.005 | 0.985, 1.008 |  |
| nested.build.plain | std | report | 0.321 | 0.307 | 0.955 | 0.949–0.962 | 0.990, 0.921 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.662 | 0.988 | 0.984–0.992 | 0.986, 0.990 |  |
| set.erase.reused | std::flat_set | report | 1.134 | 1.134 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| set.erase.reused | std::set | report | 1.303 | 1.273 | 0.978 | 0.969–0.986 | 0.986, 0.969 |  |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.003 | 0.998–1.008 | 1.002, 1.004 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.757 | 1.002 | 1.000–1.005 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.683 | 0.691 | 1.012 | 1.009–1.015 | 1.018, 1.007 | worse |
| set.insert.fresh | std::set | report | 0.643 | 0.646 | 1.004 | 1.001–1.007 | 0.992, 1.016 |  |
| set.insert.reused | std::flat_set | report | 1.110 | 1.111 | 1.001 | 0.998–1.004 | 1.001, 1.000 |  |
| set.insert.reused | std::set | report | 1.475 | 1.430 | 0.970 | 0.961–0.979 | 0.979, 0.961 |  |
| str.assign_short | std::string | report | 1.766 | 1.764 | 0.999 | 0.995–1.003 | 0.990, 1.008 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.152 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |

worse at t1a than t0a (by the rule): map.erase.fresh / std::flat_map (G9) 1.004; str.append / std::string (G9) 1.016; vec.mid_erase / std::vector (G9) 1.003; vec.mid_insert / std::vector (G9) 1.009; vec.push_back / std::vector (G9) 1.008; map.insert.reused / std::flat_map (report) 1.004; set.find.reused / std::set (report) 1.012

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 167.5 | 166.8 | 0.996 | 0.994–0.999 | 0.995, 0.998 |  |
| hero.mid_erase.plain | 908.5 | 905.4 | 0.997 | 0.994–0.999 | 0.998, 0.995 |  |
| hero.mid_insert.ord | 227.7 | 228.3 | 1.002 | 1.001–1.004 | 1.003, 1.002 | worse |
| hero.mid_insert.plain | 1252 | 1262 | 1.007 | 1.004–1.010 | 1.018, 0.996 |  |
| hero.push_back_grow.ord | 30.69 | 30.38 | 0.990 | 0.988–0.992 | 0.987, 0.992 |  |
| hero.push_back_grow.plain | 33.99 | 33.9 | 0.997 | 0.995–0.999 | 1.001, 0.993 |  |
| map.erase.fresh | 32.99 | 32.99 | 1.000 | 0.999–1.002 | 1.000, 1.000 |  |
| map.erase.reused | 29.73 | 29.94 | 1.007 | 1.003–1.011 | 1.003, 1.011 | worse |
| map.find.fresh | 5.362 | 5.283 | 0.985 | 0.976–0.994 | 0.973, 0.997 |  |
| map.find.reused | 5.304 | 5.326 | 1.004 | 1.001–1.007 | 1.003, 1.006 | worse |
| map.insert.fresh | 32.48 | 31.71 | 0.976 | 0.968–0.984 | 0.957, 0.996 |  |
| map.insert.reused | 30.06 | 30.36 | 1.010 | 1.006–1.014 | 1.003, 1.017 | worse |
| nested.build.ord | 259.1 | 258.9 | 0.999 | 0.997–1.002 | 1.003, 0.995 |  |
| nested.build.plain | 265.7 | 264.4 | 0.995 | 0.992–0.998 | 1.001, 0.989 |  |
| reset | 24.22 | 23.75 | 0.981 | 0.973–0.988 | 1.022, 0.941 |  |
| set.erase.fresh | 29.81 | 29.99 | 1.006 | 1.003–1.009 | 0.998, 1.013 |  |
| set.erase.reused | 23.11 | 23.08 | 0.999 | 0.996–1.002 | 1.002, 0.995 |  |
| set.find.fresh | 4.879 | 4.893 | 1.003 | 1.001–1.005 | 1.001, 1.005 | worse |
| set.find.reused | 4.878 | 4.884 | 1.001 | 0.999–1.003 | 1.003, 1.000 |  |
| set.insert.fresh | 28.63 | 28.55 | 0.997 | 0.995–1.000 | 0.993, 1.001 |  |
| set.insert.reused | 23.5 | 23.47 | 0.999 | 0.996–1.001 | 0.998, 1.000 |  |
| str.append | 1.537 | 1.545 | 1.005 | 1.000–1.009 | 0.997, 1.012 |  |
| str.assign_long | 2.946 | 2.939 | 0.997 | 0.995–1.000 | 0.999, 0.996 |  |
| str.assign_short | 0.9273 | 0.9277 | 1.000 | 0.998–1.003 | 0.997, 1.004 |  |
| str.assign_short.copy | 0.6155 | 0.6155 | 1.000 | 0.995–1.005 | 1.003, 0.997 |  |
| str.assign_short.view | 0.9355 | 0.9343 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| vec.iterate | 0.03324 | 0.03324 | 1.000 | 0.999–1.000 | 1.000, 1.000 |  |
| vec.mid_erase | 40.52 | 40.47 | 0.999 | 0.997–1.000 | 1.000, 0.997 |  |
| vec.mid_insert | 40.66 | 40.55 | 0.997 | 0.996–0.999 | 0.996, 0.999 |  |
| vec.push_back | 0.272 | 0.2717 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| vec.push_back_grow | 0.2694 | 0.2697 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_insert.ord 1.002; map.erase.reused 1.007; map.find.reused 1.004; map.insert.reused 1.010; set.find.fresh 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| nested.build.plain (XOffset ns/op) | 1.003 | 0.995 | 0.992–0.998 | not worse |
| set.insert.fresh (XOffset ns/op) | 1.004 | 0.997 | 0.995–1.000 | not worse |
| str.assign_long (XOffset ns/op) | 1.082 | 0.997 | 0.995–1.000 | not worse |
| vec.iterate (XOffset ns/op) | 1.019 | 1.000 | 0.999–1.000 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.009 | 0.999 | 0.997–1.000 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.003 | 1.004 | 1.002–1.006 | still worse |
| map.insert.reused / std::flat_map (report) | 1.007 | 1.004 | 1.000–1.008 | still worse |
| map.insert.reused / std::map (report) | 1.041 | 0.997 | 0.989–1.005 | not worse |
| nested.build.ord / std (G9) | 1.022 | 0.956 | 0.950–0.963 | not worse |
| nested.build.plain / std (report) | 1.026 | 0.955 | 0.949–0.962 | not worse |
| reset / std (G9) | 1.056 | 0.859 | 0.846–0.872 | not worse |
| set.find.reused / std::set (report) | 1.032 | 1.012 | 1.009–1.015 | still worse |
| set.insert.fresh / std::flat_set (G9) | 1.004 | 1.001 | 0.998–1.004 | not worse |
| set.insert.reused / std::set (report) | 1.061 | 0.970 | 0.961–0.979 | not worse |
| str.append / std::string (G9) | 1.019 | 1.016 | 1.011–1.020 | still worse |
| str.assign_long / std::string (G9) | 1.082 | 0.936 | 0.933–0.939 | not worse |
| vec.iterate / std::vector (G9) | 1.019 | 1.000 | 0.999–1.001 | not worse |
| vec.mid_erase / std::vector (G9) | 1.009 | 1.003 | 1.001–1.005 | still worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 166.66 | 165.45 | 0.993 | 226.30 | 1.000 |
| hero.mid_erase.plain | 913.79 | 913.37 | 1.000 | 223.63 | 0.997 |
| hero.mid_insert.ord | 228.02 | 227.37 | 0.997 | 296.59 | 1.004 |
| hero.mid_insert.plain | 1262.40 | 1253.73 | 0.993 | 296.12 | 1.012 |
| hero.push_back_grow.ord | 30.72 | 30.28 | 0.986 | 86.04 | 0.990 |
| hero.push_back_grow.plain | 33.97 | 34.03 | 1.002 | 86.36 | 0.996 |
| map.erase.fresh | 32.99 | 32.97 | 0.999 | 35.16 | 1.000 |
| map.erase.reused | 29.60 | 29.55 | 0.998 | 27.36 | 1.007 |
| map.find.fresh | 5.28 | 5.29 | 1.001 | 6.55 | 1.000 |
| map.find.reused | 5.29 | 5.30 | 1.001 | 6.55 | 1.000 |
| map.insert.fresh | 31.74 | 31.66 | 0.997 | 37.49 | 0.997 |
| map.insert.reused | 30.14 | 30.02 | 0.996 | 29.64 | 1.002 |
| nested.build.ord | 260.82 | 259.44 | 0.995 | 842.74 | 0.996 |
| nested.build.plain | 265.45 | 266.11 | 1.002 | 840.61 | 0.996 |
| reset | 24.97 | 25.47 | 1.020 | 127.62 | 0.983 |
| set.erase.fresh | 29.79 | 29.72 | 0.998 | 27.47 | 1.001 |
| set.erase.reused | 22.90 | 22.94 | 1.002 | 20.27 | 0.995 |
| set.find.fresh | 4.87 | 4.88 | 1.000 | 6.45 | 1.000 |
| set.find.reused | 4.87 | 4.88 | 1.000 | 6.45 | 0.999 |
| set.insert.fresh | 28.48 | 28.61 | 1.004 | 28.89 | 0.999 |
| set.insert.reused | 23.36 | 23.41 | 1.002 | 21.11 | 0.998 |
| str.append | 1.51 | 1.51 | 1.000 | 3.59 | 1.001 |
| str.assign_long | 2.71 | 2.93 | 1.082 | 3.39 | 0.999 |
| str.assign_short | 0.93 | 0.92 | 0.995 | 0.52 | 0.995 |
| str.assign_short.copy | 0.61 | 0.60 | 0.992 | 0.53 | 0.992 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 0.998 |
| vec.iterate | 0.03 | 0.03 | 1.019 | 0.03 | 0.999 |
| vec.mid_erase | 40.16 | 40.60 | 1.011 | 39.51 | 0.999 |
| vec.mid_insert | 40.51 | 40.42 | 0.998 | 40.67 | 0.997 |
| vec.push_back | 0.30 | 0.29 | 0.981 | 0.26 | 0.998 |
| vec.push_back_grow | 0.27 | 0.27 | 0.998 | 0.36 | 1.000 |

## analyze t0 (G9 at the anchor cf9b63e5)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.147 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.982 |  0.986 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.77 |  0.996 |  0.998 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.55 |  1.019 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.67 |     226.56 |  0.738 |  0.740 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     294.92 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.72 |      86.06 |  0.357 |  0.358 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     223.95 |  4.075 |  4.090 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1262.38 |     291.02 |  4.250 |  4.263 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.94 |      86.63 |  0.394 |  0.395 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.780 |  1.784 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.799 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.420 |  0.422 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.52 |  0.846 |  0.848 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.72 |  0.741 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.26 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.08 |  0.732 |  0.734 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.988 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      44.03 |  0.646 |  0.647 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.45 |  0.757 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.26 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.668 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.11 |      29.62 |  1.015 |  1.020 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.11 |      16.28 |  1.817 |  1.827 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.70 |  0.693 |  0.696 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.079 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.58 |  1.653 |  1.665 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.107 |  1.109 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.79 |  1.437 |  1.450 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.42 |  0.760 |  0.762 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.26 |  1.135 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.60 |  1.356 |  1.366 | report  |
| nested.build.ord             | std            | G9     |  808 |    260.44 |     824.22 |  0.309 |  0.312 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     117.19 |  0.196 |  0.199 | pass    |
| nested.build.plain           | std            | report |  808 |    265.62 |     822.91 |  0.316 |  0.318 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.761 |  0.763 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      40.85 |  0.990 |  0.993 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.53 |      39.47 |  1.029 |  1.031 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.02 |     225.25 |  0.735 |  0.737 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.27 |      87.24 |  0.347 |  0.348 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     225.25 |  4.054 |  4.067 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1253.27 |     304.69 |  4.205 |  4.218 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.02 |      86.91 |  0.391 |  0.392 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.57 |  0.428 |  0.429 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      37.35 |  0.846 |  0.848 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.89 |  0.734 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.63 |  0.319 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.735 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.64 |      28.89 |  0.989 |  0.992 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.64 |      44.19 |  0.647 |  0.649 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.29 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.34 |  1.088 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.27 |  0.668 |  0.670 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.022 |  1.026 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      15.87 |  1.891 |  1.904 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.70 |  0.697 |  0.701 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.646 |  1.656 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.16 |  1.108 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.22 |  1.524 |  1.536 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.18 |  0.784 |  0.787 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.34 |  1.137 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.76 |  1.346 |  1.356 | report  |
| nested.build.ord             | std            | G9     |  808 |    259.12 |     822.94 |  0.316 |  0.318 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     118.48 |  0.207 |  0.209 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     824.22 |  0.324 |  0.325 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-11T16:47:20Z block 1 start: waited 180s load 1.39 WindowServer 1.3% remote 6
2026-11-11T16:50:24Z block 2 start: waited 180s load 1.36 WindowServer 1.2% remote 6
2026-11-11T16:53:29Z block 3 start: waited 180s load 1.21 WindowServer 1.2% remote 6
2026-11-11T16:56:34Z block 4 start: waited 180s load 1.44 WindowServer 1.3% remote 6
```
