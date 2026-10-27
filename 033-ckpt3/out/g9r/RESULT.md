# G9R result (2026-10-27T06:36:17Z)

t0 35492720b5981d9672c8b8f00037dcb687aaf9fd (next_cpp26 3549272 (G8 checkpoint)); t1 30af216f6063316fb3348188f1570e53ecf26468 (next_cpp26 30af216 (L5 #125, L1 #126, L2 #127, L3 #128, L4 #129; also #120, #121)); bench/reloc of t1 (aabaa1ed859747bc5237fe471e8107c6f92c642d) for both.

## fwd_probe

```
fwd_probe: 50000000 iterations x 9 rounds; calibration 0.2280 ns/cycle (4.39 GHz)
case  pattern                                         min ns    med ns   min cyc   med cyc
calib 8 dependent adds (8 cycles)                      1.824     1.827      8.00      8.01
F1    str w, ldr w same 4 B                            0.231     0.240      1.01      1.05
F2    str x, ldr x same 8 B                            0.227     0.236      0.99      1.04
F3    str w, ldr x over it (low half)                  1.594     1.599      6.99      7.01
F4    str w size_, ldp w,w [#8] (xo push_back)         1.596     1.599      7.00      7.01
F5    str w size_, ldp w,w [#4] (begin+size)           1.596     1.600      7.00      7.02
F6    str x end, ldp x,x [#0] (libc++ vector)          1.596     1.600      7.00      7.02
F7    stp w,w, ldp w,w same 8 B                        1.597     1.599      7.00      7.01
F8    str q head, ldp w,w [#8] inside                  4.205     4.210     18.44     18.46
F9    str q head, ldr w [#8] inside                    4.199     4.202     18.42     18.43
F10   str x [#8], ldp w,w [#4] straddling              1.824     1.826      8.00      8.01

fwd_probe: 50000000 iterations x 9 rounds; calibration 0.2277 ns/cycle (4.39 GHz)
case  pattern                                         min ns    med ns   min cyc   med cyc
calib 8 dependent adds (8 cycles)                      1.822     1.827      8.00      8.02
F1    str w, ldr w same 4 B                            0.227     0.232      1.00      1.02
F2    str x, ldr x same 8 B                            0.227     0.298      1.00      1.31
F3    str w, ldr x over it (low half)                  1.595     1.597      7.01      7.02
F4    str w size_, ldp w,w [#8] (xo push_back)         1.597     1.599      7.01      7.02
F5    str w size_, ldp w,w [#4] (begin+size)           1.594     1.597      7.00      7.02
F6    str x end, ldp x,x [#0] (libc++ vector)          1.595     1.598      7.00      7.02
F7    stp w,w, ldp w,w same 8 B                        1.596     1.598      7.01      7.02
F8    str q head, ldp w,w [#8] inside                  4.197     4.205     18.43     18.47
F9    str q head, ldr w [#8] inside                    4.192     4.200     18.41     18.45
F10   str x [#8], ldp w,w [#4] straddling              1.824     1.827      8.01      8.02
```

## reloc t1 / t0 (median per file, GM of 4 files)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) |
| --- | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 270.50 | 172.04 | 0.636 | 228.02 |
| hero.mid_erase.plain | 1031.07 | 1041.24 | 1.010 | 225.33 |
| hero.mid_insert.ord | 366.62 | 242.51 | 0.661 | 300.14 |
| hero.mid_insert.plain | 1393.57 | 1409.58 | 1.011 | 301.54 |
| hero.push_back_grow.ord | 39.33 | 38.05 | 0.968 | 89.93 |
| hero.push_back_grow.plain | 42.72 | 42.43 | 0.993 | 88.32 |
| map.erase.fresh | 33.92 | 33.96 | 1.001 | 35.24 |
| map.erase.reused | 30.09 | 29.98 | 0.996 | 27.32 |
| map.find.fresh | 5.15 | 5.15 | 1.000 | 6.55 |
| map.find.reused | 5.15 | 5.15 | 1.000 | 6.56 |
| map.insert.fresh | 35.52 | 35.53 | 1.000 | 36.88 |
| map.insert.reused | 33.18 | 33.20 | 1.001 | 29.34 |
| nested.build.ord | 387.52 | 369.62 | 0.954 | 852.77 |
| nested.build.plain | 392.89 | 386.22 | 0.983 | 838.64 |
| reset | 32.72 | 34.01 | 1.039 | 162.08 |
| set.erase.fresh | 30.70 | 30.73 | 1.001 | 27.34 |
| set.erase.reused | 23.21 | 23.09 | 0.995 | 20.24 |
| set.find.fresh | 4.88 | 4.88 | 0.999 | 6.45 |
| set.find.reused | 4.87 | 4.88 | 1.001 | 6.45 |
| set.insert.fresh | 32.32 | 32.29 | 0.999 | 28.90 |
| set.insert.reused | 27.00 | 26.84 | 0.994 | 21.16 |
| str.append | 1.52 | 1.61 | 1.064 | 4.35 |
| str.assign_long | 3.29 | 2.93 | 0.891 | 3.41 |
| str.assign_short | 1.30 | 0.97 | 0.744 | 0.51 |
| str.assign_short.copy | 1.59 | 0.81 | 0.507 | 0.51 |
| str.assign_short.view | 1.28 | 0.99 | 0.769 | 2.72 |
| vec.iterate | 0.03 | 0.04 | 1.073 | 0.03 |
| vec.mid_erase | 41.61 | 41.91 | 1.007 | 39.33 |
| vec.mid_insert | 44.79 | 47.72 | 1.066 | 40.69 |
| vec.push_back | 2.03 | 0.41 | 0.202 | 0.31 |
| vec.push_back_grow | 1.76 | 0.32 | 0.182 | 0.39 |

## analyze t0

```
files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.05 |       0.31 |  6.512 |  6.569 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.76 |       0.39 |  4.476 |  4.502 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.001 |  1.008 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     45.08 |      40.69 |  1.102 |  1.112 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.83 |      39.39 |  1.057 |  1.065 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    270.19 |     227.87 |  1.184 |  1.196 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    366.53 |     306.00 |  1.220 |  1.233 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     39.31 |      90.17 |  0.437 |  0.439 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1032.55 |     224.61 |  4.592 |  4.632 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1392.58 |     294.60 |  4.624 |  4.687 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.68 |      88.46 |  0.484 |  0.489 | report  |
| str.assign_short             | std::string    | report |  404 |      1.28 |       0.50 |  2.522 |  2.550 | report  |
| str.assign_long              | std::string    | G9     |  404 |      3.39 |       3.40 |  0.970 |  0.978 | pass    |
| str.assign_short.view        | std::string    | G9     |  404 |      1.28 |       2.71 |  0.472 |  0.473 | pass    |
| str.assign_short.copy        | std::string    | report |  404 |      1.59 |       0.51 |  3.148 |  3.178 | report  |
| str.append                   | std::string    | G9     |  404 |      1.51 |       4.35 |  0.350 |  0.353 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.48 |      36.87 |  0.961 |  0.968 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.48 |      43.70 |  0.809 |  0.815 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.15 |       6.55 |  0.785 |  0.792 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.15 |      16.78 |  0.307 |  0.310 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.24 |  0.961 |  0.965 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      47.53 |  0.712 |  0.717 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.89 |  1.121 |  1.130 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      45.25 |  0.716 |  0.723 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.88 |       6.45 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.88 |      16.34 |  0.299 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.68 |      27.34 |  1.124 |  1.135 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.68 |      46.71 |  0.656 |  0.662 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.38 |  1.130 |  1.135 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.872 |  1.905 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.783 |  0.789 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       6.68 |  0.780 |  0.786 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     30.11 |      27.34 |  1.099 |  1.110 | report  |
| map.erase.reused             | std::map       | report |  404 |     30.11 |      20.67 |  1.426 |  1.448 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     27.02 |      21.16 |  1.274 |  1.285 | report  |
| set.insert.reused            | std::set       | report |  404 |     27.02 |      17.50 |  1.519 |  1.542 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.87 |       6.45 |  0.760 |  0.769 | report  |
| set.find.reused              | std::set       | report |  404 |      4.87 |       6.79 |  0.715 |  0.723 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.26 |  1.145 |  1.153 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      21.81 |  1.054 |  1.068 | report  |
| nested.build.ord             | std            | G9     |  404 |    388.00 |     853.52 |  0.455 |  0.460 | pass    |
| reset                        | std            | G9     |  404 |     32.56 |     159.52 |  0.202 |  0.204 | pass    |
| nested.build.plain           | std            | report |  404 |    393.22 |     839.84 |  0.465 |  0.471 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, set.insert.fresh, set.erase.fresh
```

## analyze t1

```
files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      0.42 |       0.32 |  1.291 |  1.301 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      0.32 |       0.41 |  0.788 |  0.794 | pass    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.04 |       0.04 |  0.999 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     48.58 |      43.78 |  1.098 |  1.109 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     42.24 |      39.79 |  1.056 |  1.067 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    172.52 |     227.20 |  0.754 |  0.758 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    242.19 |     306.64 |  0.803 |  0.813 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     38.13 |      88.26 |  0.432 |  0.436 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1040.36 |     228.52 |  4.639 |  4.680 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1409.50 |     306.64 |  4.668 |  4.726 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.44 |      89.31 |  0.478 |  0.483 | report  |
| str.assign_short             | std::string    | report |  404 |      0.97 |       0.50 |  1.922 |  1.953 | report  |
| str.assign_long              | std::string    | G9     |  404 |      2.93 |       3.40 |  0.860 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  404 |      0.99 |       2.71 |  0.364 |  0.365 | pass    |
| str.assign_short.copy        | std::string    | report |  404 |      0.80 |       0.50 |  1.601 |  1.604 | report  |
| str.append                   | std::string    | G9     |  404 |      1.61 |       3.48 |  0.460 |  0.467 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.56 |      36.70 |  0.969 |  0.979 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.56 |      43.29 |  0.823 |  0.831 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.15 |       6.55 |  0.790 |  0.796 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.15 |      16.40 |  0.315 |  0.319 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.07 |  0.963 |  0.972 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      47.04 |  0.719 |  0.727 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.97 |  1.112 |  1.120 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      44.92 |  0.716 |  0.719 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.88 |       6.45 |  0.754 |  0.760 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.88 |      16.26 |  0.301 |  0.304 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.76 |      27.34 |  1.122 |  1.131 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.76 |      46.96 |  0.650 |  0.657 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.30 |  1.133 |  1.137 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.882 |  1.898 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.779 |  0.785 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       7.76 |  0.682 |  0.692 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     29.95 |      27.18 |  1.088 |  1.097 | report  |
| map.erase.reused             | std::map       | report |  404 |     29.95 |      21.69 |  1.336 |  1.357 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.86 |      21.16 |  1.264 |  1.277 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.86 |      17.50 |  1.502 |  1.534 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.88 |       6.45 |  0.761 |  0.769 | report  |
| set.find.reused              | std::set       | report |  404 |      4.88 |       7.38 |  0.661 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.11 |      20.18 |  1.147 |  1.155 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.11 |      19.86 |  1.153 |  1.168 | report  |
| nested.build.ord             | std            | G9     |  404 |    369.80 |     856.78 |  0.435 |  0.438 | pass    |
| reset                        | std            | G9     |  404 |     34.50 |     159.52 |  0.210 |  0.212 | pass    |
| nested.build.plain           | std            | report |  404 |    386.72 |     845.06 |  0.462 |  0.467 | report  |

G9: 14 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, vec.mid_erase, set.insert.fresh, set.erase.fresh
```

## quiet

```
2026-10-27T06:05:37Z probe 1 start: waited 180s load 1.77 WindowServer 3.5% remote 6
2026-10-27T06:08:49Z reloc t0 1 start: waited 180s load 2.10 WindowServer 4.3% remote 6
2026-10-27T06:11:51Z reloc t1 1 start: waited 180s load 1.56 WindowServer 4.7% remote 6
2026-10-27T06:14:53Z reloc t1 2 start: waited 180s load 1.83 WindowServer 4.6% remote 6
2026-10-27T06:17:55Z reloc t0 2 start: waited 180s load 2.51 WindowServer 4% remote 6
2026-10-27T06:20:57Z reloc t0 3 start: waited 180s load 2.91 WindowServer 5.1% remote 6
2026-10-27T06:27:00Z reloc t1 3 start: waited 360s load 1.82 WindowServer 4.3% remote 6
2026-10-27T06:30:02Z reloc t1 4 start: waited 180s load 2.44 WindowServer 4.3% remote 6
2026-10-27T06:33:04Z reloc t0 4 start: waited 180s load 1.69 WindowServer 4.4% remote 6
2026-10-27T06:36:06Z probe 2 start: waited 180s load 1.80 WindowServer 4.5% remote 6
```
