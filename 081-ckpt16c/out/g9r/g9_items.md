## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.730 | 1.005 | 1.002–1.008 | 1.001, 1.009 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.758 | 0.761 | 1.003 | 0.999–1.007 | 1.000, 1.007 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.367 | 0.368 | 1.005 | 1.002–1.007 | 0.981, 1.029 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.935 | 0.993 | 0.991–0.996 | 0.992, 0.994 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.806 | 0.998 | 0.995–1.000 | 0.999, 0.996 |  |
| map.insert.fresh | std::flat_map | G9 | 0.871 | 0.875 | 1.004 | 1.001–1.007 | 1.007, 1.001 | worse |
| nested.build.ord | std | G9 | 0.324 | 0.324 | 1.001 | 0.997–1.006 | 0.990, 1.013 |  |
| reset | std | G9 | 0.250 | 0.248 | 0.995 | 0.987–1.004 | 0.943, 1.050 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.086 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 1.002 | 0.997 | 0.995 | 0.993–0.998 | 1.000, 0.990 |  |
| str.append | std::string | G9 | 0.416 | 0.422 | 1.015 | 1.010–1.019 | 1.006, 1.023 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.864 | 0.997 | 0.995–0.999 | 1.000, 0.994 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| vec.iterate | std::vector | G9 | 0.999 | 1.001 | 1.002 | 0.998–1.007 | 1.004, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.021 | 1.000 | 0.998–1.003 | 0.999, 1.001 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.976 | 1.022 | 1.019–1.025 | 1.023, 1.021 | worse |
| vec.push_back | std::vector | G9 | 1.116 | 1.111 | 0.995 | 0.991–0.999 | 1.004, 0.987 |  |
| vec.push_back_grow | std::vector | G9 | 0.771 | 0.769 | 0.998 | 0.994–1.001 | 0.990, 1.005 |  |
| hero.mid_erase.plain | std::vector | report | 4.058 | 4.080 | 1.005 | 1.002–1.009 | 1.004, 1.007 | worse |
| hero.mid_insert.plain | std::vector | report | 4.226 | 4.240 | 1.003 | 0.999–1.007 | 0.998, 1.009 |  |
| hero.push_back_grow.plain | std::vector | report | 0.404 | 0.405 | 1.002 | 0.999–1.004 | 0.995, 1.008 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.739 | 0.993 | 0.991–0.996 | 0.988, 0.998 |  |
| map.erase.reused | std::flat_map | report | 1.086 | 1.081 | 0.995 | 0.991–0.999 | 0.991, 0.998 |  |
| map.erase.reused | std::map | report | 1.662 | 1.680 | 1.011 | 1.004–1.017 | 0.998, 1.024 | worse? |
| map.find.fresh | std::map | report | 0.325 | 0.325 | 1.001 | 0.997–1.005 | 1.002, 1.000 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.807 | 1.000 | 0.998–1.003 | 0.998, 1.002 |  |
| map.find.reused | std::map | report | 0.714 | 0.753 | 1.055 | 1.051–1.059 | 1.051, 1.059 | worse |
| map.insert.fresh | std::map | report | 0.750 | 0.742 | 0.989 | 0.986–0.992 | 0.981, 0.996 |  |
| map.insert.reused | std::flat_map | report | 1.007 | 1.005 | 0.998 | 0.995–1.001 | 0.999, 0.996 |  |
| map.insert.reused | std::map | report | 1.820 | 1.789 | 0.983 | 0.976–0.989 | 0.948, 1.019 |  |
| nested.build.plain | std | report | 0.334 | 0.334 | 1.001 | 0.997–1.005 | 0.979, 1.023 |  |
| set.erase.fresh | std::set | report | 0.674 | 0.672 | 0.997 | 0.995–1.000 | 0.992, 1.003 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.139 | 1.001 | 0.997–1.005 | 0.999, 1.003 |  |
| set.erase.reused | std::set | report | 1.300 | 1.345 | 1.034 | 1.025–1.044 | 1.003, 1.067 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.298 | 0.996 | 0.992–1.001 | 0.997, 0.995 |  |
| set.find.reused | std::flat_set | report | 0.753 | 0.755 | 1.002 | 0.999–1.006 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.662 | 0.620 | 0.936 | 0.932–0.941 | 0.937, 0.935 |  |
| set.insert.fresh | std::set | report | 0.657 | 0.645 | 0.983 | 0.980–0.986 | 0.972, 0.994 |  |
| set.insert.reused | std::flat_set | report | 1.084 | 1.081 | 0.997 | 0.994–1.001 | 0.997, 0.997 |  |
| set.insert.reused | std::set | report | 1.437 | 1.410 | 0.982 | 0.973–0.991 | 0.945, 1.020 |  |
| str.assign_short | std::string | report | 1.763 | 1.767 | 1.002 | 0.998–1.006 | 1.004, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.148 | 0.997 | 0.993–1.002 | 0.998, 0.997 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.004; str.append / std::string (G9) 1.015; vec.mid_insert / std::vector (G9) 1.022; hero.mid_erase.plain / std::vector (report) 1.005; map.find.reused / std::map (report) 1.055; set.erase.reused / std::set (report) 1.034

above 1.01 but not worse by the rule: map.erase.reused / std::map (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164 | 164.5 | 1.003 | 1.001–1.005 | 1.000, 1.006 | worse |
| hero.mid_erase.plain | 916.5 | 913.4 | 0.997 | 0.994–0.999 | 0.994, 0.999 |  |
| hero.mid_insert.ord | 228.1 | 228.5 | 1.002 | 0.999–1.005 | 1.003, 1.001 |  |
| hero.mid_insert.plain | 1269 | 1267 | 0.998 | 0.996–1.000 | 0.994, 1.002 |  |
| hero.push_back_grow.ord | 32.01 | 31.92 | 0.997 | 0.996–0.999 | 0.992, 1.002 |  |
| hero.push_back_grow.plain | 35.26 | 35.14 | 0.996 | 0.995–0.998 | 0.998, 0.995 |  |
| map.erase.fresh | 33.06 | 33 | 0.998 | 0.996–0.999 | 0.998, 0.998 |  |
| map.erase.reused | 29.77 | 29.75 | 0.999 | 0.997–1.002 | 0.997, 1.002 |  |
| map.find.fresh | 5.29 | 5.283 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| map.find.reused | 5.297 | 5.304 | 1.001 | 1.000–1.003 | 0.999, 1.003 |  |
| map.insert.fresh | 32.32 | 32.21 | 0.996 | 0.995–0.998 | 1.000, 0.993 |  |
| map.insert.reused | 29.67 | 29.54 | 0.996 | 0.993–0.999 | 0.999, 0.992 |  |
| nested.build.ord | 271.3 | 272.6 | 1.005 | 1.001–1.008 | 1.011, 0.999 |  |
| nested.build.plain | 278 | 279.4 | 1.005 | 1.003–1.008 | 1.005, 1.006 | worse |
| reset | 30.48 | 29.56 | 0.970 | 0.966–0.974 | 0.959, 0.980 |  |
| set.erase.fresh | 29.8 | 29.8 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| set.erase.reused | 23.26 | 23.26 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| set.find.fresh | 4.888 | 4.88 | 0.998 | 0.997–1.000 | 0.997, 0.999 |  |
| set.find.reused | 4.883 | 4.885 | 1.001 | 0.998–1.003 | 0.998, 1.003 |  |
| set.insert.fresh | 29.01 | 28.9 | 0.996 | 0.994–0.998 | 1.001, 0.991 |  |
| set.insert.reused | 22.99 | 22.83 | 0.993 | 0.991–0.995 | 0.998, 0.989 |  |
| str.append | 1.566 | 1.563 | 0.998 | 0.993–1.002 | 0.994, 1.001 |  |
| str.assign_long | 2.961 | 2.938 | 0.992 | 0.990–0.995 | 0.991, 0.993 |  |
| str.assign_short | 0.9468 | 0.9315 | 0.984 | 0.979–0.988 | 0.974, 0.994 |  |
| str.assign_short.copy | 0.634 | 0.6225 | 0.982 | 0.977–0.987 | 0.977, 0.987 |  |
| str.assign_short.view | 0.9475 | 0.9382 | 0.990 | 0.987–0.994 | 0.981, 0.999 |  |
| vec.iterate | 0.03321 | 0.0332 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| vec.mid_erase | 40.43 | 40.44 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| vec.mid_insert | 40.41 | 40.56 | 1.004 | 1.002–1.005 | 1.002, 1.005 | worse |
| vec.push_back | 0.2927 | 0.2902 | 0.991 | 0.979–1.003 | 0.992, 0.991 |  |
| vec.push_back_grow | 0.2767 | 0.2769 | 1.001 | 0.995–1.007 | 1.001, 1.000 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.003; nested.build.plain 1.005; vec.mid_insert 1.004

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.732 | 0.727 | 0.993 | 0.990–0.996 | 0.990, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.761 | 0.760 | 0.999 | 0.996–1.003 | 0.994, 1.005 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.363 | 1.000 | 0.997–1.003 | 0.996, 1.004 |  |
| map.erase.fresh | std::flat_map | G9 | 0.939 | 0.942 | 1.003 | 1.000–1.006 | 1.003, 1.004 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.809 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.865 | 0.869 | 1.005 | 1.002–1.008 | 1.005, 1.004 | worse |
| nested.build.ord | std | G9 | 0.319 | 0.319 | 0.999 | 0.994–1.004 | 1.013, 0.985 |  |
| reset | std | G9 | 0.249 | 0.246 | 0.989 | 0.981–0.998 | 0.986, 0.992 |  |
| set.erase.fresh | std::flat_set | G9 | 1.093 | 1.090 | 0.997 | 0.994–1.001 | 0.998, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.997 | 0.999 | 0.996–1.002 | 0.997, 1.002 |  |
| str.append | std::string | G9 | 0.426 | 0.429 | 1.007 | 1.001–1.012 | 0.987, 1.027 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.865 | 0.999 | 0.996–1.001 | 1.000, 0.997 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.004 | 1.000–1.008 | 1.002, 1.006 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.995–1.003 | 1.000, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.024 | 1.005 | 1.003–1.007 | 1.002, 1.008 | worse |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.957 | 0.997 | 0.994–1.000 | 0.999, 0.994 |  |
| vec.push_back | std::vector | G9 | 1.122 | 1.122 | 1.000 | 0.997–1.003 | 0.995, 1.005 |  |
| vec.push_back_grow | std::vector | G9 | 0.770 | 0.767 | 0.996 | 0.992–1.000 | 0.992, 1.000 |  |
| hero.mid_erase.plain | std::vector | report | 4.064 | 4.048 | 0.996 | 0.993–0.999 | 1.001, 0.991 |  |
| hero.mid_insert.plain | std::vector | report | 4.175 | 4.155 | 0.995 | 0.991–0.999 | 0.986, 1.004 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.403 | 0.993 | 0.990–0.996 | 0.995, 0.991 |  |
| map.erase.fresh | std::map | report | 0.741 | 0.746 | 1.007 | 1.004–1.009 | 1.000, 1.014 |  |
| map.erase.reused | std::flat_map | report | 1.081 | 1.083 | 1.002 | 0.999–1.006 | 1.003, 1.002 |  |
| map.erase.reused | std::map | report | 1.642 | 1.646 | 1.002 | 0.996–1.009 | 0.993, 1.012 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.995 | 0.991–0.999 | 1.000, 0.991 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.998–1.003 | 0.999, 1.002 |  |
| map.find.reused | std::map | report | 0.715 | 0.734 | 1.026 | 1.020–1.032 | 1.032, 1.020 | worse |
| map.insert.fresh | std::map | report | 0.749 | 0.729 | 0.974 | 0.970–0.978 | 0.964, 0.984 |  |
| map.insert.reused | std::flat_map | report | 1.005 | 1.003 | 0.997 | 0.995–1.000 | 0.996, 0.998 |  |
| map.insert.reused | std::map | report | 1.807 | 1.809 | 1.001 | 0.994–1.008 | 1.025, 0.978 |  |
| nested.build.plain | std | report | 0.329 | 0.331 | 1.004 | 0.999–1.008 | 1.025, 0.983 |  |
| set.erase.fresh | std::set | report | 0.668 | 0.672 | 1.007 | 1.002–1.011 | 0.995, 1.018 |  |
| set.erase.reused | std::flat_set | report | 1.142 | 1.139 | 0.997 | 0.994–1.001 | 0.998, 0.997 |  |
| set.erase.reused | std::set | report | 1.344 | 1.330 | 0.989 | 0.981–0.998 | 0.976, 1.003 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.000 | 0.995–1.004 | 0.996, 1.003 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.756 | 0.998 | 0.996–1.001 | 1.000, 0.996 |  |
| set.find.reused | std::set | report | 0.677 | 0.677 | 1.000 | 0.996–1.004 | 0.994, 1.005 |  |
| set.insert.fresh | std::set | report | 0.649 | 0.647 | 0.997 | 0.994–1.000 | 0.999, 0.995 |  |
| set.insert.reused | std::flat_set | report | 1.079 | 1.083 | 1.004 | 1.000–1.007 | 1.001, 1.007 | worse |
| set.insert.reused | std::set | report | 1.414 | 1.395 | 0.987 | 0.977–0.997 | 1.001, 0.973 |  |
| str.assign_short | std::string | report | 1.766 | 1.764 | 0.999 | 0.996–1.002 | 0.999, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.152 | 1.002 | 0.999–1.006 | 0.999, 1.005 |  |

worse at t1a than t0a (by the rule): map.insert.fresh / std::flat_map (G9) 1.005; vec.mid_erase / std::vector (G9) 1.005; map.find.reused / std::map (report) 1.026; set.insert.reused / std::flat_set (report) 1.004

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.9 | 164.1 | 0.995 | 0.993–0.997 | 0.993, 0.997 |  |
| hero.mid_erase.plain | 912.3 | 915.3 | 1.003 | 1.000–1.006 | 1.006, 1.000 | worse |
| hero.mid_insert.ord | 229.7 | 228.9 | 0.996 | 0.995–0.998 | 0.997, 0.996 |  |
| hero.mid_insert.plain | 1251 | 1254 | 1.002 | 0.999–1.005 | 0.994, 1.010 |  |
| hero.push_back_grow.ord | 31.88 | 32.04 | 1.005 | 1.003–1.007 | 1.008, 1.002 | worse |
| hero.push_back_grow.plain | 35.29 | 35.54 | 1.007 | 1.005–1.009 | 1.007, 1.006 | worse |
| map.erase.fresh | 33.03 | 33.02 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| map.erase.reused | 29.66 | 29.73 | 1.002 | 1.000–1.005 | 1.002, 1.002 |  |
| map.find.fresh | 5.292 | 5.295 | 1.001 | 0.998–1.003 | 1.002, 0.999 |  |
| map.find.reused | 5.307 | 5.303 | 0.999 | 0.998–1.001 | 0.998, 1.000 |  |
| map.insert.fresh | 32.28 | 32.29 | 1.001 | 0.998–1.003 | 1.000, 1.001 |  |
| map.insert.reused | 29.57 | 29.57 | 1.000 | 0.999–1.002 | 1.001, 0.999 |  |
| nested.build.ord | 268.3 | 269 | 1.003 | 0.999–1.006 | 1.005, 1.001 |  |
| nested.build.plain | 277.1 | 276.7 | 0.999 | 0.995–1.002 | 0.999, 0.998 |  |
| reset | 30.11 | 28.94 | 0.961 | 0.956–0.966 | 0.989, 0.934 |  |
| set.erase.fresh | 29.78 | 29.82 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| set.erase.reused | 23.29 | 23.27 | 0.999 | 0.996–1.003 | 0.999, 0.999 |  |
| set.find.fresh | 4.882 | 4.88 | 1.000 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.89 | 4.881 | 0.998 | 0.996–1.000 | 0.999, 0.997 |  |
| set.insert.fresh | 28.86 | 28.92 | 1.002 | 1.001–1.003 | 1.003, 1.001 | worse |
| set.insert.reused | 22.77 | 22.85 | 1.004 | 1.001–1.006 | 1.002, 1.005 | worse |
| str.append | 1.565 | 1.583 | 1.011 | 1.007–1.016 | 1.009, 1.014 | worse |
| str.assign_long | 2.948 | 2.969 | 1.007 | 1.004–1.010 | 1.009, 1.005 | worse |
| str.assign_short | 0.9472 | 0.9562 | 1.010 | 1.005–1.014 | 1.010, 1.010 | worse |
| str.assign_short.copy | 0.6278 | 0.637 | 1.015 | 1.009–1.020 | 1.019, 1.011 | worse |
| str.assign_short.view | 0.9537 | 0.9537 | 1.000 | 0.995–1.005 | 1.012, 0.988 |  |
| vec.iterate | 0.03325 | 0.03332 | 1.002 | 0.999–1.005 | 1.000, 1.005 |  |
| vec.mid_erase | 40.51 | 40.64 | 1.003 | 1.002–1.005 | 1.003, 1.003 | worse |
| vec.mid_insert | 40.71 | 40.7 | 1.000 | 0.998–1.002 | 0.998, 1.001 |  |
| vec.push_back | 0.2725 | 0.2719 | 0.998 | 0.995–1.000 | 0.999, 0.996 |  |
| vec.push_back_grow | 0.2715 | 0.2712 | 0.999 | 0.996–1.002 | 0.998, 1.000 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.003; hero.push_back_grow.ord 1.005; hero.push_back_grow.plain 1.007; set.insert.fresh 1.002; set.insert.reused 1.004; str.append 1.011; str.assign_long 1.007; str.assign_short 1.010; str.assign_short.copy 1.015; vec.mid_erase 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.003 | 0.995 | 0.993–0.997 | not worse |
| nested.build.plain (XOffset ns/op) | 1.005 | 0.999 | 0.995–1.002 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.004 | 1.000 | 0.998–1.002 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.993 | 0.990–0.996 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.005 | 0.996 | 0.993–0.999 | not worse |
| map.find.reused / std::map (report) | 1.055 | 1.026 | 1.020–1.032 | still worse |
| map.insert.fresh / std::flat_map (G9) | 1.004 | 1.005 | 1.002–1.008 | still worse |
| set.erase.reused / std::set (report) | 1.034 | 0.989 | 0.981–0.998 | not worse |
| str.append / std::string (G9) | 1.015 | 1.007 | 1.001–1.012 | not worse |
| vec.mid_insert / std::vector (G9) | 1.022 | 0.997 | 0.994–1.000 | not worse |
