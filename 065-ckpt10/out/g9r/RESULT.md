# G9R3 result (2026-11-03T16:37:37Z)

t0 cc5b28eed7184138f7755da72f73a78e89848929 (next_cpp26 cc5b28ee (#202; PR #191 base)); t1 3e026071a28e0032801666edc8ccdb5f82f19e39 (CKPT10 head (PR #191 delta sync 3e026071)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 cc5b28eed7184138f7755da72f73a78e89848929 tree ca39b4bf6943112cd8a358f67667a4ca717f8107 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 cc5b28ee (#202; PR #191 base))
t1 3e026071a28e0032801666edc8ccdb5f82f19e39 tree 2fba9da672d40aa893264a33202f76059e183c48 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT10 head (PR #191 delta sync 3e026071))
t1: diff over t0 sha256 efb1931654e580730f2fcf84278bec09f566254d223b9d2dbe54cd392716252a ( 10 files changed, 5887 insertions(+), 4 deletions(-))
t1: library header diff over t0:  1 file changed, 231 insertions(+), 2 deletions(-)
t1: first-parent commits over t0:
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
reloc_bench t1 sha256 ed2c5b02f80ef7b1b102f586d3f5edf6e65bbcf29e9e36d6ff4e6dd45691813e
reloc_bench t0a sha256 1eaad515e940841a22469676e25841a514f01bd5fea45def974abaeba8442e9b
reloc_bench t1a sha256 f42541c3ff9d8c5d8f865c8a78f2e550a493de921592cf55a05b9cfc4170a758
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.725 | 0.996 | 0.992–1.001 | 0.994, 0.998 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.755 | 0.990 | 0.985–0.996 | 0.988, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.364 | 0.370 | 1.014 | 1.011–1.018 | 1.015, 1.014 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.939 | 1.003 | 1.001–1.005 | 1.003, 1.002 | worse |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.999 | 0.996–1.001 | 1.000, 0.997 |  |
| map.insert.fresh | std::flat_map | G9 | 0.869 | 0.869 | 1.000 | 0.997–1.002 | 1.007, 0.993 |  |
| nested.build.ord | std | G9 | 0.329 | 0.324 | 0.983 | 0.979–0.988 | 0.997, 0.969 |  |
| reset | std | G9 | 0.247 | 0.252 | 1.020 | 1.009–1.032 | 0.948, 1.099 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.090 | 1.002 | 0.999–1.005 | 1.003, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.755 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 1.014 | 0.994 | 0.980 | 0.978–0.982 | 0.984, 0.976 |  |
| str.append | std::string | G9 | 0.344 | 0.417 | 1.211 | 1.206–1.216 | 1.200, 1.222 | worse |
| str.assign_long | std::string | G9 | 0.865 | 0.803 | 0.929 | 0.925–0.932 | 0.927, 0.930 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.344 | 1.004 | 1.000–1.008 | 1.003, 1.006 | worse |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| vec.mid_erase | std::vector | G9 | 1.012 | 1.012 | 1.000 | 0.996–1.004 | 0.998, 1.002 |  |
| vec.mid_insert | std::vector | G9 | 0.984 | 0.960 | 0.976 | 0.971–0.980 | 0.972, 0.979 |  |
| vec.push_back | std::vector | G9 | 1.118 | 1.127 | 1.008 | 1.004–1.012 | 1.008, 1.008 | worse |
| vec.push_back_grow | std::vector | G9 | 0.778 | 0.766 | 0.984 | 0.979–0.989 | 0.979, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 4.072 | 4.000 | 0.982 | 0.978–0.987 | 0.978, 0.986 |  |
| hero.mid_insert.plain | std::vector | report | 4.207 | 4.201 | 0.998 | 0.994–1.003 | 0.995, 1.002 |  |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.410 | 1.017 | 1.013–1.021 | 1.012, 1.023 | worse |
| map.erase.fresh | std::map | report | 0.742 | 0.739 | 0.996 | 0.993–0.999 | 0.993, 0.999 |  |
| map.erase.reused | std::flat_map | report | 1.084 | 1.085 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| map.erase.reused | std::map | report | 1.655 | 1.661 | 1.003 | 0.997–1.010 | 0.994, 1.013 |  |
| map.find.fresh | std::map | report | 0.325 | 0.320 | 0.985 | 0.981–0.990 | 0.987, 0.983 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.002 | 1.001, 0.998 |  |
| map.find.reused | std::map | report | 0.743 | 0.679 | 0.913 | 0.909–0.918 | 0.877, 0.951 |  |
| map.insert.fresh | std::map | report | 0.744 | 0.748 | 1.006 | 1.003–1.009 | 1.008, 1.004 | worse |
| map.insert.reused | std::flat_map | report | 1.006 | 1.005 | 0.999 | 0.995–1.003 | 1.005, 0.993 |  |
| map.insert.reused | std::map | report | 1.805 | 1.796 | 0.995 | 0.988–1.002 | 1.015, 0.975 |  |
| nested.build.plain | std | report | 0.335 | 0.334 | 0.997 | 0.995–1.000 | 0.999, 0.996 |  |
| set.erase.fresh | std::set | report | 0.671 | 0.670 | 0.998 | 0.994–1.002 | 0.990, 1.006 |  |
| set.erase.reused | std::flat_set | report | 1.139 | 1.136 | 0.998 | 0.994–1.001 | 0.999, 0.996 |  |
| set.erase.reused | std::set | report | 1.342 | 1.345 | 1.002 | 0.995–1.010 | 0.991, 1.014 |  |
| set.find.fresh | std::set | report | 0.300 | 0.301 | 1.002 | 0.998–1.007 | 1.002, 1.003 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| set.find.reused | std::set | report | 0.662 | 0.786 | 1.187 | 1.181–1.193 | 1.177, 1.196 | worse |
| set.insert.fresh | std::set | report | 0.666 | 0.656 | 0.985 | 0.982–0.987 | 0.975, 0.995 |  |
| set.insert.reused | std::flat_set | report | 1.084 | 1.082 | 0.999 | 0.996–1.002 | 1.002, 0.996 |  |
| set.insert.reused | std::set | report | 1.455 | 1.411 | 0.970 | 0.962–0.978 | 0.978, 0.963 |  |
| str.assign_short | std::string | report | 1.768 | 1.769 | 1.000 | 0.997–1.004 | 0.998, 1.003 |  |
| str.assign_short.copy | std::string | report | 1.148 | 1.149 | 1.001 | 0.998–1.004 | 1.000, 1.002 |  |

worse at t1 than t0 (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.014; map.erase.fresh / std::flat_map (G9) 1.003; str.append / std::string (G9) 1.211; str.assign_short.view / std::string (G9) 1.004; vec.push_back / std::vector (G9) 1.008; hero.push_back_grow.plain / std::vector (report) 1.017; map.insert.fresh / std::map (report) 1.006; set.find.reused / std::set (report) 1.187

above 1.01 but not worse by the rule: reset / std (G9) 1.020

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.7 | 165 | 1.008 | 1.004–1.011 | 0.998, 1.017 |  |
| hero.mid_erase.plain | 912.3 | 910 | 0.997 | 0.994–1.001 | 0.986, 1.009 |  |
| hero.mid_insert.ord | 228.9 | 229.4 | 1.002 | 0.999–1.006 | 0.988, 1.017 |  |
| hero.mid_insert.plain | 1256 | 1273 | 1.014 | 1.010–1.018 | 1.002, 1.026 | worse |
| hero.push_back_grow.ord | 31.81 | 32.4 | 1.019 | 1.015–1.022 | 1.007, 1.031 | worse |
| hero.push_back_grow.plain | 35.12 | 36.06 | 1.027 | 1.023–1.031 | 1.013, 1.040 | worse |
| map.erase.fresh | 33.03 | 33.03 | 1.000 | 0.998–1.002 | 1.002, 0.998 |  |
| map.erase.reused | 29.75 | 29.85 | 1.003 | 1.001–1.005 | 1.003, 1.003 | worse |
| map.find.fresh | 5.292 | 5.284 | 0.998 | 0.996–1.001 | 1.000, 0.997 |  |
| map.find.reused | 5.301 | 5.307 | 1.001 | 0.999–1.003 | 1.001, 1.002 |  |
| map.insert.fresh | 32.33 | 32.25 | 0.998 | 0.996–1.000 | 0.998, 0.997 |  |
| map.insert.reused | 29.65 | 29.81 | 1.005 | 1.001–1.010 | 1.015, 0.995 |  |
| nested.build.ord | 273.4 | 270.4 | 0.989 | 0.985–0.993 | 0.985, 0.993 |  |
| nested.build.plain | 279.3 | 277.7 | 0.994 | 0.992–0.996 | 0.993, 0.995 |  |
| reset | 30.55 | 30.57 | 1.000 | 0.995–1.006 | 1.003, 0.998 |  |
| set.erase.fresh | 29.83 | 29.79 | 0.999 | 0.996–1.001 | 1.000, 0.998 |  |
| set.erase.reused | 23.21 | 23.26 | 1.002 | 0.999–1.006 | 1.001, 1.003 |  |
| set.find.fresh | 4.886 | 4.887 | 1.000 | 0.998–1.002 | 0.999, 1.002 |  |
| set.find.reused | 4.88 | 4.886 | 1.001 | 1.000–1.003 | 0.999, 1.003 |  |
| set.insert.fresh | 29.35 | 28.9 | 0.985 | 0.983–0.986 | 0.985, 0.984 |  |
| set.insert.reused | 23.02 | 22.86 | 0.993 | 0.992–0.995 | 0.995, 0.991 |  |
| str.append | 1.519 | 1.559 | 1.026 | 1.022–1.030 | 1.019, 1.034 | worse |
| str.assign_long | 2.937 | 2.766 | 0.942 | 0.938–0.946 | 0.931, 0.953 |  |
| str.assign_short | 0.9286 | 0.9468 | 1.020 | 1.015–1.024 | 1.010, 1.029 | worse |
| str.assign_short.copy | 0.6186 | 0.631 | 1.020 | 1.015–1.025 | 1.013, 1.027 | worse |
| str.assign_short.view | 0.934 | 0.9578 | 1.026 | 1.020–1.031 | 1.016, 1.035 | worse |
| vec.iterate | 0.03317 | 0.03356 | 1.012 | 1.008–1.015 | 1.001, 1.022 | worse |
| vec.mid_erase | 40.08 | 40.58 | 1.012 | 1.008–1.017 | 0.999, 1.026 | worse? |
| vec.mid_insert | 40.58 | 40.82 | 1.006 | 1.002–1.010 | 0.994, 1.018 |  |
| vec.push_back | 0.2944 | 0.2947 | 1.001 | 0.989–1.014 | 1.020, 0.982 |  |
| vec.push_back_grow | 0.2813 | 0.2786 | 0.990 | 0.983–0.998 | 0.984, 0.997 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_insert.plain 1.014; hero.push_back_grow.ord 1.019; hero.push_back_grow.plain 1.027; map.erase.reused 1.003; str.append 1.026; str.assign_short 1.020; str.assign_short.copy 1.020; str.assign_short.view 1.026; vec.iterate 1.012

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.729 | 0.730 | 1.000 | 0.998–1.003 | 0.999, 1.002 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.762 | 1.003 | 0.999–1.008 | 0.993, 1.014 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.360 | 0.994 | 0.992–0.997 | 0.995, 0.994 |  |
| map.erase.fresh | std::flat_map | G9 | 0.945 | 0.940 | 0.995 | 0.991–0.999 | 0.996, 0.994 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.808 | 1.001 | 0.996–1.006 | 1.000, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.868 | 1.002 | 0.997–1.006 | 1.005, 0.999 |  |
| nested.build.ord | std | G9 | 0.321 | 0.316 | 0.983 | 0.978–0.989 | 0.976, 0.991 |  |
| reset | std | G9 | 0.251 | 0.252 | 1.007 | 1.001–1.014 | 0.987, 1.027 |  |
| set.erase.fresh | std::flat_set | G9 | 1.092 | 1.093 | 1.001 | 0.996–1.006 | 1.000, 1.002 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.755 | 1.001 | 0.995–1.006 | 1.003, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 0.999 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| str.append | std::string | G9 | 0.426 | 0.436 | 1.024 | 1.016–1.031 | 1.004, 1.044 | worse |
| str.assign_long | std::string | G9 | 0.867 | 0.865 | 0.997 | 0.995–1.000 | 0.994, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| vec.iterate | std::vector | G9 | 0.999 | 1.001 | 1.002 | 0.996–1.007 | 1.000, 1.004 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.018 | 0.999 | 0.997–1.001 | 1.005, 0.993 |  |
| vec.mid_insert | std::vector | G9 | 0.993 | 0.959 | 0.966 | 0.965–0.968 | 0.966, 0.967 |  |
| vec.push_back | std::vector | G9 | 1.120 | 1.117 | 0.997 | 0.993–1.002 | 1.005, 0.990 |  |
| vec.push_back_grow | std::vector | G9 | 0.763 | 0.772 | 1.011 | 1.006–1.017 | 0.999, 1.024 | worse? |
| hero.mid_erase.plain | std::vector | report | 4.068 | 4.062 | 0.999 | 0.996–1.001 | 0.994, 1.003 |  |
| hero.mid_insert.plain | std::vector | report | 4.167 | 4.174 | 1.002 | 0.998–1.006 | 1.005, 0.998 |  |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.403 | 1.000 | 0.998–1.003 | 1.007, 0.994 |  |
| map.erase.fresh | std::map | report | 0.745 | 0.737 | 0.989 | 0.985–0.994 | 0.991, 0.987 |  |
| map.erase.reused | std::flat_map | report | 1.082 | 1.085 | 1.003 | 0.998–1.008 | 1.001, 1.005 |  |
| map.erase.reused | std::map | report | 1.615 | 1.663 | 1.030 | 1.022–1.038 | 1.035, 1.025 | worse |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.003 | 0.998–1.009 | 1.001, 1.005 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.804 | 0.996 | 0.992–0.999 | 0.999, 0.992 |  |
| map.find.reused | std::map | report | 0.716 | 0.718 | 1.002 | 0.996–1.007 | 1.000, 1.003 |  |
| map.insert.fresh | std::map | report | 0.739 | 0.741 | 1.002 | 0.997–1.007 | 1.009, 0.995 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 1.006 | 1.002 | 0.998–1.006 | 1.005, 0.999 |  |
| map.insert.reused | std::map | report | 1.827 | 1.762 | 0.965 | 0.959–0.970 | 0.943, 0.987 |  |
| nested.build.plain | std | report | 0.330 | 0.330 | 0.998 | 0.993–1.003 | 0.992, 1.005 |  |
| set.erase.fresh | std::set | report | 0.676 | 0.672 | 0.995 | 0.990–1.001 | 0.986, 1.004 |  |
| set.erase.reused | std::flat_set | report | 1.144 | 1.139 | 0.996 | 0.991–1.001 | 0.989, 1.003 |  |
| set.erase.reused | std::set | report | 1.325 | 1.359 | 1.025 | 1.015–1.036 | 1.013, 1.038 | worse |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 1.001 | 0.995–1.007 | 1.003, 0.998 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 0.998 | 0.992–1.003 | 0.999, 0.997 |  |
| set.find.reused | std::set | report | 0.684 | 0.668 | 0.976 | 0.969–0.984 | 0.993, 0.959 |  |
| set.insert.fresh | std::set | report | 0.648 | 0.651 | 1.005 | 1.001–1.008 | 0.992, 1.018 |  |
| set.insert.reused | std::flat_set | report | 1.080 | 1.079 | 0.999 | 0.993–1.004 | 1.001, 0.997 |  |
| set.insert.reused | std::set | report | 1.422 | 1.363 | 0.959 | 0.948–0.969 | 0.932, 0.986 |  |
| str.assign_short | std::string | report | 1.760 | 1.765 | 1.003 | 0.996–1.011 | 0.997, 1.010 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.154 | 1.003 | 0.999–1.007 | 1.002, 1.004 |  |

worse at t1a than t0a (by the rule): str.append / std::string (G9) 1.024; map.erase.reused / std::map (report) 1.030; set.erase.reused / std::set (report) 1.025

above 1.01 but not worse by the rule: vec.push_back_grow / std::vector (G9) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.5 | 164.5 | 1.000 | 0.998–1.003 | 1.001, 0.999 |  |
| hero.mid_erase.plain | 911.4 | 910.6 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| hero.mid_insert.ord | 229.2 | 229.3 | 1.000 | 0.997–1.003 | 0.999, 1.002 |  |
| hero.mid_insert.plain | 1249 | 1249 | 1.000 | 0.997–1.002 | 1.006, 0.993 |  |
| hero.push_back_grow.ord | 31.86 | 31.63 | 0.993 | 0.990–0.995 | 0.998, 0.988 |  |
| hero.push_back_grow.plain | 35.35 | 35.25 | 0.997 | 0.995–0.999 | 1.008, 0.986 |  |
| map.erase.fresh | 32.99 | 33.25 | 1.008 | 1.005–1.011 | 1.001, 1.015 | worse |
| map.erase.reused | 29.63 | 30.18 | 1.018 | 1.014–1.023 | 1.003, 1.034 | worse |
| map.find.fresh | 5.289 | 5.345 | 1.011 | 1.006–1.015 | 1.000, 1.021 | worse? |
| map.find.reused | 5.303 | 5.362 | 1.011 | 1.008–1.014 | 0.999, 1.024 | worse? |
| map.insert.fresh | 32.36 | 32.46 | 1.003 | 1.000–1.007 | 0.999, 1.008 |  |
| map.insert.reused | 29.54 | 29.74 | 1.007 | 1.003–1.010 | 1.001, 1.013 | worse |
| nested.build.ord | 267.7 | 271.1 | 1.012 | 1.008–1.017 | 0.996, 1.029 | worse? |
| nested.build.plain | 275.7 | 278 | 1.009 | 1.004–1.013 | 0.994, 1.024 |  |
| reset | 29.56 | 30.33 | 1.026 | 1.020–1.032 | 0.985, 1.069 | worse? |
| set.erase.fresh | 29.75 | 30.05 | 1.010 | 1.005–1.015 | 1.000, 1.021 | worse? |
| set.erase.reused | 23.34 | 23.51 | 1.007 | 1.002–1.013 | 0.995, 1.019 |  |
| set.find.fresh | 4.883 | 4.958 | 1.015 | 1.009–1.022 | 1.001, 1.030 | worse |
| set.find.reused | 4.934 | 4.926 | 0.998 | 0.990–1.006 | 0.999, 0.998 |  |
| set.insert.fresh | 28.9 | 29.29 | 1.013 | 1.010–1.017 | 0.999, 1.028 | worse? |
| set.insert.reused | 22.83 | 23.19 | 1.015 | 1.010–1.021 | 1.000, 1.031 | worse |
| str.append | 1.576 | 1.573 | 0.998 | 0.991–1.006 | 0.989, 1.008 |  |
| str.assign_long | 2.949 | 2.945 | 0.999 | 0.996–1.001 | 1.002, 0.996 |  |
| str.assign_short | 0.9412 | 0.9361 | 0.995 | 0.990–1.000 | 1.004, 0.985 |  |
| str.assign_short.copy | 0.6253 | 0.6269 | 1.003 | 0.997–1.009 | 1.011, 0.994 |  |
| str.assign_short.view | 0.9432 | 0.9371 | 0.994 | 0.990–0.997 | 0.998, 0.989 |  |
| vec.iterate | 0.03356 | 0.03341 | 0.996 | 0.991–1.000 | 1.000, 0.991 |  |
| vec.mid_erase | 40.46 | 40.54 | 1.002 | 1.001–1.003 | 1.007, 0.997 |  |
| vec.mid_insert | 40.51 | 40.63 | 1.003 | 1.002–1.004 | 1.003, 1.003 | worse |
| vec.push_back | 0.2764 | 0.2722 | 0.985 | 0.980–0.989 | 0.989, 0.980 |  |
| vec.push_back_grow | 0.2721 | 0.2715 | 0.998 | 0.993–1.002 | 0.996, 0.999 |  |

XOffset slower at t1a than t0a (worse by the rule): map.erase.fresh 1.008; map.erase.reused 1.018; map.insert.reused 1.007; set.find.fresh 1.015; set.insert.reused 1.015; vec.mid_insert 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_insert.plain (XOffset ns/op) | 1.014 | 1.000 | 0.997–1.002 | not worse |
| hero.push_back_grow.ord (XOffset ns/op) | 1.019 | 0.993 | 0.990–0.995 | not worse |
| hero.push_back_grow.plain (XOffset ns/op) | 1.027 | 0.997 | 0.995–0.999 | not worse |
| map.erase.reused (XOffset ns/op) | 1.003 | 1.018 | 1.014–1.023 | still worse |
| str.append (XOffset ns/op) | 1.026 | 0.998 | 0.991–1.006 | not worse |
| str.assign_short (XOffset ns/op) | 1.020 | 0.995 | 0.990–1.000 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.020 | 1.003 | 0.997–1.009 | not worse |
| str.assign_short.view (XOffset ns/op) | 1.026 | 0.994 | 0.990–0.997 | not worse |
| vec.iterate (XOffset ns/op) | 1.012 | 0.996 | 0.991–1.000 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.014 | 0.994 | 0.992–0.997 | not worse |
| hero.push_back_grow.plain / std::vector (report) | 1.017 | 1.000 | 0.998–1.003 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.003 | 0.995 | 0.991–0.999 | not worse |
| map.insert.fresh / std::map (report) | 1.006 | 1.002 | 0.997–1.007 | not worse |
| set.find.reused / std::set (report) | 1.187 | 0.976 | 0.969–0.984 | not worse |
| str.append / std::string (G9) | 1.211 | 1.024 | 1.016–1.031 | still worse |
| str.assign_short.view / std::string (G9) | 1.004 | 0.999 | 0.997–1.002 | not worse |
| vec.push_back / std::vector (G9) | 1.008 | 0.997 | 0.993–1.002 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.57 | 163.58 | 1.000 | 224.52 | 1.001 |
| hero.mid_erase.plain | 912.55 | 903.96 | 0.991 | 224.20 | 0.998 |
| hero.mid_insert.ord | 228.34 | 227.17 | 0.995 | 300.38 | 0.999 |
| hero.mid_insert.plain | 1252.73 | 1264.66 | 1.010 | 298.23 | 1.001 |
| hero.push_back_grow.ord | 31.75 | 32.08 | 1.010 | 87.12 | 0.994 |
| hero.push_back_grow.plain | 35.05 | 35.70 | 1.018 | 87.13 | 0.997 |
| map.erase.fresh | 32.98 | 32.99 | 1.000 | 35.20 | 1.001 |
| map.erase.reused | 29.71 | 29.76 | 1.002 | 27.34 | 1.004 |
| map.find.fresh | 5.28 | 5.28 | 0.999 | 6.55 | 1.001 |
| map.find.reused | 5.29 | 5.30 | 1.002 | 6.55 | 1.001 |
| map.insert.fresh | 32.30 | 32.19 | 0.996 | 37.17 | 0.999 |
| map.insert.reused | 29.62 | 29.47 | 0.995 | 29.46 | 1.000 |
| nested.build.ord | 272.53 | 269.51 | 0.989 | 829.13 | 1.005 |
| nested.build.plain | 279.27 | 277.66 | 0.994 | 833.80 | 1.005 |
| reset | 30.50 | 30.65 | 1.005 | 123.64 | 1.018 |
| set.erase.fresh | 29.78 | 29.71 | 0.998 | 27.41 | 1.000 |
| set.erase.reused | 22.96 | 23.11 | 1.007 | 20.29 | 1.001 |
| set.find.fresh | 4.88 | 4.88 | 1.000 | 6.46 | 1.015 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 29.35 | 28.88 | 0.984 | 28.94 | 1.013 |
| set.insert.reused | 23.00 | 22.84 | 0.993 | 21.19 | 1.001 |
| str.append | 1.50 | 1.54 | 1.022 | 4.39 | 0.997 |
| str.assign_long | 2.93 | 2.74 | 0.934 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.004 | 0.52 | 1.013 |
| str.assign_short.copy | 0.60 | 0.63 | 1.049 | 0.52 | 0.989 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 0.999 |
| vec.iterate | 0.03 | 0.03 | 1.002 | 0.03 | 0.995 |
| vec.mid_erase | 39.98 | 40.04 | 1.002 | 39.56 | 0.999 |
| vec.mid_insert | 40.50 | 40.38 | 0.997 | 41.23 | 1.003 |
| vec.push_back | 0.29 | 0.29 | 1.003 | 0.26 | 0.998 |
| vec.push_back_grow | 0.28 | 0.28 | 0.991 | 0.36 | 1.000 |

## analyze t0 (G9 at the anchor cc5b28ee)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.123 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.778 |  0.782 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.30 |  0.984 |  0.986 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.55 |  1.012 |  1.014 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.727 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     297.53 |  0.763 |  0.765 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.70 |      87.04 |  0.364 |  0.365 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.47 |     224.61 |  4.072 |  4.085 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.11 |     304.03 |  4.207 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.99 |      86.79 |  0.403 |  0.403 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       4.40 |  0.344 |  0.345 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.19 |  0.869 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.13 |  0.744 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.742 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     29.30 |      28.97 |  1.014 |  1.016 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     29.30 |      43.95 |  0.666 |  0.668 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.32 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.088 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.671 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.46 |  1.006 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.36 |  1.805 |  1.816 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.21 |  0.743 |  0.747 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.70 |      27.34 |  1.084 |  1.086 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.70 |      17.58 |  1.655 |  1.667 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.084 |  1.088 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.62 |  1.455 |  1.466 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.28 |  0.662 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.139 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.76 |  1.342 |  1.353 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.16 |     830.08 |  0.329 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.51 |  0.247 |  0.250 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     833.34 |  0.335 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.127 |  1.132 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.960 |  0.965 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.79 |  1.012 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.27 |  0.725 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     297.53 |  0.755 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.10 |      86.55 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    898.44 |     224.61 |  4.000 |  4.022 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.86 |     304.69 |  4.201 |  4.224 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.60 |      87.16 |  0.410 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.803 |  0.807 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.346 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.53 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.67 |  0.417 |  0.419 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      42.89 |  0.748 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.52 |  0.320 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.939 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.994 |  0.996 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.95 |  0.656 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.31 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.26 |  1.090 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.19 |  0.670 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.005 |  1.010 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.796 |  1.810 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.77 |  0.679 |  0.682 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.79 |      27.34 |  1.085 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.79 |      17.66 |  1.661 |  1.671 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.082 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.54 |  1.411 |  1.424 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.20 |  0.786 |  0.789 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.43 |  1.136 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.345 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    268.25 |     832.03 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.19 |  0.252 |  0.255 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     826.81 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-03T16:28:19Z block 1 start: waited 180s load 2.03 WindowServer 4% remote 4
2026-11-03T16:31:24Z block 2 start: waited 180s load 1.67 WindowServer 4% remote 4
2026-11-03T16:34:28Z block 3 start: waited 180s load 1.72 WindowServer 4% remote 4
2026-11-03T16:37:33Z block 4 start: waited 180s load 1.85 WindowServer 4.4% remote 4
```
