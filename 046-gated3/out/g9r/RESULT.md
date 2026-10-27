# G9R3 result (2026-10-27T19:03:08Z)

t0 f0308ee75b002ee5663ec0da9104c0c2eb57a279 (next_cpp26 f0308ee7 (#147 merge; tree of GATE_D2 head 5119a4f)); t1 7ddeab53d2e6fa872892b2d0e2e6c5a04fbf2a24 (GATE_D3 head (XFixedVector iterator class)); bench/reloc of t0 (aabaa1ed859747bc5237fe471e8107c6f92c642d) for both.

```
t0 f0308ee75b002ee5663ec0da9104c0c2eb57a279 tree ea50d00c5f7e3530c0a2e32917a1fe5d9aaf0706 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 f0308ee7 (#147 merge; tree of GATE_D2 head 5119a4f))
t1 7ddeab53d2e6fa872892b2d0e2e6c5a04fbf2a24 tree 5601009df9ebe5ff5213785c7feba5dd2036285e typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (GATE_D3 head (XFixedVector iterator class))
t1: diff over t0 sha256 20c6ce2c27df3d3a25cf84bdc6e44f37c06e25ea70bb6d4a9e8fa202947195a2 ( 1 file changed, 53 insertions(+), 4 deletions(-))
t1: library header diff over t0:  1 file changed, 53 insertions(+), 4 deletions(-)
t1: first-parent commits over t0:
  7ddeab53 proto: class iterator for XFixedVector (measurement only, not for merge)
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 95a74c73d8cfe9b0995cbf6a0b121b4e0672909a8fa409a51e4e11b68c5a21de
reloc_bench t1 sha256 59d8ea978ae7e85bb159596c47a1e88a9b4f878a5dc2a8e250ee133c1c0ca011
reloc_bench t0a sha256 78784a875e87e0f16f975861882a15566984401237f956b887b78a4281af2f0c
reloc_bench t1a sha256 92c303ba2d82a46ad6067a567f6a84cf38c0e60a42478df5e1956d044c7d697c
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than t0 f0308ee7)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.719 | 0.713 | 0.991 | 0.988–0.993 | 1.003, 0.979 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.754 | 0.751 | 0.996 | 0.992–0.999 | 1.001, 0.990 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.370 | 0.364 | 0.984 | 0.980–0.988 | 0.999, 0.969 |  |
| map.erase.fresh | std::flat_map | G9 | 0.936 | 0.936 | 1.000 | 0.998–1.002 | 1.001, 0.998 |  |
| map.find.fresh | std::flat_map | G9 | 0.786 | 0.783 | 0.997 | 0.994–0.999 | 0.996, 0.998 |  |
| map.insert.fresh | std::flat_map | G9 | 0.877 | 0.873 | 0.995 | 0.993–0.998 | 0.994, 0.997 |  |
| nested.build.ord | std | G9 | 0.337 | 0.335 | 0.996 | 0.992–1.000 | 0.981, 1.012 |  |
| reset | std | G9 | 0.182 | 0.182 | 0.999 | 0.992–1.005 | 0.995, 1.003 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.090 | 0.998 | 0.995–1.000 | 0.996, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.755 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 1.001 | 1.003 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| str.append | std::string | G9 | 0.472 | 0.422 | 0.895 | 0.891–0.899 | 0.896, 0.893 |  |
| str.assign_long | std::string | G9 | 0.798 | 0.796 | 0.997 | 0.995–0.999 | 0.997, 0.996 |  |
| str.assign_short.view | std::string | G9 | 0.341 | 0.342 | 1.005 | 1.003–1.008 | 1.011, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.020 | 1.000 | 0.980 | 0.977–0.984 | 0.981, 0.980 |  |
| vec.mid_erase | std::vector | G9 | 1.015 | 1.014 | 0.998 | 0.996–1.000 | 0.994, 1.003 |  |
| vec.mid_insert | std::vector | G9 | 0.997 | 0.995 | 0.998 | 0.996–1.001 | 1.000, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.099 | 1.120 | 1.019 | 1.012–1.026 | 0.990, 1.049 | worse? |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.766 | 0.999 | 0.993–1.005 | 1.003, 0.995 |  |
| hero.mid_erase.plain | std::vector | report | 4.014 | 3.981 | 0.992 | 0.988–0.996 | 1.007, 0.976 |  |
| hero.mid_insert.plain | std::vector | report | 4.227 | 4.201 | 0.994 | 0.991–0.997 | 0.996, 0.992 |  |
| hero.push_back_grow.plain | std::vector | report | 0.410 | 0.406 | 0.989 | 0.985–0.992 | 1.007, 0.971 |  |
| map.erase.fresh | std::map | report | 0.697 | 0.696 | 0.998 | 0.995–1.001 | 0.998, 0.999 |  |
| map.erase.reused | std::flat_map | report | 1.086 | 1.080 | 0.994 | 0.992–0.997 | 0.993, 0.996 |  |
| map.erase.reused | std::map | report | 1.353 | 1.326 | 0.981 | 0.975–0.987 | 0.968, 0.994 |  |
| map.find.fresh | std::map | report | 0.313 | 0.313 | 0.998 | 0.994–1.002 | 0.996, 1.001 |  |
| map.find.reused | std::flat_map | report | 0.786 | 0.786 | 1.000 | 0.997–1.003 | 1.002, 0.997 |  |
| map.find.reused | std::map | report | 0.693 | 0.700 | 1.010 | 1.006–1.013 | 1.016, 1.004 | worse |
| map.insert.fresh | std::map | report | 0.729 | 0.739 | 1.013 | 1.010–1.016 | 1.010, 1.017 | worse |
| map.insert.reused | std::flat_map | report | 1.007 | 1.011 | 1.004 | 1.003–1.006 | 1.003, 1.006 | worse |
| map.insert.reused | std::map | report | 1.703 | 1.690 | 0.992 | 0.989–0.996 | 0.995, 0.990 |  |
| nested.build.plain | std | report | 0.346 | 0.348 | 1.003 | 0.999–1.008 | 0.997, 1.010 |  |
| set.erase.fresh | std::set | report | 0.634 | 0.630 | 0.994 | 0.990–0.998 | 0.993, 0.995 |  |
| set.erase.reused | std::flat_set | report | 1.146 | 1.150 | 1.003 | 1.000–1.006 | 1.004, 1.003 | worse |
| set.erase.reused | std::set | report | 1.168 | 1.118 | 0.958 | 0.950–0.965 | 0.959, 0.957 |  |
| set.find.fresh | std::set | report | 0.300 | 0.301 | 1.004 | 1.000–1.009 | 1.002, 1.006 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.002 | 1.000–1.005 | 1.003, 1.002 |  |
| set.find.reused | std::set | report | 0.762 | 0.741 | 0.972 | 0.970–0.975 | 0.963, 0.981 |  |
| set.insert.fresh | std::set | report | 0.638 | 0.634 | 0.994 | 0.991–0.997 | 1.003, 0.985 |  |
| set.insert.reused | std::flat_set | report | 1.085 | 1.084 | 0.999 | 0.997–1.002 | 1.003, 0.996 |  |
| set.insert.reused | std::set | report | 1.294 | 1.308 | 1.011 | 1.002–1.019 | 1.022, 0.999 | worse? |
| str.assign_short | std::string | report | 1.834 | 1.835 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.188 | 1.190 | 1.002 | 1.000–1.004 | 1.002, 1.001 |  |

worse at t1 than t0 (by the rule): map.find.reused / std::map (report) 1.010; map.insert.fresh / std::map (report) 1.013; map.insert.reused / std::flat_map (report) 1.004; set.erase.reused / std::flat_set (report) 1.003

above 1.01 but not worse by the rule: vec.push_back / std::vector (G9) 1.019; set.insert.reused / std::set (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.7 | 164.1 | 0.996 | 0.995–0.998 | 0.998, 0.994 |  |
| hero.mid_erase.plain | 912.1 | 910.8 | 0.999 | 0.996–1.001 | 1.004, 0.993 |  |
| hero.mid_insert.ord | 229.1 | 227.8 | 0.994 | 0.992–0.996 | 0.999, 0.989 |  |
| hero.mid_insert.plain | 1269 | 1270 | 1.000 | 0.999–1.002 | 1.002, 0.999 |  |
| hero.push_back_grow.ord | 33.13 | 33.06 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| hero.push_back_grow.plain | 36.53 | 36.57 | 1.001 | 0.999–1.003 | 0.998, 1.005 |  |
| map.erase.fresh | 33.04 | 32.99 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| map.erase.reused | 29.74 | 29.65 | 0.997 | 0.995–0.999 | 1.000, 0.994 |  |
| map.find.fresh | 5.143 | 5.131 | 0.998 | 0.996–1.000 | 0.996, 0.999 |  |
| map.find.reused | 5.171 | 5.162 | 0.998 | 0.996–1.001 | 1.000, 0.997 |  |
| map.insert.fresh | 32.26 | 32.26 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.reused | 29.54 | 29.55 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| nested.build.ord | 290.9 | 291.1 | 1.001 | 0.998–1.004 | 0.996, 1.005 |  |
| nested.build.plain | 296.1 | 296.7 | 1.002 | 0.998–1.005 | 0.999, 1.005 |  |
| reset | 30.17 | 30.82 | 1.022 | 1.018–1.025 | 1.028, 1.015 | worse |
| set.erase.fresh | 29.82 | 29.77 | 0.998 | 0.996–1.001 | 0.999, 0.997 |  |
| set.erase.reused | 23.25 | 23.34 | 1.004 | 1.001–1.007 | 1.002, 1.006 | worse |
| set.find.fresh | 4.879 | 4.881 | 1.000 | 0.999–1.001 | 1.000, 1.001 |  |
| set.find.reused | 4.882 | 4.883 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| set.insert.fresh | 28.84 | 28.88 | 1.001 | 0.999–1.004 | 1.001, 1.002 |  |
| set.insert.reused | 22.84 | 22.82 | 0.999 | 0.997–1.001 | 1.003, 0.995 |  |
| str.append | 1.716 | 1.542 | 0.899 | 0.895–0.902 | 0.906, 0.891 |  |
| str.assign_long | 2.716 | 2.713 | 0.999 | 0.998–1.000 | 1.000, 0.998 |  |
| str.assign_short | 0.9297 | 0.9427 | 1.014 | 1.010–1.018 | 1.022, 1.006 | worse |
| str.assign_short.copy | 0.6196 | 0.6214 | 1.003 | 0.998–1.008 | 1.011, 0.995 |  |
| str.assign_short.view | 0.9351 | 0.9424 | 1.008 | 1.005–1.011 | 1.014, 1.001 | worse |
| vec.iterate | 0.03387 | 0.03382 | 0.998 | 0.994–1.003 | 1.001, 0.996 |  |
| vec.mid_erase | 40.52 | 40.37 | 0.996 | 0.995–0.998 | 0.995, 0.997 |  |
| vec.mid_insert | 41.2 | 40.87 | 0.992 | 0.989–0.995 | 1.000, 0.984 |  |
| vec.push_back | 0.2927 | 0.289 | 0.987 | 0.976–0.999 | 0.992, 0.982 |  |
| vec.push_back_grow | 0.282 | 0.282 | 1.000 | 0.992–1.008 | 1.001, 0.999 |  |

XOffset slower at t1 than t0 (worse by the rule): reset 1.022; set.erase.reused 1.004; str.assign_short 1.014; str.assign_short.view 1.008

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.712 | 0.714 | 1.003 | 1.000–1.005 | 1.010, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.756 | 0.998 | 0.994–1.001 | 1.000, 0.996 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.369 | 0.371 | 1.006 | 1.002–1.009 | 1.020, 0.991 |  |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.945 | 1.004 | 1.001–1.008 | 1.001, 1.008 | worse |
| map.find.fresh | std::flat_map | G9 | 0.786 | 0.786 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.873 | 0.874 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| nested.build.ord | std | G9 | 0.340 | 0.333 | 0.980 | 0.976–0.984 | 0.984, 0.976 |  |
| reset | std | G9 | 0.181 | 0.175 | 0.967 | 0.959–0.976 | 0.997, 0.938 |  |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.090 | 1.000 | 0.998–1.003 | 1.005, 0.995 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.755 | 1.000 | 0.999–1.002 | 0.999, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 1.001 | 1.002 | 1.000–1.004 | 1.002, 1.001 |  |
| str.append | std::string | G9 | 0.426 | 0.429 | 1.008 | 1.002–1.013 | 1.017, 0.998 |  |
| str.assign_long | std::string | G9 | 0.862 | 0.863 | 1.001 | 0.999–1.003 | 1.003, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.002 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.iterate | std::vector | G9 | 1.035 | 1.035 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.013 | 1.017 | 1.004 | 1.002–1.007 | 1.006, 1.003 | worse |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.990 | 0.999 | 0.996–1.002 | 1.002, 0.995 |  |
| vec.push_back | std::vector | G9 | 1.103 | 1.115 | 1.011 | 1.004–1.018 | 1.026, 0.997 | worse? |
| vec.push_back_grow | std::vector | G9 | 0.769 | 0.775 | 1.007 | 1.003–1.011 | 1.016, 0.998 |  |
| hero.mid_erase.plain | std::vector | report | 3.987 | 4.004 | 1.004 | 1.001–1.008 | 1.009, 1.000 |  |
| hero.mid_insert.plain | std::vector | report | 4.188 | 4.180 | 0.998 | 0.995–1.001 | 1.003, 0.993 |  |
| hero.push_back_grow.plain | std::vector | report | 0.407 | 0.410 | 1.007 | 1.004–1.010 | 1.016, 0.998 |  |
| map.erase.fresh | std::map | report | 0.698 | 0.692 | 0.992 | 0.989–0.996 | 0.994, 0.991 |  |
| map.erase.reused | std::flat_map | report | 1.087 | 1.088 | 1.000 | 0.998–1.003 | 0.997, 1.004 |  |
| map.erase.reused | std::map | report | 1.319 | 1.281 | 0.972 | 0.964–0.979 | 0.960, 0.983 |  |
| map.find.fresh | std::map | report | 0.315 | 0.314 | 0.999 | 0.995–1.003 | 0.997, 1.001 |  |
| map.find.reused | std::flat_map | report | 0.786 | 0.785 | 1.000 | 0.997–1.002 | 0.997, 1.002 |  |
| map.find.reused | std::map | report | 0.699 | 0.730 | 1.044 | 1.040–1.049 | 1.062, 1.027 | worse |
| map.insert.fresh | std::map | report | 0.737 | 0.740 | 1.004 | 1.001–1.007 | 1.005, 1.003 | worse |
| map.insert.reused | std::flat_map | report | 1.008 | 1.008 | 0.999 | 0.996–1.003 | 0.994, 1.005 |  |
| map.insert.reused | std::map | report | 1.725 | 1.710 | 0.991 | 0.986–0.996 | 0.961, 1.022 |  |
| nested.build.plain | std | report | 0.346 | 0.341 | 0.984 | 0.979–0.988 | 0.987, 0.981 |  |
| set.erase.fresh | std::set | report | 0.633 | 0.630 | 0.996 | 0.993–1.000 | 1.000, 0.992 |  |
| set.erase.reused | std::flat_set | report | 1.148 | 1.148 | 1.001 | 0.998–1.003 | 0.998, 1.004 |  |
| set.erase.reused | std::set | report | 1.083 | 1.087 | 1.003 | 0.995–1.012 | 1.000, 1.006 |  |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.003 | 0.998–1.008 | 1.002, 1.003 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.find.reused | std::set | report | 0.669 | 0.674 | 1.008 | 1.003–1.012 | 0.970, 1.047 |  |
| set.insert.fresh | std::set | report | 0.640 | 0.640 | 1.000 | 0.997–1.003 | 1.006, 0.994 |  |
| set.insert.reused | std::flat_set | report | 1.083 | 1.083 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| set.insert.reused | std::set | report | 1.337 | 1.319 | 0.986 | 0.978–0.994 | 0.955, 1.018 |  |
| str.assign_short | std::string | report | 1.828 | 1.828 | 1.000 | 0.998–1.003 | 0.998, 1.002 |  |
| str.assign_short.copy | std::string | report | 1.192 | 1.192 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |

worse at t1a than t0a (by the rule): map.erase.fresh / std::flat_map (G9) 1.004; vec.mid_erase / std::vector (G9) 1.004; map.find.reused / std::map (report) 1.044; map.insert.fresh / std::map (report) 1.004

above 1.01 but not worse by the rule: vec.push_back / std::vector (G9) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.2 | 163.6 | 1.002 | 1.001–1.004 | 1.004, 1.001 | worse |
| hero.mid_erase.plain | 908.3 | 912.2 | 1.004 | 1.002–1.007 | 1.003, 1.006 | worse |
| hero.mid_insert.ord | 230.1 | 229.9 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| hero.mid_insert.plain | 1256 | 1257 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| hero.push_back_grow.ord | 32.92 | 33.03 | 1.003 | 1.001–1.005 | 1.002, 1.005 | worse |
| hero.push_back_grow.plain | 36.36 | 36.4 | 1.001 | 0.999–1.003 | 1.001, 1.002 |  |
| map.erase.fresh | 33.01 | 33.18 | 1.005 | 1.002–1.008 | 1.014, 0.997 |  |
| map.erase.reused | 29.65 | 29.68 | 1.001 | 0.999–1.003 | 0.999, 1.003 |  |
| map.find.fresh | 5.135 | 5.133 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| map.find.reused | 5.156 | 5.156 | 1.000 | 0.998–1.002 | 0.998, 1.002 |  |
| map.insert.fresh | 32.27 | 32.27 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.insert.reused | 29.59 | 29.54 | 0.998 | 0.995–1.001 | 0.995, 1.002 |  |
| nested.build.ord | 292.5 | 293.2 | 1.002 | 1.000–1.005 | 1.000, 1.005 |  |
| nested.build.plain | 297 | 296.6 | 0.999 | 0.995–1.002 | 0.995, 1.002 |  |
| reset | 30.18 | 30.24 | 1.002 | 0.996–1.009 | 0.997, 1.007 |  |
| set.erase.fresh | 29.83 | 29.81 | 0.999 | 0.997–1.002 | 1.000, 0.999 |  |
| set.erase.reused | 23.29 | 23.26 | 0.999 | 0.996–1.001 | 0.995, 1.002 |  |
| set.find.fresh | 4.881 | 4.882 | 1.000 | 0.999–1.002 | 0.999, 1.001 |  |
| set.find.reused | 4.882 | 4.879 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| set.insert.fresh | 28.88 | 28.85 | 0.999 | 0.998–1.001 | 1.001, 0.997 |  |
| set.insert.reused | 22.89 | 22.92 | 1.001 | 0.999–1.004 | 1.004, 0.999 |  |
| str.append | 1.545 | 1.554 | 1.006 | 1.001–1.010 | 1.008, 1.004 | worse |
| str.assign_long | 2.942 | 2.945 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| str.assign_short | 0.9364 | 0.9493 | 1.014 | 1.009–1.018 | 1.007, 1.021 | worse |
| str.assign_short.copy | 0.6181 | 0.6233 | 1.008 | 1.004–1.013 | 1.000, 1.017 |  |
| str.assign_short.view | 0.9351 | 0.9426 | 1.008 | 1.005–1.011 | 0.999, 1.017 |  |
| vec.iterate | 0.03334 | 0.03323 | 0.997 | 0.994–0.999 | 0.996, 0.998 |  |
| vec.mid_erase | 40.39 | 40.58 | 1.005 | 1.003–1.007 | 1.008, 1.001 | worse |
| vec.mid_insert | 40.44 | 40.37 | 0.998 | 0.996–1.000 | 1.000, 0.997 |  |
| vec.push_back | 0.2718 | 0.2739 | 1.008 | 1.005–1.011 | 1.011, 1.004 | worse |
| vec.push_back_grow | 0.2744 | 0.2757 | 1.005 | 1.003–1.007 | 1.016, 0.994 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.002; hero.mid_erase.plain 1.004; hero.push_back_grow.ord 1.003; str.append 1.006; str.assign_short 1.014; vec.mid_erase 1.005; vec.push_back 1.008

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| reset (XOffset ns/op) | 1.022 | 1.002 | 0.996–1.009 | not worse |
| set.erase.reused (XOffset ns/op) | 1.004 | 0.999 | 0.996–1.001 | not worse |
| str.assign_short (XOffset ns/op) | 1.014 | 1.014 | 1.009–1.018 | still worse |
| str.assign_short.view (XOffset ns/op) | 1.008 | 1.008 | 1.005–1.011 | not worse |
| map.find.reused / std::map (report) | 1.010 | 1.044 | 1.040–1.049 | still worse |
| map.insert.fresh / std::map (report) | 1.013 | 1.004 | 1.001–1.007 | still worse |
| map.insert.reused / std::flat_map (report) | 1.004 | 0.999 | 0.996–1.003 | not worse |
| set.erase.reused / std::flat_set (report) | 1.003 | 1.001 | 0.998–1.003 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 164.71 | 164.06 | 0.996 | 227.13 | 1.001 |
| hero.mid_erase.plain | 910.11 | 907.90 | 0.998 | 227.73 | 1.003 |
| hero.mid_insert.ord | 229.05 | 227.50 | 0.993 | 307.92 | 1.000 |
| hero.mid_insert.plain | 1265.86 | 1265.85 | 1.000 | 297.93 | 1.001 |
| hero.push_back_grow.ord | 33.02 | 32.96 | 0.998 | 89.13 | 1.002 |
| hero.push_back_grow.plain | 36.45 | 36.39 | 0.999 | 88.85 | 1.001 |
| map.erase.fresh | 32.98 | 32.97 | 1.000 | 35.21 | 1.000 |
| map.erase.reused | 29.61 | 29.60 | 1.000 | 27.29 | 0.999 |
| map.find.fresh | 5.13 | 5.13 | 1.000 | 6.55 | 1.000 |
| map.find.reused | 5.17 | 5.15 | 0.997 | 6.55 | 1.000 |
| map.insert.fresh | 32.23 | 32.23 | 1.000 | 36.73 | 0.999 |
| map.insert.reused | 29.53 | 29.51 | 0.999 | 29.30 | 1.000 |
| nested.build.ord | 289.79 | 289.47 | 0.999 | 862.70 | 1.004 |
| nested.build.plain | 294.59 | 295.56 | 1.003 | 852.62 | 1.000 |
| reset | 30.17 | 30.83 | 1.022 | 165.48 | 1.011 |
| set.erase.fresh | 29.76 | 29.72 | 0.999 | 27.31 | 0.998 |
| set.erase.reused | 23.06 | 23.10 | 1.002 | 20.24 | 0.996 |
| set.find.fresh | 4.88 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.77 | 28.82 | 1.002 | 28.73 | 0.999 |
| set.insert.reused | 22.80 | 22.77 | 0.998 | 21.05 | 1.000 |
| str.append | 1.69 | 1.52 | 0.899 | 3.59 | 1.013 |
| str.assign_long | 2.71 | 2.71 | 0.999 | 3.40 | 1.000 |
| str.assign_short | 0.92 | 0.94 | 1.024 | 0.50 | 1.021 |
| str.assign_short.copy | 0.61 | 0.61 | 0.995 | 0.52 | 1.009 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.73 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.999 | 0.03 | 1.000 |
| vec.mid_erase | 40.30 | 40.31 | 1.000 | 39.76 | 1.003 |
| vec.mid_insert | 41.27 | 40.77 | 0.988 | 41.40 | 1.000 |
| vec.push_back | 0.29 | 0.29 | 0.988 | 0.27 | 1.001 |
| vec.push_back_grow | 0.28 | 0.28 | 0.996 | 0.37 | 1.005 |

## analyze t0 (G9 at f0308ee7)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.099 |  1.108 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.020 |  1.025 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.69 |  0.997 |  0.999 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.36 |      39.63 |  1.015 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.72 |     227.86 |  0.719 |  0.721 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     307.94 |  0.754 |  0.757 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.00 |      89.60 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    909.50 |     227.86 |  4.014 |  4.028 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1266.27 |     295.24 |  4.227 |  4.240 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.46 |      89.36 |  0.410 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.834 |  1.837 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.798 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.341 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.188 |  1.190 | report  |
| str.append                   | std::string    | G9     |  808 |      1.69 |       3.61 |  0.472 |  0.474 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.70 |  0.877 |  0.879 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.95 |  0.729 |  0.731 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.13 |       6.54 |  0.786 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.13 |      16.42 |  0.313 |  0.315 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.936 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.20 |  0.697 |  0.699 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.73 |  1.001 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.08 |  0.638 |  0.640 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.30 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.093 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.79 |  0.634 |  0.637 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.30 |  1.007 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      17.33 |  1.703 |  1.710 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.17 |       6.55 |  0.786 |  0.789 | report  |
| map.find.reused              | std::map       | report |  808 |      5.17 |       7.42 |  0.693 |  0.696 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.086 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      21.65 |  1.353 |  1.361 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.085 |  1.087 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.46 |  1.294 |  1.305 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.37 |  0.762 |  0.764 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.26 |  1.146 |  1.149 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      19.37 |  1.168 |  1.176 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.34 |     864.59 |  0.337 |  0.338 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     164.06 |  0.182 |  0.183 | pass    |
| nested.build.plain           | std            | report |  808 |    294.28 |     854.16 |  0.346 |  0.348 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.120 |  1.124 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.766 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.61 |  0.995 |  0.997 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.014 |  1.015 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     228.52 |  0.713 |  0.715 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     307.30 |  0.751 |  0.753 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.96 |      91.19 |  0.364 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.20 |     228.52 |  3.981 |  3.998 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1267.58 |     306.64 |  4.201 |  4.216 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.38 |      89.97 |  0.406 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.835 |  1.838 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.796 |  0.798 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.190 |  1.192 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.62 |  0.739 |  0.741 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.13 |       6.55 |  0.783 |  0.785 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.13 |      16.41 |  0.313 |  0.314 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.20 |  0.696 |  0.698 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.73 |  1.003 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.41 |  0.634 |  0.636 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.70 |      27.34 |  1.090 |  1.093 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.70 |      46.96 |  0.630 |  0.633 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.21 |  1.011 |  1.012 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      17.50 |  1.690 |  1.695 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.14 |       6.55 |  0.786 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.14 |       7.31 |  0.700 |  0.702 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.43 |  1.080 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      22.05 |  1.326 |  1.335 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.00 |  1.084 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.17 |  1.308 |  1.318 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.58 |  0.741 |  0.743 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.18 |  1.150 |  1.153 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      20.02 |  1.118 |  1.128 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.34 |     856.78 |  0.335 |  0.337 | pass    |
| reset                        | std            | G9     |  808 |     30.61 |     163.41 |  0.182 |  0.184 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     841.16 |  0.348 |  0.349 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-10-27T18:53:51Z block 1 start: waited 180s load 1.31 WindowServer 1.2% remote 6
2026-10-27T18:56:55Z block 2 start: waited 180s load 1.50 WindowServer 1.3% remote 6
2026-10-27T19:00:00Z block 3 start: waited 180s load 1.26 WindowServer 1.3% remote 6
2026-10-27T19:03:04Z block 4 start: waited 180s load 1.47 WindowServer 1.2% remote 6
```
