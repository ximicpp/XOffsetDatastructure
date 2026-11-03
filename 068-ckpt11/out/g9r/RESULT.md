# G9R3 result (2026-11-03T19:23:17Z)

t0 b6e06788102838dc1364a22aba146cc23d70a030 (next_cpp26 b6e06788 (#204; PR #191 base)); t1 827943cfb9725aeb54b2ff93ac99ff1eaf9351e2 (CKPT11 head (PR #191 delta sync 827943cf)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 b6e06788102838dc1364a22aba146cc23d70a030 tree cc6991a670c7aa918c13f95c7e21c4834e73e6ed typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 b6e06788 (#204; PR #191 base))
t1 827943cfb9725aeb54b2ff93ac99ff1eaf9351e2 tree c01689129b306891a3ba1129b861b3a4087da659 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT11 head (PR #191 delta sync 827943cf))
t1: diff over t0 sha256 b5dfc65a96547e8f0ed6b783814f55a53fd43f5975d5c870917627384f5825ba ( 10 files changed, 5893 insertions(+), 4 deletions(-))
t1: library header diff over t0:  1 file changed, 237 insertions(+), 2 deletions(-)
t1: first-parent commits over t0:
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
  84b83a1e Delta sync: whole_blocks option, no popcount calls, lazy map after full
  fa93cf72 Delta sync: touched-only diffs, SIMD block bytes, cheaper key apply
  78ed179f Delta sync: re-check a changed container on its own
  6ff26fc8 Delta sync: 64-byte change blocks, a 14-byte header, receiver checks only what changed
  779eef11 Delta sync: faster sender diff; key delta for a receiver that joins late
  44ec96e1 Delta sync: send only changed payload bytes to a receiver that holds a copy
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 95f76f10589add1e52a8ed6b74eb25c2e1bd829caa70682fc8a7ec10997c55a6
reloc_bench t1 sha256 58de6ac7ad971ddb1154e7ade1fafb07587eeea1f2e732d23682d0d5403569c9
reloc_bench t0a sha256 1eaad515e940841a22469676e25841a514f01bd5fea45def974abaeba8442e9b
reloc_bench t1a sha256 d2c1c2da2d03e2fde802a70edf6101617428cc9f1a2e0eef382611b7828033ee
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.726 | 0.728 | 1.003 | 1.001–1.005 | 1.000, 1.005 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.756 | 0.998 | 0.995–1.002 | 0.995, 1.002 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.365 | 0.366 | 1.003 | 1.001–1.005 | 1.000, 1.006 |  |
| map.erase.fresh | std::flat_map | G9 | 0.936 | 0.941 | 1.005 | 1.003–1.008 | 1.010, 1.000 | worse |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.805 | 0.998 | 0.996–1.000 | 0.996, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.866 | 0.873 | 1.008 | 1.006–1.011 | 1.009, 1.007 | worse |
| nested.build.ord | std | G9 | 0.326 | 0.326 | 0.999 | 0.996–1.002 | 0.996, 1.002 |  |
| reset | std | G9 | 0.241 | 0.261 | 1.086 | 1.072–1.100 | 0.990, 1.192 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.085 | 1.090 | 1.004 | 1.001–1.006 | 1.005, 1.002 | worse |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.755 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 1.011 | 0.999 | 0.988 | 0.986–0.990 | 0.993, 0.984 |  |
| str.append | std::string | G9 | 0.350 | 0.426 | 1.217 | 1.212–1.223 | 1.212, 1.223 | worse |
| str.assign_long | std::string | G9 | 0.867 | 0.800 | 0.924 | 0.922–0.925 | 0.924, 0.923 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.341 | 0.994 | 0.992–0.997 | 0.988, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.999–1.000 | 1.000, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.016 | 1.014 | 0.998 | 0.996–1.000 | 0.997, 1.000 |  |
| vec.mid_insert | std::vector | G9 | 0.982 | 0.956 | 0.974 | 0.972–0.976 | 0.975, 0.973 |  |
| vec.push_back | std::vector | G9 | 1.115 | 1.131 | 1.014 | 1.011–1.017 | 1.020, 1.008 | worse |
| vec.push_back_grow | std::vector | G9 | 0.775 | 0.765 | 0.987 | 0.983–0.991 | 0.981, 0.992 |  |
| hero.mid_erase.plain | std::vector | report | 4.066 | 4.001 | 0.984 | 0.981–0.987 | 0.976, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.185 | 4.185 | 1.000 | 0.996–1.004 | 0.998, 1.002 |  |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.406 | 1.009 | 1.007–1.010 | 1.006, 1.011 | worse |
| map.erase.fresh | std::map | report | 0.741 | 0.747 | 1.007 | 1.005–1.010 | 1.009, 1.006 | worse |
| map.erase.reused | std::flat_map | report | 1.081 | 1.082 | 1.001 | 0.998–1.004 | 1.002, 1.000 |  |
| map.erase.reused | std::map | report | 1.641 | 1.662 | 1.013 | 1.007–1.019 | 1.014, 1.012 | worse |
| map.find.fresh | std::map | report | 0.325 | 0.320 | 0.987 | 0.983–0.990 | 0.985, 0.988 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.002 | 0.999–1.004 | 1.002, 1.001 |  |
| map.find.reused | std::map | report | 0.734 | 0.694 | 0.945 | 0.942–0.949 | 0.941, 0.950 |  |
| map.insert.fresh | std::map | report | 0.736 | 0.749 | 1.017 | 1.014–1.020 | 1.008, 1.026 | worse |
| map.insert.reused | std::flat_map | report | 1.001 | 1.006 | 1.005 | 1.003–1.007 | 1.006, 1.004 | worse |
| map.insert.reused | std::map | report | 1.815 | 1.840 | 1.014 | 1.008–1.019 | 1.005, 1.022 | worse |
| nested.build.plain | std | report | 0.337 | 0.338 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| set.erase.fresh | std::set | report | 0.671 | 0.674 | 1.003 | 1.001–1.006 | 1.006, 1.000 | worse |
| set.erase.reused | std::flat_set | report | 1.141 | 1.137 | 0.997 | 0.993–1.000 | 1.000, 0.993 |  |
| set.erase.reused | std::set | report | 1.322 | 1.365 | 1.032 | 1.023–1.042 | 1.026, 1.039 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.002 | 0.998–1.007 | 1.003, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| set.find.reused | std::set | report | 0.663 | 0.781 | 1.179 | 1.174–1.184 | 1.184, 1.174 | worse |
| set.insert.fresh | std::set | report | 0.659 | 0.654 | 0.992 | 0.988–0.995 | 0.980, 1.004 |  |
| set.insert.reused | std::flat_set | report | 1.085 | 1.085 | 1.000 | 0.997–1.003 | 1.003, 0.998 |  |
| set.insert.reused | std::set | report | 1.446 | 1.449 | 1.002 | 0.994–1.011 | 1.001, 1.003 |  |
| str.assign_short | std::string | report | 1.770 | 1.764 | 0.996 | 0.993–1.000 | 0.998, 0.995 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.150 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.003; map.erase.fresh / std::flat_map (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.008; set.erase.fresh / std::flat_set (G9) 1.004; str.append / std::string (G9) 1.217; vec.push_back / std::vector (G9) 1.014; hero.push_back_grow.plain / std::vector (report) 1.009; map.erase.fresh / std::map (report) 1.007; map.erase.reused / std::map (report) 1.013; map.insert.fresh / std::map (report) 1.017; map.insert.reused / std::flat_map (report) 1.005; map.insert.reused / std::map (report) 1.014; set.erase.fresh / std::set (report) 1.003; set.erase.reused / std::set (report) 1.032; set.find.reused / std::set (report) 1.179

above 1.01 but not worse by the rule: reset / std (G9) 1.086

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.7 | 163.4 | 0.998 | 0.998–0.999 | 0.998, 0.999 |  |
| hero.mid_erase.plain | 912 | 896.8 | 0.983 | 0.981–0.986 | 0.975, 0.992 |  |
| hero.mid_insert.ord | 227.8 | 227.4 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| hero.mid_insert.plain | 1254 | 1251 | 0.998 | 0.995–1.001 | 0.999, 0.997 |  |
| hero.push_back_grow.ord | 31.91 | 31.93 | 1.001 | 0.999–1.002 | 1.005, 0.997 |  |
| hero.push_back_grow.plain | 35.18 | 35.44 | 1.008 | 1.006–1.009 | 1.011, 1.004 | worse |
| map.erase.fresh | 33 | 33.09 | 1.003 | 1.001–1.004 | 1.004, 1.001 | worse |
| map.erase.reused | 29.59 | 29.74 | 1.005 | 1.004–1.007 | 1.010, 1.001 | worse |
| map.find.fresh | 5.286 | 5.285 | 1.000 | 0.998–1.001 | 1.001, 0.998 |  |
| map.find.reused | 5.298 | 5.305 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| map.insert.fresh | 32.26 | 32.28 | 1.001 | 0.999–1.002 | 1.003, 0.998 |  |
| map.insert.reused | 29.55 | 29.57 | 1.001 | 0.999–1.002 | 1.005, 0.996 |  |
| nested.build.ord | 272.5 | 269.9 | 0.990 | 0.988–0.993 | 0.990, 0.991 |  |
| nested.build.plain | 279.4 | 279.7 | 1.001 | 0.999–1.003 | 0.994, 1.008 |  |
| reset | 30.83 | 30.86 | 1.001 | 0.993–1.008 | 0.999, 1.003 |  |
| set.erase.fresh | 29.8 | 29.79 | 1.000 | 0.999–1.000 | 1.002, 0.997 |  |
| set.erase.reused | 23.22 | 23.25 | 1.002 | 0.998–1.005 | 1.007, 0.996 |  |
| set.find.fresh | 4.879 | 4.879 | 1.000 | 1.000–1.001 | 1.000, 1.000 |  |
| set.find.reused | 4.88 | 4.877 | 0.999 | 0.998–1.000 | 0.999, 0.999 |  |
| set.insert.fresh | 29.25 | 28.97 | 0.990 | 0.988–0.992 | 0.993, 0.988 |  |
| set.insert.reused | 22.95 | 22.97 | 1.001 | 0.999–1.003 | 1.004, 0.997 |  |
| str.append | 1.537 | 1.552 | 1.010 | 1.006–1.014 | 1.018, 1.001 | worse |
| str.assign_long | 2.945 | 2.719 | 0.923 | 0.922–0.925 | 0.924, 0.923 |  |
| str.assign_short | 0.9373 | 0.9271 | 0.989 | 0.986–0.993 | 0.991, 0.988 |  |
| str.assign_short.copy | 0.6254 | 0.6213 | 0.994 | 0.989–0.998 | 0.999, 0.988 |  |
| str.assign_short.view | 0.9435 | 0.9369 | 0.993 | 0.989–0.997 | 0.998, 0.989 |  |
| vec.iterate | 0.03317 | 0.03314 | 0.999 | 0.999–1.000 | 1.000, 0.999 |  |
| vec.mid_erase | 40.12 | 40.25 | 1.003 | 1.002–1.005 | 1.003, 1.004 | worse |
| vec.mid_insert | 40.53 | 40.35 | 0.996 | 0.994–0.997 | 0.996, 0.995 |  |
| vec.push_back | 0.2973 | 0.2982 | 1.003 | 0.991–1.015 | 0.984, 1.022 |  |
| vec.push_back_grow | 0.2808 | 0.2786 | 0.992 | 0.986–0.999 | 0.992, 0.993 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.push_back_grow.plain 1.008; map.erase.fresh 1.003; map.erase.reused 1.005; str.append 1.010; vec.mid_erase 1.003

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.731 | 0.729 | 0.997 | 0.994–1.000 | 1.002, 0.993 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.755 | 0.997 | 0.994–1.000 | 0.994, 1.001 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.360 | 0.984 | 0.981–0.986 | 0.976, 0.991 |  |
| map.erase.fresh | std::flat_map | G9 | 0.944 | 0.942 | 0.998 | 0.995–1.000 | 0.996, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.808 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.867 | 1.000 | 0.997–1.003 | 1.007, 0.993 |  |
| nested.build.ord | std | G9 | 0.320 | 0.320 | 1.003 | 1.000–1.006 | 1.018, 0.988 |  |
| reset | std | G9 | 0.251 | 0.263 | 1.047 | 1.041–1.052 | 1.090, 1.005 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.095 | 1.091 | 0.996 | 0.993–1.000 | 0.999, 0.994 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 1.001 | 1.000 | 0.998 | 0.996–1.001 | 1.000, 0.996 |  |
| str.append | std::string | G9 | 0.432 | 0.425 | 0.983 | 0.978–0.988 | 1.002, 0.964 |  |
| str.assign_long | std::string | G9 | 0.867 | 0.866 | 0.999 | 0.997–1.001 | 0.998, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.024 | 1.020 | 0.996 | 0.993–0.999 | 0.989, 1.003 |  |
| vec.mid_insert | std::vector | G9 | 0.991 | 0.965 | 0.973 | 0.971–0.976 | 0.971, 0.976 |  |
| vec.push_back | std::vector | G9 | 1.124 | 1.118 | 0.995 | 0.992–0.998 | 0.997, 0.993 |  |
| vec.push_back_grow | std::vector | G9 | 0.756 | 0.758 | 1.003 | 0.999–1.008 | 0.995, 1.011 |  |
| hero.mid_erase.plain | std::vector | report | 4.056 | 4.030 | 0.993 | 0.990–0.997 | 0.991, 0.996 |  |
| hero.mid_insert.plain | std::vector | report | 4.155 | 4.135 | 0.995 | 0.992–0.999 | 0.985, 1.006 |  |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.400 | 0.993 | 0.991–0.995 | 0.984, 1.003 |  |
| map.erase.fresh | std::map | report | 0.746 | 0.749 | 1.005 | 1.002–1.007 | 1.002, 1.007 | worse |
| map.erase.reused | std::flat_map | report | 1.080 | 1.086 | 1.006 | 1.003–1.009 | 1.004, 1.008 | worse |
| map.erase.reused | std::map | report | 1.602 | 1.666 | 1.040 | 1.033–1.047 | 1.046, 1.033 | worse |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.001 | 0.997–1.005 | 0.998, 1.004 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| map.find.reused | std::map | report | 0.734 | 0.738 | 1.006 | 1.001–1.010 | 1.013, 0.998 |  |
| map.insert.fresh | std::map | report | 0.742 | 0.727 | 0.980 | 0.976–0.984 | 0.974, 0.987 |  |
| map.insert.reused | std::flat_map | report | 1.006 | 1.006 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| map.insert.reused | std::map | report | 1.779 | 1.812 | 1.018 | 1.012–1.024 | 1.014, 1.023 | worse |
| nested.build.plain | std | report | 0.330 | 0.334 | 1.012 | 1.009–1.016 | 1.027, 0.998 | worse? |
| set.erase.fresh | std::set | report | 0.675 | 0.676 | 1.002 | 0.999–1.005 | 1.004, 0.999 |  |
| set.erase.reused | std::flat_set | report | 1.142 | 1.135 | 0.994 | 0.991–0.998 | 0.992, 0.996 |  |
| set.erase.reused | std::set | report | 1.320 | 1.350 | 1.023 | 1.015–1.031 | 1.029, 1.017 | worse |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 0.998 | 0.994–1.003 | 0.998, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.find.reused | std::set | report | 0.677 | 0.683 | 1.009 | 1.006–1.011 | 1.005, 1.012 | worse |
| set.insert.fresh | std::set | report | 0.652 | 0.650 | 0.997 | 0.994–1.000 | 1.003, 0.991 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.083 | 1.002 | 0.998–1.005 | 1.000, 1.004 |  |
| set.insert.reused | std::set | report | 1.394 | 1.418 | 1.017 | 1.007–1.027 | 1.022, 1.013 | worse |
| str.assign_short | std::string | report | 1.768 | 1.764 | 0.998 | 0.997–0.999 | 0.997, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.149 | 0.999 | 0.998–1.000 | 1.000, 0.998 |  |

worse at t1a than t0a (by the rule): reset / std (G9) 1.047; map.erase.fresh / std::map (report) 1.005; map.erase.reused / std::flat_map (report) 1.006; map.erase.reused / std::map (report) 1.040; map.insert.reused / std::map (report) 1.018; set.erase.reused / std::set (report) 1.023; set.find.reused / std::set (report) 1.009; set.insert.reused / std::set (report) 1.017

above 1.01 but not worse by the rule: nested.build.plain / std (report) 1.012

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 165.5 | 163.8 | 0.990 | 0.987–0.993 | 0.997, 0.983 |  |
| hero.mid_erase.plain | 911.6 | 904.5 | 0.992 | 0.990–0.995 | 0.991, 0.993 |  |
| hero.mid_insert.ord | 228.8 | 228.5 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| hero.mid_insert.plain | 1251 | 1240 | 0.991 | 0.990–0.993 | 0.988, 0.995 |  |
| hero.push_back_grow.ord | 31.88 | 31.61 | 0.992 | 0.989–0.994 | 0.991, 0.993 |  |
| hero.push_back_grow.plain | 35.28 | 35.11 | 0.995 | 0.993–0.997 | 0.992, 0.999 |  |
| map.erase.fresh | 33.02 | 33.03 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| map.erase.reused | 29.65 | 29.76 | 1.004 | 1.002–1.005 | 1.002, 1.006 | worse |
| map.find.fresh | 5.288 | 5.289 | 1.000 | 0.998–1.002 | 1.000, 1.001 |  |
| map.find.reused | 5.305 | 5.303 | 1.000 | 0.998–1.001 | 0.998, 1.001 |  |
| map.insert.fresh | 32.3 | 32.24 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| map.insert.reused | 29.54 | 29.57 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| nested.build.ord | 267.3 | 269.3 | 1.008 | 1.005–1.010 | 1.019, 0.996 |  |
| nested.build.plain | 275.6 | 281.7 | 1.022 | 1.017–1.027 | 1.050, 0.995 | worse? |
| reset | 29.57 | 31.07 | 1.051 | 1.045–1.057 | 1.111, 0.994 | worse? |
| set.erase.fresh | 29.81 | 29.78 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.erase.reused | 23.28 | 23.19 | 0.996 | 0.993–0.999 | 0.996, 0.997 |  |
| set.find.fresh | 4.884 | 4.882 | 1.000 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.879 | 4.877 | 1.000 | 0.998–1.001 | 0.999, 1.001 |  |
| set.insert.fresh | 28.95 | 28.89 | 0.998 | 0.996–1.000 | 1.000, 0.996 |  |
| set.insert.reused | 22.9 | 22.84 | 0.998 | 0.995–1.000 | 0.995, 1.001 |  |
| str.append | 1.581 | 1.547 | 0.978 | 0.974–0.983 | 0.981, 0.976 |  |
| str.assign_long | 2.952 | 2.942 | 0.997 | 0.994–0.999 | 0.997, 0.996 |  |
| str.assign_short | 0.9405 | 0.9334 | 0.992 | 0.989–0.996 | 0.997, 0.988 |  |
| str.assign_short.copy | 0.6287 | 0.624 | 0.993 | 0.988–0.997 | 0.995, 0.990 |  |
| str.assign_short.view | 0.9442 | 0.938 | 0.993 | 0.990–0.997 | 1.002, 0.985 |  |
| vec.iterate | 0.03343 | 0.03324 | 0.994 | 0.992–0.997 | 0.999, 0.990 |  |
| vec.mid_erase | 40.79 | 40.55 | 0.994 | 0.991–0.997 | 0.991, 0.997 |  |
| vec.mid_insert | 40.56 | 40.73 | 1.004 | 1.002–1.006 | 1.006, 1.003 | worse |
| vec.push_back | 0.2759 | 0.2722 | 0.986 | 0.983–0.990 | 0.994, 0.979 |  |
| vec.push_back_grow | 0.2708 | 0.2694 | 0.995 | 0.992–0.998 | 0.998, 0.992 |  |

XOffset slower at t1a than t0a (worse by the rule): map.erase.reused 1.004; vec.mid_insert 1.004

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.push_back_grow.plain (XOffset ns/op) | 1.008 | 0.995 | 0.993–0.997 | not worse |
| map.erase.fresh (XOffset ns/op) | 1.003 | 1.000 | 0.999–1.002 | not worse |
| map.erase.reused (XOffset ns/op) | 1.005 | 1.004 | 1.002–1.005 | still worse |
| str.append (XOffset ns/op) | 1.010 | 0.978 | 0.974–0.983 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.003 | 0.994 | 0.991–0.997 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.003 | 0.997 | 0.994–1.000 | not worse |
| hero.push_back_grow.plain / std::vector (report) | 1.009 | 0.993 | 0.991–0.995 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.005 | 0.998 | 0.995–1.000 | not worse |
| map.erase.fresh / std::map (report) | 1.007 | 1.005 | 1.002–1.007 | still worse |
| map.erase.reused / std::map (report) | 1.013 | 1.040 | 1.033–1.047 | still worse |
| map.insert.fresh / std::flat_map (G9) | 1.008 | 1.000 | 0.997–1.003 | not worse |
| map.insert.fresh / std::map (report) | 1.017 | 0.980 | 0.976–0.984 | not worse |
| map.insert.reused / std::flat_map (report) | 1.005 | 1.000 | 0.997–1.003 | not worse |
| map.insert.reused / std::map (report) | 1.014 | 1.018 | 1.012–1.024 | still worse |
| set.erase.fresh / std::flat_set (G9) | 1.004 | 0.996 | 0.993–1.000 | not worse |
| set.erase.fresh / std::set (report) | 1.003 | 1.002 | 0.999–1.005 | not worse |
| set.erase.reused / std::set (report) | 1.032 | 1.023 | 1.015–1.031 | still worse |
| set.find.reused / std::set (report) | 1.179 | 1.009 | 1.006–1.011 | still worse |
| str.append / std::string (G9) | 1.217 | 0.983 | 0.978–0.988 | not worse |
| vec.push_back / std::vector (G9) | 1.014 | 0.995 | 0.992–0.998 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.65 | 163.49 | 0.999 | 225.07 | 0.996 |
| hero.mid_erase.plain | 911.49 | 897.25 | 0.984 | 224.85 | 0.994 |
| hero.mid_insert.ord | 227.70 | 226.89 | 0.996 | 299.65 | 1.000 |
| hero.mid_insert.plain | 1254.06 | 1246.42 | 0.994 | 300.84 | 0.993 |
| hero.push_back_grow.ord | 31.83 | 31.87 | 1.001 | 87.34 | 0.992 |
| hero.push_back_grow.plain | 35.13 | 35.39 | 1.008 | 87.14 | 0.995 |
| map.erase.fresh | 32.97 | 33.04 | 1.002 | 35.16 | 1.001 |
| map.erase.reused | 29.45 | 29.67 | 1.008 | 27.28 | 1.004 |
| map.find.fresh | 5.29 | 5.28 | 0.999 | 6.55 | 1.000 |
| map.find.reused | 5.29 | 5.30 | 1.001 | 6.55 | 0.999 |
| map.insert.fresh | 32.24 | 32.26 | 1.000 | 37.21 | 0.997 |
| map.insert.reused | 29.51 | 29.49 | 1.000 | 29.51 | 1.001 |
| nested.build.ord | 271.72 | 269.53 | 0.992 | 833.00 | 1.009 |
| nested.build.plain | 279.34 | 279.61 | 1.001 | 827.68 | 1.022 |
| reset | 30.65 | 30.80 | 1.005 | 127.68 | 1.047 |
| set.erase.fresh | 29.79 | 29.77 | 1.000 | 27.42 | 0.999 |
| set.erase.reused | 23.01 | 23.00 | 1.000 | 20.29 | 0.998 |
| set.find.fresh | 4.87 | 4.88 | 1.000 | 6.46 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 29.24 | 28.94 | 0.990 | 28.91 | 1.000 |
| set.insert.reused | 22.94 | 22.90 | 0.998 | 21.14 | 0.999 |
| str.append | 1.52 | 1.52 | 1.005 | 4.35 | 0.972 |
| str.assign_long | 2.93 | 2.71 | 0.924 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 0.999 | 0.52 | 0.990 |
| str.assign_short.copy | 0.62 | 0.61 | 0.979 | 0.55 | 0.980 |
| str.assign_short.view | 0.93 | 0.93 | 0.999 | 2.71 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.998 | 0.03 | 1.000 |
| vec.mid_erase | 40.02 | 40.13 | 1.003 | 39.46 | 1.001 |
| vec.mid_insert | 40.49 | 40.28 | 0.995 | 41.21 | 1.006 |
| vec.push_back | 0.30 | 0.30 | 1.000 | 0.27 | 0.985 |
| vec.push_back_grow | 0.28 | 0.28 | 0.986 | 0.36 | 1.000 |

## analyze t0 (G9 at the anchor b6e06788)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.115 |  1.118 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.775 |  0.778 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.26 |  0.982 |  0.983 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.63 |  1.016 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     298.17 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.87 |  0.365 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.45 |     224.61 |  4.066 |  4.076 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.86 |     304.69 |  4.185 |  4.202 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      87.18 |  0.403 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.867 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       4.35 |  0.350 |  0.351 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.19 |  0.866 |  0.868 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.38 |  0.736 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.34 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.741 |  0.743 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     29.22 |      28.89 |  1.011 |  1.013 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     29.22 |      44.11 |  0.659 |  0.661 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.671 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.54 |  1.001 |  1.002 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.815 |  1.821 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.19 |  0.734 |  0.737 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.641 |  1.650 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.085 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.71 |  1.446 |  1.457 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.49 |  0.663 |  0.667 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.141 |  1.144 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.01 |  1.322 |  1.335 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     833.31 |  0.326 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     118.50 |  0.241 |  0.245 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     828.12 |  0.337 |  0.338 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.131 |  1.134 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.956 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.71 |  1.014 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.95 |  0.728 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     297.53 |  0.756 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      87.24 |  0.366 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    897.14 |     224.61 |  4.001 |  4.012 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1248.70 |     294.92 |  4.185 |  4.203 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.36 |      87.00 |  0.406 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.341 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.749 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.27 |       6.55 |  0.805 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.27 |      16.54 |  0.320 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.08 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.747 |  0.749 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.99 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.32 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.090 |  1.093 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.674 |  0.675 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.006 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.840 |  1.852 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.63 |  0.694 |  0.697 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.082 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.66 |  1.662 |  1.671 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.08 |  1.085 |  1.090 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.46 |  1.449 |  1.464 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.23 |  0.781 |  0.783 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.137 |  1.142 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.60 |  1.365 |  1.377 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     828.12 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     30.61 |     117.19 |  0.261 |  0.264 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     828.12 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-03T19:14:00Z block 1 start: waited 180s load 1.10 WindowServer 3.3% remote 4
2026-11-03T19:17:04Z block 2 start: waited 180s load 1.37 WindowServer 3.3% remote 4
2026-11-03T19:20:09Z block 3 start: waited 180s load 1.42 WindowServer 3.7% remote 4
2026-11-03T19:23:14Z block 4 start: waited 180s load 1.63 WindowServer 3.3% remote 4
```
