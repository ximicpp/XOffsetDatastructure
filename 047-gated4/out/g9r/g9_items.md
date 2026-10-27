## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.706 | 0.717 | 1.016 | 1.012–1.020 | 1.025, 1.007 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.764 | 1.005 | 1.000–1.010 | 1.011, 1.000 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.365 | 0.367 | 1.007 | 1.003–1.012 | 1.007, 1.007 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.943 | 1.002 | 0.998–1.007 | 1.003, 1.002 |  |
| map.find.fresh | std::flat_map | G9 | 0.785 | 0.784 | 0.999 | 0.994–1.003 | 1.000, 0.997 |  |
| map.insert.fresh | std::flat_map | G9 | 0.872 | 0.874 | 1.003 | 0.999–1.006 | 1.004, 1.002 |  |
| nested.build.ord | std | G9 | 0.337 | 0.340 | 1.011 | 1.004–1.018 | 1.010, 1.011 | worse |
| reset | std | G9 | 0.190 | 0.184 | 0.970 | 0.963–0.977 | 0.942, 0.999 |  |
| set.erase.fresh | std::flat_set | G9 | 1.099 | 1.093 | 0.995 | 0.990–1.000 | 0.993, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.753 | 0.998 | 0.993–1.002 | 0.997, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.996 | 0.998 | 0.994–1.002 | 0.999, 0.997 |  |
| str.append | std::string | G9 | 0.493 | 0.462 | 0.937 | 0.932–0.941 | 0.937, 0.937 |  |
| str.assign_long | std::string | G9 | 0.799 | 0.797 | 0.998 | 0.994–1.002 | 1.001, 0.995 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.997–1.003 | 0.998, 1.001 |  |
| vec.iterate | std::vector | G9 | 0.995 | 0.977 | 0.981 | 0.977–0.986 | 0.977, 0.986 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.018 | 1.001 | 0.996–1.006 | 0.990, 1.012 |  |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.991 | 0.999 | 0.995–1.003 | 1.000, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.195 | 1.201 | 1.005 | 0.995–1.014 | 0.971, 1.039 |  |
| vec.push_back_grow | std::vector | G9 | 0.768 | 0.767 | 0.998 | 0.992–1.004 | 1.002, 0.994 |  |
| hero.mid_erase.plain | std::vector | report | 3.910 | 4.029 | 1.030 | 1.026–1.035 | 1.041, 1.020 | worse |
| hero.mid_insert.plain | std::vector | report | 4.216 | 4.209 | 0.998 | 0.993–1.004 | 1.006, 0.991 |  |
| hero.push_back_grow.plain | std::vector | report | 0.405 | 0.414 | 1.022 | 1.017–1.028 | 1.020, 1.024 | worse |
| map.erase.fresh | std::map | report | 0.691 | 0.696 | 1.008 | 1.003–1.013 | 1.000, 1.015 | worse |
| map.erase.reused | std::flat_map | report | 1.084 | 1.081 | 0.998 | 0.993–1.002 | 0.998, 0.997 |  |
| map.erase.reused | std::map | report | 1.409 | 1.425 | 1.011 | 1.000–1.022 | 1.027, 0.996 | worse? |
| map.find.fresh | std::map | report | 0.315 | 0.316 | 1.003 | 0.997–1.008 | 1.006, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.785 | 0.783 | 0.997 | 0.993–1.001 | 0.997, 0.997 |  |
| map.find.reused | std::map | report | 0.714 | 0.698 | 0.977 | 0.970–0.985 | 0.985, 0.969 |  |
| map.insert.fresh | std::map | report | 0.737 | 0.729 | 0.989 | 0.985–0.994 | 0.982, 0.997 |  |
| map.insert.reused | std::flat_map | report | 1.009 | 1.005 | 0.996 | 0.991–1.001 | 0.993, 0.999 |  |
| map.insert.reused | std::map | report | 1.707 | 1.657 | 0.971 | 0.961–0.980 | 0.971, 0.970 |  |
| nested.build.plain | std | report | 0.346 | 0.344 | 0.994 | 0.988–1.001 | 1.000, 0.989 |  |
| set.erase.fresh | std::set | report | 0.635 | 0.634 | 0.999 | 0.994–1.003 | 0.998, 0.999 |  |
| set.erase.reused | std::flat_set | report | 1.143 | 1.143 | 1.000 | 0.995–1.005 | 1.004, 0.995 |  |
| set.erase.reused | std::set | report | 1.149 | 1.159 | 1.008 | 0.998–1.018 | 1.008, 1.008 |  |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 1.000 | 0.994–1.006 | 1.001, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.754 | 0.999 | 0.994–1.003 | 1.001, 0.996 |  |
| set.find.reused | std::set | report | 0.670 | 0.731 | 1.091 | 1.083–1.099 | 1.117, 1.066 | worse |
| set.insert.fresh | std::set | report | 0.640 | 0.640 | 0.999 | 0.994–1.003 | 0.999, 0.999 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.083 | 1.002 | 0.998–1.006 | 0.999, 1.005 |  |
| set.insert.reused | std::set | report | 1.287 | 1.296 | 1.007 | 0.997–1.016 | 1.006, 1.008 |  |
| str.assign_short | std::string | report | 1.837 | 1.828 | 0.995 | 0.987–1.003 | 0.991, 0.999 |  |
| str.assign_short.copy | std::string | report | 1.188 | 1.193 | 1.004 | 0.998–1.010 | 1.000, 1.008 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.016; hero.push_back_grow.ord / std::vector (G9) 1.007; nested.build.ord / std (G9) 1.011; hero.mid_erase.plain / std::vector (report) 1.030; hero.push_back_grow.plain / std::vector (report) 1.022; map.erase.fresh / std::map (report) 1.008; set.find.reused / std::set (report) 1.091

above 1.01 but not worse by the rule: map.erase.reused / std::map (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.5 | 164.7 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| hero.mid_erase.plain | 892.9 | 916.1 | 1.026 | 1.022–1.030 | 1.030, 1.022 | worse |
| hero.mid_insert.ord | 230.8 | 232 | 1.005 | 1.001–1.009 | 1.010, 1.001 | worse |
| hero.mid_insert.plain | 1273 | 1274 | 1.001 | 0.998–1.004 | 1.005, 0.997 |  |
| hero.push_back_grow.ord | 33.13 | 33.25 | 1.004 | 1.000–1.008 | 1.006, 1.002 |  |
| hero.push_back_grow.plain | 36.94 | 36.95 | 1.000 | 0.996–1.005 | 1.000, 1.000 |  |
| map.erase.fresh | 33.12 | 33.14 | 1.000 | 0.996–1.004 | 1.004, 0.997 |  |
| map.erase.reused | 29.8 | 29.67 | 0.995 | 0.992–0.999 | 0.994, 0.997 |  |
| map.find.fresh | 5.162 | 5.162 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| map.find.reused | 5.177 | 5.162 | 0.997 | 0.994–1.000 | 0.999, 0.995 |  |
| map.insert.fresh | 32.31 | 32.32 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| map.insert.reused | 29.74 | 29.69 | 0.998 | 0.993–1.004 | 0.996, 1.000 |  |
| nested.build.ord | 296.2 | 293.9 | 0.992 | 0.986–0.999 | 0.988, 0.997 |  |
| nested.build.plain | 298.2 | 298 | 0.999 | 0.994–1.005 | 1.002, 0.997 |  |
| reset | 30.85 | 30.27 | 0.981 | 0.975–0.987 | 0.959, 1.004 |  |
| set.erase.fresh | 30.01 | 29.89 | 0.996 | 0.992–1.000 | 0.995, 0.996 |  |
| set.erase.reused | 23.34 | 23.38 | 1.002 | 0.997–1.006 | 1.009, 0.995 |  |
| set.find.fresh | 4.902 | 4.897 | 0.999 | 0.995–1.003 | 0.999, 0.999 |  |
| set.find.reused | 4.904 | 4.902 | 1.000 | 0.996–1.003 | 1.000, 0.999 |  |
| set.insert.fresh | 28.99 | 28.89 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| set.insert.reused | 23.03 | 22.83 | 0.991 | 0.987–0.996 | 0.993, 0.990 |  |
| str.append | 1.758 | 1.702 | 0.968 | 0.964–0.972 | 0.966, 0.970 |  |
| str.assign_long | 2.733 | 2.733 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| str.assign_short | 0.9231 | 0.9289 | 1.006 | 1.002–1.011 | 1.002, 1.011 | worse |
| str.assign_short.copy | 0.6049 | 0.6086 | 1.006 | 0.999–1.013 | 0.998, 1.014 |  |
| str.assign_short.view | 0.9341 | 0.9352 | 1.001 | 0.998–1.004 | 1.002, 1.001 |  |
| vec.iterate | 0.0341 | 0.0332 | 0.974 | 0.969–0.978 | 0.977, 0.970 |  |
| vec.mid_erase | 40.85 | 40.95 | 1.003 | 0.997–1.008 | 0.998, 1.007 |  |
| vec.mid_insert | 40.95 | 41.39 | 1.011 | 1.006–1.015 | 1.007, 1.014 | worse |
| vec.push_back | 0.3165 | 0.3238 | 1.023 | 1.011–1.035 | 1.020, 1.026 | worse |
| vec.push_back_grow | 0.2852 | 0.2854 | 1.001 | 0.993–1.009 | 0.998, 1.003 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.plain 1.026; hero.mid_insert.ord 1.005; str.assign_short 1.006; vec.mid_insert 1.011; vec.push_back 1.023

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.714 | 0.715 | 1.002 | 0.995–1.009 | 1.003, 1.002 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.760 | 0.996 | 0.992–1.000 | 1.001, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.359 | 0.368 | 1.026 | 1.021–1.032 | 1.052, 1.001 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.935 | 0.994 | 0.989–0.999 | 0.995, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.786 | 0.786 | 0.999 | 0.995–1.003 | 0.998, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.870 | 1.002 | 0.997–1.008 | 1.006, 0.999 |  |
| nested.build.ord | std | G9 | 0.338 | 0.333 | 0.986 | 0.981–0.991 | 0.964, 1.009 |  |
| reset | std | G9 | 0.186 | 0.185 | 0.992 | 0.984–1.000 | 0.943, 1.043 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.088 | 0.995 | 0.990–0.999 | 0.992, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.995–1.003 | 1.000, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 0.998 | 0.999 | 0.994–1.004 | 0.996, 1.001 |  |
| str.append | std::string | G9 | 0.426 | 0.424 | 0.995 | 0.990–1.000 | 1.025, 0.965 |  |
| str.assign_long | std::string | G9 | 0.862 | 0.859 | 0.997 | 0.993–1.000 | 0.995, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.996–1.003 | 0.999, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.035 | 1.034 | 0.999 | 0.993–1.006 | 1.003, 0.995 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.014 | 0.996 | 0.993–1.000 | 0.992, 1.001 |  |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.989 | 0.997 | 0.994–1.001 | 0.997, 0.997 |  |
| vec.push_back | std::vector | G9 | 1.122 | 1.128 | 1.005 | 1.001–1.008 | 1.005, 1.004 | worse |
| vec.push_back_grow | std::vector | G9 | 0.770 | 0.769 | 0.998 | 0.993–1.004 | 1.008, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 3.985 | 3.985 | 1.000 | 0.995–1.005 | 0.996, 1.004 |  |
| hero.mid_insert.plain | std::vector | report | 4.209 | 4.178 | 0.993 | 0.988–0.998 | 1.001, 0.984 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.401 | 0.989 | 0.983–0.995 | 0.971, 1.007 |  |
| map.erase.fresh | std::map | report | 0.695 | 0.693 | 0.997 | 0.991–1.004 | 0.994, 1.000 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.079 | 0.998 | 0.993–1.003 | 1.001, 0.996 |  |
| map.erase.reused | std::map | report | 1.248 | 1.307 | 1.048 | 1.037–1.058 | 1.047, 1.049 | worse |
| map.find.fresh | std::map | report | 0.314 | 0.314 | 1.000 | 0.995–1.006 | 0.999, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.783 | 0.786 | 1.004 | 0.999–1.008 | 1.007, 1.000 |  |
| map.find.reused | std::map | report | 0.719 | 0.689 | 0.958 | 0.951–0.965 | 0.976, 0.941 |  |
| map.insert.fresh | std::map | report | 0.739 | 0.733 | 0.992 | 0.988–0.997 | 0.991, 0.993 |  |
| map.insert.reused | std::flat_map | report | 1.005 | 1.007 | 1.001 | 0.998–1.004 | 0.996, 1.006 |  |
| map.insert.reused | std::map | report | 1.678 | 1.711 | 1.020 | 1.014–1.025 | 1.015, 1.024 | worse |
| nested.build.plain | std | report | 0.339 | 0.345 | 1.018 | 1.012–1.023 | 1.015, 1.020 | worse |
| set.erase.fresh | std::set | report | 0.635 | 0.630 | 0.991 | 0.986–0.996 | 0.982, 1.000 |  |
| set.erase.reused | std::flat_set | report | 1.148 | 1.150 | 1.002 | 0.998–1.007 | 0.997, 1.008 |  |
| set.erase.reused | std::set | report | 1.080 | 1.043 | 0.965 | 0.955–0.976 | 0.938, 0.994 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.001 | 0.995–1.006 | 0.997, 1.004 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.996–1.004 | 1.001, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.685 | 1.013 | 1.009–1.018 | 1.017, 1.010 | worse |
| set.insert.fresh | std::set | report | 0.641 | 0.640 | 0.997 | 0.991–1.003 | 1.000, 0.994 |  |
| set.insert.reused | std::flat_set | report | 1.080 | 1.081 | 1.000 | 0.995–1.006 | 1.001, 1.000 |  |
| set.insert.reused | std::set | report | 1.303 | 1.266 | 0.972 | 0.961–0.983 | 0.987, 0.957 |  |
| str.assign_short | std::string | report | 1.825 | 1.824 | 1.000 | 0.996–1.003 | 1.001, 0.999 |  |
| str.assign_short.copy | std::string | report | 1.193 | 1.188 | 0.996 | 0.990–1.001 | 0.996, 0.996 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.026; vec.push_back / std::vector (G9) 1.005; map.erase.reused / std::map (report) 1.048; map.insert.reused / std::map (report) 1.020; nested.build.plain / std (report) 1.018; set.find.reused / std::set (report) 1.013

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.8 | 165 | 1.001 | 0.995–1.008 | 0.995, 1.008 |  |
| hero.mid_erase.plain | 909 | 913 | 1.004 | 1.001–1.008 | 1.008, 1.001 | worse |
| hero.mid_insert.ord | 231.8 | 230 | 0.992 | 0.989–0.995 | 0.999, 0.985 |  |
| hero.mid_insert.plain | 1272 | 1263 | 0.993 | 0.990–0.996 | 0.997, 0.989 |  |
| hero.push_back_grow.ord | 33.22 | 33.05 | 0.995 | 0.991–0.999 | 0.994, 0.996 |  |
| hero.push_back_grow.plain | 36.78 | 36.74 | 0.999 | 0.994–1.004 | 1.001, 0.996 |  |
| map.erase.fresh | 33.18 | 33.05 | 0.996 | 0.992–1.000 | 0.997, 0.996 |  |
| map.erase.reused | 29.84 | 29.65 | 0.994 | 0.990–0.997 | 0.993, 0.994 |  |
| map.find.fresh | 5.15 | 5.156 | 1.001 | 0.997–1.005 | 1.001, 1.002 |  |
| map.find.reused | 5.173 | 5.179 | 1.001 | 0.997–1.005 | 1.004, 0.998 |  |
| map.insert.fresh | 32.46 | 32.44 | 1.000 | 0.995–1.005 | 1.002, 0.997 |  |
| map.insert.reused | 29.57 | 29.55 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| nested.build.ord | 294.4 | 293.4 | 0.997 | 0.992–1.001 | 1.002, 0.991 |  |
| nested.build.plain | 298.9 | 299.4 | 1.002 | 0.997–1.006 | 0.997, 1.006 |  |
| reset | 30.89 | 30.7 | 0.994 | 0.988–1.000 | 0.975, 1.013 |  |
| set.erase.fresh | 29.96 | 29.93 | 0.999 | 0.995–1.003 | 0.999, 0.999 |  |
| set.erase.reused | 23.45 | 23.52 | 1.003 | 0.997–1.009 | 0.999, 1.007 |  |
| set.find.fresh | 4.898 | 4.902 | 1.001 | 0.997–1.004 | 1.000, 1.001 |  |
| set.find.reused | 4.9 | 4.891 | 0.998 | 0.994–1.002 | 0.998, 0.998 |  |
| set.insert.fresh | 29.09 | 28.94 | 0.995 | 0.990–0.999 | 0.990, 0.999 |  |
| set.insert.reused | 23.02 | 23.15 | 1.006 | 0.999–1.013 | 1.007, 1.005 |  |
| str.append | 1.526 | 1.538 | 1.008 | 1.003–1.012 | 1.027, 0.989 |  |
| str.assign_long | 2.945 | 2.945 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |
| str.assign_short | 0.9274 | 0.9243 | 0.997 | 0.994–1.000 | 1.004, 0.989 |  |
| str.assign_short.copy | 0.6097 | 0.6145 | 1.008 | 1.003–1.013 | 1.017, 0.999 |  |
| str.assign_short.view | 0.9383 | 0.9365 | 0.998 | 0.994–1.002 | 1.002, 0.994 |  |
| vec.iterate | 0.03338 | 0.03341 | 1.001 | 0.997–1.005 | 0.992, 1.010 |  |
| vec.mid_erase | 40.69 | 40.45 | 0.994 | 0.991–0.997 | 0.996, 0.992 |  |
| vec.mid_insert | 40.54 | 40.56 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| vec.push_back | 0.2724 | 0.2727 | 1.001 | 0.999–1.004 | 1.001, 1.002 |  |
| vec.push_back_grow | 0.2772 | 0.2754 | 0.993 | 0.989–0.998 | 0.999, 0.987 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.004

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.plain (XOffset ns/op) | 1.026 | 1.004 | 1.001–1.008 | still worse |
| hero.mid_insert.ord (XOffset ns/op) | 1.005 | 0.992 | 0.989–0.995 | not worse |
| str.assign_short (XOffset ns/op) | 1.006 | 0.997 | 0.994–1.000 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.011 | 1.001 | 0.997–1.004 | not worse |
| vec.push_back (XOffset ns/op) | 1.023 | 1.001 | 0.999–1.004 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.016 | 1.002 | 0.995–1.009 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.030 | 1.000 | 0.995–1.005 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.007 | 1.026 | 1.021–1.032 | still worse |
| hero.push_back_grow.plain / std::vector (report) | 1.022 | 0.989 | 0.983–0.995 | not worse |
| map.erase.fresh / std::map (report) | 1.008 | 0.997 | 0.991–1.004 | not worse |
| nested.build.ord / std (G9) | 1.011 | 0.986 | 0.981–0.991 | not worse |
| set.find.reused / std::set (report) | 1.091 | 1.013 | 1.009–1.018 | still worse |
