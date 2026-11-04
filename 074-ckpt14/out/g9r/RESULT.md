# G9R3 result (2026-11-04T03:43:04Z)

t0 eabb583b033d9dba9b30fbdb1ffc66b4bc8f953a (next_cpp26 eabb583b (#210; current PR #191 base)); t1 6ec08cc6ba8e837925f008947aaa49b16e72c420 (CKPT14 head (PR #191 delta sync 6ec08cc6)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 eabb583b033d9dba9b30fbdb1ffc66b4bc8f953a tree ee1e26a8fa88515f4197b59a1475fb552846f78d typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 eabb583b (#210; current PR #191 base))
t1 6ec08cc6ba8e837925f008947aaa49b16e72c420 tree 4fa0e6a06fa8c90b234f58d6c6c2ea8ff5f36ed7 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT14 head (PR #191 delta sync 6ec08cc6))
t1: diff over t0 sha256 5810794fb05b683f086c0b52acccf1874fa1625ed529a754793062f8f1bedb0b ( 11 files changed, 5931 insertions(+), 24 deletions(-))
t1: library header diff over t0:  2 files changed, 271 insertions(+), 22 deletions(-)
t1: first-parent commits over t0:
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
  6ff26fc8 Delta sync: 64-byte change blocks, a 14-byte header, receiver checks only what changed
  779eef11 Delta sync: faster sender diff; key delta for a receiver that joins late
  44ec96e1 Delta sync: send only changed payload bytes to a receiver that holds a copy
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 cbdab37ce5213b313f9cbae2f81d6a569d1fe515171ead440165e3d8731521d1
reloc_bench t1 sha256 0c1b4be0bd1421852eaa03fb20a6a95e040939dee772346abe1b4d3696e91cee
reloc_bench t0a sha256 997947e5a22513ed50575bc5e308ad08ee0c568181d4dee72419e74f025ee8bc
reloc_bench t1a sha256 244492bcaef187a8d1e59661b5a4e560b863821ccd6cf8b9735ac684dbdbc32d
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.732 | 1.005 | 1.002–1.009 | 1.002, 1.009 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.759 | 0.998 | 0.994–1.001 | 0.996, 1.000 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.369 | 0.364 | 0.987 | 0.984–0.990 | 0.976, 0.997 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.936 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.872 | 1.006 | 1.003–1.008 | 1.006, 1.005 | worse |
| nested.build.ord | std | G9 | 0.329 | 0.330 | 1.005 | 1.002–1.008 | 1.014, 0.996 |  |
| reset | std | G9 | 0.235 | 0.230 | 0.977 | 0.965–0.991 | 0.891, 1.072 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.087 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.998 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| str.append | std::string | G9 | 0.424 | 0.349 | 0.825 | 0.821–0.829 | 0.848, 0.802 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.866 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.998–1.003 | 1.000, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 1.000–1.001 | 1.001, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.020 | 1.000 | 0.998–1.002 | 1.003, 0.997 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.977 | 1.022 | 1.017–1.028 | 1.032, 1.013 | worse |
| vec.push_back | std::vector | G9 | 1.112 | 1.108 | 0.996 | 0.993–0.999 | 0.991, 1.002 |  |
| vec.push_back_grow | std::vector | G9 | 0.769 | 0.482 | 0.626 | 0.625–0.628 | 0.620, 0.633 |  |
| hero.mid_erase.plain | std::vector | report | 4.051 | 4.050 | 1.000 | 0.996–1.003 | 0.998, 1.002 |  |
| hero.mid_insert.plain | std::vector | report | 4.242 | 4.256 | 1.003 | 0.999–1.007 | 0.997, 1.010 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.402 | 0.991 | 0.988–0.994 | 0.983, 0.998 |  |
| map.erase.fresh | std::map | report | 0.739 | 0.743 | 1.005 | 1.002–1.008 | 1.002, 1.008 | worse |
| map.erase.reused | std::flat_map | report | 1.085 | 1.079 | 0.995 | 0.992–0.998 | 0.997, 0.993 |  |
| map.erase.reused | std::map | report | 1.648 | 1.636 | 0.993 | 0.987–0.999 | 0.979, 1.007 |  |
| map.find.fresh | std::map | report | 0.326 | 0.325 | 1.000 | 0.996–1.004 | 0.999, 1.001 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| map.find.reused | std::map | report | 0.721 | 0.742 | 1.030 | 1.026–1.033 | 1.042, 1.017 | worse |
| map.insert.fresh | std::map | report | 0.749 | 0.752 | 1.004 | 1.002–1.007 | 1.005, 1.004 | worse |
| map.insert.reused | std::flat_map | report | 0.999 | 1.001 | 1.002 | 1.000–1.004 | 1.004, 1.001 | worse |
| map.insert.reused | std::map | report | 1.832 | 1.835 | 1.002 | 0.997–1.006 | 0.991, 1.012 |  |
| nested.build.plain | std | report | 0.338 | 0.339 | 1.004 | 1.000–1.008 | 1.001, 1.007 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.667 | 0.995 | 0.992–0.999 | 0.984, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.137 | 0.999 | 0.996–1.002 | 0.996, 1.002 |  |
| set.erase.reused | std::set | report | 1.309 | 1.338 | 1.022 | 1.014–1.031 | 1.008, 1.037 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.008 | 1.003–1.013 | 1.011, 1.006 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.677 | 0.672 | 0.993 | 0.988–0.998 | 0.941, 1.047 |  |
| set.insert.fresh | std::set | report | 0.652 | 0.656 | 1.006 | 1.004–1.009 | 1.009, 1.003 | worse |
| set.insert.reused | std::flat_set | report | 1.080 | 1.078 | 0.998 | 0.995–1.000 | 0.991, 1.005 |  |
| set.insert.reused | std::set | report | 1.454 | 1.454 | 1.000 | 0.993–1.008 | 0.991, 1.010 |  |
| str.assign_short | std::string | report | 1.762 | 1.761 | 1.000 | 0.996–1.003 | 1.002, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.151 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.006; vec.mid_insert / std::vector (G9) 1.022; map.erase.fresh / std::map (report) 1.005; map.find.reused / std::map (report) 1.030; map.insert.fresh / std::map (report) 1.004; map.insert.reused / std::flat_map (report) 1.002; set.erase.reused / std::set (report) 1.022; set.find.fresh / std::set (report) 1.008; set.insert.fresh / std::set (report) 1.006

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 165.2 | 1.009 | 1.006–1.011 | 1.012, 1.006 | worse |
| hero.mid_erase.plain | 910.6 | 912.2 | 1.002 | 0.999–1.004 | 1.004, 0.999 |  |
| hero.mid_insert.ord | 228.1 | 227.5 | 0.997 | 0.996–0.999 | 0.996, 0.999 |  |
| hero.mid_insert.plain | 1269 | 1271 | 1.002 | 1.000–1.004 | 0.997, 1.006 |  |
| hero.push_back_grow.ord | 31.95 | 32.1 | 1.005 | 1.003–1.007 | 1.007, 1.003 | worse |
| hero.push_back_grow.plain | 35.25 | 35.34 | 1.003 | 1.001–1.005 | 1.004, 1.001 | worse |
| map.erase.fresh | 33.04 | 33 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| map.erase.reused | 29.79 | 29.68 | 0.996 | 0.994–0.999 | 0.997, 0.996 |  |
| map.find.fresh | 5.286 | 5.286 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.find.reused | 5.298 | 5.3 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| map.insert.fresh | 32.24 | 32.3 | 1.002 | 1.000–1.004 | 1.001, 1.003 | worse |
| map.insert.reused | 29.52 | 29.47 | 0.998 | 0.997–1.000 | 0.999, 0.998 |  |
| nested.build.ord | 271.2 | 273.1 | 1.007 | 1.005–1.009 | 1.009, 1.005 | worse |
| nested.build.plain | 280 | 282.7 | 1.010 | 1.007–1.012 | 1.010, 1.009 | worse |
| reset | 30.37 | 29.85 | 0.983 | 0.979–0.987 | 0.963, 1.003 |  |
| set.erase.fresh | 29.81 | 29.77 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| set.erase.reused | 23.23 | 23.21 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| set.find.fresh | 4.92 | 4.881 | 0.992 | 0.989–0.995 | 0.985, 0.999 |  |
| set.find.reused | 4.879 | 4.885 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.insert.fresh | 28.88 | 28.84 | 0.999 | 0.998–1.000 | 0.998, 0.999 |  |
| set.insert.reused | 22.9 | 22.79 | 0.995 | 0.994–0.997 | 0.993, 0.997 |  |
| str.append | 1.568 | 1.523 | 0.971 | 0.967–0.975 | 0.984, 0.959 |  |
| str.assign_long | 2.939 | 2.943 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| str.assign_short | 0.9323 | 0.9323 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| str.assign_short.copy | 0.624 | 0.623 | 0.998 | 0.994–1.003 | 1.000, 0.997 |  |
| str.assign_short.view | 0.9404 | 0.9436 | 1.003 | 1.000–1.007 | 1.001, 1.006 |  |
| vec.iterate | 0.03315 | 0.03331 | 1.005 | 1.003–1.007 | 1.009, 1.001 | worse |
| vec.mid_erase | 40.44 | 40.53 | 1.002 | 1.000–1.004 | 1.004, 1.000 | worse |
| vec.mid_insert | 40.43 | 40.74 | 1.008 | 1.003–1.013 | 1.012, 1.003 | worse |
| vec.push_back | 0.2912 | 0.2911 | 1.000 | 0.988–1.012 | 1.009, 0.991 |  |
| vec.push_back_grow | 0.2769 | 0.2763 | 0.998 | 0.992–1.004 | 1.001, 0.995 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.009; hero.push_back_grow.ord 1.005; hero.push_back_grow.plain 1.003; map.insert.fresh 1.002; nested.build.ord 1.007; nested.build.plain 1.010; vec.iterate 1.005; vec.mid_erase 1.002; vec.mid_insert 1.008

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.729 | 0.725 | 0.995 | 0.992–0.998 | 1.000, 0.990 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.761 | 1.001 | 0.998–1.005 | 0.998, 1.005 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.357 | 0.358 | 1.002 | 0.997–1.007 | 1.008, 0.996 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.944 | 1.002 | 1.000–1.003 | 1.000, 1.003 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.869 | 1.002 | 0.999–1.004 | 1.004, 0.999 |  |
| nested.build.ord | std | G9 | 0.308 | 0.310 | 1.006 | 0.998–1.014 | 1.044, 0.969 |  |
| reset | std | G9 | 0.211 | 0.221 | 1.049 | 1.032–1.066 | 1.158, 0.949 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.089 | 0.998 | 0.995–1.001 | 0.997, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 0.998 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| str.append | std::string | G9 | 0.427 | 0.424 | 0.993 | 0.988–0.999 | 1.003, 0.984 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.867 | 1.003 | 1.000–1.005 | 1.002, 1.003 | worse |
| str.assign_short.view | std::string | G9 | 0.343 | 0.340 | 0.993 | 0.990–0.995 | 0.987, 0.998 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.001 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.mid_erase | std::vector | G9 | 1.014 | 1.017 | 1.002 | 1.000–1.004 | 1.002, 1.003 | worse |
| vec.mid_insert | std::vector | G9 | 0.962 | 0.961 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| vec.push_back | std::vector | G9 | 1.119 | 1.119 | 1.001 | 0.997–1.004 | 1.003, 0.998 |  |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.765 | 0.999 | 0.995–1.003 | 1.000, 0.998 |  |
| hero.mid_erase.plain | std::vector | report | 4.054 | 4.048 | 0.999 | 0.996–1.002 | 0.996, 1.001 |  |
| hero.mid_insert.plain | std::vector | report | 4.168 | 4.205 | 1.009 | 1.005–1.013 | 1.010, 1.008 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.399 | 0.399 | 1.000 | 0.995–1.005 | 0.997, 1.003 |  |
| map.erase.fresh | std::map | report | 0.739 | 0.744 | 1.007 | 1.004–1.010 | 1.012, 1.002 | worse |
| map.erase.reused | std::flat_map | report | 1.079 | 1.084 | 1.004 | 1.001–1.007 | 1.004, 1.004 | worse |
| map.erase.reused | std::map | report | 1.587 | 1.560 | 0.983 | 0.975–0.991 | 0.994, 0.972 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 1.000 | 0.996–1.004 | 0.997, 1.003 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |
| map.find.reused | std::map | report | 0.717 | 0.717 | 1.000 | 0.995–1.005 | 1.019, 0.982 |  |
| map.insert.fresh | std::map | report | 0.742 | 0.733 | 0.987 | 0.984–0.991 | 0.971, 1.004 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 1.002 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.insert.reused | std::map | report | 1.784 | 1.787 | 1.001 | 0.995–1.008 | 1.011, 0.992 |  |
| nested.build.plain | std | report | 0.318 | 0.320 | 1.006 | 0.998–1.014 | 1.050, 0.963 |  |
| set.erase.fresh | std::set | report | 0.665 | 0.665 | 1.001 | 0.997–1.005 | 0.995, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.140 | 1.136 | 0.996 | 0.992–1.000 | 0.996, 0.996 |  |
| set.erase.reused | std::set | report | 1.286 | 1.278 | 0.994 | 0.986–1.003 | 1.005, 0.983 |  |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.003 | 0.998–1.007 | 1.004, 1.001 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.684 | 1.012 | 1.008–1.015 | 1.013, 1.010 | worse |
| set.insert.fresh | std::set | report | 0.646 | 0.655 | 1.014 | 1.010–1.017 | 1.002, 1.026 | worse |
| set.insert.reused | std::flat_set | report | 1.077 | 1.080 | 1.002 | 1.000–1.005 | 1.002, 1.003 | worse |
| set.insert.reused | std::set | report | 1.396 | 1.388 | 0.994 | 0.985–1.004 | 1.016, 0.973 |  |
| str.assign_short | std::string | report | 1.767 | 1.767 | 1.000 | 0.998–1.002 | 1.002, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.152 | 1.151 | 1.000 | 0.996–1.003 | 0.996, 1.003 |  |

worse at t1a than t0a (by the rule): str.assign_long / std::string (G9) 1.003; vec.mid_erase / std::vector (G9) 1.002; hero.mid_insert.plain / std::vector (report) 1.009; map.erase.fresh / std::map (report) 1.007; map.erase.reused / std::flat_map (report) 1.004; set.find.reused / std::set (report) 1.012; set.insert.fresh / std::set (report) 1.014; set.insert.reused / std::flat_set (report) 1.002

above 1.01 but not worse by the rule: reset / std (G9) 1.049

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.6 | 164.1 | 0.997 | 0.994–0.999 | 0.997, 0.996 |  |
| hero.mid_erase.plain | 909.7 | 910.2 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| hero.mid_insert.ord | 229.8 | 229.3 | 0.998 | 0.995–1.000 | 0.998, 0.997 |  |
| hero.mid_insert.plain | 1250 | 1261 | 1.008 | 1.006–1.010 | 1.012, 1.005 | worse |
| hero.push_back_grow.ord | 31.73 | 31.73 | 1.000 | 0.998–1.002 | 1.007, 0.994 |  |
| hero.push_back_grow.plain | 35.36 | 35.24 | 0.997 | 0.994–0.999 | 0.994, 0.999 |  |
| map.erase.fresh | 33.04 | 33.03 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| map.erase.reused | 29.62 | 29.75 | 1.004 | 1.003–1.006 | 1.007, 1.002 | worse |
| map.find.fresh | 5.291 | 5.287 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| map.find.reused | 5.301 | 5.309 | 1.001 | 1.000–1.003 | 1.000, 1.003 |  |
| map.insert.fresh | 32.23 | 32.35 | 1.004 | 1.002–1.006 | 1.002, 1.005 | worse |
| map.insert.reused | 29.49 | 29.56 | 1.002 | 1.000–1.004 | 1.001, 1.003 | worse |
| nested.build.ord | 267.2 | 268.7 | 1.006 | 1.004–1.008 | 1.003, 1.008 | worse |
| nested.build.plain | 274.2 | 276.6 | 1.009 | 1.005–1.012 | 1.007, 1.010 | worse |
| reset | 29.91 | 29.26 | 0.978 | 0.973–0.984 | 0.982, 0.974 |  |
| set.erase.fresh | 29.75 | 29.79 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| set.erase.reused | 23.26 | 23.2 | 0.998 | 0.994–1.001 | 0.999, 0.996 |  |
| set.find.fresh | 4.879 | 4.888 | 1.002 | 1.000–1.003 | 1.000, 1.003 |  |
| set.find.reused | 4.878 | 4.884 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| set.insert.fresh | 28.88 | 28.93 | 1.002 | 1.000–1.003 | 1.002, 1.002 |  |
| set.insert.reused | 22.77 | 22.79 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| str.append | 1.559 | 1.566 | 1.005 | 1.000–1.009 | 1.005, 1.004 | worse |
| str.assign_long | 2.941 | 2.953 | 1.004 | 1.002–1.007 | 1.004, 1.004 | worse |
| str.assign_short | 0.9386 | 0.9406 | 1.002 | 0.998–1.006 | 1.002, 1.002 |  |
| str.assign_short.copy | 0.6303 | 0.6279 | 0.996 | 0.991–1.002 | 0.994, 0.998 |  |
| str.assign_short.view | 0.9442 | 0.9438 | 1.000 | 0.996–1.003 | 0.997, 1.002 |  |
| vec.iterate | 0.03324 | 0.03324 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | 40.41 | 40.37 | 0.999 | 0.998–1.000 | 1.001, 0.997 |  |
| vec.mid_insert | 40.7 | 40.65 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| vec.push_back | 0.2721 | 0.2724 | 1.001 | 0.999–1.004 | 0.999, 1.003 |  |
| vec.push_back_grow | 0.2706 | 0.2705 | 0.999 | 0.996–1.003 | 1.004, 0.995 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_insert.plain 1.008; map.erase.reused 1.004; map.insert.fresh 1.004; map.insert.reused 1.002; nested.build.ord 1.006; nested.build.plain 1.009; str.append 1.005; str.assign_long 1.004

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.009 | 0.997 | 0.994–0.999 | not worse |
| hero.push_back_grow.ord (XOffset ns/op) | 1.005 | 1.000 | 0.998–1.002 | not worse |
| hero.push_back_grow.plain (XOffset ns/op) | 1.003 | 0.997 | 0.994–0.999 | not worse |
| map.insert.fresh (XOffset ns/op) | 1.002 | 1.004 | 1.002–1.006 | still worse |
| nested.build.ord (XOffset ns/op) | 1.007 | 1.006 | 1.004–1.008 | still worse |
| nested.build.plain (XOffset ns/op) | 1.010 | 1.009 | 1.005–1.012 | still worse |
| vec.iterate (XOffset ns/op) | 1.005 | 1.000 | 0.999–1.001 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.002 | 0.999 | 0.998–1.000 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.008 | 0.999 | 0.997–1.000 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.995 | 0.992–0.998 | not worse |
| map.erase.fresh / std::map (report) | 1.005 | 1.007 | 1.004–1.010 | still worse |
| map.find.reused / std::map (report) | 1.030 | 1.000 | 0.995–1.005 | not worse |
| map.insert.fresh / std::flat_map (G9) | 1.006 | 1.002 | 0.999–1.004 | not worse |
| map.insert.fresh / std::map (report) | 1.004 | 0.987 | 0.984–0.991 | not worse |
| map.insert.reused / std::flat_map (report) | 1.002 | 0.998 | 0.996–1.001 | not worse |
| set.erase.reused / std::set (report) | 1.022 | 0.994 | 0.986–1.003 | not worse |
| set.find.fresh / std::set (report) | 1.008 | 1.003 | 0.998–1.007 | not worse |
| set.insert.fresh / std::set (report) | 1.006 | 1.014 | 1.010–1.017 | still worse |
| vec.mid_insert / std::vector (G9) | 1.022 | 0.999 | 0.997–1.001 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.49 | 164.72 | 1.007 | 223.58 | 0.998 |
| hero.mid_erase.plain | 909.17 | 911.10 | 1.002 | 224.56 | 1.000 |
| hero.mid_insert.ord | 227.94 | 227.13 | 0.996 | 298.05 | 0.997 |
| hero.mid_insert.plain | 1262.80 | 1267.47 | 1.004 | 297.67 | 1.007 |
| hero.push_back_grow.ord | 31.87 | 31.99 | 1.004 | 86.44 | 0.999 |
| hero.push_back_grow.plain | 35.13 | 35.25 | 1.003 | 86.67 | 0.994 |
| map.erase.fresh | 32.99 | 32.96 | 0.999 | 35.12 | 1.000 |
| map.erase.reused | 29.67 | 29.60 | 0.997 | 27.32 | 1.003 |
| map.find.fresh | 5.28 | 5.28 | 1.000 | 6.54 | 0.999 |
| map.find.reused | 5.30 | 5.29 | 0.999 | 6.55 | 1.000 |
| map.insert.fresh | 32.25 | 32.26 | 1.000 | 37.06 | 1.003 |
| map.insert.reused | 29.47 | 29.40 | 0.998 | 29.48 | 1.002 |
| nested.build.ord | 270.99 | 272.92 | 1.007 | 823.21 | 1.007 |
| nested.build.plain | 279.62 | 282.54 | 1.010 | 826.47 | 1.008 |
| reset | 30.25 | 29.77 | 0.984 | 128.75 | 0.970 |
| set.erase.fresh | 29.77 | 29.75 | 1.000 | 27.40 | 1.002 |
| set.erase.reused | 23.08 | 23.03 | 0.998 | 20.33 | 0.998 |
| set.find.fresh | 4.88 | 4.87 | 0.999 | 6.52 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.88 | 28.84 | 0.999 | 28.93 | 1.001 |
| set.insert.reused | 22.85 | 22.77 | 0.996 | 21.16 | 1.001 |
| str.append | 1.55 | 1.50 | 0.973 | 3.64 | 1.010 |
| str.assign_long | 2.93 | 2.93 | 0.999 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 0.999 | 0.52 | 1.011 |
| str.assign_short.copy | 0.61 | 0.61 | 0.989 | 0.54 | 0.982 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 0.999 |
| vec.iterate | 0.03 | 0.03 | 1.004 | 0.03 | 1.001 |
| vec.mid_erase | 40.31 | 40.40 | 1.002 | 39.60 | 0.998 |
| vec.mid_insert | 40.36 | 40.44 | 1.002 | 42.24 | 1.000 |
| vec.push_back | 0.29 | 0.29 | 1.001 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.27 | 0.991 | 0.36 | 0.996 |

## analyze t0 (G9 at the anchor eabb583b)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.112 |  1.116 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.957 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.71 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.30 |  0.728 |  0.730 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.92 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.59 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.86 |     224.61 |  4.051 |  4.065 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.67 |     303.38 |  4.242 |  4.261 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.16 |      86.83 |  0.406 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.762 |  1.769 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.65 |  0.424 |  0.426 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.868 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.749 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.54 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.35 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.19 |  0.652 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.45 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.03 |  0.670 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.46 |  0.999 |  1.001 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.832 |  1.843 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.40 |  0.721 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.085 |  1.089 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.58 |  1.648 |  1.659 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.16 |  1.080 |  1.083 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.54 |  1.454 |  1.465 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.23 |  0.677 |  0.681 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.138 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.09 |  1.309 |  1.321 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     822.94 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     120.44 |  0.235 |  0.238 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     824.22 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.108 |  1.111 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.56 |  0.482 |  0.483 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      41.75 |  0.977 |  0.984 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.79 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.72 |     223.95 |  0.732 |  0.735 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     295.90 |  0.759 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.02 |      87.10 |  0.364 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     224.61 |  4.050 |  4.064 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1268.22 |     304.03 |  4.256 |  4.272 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.83 |  0.402 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.761 |  1.767 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.49 |       4.35 |  0.349 |  0.351 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.872 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.752 |  0.754 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.19 |  0.743 |  0.745 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.78 |  0.656 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.29 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.667 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.38 |      29.38 |  1.001 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.38 |      16.03 |  1.835 |  1.840 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.10 |  0.742 |  0.744 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.079 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.66 |  1.636 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.078 |  1.081 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.46 |  1.454 |  1.466 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.18 |  0.672 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.137 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.338 |  1.349 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     825.53 |  0.330 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     29.94 |     121.09 |  0.230 |  0.233 | pass    |
| nested.build.plain           | std            | report |  808 |    282.55 |     828.12 |  0.339 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T03:33:46Z block 1 start: waited 180s load 1.33 WindowServer 3.9% remote 4
2026-11-04T03:36:50Z block 2 start: waited 180s load 1.45 WindowServer 4% remote 4
2026-11-04T03:39:55Z block 3 start: waited 180s load 2.17 WindowServer 4.8% remote 4
2026-11-04T03:43:00Z block 4 start: waited 180s load 2.35 WindowServer 42.6% remote 4
```
