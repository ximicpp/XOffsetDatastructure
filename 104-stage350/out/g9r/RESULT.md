# G9R3 result (2026-11-10T20:48:25Z)

t0 1162e37bf40fad6074b9e4cc9c7ff109a8b35288 (v3.4.0 1162e37b (previous release tag)); t1 0cbc130decd722fc1baaa4c96882c1085e5b20a3 (STAGE350 head (v3.5.0 0cbc130d)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 1162e37bf40fad6074b9e4cc9c7ff109a8b35288 tree 28785dd4ff283d8aa549739172844f43c60366a9 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.4.0 1162e37b (previous release tag))
t1 0cbc130decd722fc1baaa4c96882c1085e5b20a3 tree e44176fae42c6a107169775060d5b1b61937c90d typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (STAGE350 head (v3.5.0 0cbc130d))
t1: diff over t0 sha256 7250fdfd3e27c4c79f59feafc0da79e2d8382284f2df0728929c6c7272dc00a9 ( 55 files changed, 4113 insertions(+), 406 deletions(-))
t1: library header diff over t0:  1 file changed, 110 insertions(+), 36 deletions(-)
t1: first-parent commits over t0:
  0cbc130d Merge pull request #252 from ximicpp/claude/release-3.5.0
  46e0b4dc Merge pull request #251 from ximicpp/claude/delta-capacity-fix-20261110
  f0557545 Merge pull request #247 from ximicpp/claude/uplink-bounds-view-20261110
  21dc9708 Merge pull request #249 from ximicpp/claude/d3-product-20261110
  38a741f3 Merge pull request #248 from ximicpp/claude/w2-up-send-x86-20261110
  e71d4ed3 Merge pull request #246 from ximicpp/claude/anchor-v3.4.0
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 b1883605c93d07ff18696b926e47a161e4ae8b15a00ee31a2bd220cb168dc9f5
reloc_bench t1 sha256 99f7fa449d9a06306c8a4422a5cdc0a0167c3c046a9e2a6e7244d74f7ec0be37
reloc_bench t0a sha256 997e1491ce3abb1086e01e3c2d5b33db1c36e4c1655f6c984edb32204baaa8b5
reloc_bench t1a sha256 5d3f8e550c5c4f818b636c844538e9d3b43389af69817490dfe76c3f843c720d
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.729 | 0.727 | 0.997 | 0.994–1.000 | 1.002, 0.992 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.760 | 1.001 | 0.997–1.004 | 0.999, 1.003 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.365 | 0.360 | 0.986 | 0.982–0.991 | 1.017, 0.957 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.941 | 0.999 | 0.997–1.001 | 0.999, 0.998 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.860 | 0.855 | 0.994 | 0.992–0.997 | 0.995, 0.994 |  |
| nested.build.ord | std | G9 | 0.321 | 0.313 | 0.976 | 0.971–0.981 | 0.945, 1.008 |  |
| reset | std | G9 | 0.199 | 0.191 | 0.962 | 0.949–0.976 | 0.870, 1.065 |  |
| set.erase.fresh | std::flat_set | G9 | 1.085 | 1.086 | 1.001 | 0.998–1.004 | 1.006, 0.996 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 1.000, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.985 | 0.989 | 1.004 | 1.002–1.006 | 1.003, 1.006 | worse |
| str.append | std::string | G9 | 0.429 | 0.436 | 1.016 | 1.012–1.020 | 1.032, 1.000 | worse |
| str.assign_long | std::string | G9 | 0.800 | 0.866 | 1.082 | 1.079–1.084 | 1.081, 1.083 | worse |
| str.assign_short.view | std::string | G9 | 0.344 | 0.342 | 0.995 | 0.991–0.999 | 0.992, 0.998 |  |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.012 | 1.017 | 1.005 | 1.003–1.008 | 1.003, 1.008 | worse |
| vec.mid_insert | std::vector | G9 | 0.957 | 0.951 | 0.994 | 0.992–0.997 | 0.995, 0.994 |  |
| vec.push_back | std::vector | G9 | 1.116 | 1.129 | 1.012 | 1.008–1.016 | 1.007, 1.016 | worse |
| vec.push_back_grow | std::vector | G9 | 0.765 | 0.765 | 1.001 | 0.995–1.006 | 1.004, 0.998 |  |
| hero.mid_erase.plain | std::vector | report | 4.027 | 4.067 | 1.010 | 1.007–1.013 | 1.024, 0.996 |  |
| hero.mid_insert.plain | std::vector | report | 4.183 | 4.234 | 1.012 | 1.008–1.016 | 1.014, 1.010 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.398 | 0.988 | 0.984–0.993 | 1.022, 0.956 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.727 | 0.977 | 0.974–0.979 | 0.978, 0.975 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.079 | 0.999 | 0.996–1.002 | 1.004, 0.994 |  |
| map.erase.reused | std::map | report | 1.633 | 1.601 | 0.980 | 0.973–0.987 | 0.969, 0.991 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 0.999 | 0.995–1.003 | 1.002, 0.996 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.001 | 0.998–1.003 | 1.002, 1.000 |  |
| map.find.reused | std::map | report | 0.714 | 0.720 | 1.008 | 1.004–1.011 | 0.988, 1.028 |  |
| map.insert.fresh | std::map | report | 0.739 | 0.737 | 0.997 | 0.994–1.001 | 1.000, 0.994 |  |
| map.insert.reused | std::flat_map | report | 1.017 | 1.023 | 1.006 | 1.003–1.009 | 1.006, 1.005 | worse |
| map.insert.reused | std::map | report | 1.860 | 1.832 | 0.985 | 0.980–0.989 | 0.961, 1.009 |  |
| nested.build.plain | std | report | 0.334 | 0.322 | 0.966 | 0.962–0.970 | 0.938, 0.995 |  |
| set.erase.fresh | std::set | report | 0.671 | 0.665 | 0.992 | 0.988–0.995 | 0.988, 0.995 |  |
| set.erase.reused | std::flat_set | report | 1.133 | 1.133 | 1.000 | 0.997–1.003 | 1.001, 1.000 |  |
| set.erase.reused | std::set | report | 1.343 | 1.351 | 1.006 | 1.000–1.013 | 0.985, 1.028 |  |
| set.find.fresh | std::set | report | 0.292 | 0.299 | 1.025 | 1.020–1.030 | 1.029, 1.020 | worse |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.998–1.001 | 1.001, 0.999 |  |
| set.find.reused | std::set | report | 0.685 | 0.679 | 0.992 | 0.987–0.996 | 1.006, 0.977 |  |
| set.insert.fresh | std::set | report | 0.649 | 0.640 | 0.987 | 0.984–0.990 | 0.980, 0.993 |  |
| set.insert.reused | std::flat_set | report | 1.111 | 1.113 | 1.002 | 0.999–1.005 | 1.004, 1.000 |  |
| set.insert.reused | std::set | report | 1.498 | 1.432 | 0.956 | 0.948–0.964 | 0.938, 0.975 |  |
| str.assign_short | std::string | report | 1.764 | 1.758 | 0.997 | 0.995–0.999 | 0.997, 0.996 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.149 | 0.998 | 0.995–1.001 | 1.000, 0.997 |  |

worse at t1 than t0 (by the rule): set.insert.fresh / std::flat_set (G9) 1.004; str.append / std::string (G9) 1.016; str.assign_long / std::string (G9) 1.082; vec.mid_erase / std::vector (G9) 1.005; vec.push_back / std::vector (G9) 1.012; hero.mid_insert.plain / std::vector (report) 1.012; map.insert.reused / std::flat_map (report) 1.006; set.find.fresh / std::set (report) 1.025

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164 | 164.3 | 1.001 | 0.999–1.003 | 0.999, 1.003 |  |
| hero.mid_erase.plain | 903.1 | 914.5 | 1.013 | 1.010–1.015 | 1.022, 1.003 | worse |
| hero.mid_insert.ord | 227.3 | 228 | 1.003 | 1.001–1.005 | 1.001, 1.006 | worse |
| hero.mid_insert.plain | 1249 | 1264 | 1.012 | 1.011–1.014 | 1.012, 1.013 | worse |
| hero.push_back_grow.ord | 32.06 | 31.79 | 0.992 | 0.990–0.993 | 0.997, 0.987 |  |
| hero.push_back_grow.plain | 35.26 | 35.07 | 0.995 | 0.993–0.996 | 1.004, 0.986 |  |
| map.erase.fresh | 33.04 | 32.97 | 0.998 | 0.997–1.000 | 0.998, 0.998 |  |
| map.erase.reused | 29.66 | 29.65 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| map.find.fresh | 5.293 | 5.287 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| map.find.reused | 5.308 | 5.306 | 1.000 | 0.998–1.002 | 0.999, 1.000 |  |
| map.insert.fresh | 31.8 | 31.77 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| map.insert.reused | 29.89 | 30.07 | 1.006 | 1.004–1.009 | 1.005, 1.007 | worse |
| nested.build.ord | 265.4 | 263.7 | 0.993 | 0.990–0.996 | 0.993, 0.994 |  |
| nested.build.plain | 274.3 | 269.8 | 0.983 | 0.981–0.986 | 0.983, 0.984 |  |
| reset | 25.39 | 24.32 | 0.958 | 0.955–0.961 | 0.952, 0.964 |  |
| set.erase.fresh | 29.82 | 29.8 | 0.999 | 0.997–1.002 | 1.001, 0.997 |  |
| set.erase.reused | 23.15 | 23.05 | 0.996 | 0.993–0.999 | 0.997, 0.995 |  |
| set.find.fresh | 4.882 | 4.885 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| set.find.reused | 4.879 | 4.882 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| set.insert.fresh | 28.49 | 28.58 | 1.003 | 1.002–1.005 | 1.004, 1.003 | worse |
| set.insert.reused | 23.51 | 23.46 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| str.append | 1.526 | 1.523 | 0.998 | 0.995–1.002 | 1.011, 0.986 |  |
| str.assign_long | 2.717 | 2.936 | 1.081 | 1.079–1.082 | 1.081, 1.081 | worse |
| str.assign_short | 0.9213 | 0.9219 | 1.001 | 0.999–1.002 | 1.002, 0.999 |  |
| str.assign_short.copy | 0.6088 | 0.6027 | 0.990 | 0.986–0.994 | 0.998, 0.982 |  |
| str.assign_short.view | 0.9324 | 0.9319 | 0.999 | 0.996–1.003 | 1.002, 0.997 |  |
| vec.iterate | 0.0332 | 0.03316 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |
| vec.mid_erase | 40.07 | 40.4 | 1.008 | 1.006–1.010 | 1.007, 1.009 | worse |
| vec.mid_insert | 40.34 | 40.39 | 1.001 | 1.000–1.003 | 1.002, 1.000 |  |
| vec.push_back | 0.2905 | 0.2919 | 1.005 | 0.994–1.016 | 1.009, 1.002 |  |
| vec.push_back_grow | 0.2748 | 0.2747 | 1.000 | 0.993–1.006 | 1.001, 0.998 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.plain 1.013; hero.mid_insert.ord 1.003; hero.mid_insert.plain 1.012; map.insert.reused 1.006; set.insert.fresh 1.003; str.assign_long 1.081; vec.mid_erase 1.008

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.723 | 0.720 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.759 | 1.001 | 0.998–1.005 | 1.003, 1.000 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.357 | 0.367 | 1.028 | 1.024–1.032 | 1.035, 1.021 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.943 | 0.941 | 0.998 | 0.996–1.000 | 1.001, 0.995 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.999 | 0.996–1.001 | 0.999, 0.998 |  |
| map.insert.fresh | std::flat_map | G9 | 0.847 | 0.855 | 1.010 | 1.007–1.012 | 1.007, 1.012 | worse |
| nested.build.ord | std | G9 | 0.311 | 0.314 | 1.009 | 1.004–1.014 | 1.018, 1.000 | worse |
| reset | std | G9 | 0.192 | 0.181 | 0.939 | 0.923–0.956 | 1.007, 0.876 |  |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.094 | 1.005 | 1.003–1.007 | 1.004, 1.006 | worse |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.754 | 0.997 | 0.994–1.000 | 0.997, 0.996 |  |
| set.insert.fresh | std::flat_set | G9 | 0.992 | 0.987 | 0.996 | 0.993–0.998 | 0.991, 1.000 |  |
| str.append | std::string | G9 | 0.424 | 0.439 | 1.035 | 1.030–1.040 | 1.025, 1.045 | worse |
| str.assign_long | std::string | G9 | 0.865 | 0.866 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.340 | 0.993 | 0.987–0.998 | 0.985, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.998–1.000 | 0.999, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.015 | 1.017 | 1.002 | 1.000–1.005 | 1.007, 0.997 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.958 | 0.997 | 0.995–1.000 | 0.995, 1.000 |  |
| vec.push_back | std::vector | G9 | 1.109 | 1.119 | 1.009 | 1.005–1.013 | 1.019, 0.999 |  |
| vec.push_back_grow | std::vector | G9 | 0.757 | 0.766 | 1.012 | 1.007–1.016 | 1.003, 1.020 | worse |
| hero.mid_erase.plain | std::vector | report | 4.021 | 4.032 | 1.003 | 0.999–1.006 | 1.005, 1.000 |  |
| hero.mid_insert.plain | std::vector | report | 4.154 | 4.204 | 1.012 | 1.008–1.016 | 1.015, 1.009 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.396 | 0.407 | 1.027 | 1.024–1.030 | 1.027, 1.027 | worse |
| map.erase.fresh | std::map | report | 0.734 | 0.718 | 0.978 | 0.974–0.981 | 0.959, 0.997 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.078 | 0.997 | 0.994–1.000 | 0.996, 0.998 |  |
| map.erase.reused | std::map | report | 1.594 | 1.574 | 0.987 | 0.980–0.994 | 0.993, 0.981 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 1.000 | 0.997–1.004 | 0.999, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.809 | 1.002 | 0.999–1.004 | 1.002, 1.001 |  |
| map.find.reused | std::map | report | 0.704 | 0.741 | 1.052 | 1.046–1.057 | 1.017, 1.088 | worse |
| map.insert.fresh | std::map | report | 0.733 | 0.736 | 1.003 | 1.000–1.006 | 1.016, 0.990 |  |
| map.insert.reused | std::flat_map | report | 1.023 | 1.031 | 1.008 | 1.005–1.011 | 1.001, 1.014 | worse |
| map.insert.reused | std::map | report | 1.851 | 1.870 | 1.010 | 1.005–1.015 | 0.991, 1.030 | worse? |
| nested.build.plain | std | report | 0.322 | 0.328 | 1.018 | 1.014–1.023 | 1.038, 0.999 | worse? |
| set.erase.fresh | std::set | report | 0.664 | 0.660 | 0.994 | 0.990–0.998 | 0.996, 0.991 |  |
| set.erase.reused | std::flat_set | report | 1.134 | 1.134 | 1.000 | 0.997–1.004 | 0.997, 1.004 |  |
| set.erase.reused | std::set | report | 1.323 | 1.280 | 0.968 | 0.960–0.976 | 0.993, 0.943 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.000 | 0.995–1.005 | 0.999, 1.001 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| set.find.reused | std::set | report | 0.669 | 0.662 | 0.991 | 0.986–0.995 | 0.973, 1.008 |  |
| set.insert.fresh | std::set | report | 0.646 | 0.642 | 0.993 | 0.990–0.996 | 1.004, 0.982 |  |
| set.insert.reused | std::flat_set | report | 1.114 | 1.110 | 0.997 | 0.994–1.000 | 0.990, 1.003 |  |
| set.insert.reused | std::set | report | 1.468 | 1.486 | 1.012 | 1.003–1.021 | 0.993, 1.031 | worse? |
| str.assign_short | std::string | report | 1.770 | 1.764 | 0.997 | 0.994–0.999 | 0.998, 0.995 |  |
| str.assign_short.copy | std::string | report | 1.149 | 1.153 | 1.003 | 1.000–1.006 | 1.005, 1.001 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.028; map.insert.fresh / std::flat_map (G9) 1.010; nested.build.ord / std (G9) 1.009; set.erase.fresh / std::flat_set (G9) 1.005; str.append / std::string (G9) 1.035; vec.push_back_grow / std::vector (G9) 1.012; hero.mid_insert.plain / std::vector (report) 1.012; hero.push_back_grow.plain / std::vector (report) 1.027; map.find.reused / std::map (report) 1.052; map.insert.reused / std::flat_map (report) 1.008

above 1.01 but not worse by the rule: map.insert.reused / std::map (report) 1.010; nested.build.plain / std (report) 1.018; set.insert.reused / std::set (report) 1.012

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.7 | 163.8 | 1.000 | 0.998–1.002 | 1.002, 0.998 |  |
| hero.mid_erase.plain | 904.2 | 909.3 | 1.006 | 1.003–1.008 | 1.006, 1.005 | worse |
| hero.mid_insert.ord | 227.9 | 228.7 | 1.003 | 1.002–1.005 | 1.004, 1.003 | worse |
| hero.mid_insert.plain | 1240 | 1252 | 1.010 | 1.009–1.011 | 1.010, 1.010 | worse |
| hero.push_back_grow.ord | 31.57 | 31.7 | 1.004 | 1.002–1.006 | 1.005, 1.003 | worse |
| hero.push_back_grow.plain | 34.99 | 35.16 | 1.005 | 1.003–1.007 | 1.005, 1.005 | worse |
| map.erase.fresh | 33.03 | 33.03 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.erase.reused | 29.71 | 29.59 | 0.996 | 0.994–0.998 | 0.994, 0.999 |  |
| map.find.fresh | 5.288 | 5.288 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.find.reused | 5.302 | 5.314 | 1.002 | 1.000–1.004 | 1.000, 1.004 | worse |
| map.insert.fresh | 31.78 | 31.73 | 0.998 | 0.997–1.000 | 0.998, 0.999 |  |
| map.insert.reused | 30.17 | 30.36 | 1.006 | 1.004–1.008 | 1.002, 1.011 | worse |
| nested.build.ord | 264.6 | 259.6 | 0.981 | 0.978–0.984 | 0.979, 0.983 |  |
| nested.build.plain | 271.8 | 268.7 | 0.989 | 0.986–0.992 | 0.990, 0.987 |  |
| reset | 25.77 | 24.16 | 0.938 | 0.930–0.945 | 0.924, 0.952 |  |
| set.erase.fresh | 29.77 | 29.81 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| set.erase.reused | 23.17 | 23.09 | 0.996 | 0.993–1.000 | 0.996, 0.997 |  |
| set.find.fresh | 4.891 | 4.879 | 0.998 | 0.996–0.999 | 0.997, 0.998 |  |
| set.find.reused | 4.882 | 4.882 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.insert.fresh | 28.7 | 28.51 | 0.993 | 0.992–0.995 | 0.988, 0.999 |  |
| set.insert.reused | 23.55 | 23.46 | 0.996 | 0.994–0.998 | 0.990, 1.003 |  |
| str.append | 1.511 | 1.56 | 1.032 | 1.028–1.037 | 1.036, 1.029 | worse |
| str.assign_long | 2.94 | 2.936 | 0.999 | 0.997–1.001 | 0.999, 0.998 |  |
| str.assign_short | 0.9221 | 0.9239 | 1.002 | 1.000–1.004 | 1.003, 1.001 |  |
| str.assign_short.copy | 0.6041 | 0.6133 | 1.015 | 1.011–1.020 | 1.014, 1.016 | worse |
| str.assign_short.view | 0.9294 | 0.9323 | 1.003 | 0.999–1.007 | 1.004, 1.002 |  |
| vec.iterate | 0.03326 | 0.03327 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | 40.5 | 40.45 | 0.999 | 0.997–1.001 | 1.000, 0.997 |  |
| vec.mid_insert | 40.71 | 40.68 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| vec.push_back | 0.2721 | 0.2752 | 1.011 | 1.008–1.015 | 1.026, 0.997 | worse? |
| vec.push_back_grow | 0.2682 | 0.2705 | 1.008 | 1.005–1.011 | 1.005, 1.012 | worse |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.006; hero.mid_insert.ord 1.003; hero.mid_insert.plain 1.010; hero.push_back_grow.ord 1.004; hero.push_back_grow.plain 1.005; map.find.reused 1.002; map.insert.reused 1.006; str.append 1.032; str.assign_short.copy 1.015; vec.push_back_grow 1.008

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.plain (XOffset ns/op) | 1.013 | 1.006 | 1.003–1.008 | still worse |
| hero.mid_insert.ord (XOffset ns/op) | 1.003 | 1.003 | 1.002–1.005 | still worse |
| hero.mid_insert.plain (XOffset ns/op) | 1.012 | 1.010 | 1.009–1.011 | still worse |
| map.insert.reused (XOffset ns/op) | 1.006 | 1.006 | 1.004–1.008 | still worse |
| set.insert.fresh (XOffset ns/op) | 1.003 | 0.993 | 0.992–0.995 | not worse |
| str.assign_long (XOffset ns/op) | 1.081 | 0.999 | 0.997–1.001 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.008 | 0.999 | 0.997–1.001 | not worse |
| hero.mid_insert.plain / std::vector (report) | 1.012 | 1.012 | 1.008–1.016 | still worse |
| map.insert.reused / std::flat_map (report) | 1.006 | 1.008 | 1.005–1.011 | still worse |
| set.find.fresh / std::set (report) | 1.025 | 1.000 | 0.995–1.005 | not worse |
| set.insert.fresh / std::flat_set (G9) | 1.004 | 0.996 | 0.993–0.998 | not worse |
| str.append / std::string (G9) | 1.016 | 1.035 | 1.030–1.040 | still worse |
| str.assign_long / std::string (G9) | 1.082 | 1.001 | 0.998–1.003 | not worse |
| vec.mid_erase / std::vector (G9) | 1.005 | 1.002 | 1.000–1.005 | not worse |
| vec.push_back / std::vector (G9) | 1.012 | 1.009 | 1.005–1.013 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.82 | 163.90 | 1.001 | 224.10 | 0.999 |
| hero.mid_erase.plain | 902.77 | 914.07 | 1.013 | 224.15 | 1.006 |
| hero.mid_insert.ord | 226.56 | 227.86 | 1.006 | 295.76 | 1.002 |
| hero.mid_insert.plain | 1246.24 | 1261.78 | 1.012 | 301.16 | 1.010 |
| hero.push_back_grow.ord | 31.97 | 31.74 | 0.993 | 87.60 | 1.003 |
| hero.push_back_grow.plain | 35.20 | 34.98 | 0.994 | 87.42 | 1.005 |
| map.erase.fresh | 32.97 | 32.96 | 1.000 | 34.99 | 0.999 |
| map.erase.reused | 29.53 | 29.54 | 1.000 | 27.31 | 0.998 |
| map.find.fresh | 5.29 | 5.29 | 1.000 | 6.55 | 1.000 |
| map.find.reused | 5.30 | 5.30 | 1.000 | 6.55 | 1.001 |
| map.insert.fresh | 31.72 | 31.74 | 1.000 | 36.86 | 1.000 |
| map.insert.reused | 29.84 | 30.02 | 1.006 | 29.38 | 1.008 |
| nested.build.ord | 265.21 | 263.00 | 0.992 | 825.22 | 0.981 |
| nested.build.plain | 273.76 | 269.04 | 0.983 | 820.46 | 0.988 |
| reset | 25.30 | 24.23 | 0.958 | 127.18 | 0.927 |
| set.erase.fresh | 29.79 | 29.77 | 0.999 | 27.41 | 1.000 |
| set.erase.reused | 22.93 | 22.90 | 0.999 | 20.36 | 0.994 |
| set.find.fresh | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.find.reused | 4.87 | 4.88 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.48 | 28.56 | 1.003 | 28.93 | 0.994 |
| set.insert.reused | 23.46 | 23.41 | 0.998 | 21.09 | 0.996 |
| str.append | 1.51 | 1.51 | 0.998 | 3.53 | 1.016 |
| str.assign_long | 2.71 | 2.93 | 1.082 | 3.39 | 0.999 |
| str.assign_short | 0.92 | 0.92 | 1.000 | 0.52 | 1.000 |
| str.assign_short.copy | 0.60 | 0.60 | 0.999 | 0.52 | 0.999 |
| str.assign_short.view | 0.93 | 0.93 | 0.999 | 2.71 | 0.998 |
| vec.iterate | 0.03 | 0.03 | 1.001 | 0.03 | 0.995 |
| vec.mid_erase | 39.95 | 40.18 | 1.006 | 39.53 | 1.000 |
| vec.mid_insert | 40.31 | 40.32 | 1.000 | 42.15 | 1.000 |
| vec.push_back | 0.29 | 0.29 | 1.003 | 0.26 | 1.016 |
| vec.push_back_grow | 0.27 | 0.27 | 0.989 | 0.36 | 1.006 |

## analyze t0 (G9 at the anchor 1162e37b)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.116 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.55 |  1.012 |  1.014 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.95 |  0.729 |  0.731 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     295.58 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.02 |      86.63 |  0.365 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    902.67 |     224.61 |  4.027 |  4.040 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1246.41 |     304.05 |  4.183 |  4.201 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.26 |  0.403 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.156 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.51 |  0.429 |  0.431 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      36.87 |  0.860 |  0.862 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.72 |  0.739 |  0.742 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.36 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.942 |  0.945 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.986 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      43.78 |  0.649 |  0.651 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.67 |  0.292 |  0.294 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.671 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.87 |      29.38 |  1.017 |  1.020 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.87 |      16.03 |  1.860 |  1.868 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.35 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.080 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.633 |  1.644 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.111 |  1.115 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.46 |  1.498 |  1.509 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.99 |  0.685 |  0.688 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.133 |  1.136 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.93 |  1.343 |  1.352 | report  |
| nested.build.ord             | std            | G9     |  808 |    265.62 |     825.53 |  0.321 |  0.322 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     117.19 |  0.199 |  0.201 | pass    |
| nested.build.plain           | std            | report |  808 |    273.44 |     822.94 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.129 |  1.134 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.48 |  0.951 |  0.954 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.71 |  1.017 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     297.20 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.70 |      86.59 |  0.360 |  0.362 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.06 |     224.61 |  4.067 |  4.079 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1261.72 |     303.39 |  4.234 |  4.249 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.91 |      86.87 |  0.398 |  0.400 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.758 |  1.760 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.149 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.436 |  0.437 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.11 |  0.855 |  0.857 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.81 |  0.737 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.39 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.00 |  0.727 |  0.729 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.89 |  0.989 |  0.991 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      44.43 |  0.640 |  0.643 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.36 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.43 |  0.665 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.38 |  1.023 |  1.025 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.11 |  1.832 |  1.841 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.35 |  0.720 |  0.722 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.079 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.82 |  1.601 |  1.613 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.113 |  1.116 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.79 |  1.432 |  1.445 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.15 |  0.679 |  0.682 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.26 |  1.133 |  1.137 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.60 |  1.351 |  1.360 | report  |
| nested.build.ord             | std            | G9     |  808 |    263.03 |     825.53 |  0.313 |  0.315 | pass    |
| reset                        | std            | G9     |  808 |     24.09 |     117.83 |  0.191 |  0.194 | pass    |
| nested.build.plain           | std            | report |  808 |    269.53 |     824.22 |  0.322 |  0.324 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-10T20:39:07Z block 1 start: waited 180s load 1.31 WindowServer 1.1% remote 4
2026-11-10T20:42:12Z block 2 start: waited 180s load 1.42 WindowServer 1.9% remote 4
2026-11-10T20:45:17Z block 3 start: waited 180s load 1.19 WindowServer 1.6% remote 4
2026-11-10T20:48:21Z block 4 start: waited 180s load 1.58 WindowServer 1.1% remote 4
```
