# G9R3 result (2026-11-05T07:09:32Z)

t0 51a4815b7802ec125e2752847b69fd2a87ab7259 (v3.3.0 51a4815b (previous release tag)); t1 ee506d3f54a979ededdd71cdc5cca5857ff38f11 (STAGE331 head (v3.3.1 ee506d3f)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 51a4815b7802ec125e2752847b69fd2a87ab7259 tree 254473076fde1ce98ca5529e307417cd8c0b5b3a typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.3.0 51a4815b (previous release tag))
t1 ee506d3f54a979ededdd71cdc5cca5857ff38f11 tree becbab81e91384527bc313392fb407f61f690d36 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (STAGE331 head (v3.3.1 ee506d3f))
t1: diff over t0 sha256 c551f5740d9c40af08e7c413286635e3547490a2e037238f6631d2c7d10c6de6 ( 42 files changed, 1805 insertions(+), 128 deletions(-))
t1: library header diff over t0:  1 file changed, 133 insertions(+), 12 deletions(-)
t1: first-parent commits over t0:
  ee506d3f Merge pull request #238 from ximicpp/claude/release-3.3.1 (class 0): release 3.3.1
  eb4f0468 Merge pull request #226 from ximicpp/claude/max-capacity-limit-20261104 (class S)
  0723bc0b Merge pull request #221 from ximicpp/claude/handle-derived-fastpath-20261104 (class S)
  522f6dfb Merge pull request #233 from ximicpp/claude/slg-eb-native-20261105 (class 0): native browse delta rows NB (report only)
  84243ba5 Merge pull request #223 from ximicpp/claude/root-refill-20261104 (class L): root<T>() refills its cache on the checked path
  d9eb9559 Merge pull request #228 from ximicpp/claude/trust-admit-p1-20261104 (class L)
  7601372d Merge pull request #235 from ximicpp/claude/perf-gate-cache-floor-20261105
  9b214e19 Merge pull request #234 from ximicpp/claude/perf-gate-cache-sim-zeros-20261105 (class 0): cache-sim trailing zero counts
  fb4b96c9 Merge pull request #232 from ximicpp/claude/perf-gate-cache-sim-20261105 (class 0): perf_gate --cache-sim warning item
  62c0269e Merge pull request #231 from ximicpp/claude/cloud-only-merge-rules (class 0): merges judged in the cloud
  9f39390e Merge pull request #230 from ximicpp/claude/anchor-3.3.0 (class 0): anchor at v3.3.0, releases judged in the cloud
  c9fae222 Merge pull request #227 from ximicpp/claude/slg-flat-held-index-20261104 (class 0)
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 25d9006a12d1b47b14d53a9469e60526ebc8412c01b190def92b93c2b983a797
reloc_bench t1 sha256 70ef4974d92872cec20fcdc0207e6af7aca7ed4abc17338d8e001729a41b3762
reloc_bench t0a sha256 972c1725256dc635098dd57c1f6b270727f438c492d89778db8d403e8a292895
reloc_bench t1a sha256 d8e183035a70417d0de3f5b4430ad52d986d54b8761d9eff018bee33f8ca2159
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.721 | 0.991 | 0.988–0.994 | 0.994, 0.988 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.758 | 1.000 | 0.996–1.003 | 1.003, 0.996 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.367 | 0.356 | 0.972 | 0.967–0.976 | 0.964, 0.980 |  |
| map.erase.fresh | std::flat_map | G9 | 0.936 | 0.944 | 1.008 | 1.006–1.010 | 1.008, 1.008 | worse |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.998 | 0.996–1.000 | 1.000, 0.997 |  |
| map.insert.fresh | std::flat_map | G9 | 0.861 | 0.862 | 1.001 | 1.000–1.003 | 0.998, 1.004 |  |
| nested.build.ord | std | G9 | 0.326 | 0.324 | 0.993 | 0.990–0.996 | 0.973, 1.014 |  |
| reset | std | G9 | 0.209 | 0.204 | 0.978 | 0.960–0.996 | 1.039, 0.921 |  |
| set.erase.fresh | std::flat_set | G9 | 1.086 | 1.088 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.755 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.995 | 0.999 | 1.004 | 1.002–1.006 | 0.999, 1.009 |  |
| str.append | std::string | G9 | 0.433 | 0.436 | 1.006 | 1.002–1.011 | 1.002, 1.010 | worse |
| str.assign_long | std::string | G9 | 0.801 | 0.866 | 1.081 | 1.079–1.083 | 1.080, 1.081 | worse |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.003 | 1.000–1.005 | 1.004, 1.002 | worse |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.023 | 1.019 | 0.996 | 0.993–0.998 | 0.998, 0.994 |  |
| vec.mid_insert | std::vector | G9 | 0.959 | 0.960 | 1.001 | 0.999–1.003 | 1.000, 1.001 |  |
| vec.push_back | std::vector | G9 | 1.140 | 1.115 | 0.979 | 0.976–0.981 | 0.971, 0.986 |  |
| vec.push_back_grow | std::vector | G9 | 0.767 | 0.757 | 0.987 | 0.984–0.991 | 0.979, 0.995 |  |
| hero.mid_erase.plain | std::vector | report | 4.055 | 4.057 | 1.000 | 0.997–1.004 | 0.997, 1.004 |  |
| hero.mid_insert.plain | std::vector | report | 4.192 | 4.208 | 1.004 | 1.000–1.008 | 1.003, 1.004 |  |
| hero.push_back_grow.plain | std::vector | report | 0.410 | 0.402 | 0.982 | 0.978–0.985 | 0.958, 1.006 |  |
| map.erase.fresh | std::map | report | 0.734 | 0.730 | 0.995 | 0.992–0.998 | 0.999, 0.991 |  |
| map.erase.reused | std::flat_map | report | 1.077 | 1.079 | 1.001 | 0.998–1.004 | 1.005, 0.997 |  |
| map.erase.reused | std::map | report | 1.629 | 1.611 | 0.989 | 0.983–0.995 | 1.021, 0.958 |  |
| map.find.fresh | std::map | report | 0.325 | 0.323 | 0.995 | 0.991–0.999 | 0.997, 0.993 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.807 | 0.998 | 0.995–1.001 | 0.999, 0.997 |  |
| map.find.reused | std::map | report | 0.825 | 0.722 | 0.875 | 0.871–0.879 | 0.868, 0.883 |  |
| map.insert.fresh | std::map | report | 0.737 | 0.750 | 1.018 | 1.015–1.020 | 1.019, 1.016 | worse |
| map.insert.reused | std::flat_map | report | 1.027 | 1.035 | 1.008 | 1.005–1.010 | 1.005, 1.011 | worse |
| map.insert.reused | std::map | report | 1.847 | 1.900 | 1.028 | 1.025–1.032 | 1.018, 1.038 | worse |
| nested.build.plain | std | report | 0.338 | 0.334 | 0.986 | 0.983–0.990 | 0.980, 0.993 |  |
| set.erase.fresh | std::set | report | 0.662 | 0.661 | 0.998 | 0.994–1.002 | 1.013, 0.983 |  |
| set.erase.reused | std::flat_set | report | 1.127 | 1.127 | 1.000 | 0.995–1.004 | 1.000, 0.999 |  |
| set.erase.reused | std::set | report | 1.315 | 1.307 | 0.994 | 0.987–1.001 | 1.026, 0.964 |  |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.006 | 1.001–1.011 | 1.007, 1.005 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.998–1.004 | 1.000, 1.002 |  |
| set.find.reused | std::set | report | 0.664 | 0.654 | 0.986 | 0.981–0.991 | 0.932, 1.042 |  |
| set.insert.fresh | std::set | report | 0.654 | 0.655 | 1.002 | 0.999–1.004 | 0.998, 1.005 |  |
| set.insert.reused | std::flat_set | report | 1.109 | 1.113 | 1.004 | 1.001–1.007 | 0.999, 1.009 |  |
| set.insert.reused | std::set | report | 1.470 | 1.478 | 1.006 | 0.998–1.013 | 0.994, 1.017 |  |
| str.assign_short | std::string | report | 1.768 | 1.766 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.149 | 0.998 | 0.996–1.000 | 0.997, 1.000 |  |

worse at t1 than t0 (by the rule): map.erase.fresh / std::flat_map (G9) 1.008; str.append / std::string (G9) 1.006; str.assign_long / std::string (G9) 1.081; str.assign_short.view / std::string (G9) 1.003; map.insert.fresh / std::map (report) 1.018; map.insert.reused / std::flat_map (report) 1.008; map.insert.reused / std::map (report) 1.028; set.find.fresh / std::set (report) 1.006

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164 | 162.9 | 0.993 | 0.991–0.995 | 0.992, 0.994 |  |
| hero.mid_erase.plain | 912.8 | 912.7 | 1.000 | 0.997–1.002 | 0.997, 1.003 |  |
| hero.mid_insert.ord | 227.7 | 227.8 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| hero.mid_insert.plain | 1249 | 1257 | 1.006 | 1.005–1.008 | 1.007, 1.005 | worse |
| hero.push_back_grow.ord | 31.81 | 31.85 | 1.001 | 1.000–1.003 | 0.998, 1.004 |  |
| hero.push_back_grow.plain | 35.34 | 35.26 | 0.998 | 0.996–0.999 | 0.994, 1.001 |  |
| map.erase.fresh | 33 | 32.99 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.erase.reused | 29.61 | 29.6 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| map.find.fresh | 5.286 | 5.286 | 1.000 | 0.999–1.002 | 1.001, 0.999 |  |
| map.find.reused | 5.304 | 5.301 | 0.999 | 0.997–1.002 | 0.999, 1.000 |  |
| map.insert.fresh | 31.88 | 32 | 1.004 | 1.003–1.005 | 1.002, 1.006 | worse |
| map.insert.reused | 30.24 | 30.41 | 1.006 | 1.004–1.007 | 1.001, 1.010 | worse |
| nested.build.ord | 270.1 | 270.9 | 1.003 | 1.001–1.005 | 0.996, 1.010 |  |
| nested.build.plain | 279.3 | 279 | 0.999 | 0.996–1.002 | 0.997, 1.001 |  |
| reset | 28.15 | 28.48 | 1.012 | 1.007–1.016 | 0.991, 1.033 | worse? |
| set.erase.fresh | 29.82 | 29.84 | 1.001 | 0.998–1.004 | 1.001, 1.001 |  |
| set.erase.reused | 23.04 | 23.11 | 1.003 | 1.000–1.005 | 1.000, 1.006 |  |
| set.find.fresh | 4.882 | 4.881 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| set.find.reused | 4.884 | 4.884 | 1.000 | 0.998–1.002 | 0.999, 1.000 |  |
| set.insert.fresh | 28.81 | 28.91 | 1.004 | 1.003–1.005 | 1.000, 1.007 | worse |
| set.insert.reused | 23.4 | 23.54 | 1.006 | 1.005–1.007 | 1.002, 1.010 | worse |
| str.append | 1.539 | 1.573 | 1.022 | 1.018–1.027 | 1.013, 1.031 | worse |
| str.assign_long | 2.72 | 2.939 | 1.080 | 1.078–1.082 | 1.078, 1.083 | worse |
| str.assign_short | 0.9285 | 0.9257 | 0.997 | 0.995–0.999 | 0.986, 1.008 |  |
| str.assign_short.copy | 0.6179 | 0.6128 | 0.992 | 0.988–0.996 | 0.978, 1.005 |  |
| str.assign_short.view | 0.9358 | 0.9351 | 0.999 | 0.996–1.002 | 0.987, 1.012 |  |
| vec.iterate | 0.03315 | 0.03318 | 1.001 | 1.000–1.002 | 1.000, 1.002 |  |
| vec.mid_erase | 40.48 | 40.28 | 0.995 | 0.993–0.997 | 0.995, 0.995 |  |
| vec.mid_insert | 40.53 | 40.53 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |
| vec.push_back | 0.2962 | 0.2919 | 0.985 | 0.974–0.997 | 0.981, 0.990 |  |
| vec.push_back_grow | 0.2752 | 0.2754 | 1.001 | 0.995–1.006 | 0.994, 1.007 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_insert.plain 1.006; map.insert.fresh 1.004; map.insert.reused 1.006; set.insert.fresh 1.004; set.insert.reused 1.006; str.append 1.022; str.assign_long 1.080

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.726 | 0.721 | 0.994 | 0.991–0.996 | 0.995, 0.992 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.757 | 0.761 | 1.005 | 1.001–1.009 | 1.005, 1.005 | worse |
| hero.push_back_grow.ord | std::vector | G9 | 0.360 | 0.362 | 1.006 | 1.003–1.008 | 1.016, 0.996 |  |
| map.erase.fresh | std::flat_map | G9 | 0.944 | 0.941 | 0.997 | 0.995–0.999 | 0.997, 0.997 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.808 | 1.000 | 0.998–1.003 | 1.000, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.856 | 0.863 | 1.008 | 1.005–1.010 | 1.011, 1.004 | worse |
| nested.build.ord | std | G9 | 0.317 | 0.306 | 0.965 | 0.958–0.972 | 0.967, 0.963 |  |
| reset | std | G9 | 0.235 | 0.195 | 0.831 | 0.818–0.845 | 0.843, 0.820 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.092 | 1.000 | 0.997–1.003 | 1.001, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.998–1.001 | 1.001, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.993 | 0.994 | 0.993–0.996 | 0.989, 1.000 |  |
| str.append | std::string | G9 | 0.419 | 0.432 | 1.030 | 1.025–1.036 | 1.037, 1.024 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.866 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 1.000 | 0.998–1.003 | 0.998, 1.002 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.002 | 1.000–1.003 | 1.001, 1.002 | worse |
| vec.mid_erase | std::vector | G9 | 1.022 | 1.016 | 0.994 | 0.992–0.996 | 0.998, 0.989 |  |
| vec.mid_insert | std::vector | G9 | 0.958 | 0.957 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| vec.push_back | std::vector | G9 | 1.117 | 1.112 | 0.996 | 0.993–0.999 | 0.993, 0.998 |  |
| vec.push_back_grow | std::vector | G9 | 0.764 | 0.752 | 0.985 | 0.982–0.988 | 0.973, 0.998 |  |
| hero.mid_erase.plain | std::vector | report | 4.040 | 4.018 | 0.994 | 0.991–0.998 | 1.002, 0.987 |  |
| hero.mid_insert.plain | std::vector | report | 4.159 | 4.180 | 1.005 | 1.001–1.009 | 1.006, 1.004 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.406 | 1.007 | 1.005–1.009 | 1.014, 1.000 | worse |
| map.erase.fresh | std::map | report | 0.744 | 0.730 | 0.982 | 0.979–0.985 | 0.983, 0.980 |  |
| map.erase.reused | std::flat_map | report | 1.085 | 1.078 | 0.994 | 0.991–0.997 | 0.990, 0.997 |  |
| map.erase.reused | std::map | report | 1.581 | 1.569 | 0.992 | 0.985–0.999 | 0.988, 0.996 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 1.000 | 0.997–1.004 | 1.001, 1.000 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.002 | 1.002, 0.997 |  |
| map.find.reused | std::map | report | 0.726 | 0.733 | 1.009 | 1.004–1.013 | 1.018, 1.000 |  |
| map.insert.fresh | std::map | report | 0.718 | 0.727 | 1.012 | 1.008–1.016 | 0.998, 1.027 | worse? |
| map.insert.reused | std::flat_map | report | 1.029 | 1.032 | 1.003 | 1.000–1.005 | 1.001, 1.005 | worse |
| map.insert.reused | std::map | report | 1.861 | 1.863 | 1.001 | 0.995–1.007 | 0.970, 1.034 |  |
| nested.build.plain | std | report | 0.321 | 0.319 | 0.993 | 0.986–1.001 | 1.018, 0.969 |  |
| set.erase.fresh | std::set | report | 0.663 | 0.661 | 0.997 | 0.993–1.001 | 0.991, 1.002 |  |
| set.erase.reused | std::flat_set | report | 1.130 | 1.128 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |
| set.erase.reused | std::set | report | 1.277 | 1.276 | 0.999 | 0.991–1.007 | 1.008, 0.990 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.999 | 0.994–1.004 | 0.998, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| set.find.reused | std::set | report | 0.680 | 0.658 | 0.968 | 0.964–0.973 | 0.966, 0.970 |  |
| set.insert.fresh | std::set | report | 0.648 | 0.649 | 1.001 | 0.998–1.005 | 0.983, 1.020 |  |
| set.insert.reused | std::flat_set | report | 1.113 | 1.111 | 0.999 | 0.997–1.001 | 0.994, 1.003 |  |
| set.insert.reused | std::set | report | 1.443 | 1.430 | 0.991 | 0.982–1.001 | 0.956, 1.028 |  |
| str.assign_short | std::string | report | 1.767 | 1.766 | 1.000 | 0.999–1.000 | 0.999, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.148 | 0.997 | 0.994–1.001 | 0.996, 0.999 |  |

worse at t1a than t0a (by the rule): hero.mid_insert.ord / std::vector (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.008; str.append / std::string (G9) 1.030; vec.iterate / std::vector (G9) 1.002; hero.mid_insert.plain / std::vector (report) 1.005; hero.push_back_grow.plain / std::vector (report) 1.007; map.insert.reused / std::flat_map (report) 1.003

above 1.01 but not worse by the rule: map.insert.fresh / std::map (report) 1.012

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.1 | 164.4 | 1.001 | 1.001–1.002 | 1.002, 1.001 | worse |
| hero.mid_erase.plain | 905.7 | 903.9 | 0.998 | 0.996–1.001 | 1.003, 0.993 |  |
| hero.mid_insert.ord | 229 | 228.4 | 0.997 | 0.995–0.999 | 0.998, 0.997 |  |
| hero.mid_insert.plain | 1244 | 1245 | 1.000 | 0.999–1.002 | 1.002, 0.999 |  |
| hero.push_back_grow.ord | 31.77 | 31.82 | 1.002 | 1.000–1.003 | 1.007, 0.996 |  |
| hero.push_back_grow.plain | 35.37 | 35.49 | 1.003 | 1.002–1.005 | 1.009, 0.998 |  |
| map.erase.fresh | 33.01 | 33.03 | 1.001 | 1.000–1.002 | 1.000, 1.002 |  |
| map.erase.reused | 29.75 | 29.59 | 0.995 | 0.992–0.997 | 0.992, 0.997 |  |
| map.find.fresh | 5.29 | 5.293 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| map.find.reused | 5.305 | 5.305 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.insert.fresh | 31.99 | 31.97 | 0.999 | 0.998–1.001 | 0.998, 1.001 |  |
| map.insert.reused | 30.34 | 30.31 | 0.999 | 0.997–1.001 | 0.995, 1.004 |  |
| nested.build.ord | 269.6 | 267.7 | 0.993 | 0.991–0.995 | 0.987, 1.000 |  |
| nested.build.plain | 279.6 | 276.7 | 0.990 | 0.988–0.991 | 0.988, 0.991 |  |
| reset | 30.35 | 28.26 | 0.931 | 0.927–0.936 | 0.907, 0.956 |  |
| set.erase.fresh | 29.8 | 29.79 | 1.000 | 0.997–1.002 | 0.999, 1.001 |  |
| set.erase.reused | 23.06 | 23.07 | 1.000 | 0.998–1.003 | 1.000, 1.000 |  |
| set.find.fresh | 4.881 | 4.882 | 1.000 | 0.999–1.001 | 1.000, 1.001 |  |
| set.find.reused | 4.878 | 4.878 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| set.insert.fresh | 28.93 | 28.76 | 0.994 | 0.992–0.996 | 0.989, 0.999 |  |
| set.insert.reused | 23.47 | 23.37 | 0.996 | 0.994–0.997 | 0.993, 0.998 |  |
| str.append | 1.544 | 1.541 | 0.998 | 0.994–1.003 | 0.995, 1.001 |  |
| str.assign_long | 2.939 | 2.939 | 1.000 | 0.998–1.002 | 1.000, 1.001 |  |
| str.assign_short | 0.9314 | 0.9202 | 0.988 | 0.986–0.990 | 0.986, 0.990 |  |
| str.assign_short.copy | 0.6221 | 0.6138 | 0.987 | 0.982–0.991 | 0.989, 0.984 |  |
| str.assign_short.view | 0.9388 | 0.9289 | 0.989 | 0.987–0.992 | 0.991, 0.988 |  |
| vec.iterate | 0.03322 | 0.0333 | 1.002 | 1.001–1.004 | 1.001, 1.003 | worse |
| vec.mid_erase | 40.59 | 40.56 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| vec.mid_insert | 40.48 | 40.47 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |
| vec.push_back | 0.2717 | 0.2719 | 1.001 | 1.000–1.002 | 1.000, 1.002 |  |
| vec.push_back_grow | 0.2699 | 0.2691 | 0.997 | 0.995–1.000 | 0.990, 1.004 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.001; vec.iterate 1.002

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_insert.plain (XOffset ns/op) | 1.006 | 1.000 | 0.999–1.002 | not worse |
| map.insert.fresh (XOffset ns/op) | 1.004 | 0.999 | 0.998–1.001 | not worse |
| map.insert.reused (XOffset ns/op) | 1.006 | 0.999 | 0.997–1.001 | not worse |
| set.insert.fresh (XOffset ns/op) | 1.004 | 0.994 | 0.992–0.996 | not worse |
| set.insert.reused (XOffset ns/op) | 1.006 | 0.996 | 0.994–0.997 | not worse |
| str.append (XOffset ns/op) | 1.022 | 0.998 | 0.994–1.003 | not worse |
| str.assign_long (XOffset ns/op) | 1.080 | 1.000 | 0.998–1.002 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.008 | 0.997 | 0.995–0.999 | not worse |
| map.insert.fresh / std::map (report) | 1.018 | 1.012 | 1.008–1.016 | worse? |
| map.insert.reused / std::flat_map (report) | 1.008 | 1.003 | 1.000–1.005 | still worse |
| map.insert.reused / std::map (report) | 1.028 | 1.001 | 0.995–1.007 | not worse |
| set.find.fresh / std::set (report) | 1.006 | 0.999 | 0.994–1.004 | not worse |
| str.append / std::string (G9) | 1.006 | 1.030 | 1.025–1.036 | still worse |
| str.assign_long / std::string (G9) | 1.081 | 1.001 | 0.999–1.003 | not worse |
| str.assign_short.view / std::string (G9) | 1.003 | 1.000 | 0.998–1.003 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.82 | 162.68 | 0.993 | 224.54 | 1.001 |
| hero.mid_erase.plain | 912.70 | 913.22 | 1.001 | 225.62 | 1.000 |
| hero.mid_insert.ord | 227.05 | 227.70 | 1.003 | 298.15 | 0.997 |
| hero.mid_insert.plain | 1246.01 | 1254.75 | 1.007 | 299.28 | 1.001 |
| hero.push_back_grow.ord | 31.73 | 31.77 | 1.001 | 86.64 | 1.001 |
| hero.push_back_grow.plain | 35.27 | 35.22 | 0.999 | 86.20 | 1.003 |
| map.erase.fresh | 32.96 | 32.94 | 0.999 | 35.17 | 0.999 |
| map.erase.reused | 29.54 | 29.51 | 0.999 | 27.31 | 0.997 |
| map.find.fresh | 5.28 | 5.28 | 0.999 | 6.54 | 0.999 |
| map.find.reused | 5.29 | 5.29 | 1.001 | 6.55 | 1.002 |
| map.insert.fresh | 31.90 | 32.02 | 1.004 | 36.94 | 0.998 |
| map.insert.reused | 30.20 | 30.37 | 1.006 | 29.39 | 0.998 |
| nested.build.ord | 269.78 | 270.75 | 1.004 | 826.76 | 0.993 |
| nested.build.plain | 279.04 | 278.15 | 0.997 | 823.99 | 0.990 |
| reset | 28.10 | 28.46 | 1.013 | 134.57 | 0.935 |
| set.erase.fresh | 29.77 | 29.79 | 1.001 | 27.43 | 0.999 |
| set.erase.reused | 22.87 | 22.86 | 1.000 | 20.37 | 1.003 |
| set.find.fresh | 4.87 | 4.87 | 1.000 | 6.47 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.80 | 28.90 | 1.004 | 28.92 | 0.995 |
| set.insert.reused | 23.38 | 23.54 | 1.007 | 21.10 | 0.997 |
| str.append | 1.52 | 1.55 | 1.022 | 3.50 | 1.006 |
| str.assign_long | 2.71 | 2.93 | 1.081 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 0.999 | 0.52 | 1.000 |
| str.assign_short.copy | 0.61 | 0.60 | 0.983 | 0.53 | 0.974 |
| str.assign_short.view | 0.93 | 0.93 | 1.001 | 2.71 | 0.998 |
| vec.iterate | 0.03 | 0.03 | 1.000 | 0.03 | 1.004 |
| vec.mid_erase | 40.35 | 40.14 | 0.995 | 39.52 | 1.001 |
| vec.mid_insert | 40.49 | 40.50 | 1.000 | 42.16 | 1.000 |
| vec.push_back | 0.30 | 0.29 | 0.987 | 0.26 | 0.999 |
| vec.push_back_grow | 0.27 | 0.27 | 1.002 | 0.36 | 0.995 |

## analyze t0 (G9 at the anchor 51a4815b)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.140 |  1.143 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.959 |  0.961 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.55 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.728 |  0.730 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     294.27 |  0.758 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.74 |      86.30 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.43 |     224.61 |  4.055 |  4.068 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1246.41 |     304.69 |  4.192 |  4.208 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      85.98 |  0.410 |  0.411 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.47 |  0.433 |  0.435 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.861 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.89 |  0.737 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.54 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.31 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.995 |  0.997 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.34 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.68 |  0.662 |  0.665 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.19 |      29.38 |  1.027 |  1.029 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.19 |      16.28 |  1.847 |  1.852 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.809 |  0.812 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       6.46 |  0.825 |  0.829 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.82 |  1.629 |  1.639 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.75 |  1.470 |  1.481 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.19 |  0.664 |  0.668 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.09 |  1.315 |  1.324 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     825.50 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     119.14 |  0.209 |  0.212 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     824.86 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.115 |  1.119 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.757 |  0.760 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      42.24 |  0.960 |  0.962 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.47 |  1.019 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    162.77 |     224.61 |  0.721 |  0.723 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     298.19 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.79 |  0.356 |  0.359 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     225.25 |  4.057 |  4.071 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.20 |     304.69 |  4.208 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.57 |  0.402 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.58 |  0.436 |  0.438 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.98 |      37.11 |  0.862 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.98 |      42.48 |  0.750 |  0.752 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.39 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.83 |  0.944 |  0.946 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.41 |  0.730 |  0.733 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.655 |  0.657 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.088 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      45.33 |  0.661 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.035 |  1.037 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      15.95 |  1.900 |  1.907 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.34 |  0.722 |  0.725 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.079 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      18.55 |  1.611 |  1.622 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.113 |  1.117 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.71 |  1.478 |  1.491 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.47 |  0.654 |  0.657 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.133 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.82 |  1.307 |  1.317 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     833.31 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     119.80 |  0.204 |  0.208 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     834.62 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-05T07:00:14Z block 1 start: waited 180s load 1.48 WindowServer 1.2% remote 5
2026-11-05T07:03:19Z block 2 start: waited 180s load 1.19 WindowServer 1.3% remote 5
2026-11-05T07:06:23Z block 3 start: waited 180s load 1.30 WindowServer 1.1% remote 5
2026-11-05T07:09:28Z block 4 start: waited 180s load 1.55 WindowServer 1.2% remote 5
```
