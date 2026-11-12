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
