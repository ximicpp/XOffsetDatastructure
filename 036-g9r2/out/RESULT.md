# G9R2 result (2026-10-27T07:50:59Z)

probe built against t1 30af216f6063316fb3348188f1570e53ecf26468; reloc t0 35492720b5981d9672c8b8f00037dcb687aaf9fd (next_cpp26 3549272 (G8 checkpoint)), t1 (next_cpp26 30af216 (L5 #125, L1 #126, L2 #127, L3 #128, L4 #129; also #120, #121)); bench/reloc of t1 (aabaa1ed859747bc5237fe471e8107c6f92c642d) for both.

## g9r2_probe

```
g9r2_probe: M 2048, K 512, 101 rounds (+3 warm-up)
placement (address % 4096 / % 64): xo head 48/48 data 64/0 cap 4096; std data 0/0 cap 4096; raw.stdpos data 0/0; std.xopos data 64/0
ins  xo                     54.93 ns/op (median of 101 rounds)
ins  raw                    50.29 ns/op (median of 101 rounds)
ins  raw.size_first         49.07 ns/op (median of 101 rounds)
ins  raw.ldr                49.97 ns/op (median of 101 rounds)
ins  raw.stdpos             50.38 ns/op (median of 101 rounds)
ins  std                    50.21 ns/op (median of 101 rounds)
ins  std.xopos              49.32 ns/op (median of 101 rounds)
era  xo                     44.27 ns/op (median of 101 rounds)
era  raw                    42.81 ns/op (median of 101 rounds)
era  raw.size_first         42.56 ns/op (median of 101 rounds)
era  raw.ldr                42.56 ns/op (median of 101 rounds)
era  raw.noscrub            43.21 ns/op (median of 101 rounds)
era  raw.stdpos             42.81 ns/op (median of 101 rounds)
era  std                    42.72 ns/op (median of 101 rounds)
era  std.xopos              42.97 ns/op (median of 101 rounds)
checks: all passed

g9r2_probe: M 2048, K 512, 101 rounds (+3 warm-up)
placement (address % 4096 / % 64): xo head 48/48 data 64/0 cap 4096; std data 0/0 cap 4096; raw.stdpos data 0/0; std.xopos data 64/0
ins  xo                     54.77 ns/op (median of 101 rounds)
ins  raw                    50.38 ns/op (median of 101 rounds)
ins  raw.size_first         48.91 ns/op (median of 101 rounds)
ins  raw.ldr                49.80 ns/op (median of 101 rounds)
ins  raw.stdpos             50.29 ns/op (median of 101 rounds)
ins  std                    49.97 ns/op (median of 101 rounds)
ins  std.xopos              49.40 ns/op (median of 101 rounds)
era  xo                     44.03 ns/op (median of 101 rounds)
era  raw                    42.89 ns/op (median of 101 rounds)
era  raw.size_first         42.72 ns/op (median of 101 rounds)
era  raw.ldr                42.56 ns/op (median of 101 rounds)
era  raw.noscrub            43.05 ns/op (median of 101 rounds)
era  raw.stdpos             42.81 ns/op (median of 101 rounds)
era  std                    42.89 ns/op (median of 101 rounds)
era  std.xopos              43.05 ns/op (median of 101 rounds)
checks: all passed
```

# g9r2_probe pairs

## ins (probe_1, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.93 |
| raw | 50.29 |
| raw.size_first | 49.07 |
| raw.ldr | 49.97 |
| raw.stdpos | 50.38 |
| std | 50.21 |
| std.xopos | 49.32 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.105 | 1.091–1.118 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.077–1.111 | +4.48 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.027 | 1.009–1.045 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.002 | 0.979–1.025 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.984–1.022 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.980 | 0.978–0.982 | -0.90 |
| raw / std: raw / std (what is left after container code) | 1.010 | 0.996–1.024 | +0.08 |

## era (probe_1, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.27 |
| raw | 42.81 |
| raw.size_first | 42.56 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.21 |
| raw.stdpos | 42.81 |
| std | 42.72 |
| std.xopos | 42.97 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.042 | 1.037–1.046 | +1.63 |
| xo / raw: container code: library vs raw (L10) | 1.035 | 1.028–1.043 | +1.38 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 0.990 | 0.962–1.019 | +0.24 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.012 | 1.008–1.015 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.991 | 0.983–0.998 | -0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.004 | 0.996–1.012 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.006 | 0.999–1.014 | +0.16 |
| raw / std: raw / std (what is left after container code) | 1.006 | 1.001–1.012 | +0.16 |

## ins (probe_2, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.77 |
| raw | 50.38 |
| raw.size_first | 48.91 |
| raw.ldr | 49.80 |
| raw.stdpos | 50.29 |
| std | 49.97 |
| std.xopos | 49.40 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.097 | 1.073–1.121 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.070–1.119 | +4.23 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.025 | 1.022–1.029 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.005 | 1.001–1.010 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.000 | 0.997–1.004 | +0.08 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.990 | 0.983–0.996 | -0.65 |
| raw / std: raw / std (what is left after container code) | 1.002 | 0.998–1.007 | +0.24 |

## era (probe_2, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.03 |
| raw | 42.89 |
| raw.size_first | 42.72 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.05 |
| raw.stdpos | 42.81 |
| std | 42.89 |
| std.xopos | 43.05 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.024 | 1.018–1.031 | +1.14 |
| xo / raw: container code: library vs raw (L10) | 1.025 | 1.019–1.030 | +1.22 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.005 | 0.998–1.011 | +0.16 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.006 | 0.997–1.015 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.996 | 0.987–1.004 | -0.16 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.993–1.012 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.008 | 0.996–1.019 | +0.08 |
| raw / std: raw / std (what is left after container code) | 1.000 | 0.991–1.009 | -0.08 |

## ins (pooled, 202 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.93 |
| raw | 50.33 |
| raw.size_first | 48.99 |
| raw.ldr | 49.89 |
| raw.stdpos | 50.37 |
| std | 50.13 |
| std.xopos | 49.32 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.101 | 1.087–1.115 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.079–1.109 | +4.39 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.026 | 1.017–1.035 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.004 | 0.992–1.015 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.002 | 0.992–1.011 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.985 | 0.981–0.988 | -0.81 |
| raw / std: raw / std (what is left after container code) | 1.006 | 0.999–1.013 | +0.16 |

## era (pooled, 202 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.27 |
| raw | 42.89 |
| raw.size_first | 42.72 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.21 |
| raw.stdpos | 42.81 |
| std | 42.89 |
| std.xopos | 42.97 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.033 | 1.029–1.037 | +1.22 |
| xo / raw: container code: library vs raw (L10) | 1.030 | 1.025–1.035 | +1.30 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 0.997 | 0.983–1.012 | +0.24 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.009 | 1.004–1.013 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.993 | 0.987–0.999 | -0.24 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.997–1.010 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.007 | 1.000–1.014 | +0.08 |
| raw / std: raw / std (what is left after container code) | 1.003 | 0.998–1.008 | +0.00 |


## G9 items, t1 / t0 (second batch after G9R) and the aligned pair t1a / t0a

## XOffset / baseline, t1 / t0 (default build, second batch after G9R)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 1.190 | 0.755 | 0.635 | 0.620–0.650 | 0.643, 0.627 |  |
| hero.mid_insert.ord | std::vector | G9 | 1.219 | 0.804 | 0.660 | 0.649–0.670 | 0.656, 0.663 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.442 | 0.432 | 0.978 | 0.963–0.993 | 0.974, 0.982 |  |
| map.erase.fresh | std::flat_map | G9 | 0.964 | 0.970 | 1.006 | 0.991–1.021 | 0.989, 1.023 |  |
| map.find.fresh | std::flat_map | G9 | 0.777 | 0.773 | 0.995 | 0.975–1.016 | 0.982, 1.009 |  |
| map.insert.fresh | std::flat_map | G9 | 0.961 | 0.955 | 0.994 | 0.979–1.009 | 0.991, 0.997 |  |
| nested.build.ord | std | G9 | 0.457 | 0.417 | 0.914 | 0.902–0.926 | 0.903, 0.925 |  |
| reset | std | G9 | 0.211 | 0.200 | 0.947 | 0.935–0.960 | 0.984, 0.912 |  |
| set.erase.fresh | std::flat_set | G9 | 1.127 | 1.139 | 1.010 | 0.999–1.021 | 1.016, 1.004 |  |
| set.find.fresh | std::flat_set | G9 | 0.761 | 0.762 | 1.001 | 0.984–1.019 | 1.007, 0.996 |  |
| set.insert.fresh | std::flat_set | G9 | 1.118 | 1.119 | 1.001 | 0.993–1.009 | 0.993, 1.009 |  |
| str.append | std::string | G9 | 0.350 | 0.437 | 1.248 | 1.234–1.262 | 1.215, 1.282 | worse |
| str.assign_long | std::string | G9 | 0.940 | 0.855 | 0.910 | 0.894–0.925 | 0.913, 0.906 |  |
| str.assign_short.view | std::string | G9 | 0.473 | 0.364 | 0.769 | 0.754–0.783 | 0.771, 0.767 |  |
| vec.iterate | std::vector | G9 | 1.000 | 0.993 | 0.993 | 0.974–1.013 | 1.002, 0.984 |  |
| vec.mid_erase | std::vector | G9 | 1.054 | 1.027 | 0.974 | 0.963–0.985 | 0.958, 0.990 |  |
| vec.mid_insert | std::vector | G9 | 1.096 | 1.101 | 1.004 | 0.990–1.018 | 1.006, 1.002 |  |
| vec.push_back | std::vector | G9 | 6.565 | 1.290 | 0.197 | 0.193–0.200 | 0.197, 0.196 |  |
| vec.push_back_grow | std::vector | G9 | 4.500 | 0.778 | 0.173 | 0.169–0.177 | 0.172, 0.174 |  |
| hero.mid_erase.plain | std::vector | report | 4.559 | 4.549 | 0.998 | 0.983–1.012 | 1.009, 0.987 |  |
| hero.mid_insert.plain | std::vector | report | 4.637 | 4.689 | 1.011 | 0.994–1.029 | 1.015, 1.008 | worse? |
| hero.push_back_grow.plain | std::vector | report | 0.474 | 0.464 | 0.979 | 0.966–0.992 | 0.974, 0.984 |  |
| map.erase.fresh | std::map | report | 0.712 | 0.725 | 1.017 | 1.008–1.027 | 1.020, 1.015 | worse |
| map.erase.reused | std::flat_map | report | 1.103 | 1.089 | 0.987 | 0.981–0.994 | 0.998, 0.977 |  |
| map.erase.reused | std::map | report | 1.459 | 1.312 | 0.899 | 0.878–0.920 | 0.951, 0.849 |  |
| map.find.fresh | std::map | report | 0.305 | 0.315 | 1.031 | 1.017–1.046 | 1.031, 1.031 | worse |
| map.find.reused | std::flat_map | report | 0.784 | 0.783 | 0.998 | 0.980–1.017 | 0.983, 1.014 |  |
| map.find.reused | std::map | report | 0.785 | 0.715 | 0.910 | 0.899–0.922 | 0.911, 0.910 |  |
| map.insert.fresh | std::map | report | 0.812 | 0.815 | 1.004 | 0.997–1.010 | 0.992, 1.016 |  |
| map.insert.reused | std::flat_map | report | 1.113 | 1.123 | 1.009 | 0.990–1.028 | 1.005, 1.013 |  |
| map.insert.reused | std::map | report | 1.799 | 1.796 | 0.998 | 0.965–1.033 | 1.047, 0.951 |  |
| nested.build.plain | std | report | 0.461 | 0.455 | 0.988 | 0.979–0.997 | 0.972, 1.004 |  |
| set.erase.fresh | std::set | report | 0.659 | 0.655 | 0.994 | 0.983–1.005 | 0.980, 1.008 |  |
| set.erase.reused | std::flat_set | report | 1.130 | 1.137 | 1.006 | 0.987–1.025 | 1.001, 1.012 |  |
| set.erase.reused | std::set | report | 1.014 | 1.197 | 1.181 | 1.152–1.210 | 1.194, 1.167 | worse |
| set.find.fresh | std::set | report | 0.298 | 0.303 | 1.014 | 0.991–1.037 | 1.028, 1.000 | worse? |
| set.find.reused | std::flat_set | report | 0.751 | 0.759 | 1.010 | 0.997–1.023 | 1.005, 1.015 |  |
| set.find.reused | std::set | report | 0.715 | 0.686 | 0.958 | 0.939–0.978 | 0.932, 0.985 |  |
| set.insert.fresh | std::set | report | 0.714 | 0.719 | 1.007 | 0.995–1.018 | 1.009, 1.005 |  |
| set.insert.reused | std::flat_set | report | 1.276 | 1.269 | 0.994 | 0.985–1.003 | 0.991, 0.997 |  |
| set.insert.reused | std::set | report | 1.520 | 1.492 | 0.982 | 0.956–1.008 | 0.961, 1.003 |  |
| str.assign_short | std::string | report | 2.538 | 1.959 | 0.772 | 0.755–0.789 | 0.779, 0.764 |  |
| str.assign_short.copy | std::string | report | 3.142 | 1.601 | 0.510 | 0.508–0.512 | 0.512, 0.507 |  |

worse at t1 than t0 (by the rule): str.append / std::string (G9) 1.248; map.erase.fresh / std::map (report) 1.017; map.find.fresh / std::map (report) 1.031; set.erase.reused / std::set (report) 1.181

above 1.01 but not worse by the rule: hero.mid_insert.plain / std::vector (report) 1.011; set.find.fresh / std::set (report) 1.014

## XOffset ns/op alone, t1 / t0 (default build, second batch after G9R)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 276.4 | 173.6 | 0.628 | 0.617–0.639 | 0.627, 0.629 |  |
| hero.mid_erase.plain | 1036 | 1043 | 1.007 | 0.996–1.019 | 1.005, 1.009 |  |
| hero.mid_insert.ord | 367.1 | 244.2 | 0.665 | 0.661–0.670 | 0.669, 0.662 |  |
| hero.mid_insert.plain | 1407 | 1418 | 1.008 | 0.995–1.021 | 1.010, 1.006 |  |
| hero.push_back_grow.ord | 40.28 | 38.32 | 0.951 | 0.938–0.965 | 0.945, 0.958 |  |
| hero.push_back_grow.plain | 43.3 | 42.87 | 0.990 | 0.979–1.001 | 0.990, 0.991 |  |
| map.erase.fresh | 34.15 | 34.28 | 1.004 | 0.992–1.015 | 1.001, 1.007 |  |
| map.erase.reused | 30.16 | 30.33 | 1.006 | 0.995–1.017 | 0.992, 1.020 |  |
| map.find.fresh | 5.168 | 5.142 | 0.995 | 0.987–1.003 | 0.994, 0.997 |  |
| map.find.reused | 5.198 | 5.184 | 0.997 | 0.987–1.008 | 0.997, 0.997 |  |
| map.insert.fresh | 35.54 | 35.8 | 1.007 | 1.001–1.014 | 1.006, 1.009 | worse |
| map.insert.reused | 33.66 | 33.63 | 0.999 | 0.989–1.009 | 1.007, 0.992 |  |
| nested.build.ord | 388.6 | 377.1 | 0.970 | 0.961–0.979 | 0.968, 0.972 |  |
| nested.build.plain | 390.8 | 387.6 | 0.992 | 0.984–1.000 | 0.997, 0.987 |  |
| reset | 33.88 | 33.53 | 0.990 | 0.977–1.003 | 1.031, 0.950 |  |
| set.erase.fresh | 30.91 | 31.08 | 1.006 | 0.994–1.017 | 1.003, 1.008 |  |
| set.erase.reused | 23.49 | 23.34 | 0.994 | 0.981–1.007 | 1.003, 0.984 |  |
| set.find.fresh | 4.953 | 4.956 | 1.001 | 0.981–1.020 | 1.013, 0.988 |  |
| set.find.reused | 4.893 | 4.931 | 1.008 | 0.999–1.017 | 1.004, 1.012 |  |
| set.insert.fresh | 32.39 | 32.44 | 1.002 | 0.995–1.008 | 0.994, 1.009 |  |
| set.insert.reused | 26.93 | 27.24 | 1.012 | 1.003–1.020 | 1.025, 0.999 | worse? |
| str.append | 1.529 | 1.551 | 1.014 | 1.002–1.026 | 0.997, 1.032 | worse? |
| str.assign_long | 3.222 | 2.969 | 0.921 | 0.909–0.934 | 0.903, 0.941 |  |
| str.assign_short | 1.279 | 1.023 | 0.800 | 0.781–0.820 | 0.824, 0.778 |  |
| str.assign_short.copy | 1.588 | 0.8408 | 0.530 | 0.525–0.534 | 0.524, 0.535 |  |
| str.assign_short.view | 1.305 | 0.9998 | 0.766 | 0.751–0.782 | 0.773, 0.759 |  |
| vec.iterate | 0.03344 | 0.0366 | 1.095 | 1.077–1.113 | 1.061, 1.129 | worse |
| vec.mid_erase | 41.93 | 41.89 | 0.999 | 0.991–1.007 | 1.007, 0.991 |  |
| vec.mid_insert | 45.76 | 47.78 | 1.044 | 1.034–1.055 | 1.060, 1.028 | worse |
| vec.push_back | 2.088 | 0.4126 | 0.198 | 0.196–0.199 | 0.202, 0.193 |  |
| vec.push_back_grow | 1.815 | 0.3266 | 0.180 | 0.177–0.183 | 0.182, 0.178 |  |

XOffset slower at t1 than t0 (worse by the rule): map.insert.fresh 1.007; vec.iterate 1.095; vec.mid_insert 1.044

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 1.194 | 0.758 | 0.635 | 0.631–0.639 | 0.638, 0.631 |  |
| hero.mid_insert.ord | std::vector | G9 | 1.226 | 0.803 | 0.655 | 0.647–0.664 | 0.662, 0.649 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.433 | 0.425 | 0.982 | 0.972–0.992 | 0.981, 0.982 |  |
| map.erase.fresh | std::flat_map | G9 | 0.973 | 0.964 | 0.990 | 0.984–0.997 | 0.988, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.782 | 0.782 | 1.000 | 0.992–1.008 | 0.999, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.957 | 0.966 | 1.010 | 1.001–1.019 | 1.017, 1.003 | worse |
| nested.build.ord | std | G9 | 0.452 | 0.438 | 0.969 | 0.960–0.978 | 0.961, 0.977 |  |
| reset | std | G9 | 0.206 | 0.210 | 1.021 | 1.011–1.031 | 1.019, 1.022 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.117 | 1.126 | 1.007 | 1.000–1.015 | 1.013, 1.002 |  |
| set.find.fresh | std::flat_set | G9 | 0.751 | 0.758 | 1.009 | 1.000–1.018 | 1.010, 1.008 | worse |
| set.insert.fresh | std::flat_set | G9 | 1.114 | 1.114 | 1.000 | 0.990–1.010 | 1.000, 1.000 |  |
| str.append | std::string | G9 | 0.435 | 0.432 | 0.995 | 0.984–1.005 | 1.034, 0.957 |  |
| str.assign_long | std::string | G9 | 0.976 | 0.861 | 0.883 | 0.876–0.889 | 0.888, 0.877 |  |
| str.assign_short.view | std::string | G9 | 0.472 | 0.364 | 0.773 | 0.770–0.776 | 0.773, 0.772 |  |
| vec.iterate | std::vector | G9 | 1.012 | 1.001 | 0.989 | 0.975–1.003 | 0.979, 0.998 |  |
| vec.mid_erase | std::vector | G9 | 1.057 | 1.047 | 0.991 | 0.984–0.997 | 0.995, 0.986 |  |
| vec.mid_insert | std::vector | G9 | 1.089 | 1.087 | 0.998 | 0.987–1.009 | 1.007, 0.989 |  |
| vec.push_back | std::vector | G9 | 6.463 | 1.194 | 0.185 | 0.182–0.188 | 0.185, 0.184 |  |
| vec.push_back_grow | std::vector | G9 | 4.506 | 0.788 | 0.175 | 0.174–0.176 | 0.174, 0.176 |  |
| hero.mid_erase.plain | std::vector | report | 4.586 | 4.543 | 0.991 | 0.983–0.998 | 0.992, 0.990 |  |
| hero.mid_insert.plain | std::vector | report | 4.672 | 4.619 | 0.989 | 0.980–0.998 | 0.985, 0.993 |  |
| hero.push_back_grow.plain | std::vector | report | 0.482 | 0.475 | 0.986 | 0.977–0.996 | 0.963, 1.010 |  |
| map.erase.fresh | std::map | report | 0.722 | 0.717 | 0.993 | 0.985–1.002 | 0.982, 1.004 |  |
| map.erase.reused | std::flat_map | report | 1.089 | 1.093 | 1.003 | 0.991–1.016 | 1.006, 1.001 |  |
| map.erase.reused | std::map | report | 1.351 | 1.352 | 1.001 | 0.983–1.019 | 1.027, 0.976 |  |
| map.find.fresh | std::map | report | 0.313 | 0.314 | 1.004 | 0.994–1.013 | 1.000, 1.007 |  |
| map.find.reused | std::flat_map | report | 0.784 | 0.785 | 1.001 | 0.992–1.011 | 0.990, 1.012 |  |
| map.find.reused | std::map | report | 0.727 | 0.715 | 0.983 | 0.976–0.990 | 0.954, 1.013 |  |
| map.insert.fresh | std::map | report | 0.813 | 0.819 | 1.008 | 0.999–1.016 | 1.007, 1.008 |  |
| map.insert.reused | std::flat_map | report | 1.134 | 1.133 | 0.999 | 0.992–1.005 | 1.003, 0.994 |  |
| map.insert.reused | std::map | report | 1.887 | 1.919 | 1.017 | 1.005–1.029 | 1.019, 1.015 | worse |
| nested.build.plain | std | report | 0.452 | 0.450 | 0.997 | 0.987–1.007 | 1.002, 0.992 |  |
| set.erase.fresh | std::set | report | 0.652 | 0.656 | 1.007 | 0.999–1.015 | 1.009, 1.005 |  |
| set.erase.reused | std::flat_set | report | 1.146 | 1.155 | 1.007 | 1.002–1.013 | 1.009, 1.006 | worse |
| set.erase.reused | std::set | report | 1.116 | 1.124 | 1.007 | 0.994–1.020 | 0.985, 1.029 |  |
| set.find.fresh | std::set | report | 0.298 | 0.300 | 1.007 | 0.997–1.017 | 1.011, 1.003 |  |
| set.find.reused | std::flat_set | report | 0.753 | 0.762 | 1.011 | 1.001–1.022 | 1.018, 1.005 | worse |
| set.find.reused | std::set | report | 0.685 | 0.693 | 1.012 | 1.002–1.023 | 1.020, 1.005 | worse |
| set.insert.fresh | std::set | report | 0.715 | 0.714 | 0.998 | 0.992–1.003 | 0.996, 0.999 |  |
| set.insert.reused | std::flat_set | report | 1.262 | 1.271 | 1.007 | 1.002–1.012 | 1.009, 1.004 | worse |
| set.insert.reused | std::set | report | 1.519 | 1.530 | 1.007 | 0.990–1.025 | 0.972, 1.043 |  |
| str.assign_short | std::string | report | 2.533 | 1.909 | 0.754 | 0.745–0.762 | 0.752, 0.755 |  |
| str.assign_short.copy | std::string | report | 3.188 | 1.600 | 0.502 | 0.495–0.508 | 0.499, 0.505 |  |

worse at t1a than t0a (by the rule): map.insert.fresh / std::flat_map (G9) 1.010; reset / std (G9) 1.021; set.find.fresh / std::flat_set (G9) 1.009; map.insert.reused / std::map (report) 1.017; set.erase.reused / std::flat_set (report) 1.007; set.find.reused / std::flat_set (report) 1.011; set.find.reused / std::set (report) 1.012; set.insert.reused / std::flat_set (report) 1.007

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 273.9 | 173.4 | 0.633 | 0.630–0.636 | 0.637, 0.630 |  |
| hero.mid_erase.plain | 1044 | 1041 | 0.997 | 0.989–1.006 | 0.993, 1.002 |  |
| hero.mid_insert.ord | 374 | 247.4 | 0.661 | 0.656–0.667 | 0.667, 0.656 |  |
| hero.mid_insert.plain | 1409 | 1397 | 0.992 | 0.986–0.997 | 0.987, 0.996 |  |
| hero.push_back_grow.ord | 39.44 | 38.35 | 0.972 | 0.963–0.982 | 0.973, 0.972 |  |
| hero.push_back_grow.plain | 43.27 | 42.79 | 0.989 | 0.981–0.996 | 0.986, 0.992 |  |
| map.erase.fresh | 34.21 | 34.13 | 0.998 | 0.992–1.004 | 0.995, 1.001 |  |
| map.erase.reused | 30.43 | 30.29 | 0.995 | 0.988–1.003 | 1.000, 0.990 |  |
| map.find.fresh | 5.139 | 5.156 | 1.003 | 0.999–1.008 | 1.001, 1.006 |  |
| map.find.reused | 5.187 | 5.188 | 1.000 | 0.993–1.008 | 0.995, 1.005 |  |
| map.insert.fresh | 35.7 | 35.81 | 1.003 | 0.997–1.009 | 1.007, 0.999 |  |
| map.insert.reused | 33.4 | 33.3 | 0.997 | 0.992–1.002 | 0.993, 1.001 |  |
| nested.build.ord | 386.4 | 373.8 | 0.967 | 0.960–0.974 | 0.955, 0.980 |  |
| nested.build.plain | 390 | 387.3 | 0.993 | 0.982–1.005 | 0.995, 0.991 |  |
| reset | 33.73 | 34.51 | 1.023 | 1.015–1.031 | 1.024, 1.022 | worse |
| set.erase.fresh | 30.74 | 30.8 | 1.002 | 0.998–1.007 | 1.004, 1.001 |  |
| set.erase.reused | 23.37 | 23.48 | 1.005 | 0.996–1.013 | 1.011, 0.998 |  |
| set.find.fresh | 4.885 | 4.925 | 1.008 | 1.001–1.016 | 1.011, 1.006 | worse |
| set.find.reused | 4.909 | 4.929 | 1.004 | 0.994–1.014 | 1.010, 0.997 |  |
| set.insert.fresh | 32.45 | 32.46 | 1.000 | 0.996–1.005 | 1.003, 0.998 |  |
| set.insert.reused | 26.84 | 26.86 | 1.001 | 0.996–1.006 | 1.007, 0.995 |  |
| str.append | 1.548 | 1.568 | 1.013 | 1.003–1.023 | 1.034, 0.993 | worse? |
| str.assign_long | 3.336 | 2.953 | 0.885 | 0.880–0.890 | 0.884, 0.887 |  |
| str.assign_short | 1.284 | 0.9795 | 0.763 | 0.759–0.767 | 0.770, 0.756 |  |
| str.assign_short.copy | 1.641 | 0.8248 | 0.503 | 0.495–0.510 | 0.498, 0.507 |  |
| str.assign_short.view | 1.295 | 0.9901 | 0.764 | 0.760–0.768 | 0.775, 0.753 |  |
| vec.iterate | 0.03405 | 0.03321 | 0.975 | 0.960–0.990 | 0.953, 0.997 |  |
| vec.mid_erase | 41.97 | 41.97 | 1.000 | 0.995–1.005 | 1.008, 0.992 |  |
| vec.mid_insert | 44.67 | 44.74 | 1.002 | 0.993–1.010 | 1.011, 0.993 |  |
| vec.push_back | 1.714 | 0.2947 | 0.172 | 0.169–0.175 | 0.160, 0.185 |  |
| vec.push_back_grow | 1.668 | 0.2825 | 0.169 | 0.168–0.171 | 0.164, 0.175 |  |

XOffset slower at t1a than t0a (worse by the rule): reset 1.023; set.find.fresh 1.008

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.insert.fresh (XOffset ns/op) | 1.007 | 1.003 | 0.997–1.009 | not worse |
| vec.iterate (XOffset ns/op) | 1.095 | 0.975 | 0.960–0.990 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.044 | 1.002 | 0.993–1.010 | not worse |
| map.erase.fresh / std::map (report) | 1.017 | 0.993 | 0.985–1.002 | not worse |
| map.find.fresh / std::map (report) | 1.031 | 1.004 | 0.994–1.013 | not worse |
| set.erase.reused / std::set (report) | 1.181 | 1.007 | 0.994–1.020 | not worse |
| str.append / std::string (G9) | 1.248 | 0.995 | 0.984–1.005 | not worse |

## reloc t1 / t0 (median per file, GM of 2 files; aligned pair GM of 4 files)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 270.19 | 171.88 | 0.636 | 228.84 | 0.633 |
| hero.mid_erase.plain | 1026.37 | 1036.77 | 1.010 | 227.54 | 0.995 |
| hero.mid_insert.ord | 366.54 | 242.84 | 0.663 | 306.64 | 0.665 |
| hero.mid_insert.plain | 1388.48 | 1400.54 | 1.009 | 299.74 | 0.987 |
| hero.push_back_grow.ord | 39.32 | 38.03 | 0.967 | 89.92 | 0.972 |
| hero.push_back_grow.plain | 42.83 | 42.56 | 0.994 | 91.06 | 0.987 |
| map.erase.fresh | 33.86 | 34.02 | 1.005 | 35.13 | 1.001 |
| map.erase.reused | 30.11 | 29.95 | 0.995 | 27.26 | 1.001 |
| map.find.fresh | 5.14 | 5.14 | 1.000 | 6.55 | 1.000 |
| map.find.reused | 5.15 | 5.15 | 1.000 | 6.55 | 0.996 |
| map.insert.fresh | 35.48 | 35.52 | 1.001 | 36.91 | 1.000 |
| map.insert.reused | 33.22 | 33.24 | 1.001 | 29.38 | 0.999 |
| nested.build.ord | 389.01 | 372.40 | 0.957 | 850.49 | 0.976 |
| nested.build.plain | 390.29 | 384.43 | 0.985 | 853.50 | 1.007 |
| reset | 33.52 | 33.20 | 0.991 | 158.85 | 1.015 |
| set.erase.fresh | 30.68 | 30.68 | 1.000 | 27.30 | 1.003 |
| set.erase.reused | 23.11 | 23.11 | 1.000 | 20.30 | 0.991 |
| set.find.fresh | 4.88 | 4.87 | 0.998 | 6.45 | 1.002 |
| set.find.reused | 4.88 | 4.88 | 1.001 | 6.45 | 1.002 |
| set.insert.fresh | 32.31 | 32.23 | 0.997 | 28.93 | 1.001 |
| set.insert.reused | 26.94 | 26.85 | 0.997 | 21.08 | 1.000 |
| str.append | 1.50 | 1.50 | 1.002 | 4.33 | 1.001 |
| str.assign_long | 3.18 | 2.93 | 0.923 | 3.40 | 0.891 |
| str.assign_short | 1.28 | 0.97 | 0.757 | 0.50 | 0.757 |
| str.assign_short.copy | 1.59 | 0.84 | 0.531 | 0.50 | 0.507 |
| str.assign_short.view | 1.28 | 0.99 | 0.771 | 2.71 | 0.770 |
| vec.iterate | 0.03 | 0.04 | 1.076 | 0.03 | 0.995 |
| vec.mid_erase | 42.07 | 40.97 | 0.974 | 39.67 | 0.987 |
| vec.mid_insert | 45.08 | 48.66 | 1.079 | 40.69 | 1.007 |
| vec.push_back | 2.06 | 0.41 | 0.201 | 0.32 | 0.170 |
| vec.push_back_grow | 1.80 | 0.32 | 0.179 | 0.40 | 0.170 |

## analyze t0

```
files: 2  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  202 |      2.06 |       0.31 |  6.565 |  6.639 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  202 |      1.80 |       0.40 |  4.500 |  4.542 | fail    |
| vec.iterate                  | std::vector    | G9     |  202 |      0.03 |       0.03 |  1.000 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  202 |     45.00 |      40.69 |  1.096 |  1.118 | fail    |
| vec.mid_erase                | std::vector    | G9     |  202 |     42.16 |      39.63 |  1.054 |  1.060 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  202 |    270.19 |     228.52 |  1.190 |  1.221 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  202 |    366.53 |     306.64 |  1.219 |  1.226 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  202 |     39.35 |      89.40 |  0.442 |  0.450 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  202 |   1025.39 |     227.88 |  4.559 |  4.608 | report  |
| hero.mid_insert.plain        | std::vector    | report |  202 |   1388.67 |     305.34 |  4.637 |  4.735 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  202 |     42.83 |      91.31 |  0.474 |  0.480 | report  |
| str.assign_short             | std::string    | report |  202 |      1.28 |       0.50 |  2.538 |  2.541 | report  |
| str.assign_long              | std::string    | G9     |  202 |      3.18 |       3.40 |  0.940 |  0.955 | pass    |
| str.assign_short.view        | std::string    | G9     |  202 |      1.28 |       2.71 |  0.473 |  0.486 | pass    |
| str.assign_short.copy        | std::string    | report |  202 |      1.59 |       0.50 |  3.142 |  3.145 | report  |
| str.append                   | std::string    | G9     |  202 |      1.50 |       4.35 |  0.350 |  0.353 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  202 |     35.48 |      36.87 |  0.961 |  0.967 | pass    |
| map.insert.fresh             | std::map       | report |  202 |     35.48 |      43.74 |  0.812 |  0.816 | report  |
| map.find.fresh               | std::flat_map  | G9     |  202 |      5.15 |       6.55 |  0.777 |  0.793 | pass    |
| map.find.fresh               | std::map       | report |  202 |      5.15 |      16.88 |  0.305 |  0.310 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  202 |     33.86 |      35.16 |  0.964 |  0.978 | pass    |
| map.erase.fresh              | std::map       | report |  202 |     33.86 |      47.28 |  0.712 |  0.718 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  202 |     32.31 |      28.97 |  1.118 |  1.125 | fail    |
| set.insert.fresh             | std::set       | report |  202 |     32.31 |      45.45 |  0.714 |  0.718 | report  |
| set.find.fresh               | std::flat_set  | G9     |  202 |      4.88 |       6.45 |  0.761 |  0.772 | pass    |
| set.find.fresh               | std::set       | report |  202 |      4.88 |      16.41 |  0.298 |  0.305 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  202 |     30.68 |      27.26 |  1.127 |  1.139 | fail    |
| set.erase.fresh              | std::set       | report |  202 |     30.68 |      46.55 |  0.659 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  202 |     33.20 |      29.38 |  1.113 |  1.139 | report  |
| map.insert.reused            | std::map       | report |  202 |     33.20 |      18.23 |  1.799 |  1.841 | report  |
| map.find.reused              | std::flat_map  | report |  202 |      5.15 |       6.55 |  0.784 |  0.802 | report  |
| map.find.reused              | std::map       | report |  202 |      5.15 |       6.56 |  0.785 |  0.796 | report  |
| map.erase.reused             | std::flat_map  | report |  202 |     30.11 |      27.34 |  1.103 |  1.106 | report  |
| map.erase.reused             | std::map       | report |  202 |     30.11 |      20.59 |  1.459 |  1.477 | report  |
| set.insert.reused            | std::flat_set  | report |  202 |     26.94 |      21.08 |  1.276 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  202 |     26.94 |      17.74 |  1.520 |  1.541 | report  |
| set.find.reused              | std::flat_set  | report |  202 |      4.88 |       6.45 |  0.751 |  0.762 | report  |
| set.find.reused              | std::set       | report |  202 |      4.88 |       6.92 |  0.715 |  0.725 | report  |
| set.erase.reused             | std::flat_set  | report |  202 |     23.11 |      20.26 |  1.130 |  1.154 | report  |
| set.erase.reused             | std::set       | report |  202 |     23.11 |      22.38 |  1.014 |  1.046 | report  |
| nested.build.ord             | std            | G9     |  202 |    388.03 |     865.88 |  0.457 |  0.460 | pass    |
| reset                        | std            | G9     |  202 |     33.84 |     158.86 |  0.211 |  0.213 | pass    |
| nested.build.plain           | std            | report |  202 |    389.33 |     855.47 |  0.461 |  0.464 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, set.insert.fresh, set.erase.fresh
```

## analyze t1

```
files: 2  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  202 |      0.41 |       0.31 |  1.290 |  1.321 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  202 |      0.32 |       0.41 |  0.778 |  0.804 | pass    |
| vec.iterate                  | std::vector    | G9     |  202 |      0.04 |       0.04 |  0.993 |  1.021 | pass    |
| vec.mid_insert               | std::vector    | G9     |  202 |     48.66 |      43.78 |  1.101 |  1.107 | fail    |
| vec.mid_erase                | std::vector    | G9     |  202 |     40.94 |      39.79 |  1.027 |  1.042 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  202 |    172.53 |     225.91 |  0.755 |  0.771 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  202 |    242.83 |     293.62 |  0.804 |  0.822 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  202 |     38.04 |      88.77 |  0.432 |  0.438 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  202 |   1037.11 |     227.86 |  4.549 |  4.632 | report  |
| hero.mid_insert.plain        | std::vector    | report |  202 |   1401.69 |     307.28 |  4.689 |  4.756 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  202 |     42.56 |      92.94 |  0.464 |  0.470 | report  |
| str.assign_short             | std::string    | report |  202 |      0.97 |       0.50 |  1.959 |  2.020 | report  |
| str.assign_long              | std::string    | G9     |  202 |      2.93 |       3.40 |  0.855 |  0.870 | pass    |
| str.assign_short.view        | std::string    | G9     |  202 |      0.99 |       2.71 |  0.364 |  0.366 | pass    |
| str.assign_short.copy        | std::string    | report |  202 |      0.81 |       0.50 |  1.601 |  1.609 | report  |
| str.append                   | std::string    | G9     |  202 |      1.50 |       3.46 |  0.437 |  0.442 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  202 |     35.52 |      36.83 |  0.955 |  0.975 | pass    |
| map.insert.fresh             | std::map       | report |  202 |     35.52 |      43.70 |  0.815 |  0.821 | report  |
| map.find.fresh               | std::flat_map  | G9     |  202 |      5.14 |       6.55 |  0.773 |  0.789 | pass    |
| map.find.fresh               | std::map       | report |  202 |      5.14 |      16.28 |  0.315 |  0.319 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  202 |     34.02 |      34.99 |  0.970 |  0.986 | pass    |
| map.erase.fresh              | std::map       | report |  202 |     34.02 |      46.96 |  0.725 |  0.733 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  202 |     32.23 |      28.89 |  1.119 |  1.129 | fail    |
| set.insert.fresh             | std::set       | report |  202 |     32.23 |      44.68 |  0.719 |  0.730 | report  |
| set.find.fresh               | std::flat_set  | G9     |  202 |      4.87 |       6.45 |  0.762 |  0.777 | pass    |
| set.find.fresh               | std::set       | report |  202 |      4.87 |      16.31 |  0.303 |  0.310 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  202 |     30.68 |      27.18 |  1.139 |  1.152 | fail    |
| set.erase.fresh              | std::set       | report |  202 |     30.68 |      46.88 |  0.655 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  202 |     33.28 |      29.30 |  1.123 |  1.138 | report  |
| map.insert.reused            | std::map       | report |  202 |     33.28 |      17.99 |  1.796 |  1.875 | report  |
| map.find.reused              | std::flat_map  | report |  202 |      5.15 |       6.55 |  0.783 |  0.794 | report  |
| map.find.reused              | std::map       | report |  202 |      5.15 |       7.22 |  0.715 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  202 |     29.95 |      27.26 |  1.089 |  1.099 | report  |
| map.erase.reused             | std::map       | report |  202 |     29.95 |      22.22 |  1.312 |  1.353 | report  |
| set.insert.reused            | std::flat_set  | report |  202 |     26.86 |      21.08 |  1.269 |  1.285 | report  |
| set.insert.reused            | std::set       | report |  202 |     26.86 |      17.50 |  1.492 |  1.546 | report  |
| set.find.reused              | std::flat_set  | report |  202 |      4.88 |       6.45 |  0.759 |  0.767 | report  |
| set.find.reused              | std::set       | report |  202 |      4.88 |       7.10 |  0.686 |  0.704 | report  |
| set.erase.reused             | std::flat_set  | report |  202 |     23.11 |      20.18 |  1.137 |  1.156 | report  |
| set.erase.reused             | std::set       | report |  202 |     23.11 |      19.21 |  1.197 |  1.217 | report  |
| nested.build.ord             | std            | G9     |  202 |    372.41 |     889.31 |  0.417 |  0.425 | pass    |
| reset                        | std            | G9     |  202 |     33.20 |     168.61 |  0.200 |  0.203 | pass    |
| nested.build.plain           | std            | report |  202 |    384.11 |     843.75 |  0.455 |  0.460 | report  |

G9: 15 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, set.insert.fresh, set.erase.fresh
```

## quiet

```
2026-10-27T07:32:44Z probe 1 start: waited 210s load 2.01 WindowServer 4.7% remote 6
2026-10-27T07:35:46Z reloc t0 1 start: waited 180s load 1.77 WindowServer 4.1% remote 6
2026-10-27T07:38:48Z reloc t1 1 start: waited 180s load 2.33 WindowServer 4.5% remote 6
2026-10-27T07:41:50Z reloc t1 2 start: waited 180s load 1.70 WindowServer 4.7% remote 6
2026-10-27T07:44:52Z reloc t0 2 start: waited 180s load 1.63 WindowServer 4.8% remote 6
2026-10-27T07:47:54Z reloc aligned start: waited 180s load 1.98 WindowServer 4.6% remote 6
2026-10-27T07:50:58Z probe 2 start: waited 180s load 2.45 WindowServer 4.6% remote 6
```
