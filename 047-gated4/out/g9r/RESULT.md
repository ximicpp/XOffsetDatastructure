# G9R3 result (2026-10-27T21:08:52Z)

t0 43669586b5ad29962050e36dff8ffc252917f868 (next_cpp26 43669586 (#146 merge, with #149)); t1 3dacea2c9627aef295d74caf0b6b097a8ed05224 (GATE_D4 head claude/d-iter-20261027 (XFixedVector iterator class PR)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 43669586b5ad29962050e36dff8ffc252917f868 tree dc3e42d4fb25ef7e2cb4aa98bff6e3460377f989 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 43669586 (#146 merge, with #149))
t1 3dacea2c9627aef295d74caf0b6b097a8ed05224 tree c67fa69e0f3670111ccb36be9eb3d743195463c7 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (GATE_D4 head claude/d-iter-20261027 (XFixedVector iterator class PR))
t1: diff over t0 sha256 20200051c12f3cd5e119d417a3ea206403f6e7086d1d893323316337a2b686e3 ( 3 files changed, 152 insertions(+), 5 deletions(-))
t1: library header diff over t0:  1 file changed, 53 insertions(+), 4 deletions(-)
t1: first-parent commits over t0:
  3dacea2c Container iterators: a contiguous iterator class instead of T*
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 a936ef69e9796e7278aaf8026a293114f782baee75604a85ffd62d4c68236b7b
reloc_bench t1 sha256 adf6662df28f6fbca26935b706d2a19849b87c701507bfeb11d8cd0cb958f2c7
reloc_bench t0a sha256 f687c5133aa0b2dbb61f7811bc254744edd4cefd16e6e5d65ddc8cc133732e4d
reloc_bench t1a sha256 f8d3a6dc6d41f326221127305b68af868523f552e84d4af55a18f153e91d89fe
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than t0 43669586)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.706 | 0.717 | 1.016 | 1.012–1.020 | 1.025, 1.007 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.764 | 1.005 | 1.000–1.010 | 1.011, 1.000 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.365 | 0.367 | 1.007 | 1.003–1.012 | 1.007, 1.007 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.943 | 1.002 | 0.998–1.007 | 1.003, 1.002 |  |
| map.find.fresh | std::flat_map | G9 | 0.785 | 0.784 | 0.999 | 0.994–1.003 | 1.000, 0.997 |  |
| map.insert.fresh | std::flat_map | G9 | 0.872 | 0.874 | 1.003 | 0.999–1.006 | 1.004, 1.002 |  |
| nested.build.ord | std | G9 | 0.337 | 0.340 | 1.011 | 1.004–1.018 | 1.010, 1.011 | worse |
| reset | std | G9 | 0.190 | 0.184 | 0.970 | 0.963–0.977 | 0.942, 0.999 |  |
| set.erase.fresh | std::flat_set | G9 | 1.099 | 1.093 | 0.995 | 0.990–1.000 | 0.993, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.753 | 0.998 | 0.993–1.002 | 0.997, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.996 | 0.998 | 0.994–1.002 | 0.999, 0.997 |  |
| str.append | std::string | G9 | 0.493 | 0.462 | 0.937 | 0.932–0.941 | 0.937, 0.937 |  |
| str.assign_long | std::string | G9 | 0.799 | 0.797 | 0.998 | 0.994–1.002 | 1.001, 0.995 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.997–1.003 | 0.998, 1.001 |  |
| vec.iterate | std::vector | G9 | 0.995 | 0.977 | 0.981 | 0.977–0.986 | 0.977, 0.986 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.018 | 1.001 | 0.996–1.006 | 0.990, 1.012 |  |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.991 | 0.999 | 0.995–1.003 | 1.000, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.195 | 1.201 | 1.005 | 0.995–1.014 | 0.971, 1.039 |  |
| vec.push_back_grow | std::vector | G9 | 0.768 | 0.767 | 0.998 | 0.992–1.004 | 1.002, 0.994 |  |
| hero.mid_erase.plain | std::vector | report | 3.910 | 4.029 | 1.030 | 1.026–1.035 | 1.041, 1.020 | worse |
| hero.mid_insert.plain | std::vector | report | 4.216 | 4.209 | 0.998 | 0.993–1.004 | 1.006, 0.991 |  |
| hero.push_back_grow.plain | std::vector | report | 0.405 | 0.414 | 1.022 | 1.017–1.028 | 1.020, 1.024 | worse |
| map.erase.fresh | std::map | report | 0.691 | 0.696 | 1.008 | 1.003–1.013 | 1.000, 1.015 | worse |
| map.erase.reused | std::flat_map | report | 1.084 | 1.081 | 0.998 | 0.993–1.002 | 0.998, 0.997 |  |
| map.erase.reused | std::map | report | 1.409 | 1.425 | 1.011 | 1.000–1.022 | 1.027, 0.996 | worse? |
| map.find.fresh | std::map | report | 0.315 | 0.316 | 1.003 | 0.997–1.008 | 1.006, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.785 | 0.783 | 0.997 | 0.993–1.001 | 0.997, 0.997 |  |
| map.find.reused | std::map | report | 0.714 | 0.698 | 0.977 | 0.970–0.985 | 0.985, 0.969 |  |
| map.insert.fresh | std::map | report | 0.737 | 0.729 | 0.989 | 0.985–0.994 | 0.982, 0.997 |  |
| map.insert.reused | std::flat_map | report | 1.009 | 1.005 | 0.996 | 0.991–1.001 | 0.993, 0.999 |  |
| map.insert.reused | std::map | report | 1.707 | 1.657 | 0.971 | 0.961–0.980 | 0.971, 0.970 |  |
| nested.build.plain | std | report | 0.346 | 0.344 | 0.994 | 0.988–1.001 | 1.000, 0.989 |  |
| set.erase.fresh | std::set | report | 0.635 | 0.634 | 0.999 | 0.994–1.003 | 0.998, 0.999 |  |
| set.erase.reused | std::flat_set | report | 1.143 | 1.143 | 1.000 | 0.995–1.005 | 1.004, 0.995 |  |
| set.erase.reused | std::set | report | 1.149 | 1.159 | 1.008 | 0.998–1.018 | 1.008, 1.008 |  |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 1.000 | 0.994–1.006 | 1.001, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.754 | 0.999 | 0.994–1.003 | 1.001, 0.996 |  |
| set.find.reused | std::set | report | 0.670 | 0.731 | 1.091 | 1.083–1.099 | 1.117, 1.066 | worse |
| set.insert.fresh | std::set | report | 0.640 | 0.640 | 0.999 | 0.994–1.003 | 0.999, 0.999 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.083 | 1.002 | 0.998–1.006 | 0.999, 1.005 |  |
| set.insert.reused | std::set | report | 1.287 | 1.296 | 1.007 | 0.997–1.016 | 1.006, 1.008 |  |
| str.assign_short | std::string | report | 1.837 | 1.828 | 0.995 | 0.987–1.003 | 0.991, 0.999 |  |
| str.assign_short.copy | std::string | report | 1.188 | 1.193 | 1.004 | 0.998–1.010 | 1.000, 1.008 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.016; hero.push_back_grow.ord / std::vector (G9) 1.007; nested.build.ord / std (G9) 1.011; hero.mid_erase.plain / std::vector (report) 1.030; hero.push_back_grow.plain / std::vector (report) 1.022; map.erase.fresh / std::map (report) 1.008; set.find.reused / std::set (report) 1.091

above 1.01 but not worse by the rule: map.erase.reused / std::map (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.5 | 164.7 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| hero.mid_erase.plain | 892.9 | 916.1 | 1.026 | 1.022–1.030 | 1.030, 1.022 | worse |
| hero.mid_insert.ord | 230.8 | 232 | 1.005 | 1.001–1.009 | 1.010, 1.001 | worse |
| hero.mid_insert.plain | 1273 | 1274 | 1.001 | 0.998–1.004 | 1.005, 0.997 |  |
| hero.push_back_grow.ord | 33.13 | 33.25 | 1.004 | 1.000–1.008 | 1.006, 1.002 |  |
| hero.push_back_grow.plain | 36.94 | 36.95 | 1.000 | 0.996–1.005 | 1.000, 1.000 |  |
| map.erase.fresh | 33.12 | 33.14 | 1.000 | 0.996–1.004 | 1.004, 0.997 |  |
| map.erase.reused | 29.8 | 29.67 | 0.995 | 0.992–0.999 | 0.994, 0.997 |  |
| map.find.fresh | 5.162 | 5.162 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| map.find.reused | 5.177 | 5.162 | 0.997 | 0.994–1.000 | 0.999, 0.995 |  |
| map.insert.fresh | 32.31 | 32.32 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| map.insert.reused | 29.74 | 29.69 | 0.998 | 0.993–1.004 | 0.996, 1.000 |  |
| nested.build.ord | 296.2 | 293.9 | 0.992 | 0.986–0.999 | 0.988, 0.997 |  |
| nested.build.plain | 298.2 | 298 | 0.999 | 0.994–1.005 | 1.002, 0.997 |  |
| reset | 30.85 | 30.27 | 0.981 | 0.975–0.987 | 0.959, 1.004 |  |
| set.erase.fresh | 30.01 | 29.89 | 0.996 | 0.992–1.000 | 0.995, 0.996 |  |
| set.erase.reused | 23.34 | 23.38 | 1.002 | 0.997–1.006 | 1.009, 0.995 |  |
| set.find.fresh | 4.902 | 4.897 | 0.999 | 0.995–1.003 | 0.999, 0.999 |  |
| set.find.reused | 4.904 | 4.902 | 1.000 | 0.996–1.003 | 1.000, 0.999 |  |
| set.insert.fresh | 28.99 | 28.89 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| set.insert.reused | 23.03 | 22.83 | 0.991 | 0.987–0.996 | 0.993, 0.990 |  |
| str.append | 1.758 | 1.702 | 0.968 | 0.964–0.972 | 0.966, 0.970 |  |
| str.assign_long | 2.733 | 2.733 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| str.assign_short | 0.9231 | 0.9289 | 1.006 | 1.002–1.011 | 1.002, 1.011 | worse |
| str.assign_short.copy | 0.6049 | 0.6086 | 1.006 | 0.999–1.013 | 0.998, 1.014 |  |
| str.assign_short.view | 0.9341 | 0.9352 | 1.001 | 0.998–1.004 | 1.002, 1.001 |  |
| vec.iterate | 0.0341 | 0.0332 | 0.974 | 0.969–0.978 | 0.977, 0.970 |  |
| vec.mid_erase | 40.85 | 40.95 | 1.003 | 0.997–1.008 | 0.998, 1.007 |  |
| vec.mid_insert | 40.95 | 41.39 | 1.011 | 1.006–1.015 | 1.007, 1.014 | worse |
| vec.push_back | 0.3165 | 0.3238 | 1.023 | 1.011–1.035 | 1.020, 1.026 | worse |
| vec.push_back_grow | 0.2852 | 0.2854 | 1.001 | 0.993–1.009 | 0.998, 1.003 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.plain 1.026; hero.mid_insert.ord 1.005; str.assign_short 1.006; vec.mid_insert 1.011; vec.push_back 1.023

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.714 | 0.715 | 1.002 | 0.995–1.009 | 1.003, 1.002 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.760 | 0.996 | 0.992–1.000 | 1.001, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.359 | 0.368 | 1.026 | 1.021–1.032 | 1.052, 1.001 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.935 | 0.994 | 0.989–0.999 | 0.995, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.786 | 0.786 | 0.999 | 0.995–1.003 | 0.998, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.870 | 1.002 | 0.997–1.008 | 1.006, 0.999 |  |
| nested.build.ord | std | G9 | 0.338 | 0.333 | 0.986 | 0.981–0.991 | 0.964, 1.009 |  |
| reset | std | G9 | 0.186 | 0.185 | 0.992 | 0.984–1.000 | 0.943, 1.043 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.088 | 0.995 | 0.990–0.999 | 0.992, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.995–1.003 | 1.000, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 0.998 | 0.999 | 0.994–1.004 | 0.996, 1.001 |  |
| str.append | std::string | G9 | 0.426 | 0.424 | 0.995 | 0.990–1.000 | 1.025, 0.965 |  |
| str.assign_long | std::string | G9 | 0.862 | 0.859 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.996–1.003 | 0.999, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.035 | 1.034 | 0.999 | 0.993–1.006 | 1.003, 0.995 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.014 | 0.996 | 0.993–1.000 | 0.992, 1.001 |  |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.989 | 0.997 | 0.994–1.001 | 0.997, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.122 | 1.128 | 1.005 | 1.001–1.008 | 1.005, 1.004 | worse |
| vec.push_back_grow | std::vector | G9 | 0.770 | 0.769 | 0.998 | 0.993–1.004 | 1.008, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 3.985 | 3.985 | 1.000 | 0.995–1.005 | 0.996, 1.004 |  |
| hero.mid_insert.plain | std::vector | report | 4.209 | 4.178 | 0.993 | 0.988–0.998 | 1.001, 0.984 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.401 | 0.989 | 0.983–0.995 | 0.971, 1.007 |  |
| map.erase.fresh | std::map | report | 0.695 | 0.693 | 0.997 | 0.991–1.004 | 0.994, 1.000 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.079 | 0.998 | 0.993–1.003 | 1.001, 0.996 |  |
| map.erase.reused | std::map | report | 1.248 | 1.307 | 1.048 | 1.037–1.058 | 1.047, 1.049 | worse |
| map.find.fresh | std::map | report | 0.314 | 0.314 | 1.000 | 0.995–1.006 | 0.999, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.783 | 0.786 | 1.004 | 0.999–1.008 | 1.007, 1.000 |  |
| map.find.reused | std::map | report | 0.719 | 0.689 | 0.958 | 0.951–0.965 | 0.976, 0.941 |  |
| map.insert.fresh | std::map | report | 0.739 | 0.733 | 0.992 | 0.988–0.997 | 0.991, 0.993 |  |
| map.insert.reused | std::flat_map | report | 1.005 | 1.007 | 1.001 | 0.998–1.004 | 0.996, 1.006 |  |
| map.insert.reused | std::map | report | 1.678 | 1.711 | 1.020 | 1.014–1.025 | 1.015, 1.024 | worse |
| nested.build.plain | std | report | 0.339 | 0.345 | 1.018 | 1.012–1.023 | 1.015, 1.020 | worse |
| set.erase.fresh | std::set | report | 0.635 | 0.630 | 0.991 | 0.986–0.996 | 0.982, 1.000 |  |
| set.erase.reused | std::flat_set | report | 1.148 | 1.150 | 1.002 | 0.998–1.007 | 0.997, 1.008 |  |
| set.erase.reused | std::set | report | 1.080 | 1.043 | 0.965 | 0.955–0.976 | 0.938, 0.994 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.001 | 0.995–1.006 | 0.997, 1.004 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.685 | 1.013 | 1.009–1.018 | 1.017, 1.010 | worse |
| set.insert.fresh | std::set | report | 0.641 | 0.640 | 0.997 | 0.991–1.003 | 1.000, 0.994 |  |
| set.insert.reused | std::flat_set | report | 1.080 | 1.081 | 1.000 | 0.995–1.006 | 1.001, 1.000 |  |
| set.insert.reused | std::set | report | 1.303 | 1.266 | 0.972 | 0.961–0.983 | 0.987, 0.957 |  |
| str.assign_short | std::string | report | 1.825 | 1.824 | 1.000 | 0.996–1.003 | 1.001, 0.999 |  |
| str.assign_short.copy | std::string | report | 1.193 | 1.188 | 0.996 | 0.990–1.001 | 0.996, 0.996 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.026; vec.push_back / std::vector (G9) 1.005; map.erase.reused / std::map (report) 1.048; map.insert.reused / std::map (report) 1.020; nested.build.plain / std (report) 1.018; set.find.reused / std::set (report) 1.013

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.8 | 165 | 1.001 | 0.995–1.008 | 0.995, 1.008 |  |
| hero.mid_erase.plain | 909 | 913 | 1.004 | 1.001–1.008 | 1.008, 1.001 | worse |
| hero.mid_insert.ord | 231.8 | 230 | 0.992 | 0.989–0.995 | 0.999, 0.985 |  |
| hero.mid_insert.plain | 1272 | 1263 | 0.993 | 0.990–0.996 | 0.997, 0.989 |  |
| hero.push_back_grow.ord | 33.22 | 33.05 | 0.995 | 0.991–0.999 | 0.994, 0.996 |  |
| hero.push_back_grow.plain | 36.78 | 36.74 | 0.999 | 0.994–1.004 | 1.001, 0.996 |  |
| map.erase.fresh | 33.18 | 33.05 | 0.996 | 0.992–1.000 | 0.997, 0.996 |  |
| map.erase.reused | 29.84 | 29.65 | 0.994 | 0.990–0.997 | 0.993, 0.994 |  |
| map.find.fresh | 5.15 | 5.156 | 1.001 | 0.997–1.005 | 1.001, 1.002 |  |
| map.find.reused | 5.173 | 5.179 | 1.001 | 0.997–1.005 | 1.004, 0.998 |  |
| map.insert.fresh | 32.46 | 32.44 | 1.000 | 0.995–1.005 | 1.002, 0.997 |  |
| map.insert.reused | 29.57 | 29.55 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| nested.build.ord | 294.4 | 293.4 | 0.997 | 0.992–1.001 | 1.002, 0.991 |  |
| nested.build.plain | 298.9 | 299.4 | 1.002 | 0.997–1.006 | 0.997, 1.006 |  |
| reset | 30.89 | 30.7 | 0.994 | 0.988–1.000 | 0.975, 1.013 |  |
| set.erase.fresh | 29.96 | 29.93 | 0.999 | 0.995–1.003 | 0.999, 0.999 |  |
| set.erase.reused | 23.45 | 23.52 | 1.003 | 0.997–1.009 | 0.999, 1.007 |  |
| set.find.fresh | 4.898 | 4.902 | 1.001 | 0.997–1.004 | 1.000, 1.001 |  |
| set.find.reused | 4.9 | 4.891 | 0.998 | 0.994–1.002 | 0.998, 0.998 |  |
| set.insert.fresh | 29.09 | 28.94 | 0.995 | 0.990–0.999 | 0.990, 0.999 |  |
| set.insert.reused | 23.02 | 23.15 | 1.006 | 0.999–1.013 | 1.007, 1.005 |  |
| str.append | 1.526 | 1.538 | 1.008 | 1.003–1.012 | 1.027, 0.989 |  |
| str.assign_long | 2.945 | 2.945 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |
| str.assign_short | 0.9274 | 0.9243 | 0.997 | 0.994–1.000 | 1.004, 0.989 |  |
| str.assign_short.copy | 0.6097 | 0.6145 | 1.008 | 1.003–1.013 | 1.017, 0.999 |  |
| str.assign_short.view | 0.9383 | 0.9365 | 0.998 | 0.994–1.002 | 1.002, 0.994 |  |
| vec.iterate | 0.03338 | 0.03341 | 1.001 | 0.997–1.005 | 0.992, 1.010 |  |
| vec.mid_erase | 40.69 | 40.45 | 0.994 | 0.991–0.997 | 0.996, 0.992 |  |
| vec.mid_insert | 40.54 | 40.56 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| vec.push_back | 0.2724 | 0.2727 | 1.001 | 0.999–1.004 | 1.001, 1.002 |  |
| vec.push_back_grow | 0.2772 | 0.2754 | 0.993 | 0.989–0.998 | 0.999, 0.987 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.004

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.plain (XOffset ns/op) | 1.026 | 1.004 | 1.001–1.008 | still worse |
| hero.mid_insert.ord (XOffset ns/op) | 1.005 | 0.992 | 0.989–0.995 | not worse |
| str.assign_short (XOffset ns/op) | 1.006 | 0.997 | 0.994–1.000 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.011 | 1.001 | 0.997–1.004 | not worse |
| vec.push_back (XOffset ns/op) | 1.023 | 1.001 | 0.999–1.004 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.016 | 1.002 | 0.995–1.009 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.030 | 1.000 | 0.995–1.005 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.007 | 1.026 | 1.021–1.032 | still worse |
| hero.push_back_grow.plain / std::vector (report) | 1.022 | 0.989 | 0.983–0.995 | not worse |
| map.erase.fresh / std::map (report) | 1.008 | 0.997 | 0.991–1.004 | not worse |
| nested.build.ord / std (G9) | 1.011 | 0.986 | 0.981–0.991 | not worse |
| set.find.reused / std::set (report) | 1.091 | 1.013 | 1.009–1.018 | still worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.62 | 164.14 | 1.003 | 232.13 | 1.001 |
| hero.mid_erase.plain | 888.74 | 911.52 | 1.026 | 226.50 | 1.003 |
| hero.mid_insert.ord | 229.57 | 230.18 | 1.003 | 302.54 | 0.995 |
| hero.mid_insert.plain | 1263.58 | 1265.75 | 1.002 | 301.40 | 0.996 |
| hero.push_back_grow.ord | 32.88 | 33.01 | 1.004 | 90.47 | 0.997 |
| hero.push_back_grow.plain | 36.56 | 36.69 | 1.003 | 91.05 | 0.999 |
| map.erase.fresh | 32.98 | 32.93 | 0.998 | 35.05 | 1.000 |
| map.erase.reused | 29.57 | 29.54 | 0.999 | 27.19 | 0.996 |
| map.find.fresh | 5.15 | 5.14 | 1.000 | 6.55 | 1.000 |
| map.find.reused | 5.15 | 5.15 | 1.000 | 6.56 | 1.000 |
| map.insert.fresh | 32.17 | 32.16 | 1.000 | 36.84 | 1.001 |
| map.insert.reused | 29.45 | 29.49 | 1.001 | 29.19 | 1.001 |
| nested.build.ord | 293.37 | 291.02 | 0.992 | 874.29 | 0.997 |
| nested.build.plain | 295.48 | 294.75 | 0.998 | 856.75 | 1.001 |
| reset | 30.82 | 30.10 | 0.976 | 161.75 | 0.992 |
| set.erase.fresh | 29.83 | 29.79 | 0.999 | 27.21 | 0.999 |
| set.erase.reused | 23.14 | 23.07 | 0.997 | 20.21 | 1.002 |
| set.find.fresh | 4.88 | 4.88 | 1.000 | 6.46 | 1.000 |
| set.find.reused | 4.88 | 4.87 | 0.999 | 6.45 | 1.000 |
| set.insert.fresh | 28.82 | 28.82 | 1.000 | 28.84 | 1.000 |
| set.insert.reused | 22.83 | 22.79 | 0.998 | 21.07 | 0.997 |
| str.append | 1.73 | 1.68 | 0.971 | 3.53 | 0.998 |
| str.assign_long | 2.71 | 2.71 | 1.001 | 3.40 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.001 | 0.50 | 0.998 |
| str.assign_short.copy | 0.60 | 0.60 | 1.001 | 0.50 | 1.012 |
| str.assign_short.view | 0.93 | 0.93 | 1.001 | 2.71 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.978 | 0.03 | 0.998 |
| vec.mid_erase | 40.52 | 40.52 | 1.000 | 39.85 | 0.998 |
| vec.mid_insert | 40.70 | 41.10 | 1.010 | 41.07 | 0.999 |
| vec.push_back | 0.32 | 0.32 | 1.023 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.28 | 1.003 | 0.37 | 0.993 |

## analyze t0 (G9 at 43669586)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.195 |  1.206 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.768 |  0.773 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.995 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      40.69 |  0.992 |  0.996 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.79 |  1.018 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     231.77 |  0.706 |  0.709 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    229.81 |     306.64 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.88 |      90.01 |  0.365 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    888.35 |     228.52 |  3.910 |  3.928 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.67 |     305.98 |  4.216 |  4.237 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.58 |      90.25 |  0.405 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.837 |  1.843 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.799 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.188 |  1.191 | report  |
| str.append                   | std::string    | G9     |  808 |      1.72 |       3.57 |  0.493 |  0.495 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.87 |  0.872 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      43.62 |  0.737 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.785 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.38 |  0.315 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.36 |  0.691 |  0.694 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.998 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.00 |  0.640 |  0.644 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.37 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.099 |  1.104 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.83 |  0.635 |  0.638 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.21 |  1.009 |  1.015 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      17.09 |  1.707 |  1.722 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.16 |  0.714 |  0.719 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.18 |  1.084 |  1.089 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      20.35 |  1.409 |  1.425 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.081 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      17.50 |  1.287 |  1.302 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.10 |  0.670 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.19 |      20.18 |  1.143 |  1.150 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.19 |      19.94 |  1.149 |  1.161 | report  |
| nested.build.ord             | std            | G9     |  808 |    292.97 |     875.00 |  0.337 |  0.339 | pass    |
| reset                        | std            | G9     |  808 |     31.25 |     160.16 |  0.190 |  0.191 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     852.88 |  0.346 |  0.348 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.201 |  1.213 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.767 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.977 |  0.981 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      40.69 |  0.991 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.71 |  1.018 |  1.024 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.86 |  0.717 |  0.719 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    230.47 |     296.88 |  0.764 |  0.769 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.04 |      89.76 |  0.367 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.81 |     227.88 |  4.029 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1265.62 |     306.64 |  4.209 |  4.231 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.66 |      88.38 |  0.414 |  0.416 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.828 |  1.847 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.797 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.193 |  1.202 | report  |
| str.append                   | std::string    | G9     |  808 |      1.68 |       3.61 |  0.462 |  0.464 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.79 |  0.874 |  0.877 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      44.03 |  0.729 |  0.733 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.54 |  0.784 |  0.787 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.34 |  0.316 |  0.318 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.943 |  0.947 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.28 |  0.696 |  0.699 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  1.000 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.92 |  0.640 |  0.642 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.753 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.33 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.093 |  1.099 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.88 |  0.634 |  0.636 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      17.74 |  1.657 |  1.675 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.783 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.25 |  0.698 |  0.704 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.18 |  1.081 |  1.087 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      20.02 |  1.425 |  1.440 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.083 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.74 |  1.296 |  1.305 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.56 |  0.731 |  0.736 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.143 |  1.148 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      19.41 |  1.159 |  1.170 | report  |
| nested.build.ord             | std            | G9     |  808 |    291.66 |     860.02 |  0.340 |  0.342 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     163.42 |  0.184 |  0.186 | pass    |
| nested.build.plain           | std            | report |  808 |    294.28 |     858.06 |  0.344 |  0.346 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-10-27T20:59:35Z block 1 start: waited 180s load 1.65 WindowServer 4% remote 6
2026-10-27T21:02:39Z block 2 start: waited 180s load 1.65 WindowServer 4.1% remote 6
2026-10-27T21:05:44Z block 3 start: waited 180s load 1.76 WindowServer 3.7% remote 6
2026-10-27T21:08:48Z block 4 start: waited 180s load 1.78 WindowServer 3.2% remote 6
```
