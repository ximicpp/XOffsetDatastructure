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
