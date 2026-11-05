# G9R3 result (2026-11-05T04:03:25Z)

t0 51a4815b7802ec125e2752847b69fd2a87ab7259 (next_cpp26 51a4815b (3.3.0; merged into PR #228)); t1 bf70e9986e54d927686ac372e65c706b071a86b7 (CKPT21b head (PR #228 trust tier P1 bf70e998)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 51a4815b7802ec125e2752847b69fd2a87ab7259 tree 254473076fde1ce98ca5529e307417cd8c0b5b3a typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 51a4815b (3.3.0; merged into PR #228))
t1 bf70e9986e54d927686ac372e65c706b071a86b7 tree 90e6b1074e49c36790ace0c64694f83861b887d9 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT21b head (PR #228 trust tier P1 bf70e998))
t1: diff over t0 sha256 d364a5353ef8efab2a3ef00f79c78bdacb6ede32bf671b4f425715daeff62a6e ( 11 files changed, 739 insertions(+), 4 deletions(-))
t1: library header diff over t0:  1 file changed, 79 insertions(+)
t1: first-parent commits over t0:
  bf70e998 Merge next_cpp26 51a4815b (3.3.0) into trust-admit-p1
  1ae7aedd test_receiver: do not name a variable far (a macro in Windows headers)
  cd19776e Trust tier P1: XReceiver admit for whole-wire reload and load
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 25d9006a12d1b47b14d53a9469e60526ebc8412c01b190def92b93c2b983a797
reloc_bench t1 sha256 25d9006a12d1b47b14d53a9469e60526ebc8412c01b190def92b93c2b983a797
reloc_bench t0a sha256 972c1725256dc635098dd57c1f6b270727f438c492d89778db8d403e8a292895
reloc_bench t1a sha256 972c1725256dc635098dd57c1f6b270727f438c492d89778db8d403e8a292895
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.717 | 0.989 | 0.986–0.993 | 0.984, 0.995 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.759 | 1.000 | 0.996–1.004 | 1.002, 0.997 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.369 | 0.370 | 1.003 | 1.000–1.006 | 1.005, 1.002 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.935 | 0.997 | 0.994–1.000 | 1.000, 0.994 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.861 | 0.862 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| nested.build.ord | std | G9 | 0.329 | 0.327 | 0.995 | 0.991–0.999 | 0.999, 0.991 |  |
| reset | std | G9 | 0.232 | 0.235 | 1.011 | 0.997–1.025 | 0.918, 1.113 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.083 | 1.084 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.752 | 0.754 | 1.002 | 0.999–1.005 | 1.004, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 0.996 | 0.997 | 0.994–0.999 | 0.994, 0.999 |  |
| str.append | std::string | G9 | 0.434 | 0.430 | 0.991 | 0.986–0.995 | 0.990, 0.991 |  |
| str.assign_long | std::string | G9 | 0.797 | 0.802 | 1.006 | 1.003–1.008 | 1.007, 1.004 | worse |
| str.assign_short.view | std::string | G9 | 0.340 | 0.343 | 1.008 | 1.006–1.011 | 1.013, 1.004 | worse |
| vec.iterate | std::vector | G9 | 1.000 | 1.001 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.023 | 1.023 | 1.000 | 0.998–1.003 | 1.005, 0.996 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.961 | 1.001 | 0.998–1.004 | 1.001, 1.002 |  |
| vec.push_back | std::vector | G9 | 1.145 | 1.143 | 0.998 | 0.992–1.004 | 1.001, 0.995 |  |
| vec.push_back_grow | std::vector | G9 | 0.764 | 0.763 | 0.999 | 0.993–1.005 | 0.997, 1.001 |  |
| hero.mid_erase.plain | std::vector | report | 4.079 | 4.073 | 0.998 | 0.995–1.002 | 0.993, 1.004 |  |
| hero.mid_insert.plain | std::vector | report | 4.215 | 4.211 | 0.999 | 0.995–1.003 | 1.002, 0.996 |  |
| hero.push_back_grow.plain | std::vector | report | 0.409 | 0.412 | 1.006 | 1.003–1.008 | 1.015, 0.997 |  |
| map.erase.fresh | std::map | report | 0.743 | 0.735 | 0.990 | 0.986–0.993 | 0.968, 1.011 |  |
| map.erase.reused | std::flat_map | report | 1.071 | 1.076 | 1.004 | 1.001–1.007 | 1.011, 0.997 |  |
| map.erase.reused | std::map | report | 1.654 | 1.637 | 0.990 | 0.984–0.996 | 0.955, 1.026 |  |
| map.find.fresh | std::map | report | 0.325 | 0.325 | 1.000 | 0.996–1.004 | 0.998, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.806 | 0.806 | 0.999 | 0.997–1.002 | 1.002, 0.997 |  |
| map.find.reused | std::map | report | 0.831 | 0.828 | 0.996 | 0.993–1.000 | 0.990, 1.002 |  |
| map.insert.fresh | std::map | report | 0.733 | 0.736 | 1.004 | 1.000–1.008 | 1.012, 0.996 |  |
| map.insert.reused | std::flat_map | report | 1.027 | 1.028 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| map.insert.reused | std::map | report | 1.845 | 1.828 | 0.991 | 0.985–0.997 | 1.003, 0.979 |  |
| nested.build.plain | std | report | 0.343 | 0.340 | 0.989 | 0.985–0.994 | 0.976, 1.003 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.665 | 0.993 | 0.990–0.996 | 0.977, 1.010 |  |
| set.erase.reused | std::flat_set | report | 1.127 | 1.126 | 0.999 | 0.995–1.003 | 1.001, 0.997 |  |
| set.erase.reused | std::set | report | 1.338 | 1.337 | 0.999 | 0.993–1.006 | 0.974, 1.025 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.002 | 0.997–1.006 | 1.001, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.754 | 0.998 | 0.996–1.001 | 1.000, 0.997 |  |
| set.find.reused | std::set | report | 0.683 | 0.668 | 0.977 | 0.972–0.983 | 0.968, 0.987 |  |
| set.insert.fresh | std::set | report | 0.650 | 0.650 | 1.000 | 0.997–1.003 | 1.009, 0.991 |  |
| set.insert.reused | std::flat_set | report | 1.109 | 1.109 | 0.999 | 0.996–1.002 | 1.000, 0.999 |  |
| set.insert.reused | std::set | report | 1.470 | 1.463 | 0.995 | 0.987–1.004 | 1.012, 0.978 |  |
| str.assign_short | std::string | report | 1.772 | 1.769 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.152 | 1.001 | 0.998–1.005 | 1.001, 1.001 |  |

worse at t1 than t0 (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.003; str.assign_long / std::string (G9) 1.006; str.assign_short.view / std::string (G9) 1.008

above 1.01 but not worse by the rule: reset / std (G9) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.2 | 164.1 | 0.999 | 0.997–1.002 | 1.001, 0.997 |  |
| hero.mid_erase.plain | 912.6 | 915.5 | 1.003 | 1.001–1.005 | 1.001, 1.005 | worse |
| hero.mid_insert.ord | 227.8 | 228.2 | 1.002 | 1.000–1.004 | 1.003, 1.001 | worse |
| hero.mid_insert.plain | 1257 | 1255 | 0.998 | 0.997–1.000 | 1.004, 0.993 |  |
| hero.push_back_grow.ord | 31.95 | 31.88 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| hero.push_back_grow.plain | 35.4 | 35.35 | 0.999 | 0.997–1.000 | 1.001, 0.997 |  |
| map.erase.fresh | 33.07 | 33.04 | 0.999 | 0.997–1.001 | 1.001, 0.998 |  |
| map.erase.reused | 29.53 | 29.63 | 1.003 | 1.002–1.005 | 1.006, 1.001 | worse |
| map.find.fresh | 5.291 | 5.292 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| map.find.reused | 5.301 | 5.294 | 0.999 | 0.998–1.000 | 0.998, 0.999 |  |
| map.insert.fresh | 31.96 | 31.93 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| map.insert.reused | 30.28 | 30.27 | 1.000 | 0.999–1.001 | 0.999, 1.001 |  |
| nested.build.ord | 272.5 | 272.3 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| nested.build.plain | 281.8 | 281.2 | 0.998 | 0.994–1.002 | 0.991, 1.005 |  |
| reset | 28.78 | 29.11 | 1.012 | 1.004–1.019 | 0.997, 1.027 | worse? |
| set.erase.fresh | 29.79 | 29.78 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| set.erase.reused | 23.05 | 23.06 | 1.001 | 0.998–1.004 | 1.003, 0.998 |  |
| set.find.fresh | 4.88 | 4.887 | 1.002 | 1.000–1.003 | 1.003, 1.000 |  |
| set.find.reused | 4.883 | 4.884 | 1.000 | 0.998–1.002 | 1.001, 1.000 |  |
| set.insert.fresh | 28.89 | 28.81 | 0.997 | 0.995–1.000 | 0.995, 1.000 |  |
| set.insert.reused | 23.44 | 23.42 | 0.999 | 0.997–1.002 | 1.002, 0.997 |  |
| str.append | 1.544 | 1.543 | 0.999 | 0.995–1.003 | 0.994, 1.004 |  |
| str.assign_long | 2.716 | 2.722 | 1.002 | 1.000–1.004 | 1.001, 1.002 |  |
| str.assign_short | 0.9301 | 0.928 | 0.998 | 0.994–1.001 | 0.997, 0.999 |  |
| str.assign_short.copy | 0.6146 | 0.6186 | 1.007 | 1.002–1.011 | 1.003, 1.010 | worse |
| str.assign_short.view | 0.9331 | 0.9348 | 1.002 | 0.999–1.004 | 1.002, 1.001 |  |
| vec.iterate | 0.0332 | 0.03315 | 0.998 | 0.998–0.999 | 0.999, 0.998 |  |
| vec.mid_erase | 40.5 | 40.54 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| vec.mid_insert | 40.59 | 40.61 | 1.001 | 0.999–1.002 | 0.999, 1.002 |  |
| vec.push_back | 0.2987 | 0.2944 | 0.986 | 0.974–0.997 | 0.980, 0.991 |  |
| vec.push_back_grow | 0.277 | 0.2734 | 0.987 | 0.982–0.993 | 0.985, 0.989 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.plain 1.003; hero.mid_insert.ord 1.002; map.erase.reused 1.003; str.assign_short.copy 1.007

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.725 | 1.000 | 0.994–1.006 | 1.003, 0.997 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.758 | 0.997 | 0.991–1.004 | 1.002, 0.993 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.356 | 0.368 | 1.033 | 1.027–1.038 | 1.060, 1.006 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.948 | 0.943 | 0.994 | 0.990–0.999 | 0.995, 0.994 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.858 | 0.854 | 0.996 | 0.992–0.999 | 0.992, 0.999 |  |
| nested.build.ord | std | G9 | 0.318 | 0.314 | 0.986 | 0.979–0.992 | 1.021, 0.951 |  |
| reset | std | G9 | 0.225 | 0.219 | 0.974 | 0.958–0.991 | 0.940, 1.009 |  |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.095 | 1.004 | 0.997–1.011 | 1.000, 1.008 |  |
| set.find.fresh | std::flat_set | G9 | 0.757 | 0.756 | 1.000 | 0.995–1.005 | 0.999, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.993 | 0.998 | 0.993–1.002 | 0.998, 0.998 |  |
| str.append | std::string | G9 | 0.431 | 0.431 | 1.001 | 0.995–1.007 | 1.020, 0.983 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.864 | 0.998 | 0.994–1.001 | 0.998, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.001 | 0.999–1.004 | 1.000, 1.002 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.002 | 1.001 | 0.998–1.005 | 1.001, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.020 | 1.000 | 0.996–1.004 | 1.003, 0.997 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.957 | 1.002 | 0.998–1.007 | 1.003, 1.002 |  |
| vec.push_back | std::vector | G9 | 1.120 | 1.113 | 0.994 | 0.990–0.998 | 0.997, 0.991 |  |
| vec.push_back_grow | std::vector | G9 | 0.759 | 0.758 | 0.998 | 0.993–1.004 | 1.008, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 4.060 | 4.035 | 0.994 | 0.990–0.998 | 0.989, 0.999 |  |
| hero.mid_insert.plain | std::vector | report | 4.182 | 4.153 | 0.993 | 0.988–0.998 | 0.995, 0.991 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.409 | 1.009 | 1.005–1.013 | 1.006, 1.011 | worse |
| map.erase.fresh | std::map | report | 0.746 | 0.730 | 0.978 | 0.973–0.983 | 0.963, 0.993 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.079 | 0.998 | 0.993–1.003 | 1.000, 0.996 |  |
| map.erase.reused | std::map | report | 1.602 | 1.563 | 0.976 | 0.967–0.984 | 0.968, 0.983 |  |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.004 | 0.999–1.008 | 1.001, 1.007 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.809 | 0.999 | 0.994–1.004 | 1.002, 0.996 |  |
| map.find.reused | std::map | report | 0.734 | 0.728 | 0.992 | 0.986–0.998 | 0.963, 1.022 |  |
| map.insert.fresh | std::map | report | 0.718 | 0.714 | 0.995 | 0.990–0.999 | 0.999, 0.990 |  |
| map.insert.reused | std::flat_map | report | 1.029 | 1.030 | 1.001 | 0.996–1.006 | 1.001, 1.000 |  |
| map.insert.reused | std::map | report | 1.895 | 1.836 | 0.969 | 0.963–0.975 | 0.991, 0.947 |  |
| nested.build.plain | std | report | 0.331 | 0.325 | 0.981 | 0.974–0.989 | 1.023, 0.942 |  |
| set.erase.fresh | std::set | report | 0.667 | 0.664 | 0.995 | 0.990–1.001 | 0.983, 1.008 |  |
| set.erase.reused | std::flat_set | report | 1.136 | 1.129 | 0.994 | 0.989–0.999 | 1.002, 0.986 |  |
| set.erase.reused | std::set | report | 1.297 | 1.288 | 0.993 | 0.983–1.003 | 0.993, 0.993 |  |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.002 | 0.996–1.008 | 1.002, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.758 | 0.755 | 0.997 | 0.992–1.001 | 0.997, 0.996 |  |
| set.find.reused | std::set | report | 0.674 | 0.674 | 1.000 | 0.995–1.005 | 0.974, 1.027 |  |
| set.insert.fresh | std::set | report | 0.652 | 0.644 | 0.988 | 0.983–0.993 | 0.998, 0.978 |  |
| set.insert.reused | std::flat_set | report | 1.110 | 1.108 | 0.998 | 0.994–1.002 | 1.003, 0.994 |  |
| set.insert.reused | std::set | report | 1.479 | 1.434 | 0.970 | 0.959–0.980 | 1.000, 0.940 |  |
| str.assign_short | std::string | report | 1.763 | 1.770 | 1.004 | 1.002–1.006 | 1.001, 1.007 | worse |
| str.assign_short.copy | std::string | report | 1.150 | 1.151 | 1.001 | 0.996–1.006 | 1.002, 1.000 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.033; hero.push_back_grow.plain / std::vector (report) 1.009; str.assign_short / std::string (report) 1.004

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.4 | 166.4 | 1.012 | 1.007–1.017 | 1.001, 1.023 | worse |
| hero.mid_erase.plain | 912.6 | 918.1 | 1.006 | 1.002–1.010 | 0.990, 1.023 |  |
| hero.mid_insert.ord | 232.9 | 231.1 | 0.992 | 0.986–0.999 | 0.966, 1.020 |  |
| hero.mid_insert.plain | 1250 | 1258 | 1.006 | 1.003–1.010 | 0.993, 1.020 |  |
| hero.push_back_grow.ord | 31.98 | 32.27 | 1.009 | 1.004–1.014 | 0.994, 1.025 |  |
| hero.push_back_grow.plain | 35.42 | 35.69 | 1.008 | 1.004–1.011 | 0.993, 1.023 |  |
| map.erase.fresh | 33.28 | 33.03 | 0.992 | 0.988–0.997 | 0.994, 0.991 |  |
| map.erase.reused | 30.01 | 29.61 | 0.987 | 0.982–0.991 | 0.997, 0.976 |  |
| map.find.fresh | 5.295 | 5.307 | 1.002 | 1.000–1.005 | 1.000, 1.005 |  |
| map.find.reused | 5.372 | 5.309 | 0.988 | 0.984–0.993 | 1.000, 0.977 |  |
| map.insert.fresh | 31.99 | 31.95 | 0.999 | 0.997–1.001 | 0.999, 0.998 |  |
| map.insert.reused | 30.67 | 30.3 | 0.988 | 0.984–0.992 | 0.997, 0.979 |  |
| nested.build.ord | 270.5 | 268.4 | 0.992 | 0.988–0.996 | 0.999, 0.985 |  |
| nested.build.plain | 280.3 | 277.5 | 0.990 | 0.986–0.994 | 1.000, 0.981 |  |
| reset | 29.19 | 28.29 | 0.969 | 0.963–0.975 | 0.965, 0.974 |  |
| set.erase.fresh | 30.2 | 29.87 | 0.989 | 0.985–0.994 | 0.999, 0.979 |  |
| set.erase.reused | 23.47 | 23.06 | 0.982 | 0.977–0.988 | 1.002, 0.963 |  |
| set.find.fresh | 4.947 | 4.888 | 0.988 | 0.984–0.992 | 0.996, 0.980 |  |
| set.find.reused | 4.938 | 4.882 | 0.989 | 0.985–0.993 | 1.000, 0.978 |  |
| set.insert.fresh | 29.24 | 28.85 | 0.987 | 0.982–0.991 | 0.999, 0.975 |  |
| set.insert.reused | 23.79 | 23.41 | 0.984 | 0.979–0.989 | 1.002, 0.966 |  |
| str.append | 1.557 | 1.566 | 1.005 | 1.000–1.010 | 1.021, 0.990 |  |
| str.assign_long | 2.943 | 2.946 | 1.001 | 0.999–1.003 | 0.999, 1.003 |  |
| str.assign_short | 0.9302 | 0.9253 | 0.995 | 0.992–0.997 | 1.001, 0.989 |  |
| str.assign_short.copy | 0.6203 | 0.615 | 0.992 | 0.987–0.997 | 1.004, 0.979 |  |
| str.assign_short.view | 0.9393 | 0.9328 | 0.993 | 0.990–0.996 | 0.997, 0.989 |  |
| vec.iterate | 0.0333 | 0.03382 | 1.016 | 1.011–1.021 | 0.998, 1.033 | worse? |
| vec.mid_erase | 40.53 | 40.84 | 1.008 | 1.004–1.012 | 1.001, 1.015 | worse |
| vec.mid_insert | 40.52 | 40.93 | 1.010 | 1.006–1.014 | 1.001, 1.019 | worse |
| vec.push_back | 0.2719 | 0.2746 | 1.010 | 1.007–1.014 | 1.000, 1.020 | worse |
| vec.push_back_grow | 0.2699 | 0.2721 | 1.008 | 1.004–1.012 | 0.998, 1.018 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.012; vec.mid_erase 1.008; vec.mid_insert 1.010; vec.push_back 1.010

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.plain (XOffset ns/op) | 1.003 | 1.006 | 1.002–1.010 | not worse |
| hero.mid_insert.ord (XOffset ns/op) | 1.002 | 0.992 | 0.986–0.999 | not worse |
| map.erase.reused (XOffset ns/op) | 1.003 | 0.987 | 0.982–0.991 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.007 | 0.992 | 0.987–0.997 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.003 | 1.033 | 1.027–1.038 | still worse |
| str.assign_long / std::string (G9) | 1.006 | 0.998 | 0.994–1.001 | not worse |
| str.assign_short.view / std::string (G9) | 1.008 | 1.001 | 0.999–1.004 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.94 | 163.74 | 0.999 | 226.22 | 1.002 |
| hero.mid_erase.plain | 912.98 | 914.77 | 1.002 | 223.26 | 1.002 |
| hero.mid_insert.ord | 227.25 | 227.54 | 1.001 | 296.31 | 0.986 |
| hero.mid_insert.plain | 1254.93 | 1252.71 | 0.998 | 299.87 | 1.009 |
| hero.push_back_grow.ord | 31.83 | 31.76 | 0.998 | 86.49 | 1.008 |
| hero.push_back_grow.plain | 35.31 | 35.26 | 0.998 | 86.33 | 1.002 |
| map.erase.fresh | 32.97 | 32.98 | 1.000 | 35.16 | 0.998 |
| map.erase.reused | 29.41 | 29.57 | 1.006 | 27.40 | 0.996 |
| map.find.fresh | 5.28 | 5.29 | 1.002 | 6.55 | 1.000 |
| map.find.reused | 5.30 | 5.29 | 0.998 | 6.55 | 0.999 |
| map.insert.fresh | 31.90 | 31.91 | 1.000 | 36.96 | 0.999 |
| map.insert.reused | 30.23 | 30.22 | 1.000 | 29.38 | 0.996 |
| nested.build.ord | 272.45 | 271.72 | 0.997 | 824.60 | 1.002 |
| nested.build.plain | 280.35 | 281.06 | 1.003 | 817.16 | 0.998 |
| reset | 28.61 | 29.05 | 1.015 | 123.21 | 0.983 |
| set.erase.fresh | 29.76 | 29.77 | 1.000 | 27.46 | 0.997 |
| set.erase.reused | 22.85 | 22.85 | 1.000 | 20.38 | 0.990 |
| set.find.fresh | 4.88 | 4.87 | 0.999 | 6.47 | 0.999 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 28.81 | 28.80 | 1.000 | 28.89 | 0.984 |
| set.insert.reused | 23.39 | 23.40 | 1.000 | 21.12 | 0.996 |
| str.append | 1.51 | 1.52 | 1.004 | 3.50 | 1.004 |
| str.assign_long | 2.71 | 2.71 | 1.000 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.000 | 0.52 | 0.999 |
| str.assign_short.copy | 0.61 | 0.60 | 0.994 | 0.53 | 0.993 |
| str.assign_short.view | 0.93 | 0.93 | 0.999 | 2.73 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.999 | 0.03 | 1.004 |
| vec.mid_erase | 40.42 | 40.44 | 1.000 | 39.57 | 0.997 |
| vec.mid_insert | 40.51 | 40.51 | 1.000 | 42.15 | 1.000 |
| vec.push_back | 0.30 | 0.29 | 0.995 | 0.26 | 1.001 |
| vec.push_back_grow | 0.27 | 0.27 | 0.996 | 0.36 | 0.999 |

## analyze t0 (G9 at the anchor 51a4815b)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.145 |  1.154 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.764 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.960 |  0.963 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.55 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.20 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      86.51 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     224.61 |  4.079 |  4.091 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.03 |  4.215 |  4.232 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      86.22 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.772 |  1.777 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.797 |  0.799 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.340 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.49 |  0.434 |  0.436 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.861 |  0.865 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      43.13 |  0.733 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.33 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.743 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.27 |  0.650 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.752 |  0.754 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.39 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.083 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.670 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.027 |  1.031 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.11 |  1.845 |  1.857 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.806 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.39 |  0.831 |  0.835 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.38 |      27.42 |  1.071 |  1.074 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.38 |      17.50 |  1.654 |  1.664 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.54 |  1.470 |  1.483 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.07 |  0.683 |  0.687 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.93 |  1.338 |  1.347 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     822.94 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     28.66 |     117.84 |  0.232 |  0.236 | pass    |
| nested.build.plain           | std            | report |  808 |    279.97 |     817.72 |  0.343 |  0.345 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.147 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.763 |  0.766 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.63 |  1.023 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     229.17 |  0.717 |  0.720 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      85.77 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    915.36 |     224.61 |  4.073 |  4.088 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1253.27 |     295.58 |  4.211 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      85.69 |  0.412 |  0.413 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.802 |  0.804 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.152 |  1.157 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.57 |  0.430 |  0.432 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.862 |  0.864 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.97 |  0.736 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.935 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.735 |  0.738 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.97 |  0.996 |  0.997 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.19 |  0.650 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.665 |  0.667 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.028 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.36 |  1.828 |  1.837 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.806 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       6.33 |  0.828 |  0.831 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.43 |  1.076 |  1.078 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.637 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.112 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.463 |  1.475 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.16 |  0.668 |  0.672 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.126 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.76 |  1.337 |  1.347 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     826.17 |  0.327 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.235 |  0.238 | pass    |
| nested.build.plain           | std            | report |  808 |    281.25 |     828.12 |  0.340 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-05T03:54:08Z block 1 start: waited 180s load 1.84 WindowServer 6.1% remote 4
2026-11-05T03:57:12Z block 2 start: waited 180s load 1.36 WindowServer 5.7% remote 4
2026-11-05T04:00:17Z block 3 start: waited 180s load 1.44 WindowServer 5.8% remote 4
2026-11-05T04:03:21Z block 4 start: waited 180s load 1.76 WindowServer 4.5% remote 4
```
