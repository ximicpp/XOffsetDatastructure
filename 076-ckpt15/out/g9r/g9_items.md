## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.724 | 0.725 | 1.001 | 0.999–1.004 | 0.996, 1.007 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.762 | 0.754 | 0.991 | 0.987–0.994 | 0.992, 0.989 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.368 | 0.363 | 0.985 | 0.983–0.988 | 0.979, 0.991 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.938 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.002 | 1.000–1.004 | 1.001, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.869 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| nested.build.ord | std | G9 | 0.339 | 0.327 | 0.966 | 0.957–0.975 | 0.920, 1.015 |  |
| reset | std | G9 | 0.256 | 0.228 | 0.889 | 0.877–0.900 | 0.796, 0.992 |  |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.084 | 0.996 | 0.994–0.998 | 0.994, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.754 | 0.755 | 1.002 | 0.998–1.005 | 1.003, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.999 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| str.append | std::string | G9 | 0.416 | 0.426 | 1.025 | 1.020–1.030 | 1.026, 1.024 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.800 | 0.924 | 0.922–0.927 | 0.924, 0.925 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.344 | 1.003 | 1.001–1.006 | 1.003, 1.004 | worse |
| vec.iterate | std::vector | G9 | 0.999 | 1.000 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.019 | 0.999 | 0.996–1.001 | 0.999, 0.998 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.958 | 1.003 | 1.000–1.006 | 1.002, 1.005 | worse |
| vec.push_back | std::vector | G9 | 1.114 | 1.105 | 0.992 | 0.989–0.995 | 0.994, 0.990 |  |
| vec.push_back_grow | std::vector | G9 | 0.768 | 0.769 | 1.001 | 0.997–1.005 | 1.005, 0.997 |  |
| hero.mid_erase.plain | std::vector | report | 4.061 | 4.017 | 0.989 | 0.986–0.992 | 0.988, 0.990 |  |
| hero.mid_insert.plain | std::vector | report | 4.242 | 4.188 | 0.987 | 0.984–0.991 | 0.993, 0.982 |  |
| hero.push_back_grow.plain | std::vector | report | 0.407 | 0.402 | 0.988 | 0.986–0.991 | 0.986, 0.991 |  |
| map.erase.fresh | std::map | report | 0.747 | 0.739 | 0.989 | 0.986–0.992 | 0.978, 0.999 |  |
| map.erase.reused | std::flat_map | report | 1.073 | 1.080 | 1.006 | 0.995–1.017 | 1.017, 0.995 |  |
| map.erase.reused | std::map | report | 1.625 | 1.611 | 0.991 | 0.980–1.002 | 1.010, 0.972 |  |
| map.find.fresh | std::map | report | 0.324 | 0.326 | 1.005 | 1.001–1.009 | 1.002, 1.007 | worse |
| map.find.reused | std::flat_map | report | 0.808 | 0.809 | 1.002 | 1.000–1.004 | 1.000, 1.003 |  |
| map.find.reused | std::map | report | 0.707 | 0.697 | 0.986 | 0.981–0.991 | 0.981, 0.990 |  |
| map.insert.fresh | std::map | report | 0.743 | 0.746 | 1.004 | 1.001–1.008 | 1.008, 1.000 | worse |
| map.insert.reused | std::flat_map | report | 1.005 | 1.001 | 0.996 | 0.993–1.000 | 0.997, 0.995 |  |
| map.insert.reused | std::map | report | 1.836 | 1.811 | 0.987 | 0.982–0.991 | 0.978, 0.995 |  |
| nested.build.plain | std | report | 0.338 | 0.337 | 0.998 | 0.995–1.001 | 0.986, 1.009 |  |
| set.erase.fresh | std::set | report | 0.680 | 0.663 | 0.976 | 0.973–0.979 | 0.962, 0.990 |  |
| set.erase.reused | std::flat_set | report | 1.140 | 1.137 | 0.998 | 0.993–1.002 | 0.996, 1.000 |  |
| set.erase.reused | std::set | report | 1.329 | 1.341 | 1.010 | 1.002–1.018 | 0.995, 1.025 |  |
| set.find.fresh | std::set | report | 0.300 | 0.297 | 0.993 | 0.988–0.998 | 0.993, 0.993 |  |
| set.find.reused | std::flat_set | report | 0.761 | 0.756 | 0.993 | 0.986–1.000 | 0.984, 1.003 |  |
| set.find.reused | std::set | report | 0.685 | 0.764 | 1.115 | 1.104–1.126 | 1.106, 1.125 | worse |
| set.insert.fresh | std::set | report | 0.654 | 0.654 | 0.999 | 0.997–1.002 | 0.997, 1.002 |  |
| set.insert.reused | std::flat_set | report | 1.079 | 1.079 | 0.999 | 0.993–1.005 | 1.000, 0.999 |  |
| set.insert.reused | std::set | report | 1.372 | 1.421 | 1.035 | 1.021–1.050 | 1.072, 1.000 | worse? |
| str.assign_short | std::string | report | 1.761 | 1.771 | 1.006 | 1.001–1.010 | 1.008, 1.003 | worse |
| str.assign_short.copy | std::string | report | 1.149 | 1.150 | 1.001 | 0.997–1.004 | 0.997, 1.004 |  |

worse at t1 than t0 (by the rule): str.append / std::string (G9) 1.025; str.assign_short.view / std::string (G9) 1.003; vec.mid_insert / std::vector (G9) 1.003; map.find.fresh / std::map (report) 1.005; map.insert.fresh / std::map (report) 1.004; set.find.reused / std::set (report) 1.115; str.assign_short / std::string (report) 1.006

above 1.01 but not worse by the rule: set.insert.reused / std::set (report) 1.035

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.5 | 163.2 | 0.998 | 0.997–0.999 | 0.996, 1.000 |  |
| hero.mid_erase.plain | 914.2 | 910.6 | 0.996 | 0.993–0.999 | 0.993, 0.999 |  |
| hero.mid_insert.ord | 227.9 | 227.1 | 0.997 | 0.995–0.999 | 0.998, 0.995 |  |
| hero.mid_insert.plain | 1264 | 1261 | 0.998 | 0.995–1.000 | 1.001, 0.995 |  |
| hero.push_back_grow.ord | 31.81 | 31.84 | 1.001 | 0.999–1.002 | 0.995, 1.006 |  |
| hero.push_back_grow.plain | 35.13 | 35.31 | 1.005 | 1.003–1.007 | 1.003, 1.007 | worse |
| map.erase.fresh | 33.02 | 33.05 | 1.001 | 1.000–1.003 | 1.001, 1.001 |  |
| map.erase.reused | 31.42 | 29.61 | 0.942 | 0.929–0.956 | 0.891, 0.997 |  |
| map.find.fresh | 5.287 | 5.287 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.find.reused | 5.298 | 5.311 | 1.002 | 1.001–1.004 | 1.002, 1.003 | worse |
| map.insert.fresh | 32.23 | 32.21 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.reused | 29.65 | 29.5 | 0.995 | 0.992–0.998 | 0.992, 0.998 |  |
| nested.build.ord | 302.4 | 271.7 | 0.899 | 0.878–0.920 | 0.800, 1.010 |  |
| nested.build.plain | 279 | 277.9 | 0.996 | 0.994–0.998 | 0.991, 1.001 |  |
| reset | 30.49 | 28.98 | 0.951 | 0.942–0.959 | 0.915, 0.988 |  |
| set.erase.fresh | 29.92 | 29.76 | 0.995 | 0.992–0.997 | 0.992, 0.998 |  |
| set.erase.reused | 23.43 | 23.18 | 0.989 | 0.983–0.996 | 0.980, 0.999 |  |
| set.find.fresh | 4.903 | 4.89 | 0.997 | 0.995–1.000 | 0.993, 1.002 |  |
| set.find.reused | 5.479 | 4.885 | 0.891 | 0.869–0.915 | 0.793, 1.002 |  |
| set.insert.fresh | 28.94 | 28.86 | 0.997 | 0.995–0.999 | 0.995, 0.999 |  |
| set.insert.reused | 25.21 | 22.8 | 0.904 | 0.888–0.921 | 0.819, 0.998 |  |
| str.append | 1.554 | 1.577 | 1.015 | 1.010–1.019 | 1.012, 1.018 | worse |
| str.assign_long | 2.95 | 2.749 | 0.932 | 0.929–0.935 | 0.928, 0.935 |  |
| str.assign_short | 0.9424 | 0.9574 | 1.016 | 1.010–1.021 | 1.015, 1.017 | worse |
| str.assign_short.copy | 0.6259 | 0.6346 | 1.014 | 1.009–1.019 | 0.999, 1.029 | worse? |
| str.assign_short.view | 0.9483 | 0.9554 | 1.008 | 1.003–1.012 | 1.002, 1.013 | worse |
| vec.iterate | 0.03319 | 0.03314 | 0.998 | 0.997–0.999 | 0.997, 0.999 |  |
| vec.mid_erase | 40.33 | 40.27 | 0.999 | 0.996–1.001 | 0.997, 1.000 |  |
| vec.mid_insert | 40.47 | 40.42 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| vec.push_back | 0.2881 | 0.2917 | 1.013 | 1.002–1.024 | 1.013, 1.012 | worse |
| vec.push_back_grow | 0.2748 | 0.2788 | 1.015 | 1.009–1.021 | 1.017, 1.012 | worse |

XOffset slower at t1 than t0 (worse by the rule): hero.push_back_grow.plain 1.005; map.find.reused 1.002; str.append 1.015; str.assign_short 1.016; str.assign_short.view 1.008; vec.push_back 1.013; vec.push_back_grow 1.015

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.723 | 0.995 | 0.991–0.999 | 0.991, 0.999 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.762 | 1.003 | 1.000–1.007 | 0.999, 1.007 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.353 | 0.364 | 1.033 | 1.028–1.037 | 1.020, 1.046 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.941 | 1.003 | 1.000–1.006 | 0.999, 1.007 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.998–1.004 | 0.999, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.870 | 1.002 | 1.000–1.005 | 1.000, 1.005 |  |
| nested.build.ord | std | G9 | 0.326 | 0.323 | 0.993 | 0.990–0.997 | 0.992, 0.995 |  |
| reset | std | G9 | 0.248 | 0.222 | 0.897 | 0.885–0.909 | 0.923, 0.872 |  |
| set.erase.fresh | std::flat_set | G9 | 1.092 | 1.090 | 0.998 | 0.994–1.002 | 0.997, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 1.000 | 1.001 | 0.998–1.003 | 1.001, 1.001 |  |
| str.append | std::string | G9 | 0.432 | 0.422 | 0.978 | 0.973–0.983 | 0.960, 0.997 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.866 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.342 | 0.998 | 0.995–1.001 | 1.000, 0.996 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.019 | 1.000 | 0.998–1.002 | 1.002, 0.999 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.979 | 1.020 | 1.016–1.024 | 1.017, 1.023 | worse |
| vec.push_back | std::vector | G9 | 1.118 | 1.118 | 1.000 | 0.997–1.003 | 1.003, 0.997 |  |
| vec.push_back_grow | std::vector | G9 | 0.774 | 0.767 | 0.991 | 0.987–0.995 | 0.994, 0.987 |  |
| hero.mid_erase.plain | std::vector | report | 4.015 | 4.032 | 1.004 | 0.999–1.009 | 1.009, 0.999 |  |
| hero.mid_insert.plain | std::vector | report | 4.147 | 4.183 | 1.009 | 1.005–1.012 | 1.003, 1.014 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.391 | 0.408 | 1.043 | 1.038–1.047 | 1.030, 1.056 | worse |
| map.erase.fresh | std::map | report | 0.729 | 0.739 | 1.014 | 1.010–1.017 | 1.019, 1.009 | worse |
| map.erase.reused | std::flat_map | report | 1.081 | 1.082 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| map.erase.reused | std::map | report | 1.621 | 1.577 | 0.973 | 0.966–0.980 | 0.969, 0.976 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.995 | 0.991–0.999 | 0.996, 0.995 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.002 | 0.999–1.004 | 1.001, 1.002 |  |
| map.find.reused | std::map | report | 0.736 | 0.741 | 1.007 | 1.003–1.011 | 1.013, 1.001 | worse |
| map.insert.fresh | std::map | report | 0.739 | 0.750 | 1.016 | 1.012–1.019 | 1.003, 1.028 | worse |
| map.insert.reused | std::flat_map | report | 1.010 | 1.001 | 0.991 | 0.989–0.994 | 0.994, 0.988 |  |
| map.insert.reused | std::map | report | 1.844 | 1.826 | 0.990 | 0.985–0.995 | 0.966, 1.015 |  |
| nested.build.plain | std | report | 0.334 | 0.337 | 1.007 | 1.004–1.011 | 1.008, 1.006 | worse |
| set.erase.fresh | std::set | report | 0.670 | 0.667 | 0.995 | 0.991–0.999 | 1.005, 0.985 |  |
| set.erase.reused | std::flat_set | report | 1.143 | 1.144 | 1.001 | 0.997–1.004 | 1.000, 1.002 |  |
| set.erase.reused | std::set | report | 1.356 | 1.309 | 0.966 | 0.958–0.973 | 0.966, 0.966 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.997 | 0.993–1.002 | 0.995, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.find.reused | std::set | report | 0.689 | 0.690 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| set.insert.fresh | std::set | report | 0.647 | 0.655 | 1.011 | 1.008–1.015 | 0.998, 1.024 | worse? |
| set.insert.reused | std::flat_set | report | 1.082 | 1.080 | 0.999 | 0.996–1.002 | 1.000, 0.998 |  |
| set.insert.reused | std::set | report | 1.463 | 1.459 | 0.998 | 0.989–1.006 | 0.999, 0.997 |  |
| str.assign_short | std::string | report | 1.765 | 1.768 | 1.002 | 0.999–1.005 | 1.005, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.150 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |

worse at t1a than t0a (by the rule): hero.push_back_grow.ord / std::vector (G9) 1.033; vec.mid_insert / std::vector (G9) 1.020; hero.mid_insert.plain / std::vector (report) 1.009; hero.push_back_grow.plain / std::vector (report) 1.043; map.erase.fresh / std::map (report) 1.014; map.find.reused / std::map (report) 1.007; map.insert.fresh / std::map (report) 1.016; nested.build.plain / std (report) 1.007

above 1.01 but not worse by the rule: set.insert.fresh / std::set (report) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 165.6 | 163.9 | 0.990 | 0.986–0.994 | 0.990, 0.990 |  |
| hero.mid_erase.plain | 906.5 | 914.8 | 1.009 | 1.006–1.012 | 1.016, 1.003 | worse |
| hero.mid_insert.ord | 229.8 | 228.6 | 0.995 | 0.993–0.997 | 0.995, 0.995 |  |
| hero.mid_insert.plain | 1245 | 1261 | 1.013 | 1.010–1.015 | 1.011, 1.015 | worse |
| hero.push_back_grow.ord | 31.78 | 31.64 | 0.995 | 0.993–0.998 | 0.991, 1.000 |  |
| hero.push_back_grow.plain | 35.26 | 35.31 | 1.001 | 0.999–1.004 | 0.994, 1.009 |  |
| map.erase.fresh | 33.08 | 32.99 | 0.997 | 0.996–0.999 | 0.999, 0.996 |  |
| map.erase.reused | 29.63 | 29.73 | 1.003 | 1.001–1.006 | 1.004, 1.003 | worse |
| map.find.fresh | 5.308 | 5.288 | 0.996 | 0.994–0.999 | 0.998, 0.995 |  |
| map.find.reused | 5.308 | 5.307 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.fresh | 32.22 | 32.28 | 1.002 | 1.000–1.004 | 1.002, 1.002 |  |
| map.insert.reused | 29.61 | 29.46 | 0.995 | 0.994–0.997 | 0.995, 0.996 |  |
| nested.build.ord | 267.6 | 266.9 | 0.997 | 0.995–1.000 | 0.997, 0.997 |  |
| nested.build.plain | 276.9 | 277.3 | 1.001 | 0.999–1.004 | 1.010, 0.993 |  |
| reset | 30.46 | 28.7 | 0.942 | 0.936–0.948 | 0.946, 0.938 |  |
| set.erase.fresh | 29.8 | 29.82 | 1.001 | 0.998–1.004 | 0.998, 1.003 |  |
| set.erase.reused | 23.29 | 23.3 | 1.001 | 0.997–1.004 | 1.000, 1.002 |  |
| set.find.fresh | 4.887 | 4.884 | 0.999 | 0.998–1.001 | 0.998, 1.001 |  |
| set.find.reused | 4.88 | 4.881 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| set.insert.fresh | 28.93 | 28.94 | 1.000 | 0.998–1.002 | 1.003, 0.998 |  |
| set.insert.reused | 22.86 | 22.85 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| str.append | 1.571 | 1.561 | 0.994 | 0.989–0.999 | 0.993, 0.995 |  |
| str.assign_long | 2.943 | 2.966 | 1.008 | 1.005–1.011 | 1.007, 1.008 | worse |
| str.assign_short | 0.9413 | 0.957 | 1.017 | 1.012–1.021 | 1.029, 1.005 | worse |
| str.assign_short.copy | 0.6309 | 0.6342 | 1.005 | 1.000–1.010 | 1.014, 0.996 |  |
| str.assign_short.view | 0.9513 | 0.9555 | 1.004 | 1.000–1.009 | 1.013, 0.996 |  |
| vec.iterate | 0.03327 | 0.03325 | 0.999 | 0.999–1.000 | 0.999, 1.000 |  |
| vec.mid_erase | 40.53 | 40.51 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| vec.mid_insert | 40.81 | 40.82 | 1.000 | 0.997–1.003 | 0.998, 1.003 |  |
| vec.push_back | 0.2722 | 0.2719 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.push_back_grow | 0.2746 | 0.2697 | 0.982 | 0.979–0.986 | 0.990, 0.975 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.plain 1.009; hero.mid_insert.plain 1.013; map.erase.reused 1.003; str.assign_long 1.008; str.assign_short 1.017

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.push_back_grow.plain (XOffset ns/op) | 1.005 | 1.001 | 0.999–1.004 | not worse |
| map.find.reused (XOffset ns/op) | 1.002 | 1.000 | 0.998–1.002 | not worse |
| str.append (XOffset ns/op) | 1.015 | 0.994 | 0.989–0.999 | not worse |
| str.assign_short (XOffset ns/op) | 1.016 | 1.017 | 1.012–1.021 | still worse |
| str.assign_short.view (XOffset ns/op) | 1.008 | 1.004 | 1.000–1.009 | not worse |
| vec.push_back (XOffset ns/op) | 1.013 | 0.999 | 0.997–1.001 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.015 | 0.982 | 0.979–0.986 | not worse |
| map.find.fresh / std::map (report) | 1.005 | 0.995 | 0.991–0.999 | not worse |
| map.insert.fresh / std::map (report) | 1.004 | 1.016 | 1.012–1.019 | still worse |
| set.find.reused / std::set (report) | 1.115 | 1.001 | 0.998–1.004 | not worse |
| str.append / std::string (G9) | 1.025 | 0.978 | 0.973–0.983 | not worse |
| str.assign_short / std::string (report) | 1.006 | 1.002 | 0.999–1.005 | not worse |
| str.assign_short.view / std::string (G9) | 1.003 | 0.998 | 0.995–1.001 | not worse |
| vec.mid_insert / std::vector (G9) | 1.003 | 1.020 | 1.016–1.024 | still worse |
