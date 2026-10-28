## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.723 | 0.727 | 1.005 | 1.002–1.007 | 1.006, 1.003 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.758 | 0.998 | 0.995–1.002 | 1.003, 0.994 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.381 | 0.383 | 1.007 | 1.005–1.009 | 1.004, 1.009 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.941 | 1.004 | 1.002–1.007 | 1.005, 1.003 | worse |
| map.find.fresh | std::flat_map | G9 | 0.784 | 0.784 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.870 | 0.867 | 0.996 | 0.994–0.998 | 0.999, 0.994 |  |
| nested.build.ord | std | G9 | 0.347 | 0.349 | 1.008 | 1.003–1.012 | 0.995, 1.021 |  |
| reset | std | G9 | 0.224 | 0.223 | 0.996 | 0.977–1.015 | 1.040, 0.954 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.085 | 0.998 | 0.996–0.999 | 0.998, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.001 | 0.998–1.004 | 1.001, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 0.995 | 0.998 | 1.003 | 1.000–1.006 | 1.002, 1.004 | worse |
| str.append | std::string | G9 | 0.419 | 0.428 | 1.024 | 1.019–1.028 | 1.035, 1.013 | worse |
| str.assign_long | std::string | G9 | 0.864 | 0.800 | 0.925 | 0.923–0.927 | 0.928, 0.923 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.317 | 0.923 | 0.921–0.926 | 0.921, 0.926 |  |
| vec.iterate | std::vector | G9 | 0.952 | 1.001 | 1.051 | 1.048–1.054 | 1.051, 1.051 | worse |
| vec.mid_erase | std::vector | G9 | 1.013 | 1.023 | 1.009 | 1.007–1.012 | 1.006, 1.013 | worse |
| vec.mid_insert | std::vector | G9 | 0.959 | 0.991 | 1.033 | 1.031–1.036 | 1.034, 1.033 | worse |
| vec.push_back | std::vector | G9 | 1.143 | 1.118 | 0.978 | 0.974–0.982 | 0.981, 0.976 |  |
| vec.push_back_grow | std::vector | G9 | 0.777 | 0.787 | 1.014 | 1.011–1.017 | 1.015, 1.013 | worse |
| hero.mid_erase.plain | std::vector | report | 4.043 | 4.066 | 1.006 | 1.002–1.009 | 1.010, 1.001 | worse |
| hero.mid_insert.plain | std::vector | report | 4.231 | 4.076 | 0.963 | 0.960–0.967 | 0.961, 0.965 |  |
| hero.push_back_grow.plain | std::vector | report | 0.421 | 0.420 | 0.998 | 0.996–1.001 | 0.996, 1.001 |  |
| map.erase.fresh | std::map | report | 0.741 | 0.732 | 0.988 | 0.984–0.991 | 0.990, 0.985 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.078 | 0.998 | 0.994–1.001 | 0.998, 0.998 |  |
| map.erase.reused | std::map | report | 1.651 | 1.617 | 0.980 | 0.973–0.987 | 0.982, 0.977 |  |
| map.find.fresh | std::map | report | 0.315 | 0.315 | 0.998 | 0.994–1.002 | 1.000, 0.996 |  |
| map.find.reused | std::flat_map | report | 0.784 | 0.785 | 1.001 | 0.998–1.003 | 1.002, 0.999 |  |
| map.find.reused | std::map | report | 0.718 | 0.714 | 0.994 | 0.989–0.999 | 0.969, 1.020 |  |
| map.insert.fresh | std::map | report | 0.750 | 0.743 | 0.991 | 0.988–0.995 | 0.994, 0.988 |  |
| map.insert.reused | std::flat_map | report | 1.005 | 1.001 | 0.997 | 0.993–1.000 | 0.999, 0.994 |  |
| map.insert.reused | std::map | report | 1.818 | 1.846 | 1.015 | 1.008–1.022 | 1.013, 1.017 | worse |
| nested.build.plain | std | report | 0.358 | 0.358 | 1.001 | 0.997–1.005 | 0.985, 1.017 |  |
| set.erase.fresh | std::set | report | 0.668 | 0.661 | 0.990 | 0.986–0.993 | 0.999, 0.981 |  |
| set.erase.reused | std::flat_set | report | 1.137 | 1.140 | 1.002 | 0.999–1.005 | 1.003, 1.001 |  |
| set.erase.reused | std::set | report | 1.356 | 1.321 | 0.975 | 0.968–0.981 | 0.981, 0.968 |  |
| set.find.fresh | std::set | report | 0.301 | 0.302 | 1.002 | 0.997–1.007 | 1.002, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.754 | 0.756 | 1.004 | 1.001–1.007 | 1.002, 1.006 | worse |
| set.find.reused | std::set | report | 0.704 | 0.723 | 1.027 | 1.023–1.031 | 1.027, 1.026 | worse |
| set.insert.fresh | std::set | report | 0.656 | 0.649 | 0.989 | 0.987–0.992 | 0.989, 0.989 |  |
| set.insert.reused | std::flat_set | report | 1.080 | 1.084 | 1.004 | 1.002–1.006 | 1.003, 1.005 | worse |
| set.insert.reused | std::set | report | 1.419 | 1.459 | 1.028 | 1.019–1.036 | 1.022, 1.033 | worse |
| str.assign_short | std::string | report | 1.766 | 1.768 | 1.001 | 0.999–1.004 | 1.000, 1.002 |  |
| str.assign_short.copy | std::string | report | 1.149 | 1.148 | 1.000 | 0.994–1.005 | 1.001, 0.998 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; hero.push_back_grow.ord / std::vector (G9) 1.007; map.erase.fresh / std::flat_map (G9) 1.004; set.insert.fresh / std::flat_set (G9) 1.003; str.append / std::string (G9) 1.024; vec.iterate / std::vector (G9) 1.051; vec.mid_erase / std::vector (G9) 1.009; vec.mid_insert / std::vector (G9) 1.033; vec.push_back_grow / std::vector (G9) 1.014; hero.mid_erase.plain / std::vector (report) 1.006; map.insert.reused / std::map (report) 1.015; set.find.reused / std::flat_set (report) 1.004; set.find.reused / std::set (report) 1.027; set.insert.reused / std::flat_set (report) 1.004; set.insert.reused / std::set (report) 1.028

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.2 | 164.1 | 1.005 | 1.003–1.007 | 1.005, 1.005 | worse |
| hero.mid_erase.plain | 909 | 914.2 | 1.006 | 1.003–1.008 | 1.009, 1.002 | worse |
| hero.mid_insert.ord | 228.4 | 227.8 | 0.997 | 0.995–1.000 | 1.000, 0.995 |  |
| hero.mid_insert.plain | 1262 | 1220 | 0.966 | 0.964–0.968 | 0.964, 0.968 |  |
| hero.push_back_grow.ord | 33.17 | 33.47 | 1.009 | 1.007–1.010 | 1.010, 1.007 | worse |
| hero.push_back_grow.plain | 36.7 | 36.75 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| map.erase.fresh | 32.98 | 33.05 | 1.002 | 1.000–1.004 | 1.003, 1.001 | worse |
| map.erase.reused | 29.68 | 29.64 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.find.fresh | 5.14 | 5.138 | 1.000 | 0.998–1.001 | 1.001, 0.999 |  |
| map.find.reused | 5.156 | 5.153 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.fresh | 32.28 | 32.27 | 1.000 | 0.998–1.001 | 0.997, 1.002 |  |
| map.insert.reused | 29.55 | 29.56 | 1.000 | 0.998–1.003 | 1.001, 1.000 |  |
| nested.build.ord | 294.8 | 295.5 | 1.002 | 0.999–1.006 | 1.002, 1.003 |  |
| nested.build.plain | 301.7 | 301.8 | 1.001 | 0.997–1.004 | 0.996, 1.005 |  |
| reset | 30.02 | 30.53 | 1.017 | 1.012–1.022 | 1.040, 0.994 | worse? |
| set.erase.fresh | 29.8 | 29.77 | 0.999 | 0.998–1.000 | 0.999, 0.999 |  |
| set.erase.reused | 23.25 | 23.3 | 1.002 | 0.999–1.005 | 1.002, 1.003 |  |
| set.find.fresh | 4.885 | 4.903 | 1.004 | 1.001–1.006 | 1.002, 1.005 | worse |
| set.find.reused | 4.882 | 4.895 | 1.003 | 1.000–1.005 | 1.003, 1.002 | worse |
| set.insert.fresh | 28.93 | 28.87 | 0.998 | 0.996–0.999 | 0.997, 0.998 |  |
| set.insert.reused | 22.79 | 22.84 | 1.002 | 1.000–1.004 | 1.001, 1.003 |  |
| str.append | 1.55 | 1.546 | 0.998 | 0.993–1.002 | 1.006, 0.990 |  |
| str.assign_long | 2.937 | 2.717 | 0.925 | 0.923–0.927 | 0.927, 0.924 |  |
| str.assign_short | 0.9279 | 0.9313 | 1.004 | 1.000–1.007 | 1.005, 1.002 | worse |
| str.assign_short.copy | 0.6324 | 0.6173 | 0.976 | 0.969–0.983 | 0.958, 0.995 |  |
| str.assign_short.view | 0.9394 | 0.938 | 0.999 | 0.995–1.002 | 0.999, 0.998 |  |
| vec.iterate | 0.03222 | 0.03392 | 1.053 | 1.048–1.058 | 1.054, 1.051 | worse |
| vec.mid_erase | 40.2 | 40.47 | 1.007 | 1.005–1.009 | 1.007, 1.006 | worse |
| vec.mid_insert | 40.9 | 41.36 | 1.011 | 1.008–1.014 | 1.008, 1.015 | worse |
| vec.push_back | 0.2944 | 0.2911 | 0.989 | 0.978–1.000 | 0.988, 0.990 |  |
| vec.push_back_grow | 0.2814 | 0.2867 | 1.019 | 1.012–1.026 | 1.018, 1.019 | worse |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.005; hero.mid_erase.plain 1.006; hero.push_back_grow.ord 1.009; map.erase.fresh 1.002; set.find.fresh 1.004; set.find.reused 1.003; str.assign_short 1.004; vec.iterate 1.053; vec.mid_erase 1.007; vec.mid_insert 1.011; vec.push_back_grow 1.019

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.726 | 0.722 | 0.994 | 0.991–0.997 | 0.995, 0.994 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.762 | 0.761 | 1.000 | 0.996–1.004 | 0.996, 1.003 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.381 | 0.381 | 1.001 | 0.998–1.003 | 1.003, 0.998 |  |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.942 | 1.002 | 1.000–1.004 | 1.005, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.785 | 0.784 | 0.999 | 0.997–1.002 | 0.998, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.872 | 0.869 | 0.996 | 0.994–0.998 | 1.001, 0.991 |  |
| nested.build.ord | std | G9 | 0.349 | 0.354 | 1.014 | 1.010–1.018 | 1.023, 1.005 | worse |
| reset | std | G9 | 0.246 | 0.212 | 0.863 | 0.852–0.874 | 0.755, 0.986 |  |
| set.erase.fresh | std::flat_set | G9 | 1.088 | 1.093 | 1.005 | 1.001–1.008 | 1.003, 1.006 | worse |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.996–1.001 | 0.999, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 0.999 | 1.000 | 0.998–1.001 | 0.998, 1.002 |  |
| str.append | std::string | G9 | 0.426 | 0.429 | 1.006 | 1.001–1.012 | 1.001, 1.012 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.867 | 1.000 | 0.998–1.003 | 1.001, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.001 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.019 | 1.002 | 1.000–1.004 | 1.009, 0.996 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.993 | 1.040 | 1.037–1.043 | 1.040, 1.040 | worse |
| vec.push_back | std::vector | G9 | 1.117 | 1.120 | 1.003 | 1.000–1.005 | 1.007, 0.999 |  |
| vec.push_back_grow | std::vector | G9 | 0.783 | 0.771 | 0.985 | 0.980–0.989 | 0.983, 0.986 |  |
| hero.mid_erase.plain | std::vector | report | 4.049 | 4.031 | 0.996 | 0.993–0.998 | 0.999, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.174 | 4.179 | 1.001 | 0.997–1.005 | 0.994, 1.009 |  |
| hero.push_back_grow.plain | std::vector | report | 0.423 | 0.421 | 0.995 | 0.992–0.998 | 1.002, 0.987 |  |
| map.erase.fresh | std::map | report | 0.742 | 0.734 | 0.989 | 0.986–0.992 | 0.991, 0.986 |  |
| map.erase.reused | std::flat_map | report | 1.084 | 1.075 | 0.992 | 0.988–0.996 | 0.990, 0.993 |  |
| map.erase.reused | std::map | report | 1.609 | 1.587 | 0.986 | 0.979–0.994 | 0.962, 1.011 |  |
| map.find.fresh | std::map | report | 0.314 | 0.314 | 1.000 | 0.996–1.004 | 0.998, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.785 | 0.785 | 1.001 | 0.998–1.003 | 1.001, 1.000 |  |
| map.find.reused | std::map | report | 0.700 | 0.707 | 1.011 | 1.006–1.016 | 1.017, 1.006 | worse |
| map.insert.fresh | std::map | report | 0.750 | 0.750 | 1.000 | 0.998–1.003 | 0.996, 1.005 |  |
| map.insert.reused | std::flat_map | report | 1.001 | 1.003 | 1.002 | 0.998–1.006 | 1.000, 1.003 |  |
| map.insert.reused | std::map | report | 1.850 | 1.857 | 1.004 | 0.997–1.010 | 0.984, 1.023 |  |
| nested.build.plain | std | report | 0.357 | 0.361 | 1.010 | 1.006–1.014 | 1.016, 1.004 | worse |
| set.erase.fresh | std::set | report | 0.678 | 0.658 | 0.971 | 0.968–0.974 | 0.974, 0.968 |  |
| set.erase.reused | std::flat_set | report | 1.141 | 1.147 | 1.005 | 1.000–1.010 | 1.005, 1.005 | worse |
| set.erase.reused | std::set | report | 1.314 | 1.309 | 0.996 | 0.988–1.005 | 0.975, 1.019 |  |
| set.find.fresh | std::set | report | 0.289 | 0.299 | 1.034 | 1.030–1.039 | 1.034, 1.035 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.757 | 1.002 | 0.999–1.005 | 1.001, 1.004 |  |
| set.find.reused | std::set | report | 0.675 | 0.686 | 1.017 | 1.013–1.021 | 1.034, 1.000 | worse? |
| set.insert.fresh | std::set | report | 0.656 | 0.657 | 1.002 | 0.999–1.004 | 1.003, 1.001 |  |
| set.insert.reused | std::flat_set | report | 1.083 | 1.081 | 0.998 | 0.995–1.001 | 0.996, 1.000 |  |
| set.insert.reused | std::set | report | 1.457 | 1.462 | 1.004 | 0.995–1.012 | 0.997, 1.010 |  |
| str.assign_short | std::string | report | 1.767 | 1.763 | 0.998 | 0.996–1.000 | 1.000, 0.996 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.146 | 0.996 | 0.992–1.000 | 0.995, 0.996 |  |

worse at t1a than t0a (by the rule): nested.build.ord / std (G9) 1.014; set.erase.fresh / std::flat_set (G9) 1.005; str.append / std::string (G9) 1.006; vec.mid_insert / std::vector (G9) 1.040; map.find.reused / std::map (report) 1.011; nested.build.plain / std (report) 1.010; set.erase.reused / std::flat_set (report) 1.005; set.find.fresh / std::set (report) 1.034

above 1.01 but not worse by the rule: set.find.reused / std::set (report) 1.017

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 164.3 | 1.003 | 1.001–1.005 | 1.006, 1.000 |  |
| hero.mid_erase.plain | 908.7 | 907.7 | 0.999 | 0.996–1.001 | 1.005, 0.993 |  |
| hero.mid_insert.ord | 229.6 | 230 | 1.002 | 1.000–1.004 | 1.003, 1.000 |  |
| hero.mid_insert.plain | 1251 | 1249 | 0.999 | 0.996–1.001 | 0.997, 1.000 |  |
| hero.push_back_grow.ord | 33.3 | 33.14 | 0.995 | 0.993–0.997 | 0.999, 0.992 |  |
| hero.push_back_grow.plain | 36.67 | 36.47 | 0.995 | 0.993–0.997 | 0.999, 0.990 |  |
| map.erase.fresh | 33 | 33.01 | 1.000 | 0.999–1.002 | 1.002, 0.999 |  |
| map.erase.reused | 29.74 | 29.57 | 0.994 | 0.992–0.996 | 0.990, 0.998 |  |
| map.find.fresh | 5.138 | 5.132 | 0.999 | 0.997–1.000 | 0.997, 1.000 |  |
| map.find.reused | 5.155 | 5.156 | 1.000 | 0.998–1.002 | 0.998, 1.002 |  |
| map.insert.fresh | 32.26 | 32.22 | 0.999 | 0.998–1.000 | 0.999, 0.998 |  |
| map.insert.reused | 29.58 | 29.53 | 0.998 | 0.995–1.001 | 0.997, 0.999 |  |
| nested.build.ord | 294.2 | 295.4 | 1.004 | 1.001–1.007 | 1.004, 1.004 | worse |
| nested.build.plain | 301.5 | 301.8 | 1.001 | 0.998–1.004 | 1.004, 0.998 |  |
| reset | 29.89 | 30.54 | 1.022 | 1.016–1.027 | 1.040, 1.003 | worse |
| set.erase.fresh | 29.86 | 29.8 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| set.erase.reused | 23.36 | 23.36 | 1.000 | 0.997–1.004 | 1.001, 0.999 |  |
| set.find.fresh | 4.885 | 4.883 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| set.find.reused | 4.882 | 4.888 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| set.insert.fresh | 28.9 | 28.87 | 0.999 | 0.998–1.000 | 0.998, 1.000 |  |
| set.insert.reused | 22.85 | 22.83 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| str.append | 1.551 | 1.554 | 1.002 | 0.997–1.006 | 1.005, 0.999 |  |
| str.assign_long | 2.943 | 2.945 | 1.001 | 0.998–1.003 | 1.005, 0.996 |  |
| str.assign_short | 0.9311 | 0.9322 | 1.001 | 0.998–1.004 | 1.011, 0.991 |  |
| str.assign_short.copy | 0.6243 | 0.626 | 1.003 | 0.998–1.008 | 1.013, 0.993 |  |
| str.assign_short.view | 0.9351 | 0.9396 | 1.005 | 1.002–1.008 | 1.019, 0.991 |  |
| vec.iterate | 0.03322 | 0.03352 | 1.009 | 1.006–1.012 | 1.000, 1.018 |  |
| vec.mid_erase | 40.43 | 40.54 | 1.003 | 1.001–1.004 | 1.005, 1.001 | worse |
| vec.mid_insert | 40.51 | 40.58 | 1.002 | 1.000–1.004 | 1.002, 1.002 |  |
| vec.push_back | 0.2717 | 0.2716 | 1.000 | 0.999–1.000 | 1.000, 1.000 |  |
| vec.push_back_grow | 0.2751 | 0.2755 | 1.002 | 0.999–1.005 | 1.011, 0.992 |  |

XOffset slower at t1a than t0a (worse by the rule): nested.build.ord 1.004; reset 1.022; vec.mid_erase 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.005 | 1.003 | 1.001–1.005 | not worse |
| hero.mid_erase.plain (XOffset ns/op) | 1.006 | 0.999 | 0.996–1.001 | not worse |
| hero.push_back_grow.ord (XOffset ns/op) | 1.009 | 0.995 | 0.993–0.997 | not worse |
| map.erase.fresh (XOffset ns/op) | 1.002 | 1.000 | 0.999–1.002 | not worse |
| set.find.fresh (XOffset ns/op) | 1.004 | 1.000 | 0.998–1.001 | not worse |
| set.find.reused (XOffset ns/op) | 1.003 | 1.001 | 0.999–1.004 | not worse |
| str.assign_short (XOffset ns/op) | 1.004 | 1.001 | 0.998–1.004 | not worse |
| vec.iterate (XOffset ns/op) | 1.053 | 1.009 | 1.006–1.012 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.007 | 1.003 | 1.001–1.004 | still worse |
| vec.mid_insert (XOffset ns/op) | 1.011 | 1.002 | 1.000–1.004 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.019 | 1.002 | 0.999–1.005 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.994 | 0.991–0.997 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.006 | 0.996 | 0.993–0.998 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.007 | 1.001 | 0.998–1.003 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.004 | 1.002 | 1.000–1.004 | not worse |
| map.insert.reused / std::map (report) | 1.015 | 1.004 | 0.997–1.010 | not worse |
| set.find.reused / std::flat_set (report) | 1.004 | 1.002 | 0.999–1.005 | not worse |
| set.find.reused / std::set (report) | 1.027 | 1.017 | 1.013–1.021 | worse? |
| set.insert.fresh / std::flat_set (G9) | 1.003 | 1.000 | 0.998–1.001 | not worse |
| set.insert.reused / std::flat_set (report) | 1.004 | 0.998 | 0.995–1.001 | not worse |
| set.insert.reused / std::set (report) | 1.028 | 1.004 | 0.995–1.012 | not worse |
| str.append / std::string (G9) | 1.024 | 1.006 | 1.001–1.012 | still worse |
| vec.iterate / std::vector (G9) | 1.051 | 1.001 | 0.999–1.002 | not worse |
| vec.mid_erase / std::vector (G9) | 1.009 | 1.002 | 1.000–1.004 | not worse |
| vec.mid_insert / std::vector (G9) | 1.033 | 1.040 | 1.037–1.043 | still worse |
| vec.push_back_grow / std::vector (G9) | 1.014 | 0.985 | 0.980–0.989 | not worse |
