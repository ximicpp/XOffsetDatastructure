# G9R3 result (2026-11-12T09:34:31Z)

t0 0cbc130decd722fc1baaa4c96882c1085e5b20a3 (v3.5.0 0cbc130d (previous release tag)); t1 372c117e2a116d523796f0365a0ceb720ba010cc (STAGE360 head (v3.6.0 372c117e)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 0cbc130decd722fc1baaa4c96882c1085e5b20a3 tree e44176fae42c6a107169775060d5b1b61937c90d typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.5.0 0cbc130d (previous release tag))
t1 372c117e2a116d523796f0365a0ceb720ba010cc tree 081ab403d12ebca5dcab22114b48094f0b2fd692 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (STAGE360 head (v3.6.0 372c117e))
t1: diff over t0 sha256 167ee68b542f09a8d6b13bbf7e07cf17c512094b57d3f25c53900f1c945d6b91 ( 111 files changed, 8296 insertions(+), 1236 deletions(-))
t1: library header diff over t0:  2 files changed, 1590 insertions(+), 265 deletions(-)
t1: first-parent commits over t0:
  372c117e Merge pull request #270: Release 3.6.0
  7d16b446 Merge pull request #267 from ximicpp/claude/grow-sized-reserve-20261111
  687db6a8 Merge pull request #268 from ximicpp/claude/project-thread-xg93tg
  bb537d0d Merge #266: fix #265 free-table load growth (SLG M D3) + root<T>() cache refill, redo of #223 (L class)
  cf9b63e5 Merge #256: batch operations, build<T>, no-crash fuzz and stress mode (growth policy part 4)
  7e4ad666 Merge #254: named field accessors and a proxy root (growth policy part 3)
  aad6cd86 Merge #265: free table, 4-byte bin heads over a shared word pool, 12 KiB -> 2.4 KiB (L class)
  9e173149 Merge #264: shared deltas, no erroneous-behaviour fill of encoder scratch; cheaper byte_mask (L class)
  ccd7d68a Merge #263: D3 smaller per-delivery code and cheaper update snapshots (L class)
  18445198 Merge pull request #261 from ximicpp/claude/grow-fast-proxy-20261111
  ce658e8e Merge #260: D3 changed-member encoding for the small-server publish and send cost (L class)
  1f9a8e92 Merge pull request #262 from ximicpp/claude/project-thread-jf4hzl
  cb3b5119 Merge pull request #259 from ximicpp/claude/mem-retire-discard
  d8c5018d Merge #257: local_validate validation tiers, pr by default (class 0)
  b5a14f7c Merge pull request #258 from ximicpp/claude/project-thread-xg93tg
  9e636300 Merge pull request #255 from ximicpp/claude/grow-relocating-storage-20261111
  7a0d3d96 Merge pull request #253 from ximicpp/claude/anchor-v3.5.0
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 99f7fa449d9a06306c8a4422a5cdc0a0167c3c046a9e2a6e7244d74f7ec0be37
reloc_bench t1 sha256 9fb6eb4bb31f75b8ab7a1b1998bd18918328fd9e6f2d06e49ca86e56f7b4f038
reloc_bench t0a sha256 5d3f8e550c5c4f818b636c844538e9d3b43389af69817490dfe76c3f843c720d
reloc_bench t1a sha256 1566cf243663fc9f33c2909449bdc12929212faa74de1b84a9f6bf06a068e360
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.738 | 1.013 | 1.008–1.019 | 1.004, 1.023 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.761 | 0.997 | 0.992–1.002 | 0.989, 1.006 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.346 | 0.945 | 0.940–0.950 | 0.924, 0.966 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.941 | 0.998 | 0.994–1.003 | 1.003, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.806 | 0.997 | 0.993–1.001 | 0.996, 0.998 |  |
| map.insert.fresh | std::flat_map | G9 | 0.854 | 0.842 | 0.986 | 0.984–0.988 | 0.986, 0.986 |  |
| nested.build.ord | std | G9 | 0.321 | 0.308 | 0.957 | 0.951–0.964 | 0.954, 0.960 |  |
| reset | std | G9 | 0.194 | 0.184 | 0.944 | 0.928–0.960 | 0.854, 1.044 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.082 | 0.995 | 0.991–0.999 | 0.993, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.753 | 0.753 | 0.999 | 0.995–1.004 | 0.997, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 0.987 | 0.989 | 1.002 | 0.998–1.006 | 1.000, 1.005 |  |
| str.append | std::string | G9 | 0.431 | 0.419 | 0.972 | 0.968–0.976 | 0.970, 0.974 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.864 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.002 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.017 | 0.997 | 0.993–1.001 | 0.995, 0.998 |  |
| vec.mid_insert | std::vector | G9 | 0.953 | 0.992 | 1.041 | 1.037–1.045 | 1.039, 1.043 | worse |
| vec.push_back | std::vector | G9 | 1.122 | 1.128 | 1.006 | 1.002–1.009 | 1.009, 1.002 | worse |
| vec.push_back_grow | std::vector | G9 | 0.773 | 0.764 | 0.988 | 0.984–0.993 | 0.983, 0.993 |  |
| hero.mid_erase.plain | std::vector | report | 4.077 | 4.050 | 0.993 | 0.989–0.997 | 0.987, 1.000 |  |
| hero.mid_insert.plain | std::vector | report | 4.202 | 4.192 | 0.998 | 0.993–1.002 | 0.993, 1.003 |  |
| hero.push_back_grow.plain | std::vector | report | 0.405 | 0.380 | 0.939 | 0.935–0.943 | 0.919, 0.959 |  |
| map.erase.fresh | std::map | report | 0.735 | 0.735 | 1.001 | 0.996–1.006 | 0.978, 1.024 |  |
| map.erase.reused | std::flat_map | report | 1.083 | 1.075 | 0.992 | 0.988–0.997 | 0.987, 0.998 |  |
| map.erase.reused | std::map | report | 1.617 | 1.610 | 0.996 | 0.988–1.003 | 0.952, 1.041 |  |
| map.find.fresh | std::map | report | 0.324 | 0.319 | 0.984 | 0.979–0.989 | 0.982, 0.986 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.995–1.004 | 1.000, 0.999 |  |
| map.find.reused | std::map | report | 0.729 | 0.708 | 0.971 | 0.965–0.977 | 0.982, 0.960 |  |
| map.insert.fresh | std::map | report | 0.741 | 0.736 | 0.992 | 0.989–0.995 | 0.988, 0.996 |  |
| map.insert.reused | std::flat_map | report | 1.022 | 1.017 | 0.996 | 0.991–1.000 | 0.993, 0.998 |  |
| map.insert.reused | std::map | report | 1.852 | 1.818 | 0.982 | 0.973–0.990 | 0.967, 0.996 |  |
| nested.build.plain | std | report | 0.327 | 0.316 | 0.966 | 0.960–0.971 | 0.955, 0.977 |  |
| set.erase.fresh | std::set | report | 0.669 | 0.664 | 0.992 | 0.987–0.997 | 0.964, 1.021 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.135 | 0.998 | 0.993–1.002 | 0.993, 1.002 |  |
| set.erase.reused | std::set | report | 1.343 | 1.306 | 0.972 | 0.964–0.980 | 0.947, 0.998 |  |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.007 | 1.001–1.013 | 1.008, 1.005 | worse |
| set.find.reused | std::flat_set | report | 0.754 | 0.750 | 0.994 | 0.990–0.999 | 0.989, 1.000 |  |
| set.find.reused | std::set | report | 0.659 | 0.765 | 1.162 | 1.155–1.168 | 1.141, 1.182 | worse |
| set.insert.fresh | std::set | report | 0.648 | 0.644 | 0.994 | 0.990–0.999 | 0.988, 1.000 |  |
| set.insert.reused | std::flat_set | report | 1.113 | 1.108 | 0.995 | 0.991–0.999 | 0.994, 0.996 |  |
| set.insert.reused | std::set | report | 1.475 | 1.463 | 0.992 | 0.982–1.001 | 0.972, 1.011 |  |
| str.assign_short | std::string | report | 1.757 | 1.771 | 1.008 | 1.005–1.012 | 1.013, 1.004 | worse |
| str.assign_short.copy | std::string | report | 1.148 | 1.151 | 1.003 | 0.999–1.006 | 1.000, 1.006 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.013; vec.mid_insert / std::vector (G9) 1.041; vec.push_back / std::vector (G9) 1.006; set.find.fresh / std::set (report) 1.007; set.find.reused / std::set (report) 1.162; str.assign_short / std::string (report) 1.008

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 165.8 | 167.3 | 1.009 | 1.005–1.013 | 0.996, 1.022 |  |
| hero.mid_erase.plain | 923.1 | 910.2 | 0.986 | 0.982–0.990 | 0.970, 1.003 |  |
| hero.mid_insert.ord | 231 | 228.6 | 0.990 | 0.985–0.994 | 0.978, 1.002 |  |
| hero.mid_insert.plain | 1270 | 1251 | 0.986 | 0.983–0.989 | 0.975, 0.996 |  |
| hero.push_back_grow.ord | 32.23 | 30.66 | 0.951 | 0.948–0.955 | 0.936, 0.966 |  |
| hero.push_back_grow.plain | 35.3 | 33.74 | 0.956 | 0.954–0.958 | 0.947, 0.964 |  |
| map.erase.fresh | 32.99 | 33.31 | 1.010 | 1.006–1.014 | 1.019, 1.000 | worse |
| map.erase.reused | 29.65 | 29.82 | 1.006 | 1.002–1.009 | 1.012, 1.000 |  |
| map.find.fresh | 5.292 | 5.344 | 1.010 | 1.006–1.013 | 1.018, 1.001 | worse |
| map.find.reused | 5.307 | 5.364 | 1.011 | 1.007–1.015 | 1.023, 0.998 | worse? |
| map.insert.fresh | 31.76 | 31.68 | 0.998 | 0.996–0.999 | 0.998, 0.998 |  |
| map.insert.reused | 30.05 | 30.33 | 1.009 | 1.005–1.013 | 1.019, 1.000 |  |
| nested.build.ord | 264.1 | 262.3 | 0.993 | 0.988–0.998 | 1.009, 0.977 |  |
| nested.build.plain | 270.3 | 269.9 | 0.998 | 0.994–1.003 | 1.012, 0.985 |  |
| reset | 25.06 | 24.83 | 0.991 | 0.985–0.996 | 1.032, 0.951 |  |
| set.erase.fresh | 29.76 | 29.93 | 1.006 | 1.003–1.009 | 1.013, 0.999 |  |
| set.erase.reused | 23.16 | 23.39 | 1.010 | 1.006–1.014 | 1.016, 1.005 | worse |
| set.find.fresh | 4.882 | 4.935 | 1.011 | 1.007–1.015 | 1.022, 1.000 | worse |
| set.find.reused | 4.877 | 4.923 | 1.009 | 1.006–1.013 | 1.017, 1.002 | worse |
| set.insert.fresh | 28.59 | 28.93 | 1.012 | 1.008–1.016 | 1.022, 1.002 | worse |
| set.insert.reused | 23.5 | 23.62 | 1.005 | 1.001–1.009 | 1.014, 0.997 |  |
| str.append | 1.52 | 1.517 | 0.998 | 0.994–1.001 | 1.006, 0.990 |  |
| str.assign_long | 2.936 | 2.938 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| str.assign_short | 0.9225 | 0.9274 | 1.005 | 1.003–1.007 | 1.013, 0.998 |  |
| str.assign_short.copy | 0.6087 | 0.6069 | 0.997 | 0.993–1.001 | 0.999, 0.995 |  |
| str.assign_short.view | 0.9299 | 0.9326 | 1.003 | 1.001–1.005 | 1.005, 1.001 | worse |
| vec.iterate | 0.03357 | 0.03316 | 0.988 | 0.983–0.992 | 0.976, 1.000 |  |
| vec.mid_erase | 40.84 | 40.38 | 0.989 | 0.985–0.993 | 0.976, 1.001 |  |
| vec.mid_insert | 40.81 | 40.36 | 0.989 | 0.985–0.993 | 0.978, 1.000 |  |
| vec.push_back | 0.2923 | 0.2886 | 0.987 | 0.977–0.997 | 0.973, 1.002 |  |
| vec.push_back_grow | 0.2779 | 0.274 | 0.986 | 0.981–0.992 | 0.981, 0.991 |  |

XOffset slower at t1 than t0 (worse by the rule): map.erase.fresh 1.010; map.find.fresh 1.010; set.erase.reused 1.010; set.find.fresh 1.011; set.find.reused 1.009; set.insert.fresh 1.012; str.assign_short.view 1.003

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.736 | 1.016 | 1.013–1.018 | 1.012, 1.019 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.757 | 0.992 | 0.988–0.996 | 0.992, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.349 | 0.954 | 0.951–0.957 | 0.969, 0.939 |  |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.945 | 1.004 | 1.001–1.007 | 1.002, 1.006 | worse |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.807 | 1.001 | 0.998–1.003 | 1.001, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.856 | 0.845 | 0.987 | 0.985–0.990 | 0.987, 0.988 |  |
| nested.build.ord | std | G9 | 0.308 | 0.311 | 1.007 | 1.001–1.013 | 1.021, 0.993 |  |
| reset | std | G9 | 0.189 | 0.178 | 0.942 | 0.927–0.958 | 0.956, 0.929 |  |
| set.erase.fresh | std::flat_set | G9 | 1.095 | 1.095 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.998 | 0.996–1.001 | 0.997, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.989 | 0.988 | 0.999 | 0.996–1.001 | 0.997, 1.001 |  |
| str.append | std::string | G9 | 0.430 | 0.430 | 0.999 | 0.995–1.003 | 0.994, 1.004 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.811 | 0.938 | 0.936–0.940 | 0.939, 0.937 |  |
| str.assign_short.view | std::string | G9 | 0.341 | 0.343 | 1.006 | 1.002–1.009 | 0.996, 1.016 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.022 | 1.001 | 0.999–1.003 | 0.994, 1.008 |  |
| vec.mid_insert | std::vector | G9 | 0.956 | 0.994 | 1.040 | 1.038–1.042 | 1.038, 1.043 | worse |
| vec.push_back | std::vector | G9 | 1.120 | 1.113 | 0.994 | 0.991–0.996 | 0.990, 0.998 |  |
| vec.push_back_grow | std::vector | G9 | 0.775 | 0.762 | 0.983 | 0.980–0.986 | 0.987, 0.979 |  |
| hero.mid_erase.plain | std::vector | report | 4.042 | 4.025 | 0.996 | 0.993–0.999 | 0.996, 0.995 |  |
| hero.mid_insert.plain | std::vector | report | 4.205 | 4.180 | 0.994 | 0.991–0.998 | 0.993, 0.996 |  |
| hero.push_back_grow.plain | std::vector | report | 0.408 | 0.388 | 0.952 | 0.950–0.954 | 0.959, 0.946 |  |
| map.erase.fresh | std::map | report | 0.735 | 0.737 | 1.003 | 1.000–1.006 | 0.978, 1.028 |  |
| map.erase.reused | std::flat_map | report | 1.082 | 1.079 | 0.997 | 0.995–0.999 | 0.995, 0.998 |  |
| map.erase.reused | std::map | report | 1.595 | 1.568 | 0.983 | 0.976–0.989 | 0.950, 1.017 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.999 | 0.995–1.002 | 0.999, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.805 | 0.996 | 0.994–0.998 | 1.000, 0.992 |  |
| map.find.reused | std::map | report | 0.730 | 0.729 | 0.998 | 0.994–1.002 | 1.002, 0.994 |  |
| map.insert.fresh | std::map | report | 0.740 | 0.731 | 0.987 | 0.984–0.990 | 1.000, 0.975 |  |
| map.insert.reused | std::flat_map | report | 1.034 | 1.018 | 0.985 | 0.982–0.987 | 0.987, 0.982 |  |
| map.insert.reused | std::map | report | 1.847 | 1.844 | 0.999 | 0.992–1.005 | 0.999, 0.998 |  |
| nested.build.plain | std | report | 0.316 | 0.316 | 1.000 | 0.994–1.006 | 1.011, 0.990 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.669 | 0.998 | 0.995–1.002 | 0.977, 1.020 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.134 | 0.997 | 0.994–0.999 | 0.993, 1.001 |  |
| set.erase.reused | std::set | report | 1.291 | 1.286 | 0.996 | 0.987–1.006 | 0.988, 1.005 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.999 | 0.995–1.004 | 0.999, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.755 | 0.998 | 0.995–1.001 | 0.997, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.687 | 1.015 | 1.011–1.019 | 1.017, 1.013 | worse |
| set.insert.fresh | std::set | report | 0.643 | 0.641 | 0.997 | 0.994–1.000 | 0.997, 0.997 |  |
| set.insert.reused | std::flat_set | report | 1.108 | 1.111 | 1.003 | 1.000–1.005 | 1.003, 1.002 | worse |
| set.insert.reused | std::set | report | 1.459 | 1.462 | 1.002 | 0.993–1.012 | 0.993, 1.011 |  |
| str.assign_short | std::string | report | 1.765 | 1.764 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.150 | 0.999 | 0.996–1.001 | 0.998, 1.000 |  |

worse at t1a than t0a (by the rule): hero.mid_erase.ord / std::vector (G9) 1.016; map.erase.fresh / std::flat_map (G9) 1.004; vec.mid_insert / std::vector (G9) 1.040; set.find.reused / std::set (report) 1.015; set.insert.reused / std::flat_set (report) 1.003

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 166.8 | 1.018 | 1.017–1.020 | 1.017, 1.019 | worse |
| hero.mid_erase.plain | 906.4 | 905.2 | 0.999 | 0.996–1.001 | 1.000, 0.997 |  |
| hero.mid_insert.ord | 229.3 | 228.8 | 0.998 | 0.996–1.000 | 0.994, 1.002 |  |
| hero.mid_insert.plain | 1253 | 1250 | 0.998 | 0.996–0.999 | 0.993, 1.003 |  |
| hero.push_back_grow.ord | 31.67 | 30.56 | 0.965 | 0.963–0.967 | 0.967, 0.963 |  |
| hero.push_back_grow.plain | 35.18 | 33.96 | 0.965 | 0.964–0.967 | 0.962, 0.968 |  |
| map.erase.fresh | 33.05 | 33 | 0.999 | 0.996–1.001 | 1.001, 0.997 |  |
| map.erase.reused | 29.68 | 29.6 | 0.997 | 0.996–0.999 | 0.997, 0.997 |  |
| map.find.fresh | 5.286 | 5.283 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.find.reused | 5.307 | 5.303 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.fresh | 31.77 | 31.76 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.reused | 30.4 | 30.1 | 0.990 | 0.988–0.993 | 0.995, 0.985 |  |
| nested.build.ord | 260.7 | 260.3 | 0.999 | 0.997–1.000 | 1.006, 0.991 |  |
| nested.build.plain | 268.1 | 266.4 | 0.994 | 0.991–0.996 | 0.993, 0.995 |  |
| reset | 24.06 | 24.92 | 1.035 | 1.030–1.041 | 1.038, 1.033 | worse |
| set.erase.fresh | 29.77 | 29.8 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| set.erase.reused | 23.19 | 23.07 | 0.995 | 0.992–0.998 | 0.990, 0.999 |  |
| set.find.fresh | 4.888 | 4.881 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| set.find.reused | 4.887 | 4.877 | 0.998 | 0.995–1.001 | 0.996, 1.000 |  |
| set.insert.fresh | 28.56 | 28.53 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.insert.reused | 23.45 | 23.46 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| str.append | 1.528 | 1.538 | 1.006 | 1.002–1.010 | 1.015, 0.998 |  |
| str.assign_long | 2.936 | 2.936 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| str.assign_short | 0.9218 | 0.925 | 1.003 | 1.001–1.005 | 1.007, 1.000 |  |
| str.assign_short.copy | 0.6114 | 0.6144 | 1.005 | 1.001–1.009 | 1.013, 0.997 |  |
| str.assign_short.view | 0.9326 | 0.9336 | 1.001 | 0.998–1.005 | 1.003, 0.999 |  |
| vec.iterate | 0.03323 | 0.03326 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| vec.mid_erase | 40.67 | 40.65 | 0.999 | 0.998–1.001 | 0.993, 1.006 |  |
| vec.mid_insert | 40.67 | 40.66 | 1.000 | 0.998–1.001 | 0.997, 1.002 |  |
| vec.push_back | 0.272 | 0.2716 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| vec.push_back_grow | 0.2716 | 0.2685 | 0.988 | 0.986–0.990 | 0.990, 0.987 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.018; reset 1.035

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.erase.fresh (XOffset ns/op) | 1.010 | 0.999 | 0.996–1.001 | not worse |
| map.find.fresh (XOffset ns/op) | 1.010 | 0.999 | 0.998–1.001 | not worse |
| set.erase.reused (XOffset ns/op) | 1.010 | 0.995 | 0.992–0.998 | not worse |
| set.find.fresh (XOffset ns/op) | 1.011 | 0.999 | 0.997–1.000 | not worse |
| set.find.reused (XOffset ns/op) | 1.009 | 0.998 | 0.995–1.001 | not worse |
| set.insert.fresh (XOffset ns/op) | 1.012 | 0.999 | 0.997–1.001 | not worse |
| str.assign_short.view (XOffset ns/op) | 1.003 | 1.001 | 0.998–1.005 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.013 | 1.016 | 1.013–1.018 | still worse |
| set.find.fresh / std::set (report) | 1.007 | 0.999 | 0.995–1.004 | not worse |
| set.find.reused / std::set (report) | 1.162 | 1.015 | 1.011–1.019 | still worse |
| str.assign_short / std::string (report) | 1.008 | 0.999 | 0.997–1.002 | not worse |
| vec.mid_insert / std::vector (G9) | 1.041 | 1.040 | 1.038–1.042 | still worse |
| vec.push_back / std::vector (G9) | 1.006 | 0.994 | 0.991–0.996 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.98 | 167.15 | 1.019 | 225.33 | 1.019 |
| hero.mid_erase.plain | 916.30 | 910.32 | 0.993 | 224.36 | 0.997 |
| hero.mid_insert.ord | 228.39 | 227.95 | 0.998 | 298.82 | 0.999 |
| hero.mid_insert.plain | 1258.54 | 1248.95 | 0.992 | 301.89 | 0.998 |
| hero.push_back_grow.ord | 31.87 | 30.60 | 0.960 | 87.25 | 0.965 |
| hero.push_back_grow.plain | 35.18 | 33.66 | 0.957 | 86.94 | 0.965 |
| map.erase.fresh | 32.96 | 32.98 | 1.001 | 34.89 | 0.999 |
| map.erase.reused | 29.56 | 29.48 | 0.997 | 27.28 | 0.996 |
| map.find.fresh | 5.29 | 5.30 | 1.002 | 6.55 | 0.998 |
| map.find.reused | 5.29 | 5.31 | 1.003 | 6.56 | 1.000 |
| map.insert.fresh | 31.74 | 31.67 | 0.998 | 37.05 | 1.000 |
| map.insert.reused | 30.04 | 30.01 | 0.999 | 29.36 | 0.990 |
| nested.build.ord | 263.51 | 260.07 | 0.987 | 820.47 | 1.000 |
| nested.build.plain | 270.02 | 266.66 | 0.988 | 824.46 | 0.993 |
| reset | 25.13 | 24.56 | 0.977 | 128.42 | 1.037 |
| set.erase.fresh | 29.75 | 29.72 | 0.999 | 27.35 | 1.000 |
| set.erase.reused | 23.03 | 23.03 | 1.000 | 20.31 | 0.998 |
| set.find.fresh | 4.87 | 4.88 | 1.002 | 6.46 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.56 | 28.60 | 1.001 | 28.93 | 1.001 |
| set.insert.reused | 23.48 | 23.45 | 0.999 | 21.08 | 1.000 |
| str.append | 1.49 | 1.50 | 1.003 | 3.48 | 1.005 |
| str.assign_long | 2.93 | 2.93 | 1.000 | 3.39 | 1.001 |
| str.assign_short | 0.92 | 0.92 | 1.001 | 0.52 | 1.000 |
| str.assign_short.copy | 0.60 | 0.60 | 1.001 | 0.52 | 0.995 |
| str.assign_short.view | 0.93 | 0.93 | 1.001 | 2.71 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.999 | 0.03 | 1.002 |
| vec.mid_erase | 40.27 | 40.25 | 0.999 | 39.62 | 1.000 |
| vec.mid_insert | 40.33 | 40.31 | 0.999 | 42.44 | 0.999 |
| vec.push_back | 0.29 | 0.29 | 0.992 | 0.26 | 0.998 |
| vec.push_back_grow | 0.27 | 0.27 | 0.995 | 0.35 | 0.989 |

## analyze t0 (G9 at the anchor 0cbc130d)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.122 |  1.127 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.773 |  0.777 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.56 |  0.953 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.020 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.728 |  0.733 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     298.17 |  0.763 |  0.767 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      87.20 |  0.366 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.42 |     224.61 |  4.077 |  4.097 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.202 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      86.59 |  0.405 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.757 |  1.763 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.431 |  0.433 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.11 |  0.854 |  0.856 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.64 |  0.741 |  0.744 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.809 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.38 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.83 |  0.942 |  0.945 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.735 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.97 |  0.987 |  0.989 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      43.95 |  0.648 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.753 |  0.755 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.39 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.669 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.38 |  1.022 |  1.024 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.19 |  1.852 |  1.858 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.41 |  0.729 |  0.733 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.083 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.617 |  1.628 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.113 |  1.117 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.79 |  1.475 |  1.489 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.754 |  0.755 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.41 |  0.659 |  0.661 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.138 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.76 |  1.343 |  1.353 | report  |
| nested.build.ord             | std            | G9     |  808 |    263.03 |     824.22 |  0.321 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     118.48 |  0.194 |  0.197 | pass    |
| nested.build.plain           | std            | report |  808 |    269.53 |     824.22 |  0.327 |  0.328 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.128 |  1.131 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.764 |  0.766 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.69 |  0.992 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.79 |  1.017 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    167.31 |     227.22 |  0.738 |  0.740 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     297.53 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.60 |      87.89 |  0.346 |  0.347 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.25 |  4.050 |  4.060 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1251.95 |     298.17 |  4.192 |  4.206 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.65 |      87.77 |  0.380 |  0.382 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.771 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      37.52 |  0.842 |  0.844 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.89 |  0.736 |  0.738 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.30 |       6.56 |  0.806 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.30 |      16.67 |  0.319 |  0.320 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.941 |  0.946 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.52 |  0.735 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.64 |      28.97 |  0.989 |  0.994 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.64 |      44.35 |  0.644 |  0.648 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.753 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.38 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.42 |  1.082 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.27 |  0.664 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.017 |  1.023 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.03 |  1.818 |  1.840 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.31 |       6.56 |  0.808 |  0.812 | report  |
| map.find.reused              | std::map       | report |  808 |      5.31 |       7.41 |  0.708 |  0.712 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.075 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.90 |  1.610 |  1.624 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.16 |  1.108 |  1.112 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.54 |  1.463 |  1.478 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.49 |  0.750 |  0.754 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.28 |  0.765 |  0.771 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.135 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.58 |  1.306 |  1.318 | report  |
| nested.build.ord             | std            | G9     |  808 |    259.12 |     824.22 |  0.308 |  0.310 | pass    |
| reset                        | std            | G9     |  808 |     24.75 |     117.84 |  0.184 |  0.187 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     825.53 |  0.316 |  0.319 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-12T09:25:13Z block 1 start: waited 180s load 1.34 WindowServer 1.3% remote 6
2026-11-12T09:28:17Z block 2 start: waited 180s load 1.41 WindowServer 1.2% remote 6
2026-11-12T09:31:22Z block 3 start: waited 180s load 1.08 WindowServer 1.1% remote 6
2026-11-12T09:34:27Z block 4 start: waited 180s load 1.12 WindowServer 1.2% remote 6
```
