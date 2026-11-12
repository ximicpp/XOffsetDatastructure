# G9R3 result (2026-11-12T11:27:50Z)

t0 30fb525c262b235a9de2a982942d60c4069808ab (next_cpp26 30fb525c (first parent of #269)); t1 8753965d49520ea0ca522d1639033f831237c703 (PR269 head (8753965d, #269 merged)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 30fb525c262b235a9de2a982942d60c4069808ab tree 84116a2417b84411217de327df6f36c8b3933206 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 30fb525c (first parent of #269))
t1 8753965d49520ea0ca522d1639033f831237c703 tree 394a16be6963343e1929f600353fc22efdb56b61 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (PR269 head (8753965d, #269 merged))
t1: diff over t0 sha256 e2f24b4bfd3c8a583664c44a912819a08ac3e28234fd06ba8c318bae35759031 ( 4 files changed, 72 insertions(+), 8 deletions(-))
t1: library header diff over t0:  1 file changed, 42 insertions(+), 3 deletions(-)
t1: first-parent commits over t0:
  8753965d Merge pull request #269: 3.6 perf, first-storage bump (P1) and SLG UL in-place refill (P2)
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 9fb6eb4bb31f75b8ab7a1b1998bd18918328fd9e6f2d06e49ca86e56f7b4f038
reloc_bench t1 sha256 c86cdc10ca6c2cf24137c8e7cc1278711d3dce66406a6406068aeaa0b7021344
reloc_bench t0a sha256 1566cf243663fc9f33c2909449bdc12929212faa74de1b84a9f6bf06a068e360
reloc_bench t1a sha256 2e840753fd54872ec634b4aa7df42fe5d0be100ebffcb13f525f1ab86c36e563
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.741 | 0.733 | 0.989 | 0.986–0.992 | 0.993, 0.986 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.759 | 0.999 | 0.995–1.003 | 1.001, 0.997 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.352 | 0.347 | 0.987 | 0.984–0.989 | 1.002, 0.972 |  |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.936 | 0.996 | 0.994–0.997 | 0.998, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.810 | 0.807 | 0.997 | 0.995–0.999 | 0.996, 0.998 |  |
| map.insert.fresh | std::flat_map | G9 | 0.847 | 0.843 | 0.995 | 0.992–0.998 | 0.995, 0.995 |  |
| nested.build.ord | std | G9 | 0.316 | 0.313 | 0.989 | 0.986–0.993 | 1.003, 0.976 |  |
| reset | std | G9 | 0.188 | 0.171 | 0.908 | 0.894–0.923 | 0.984, 0.838 |  |
| set.erase.fresh | std::flat_set | G9 | 1.086 | 1.085 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.753 | 0.756 | 1.004 | 1.001–1.006 | 1.006, 1.001 | worse |
| set.insert.fresh | std::flat_set | G9 | 0.987 | 0.985 | 0.998 | 0.996–1.001 | 0.999, 0.998 |  |
| str.append | std::string | G9 | 0.419 | 0.420 | 1.003 | 0.998–1.007 | 0.995, 1.011 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.865 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.342 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.016 | 1.024 | 1.008 | 1.006–1.009 | 1.007, 1.009 | worse |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.988 | 0.995 | 0.993–0.998 | 0.988, 1.003 |  |
| vec.push_back | std::vector | G9 | 1.119 | 1.133 | 1.013 | 1.010–1.015 | 1.023, 1.002 | worse |
| vec.push_back_grow | std::vector | G9 | 0.765 | 0.768 | 1.003 | 1.000–1.007 | 0.999, 1.008 |  |
| hero.mid_erase.plain | std::vector | report | 4.058 | 4.003 | 0.986 | 0.983–0.990 | 0.994, 0.979 |  |
| hero.mid_insert.plain | std::vector | report | 4.230 | 4.206 | 0.994 | 0.991–0.997 | 1.001, 0.988 |  |
| hero.push_back_grow.plain | std::vector | report | 0.387 | 0.389 | 1.004 | 1.002–1.007 | 1.018, 0.991 |  |
| map.erase.fresh | std::map | report | 0.737 | 0.726 | 0.984 | 0.981–0.987 | 0.985, 0.983 |  |
| map.erase.reused | std::flat_map | report | 1.078 | 1.077 | 0.999 | 0.996–1.002 | 0.997, 1.000 |  |
| map.erase.reused | std::map | report | 1.622 | 1.546 | 0.953 | 0.947–0.960 | 0.957, 0.949 |  |
| map.find.fresh | std::map | report | 0.320 | 0.325 | 1.017 | 1.013–1.022 | 1.017, 1.018 | worse |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| map.find.reused | std::map | report | 0.714 | 0.837 | 1.172 | 1.169–1.176 | 1.174, 1.171 | worse |
| map.insert.fresh | std::map | report | 0.735 | 0.731 | 0.994 | 0.989–0.999 | 0.998, 0.990 |  |
| map.insert.reused | std::flat_map | report | 1.018 | 1.029 | 1.011 | 1.009–1.014 | 1.011, 1.011 | worse |
| map.insert.reused | std::map | report | 1.892 | 1.859 | 0.983 | 0.978–0.988 | 0.985, 0.980 |  |
| nested.build.plain | std | report | 0.323 | 0.319 | 0.987 | 0.983–0.990 | 0.996, 0.978 |  |
| set.erase.fresh | std::set | report | 0.669 | 0.658 | 0.983 | 0.980–0.987 | 0.986, 0.980 |  |
| set.erase.reused | std::flat_set | report | 1.132 | 1.134 | 1.002 | 0.999–1.005 | 1.003, 1.000 |  |
| set.erase.reused | std::set | report | 1.331 | 1.275 | 0.957 | 0.951–0.964 | 0.957, 0.958 |  |
| set.find.fresh | std::set | report | 0.301 | 0.302 | 1.003 | 0.998–1.007 | 1.004, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.find.reused | std::set | report | 0.785 | 0.751 | 0.956 | 0.952–0.959 | 0.952, 0.959 |  |
| set.insert.fresh | std::set | report | 0.646 | 0.642 | 0.994 | 0.991–0.997 | 0.999, 0.989 |  |
| set.insert.reused | std::flat_set | report | 1.107 | 1.106 | 0.999 | 0.997–1.002 | 0.998, 1.000 |  |
| set.insert.reused | std::set | report | 1.513 | 1.442 | 0.953 | 0.946–0.961 | 0.951, 0.956 |  |
| str.assign_short | std::string | report | 1.764 | 1.766 | 1.001 | 0.999–1.003 | 0.999, 1.003 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.151 | 1.001 | 0.998–1.003 | 1.001, 1.000 |  |

worse at t1 than t0 (by the rule): set.find.fresh / std::flat_set (G9) 1.004; vec.mid_erase / std::vector (G9) 1.008; vec.push_back / std::vector (G9) 1.013; map.find.fresh / std::map (report) 1.017; map.find.reused / std::map (report) 1.172; map.insert.reused / std::flat_map (report) 1.011

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 167.4 | 165.8 | 0.990 | 0.989–0.992 | 0.993, 0.987 |  |
| hero.mid_erase.plain | 911 | 900.1 | 0.988 | 0.985–0.991 | 0.999, 0.977 |  |
| hero.mid_insert.ord | 227.7 | 227.5 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| hero.mid_insert.plain | 1258 | 1258 | 1.000 | 0.998–1.001 | 1.005, 0.994 |  |
| hero.push_back_grow.ord | 30.66 | 30.28 | 0.988 | 0.986–0.989 | 0.988, 0.987 |  |
| hero.push_back_grow.plain | 33.69 | 33.82 | 1.004 | 1.002–1.006 | 1.008, 1.000 |  |
| map.erase.fresh | 33 | 33 | 1.000 | 0.999–1.001 | 1.001, 1.000 |  |
| map.erase.reused | 29.6 | 29.58 | 0.999 | 0.997–1.001 | 1.000, 0.999 |  |
| map.find.fresh | 5.303 | 5.286 | 0.997 | 0.994–0.999 | 0.996, 0.997 |  |
| map.find.reused | 5.305 | 5.304 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.fresh | 31.81 | 31.77 | 0.999 | 0.996–1.001 | 0.995, 1.003 |  |
| map.insert.reused | 30.04 | 30.43 | 1.013 | 1.012–1.014 | 1.013, 1.013 | worse |
| nested.build.ord | 261.4 | 259.2 | 0.991 | 0.988–0.994 | 0.992, 0.991 |  |
| nested.build.plain | 267.1 | 264.9 | 0.992 | 0.990–0.994 | 0.994, 0.990 |  |
| reset | 24.89 | 24.82 | 0.997 | 0.993–1.001 | 0.998, 0.996 |  |
| set.erase.fresh | 29.77 | 29.8 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.erase.reused | 23.07 | 23.13 | 1.002 | 1.000–1.005 | 1.004, 1.001 |  |
| set.find.fresh | 4.883 | 4.886 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| set.find.reused | 4.884 | 4.883 | 1.000 | 0.998–1.002 | 0.997, 1.002 |  |
| set.insert.fresh | 28.59 | 28.52 | 0.998 | 0.996–0.999 | 0.997, 0.998 |  |
| set.insert.reused | 23.43 | 23.39 | 0.998 | 0.998–0.999 | 0.998, 0.999 |  |
| str.append | 1.541 | 1.547 | 1.004 | 0.999–1.008 | 0.996, 1.012 |  |
| str.assign_long | 2.935 | 2.942 | 1.002 | 1.001–1.004 | 1.005, 1.000 |  |
| str.assign_short | 0.9242 | 0.9337 | 1.010 | 1.007–1.013 | 1.009, 1.011 | worse |
| str.assign_short.copy | 0.6139 | 0.623 | 1.015 | 1.010–1.020 | 1.016, 1.014 | worse |
| str.assign_short.view | 0.9333 | 0.9416 | 1.009 | 1.006–1.012 | 1.008, 1.010 | worse |
| vec.iterate | 0.03316 | 0.0332 | 1.001 | 0.999–1.003 | 0.999, 1.003 |  |
| vec.mid_erase | 40.13 | 40.46 | 1.008 | 1.007–1.010 | 1.006, 1.010 | worse |
| vec.mid_insert | 40.38 | 40.62 | 1.006 | 1.004–1.008 | 1.009, 1.003 | worse |
| vec.push_back | 0.2929 | 0.3009 | 1.027 | 1.013–1.041 | 1.032, 1.022 | worse |
| vec.push_back_grow | 0.2752 | 0.278 | 1.010 | 1.004–1.016 | 1.008, 1.012 | worse |

XOffset slower at t1 than t0 (worse by the rule): map.insert.reused 1.013; str.assign_short 1.010; str.assign_short.copy 1.015; str.assign_short.view 1.009; vec.mid_erase 1.008; vec.mid_insert 1.006; vec.push_back 1.027; vec.push_back_grow 1.010

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.734 | 0.737 | 1.005 | 1.002–1.007 | 1.000, 1.009 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.762 | 0.761 | 0.999 | 0.996–1.003 | 0.992, 1.006 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.349 | 0.348 | 0.997 | 0.995–0.999 | 0.994, 1.000 |  |
| map.erase.fresh | std::flat_map | G9 | 0.944 | 0.939 | 0.995 | 0.993–0.997 | 0.995, 0.995 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.809 | 1.001 | 0.999–1.003 | 1.002, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.844 | 0.844 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| nested.build.ord | std | G9 | 0.315 | 0.298 | 0.945 | 0.939–0.951 | 0.916, 0.975 |  |
| reset | std | G9 | 0.202 | 0.154 | 0.763 | 0.754–0.771 | 0.697, 0.834 |  |
| set.erase.fresh | std::flat_set | G9 | 1.094 | 1.092 | 0.998 | 0.995–1.001 | 0.996, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.990 | 0.985 | 0.995 | 0.993–0.998 | 0.997, 0.993 |  |
| str.append | std::string | G9 | 0.430 | 0.430 | 1.000 | 0.996–1.005 | 0.999, 1.002 |  |
| str.assign_long | std::string | G9 | 0.813 | 0.813 | 1.000 | 0.998–1.002 | 0.998, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 0.999 | 0.996–1.002 | 0.997, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 0.999–1.000 | 1.000, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.018 | 1.000 | 0.999–1.001 | 1.008, 0.993 |  |
| vec.mid_insert | std::vector | G9 | 0.995 | 0.992 | 0.997 | 0.996–0.999 | 0.997, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.117 | 1.111 | 0.994 | 0.992–0.997 | 1.001, 0.987 |  |
| vec.push_back_grow | std::vector | G9 | 0.768 | 0.763 | 0.994 | 0.990–0.997 | 0.990, 0.997 |  |
| hero.mid_erase.plain | std::vector | report | 4.010 | 4.071 | 1.015 | 1.012–1.018 | 1.005, 1.026 | worse |
| hero.mid_insert.plain | std::vector | report | 4.207 | 4.231 | 1.006 | 1.003–1.009 | 1.007, 1.004 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.389 | 0.385 | 0.989 | 0.986–0.991 | 0.994, 0.983 |  |
| map.erase.fresh | std::map | report | 0.734 | 0.728 | 0.992 | 0.988–0.995 | 0.976, 1.008 |  |
| map.erase.reused | std::flat_map | report | 1.082 | 1.079 | 0.997 | 0.994–0.999 | 0.996, 0.997 |  |
| map.erase.reused | std::map | report | 1.585 | 1.551 | 0.978 | 0.971–0.986 | 0.955, 1.002 |  |
| map.find.fresh | std::map | report | 0.324 | 0.325 | 1.004 | 1.000–1.008 | 1.004, 1.004 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.806 | 0.998 | 0.996–1.000 | 0.998, 0.998 |  |
| map.find.reused | std::map | report | 0.737 | 0.716 | 0.972 | 0.967–0.977 | 0.990, 0.954 |  |
| map.insert.fresh | std::map | report | 0.742 | 0.733 | 0.988 | 0.985–0.990 | 0.981, 0.995 |  |
| map.insert.reused | std::flat_map | report | 1.015 | 1.015 | 1.000 | 0.997–1.003 | 1.000, 1.000 |  |
| map.insert.reused | std::map | report | 1.863 | 1.861 | 0.999 | 0.992–1.006 | 0.983, 1.015 |  |
| nested.build.plain | std | report | 0.321 | 0.307 | 0.957 | 0.951–0.963 | 0.917, 0.999 |  |
| set.erase.fresh | std::set | report | 0.666 | 0.663 | 0.996 | 0.993–0.999 | 0.990, 1.002 |  |
| set.erase.reused | std::flat_set | report | 1.134 | 1.134 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |
| set.erase.reused | std::set | report | 1.313 | 1.276 | 0.972 | 0.965–0.979 | 0.939, 1.006 |  |
| set.find.fresh | std::set | report | 0.300 | 0.298 | 0.994 | 0.990–0.999 | 0.993, 0.996 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| set.find.reused | std::set | report | 0.679 | 0.683 | 1.007 | 1.004–1.011 | 0.990, 1.024 |  |
| set.insert.fresh | std::set | report | 0.647 | 0.642 | 0.992 | 0.989–0.995 | 0.989, 0.995 |  |
| set.insert.reused | std::flat_set | report | 1.111 | 1.113 | 1.002 | 1.000–1.004 | 1.003, 1.001 |  |
| set.insert.reused | std::set | report | 1.463 | 1.473 | 1.006 | 0.997–1.016 | 0.998, 1.015 |  |
| str.assign_short | std::string | report | 1.764 | 1.765 | 1.001 | 1.000–1.002 | 1.001, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.152 | 1.154 | 1.002 | 0.999–1.004 | 1.002, 1.001 |  |

worse at t1a than t0a (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; hero.mid_erase.plain / std::vector (report) 1.015; hero.mid_insert.plain / std::vector (report) 1.006

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 167 | 167.6 | 1.003 | 1.002–1.005 | 1.002, 1.005 | worse |
| hero.mid_erase.plain | 904.5 | 915 | 1.012 | 1.009–1.014 | 1.006, 1.018 | worse |
| hero.mid_insert.ord | 229.1 | 228.9 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| hero.mid_insert.plain | 1258 | 1263 | 1.004 | 1.002–1.006 | 1.006, 1.002 | worse |
| hero.push_back_grow.ord | 30.35 | 30.21 | 0.995 | 0.994–0.997 | 0.989, 1.001 |  |
| hero.push_back_grow.plain | 33.77 | 33.56 | 0.994 | 0.992–0.995 | 0.997, 0.990 |  |
| map.erase.fresh | 33 | 33.02 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| map.erase.reused | 29.67 | 29.66 | 1.000 | 0.998–1.002 | 0.997, 1.003 |  |
| map.find.fresh | 5.284 | 5.291 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| map.find.reused | 5.304 | 5.296 | 0.998 | 0.997–1.000 | 0.999, 0.998 |  |
| map.insert.fresh | 31.72 | 31.68 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| map.insert.reused | 30.16 | 30.06 | 0.997 | 0.994–0.999 | 0.998, 0.995 |  |
| nested.build.ord | 260.7 | 256.4 | 0.984 | 0.982–0.986 | 0.984, 0.983 |  |
| nested.build.plain | 265.2 | 263 | 0.992 | 0.989–0.994 | 0.993, 0.990 |  |
| reset | 24.67 | 24.18 | 0.980 | 0.976–0.984 | 0.992, 0.969 |  |
| set.erase.fresh | 29.8 | 29.79 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |
| set.erase.reused | 23.1 | 23.12 | 1.001 | 0.999–1.004 | 0.999, 1.004 |  |
| set.find.fresh | 4.88 | 4.884 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| set.find.reused | 4.879 | 4.882 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| set.insert.fresh | 28.56 | 28.52 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| set.insert.reused | 23.47 | 23.51 | 1.002 | 0.999–1.004 | 1.004, 1.000 |  |
| str.append | 1.559 | 1.562 | 1.002 | 0.997–1.007 | 0.994, 1.009 |  |
| str.assign_long | 2.946 | 2.94 | 0.998 | 0.996–1.000 | 0.994, 1.002 |  |
| str.assign_short | 0.9342 | 0.9334 | 0.999 | 0.996–1.003 | 0.997, 1.001 |  |
| str.assign_short.copy | 0.6222 | 0.6275 | 1.008 | 1.003–1.013 | 1.009, 1.008 | worse |
| str.assign_short.view | 0.9404 | 0.9422 | 1.002 | 0.998–1.006 | 1.004, 1.000 |  |
| vec.iterate | 0.03324 | 0.03324 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | 40.48 | 40.61 | 1.003 | 1.002–1.004 | 1.005, 1.002 | worse |
| vec.mid_insert | 40.66 | 40.5 | 0.996 | 0.995–0.997 | 0.996, 0.996 |  |
| vec.push_back | 0.2716 | 0.2718 | 1.001 | 1.000–1.001 | 1.001, 1.000 | worse |
| vec.push_back_grow | 0.2699 | 0.2687 | 0.996 | 0.993–0.998 | 1.002, 0.990 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.003; hero.mid_erase.plain 1.012; hero.mid_insert.plain 1.004; str.assign_short.copy 1.008; vec.mid_erase 1.003; vec.push_back 1.001

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.insert.reused (XOffset ns/op) | 1.013 | 0.997 | 0.994–0.999 | not worse |
| str.assign_short (XOffset ns/op) | 1.010 | 0.999 | 0.996–1.003 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.015 | 1.008 | 1.003–1.013 | still worse |
| str.assign_short.view (XOffset ns/op) | 1.009 | 1.002 | 0.998–1.006 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.008 | 1.003 | 1.002–1.004 | still worse |
| vec.mid_insert (XOffset ns/op) | 1.006 | 0.996 | 0.995–0.997 | not worse |
| vec.push_back (XOffset ns/op) | 1.027 | 1.001 | 1.000–1.001 | still worse |
| vec.push_back_grow (XOffset ns/op) | 1.010 | 0.996 | 0.993–0.998 | not worse |
| map.find.fresh / std::map (report) | 1.017 | 1.004 | 1.000–1.008 | not worse |
| map.find.reused / std::map (report) | 1.172 | 0.972 | 0.967–0.977 | not worse |
| map.insert.reused / std::flat_map (report) | 1.011 | 1.000 | 0.997–1.003 | not worse |
| set.find.fresh / std::flat_set (G9) | 1.004 | 0.999 | 0.997–1.001 | not worse |
| vec.mid_erase / std::vector (G9) | 1.008 | 1.000 | 0.999–1.001 | not worse |
| vec.push_back / std::vector (G9) | 1.013 | 0.994 | 0.992–0.997 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 167.23 | 165.61 | 0.990 | 225.41 | 1.001 |
| hero.mid_erase.plain | 909.99 | 898.10 | 0.987 | 224.20 | 1.013 |
| hero.mid_insert.ord | 227.62 | 226.97 | 0.997 | 298.10 | 0.997 |
| hero.mid_insert.plain | 1255.72 | 1253.33 | 0.998 | 294.19 | 1.004 |
| hero.push_back_grow.ord | 30.57 | 30.20 | 0.988 | 86.99 | 0.995 |
| hero.push_back_grow.plain | 33.60 | 33.78 | 1.005 | 86.92 | 0.994 |
| map.erase.fresh | 32.98 | 32.98 | 1.000 | 35.06 | 1.001 |
| map.erase.reused | 29.48 | 29.50 | 1.001 | 27.30 | 1.000 |
| map.find.fresh | 5.29 | 5.28 | 0.998 | 6.56 | 1.001 |
| map.find.reused | 5.30 | 5.29 | 0.998 | 6.56 | 0.999 |
| map.insert.fresh | 31.70 | 31.75 | 1.002 | 37.42 | 0.998 |
| map.insert.reused | 30.00 | 30.48 | 1.016 | 29.43 | 0.997 |
| nested.build.ord | 261.06 | 258.22 | 0.989 | 825.62 | 0.981 |
| nested.build.plain | 266.43 | 264.48 | 0.993 | 826.27 | 0.992 |
| reset | 24.89 | 24.89 | 1.000 | 131.81 | 0.975 |
| set.erase.fresh | 29.73 | 29.77 | 1.001 | 27.37 | 0.998 |
| set.erase.reused | 22.88 | 22.92 | 1.002 | 20.35 | 1.004 |
| set.find.fresh | 4.88 | 4.87 | 0.998 | 6.47 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 28.56 | 28.50 | 0.998 | 28.91 | 0.998 |
| set.insert.reused | 23.41 | 23.39 | 0.999 | 21.11 | 1.003 |
| str.append | 1.53 | 1.53 | 1.002 | 3.63 | 1.007 |
| str.assign_long | 2.93 | 2.93 | 1.000 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.93 | 1.005 | 0.52 | 0.988 |
| str.assign_short.copy | 0.60 | 0.61 | 1.015 | 0.52 | 1.018 |
| str.assign_short.view | 0.93 | 0.93 | 1.001 | 2.71 | 1.000 |
| vec.iterate | 0.03 | 0.03 | 0.998 | 0.03 | 1.000 |
| vec.mid_erase | 40.02 | 40.39 | 1.009 | 39.50 | 1.001 |
| vec.mid_insert | 40.33 | 40.55 | 1.005 | 40.61 | 0.995 |
| vec.push_back | 0.29 | 0.30 | 1.027 | 0.26 | 1.002 |
| vec.push_back_grow | 0.27 | 0.28 | 1.008 | 0.36 | 0.996 |

## analyze t0 (G9 at the anchor 30fb525c)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.119 |  1.122 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.69 |  0.992 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.47 |  1.016 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    167.31 |     225.91 |  0.741 |  0.743 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.56 |      86.38 |  0.352 |  0.353 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    909.50 |     224.61 |  4.058 |  4.070 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1256.50 |     292.31 |  4.230 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.61 |      86.26 |  0.387 |  0.388 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.151 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.70 |      37.44 |  0.847 |  0.850 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.70 |      42.89 |  0.735 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.810 |  0.812 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.63 |  0.320 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.89 |  0.987 |  0.989 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      44.19 |  0.646 |  0.647 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.753 |  0.755 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.33 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.34 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.19 |  0.669 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.018 |  1.021 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      15.87 |  1.892 |  1.898 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.40 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.078 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.70 |  1.622 |  1.633 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.107 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.30 |  1.513 |  1.525 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.15 |  0.785 |  0.789 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.132 |  1.135 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.77 |  1.331 |  1.341 | report  |
| nested.build.ord             | std            | G9     |  808 |    260.44 |     822.91 |  0.316 |  0.317 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     119.14 |  0.188 |  0.191 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     822.91 |  0.323 |  0.324 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.133 |  1.136 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.768 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.77 |  0.988 |  0.991 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.47 |  1.024 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.02 |     224.61 |  0.733 |  0.736 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.27 |      86.55 |  0.347 |  0.348 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    899.74 |     224.61 |  4.003 |  4.017 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1254.56 |     304.05 |  4.206 |  4.220 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.73 |      86.67 |  0.389 |  0.390 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.61 |  0.420 |  0.422 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.68 |  0.843 |  0.845 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      43.05 |  0.731 |  0.733 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.31 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.33 |  0.726 |  0.728 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.987 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      44.15 |  0.642 |  0.644 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.26 |  0.302 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.085 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      45.57 |  0.658 |  0.661 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.44 |      29.46 |  1.029 |  1.032 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.44 |      16.20 |  1.859 |  1.871 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.30 |  0.837 |  0.840 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      19.04 |  1.546 |  1.556 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.16 |  1.106 |  1.108 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.95 |  1.442 |  1.454 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.48 |  0.751 |  0.753 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.26 |  1.134 |  1.138 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      18.07 |  1.275 |  1.283 | report  |
| nested.build.ord             | std            | G9     |  808 |    257.81 |     821.62 |  0.313 |  0.314 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     153.64 |  0.171 |  0.174 | pass    |
| nested.build.plain           | std            | report |  808 |    264.34 |     830.72 |  0.319 |  0.320 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-12T11:18:32Z block 1 start: waited 180s load 1.45 WindowServer 1.2% remote 4
2026-11-12T11:21:37Z block 2 start: waited 180s load 1.06 WindowServer 1.4% remote 4
2026-11-12T11:24:41Z block 3 start: waited 180s load 1.02 WindowServer 1.1% remote 4
2026-11-12T11:27:46Z block 4 start: waited 180s load 1.61 WindowServer 1.1% remote 4
```
