# G9R3 result (2026-11-04T12:23:25Z)

t0 1c528d67739bc94d91205c9dd2bf4bf1260253bf (next_cpp26 1c528d67 (#213; mainline merged into PR #191)); t1 d438627ad0a39523e9c7190a57b8376cb1d97d28 (CKPT16E head (PR #191 delta sync d438627a)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 1c528d67739bc94d91205c9dd2bf4bf1260253bf tree b57398995a974c757b82e4e84efa123bd609ef26 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 1c528d67 (#213; mainline merged into PR #191))
t1 d438627ad0a39523e9c7190a57b8376cb1d97d28 tree d4f4865f4f4c657f8dd768c95d25fcf983175471 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT16E head (PR #191 delta sync d438627a))
t1: diff over t0 sha256 bef080f029c4152f57b3b0b261cba31e9b68c3cc8b1cc119a7371c75fc5f147c ( 11 files changed, 5904 insertions(+), 19 deletions(-))
t1: library header diff over t0:  2 files changed, 249 insertions(+), 17 deletions(-)
t1: first-parent commits over t0:
  d438627a Merge next_cpp26 1c528d67 (#213) into delta sync
  298d718f Merge next_cpp26 b817ed36 (#217) into delta sync
  09aade20 Merge next_cpp26 2e105330 (#215, #216, #218) into delta sync
  d5401486 Free table: loading holes always marks the generation stale
  58779139 Merge next_cpp26 (49ce3c68, #211 #212) into delta sync
  a8f12389 XBuffer: a move or swap clears the cached root again
  6ec08cc6 Merge next_cpp26 (eabb583b, #210) into delta sync
  374a8728 Merge next_cpp26 (32ee1473, #207 #208) into delta sync
  153d5177 Merge next_cpp26 (9e14ece1, #209) into delta sync
  7157488f Merge next_cpp26 (c2ec7ee3, #205 #206) into delta sync
  845c230b XBufferCore: keep root cache, base, size and free table on one run
  827943cf Free table: keep pre-generation field offsets, drop redundant bump_gen
  0bbbaa07 Merge next_cpp26 (b6e0678, #204) into delta sync
  3e026071 Merge next_cpp26 (cc5b28e) into delta sync
  4889cdd4 Free table: a change marks the generation stale, a read counts it up
  3278287d Merge next_cpp26 (d9db04d) into delta sync
  20ae12ea Free table: inline the generation bump
  a95bed62 Delta: a touches baseline sends touched bytes past the touch list
  8045e6b4 Merge next_cpp26 910b0223 (#199, #200) into delta sync
  4d117cf7 Delta receiver: re-verify only a vector of records that moved or shrank
  a126a042 CHANGELOG: receiver map growth, touch list filter
  9bd146c8 Merge next_cpp26 (7e24016a: #198) into the delta sync branch
  67494349 Delta receiver maps grow by half again; touch list skips the scan for new blocks
  ce1bed30 Merge next_cpp26 (c5a801a9: #193) into the delta sync branch
  3e203f69 Cached root follows an XBuffer move and swap
  e2697b51 Delta baseline copy: aligned buffer with room to grow, filled and moved with memset/memcpy
  44e91402 Merge next_cpp26 (87227259: #194) into the delta sync branch
  51c2f07b Delta: records growth checked on its own, byte-exact touch_set, hole table skipped when unchanged, cached root follows a storage move
  8b3029b6 Delta receiver: moved containers checked on their own against a live-block map
  522f2129 Merge next_cpp26 (670fcc75: #197) into the delta sync branch
  65e98ce7 Delta touch log: a short list per period, stamps only when shared; touches baselines
  ece7d87e Delta blocks on 64-byte payload offsets; baseline copy line-aligned
  dffba8ad Merge next_cpp26 (f04d6230: #192, #195, #196) into the delta sync branch
  98046a99 Merge next_cpp26 (c9fbaea2, bench/slg demo case set) into the delta sync branch
  bc21be6d Delta sync: the baselines of a thread share the encode scratch
  9640d118 Delta sync: test replicas fed on several threads and at thread exit
  5446db55 Delta sync: the cursors of a thread share the apply scratch
  5bb90b94 Delta sync: compare touched blocks by runs; never skip blocks past the baseline's used size
  f39fe1fa Delta sync: key deltas pair with the touch log, node lookup by block, 24-byte map
  fe1ebbd4 Delta sync: write a block's mask bytes in one shuffle; CHANGELOG for format 1.0
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 bda36a5d999b89dce10307362d737ffe0ad36216ea0b0e06898a549563196d65
reloc_bench t1 sha256 5d3a5f4b2d326c7cbaa036b8c53d31a776b1a4fd674c8ecd20794add4813f531
reloc_bench t0a sha256 c896031c33e1abe6059c1fbb5b546e01a1cc160794aeef9122cc4d3978e1a4b4
reloc_bench t1a sha256 6fa9ff1605a6e8edffb26f903f8afb0b6da9714d55e303986704d466811888f6
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.726 | 0.726 | 1.000 | 0.997–1.002 | 0.997, 1.003 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.752 | 0.759 | 1.009 | 1.006–1.012 | 1.009, 1.009 | worse |
| hero.push_back_grow.ord | std::vector | G9 | 0.358 | 0.363 | 1.012 | 1.009–1.015 | 0.990, 1.035 | worse? |
| map.erase.fresh | std::flat_map | G9 | 0.939 | 0.937 | 0.999 | 0.997–1.001 | 0.997, 1.000 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.869 | 0.873 | 1.005 | 1.003–1.007 | 1.006, 1.004 | worse |
| nested.build.ord | std | G9 | 0.322 | 0.326 | 1.014 | 1.010–1.018 | 0.991, 1.038 | worse? |
| reset | std | G9 | 0.234 | 0.232 | 0.987 | 0.976–0.998 | 1.015, 0.961 |  |
| set.erase.fresh | std::flat_set | G9 | 1.081 | 1.087 | 1.005 | 1.002–1.008 | 1.004, 1.007 | worse |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.755 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 1.002 | 0.998 | 0.996 | 0.994–0.999 | 0.996, 0.996 |  |
| str.append | std::string | G9 | 0.422 | 0.423 | 1.001 | 0.996–1.006 | 0.984, 1.018 |  |
| str.assign_long | std::string | G9 | 0.872 | 0.801 | 0.919 | 0.905–0.934 | 1.002, 0.843 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.339 | 0.988 | 0.983–0.992 | 0.976, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.002 | 0.980 | 0.979 | 0.976–0.982 | 0.979, 0.979 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.024 | 1.005 | 1.003–1.006 | 1.005, 1.004 | worse |
| vec.mid_insert | std::vector | G9 | 0.957 | 0.969 | 1.013 | 1.011–1.015 | 1.004, 1.022 | worse |
| vec.push_back | std::vector | G9 | 1.101 | 1.141 | 1.037 | 1.033–1.040 | 1.035, 1.039 | worse |
| vec.push_back_grow | std::vector | G9 | 0.759 | 0.772 | 1.017 | 1.014–1.021 | 1.022, 1.013 | worse |
| hero.mid_erase.plain | std::vector | report | 4.039 | 3.992 | 0.988 | 0.986–0.991 | 0.988, 0.989 |  |
| hero.mid_insert.plain | std::vector | report | 4.167 | 4.214 | 1.011 | 1.008–1.015 | 1.004, 1.018 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.398 | 0.405 | 1.017 | 1.014–1.021 | 0.995, 1.040 | worse? |
| map.erase.fresh | std::map | report | 0.743 | 0.742 | 0.998 | 0.996–1.001 | 0.995, 1.001 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.081 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| map.erase.reused | std::map | report | 1.650 | 1.637 | 0.992 | 0.987–0.998 | 0.986, 0.999 |  |
| map.find.fresh | std::map | report | 0.325 | 0.323 | 0.993 | 0.989–0.997 | 0.991, 0.995 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.807 | 0.998 | 0.997–1.000 | 0.997, 1.000 |  |
| map.find.reused | std::map | report | 0.692 | 0.715 | 1.034 | 1.029–1.039 | 1.047, 1.021 | worse |
| map.insert.fresh | std::map | report | 0.746 | 0.748 | 1.002 | 1.000–1.005 | 0.982, 1.023 |  |
| map.insert.reused | std::flat_map | report | 1.000 | 1.005 | 1.005 | 1.003–1.007 | 1.003, 1.007 | worse |
| map.insert.reused | std::map | report | 1.788 | 1.757 | 0.982 | 0.976–0.989 | 0.965, 1.000 |  |
| nested.build.plain | std | report | 0.329 | 0.339 | 1.031 | 1.026–1.036 | 1.004, 1.059 | worse |
| set.erase.fresh | std::set | report | 0.668 | 0.670 | 1.003 | 1.000–1.006 | 1.000, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.136 | 1.140 | 1.004 | 1.001–1.006 | 1.006, 1.002 | worse |
| set.erase.reused | std::set | report | 1.351 | 1.347 | 0.997 | 0.990–1.005 | 0.996, 0.998 |  |
| set.find.fresh | std::set | report | 0.297 | 0.300 | 1.010 | 1.006–1.015 | 1.013, 1.008 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| set.find.reused | std::set | report | 0.762 | 0.659 | 0.864 | 0.861–0.867 | 0.875, 0.853 |  |
| set.insert.fresh | std::set | report | 0.652 | 0.653 | 1.001 | 0.997–1.004 | 0.979, 1.023 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.082 | 1.002 | 0.999–1.005 | 1.004, 0.999 |  |
| set.insert.reused | std::set | report | 1.400 | 1.394 | 0.996 | 0.987–1.004 | 0.981, 1.011 |  |
| str.assign_short | std::string | report | 1.765 | 1.770 | 1.003 | 1.001–1.005 | 1.004, 1.002 | worse |
| str.assign_short.copy | std::string | report | 1.152 | 1.149 | 0.998 | 0.996–0.999 | 0.996, 0.999 |  |

worse at t1 than t0 (by the rule): hero.mid_insert.ord / std::vector (G9) 1.009; map.insert.fresh / std::flat_map (G9) 1.005; set.erase.fresh / std::flat_set (G9) 1.005; vec.mid_erase / std::vector (G9) 1.005; vec.mid_insert / std::vector (G9) 1.013; vec.push_back / std::vector (G9) 1.037; vec.push_back_grow / std::vector (G9) 1.017; hero.mid_insert.plain / std::vector (report) 1.011; map.find.reused / std::map (report) 1.034; map.insert.reused / std::flat_map (report) 1.005; nested.build.plain / std (report) 1.031; set.erase.reused / std::flat_set (report) 1.004; set.find.fresh / std::set (report) 1.010; str.assign_short / std::string (report) 1.003

above 1.01 but not worse by the rule: hero.push_back_grow.ord / std::vector (G9) 1.012; nested.build.ord / std (G9) 1.014; hero.push_back_grow.plain / std::vector (report) 1.017

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.2 | 163.3 | 1.001 | 0.999–1.002 | 1.003, 0.998 |  |
| hero.mid_erase.plain | 906.7 | 897.1 | 0.989 | 0.987–0.992 | 0.992, 0.987 |  |
| hero.mid_insert.ord | 227.3 | 228.5 | 1.006 | 1.004–1.007 | 1.009, 1.002 | worse |
| hero.mid_insert.plain | 1250 | 1263 | 1.011 | 1.009–1.013 | 1.009, 1.012 | worse |
| hero.push_back_grow.ord | 31.9 | 32.01 | 1.003 | 1.002–1.005 | 1.001, 1.006 | worse |
| hero.push_back_grow.plain | 35.44 | 35.72 | 1.008 | 1.007–1.010 | 1.006, 1.010 | worse |
| map.erase.fresh | 33.04 | 32.99 | 0.999 | 0.997–1.000 | 0.998, 1.000 |  |
| map.erase.reused | 29.59 | 29.63 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| map.find.fresh | 5.282 | 5.287 | 1.001 | 1.000–1.002 | 1.001, 1.001 |  |
| map.find.reused | 5.309 | 5.3 | 0.998 | 0.997–1.000 | 0.997, 1.000 |  |
| map.insert.fresh | 32.21 | 32.19 | 0.999 | 0.998–1.001 | 1.000, 0.999 |  |
| map.insert.reused | 29.52 | 29.52 | 1.000 | 0.999–1.002 | 0.997, 1.003 |  |
| nested.build.ord | 271.4 | 273.5 | 1.008 | 1.006–1.009 | 1.009, 1.006 | worse |
| nested.build.plain | 277.8 | 283.1 | 1.019 | 1.017–1.021 | 1.022, 1.016 | worse |
| reset | 29.03 | 28.48 | 0.981 | 0.976–0.986 | 0.982, 0.981 |  |
| set.erase.fresh | 29.77 | 29.78 | 1.000 | 1.000–1.001 | 1.000, 1.000 |  |
| set.erase.reused | 23.2 | 23.25 | 1.002 | 1.000–1.005 | 1.005, 1.000 |  |
| set.find.fresh | 4.885 | 4.881 | 0.999 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.883 | 4.877 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| set.insert.fresh | 28.93 | 28.88 | 0.998 | 0.996–1.000 | 0.998, 0.998 |  |
| set.insert.reused | 22.86 | 22.81 | 0.998 | 0.995–1.001 | 0.999, 0.997 |  |
| str.append | 1.544 | 1.56 | 1.010 | 1.006–1.014 | 1.005, 1.015 | worse |
| str.assign_long | 2.957 | 2.725 | 0.921 | 0.907–0.936 | 1.005, 0.845 |  |
| str.assign_short | 0.921 | 0.9392 | 1.020 | 1.017–1.023 | 1.021, 1.018 | worse |
| str.assign_short.copy | 0.6173 | 0.6245 | 1.012 | 1.007–1.016 | 1.008, 1.015 | worse |
| str.assign_short.view | 0.9331 | 0.9475 | 1.015 | 1.010–1.020 | 1.012, 1.019 | worse |
| vec.iterate | 0.03318 | 0.03258 | 0.982 | 0.981–0.983 | 0.984, 0.980 |  |
| vec.mid_erase | 40.3 | 40.42 | 1.003 | 1.002–1.004 | 1.005, 1.001 | worse |
| vec.mid_insert | 40.42 | 40.63 | 1.005 | 1.004–1.007 | 1.006, 1.004 | worse |
| vec.push_back | 0.2915 | 0.2874 | 0.986 | 0.975–0.996 | 0.951, 1.022 |  |
| vec.push_back_grow | 0.2767 | 0.2745 | 0.992 | 0.987–0.997 | 0.988, 0.997 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_insert.ord 1.006; hero.mid_insert.plain 1.011; hero.push_back_grow.ord 1.003; hero.push_back_grow.plain 1.008; nested.build.ord 1.008; nested.build.plain 1.019; str.append 1.010; str.assign_short 1.020; str.assign_short.copy 1.012; str.assign_short.view 1.015; vec.mid_erase 1.003; vec.mid_insert 1.005

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.728 | 1.002 | 0.998–1.006 | 1.015, 0.989 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.761 | 1.003 | 0.999–1.006 | 1.001, 1.004 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.362 | 0.996 | 0.994–0.998 | 0.980, 1.012 |  |
| map.erase.fresh | std::flat_map | G9 | 0.943 | 0.945 | 1.002 | 1.000–1.004 | 1.005, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.809 | 1.001 | 0.998–1.003 | 1.001, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.869 | 0.875 | 1.007 | 1.005–1.009 | 1.013, 1.001 | worse |
| nested.build.ord | std | G9 | 0.315 | 0.321 | 1.022 | 1.017–1.026 | 1.030, 1.013 | worse |
| reset | std | G9 | 0.213 | 0.241 | 1.131 | 1.121–1.142 | 1.147, 1.116 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.089 | 1.095 | 1.005 | 1.003–1.007 | 1.009, 1.001 | worse |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.002 | 1.000–1.004 | 1.004, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 1.008 | 1.009 | 1.007–1.011 | 1.017, 1.002 | worse |
| str.append | std::string | G9 | 0.427 | 0.433 | 1.013 | 1.008–1.018 | 1.016, 1.010 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.865 | 0.998 | 0.996–1.001 | 1.000, 0.997 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.024 | 1.007 | 1.005–1.009 | 1.005, 1.009 | worse |
| vec.mid_insert | std::vector | G9 | 0.976 | 0.958 | 0.981 | 0.980–0.983 | 0.979, 0.984 |  |
| vec.push_back | std::vector | G9 | 1.109 | 1.120 | 1.010 | 1.006–1.014 | 1.017, 1.003 | worse |
| vec.push_back_grow | std::vector | G9 | 0.764 | 0.757 | 0.991 | 0.987–0.994 | 0.990, 0.991 |  |
| hero.mid_erase.plain | std::vector | report | 4.043 | 4.052 | 1.002 | 0.999–1.005 | 1.003, 1.001 |  |
| hero.mid_insert.plain | std::vector | report | 4.186 | 4.128 | 0.986 | 0.982–0.990 | 0.974, 0.998 |  |
| hero.push_back_grow.plain | std::vector | report | 0.404 | 0.404 | 0.998 | 0.996–1.001 | 0.983, 1.014 |  |
| map.erase.fresh | std::map | report | 0.737 | 0.746 | 1.013 | 1.010–1.016 | 1.020, 1.005 | worse |
| map.erase.reused | std::flat_map | report | 1.083 | 1.086 | 1.002 | 1.000–1.004 | 1.008, 0.996 |  |
| map.erase.reused | std::map | report | 1.577 | 1.627 | 1.032 | 1.024–1.040 | 1.054, 1.010 | worse |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.003 | 0.998–1.007 | 1.003, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.808 | 0.999 | 0.997–1.001 | 1.001, 0.997 |  |
| map.find.reused | std::map | report | 0.715 | 0.737 | 1.031 | 1.028–1.035 | 1.019, 1.044 | worse |
| map.insert.fresh | std::map | report | 0.749 | 0.728 | 0.971 | 0.968–0.975 | 0.963, 0.980 |  |
| map.insert.reused | std::flat_map | report | 1.006 | 1.008 | 1.002 | 0.999–1.005 | 1.011, 0.993 |  |
| map.insert.reused | std::map | report | 1.813 | 1.845 | 1.017 | 1.010–1.024 | 1.006, 1.029 | worse |
| nested.build.plain | std | report | 0.325 | 0.334 | 1.027 | 1.023–1.032 | 1.034, 1.021 | worse |
| set.erase.fresh | std::set | report | 0.672 | 0.675 | 1.005 | 1.002–1.008 | 1.006, 1.003 | worse |
| set.erase.reused | std::flat_set | report | 1.143 | 1.139 | 0.996 | 0.994–0.998 | 1.004, 0.988 |  |
| set.erase.reused | std::set | report | 1.299 | 1.335 | 1.028 | 1.019–1.037 | 1.043, 1.013 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.001 | 0.996–1.005 | 1.002, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| set.find.reused | std::set | report | 0.672 | 0.676 | 1.006 | 1.003–1.008 | 1.004, 1.007 | worse |
| set.insert.fresh | std::set | report | 0.647 | 0.653 | 1.010 | 1.006–1.015 | 0.982, 1.039 | worse? |
| set.insert.reused | std::flat_set | report | 1.084 | 1.089 | 1.004 | 1.002–1.006 | 1.012, 0.996 |  |
| set.insert.reused | std::set | report | 1.401 | 1.456 | 1.039 | 1.030–1.049 | 1.025, 1.054 | worse |
| str.assign_short | std::string | report | 1.766 | 1.766 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.150 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |

worse at t1a than t0a (by the rule): map.insert.fresh / std::flat_map (G9) 1.007; nested.build.ord / std (G9) 1.022; reset / std (G9) 1.131; set.erase.fresh / std::flat_set (G9) 1.005; set.insert.fresh / std::flat_set (G9) 1.009; str.append / std::string (G9) 1.013; vec.mid_erase / std::vector (G9) 1.007; vec.push_back / std::vector (G9) 1.010; map.erase.fresh / std::map (report) 1.013; map.erase.reused / std::map (report) 1.032; map.find.reused / std::map (report) 1.031; map.insert.reused / std::map (report) 1.017; nested.build.plain / std (report) 1.027; set.erase.fresh / std::set (report) 1.005; set.erase.reused / std::set (report) 1.028; set.find.reused / std::set (report) 1.006; set.insert.reused / std::set (report) 1.039

above 1.01 but not worse by the rule: set.insert.fresh / std::set (report) 1.010

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.9 | 164.9 | 1.006 | 1.003–1.008 | 1.007, 1.004 | worse |
| hero.mid_erase.plain | 910.9 | 907.5 | 0.996 | 0.994–0.999 | 0.992, 1.000 |  |
| hero.mid_insert.ord | 228.7 | 230 | 1.006 | 1.004–1.008 | 1.010, 1.002 | worse |
| hero.mid_insert.plain | 1256 | 1237 | 0.985 | 0.983–0.987 | 0.976, 0.994 |  |
| hero.push_back_grow.ord | 31.65 | 31.66 | 1.000 | 0.999–1.002 | 0.999, 1.002 |  |
| hero.push_back_grow.plain | 35.15 | 35.19 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| map.erase.fresh | 33.05 | 33.07 | 1.001 | 0.999–1.003 | 1.005, 0.997 |  |
| map.erase.reused | 29.69 | 29.83 | 1.005 | 1.003–1.006 | 1.010, 1.000 |  |
| map.find.fresh | 5.29 | 5.293 | 1.000 | 0.998–1.003 | 1.001, 1.000 |  |
| map.find.reused | 5.303 | 5.303 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| map.insert.fresh | 32.32 | 32.43 | 1.003 | 1.002–1.005 | 1.010, 0.997 |  |
| map.insert.reused | 29.6 | 29.78 | 1.006 | 1.003–1.009 | 1.016, 0.996 |  |
| nested.build.ord | 266.6 | 267.2 | 1.002 | 1.000–1.005 | 0.999, 1.006 |  |
| nested.build.plain | 275.9 | 276.9 | 1.004 | 1.002–1.006 | 0.998, 1.009 |  |
| reset | 28.18 | 28.44 | 1.009 | 1.005–1.013 | 0.992, 1.027 |  |
| set.erase.fresh | 29.83 | 29.81 | 0.999 | 0.997–1.001 | 1.004, 0.995 |  |
| set.erase.reused | 23.29 | 23.28 | 0.999 | 0.997–1.002 | 1.007, 0.991 |  |
| set.find.fresh | 4.882 | 4.883 | 1.000 | 0.999–1.002 | 1.000, 1.000 |  |
| set.find.reused | 4.882 | 4.877 | 0.999 | 0.998–1.000 | 0.999, 0.999 |  |
| set.insert.fresh | 28.95 | 29.09 | 1.005 | 1.003–1.007 | 1.010, 0.999 |  |
| set.insert.reused | 22.86 | 22.92 | 1.003 | 1.001–1.004 | 1.011, 0.994 |  |
| str.append | 1.551 | 1.567 | 1.010 | 1.005–1.015 | 1.007, 1.013 | worse |
| str.assign_long | 2.952 | 2.937 | 0.995 | 0.993–0.997 | 0.994, 0.996 |  |
| str.assign_short | 0.941 | 0.9333 | 0.992 | 0.988–0.995 | 0.985, 0.998 |  |
| str.assign_short.copy | 0.6284 | 0.6228 | 0.991 | 0.986–0.996 | 0.982, 1.000 |  |
| str.assign_short.view | 0.9456 | 0.9363 | 0.990 | 0.987–0.993 | 0.985, 0.995 |  |
| vec.iterate | 0.03325 | 0.03326 | 1.000 | 0.999–1.001 | 1.000, 1.001 |  |
| vec.mid_erase | 40.53 | 40.62 | 1.002 | 1.001–1.004 | 1.002, 1.003 | worse |
| vec.mid_insert | 40.7 | 40.5 | 0.995 | 0.994–0.996 | 0.994, 0.996 |  |
| vec.push_back | 0.2718 | 0.2719 | 1.000 | 1.000–1.001 | 1.000, 1.001 |  |
| vec.push_back_grow | 0.2705 | 0.2713 | 1.003 | 1.001–1.005 | 0.998, 1.008 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.006; hero.mid_insert.ord 1.006; str.append 1.010; vec.mid_erase 1.002

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_insert.ord (XOffset ns/op) | 1.006 | 1.006 | 1.004–1.008 | still worse |
| hero.mid_insert.plain (XOffset ns/op) | 1.011 | 0.985 | 0.983–0.987 | not worse |
| hero.push_back_grow.ord (XOffset ns/op) | 1.003 | 1.000 | 0.999–1.002 | not worse |
| hero.push_back_grow.plain (XOffset ns/op) | 1.008 | 1.001 | 0.999–1.003 | not worse |
| nested.build.ord (XOffset ns/op) | 1.008 | 1.002 | 1.000–1.005 | not worse |
| nested.build.plain (XOffset ns/op) | 1.019 | 1.004 | 1.002–1.006 | not worse |
| str.append (XOffset ns/op) | 1.010 | 1.010 | 1.005–1.015 | still worse |
| str.assign_short (XOffset ns/op) | 1.020 | 0.992 | 0.988–0.995 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.012 | 0.991 | 0.986–0.996 | not worse |
| str.assign_short.view (XOffset ns/op) | 1.015 | 0.990 | 0.987–0.993 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.003 | 1.002 | 1.001–1.004 | still worse |
| vec.mid_insert (XOffset ns/op) | 1.005 | 0.995 | 0.994–0.996 | not worse |
| hero.mid_insert.ord / std::vector (G9) | 1.009 | 1.003 | 0.999–1.006 | not worse |
| hero.mid_insert.plain / std::vector (report) | 1.011 | 0.986 | 0.982–0.990 | not worse |
| map.find.reused / std::map (report) | 1.034 | 1.031 | 1.028–1.035 | still worse |
| map.insert.fresh / std::flat_map (G9) | 1.005 | 1.007 | 1.005–1.009 | still worse |
| map.insert.reused / std::flat_map (report) | 1.005 | 1.002 | 0.999–1.005 | not worse |
| nested.build.plain / std (report) | 1.031 | 1.027 | 1.023–1.032 | still worse |
| set.erase.fresh / std::flat_set (G9) | 1.005 | 1.005 | 1.003–1.007 | still worse |
| set.erase.reused / std::flat_set (report) | 1.004 | 0.996 | 0.994–0.998 | not worse |
| set.find.fresh / std::set (report) | 1.010 | 1.001 | 0.996–1.005 | not worse |
| str.assign_short / std::string (report) | 1.003 | 1.000 | 0.998–1.002 | not worse |
| vec.mid_erase / std::vector (G9) | 1.005 | 1.007 | 1.005–1.009 | still worse |
| vec.mid_insert / std::vector (G9) | 1.013 | 0.981 | 0.980–0.983 | not worse |
| vec.push_back / std::vector (G9) | 1.037 | 1.010 | 1.006–1.014 | still worse |
| vec.push_back_grow / std::vector (G9) | 1.017 | 0.991 | 0.987–0.994 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.01 | 163.00 | 1.000 | 223.94 | 1.007 |
| hero.mid_erase.plain | 905.95 | 895.04 | 0.988 | 224.81 | 0.998 |
| hero.mid_insert.ord | 226.89 | 228.31 | 1.006 | 301.57 | 1.005 |
| hero.mid_insert.plain | 1249.86 | 1259.18 | 1.007 | 301.69 | 0.987 |
| hero.push_back_grow.ord | 31.83 | 31.94 | 1.003 | 88.89 | 1.001 |
| hero.push_back_grow.plain | 35.36 | 35.69 | 1.009 | 88.83 | 1.001 |
| map.erase.fresh | 33.00 | 32.96 | 0.999 | 35.10 | 1.001 |
| map.erase.reused | 29.50 | 29.55 | 1.002 | 27.26 | 1.006 |
| map.find.fresh | 5.28 | 5.28 | 1.000 | 6.55 | 1.000 |
| map.find.reused | 5.30 | 5.29 | 0.999 | 6.56 | 1.002 |
| map.insert.fresh | 32.19 | 32.18 | 1.000 | 37.02 | 1.003 |
| map.insert.reused | 29.46 | 29.48 | 1.001 | 29.45 | 1.006 |
| nested.build.ord | 271.40 | 273.02 | 1.006 | 842.44 | 1.000 |
| nested.build.plain | 277.58 | 282.77 | 1.019 | 843.60 | 1.004 |
| reset | 28.95 | 28.41 | 0.981 | 121.61 | 1.004 |
| set.erase.fresh | 29.78 | 29.77 | 1.000 | 27.47 | 1.000 |
| set.erase.reused | 23.00 | 23.07 | 1.003 | 20.35 | 1.001 |
| set.find.fresh | 4.88 | 4.87 | 0.999 | 6.47 | 1.001 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 28.85 | 28.88 | 1.001 | 28.85 | 1.004 |
| set.insert.reused | 22.81 | 22.81 | 1.000 | 21.12 | 1.002 |
| str.append | 1.51 | 1.55 | 1.021 | 3.61 | 1.001 |
| str.assign_long | 2.95 | 2.71 | 0.918 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.001 | 0.52 | 0.996 |
| str.assign_short.copy | 0.60 | 0.62 | 1.028 | 0.52 | 0.976 |
| str.assign_short.view | 0.93 | 0.93 | 0.999 | 2.71 | 1.002 |
| vec.iterate | 0.03 | 0.03 | 0.983 | 0.03 | 0.998 |
| vec.mid_erase | 40.21 | 40.22 | 1.000 | 39.54 | 1.003 |
| vec.mid_insert | 40.39 | 40.57 | 1.004 | 42.20 | 0.995 |
| vec.push_back | 0.29 | 0.29 | 0.988 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.27 | 0.995 | 0.36 | 1.004 |

## analyze t0 (G9 at the anchor 1c528d67)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.101 |  1.105 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.759 |  0.761 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.957 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.63 |  1.019 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.61 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     304.69 |  0.752 |  0.755 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      87.85 |  0.358 |  0.360 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.95 |     224.61 |  4.039 |  4.049 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1249.34 |     304.03 |  4.167 |  4.181 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      87.46 |  0.398 |  0.400 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.872 |  0.891 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.152 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.61 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.89 |  0.746 |  0.749 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.939 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.743 |  0.745 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  1.002 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.95 |  0.652 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.51 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.081 |  1.085 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.27 |  0.668 |  0.670 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.46 |  1.000 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.36 |  1.788 |  1.798 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.66 |  0.692 |  0.695 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.080 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.58 |  1.650 |  1.659 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.16 |  1.081 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      16.03 |  1.400 |  1.412 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.36 |  0.762 |  0.765 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.136 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.85 |  1.351 |  1.361 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     829.41 |  0.322 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.234 |  0.237 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     832.03 |  0.329 |  0.331 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.141 |  1.144 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.772 |  0.774 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.980 |  0.985 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.32 |  0.969 |  0.971 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.024 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.97 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     297.53 |  0.759 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      87.97 |  0.363 |  0.363 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    894.53 |     224.61 |  3.992 |  4.003 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.214 |  4.231 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.68 |      87.97 |  0.405 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.774 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.339 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.149 |  1.151 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.64 |  0.423 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.19 |      36.78 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.19 |      42.81 |  0.748 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.41 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.742 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.11 |  0.653 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.670 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.007 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.36 |  1.757 |  1.770 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.43 |  0.715 |  0.720 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.637 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.082 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.87 |  1.394 |  1.406 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.39 |  0.659 |  0.661 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.35 |  1.140 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.347 |  1.358 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     839.84 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     117.84 |  0.232 |  0.234 | pass    |
| nested.build.plain           | std            | report |  808 |    282.56 |     834.66 |  0.339 |  0.340 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T12:14:07Z block 1 start: waited 180s load 1.93 WindowServer 3.6% remote 6
2026-11-04T12:17:12Z block 2 start: waited 180s load 1.97 WindowServer 5.5% remote 6
2026-11-04T12:20:16Z block 3 start: waited 180s load 2.29 WindowServer 4.3% remote 6
2026-11-04T12:23:21Z block 4 start: waited 180s load 1.87 WindowServer 4.3% remote 6
```
