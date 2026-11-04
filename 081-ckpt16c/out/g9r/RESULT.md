# G9R3 result (2026-11-04T10:18:28Z)

t0 49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479 (next_cpp26 49ce3c68 (#212; mainline merged into PR #191)); t1 d5401486e5cc2da95c495c3d4f1699850ce1fd53 (CKPT16C head (PR #191 delta sync d5401486)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479 tree 9b9544a872e52b29c154df3e2940c7ce93bef908 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 49ce3c68 (#212; mainline merged into PR #191))
t1 d5401486e5cc2da95c495c3d4f1699850ce1fd53 tree b182991cf2ef6bf0e0535f956f9e4c2b7c6a2202 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT16C head (PR #191 delta sync d5401486))
t1: diff over t0 sha256 e9d1dc0a7a9913fb4345c0bc442e4b9351ed57d889cebf817d2f9be0b187ea4c ( 11 files changed, 5904 insertions(+), 19 deletions(-))
t1: library header diff over t0:  2 files changed, 249 insertions(+), 17 deletions(-)
t1: first-parent commits over t0:
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
  84b83a1e Delta sync: whole_blocks option, no popcount calls, lazy map after full
  fa93cf72 Delta sync: touched-only diffs, SIMD block bytes, cheaper key apply
  78ed179f Delta sync: re-check a changed container on its own
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 cbdab37ce5213b313f9cbae2f81d6a569d1fe515171ead440165e3d8731521d1
reloc_bench t1 sha256 8fd8dca0e7cc2302257a224b28330c8f4f7498fa0ce3495feb56657bc8f330d8
reloc_bench t0a sha256 997947e5a22513ed50575bc5e308ad08ee0c568181d4dee72419e74f025ee8bc
reloc_bench t1a sha256 4d9c83151ffcd4a25413753dcd3364a278cfba03836c34fa013b2a3223e1075c
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.730 | 1.005 | 1.002–1.008 | 1.001, 1.009 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.761 | 1.003 | 0.999–1.007 | 1.000, 1.007 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.367 | 0.368 | 1.005 | 1.002–1.007 | 0.981, 1.029 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.935 | 0.993 | 0.991–0.996 | 0.992, 0.994 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.806 | 0.998 | 0.995–1.000 | 0.999, 0.996 |  |
| map.insert.fresh | std::flat_map | G9 | 0.871 | 0.875 | 1.004 | 1.001–1.007 | 1.007, 1.001 | worse |
| nested.build.ord | std | G9 | 0.324 | 0.324 | 1.001 | 0.997–1.006 | 0.990, 1.013 |  |
| reset | std | G9 | 0.250 | 0.248 | 0.995 | 0.987–1.004 | 0.943, 1.050 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.086 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 1.002 | 0.997 | 0.995 | 0.993–0.998 | 1.000, 0.990 |  |
| str.append | std::string | G9 | 0.416 | 0.422 | 1.015 | 1.010–1.019 | 1.006, 1.023 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.864 | 0.997 | 0.995–0.999 | 1.000, 0.994 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| vec.iterate | std::vector | G9 | 0.999 | 1.001 | 1.002 | 0.998–1.007 | 1.004, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.021 | 1.000 | 0.998–1.003 | 0.999, 1.001 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.976 | 1.022 | 1.019–1.025 | 1.023, 1.021 | worse |
| vec.push_back | std::vector | G9 | 1.116 | 1.111 | 0.995 | 0.991–0.999 | 1.004, 0.987 |  |
| vec.push_back_grow | std::vector | G9 | 0.771 | 0.769 | 0.998 | 0.994–1.001 | 0.990, 1.005 |  |
| hero.mid_erase.plain | std::vector | report | 4.058 | 4.080 | 1.005 | 1.002–1.009 | 1.004, 1.007 | worse |
| hero.mid_insert.plain | std::vector | report | 4.226 | 4.240 | 1.003 | 0.999–1.007 | 0.998, 1.009 |  |
| hero.push_back_grow.plain | std::vector | report | 0.404 | 0.405 | 1.002 | 0.999–1.004 | 0.995, 1.008 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.739 | 0.993 | 0.991–0.996 | 0.988, 0.998 |  |
| map.erase.reused | std::flat_map | report | 1.086 | 1.081 | 0.995 | 0.991–0.999 | 0.991, 0.998 |  |
| map.erase.reused | std::map | report | 1.662 | 1.680 | 1.011 | 1.004–1.017 | 0.998, 1.024 | worse? |
| map.find.fresh | std::map | report | 0.325 | 0.325 | 1.001 | 0.997–1.005 | 1.002, 1.000 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.807 | 1.000 | 0.998–1.003 | 0.998, 1.002 |  |
| map.find.reused | std::map | report | 0.714 | 0.753 | 1.055 | 1.051–1.059 | 1.051, 1.059 | worse |
| map.insert.fresh | std::map | report | 0.750 | 0.742 | 0.989 | 0.986–0.992 | 0.981, 0.996 |  |
| map.insert.reused | std::flat_map | report | 1.007 | 1.005 | 0.998 | 0.995–1.001 | 0.999, 0.996 |  |
| map.insert.reused | std::map | report | 1.820 | 1.789 | 0.983 | 0.976–0.989 | 0.948, 1.019 |  |
| nested.build.plain | std | report | 0.334 | 0.334 | 1.001 | 0.997–1.005 | 0.979, 1.023 |  |
| set.erase.fresh | std::set | report | 0.674 | 0.672 | 0.997 | 0.995–1.000 | 0.992, 1.003 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.139 | 1.001 | 0.997–1.005 | 0.999, 1.003 |  |
| set.erase.reused | std::set | report | 1.300 | 1.345 | 1.034 | 1.025–1.044 | 1.003, 1.067 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.298 | 0.996 | 0.992–1.001 | 0.997, 0.995 |  |
| set.find.reused | std::flat_set | report | 0.753 | 0.755 | 1.002 | 0.999–1.006 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.662 | 0.620 | 0.936 | 0.932–0.941 | 0.937, 0.935 |  |
| set.insert.fresh | std::set | report | 0.657 | 0.645 | 0.983 | 0.980–0.986 | 0.972, 0.994 |  |
| set.insert.reused | std::flat_set | report | 1.084 | 1.081 | 0.997 | 0.994–1.001 | 0.997, 0.997 |  |
| set.insert.reused | std::set | report | 1.437 | 1.410 | 0.982 | 0.973–0.991 | 0.945, 1.020 |  |
| str.assign_short | std::string | report | 1.763 | 1.767 | 1.002 | 0.998–1.006 | 1.004, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.148 | 0.997 | 0.993–1.002 | 0.998, 0.997 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.004; str.append / std::string (G9) 1.015; vec.mid_insert / std::vector (G9) 1.022; hero.mid_erase.plain / std::vector (report) 1.005; map.find.reused / std::map (report) 1.055; set.erase.reused / std::set (report) 1.034

above 1.01 but not worse by the rule: map.erase.reused / std::map (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164 | 164.5 | 1.003 | 1.001–1.005 | 1.000, 1.006 | worse |
| hero.mid_erase.plain | 916.5 | 913.4 | 0.997 | 0.994–0.999 | 0.994, 0.999 |  |
| hero.mid_insert.ord | 228.1 | 228.5 | 1.002 | 0.999–1.005 | 1.003, 1.001 |  |
| hero.mid_insert.plain | 1269 | 1267 | 0.998 | 0.996–1.000 | 0.994, 1.002 |  |
| hero.push_back_grow.ord | 32.01 | 31.92 | 0.997 | 0.996–0.999 | 0.992, 1.002 |  |
| hero.push_back_grow.plain | 35.26 | 35.14 | 0.996 | 0.995–0.998 | 0.998, 0.995 |  |
| map.erase.fresh | 33.06 | 33 | 0.998 | 0.996–0.999 | 0.998, 0.998 |  |
| map.erase.reused | 29.77 | 29.75 | 0.999 | 0.997–1.002 | 0.997, 1.002 |  |
| map.find.fresh | 5.29 | 5.283 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| map.find.reused | 5.297 | 5.304 | 1.001 | 1.000–1.003 | 0.999, 1.003 |  |
| map.insert.fresh | 32.32 | 32.21 | 0.996 | 0.995–0.998 | 1.000, 0.993 |  |
| map.insert.reused | 29.67 | 29.54 | 0.996 | 0.993–0.999 | 0.999, 0.992 |  |
| nested.build.ord | 271.3 | 272.6 | 1.005 | 1.001–1.008 | 1.011, 0.999 |  |
| nested.build.plain | 278 | 279.4 | 1.005 | 1.003–1.008 | 1.005, 1.006 | worse |
| reset | 30.48 | 29.56 | 0.970 | 0.966–0.974 | 0.959, 0.980 |  |
| set.erase.fresh | 29.8 | 29.8 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| set.erase.reused | 23.26 | 23.26 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| set.find.fresh | 4.888 | 4.88 | 0.998 | 0.997–1.000 | 0.997, 0.999 |  |
| set.find.reused | 4.883 | 4.885 | 1.001 | 0.998–1.003 | 0.998, 1.003 |  |
| set.insert.fresh | 29.01 | 28.9 | 0.996 | 0.994–0.998 | 1.001, 0.991 |  |
| set.insert.reused | 22.99 | 22.83 | 0.993 | 0.991–0.995 | 0.998, 0.989 |  |
| str.append | 1.566 | 1.563 | 0.998 | 0.993–1.002 | 0.994, 1.001 |  |
| str.assign_long | 2.961 | 2.938 | 0.992 | 0.990–0.995 | 0.991, 0.993 |  |
| str.assign_short | 0.9468 | 0.9315 | 0.984 | 0.979–0.988 | 0.974, 0.994 |  |
| str.assign_short.copy | 0.634 | 0.6225 | 0.982 | 0.977–0.987 | 0.977, 0.987 |  |
| str.assign_short.view | 0.9475 | 0.9382 | 0.990 | 0.987–0.994 | 0.981, 0.999 |  |
| vec.iterate | 0.03321 | 0.0332 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| vec.mid_erase | 40.43 | 40.44 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| vec.mid_insert | 40.41 | 40.56 | 1.004 | 1.002–1.005 | 1.002, 1.005 | worse |
| vec.push_back | 0.2927 | 0.2902 | 0.991 | 0.979–1.003 | 0.992, 0.991 |  |
| vec.push_back_grow | 0.2767 | 0.2769 | 1.001 | 0.995–1.007 | 1.001, 1.000 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.003; nested.build.plain 1.005; vec.mid_insert 1.004

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.732 | 0.727 | 0.993 | 0.990–0.996 | 0.990, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.761 | 0.760 | 0.999 | 0.996–1.003 | 0.994, 1.005 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.363 | 1.000 | 0.997–1.003 | 0.996, 1.004 |  |
| map.erase.fresh | std::flat_map | G9 | 0.939 | 0.942 | 1.003 | 1.000–1.006 | 1.003, 1.004 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.809 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.865 | 0.869 | 1.005 | 1.002–1.008 | 1.005, 1.004 | worse |
| nested.build.ord | std | G9 | 0.319 | 0.319 | 0.999 | 0.994–1.004 | 1.013, 0.985 |  |
| reset | std | G9 | 0.249 | 0.246 | 0.989 | 0.981–0.998 | 0.986, 0.992 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.090 | 0.997 | 0.994–1.001 | 0.998, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.997 | 0.999 | 0.996–1.002 | 0.997, 1.002 |  |
| str.append | std::string | G9 | 0.426 | 0.429 | 1.007 | 1.001–1.012 | 0.987, 1.027 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.865 | 0.999 | 0.996–1.001 | 1.000, 0.997 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.004 | 1.000–1.008 | 1.002, 1.006 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.995–1.003 | 1.000, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.024 | 1.005 | 1.003–1.007 | 1.002, 1.008 | worse |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.957 | 0.997 | 0.994–1.000 | 0.999, 0.994 |  |
| vec.push_back | std::vector | G9 | 1.122 | 1.122 | 1.000 | 0.997–1.003 | 0.995, 1.005 |  |
| vec.push_back_grow | std::vector | G9 | 0.770 | 0.767 | 0.996 | 0.992–1.000 | 0.992, 1.000 |  |
| hero.mid_erase.plain | std::vector | report | 4.064 | 4.048 | 0.996 | 0.993–0.999 | 1.001, 0.991 |  |
| hero.mid_insert.plain | std::vector | report | 4.175 | 4.155 | 0.995 | 0.991–0.999 | 0.986, 1.004 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.403 | 0.993 | 0.990–0.996 | 0.995, 0.991 |  |
| map.erase.fresh | std::map | report | 0.741 | 0.746 | 1.007 | 1.004–1.009 | 1.000, 1.014 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.083 | 1.002 | 0.999–1.006 | 1.003, 1.002 |  |
| map.erase.reused | std::map | report | 1.642 | 1.646 | 1.002 | 0.996–1.009 | 0.993, 1.012 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.995 | 0.991–0.999 | 1.000, 0.991 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.998–1.003 | 0.999, 1.002 |  |
| map.find.reused | std::map | report | 0.715 | 0.734 | 1.026 | 1.020–1.032 | 1.032, 1.020 | worse |
| map.insert.fresh | std::map | report | 0.749 | 0.729 | 0.974 | 0.970–0.978 | 0.964, 0.984 |  |
| map.insert.reused | std::flat_map | report | 1.005 | 1.003 | 0.997 | 0.995–1.000 | 0.996, 0.998 |  |
| map.insert.reused | std::map | report | 1.807 | 1.809 | 1.001 | 0.994–1.008 | 1.025, 0.978 |  |
| nested.build.plain | std | report | 0.329 | 0.331 | 1.004 | 0.999–1.008 | 1.025, 0.983 |  |
| set.erase.fresh | std::set | report | 0.668 | 0.672 | 1.007 | 1.002–1.011 | 0.995, 1.018 |  |
| set.erase.reused | std::flat_set | report | 1.142 | 1.139 | 0.997 | 0.994–1.001 | 0.998, 0.997 |  |
| set.erase.reused | std::set | report | 1.344 | 1.330 | 0.989 | 0.981–0.998 | 0.976, 1.003 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.000 | 0.995–1.004 | 0.996, 1.003 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.756 | 0.998 | 0.996–1.001 | 1.000, 0.996 |  |
| set.find.reused | std::set | report | 0.677 | 0.677 | 1.000 | 0.996–1.004 | 0.994, 1.005 |  |
| set.insert.fresh | std::set | report | 0.649 | 0.647 | 0.997 | 0.994–1.000 | 0.999, 0.995 |  |
| set.insert.reused | std::flat_set | report | 1.079 | 1.083 | 1.004 | 1.000–1.007 | 1.001, 1.007 | worse |
| set.insert.reused | std::set | report | 1.414 | 1.395 | 0.987 | 0.977–0.997 | 1.001, 0.973 |  |
| str.assign_short | std::string | report | 1.766 | 1.764 | 0.999 | 0.996–1.002 | 0.999, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.152 | 1.002 | 0.999–1.006 | 0.999, 1.005 |  |

worse at t1a than t0a (by the rule): map.insert.fresh / std::flat_map (G9) 1.005; vec.mid_erase / std::vector (G9) 1.005; map.find.reused / std::map (report) 1.026; set.insert.reused / std::flat_set (report) 1.004

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.9 | 164.1 | 0.995 | 0.993–0.997 | 0.993, 0.997 |  |
| hero.mid_erase.plain | 912.3 | 915.3 | 1.003 | 1.000–1.006 | 1.006, 1.000 | worse |
| hero.mid_insert.ord | 229.7 | 228.9 | 0.996 | 0.995–0.998 | 0.997, 0.996 |  |
| hero.mid_insert.plain | 1251 | 1254 | 1.002 | 0.999–1.005 | 0.994, 1.010 |  |
| hero.push_back_grow.ord | 31.88 | 32.04 | 1.005 | 1.003–1.007 | 1.008, 1.002 | worse |
| hero.push_back_grow.plain | 35.29 | 35.54 | 1.007 | 1.005–1.009 | 1.007, 1.006 | worse |
| map.erase.fresh | 33.03 | 33.02 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| map.erase.reused | 29.66 | 29.73 | 1.002 | 1.000–1.005 | 1.002, 1.002 |  |
| map.find.fresh | 5.292 | 5.295 | 1.001 | 0.998–1.003 | 1.002, 0.999 |  |
| map.find.reused | 5.307 | 5.303 | 0.999 | 0.998–1.001 | 0.998, 1.000 |  |
| map.insert.fresh | 32.28 | 32.29 | 1.001 | 0.998–1.003 | 1.000, 1.001 |  |
| map.insert.reused | 29.57 | 29.57 | 1.000 | 0.999–1.002 | 1.001, 0.999 |  |
| nested.build.ord | 268.3 | 269 | 1.003 | 0.999–1.006 | 1.005, 1.001 |  |
| nested.build.plain | 277.1 | 276.7 | 0.999 | 0.995–1.002 | 0.999, 0.998 |  |
| reset | 30.11 | 28.94 | 0.961 | 0.956–0.966 | 0.989, 0.934 |  |
| set.erase.fresh | 29.78 | 29.82 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| set.erase.reused | 23.29 | 23.27 | 0.999 | 0.996–1.003 | 0.999, 0.999 |  |
| set.find.fresh | 4.882 | 4.88 | 1.000 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.89 | 4.881 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| set.insert.fresh | 28.86 | 28.92 | 1.002 | 1.001–1.003 | 1.003, 1.001 | worse |
| set.insert.reused | 22.77 | 22.85 | 1.004 | 1.001–1.006 | 1.002, 1.005 | worse |
| str.append | 1.565 | 1.583 | 1.011 | 1.007–1.016 | 1.009, 1.014 | worse |
| str.assign_long | 2.948 | 2.969 | 1.007 | 1.004–1.010 | 1.009, 1.005 | worse |
| str.assign_short | 0.9472 | 0.9562 | 1.010 | 1.005–1.014 | 1.010, 1.010 | worse |
| str.assign_short.copy | 0.6278 | 0.637 | 1.015 | 1.009–1.020 | 1.019, 1.011 | worse |
| str.assign_short.view | 0.9537 | 0.9537 | 1.000 | 0.995–1.005 | 1.012, 0.988 |  |
| vec.iterate | 0.03325 | 0.03332 | 1.002 | 0.999–1.005 | 1.000, 1.005 |  |
| vec.mid_erase | 40.51 | 40.64 | 1.003 | 1.002–1.005 | 1.003, 1.003 | worse |
| vec.mid_insert | 40.71 | 40.7 | 1.000 | 0.998–1.002 | 0.998, 1.001 |  |
| vec.push_back | 0.2725 | 0.2719 | 0.998 | 0.995–1.000 | 0.999, 0.996 |  |
| vec.push_back_grow | 0.2715 | 0.2712 | 0.999 | 0.996–1.002 | 0.998, 1.000 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.003; hero.push_back_grow.ord 1.005; hero.push_back_grow.plain 1.007; set.insert.fresh 1.002; set.insert.reused 1.004; str.append 1.011; str.assign_long 1.007; str.assign_short 1.010; str.assign_short.copy 1.015; vec.mid_erase 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.003 | 0.995 | 0.993–0.997 | not worse |
| nested.build.plain (XOffset ns/op) | 1.005 | 0.999 | 0.995–1.002 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.004 | 1.000 | 0.998–1.002 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.993 | 0.990–0.996 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.005 | 0.996 | 0.993–0.999 | not worse |
| map.find.reused / std::map (report) | 1.055 | 1.026 | 1.020–1.032 | still worse |
| map.insert.fresh / std::flat_map (G9) | 1.004 | 1.005 | 1.002–1.008 | still worse |
| set.erase.reused / std::set (report) | 1.034 | 0.989 | 0.981–0.998 | not worse |
| str.append / std::string (G9) | 1.015 | 1.007 | 1.001–1.012 | not worse |
| vec.mid_insert / std::vector (G9) | 1.022 | 0.997 | 0.994–1.000 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.65 | 164.39 | 1.004 | 225.32 | 0.994 |
| hero.mid_erase.plain | 913.38 | 913.96 | 1.001 | 224.49 | 1.001 |
| hero.mid_insert.ord | 227.62 | 227.62 | 1.000 | 297.04 | 0.997 |
| hero.mid_insert.plain | 1262.99 | 1264.10 | 1.001 | 301.66 | 0.998 |
| hero.push_back_grow.ord | 31.94 | 31.82 | 0.996 | 87.02 | 1.003 |
| hero.push_back_grow.plain | 35.18 | 35.10 | 0.998 | 87.16 | 1.004 |
| map.erase.fresh | 33.02 | 32.97 | 0.998 | 35.06 | 1.000 |
| map.erase.reused | 29.66 | 29.62 | 0.998 | 27.27 | 1.000 |
| map.find.fresh | 5.29 | 5.28 | 0.999 | 6.55 | 1.000 |
| map.find.reused | 5.29 | 5.30 | 1.001 | 6.55 | 1.000 |
| map.insert.fresh | 32.31 | 32.18 | 0.996 | 36.99 | 1.003 |
| map.insert.reused | 29.61 | 29.47 | 0.995 | 29.40 | 1.001 |
| nested.build.ord | 270.50 | 272.12 | 1.006 | 834.69 | 1.004 |
| nested.build.plain | 277.49 | 279.27 | 1.006 | 831.82 | 1.004 |
| reset | 30.50 | 29.45 | 0.965 | 121.58 | 0.966 |
| set.erase.fresh | 29.75 | 29.78 | 1.001 | 27.37 | 1.002 |
| set.erase.reused | 23.08 | 23.08 | 1.000 | 20.31 | 1.000 |
| set.find.fresh | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.find.reused | 4.88 | 4.87 | 0.999 | 6.46 | 1.000 |
| set.insert.fresh | 28.98 | 28.88 | 0.997 | 28.93 | 1.001 |
| set.insert.reused | 22.97 | 22.80 | 0.993 | 21.13 | 0.999 |
| str.append | 1.57 | 1.53 | 0.978 | 3.73 | 1.018 |
| str.assign_long | 2.93 | 2.93 | 1.000 | 3.39 | 1.001 |
| str.assign_short | 0.94 | 0.92 | 0.977 | 0.53 | 1.022 |
| str.assign_short.copy | 0.63 | 0.61 | 0.962 | 0.55 | 1.023 |
| str.assign_short.view | 0.93 | 0.93 | 1.001 | 2.71 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 1.002 | 0.03 | 0.999 |
| vec.mid_erase | 40.25 | 40.42 | 1.004 | 39.55 | 1.001 |
| vec.mid_insert | 40.35 | 40.48 | 1.003 | 42.24 | 1.000 |
| vec.push_back | 0.29 | 0.29 | 0.999 | 0.26 | 1.000 |
| vec.push_back_grow | 0.28 | 0.27 | 0.993 | 0.36 | 0.997 |

## analyze t0 (G9 at the anchor 49ce3c68)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.116 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.771 |  0.774 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.999 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.957 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.55 |  1.021 |  1.024 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     298.83 |  0.758 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      86.75 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     224.61 |  4.058 |  4.072 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.02 |     304.69 |  4.226 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      86.71 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.763 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.66 |       0.57 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.56 |       3.72 |  0.416 |  0.418 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.871 |  0.874 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.750 |  0.753 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.07 |  0.942 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  1.002 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.657 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.03 |  0.674 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.38 |  1.007 |  1.011 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.11 |  1.820 |  1.829 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.44 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.086 |  1.090 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.50 |  1.662 |  1.672 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.084 |  1.089 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.62 |  1.437 |  1.449 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.753 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.47 |  0.662 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.138 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.09 |  1.300 |  1.315 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     832.03 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.83 |  0.250 |  0.252 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     826.17 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.111 |  1.115 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.26 |  0.976 |  0.979 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.63 |  1.021 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.70 |     224.61 |  0.730 |  0.732 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.47 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.06 |     223.95 |  4.080 |  4.093 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1264.65 |     303.71 |  4.240 |  4.256 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.12 |      86.67 |  0.405 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.767 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.66 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.70 |  0.875 |  0.877 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      43.13 |  0.742 |  0.744 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.806 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.28 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.935 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.51 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.997 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.27 |  0.645 |  0.648 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.41 |  0.298 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.086 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.672 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.11 |  1.789 |  1.803 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.807 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.99 |  0.753 |  0.756 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.081 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.42 |  1.680 |  1.691 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.71 |  1.410 |  1.424 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.83 |  0.620 |  0.622 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.139 |  1.142 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.345 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     834.66 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     29.94 |     117.19 |  0.248 |  0.250 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     828.12 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T10:09:10Z block 1 start: waited 180s load 1.83 WindowServer 7.9% remote 6
2026-11-04T10:12:15Z block 2 start: waited 180s load 1.95 WindowServer 4.8% remote 6
2026-11-04T10:15:20Z block 3 start: waited 180s load 1.30 WindowServer 4.6% remote 6
2026-11-04T10:18:24Z block 4 start: waited 180s load 2.13 WindowServer 43.9% remote 6
```
