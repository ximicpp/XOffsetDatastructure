# G9R3 result (2026-11-05T02:54:02Z)

t0 c9fae222bb86ef829f9cf5b7a7c241e6978e93f6 (next_cpp26 c9fae222 (3.3.0, #227; merged into PR #223)); t1 9313428e633aac1e9c2e0a57ab03a17cc718f244 (CKPT20 head (PR #223 root cache refill 9313428e)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 c9fae222bb86ef829f9cf5b7a7c241e6978e93f6 tree d9cb2e8d37d8211a0ddadcbe533688782a7faaff typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 c9fae222 (3.3.0, #227; merged into PR #223))
t1 9313428e633aac1e9c2e0a57ab03a17cc718f244 tree c5a99eebc2faa59760926c5a6c981fc1d2587609 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT20 head (PR #223 root cache refill 9313428e))
t1: diff over t0 sha256 431ee370a21da1527bcb0dbb0eeb50e92ed8424a4082b0930d2a7e5c0016d214 ( 4 files changed, 72 insertions(+), 7 deletions(-))
t1: library header diff over t0:  1 file changed, 33 insertions(+), 6 deletions(-)
t1: first-parent commits over t0:
  9313428e Merge next_cpp26 c9fae222 (3.3.0, #227) into root refill
  16de37a5 Merge next_cpp26 e7fd8cb0 (#214, #219, #220) into root refill
  937d833d root<T>(): fill the cache on the checked path, inline the hit
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 25d9006a12d1b47b14d53a9469e60526ebc8412c01b190def92b93c2b983a797
reloc_bench t1 sha256 9484e95dca8b55bb72b9f4031703346b000db68357aa41b66cec50b04baa5b9b
reloc_bench t0a sha256 972c1725256dc635098dd57c1f6b270727f438c492d89778db8d403e8a292895
reloc_bench t1a sha256 fe67d0d985d7dea7d13bdc7569539adeaff5c999e0c977ae1b454c6afa2c5f61
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.723 | 0.994 | 0.991–0.996 | 0.988, 0.999 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.761 | 1.001 | 0.998–1.005 | 1.001, 1.002 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.370 | 0.372 | 1.005 | 1.003–1.007 | 0.997, 1.013 |  |
| map.erase.fresh | std::flat_map | G9 | 0.935 | 0.941 | 1.006 | 1.004–1.008 | 1.007, 1.005 | worse |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.808 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.863 | 0.861 | 0.997 | 0.995–0.999 | 1.000, 0.994 |  |
| nested.build.ord | std | G9 | 0.327 | 0.326 | 0.996 | 0.992–0.999 | 0.988, 1.003 |  |
| reset | std | G9 | 0.221 | 0.214 | 0.971 | 0.955–0.987 | 0.870, 1.083 |  |
| set.erase.fresh | std::flat_set | G9 | 1.084 | 1.085 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.756 | 1.002 | 1.001–1.004 | 1.003, 1.002 | worse |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.996 | 1.001 | 0.999–1.002 | 1.000, 1.002 |  |
| str.append | std::string | G9 | 0.436 | 0.428 | 0.982 | 0.978–0.987 | 0.993, 0.972 |  |
| str.assign_long | std::string | G9 | 0.800 | 0.865 | 1.081 | 1.079–1.084 | 1.080, 1.083 | worse |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.003 | 1.001–1.005 | 1.002, 1.003 | worse |
| vec.iterate | std::vector | G9 | 1.003 | 1.003 | 1.000 | 0.996–1.004 | 0.997, 1.003 |  |
| vec.mid_erase | std::vector | G9 | 1.023 | 1.017 | 0.994 | 0.992–0.996 | 0.993, 0.995 |  |
| vec.mid_insert | std::vector | G9 | 0.961 | 0.961 | 1.000 | 0.997–1.003 | 0.998, 1.002 |  |
| vec.push_back | std::vector | G9 | 1.148 | 1.118 | 0.974 | 0.972–0.976 | 0.975, 0.973 |  |
| vec.push_back_grow | std::vector | G9 | 0.769 | 0.765 | 0.995 | 0.992–0.998 | 0.995, 0.995 |  |
| hero.mid_erase.plain | std::vector | report | 4.069 | 4.065 | 0.999 | 0.996–1.002 | 0.996, 1.002 |  |
| hero.mid_insert.plain | std::vector | report | 4.229 | 4.251 | 1.005 | 1.001–1.009 | 1.003, 1.007 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.411 | 0.414 | 1.007 | 1.005–1.009 | 0.998, 1.016 |  |
| map.erase.fresh | std::map | report | 0.737 | 0.737 | 1.000 | 0.997–1.003 | 0.988, 1.012 |  |
| map.erase.reused | std::flat_map | report | 1.071 | 1.078 | 1.007 | 1.004–1.009 | 1.007, 1.006 | worse |
| map.erase.reused | std::map | report | 1.636 | 1.634 | 0.999 | 0.993–1.004 | 0.987, 1.010 |  |
| map.find.fresh | std::map | report | 0.326 | 0.324 | 0.995 | 0.991–0.999 | 0.995, 0.995 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.999–1.002 | 0.999, 1.002 |  |
| map.find.reused | std::map | report | 0.824 | 0.746 | 0.906 | 0.902–0.909 | 0.911, 0.901 |  |
| map.insert.fresh | std::map | report | 0.734 | 0.751 | 1.024 | 1.020–1.027 | 1.029, 1.018 | worse |
| map.insert.reused | std::flat_map | report | 1.028 | 1.032 | 1.003 | 1.002–1.005 | 1.011, 0.996 |  |
| map.insert.reused | std::map | report | 1.847 | 1.868 | 1.011 | 1.007–1.016 | 1.004, 1.019 | worse |
| nested.build.plain | std | report | 0.342 | 0.340 | 0.994 | 0.991–0.998 | 0.999, 0.990 |  |
| set.erase.fresh | std::set | report | 0.666 | 0.669 | 1.004 | 1.001–1.008 | 0.993, 1.015 |  |
| set.erase.reused | std::flat_set | report | 1.127 | 1.134 | 1.007 | 1.004–1.009 | 1.006, 1.008 | worse |
| set.erase.reused | std::set | report | 1.333 | 1.340 | 1.005 | 0.999–1.011 | 0.980, 1.030 |  |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.007 | 1.002–1.011 | 1.006, 1.007 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.687 | 0.653 | 0.950 | 0.946–0.954 | 0.953, 0.946 |  |
| set.insert.fresh | std::set | report | 0.654 | 0.652 | 0.998 | 0.995–1.000 | 0.995, 1.000 |  |
| set.insert.reused | std::flat_set | report | 1.109 | 1.111 | 1.001 | 0.998–1.004 | 1.000, 1.003 |  |
| set.insert.reused | std::set | report | 1.480 | 1.462 | 0.988 | 0.980–0.996 | 0.965, 1.011 |  |
| str.assign_short | std::string | report | 1.768 | 1.764 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.153 | 1.002 | 1.000–1.004 | 1.002, 1.003 | worse |

worse at t1 than t0 (by the rule): map.erase.fresh / std::flat_map (G9) 1.006; set.find.fresh / std::flat_set (G9) 1.002; str.assign_long / std::string (G9) 1.081; str.assign_short.view / std::string (G9) 1.003; hero.mid_insert.plain / std::vector (report) 1.005; map.erase.reused / std::flat_map (report) 1.007; map.insert.fresh / std::map (report) 1.024; map.insert.reused / std::map (report) 1.011; set.erase.reused / std::flat_set (report) 1.007; set.find.fresh / std::set (report) 1.007; str.assign_short.copy / std::string (report) 1.002

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 163.8 | 1.000 | 0.998–1.002 | 0.997, 1.003 |  |
| hero.mid_erase.plain | 912.5 | 913.6 | 1.001 | 0.999–1.003 | 0.998, 1.004 |  |
| hero.mid_insert.ord | 227.9 | 228.1 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| hero.mid_insert.plain | 1260 | 1268 | 1.006 | 1.005–1.008 | 1.005, 1.007 | worse |
| hero.push_back_grow.ord | 31.98 | 31.97 | 1.000 | 0.998–1.001 | 0.991, 1.009 |  |
| hero.push_back_grow.plain | 35.43 | 35.44 | 1.000 | 0.999–1.002 | 0.990, 1.010 |  |
| map.erase.fresh | 33 | 32.97 | 0.999 | 0.997–1.001 | 0.999, 1.000 |  |
| map.erase.reused | 29.52 | 29.57 | 1.002 | 1.000–1.003 | 1.004, 0.999 |  |
| map.find.fresh | 5.296 | 5.288 | 0.998 | 0.997–1.000 | 0.998, 0.999 |  |
| map.find.reused | 5.3 | 5.303 | 1.000 | 0.999–1.002 | 0.999, 1.002 |  |
| map.insert.fresh | 31.88 | 31.97 | 1.003 | 1.002–1.004 | 1.002, 1.004 | worse |
| map.insert.reused | 30.28 | 30.36 | 1.002 | 1.001–1.004 | 1.003, 1.002 | worse |
| nested.build.ord | 270.4 | 270.8 | 1.002 | 0.999–1.004 | 0.999, 1.004 |  |
| nested.build.plain | 280.8 | 279.3 | 0.995 | 0.992–0.998 | 0.995, 0.994 |  |
| reset | 28.34 | 28.25 | 0.997 | 0.992–1.001 | 0.997, 0.996 |  |
| set.erase.fresh | 29.77 | 29.81 | 1.001 | 1.000–1.003 | 1.002, 1.000 |  |
| set.erase.reused | 23.02 | 23.15 | 1.006 | 1.003–1.008 | 1.005, 1.007 | worse |
| set.find.fresh | 4.883 | 4.881 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| set.find.reused | 4.881 | 4.886 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| set.insert.fresh | 28.8 | 28.79 | 1.000 | 0.999–1.000 | 1.000, 0.999 |  |
| set.insert.reused | 23.42 | 23.43 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| str.append | 1.543 | 1.533 | 0.994 | 0.990–0.997 | 0.998, 0.990 |  |
| str.assign_long | 2.717 | 2.936 | 1.081 | 1.079–1.082 | 1.079, 1.082 | worse |
| str.assign_short | 0.9272 | 0.9232 | 0.996 | 0.993–0.998 | 0.988, 1.004 |  |
| str.assign_short.copy | 0.6148 | 0.6125 | 0.996 | 0.992–1.001 | 0.984, 1.008 |  |
| str.assign_short.view | 0.9345 | 0.935 | 1.001 | 0.998–1.003 | 0.989, 1.012 |  |
| vec.iterate | 0.0332 | 0.03322 | 1.001 | 0.997–1.004 | 0.997, 1.004 |  |
| vec.mid_erase | 40.41 | 40.26 | 0.996 | 0.995–0.998 | 0.997, 0.996 |  |
| vec.mid_insert | 40.61 | 40.55 | 0.998 | 0.997–1.000 | 0.996, 1.001 |  |
| vec.push_back | 0.2983 | 0.2885 | 0.967 | 0.956–0.978 | 0.971, 0.963 |  |
| vec.push_back_grow | 0.2763 | 0.2742 | 0.992 | 0.987–0.997 | 0.992, 0.992 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_insert.plain 1.006; map.insert.fresh 1.003; map.insert.reused 1.002; set.erase.reused 1.006; str.assign_long 1.081

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.725 | 1.000 | 0.997–1.003 | 1.005, 0.995 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.761 | 0.997 | 0.994–1.001 | 0.997, 0.998 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.364 | 0.368 | 1.011 | 1.008–1.013 | 1.020, 1.001 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.943 | 0.942 | 0.999 | 0.997–1.002 | 1.000, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.809 | 1.001 | 0.999–1.003 | 1.001, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.858 | 0.861 | 1.003 | 1.001–1.005 | 1.001, 1.005 | worse |
| nested.build.ord | std | G9 | 0.327 | 0.315 | 0.963 | 0.958–0.969 | 0.999, 0.930 |  |
| reset | std | G9 | 0.220 | 0.201 | 0.912 | 0.898–0.927 | 0.998, 0.834 |  |
| set.erase.fresh | std::flat_set | G9 | 1.092 | 1.090 | 0.998 | 0.996–1.001 | 1.002, 0.995 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.994 | 0.994 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| str.append | std::string | G9 | 0.426 | 0.426 | 1.000 | 0.995–1.005 | 1.002, 0.998 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.865 | 0.998 | 0.996–1.001 | 0.997, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.991–1.009 | 0.999, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.020 | 0.999 | 0.998–1.001 | 1.004, 0.994 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.956 | 0.996 | 0.994–0.998 | 0.993, 1.000 |  |
| vec.push_back | std::vector | G9 | 1.116 | 1.118 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| vec.push_back_grow | std::vector | G9 | 0.764 | 0.769 | 1.007 | 1.004–1.009 | 0.993, 1.021 |  |
| hero.mid_erase.plain | std::vector | report | 4.035 | 4.046 | 1.003 | 1.000–1.006 | 0.998, 1.007 |  |
| hero.mid_insert.plain | std::vector | report | 4.208 | 4.195 | 0.997 | 0.993–1.001 | 0.997, 0.997 |  |
| hero.push_back_grow.plain | std::vector | report | 0.411 | 0.410 | 0.996 | 0.994–0.998 | 0.999, 0.994 |  |
| map.erase.fresh | std::map | report | 0.743 | 0.733 | 0.987 | 0.984–0.990 | 0.983, 0.991 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.082 | 1.002 | 0.999–1.005 | 1.000, 1.003 |  |
| map.erase.reused | std::map | report | 1.588 | 1.583 | 0.997 | 0.989–1.004 | 1.025, 0.970 |  |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.001 | 0.996–1.005 | 1.002, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.810 | 0.810 | 0.999 | 0.996–1.002 | 1.001, 0.996 |  |
| map.find.reused | std::map | report | 0.733 | 0.725 | 0.989 | 0.984–0.994 | 1.004, 0.975 |  |
| map.insert.fresh | std::map | report | 0.730 | 0.713 | 0.977 | 0.973–0.981 | 0.978, 0.976 |  |
| map.insert.reused | std::flat_map | report | 1.028 | 1.025 | 0.997 | 0.994–1.001 | 0.997, 0.998 |  |
| map.insert.reused | std::map | report | 1.901 | 1.846 | 0.971 | 0.966–0.976 | 0.994, 0.949 |  |
| nested.build.plain | std | report | 0.340 | 0.324 | 0.953 | 0.947–0.959 | 0.986, 0.922 |  |
| set.erase.fresh | std::set | report | 0.672 | 0.655 | 0.975 | 0.972–0.977 | 0.979, 0.970 |  |
| set.erase.reused | std::flat_set | report | 1.129 | 1.131 | 1.002 | 0.999–1.004 | 0.998, 1.005 |  |
| set.erase.reused | std::set | report | 1.287 | 1.291 | 1.004 | 0.996–1.011 | 1.032, 0.976 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.001 | 0.996–1.005 | 0.999, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.756 | 0.999 | 0.997–1.001 | 0.999, 1.000 |  |
| set.find.reused | std::set | report | 0.676 | 0.666 | 0.984 | 0.980–0.989 | 0.992, 0.976 |  |
| set.insert.fresh | std::set | report | 0.653 | 0.651 | 0.997 | 0.995–0.999 | 1.002, 0.992 |  |
| set.insert.reused | std::flat_set | report | 1.111 | 1.111 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |
| set.insert.reused | std::set | report | 1.499 | 1.438 | 0.959 | 0.951–0.968 | 0.973, 0.946 |  |
| str.assign_short | std::string | report | 1.769 | 1.766 | 0.999 | 0.997–1.001 | 1.000, 0.997 |  |
| str.assign_short.copy | std::string | report | 1.152 | 1.151 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.011; map.insert.fresh / std::flat_map (G9) 1.003

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.4 | 164.3 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| hero.mid_erase.plain | 905.4 | 910 | 1.005 | 1.003–1.007 | 0.999, 1.012 |  |
| hero.mid_insert.ord | 229 | 228 | 0.995 | 0.994–0.997 | 0.994, 0.997 |  |
| hero.mid_insert.plain | 1253 | 1248 | 0.996 | 0.994–0.998 | 0.997, 0.995 |  |
| hero.push_back_grow.ord | 31.75 | 31.99 | 1.008 | 1.006–1.010 | 1.013, 1.002 | worse |
| hero.push_back_grow.plain | 35.44 | 35.55 | 1.003 | 1.001–1.005 | 1.006, 1.001 | worse |
| map.erase.fresh | 32.98 | 33.1 | 1.003 | 1.001–1.005 | 1.004, 1.002 | worse |
| map.erase.reused | 29.68 | 29.66 | 0.999 | 0.997–1.002 | 0.997, 1.002 |  |
| map.find.fresh | 5.291 | 5.294 | 1.001 | 0.999–1.002 | 0.999, 1.002 |  |
| map.find.reused | 5.32 | 5.315 | 0.999 | 0.997–1.002 | 1.001, 0.997 |  |
| map.insert.fresh | 31.94 | 31.95 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| map.insert.reused | 30.25 | 30.23 | 0.999 | 0.998–1.000 | 0.999, 1.000 |  |
| nested.build.ord | 269.7 | 270.3 | 1.002 | 1.000–1.005 | 0.997, 1.008 |  |
| nested.build.plain | 279.1 | 279 | 1.000 | 0.997–1.003 | 0.998, 1.002 |  |
| reset | 29.59 | 29.13 | 0.984 | 0.978–0.991 | 0.976, 0.993 |  |
| set.erase.fresh | 29.77 | 29.76 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| set.erase.reused | 23.05 | 23.11 | 1.003 | 1.000–1.005 | 1.001, 1.005 | worse |
| set.find.fresh | 4.881 | 4.887 | 1.001 | 0.999–1.003 | 1.002, 1.001 |  |
| set.find.reused | 4.88 | 4.884 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| set.insert.fresh | 28.79 | 28.79 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| set.insert.reused | 23.39 | 23.41 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| str.append | 1.54 | 1.549 | 1.006 | 1.001–1.011 | 0.994, 1.019 |  |
| str.assign_long | 2.936 | 2.949 | 1.004 | 1.002–1.007 | 0.999, 1.010 |  |
| str.assign_short | 0.924 | 0.9416 | 1.019 | 1.015–1.023 | 1.008, 1.030 | worse |
| str.assign_short.copy | 0.6154 | 0.6173 | 1.003 | 0.999–1.007 | 1.008, 0.998 |  |
| str.assign_short.view | 0.9324 | 0.9491 | 1.018 | 1.008–1.028 | 1.007, 1.029 | worse |
| vec.iterate | 0.03326 | 0.03332 | 1.002 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.mid_erase | 40.6 | 40.37 | 0.994 | 0.993–0.995 | 0.994, 0.994 |  |
| vec.mid_insert | 40.56 | 40.49 | 0.998 | 0.997–1.000 | 0.997, 1.000 |  |
| vec.push_back | 0.2719 | 0.2718 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| vec.push_back_grow | 0.2687 | 0.2714 | 1.010 | 1.008–1.012 | 1.002, 1.019 | worse |

XOffset slower at t1a than t0a (worse by the rule): hero.push_back_grow.ord 1.008; hero.push_back_grow.plain 1.003; map.erase.fresh 1.003; set.erase.reused 1.003; str.assign_short 1.019; str.assign_short.view 1.018; vec.push_back_grow 1.010

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_insert.plain (XOffset ns/op) | 1.006 | 0.996 | 0.994–0.998 | not worse |
| map.insert.fresh (XOffset ns/op) | 1.003 | 1.000 | 0.999–1.002 | not worse |
| map.insert.reused (XOffset ns/op) | 1.002 | 0.999 | 0.998–1.000 | not worse |
| set.erase.reused (XOffset ns/op) | 1.006 | 1.003 | 1.000–1.005 | still worse |
| str.assign_long (XOffset ns/op) | 1.081 | 1.004 | 1.002–1.007 | not worse |
| hero.mid_insert.plain / std::vector (report) | 1.005 | 0.997 | 0.993–1.001 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.006 | 0.999 | 0.997–1.002 | not worse |
| map.erase.reused / std::flat_map (report) | 1.007 | 1.002 | 0.999–1.005 | not worse |
| map.insert.fresh / std::map (report) | 1.024 | 0.977 | 0.973–0.981 | not worse |
| map.insert.reused / std::map (report) | 1.011 | 0.971 | 0.966–0.976 | not worse |
| set.erase.reused / std::flat_set (report) | 1.007 | 1.002 | 0.999–1.004 | not worse |
| set.find.fresh / std::flat_set (G9) | 1.002 | 1.000 | 0.998–1.002 | not worse |
| set.find.fresh / std::set (report) | 1.007 | 1.001 | 0.996–1.005 | not worse |
| str.assign_long / std::string (G9) | 1.081 | 0.998 | 0.996–1.001 | not worse |
| str.assign_short.copy / std::string (report) | 1.002 | 1.000 | 0.998–1.001 | not worse |
| str.assign_short.view / std::string (G9) | 1.003 | 1.000 | 0.991–1.009 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.49 | 163.65 | 1.001 | 224.84 | 1.000 |
| hero.mid_erase.plain | 912.37 | 912.04 | 1.000 | 224.61 | 1.004 |
| hero.mid_insert.ord | 227.17 | 227.70 | 1.002 | 299.47 | 0.995 |
| hero.mid_insert.plain | 1257.48 | 1266.10 | 1.007 | 298.51 | 0.996 |
| hero.push_back_grow.ord | 31.89 | 31.91 | 1.001 | 86.31 | 1.006 |
| hero.push_back_grow.plain | 35.38 | 35.39 | 1.000 | 86.12 | 1.002 |
| map.erase.fresh | 32.96 | 32.93 | 0.999 | 35.20 | 1.002 |
| map.erase.reused | 29.43 | 29.44 | 1.001 | 27.43 | 1.002 |
| map.find.fresh | 5.29 | 5.28 | 0.998 | 6.55 | 1.000 |
| map.find.reused | 5.30 | 5.29 | 0.999 | 6.55 | 1.000 |
| map.insert.fresh | 31.90 | 31.95 | 1.002 | 36.90 | 1.000 |
| map.insert.reused | 30.25 | 30.31 | 1.002 | 29.42 | 0.999 |
| nested.build.ord | 270.17 | 270.34 | 1.001 | 825.30 | 1.003 |
| nested.build.plain | 280.75 | 278.79 | 0.993 | 821.12 | 0.999 |
| reset | 28.26 | 28.18 | 0.997 | 128.12 | 0.987 |
| set.erase.fresh | 29.75 | 29.80 | 1.002 | 27.44 | 1.000 |
| set.erase.reused | 22.86 | 22.94 | 1.004 | 20.37 | 1.004 |
| set.find.fresh | 4.87 | 4.87 | 1.000 | 6.47 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 28.81 | 28.76 | 0.998 | 28.87 | 0.999 |
| set.insert.reused | 23.36 | 23.41 | 1.002 | 21.04 | 1.002 |
| str.append | 1.52 | 1.51 | 0.991 | 3.50 | 1.006 |
| str.assign_long | 2.71 | 2.93 | 1.081 | 3.39 | 1.001 |
| str.assign_short | 0.92 | 0.92 | 0.999 | 0.52 | 1.016 |
| str.assign_short.copy | 0.61 | 0.60 | 0.987 | 0.53 | 1.008 |
| str.assign_short.view | 0.93 | 0.93 | 1.002 | 2.71 | 1.002 |
| vec.iterate | 0.03 | 0.03 | 1.004 | 0.03 | 1.002 |
| vec.mid_erase | 40.33 | 40.15 | 0.995 | 39.48 | 0.993 |
| vec.mid_insert | 40.55 | 40.50 | 0.999 | 42.14 | 0.998 |
| vec.push_back | 0.30 | 0.29 | 0.970 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.27 | 0.986 | 0.36 | 1.009 |

## analyze t0 (G9 at the anchor c9fae222)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.148 |  1.150 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.003 |  1.007 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.47 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.25 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     294.92 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.47 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.76 |     224.61 |  4.069 |  4.080 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.03 |  4.229 |  4.246 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      86.06 |  0.411 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.46 |  0.436 |  0.438 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.87 |  0.863 |  0.864 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.89 |  0.734 |  0.737 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.809 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.935 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.51 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.666 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.46 |  1.028 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.28 |  1.847 |  1.856 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.42 |  0.824 |  0.826 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.071 |  1.073 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.636 |  1.645 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.114 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.480 |  1.491 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.06 |  0.687 |  0.690 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.130 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.85 |  1.333 |  1.341 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     822.94 |  0.327 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     118.50 |  0.221 |  0.224 | pass    |
| nested.build.plain           | std            | report |  808 |    280.61 |     821.62 |  0.342 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.118 |  1.120 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.003 |  1.006 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.963 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.63 |  1.017 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.91 |  0.723 |  0.724 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.27 |  0.761 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.94 |      86.06 |  0.372 |  0.372 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.45 |     225.25 |  4.065 |  4.079 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1266.92 |     304.69 |  4.251 |  4.267 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.40 |      85.82 |  0.414 |  0.415 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.767 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.153 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.54 |  0.428 |  0.430 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.98 |      37.11 |  0.861 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.98 |      42.48 |  0.751 |  0.753 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.34 |  0.324 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.996 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.99 |  0.652 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.28 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.669 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.35 |      29.38 |  1.032 |  1.034 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.35 |      16.03 |  1.868 |  1.876 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.08 |  0.746 |  0.749 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.078 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.74 |  1.634 |  1.643 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.111 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.54 |  1.462 |  1.475 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.55 |  0.653 |  0.655 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.134 |  1.137 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.85 |  1.340 |  1.348 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     826.81 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     118.48 |  0.214 |  0.218 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     822.91 |  0.340 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-05T02:44:44Z block 1 start: waited 180s load 1.53 WindowServer 8% remote 4
2026-11-05T02:47:49Z block 2 start: waited 180s load 1.51 WindowServer 7.4% remote 4
2026-11-05T02:50:53Z block 3 start: waited 180s load 1.88 WindowServer 7.7% remote 4
2026-11-05T02:53:58Z block 4 start: waited 180s load 1.58 WindowServer 8.5% remote 4
```
