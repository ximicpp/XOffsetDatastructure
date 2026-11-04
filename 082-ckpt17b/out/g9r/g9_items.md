## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.722 | 0.996 | 0.993–0.999 | 0.993, 0.999 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.762 | 0.760 | 0.998 | 0.992–1.004 | 0.994, 1.003 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.371 | 0.367 | 0.990 | 0.986–0.993 | 1.002, 0.977 |  |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.938 | 1.001 | 0.998–1.004 | 0.999, 1.003 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.814 | 1.007 | 1.006–1.009 | 1.009, 1.006 | worse |
| map.insert.fresh | std::flat_map | G9 | 0.870 | 0.857 | 0.985 | 0.983–0.987 | 0.982, 0.987 |  |
| nested.build.ord | std | G9 | 0.327 | 0.325 | 0.994 | 0.989–0.998 | 0.995, 0.993 |  |
| reset | std | G9 | 0.231 | 0.256 | 1.106 | 1.093–1.119 | 1.011, 1.210 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.086 | 1.086 | 1.001 | 0.998–1.003 | 1.000, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.995 | 0.998 | 0.996–1.000 | 0.999, 0.996 |  |
| str.append | std::string | G9 | 0.418 | 0.435 | 1.042 | 1.037–1.047 | 1.045, 1.039 | worse |
| str.assign_long | std::string | G9 | 0.867 | 0.866 | 0.999 | 0.996–1.001 | 1.001, 0.996 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 0.998 | 0.997–1.000 | 0.998, 0.998 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.000 | 0.999 | 0.997–1.001 | 0.998, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.020 | 1.002 | 1.000–1.003 | 1.005, 0.999 |  |
| vec.mid_insert | std::vector | G9 | 0.956 | 0.961 | 1.006 | 1.003–1.008 | 1.003, 1.008 | worse |
| vec.push_back | std::vector | G9 | 1.115 | 1.117 | 1.002 | 0.998–1.005 | 1.011, 0.993 |  |
| vec.push_back_grow | std::vector | G9 | 0.767 | 0.780 | 1.017 | 1.015–1.020 | 1.021, 1.013 | worse |
| hero.mid_erase.plain | std::vector | report | 4.054 | 4.050 | 0.999 | 0.996–1.002 | 0.993, 1.005 |  |
| hero.mid_insert.plain | std::vector | report | 4.219 | 4.170 | 0.988 | 0.985–0.992 | 0.989, 0.987 |  |
| hero.push_back_grow.plain | std::vector | report | 0.408 | 0.408 | 0.998 | 0.996–1.001 | 0.996, 1.000 |  |
| map.erase.fresh | std::map | report | 0.748 | 0.747 | 0.998 | 0.995–1.001 | 0.998, 0.998 |  |
| map.erase.reused | std::flat_map | report | 1.085 | 1.078 | 0.994 | 0.990–0.997 | 0.999, 0.989 |  |
| map.erase.reused | std::map | report | 1.657 | 1.680 | 1.014 | 1.008–1.020 | 0.995, 1.033 | worse? |
| map.find.fresh | std::map | report | 0.325 | 0.325 | 0.998 | 0.994–1.002 | 0.999, 0.997 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.814 | 1.008 | 1.006–1.010 | 1.009, 1.007 | worse |
| map.find.reused | std::map | report | 0.691 | 0.822 | 1.191 | 1.185–1.197 | 1.211, 1.171 | worse |
| map.insert.fresh | std::map | report | 0.748 | 0.749 | 1.001 | 0.999–1.004 | 1.001, 1.001 |  |
| map.insert.reused | std::flat_map | report | 1.003 | 1.032 | 1.029 | 1.026–1.032 | 1.029, 1.029 | worse |
| map.insert.reused | std::map | report | 1.826 | 1.901 | 1.041 | 1.035–1.048 | 1.056, 1.026 | worse |
| nested.build.plain | std | report | 0.336 | 0.336 | 0.999 | 0.996–1.002 | 1.009, 0.990 |  |
| set.erase.fresh | std::set | report | 0.674 | 0.674 | 0.999 | 0.996–1.002 | 0.997, 1.001 |  |
| set.erase.reused | std::flat_set | report | 1.144 | 1.134 | 0.991 | 0.986–0.996 | 0.993, 0.990 |  |
| set.erase.reused | std::set | report | 1.322 | 1.362 | 1.030 | 1.021–1.040 | 1.010, 1.050 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 1.002 | 0.996–1.007 | 0.995, 1.008 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.find.reused | std::set | report | 0.664 | 0.712 | 1.073 | 1.067–1.078 | 1.051, 1.095 | worse |
| set.insert.fresh | std::set | report | 0.653 | 0.652 | 0.999 | 0.996–1.002 | 1.002, 0.996 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.108 | 1.025 | 1.023–1.027 | 1.027, 1.023 | worse |
| set.insert.reused | std::set | report | 1.440 | 1.508 | 1.047 | 1.039–1.055 | 1.052, 1.043 | worse |
| str.assign_short | std::string | report | 1.760 | 1.762 | 1.001 | 0.998–1.004 | 0.999, 1.002 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.153 | 1.002 | 0.997–1.006 | 1.004, 1.000 |  |

worse at t1 than t0 (by the rule): map.find.fresh / std::flat_map (G9) 1.007; reset / std (G9) 1.106; str.append / std::string (G9) 1.042; vec.mid_insert / std::vector (G9) 1.006; vec.push_back_grow / std::vector (G9) 1.017; map.find.reused / std::flat_map (report) 1.008; map.find.reused / std::map (report) 1.191; map.insert.reused / std::flat_map (report) 1.029; map.insert.reused / std::map (report) 1.041; set.erase.reused / std::set (report) 1.030; set.find.reused / std::set (report) 1.073; set.insert.reused / std::flat_set (report) 1.025; set.insert.reused / std::set (report) 1.047

above 1.01 but not worse by the rule: map.erase.reused / std::map (report) 1.014

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.1 | 162.8 | 0.992 | 0.990–0.995 | 0.993, 0.992 |  |
| hero.mid_erase.plain | 910.5 | 909.1 | 0.998 | 0.996–1.001 | 0.997, 1.000 |  |
| hero.mid_insert.ord | 228.6 | 233.1 | 1.020 | 1.011–1.028 | 0.998, 1.041 | worse? |
| hero.mid_insert.plain | 1261 | 1249 | 0.991 | 0.988–0.993 | 0.989, 0.993 |  |
| hero.push_back_grow.ord | 32.24 | 31.87 | 0.988 | 0.982–0.995 | 1.002, 0.975 |  |
| hero.push_back_grow.plain | 35.3 | 35.25 | 0.998 | 0.996–1.001 | 1.002, 0.995 |  |
| map.erase.fresh | 33.02 | 33.26 | 1.007 | 1.004–1.010 | 1.001, 1.013 | worse |
| map.erase.reused | 29.79 | 29.57 | 0.993 | 0.990–0.995 | 0.997, 0.989 |  |
| map.find.fresh | 5.286 | 5.333 | 1.009 | 1.007–1.011 | 1.007, 1.011 | worse |
| map.find.reused | 5.298 | 5.345 | 1.009 | 1.007–1.010 | 1.008, 1.009 | worse |
| map.insert.fresh | 32.22 | 31.98 | 0.992 | 0.991–0.994 | 0.992, 0.993 |  |
| map.insert.reused | 29.52 | 30.3 | 1.027 | 1.024–1.029 | 1.027, 1.027 | worse |
| nested.build.ord | 273.8 | 269 | 0.983 | 0.978–0.987 | 0.986, 0.979 |  |
| nested.build.plain | 277.5 | 278 | 1.002 | 0.999–1.004 | 1.002, 1.002 |  |
| reset | 29.63 | 30.19 | 1.019 | 1.013–1.025 | 1.003, 1.035 | worse |
| set.erase.fresh | 29.77 | 29.85 | 1.003 | 1.001–1.005 | 1.001, 1.005 | worse |
| set.erase.reused | 23.49 | 23.16 | 0.986 | 0.979–0.992 | 0.995, 0.976 |  |
| set.find.fresh | 4.879 | 4.901 | 1.004 | 1.002–1.007 | 1.000, 1.009 | worse |
| set.find.reused | 4.91 | 4.878 | 0.994 | 0.991–0.996 | 0.999, 0.988 |  |
| set.insert.fresh | 28.89 | 28.94 | 1.002 | 0.999–1.004 | 0.998, 1.006 |  |
| set.insert.reused | 22.85 | 23.49 | 1.028 | 1.026–1.029 | 1.026, 1.030 | worse |
| str.append | 1.548 | 1.557 | 1.006 | 1.001–1.010 | 1.010, 1.001 | worse |
| str.assign_long | 2.942 | 2.943 | 1.000 | 0.998–1.002 | 1.002, 0.999 |  |
| str.assign_short | 0.9303 | 0.9274 | 0.997 | 0.994–1.000 | 1.002, 0.992 |  |
| str.assign_short.copy | 0.6234 | 0.6223 | 0.998 | 0.993–1.004 | 0.997, 1.000 |  |
| str.assign_short.view | 0.9377 | 0.9346 | 0.997 | 0.994–0.999 | 0.999, 0.994 |  |
| vec.iterate | 0.03315 | 0.03317 | 1.001 | 1.000–1.001 | 1.001, 1.000 |  |
| vec.mid_erase | 40.29 | 40.38 | 1.002 | 1.001–1.004 | 1.005, 0.999 |  |
| vec.mid_insert | 40.38 | 40.71 | 1.008 | 1.005–1.011 | 1.004, 1.012 | worse |
| vec.push_back | 0.2915 | 0.2943 | 1.010 | 0.997–1.022 | 0.999, 1.021 |  |
| vec.push_back_grow | 0.2752 | 0.2792 | 1.015 | 1.009–1.020 | 1.020, 1.009 | worse |

XOffset slower at t1 than t0 (worse by the rule): map.erase.fresh 1.007; map.find.fresh 1.009; map.find.reused 1.009; map.insert.reused 1.027; reset 1.019; set.erase.fresh 1.003; set.find.fresh 1.004; set.insert.reused 1.028; str.append 1.006; vec.mid_insert 1.008; vec.push_back_grow 1.015

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.724 | 0.994 | 0.990–0.997 | 0.996, 0.991 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.754 | 0.992 | 0.988–0.995 | 0.994, 0.989 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.362 | 0.354 | 0.977 | 0.973–0.982 | 1.009, 0.947 |  |
| map.erase.fresh | std::flat_map | G9 | 0.940 | 0.942 | 1.002 | 1.000–1.003 | 1.003, 1.000 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.808 | 1.000 | 0.998–1.002 | 1.000, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.860 | 0.991 | 0.988–0.993 | 0.988, 0.993 |  |
| nested.build.ord | std | G9 | 0.310 | 0.317 | 1.021 | 1.015–1.027 | 1.035, 1.007 | worse |
| reset | std | G9 | 0.224 | 0.206 | 0.918 | 0.904–0.932 | 0.780, 1.079 |  |
| set.erase.fresh | std::flat_set | G9 | 1.094 | 1.092 | 0.998 | 0.994–1.002 | 0.998, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.998–1.001 | 0.998, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.993 | 0.997 | 0.995–0.999 | 0.996, 0.998 |  |
| str.append | std::string | G9 | 0.437 | 0.431 | 0.988 | 0.983–0.993 | 0.997, 0.979 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.866 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.340 | 0.342 | 1.007 | 1.005–1.009 | 1.010, 1.003 | worse |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 0.999 | 0.998–1.000 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.025 | 1.022 | 0.997 | 0.995–0.999 | 0.988, 1.005 |  |
| vec.mid_insert | std::vector | G9 | 0.960 | 0.961 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| vec.push_back | std::vector | G9 | 1.117 | 1.118 | 1.000 | 0.998–1.003 | 0.994, 1.006 |  |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.762 | 0.995 | 0.992–0.998 | 1.007, 0.983 |  |
| hero.mid_erase.plain | std::vector | report | 4.044 | 4.002 | 0.990 | 0.986–0.993 | 0.987, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.174 | 4.160 | 0.996 | 0.993–1.000 | 1.003, 0.990 |  |
| hero.push_back_grow.plain | std::vector | report | 0.401 | 0.395 | 0.984 | 0.979–0.988 | 1.023, 0.946 |  |
| map.erase.fresh | std::map | report | 0.734 | 0.733 | 0.998 | 0.995–1.001 | 1.008, 0.989 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.078 | 0.998 | 0.996–1.000 | 1.001, 0.995 |  |
| map.erase.reused | std::map | report | 1.572 | 1.583 | 1.007 | 1.000–1.013 | 1.000, 1.013 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 1.001 | 0.997–1.005 | 1.000, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.807 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.find.reused | std::map | report | 0.732 | 0.743 | 1.015 | 1.010–1.021 | 1.045, 0.986 | worse? |
| map.insert.fresh | std::map | report | 0.740 | 0.735 | 0.993 | 0.989–0.996 | 1.011, 0.975 |  |
| map.insert.reused | std::flat_map | report | 1.006 | 1.037 | 1.031 | 1.029–1.033 | 1.030, 1.032 | worse |
| map.insert.reused | std::map | report | 1.805 | 1.885 | 1.044 | 1.040–1.048 | 1.069, 1.020 | worse |
| nested.build.plain | std | report | 0.326 | 0.328 | 1.007 | 1.002–1.012 | 0.996, 1.018 |  |
| set.erase.fresh | std::set | report | 0.662 | 0.662 | 1.001 | 0.996–1.006 | 0.995, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.132 | 1.132 | 1.000 | 0.997–1.002 | 1.005, 0.995 |  |
| set.erase.reused | std::set | report | 1.298 | 1.306 | 1.006 | 0.999–1.013 | 0.984, 1.029 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.998 | 0.994–1.003 | 0.997, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 0.999, 1.000 |  |
| set.find.reused | std::set | report | 0.684 | 0.660 | 0.965 | 0.960–0.970 | 0.987, 0.944 |  |
| set.insert.fresh | std::set | report | 0.653 | 0.650 | 0.995 | 0.993–0.997 | 1.012, 0.978 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.103 | 1.020 | 1.017–1.023 | 1.023, 1.017 | worse |
| set.insert.reused | std::set | report | 1.415 | 1.465 | 1.035 | 1.027–1.044 | 1.054, 1.016 | worse |
| str.assign_short | std::string | report | 1.766 | 1.765 | 0.999 | 0.997–1.002 | 0.999, 1.000 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.150 | 0.999 | 0.996–1.001 | 0.998, 0.999 |  |

worse at t1a than t0a (by the rule): nested.build.ord / std (G9) 1.021; str.assign_short.view / std::string (G9) 1.007; map.insert.reused / std::flat_map (report) 1.031; map.insert.reused / std::map (report) 1.044; set.insert.reused / std::flat_set (report) 1.020; set.insert.reused / std::set (report) 1.035

above 1.01 but not worse by the rule: map.find.reused / std::map (report) 1.015

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.4 | 164.3 | 1.000 | 0.998–1.002 | 0.997, 1.003 |  |
| hero.mid_erase.plain | 908.5 | 901.9 | 0.993 | 0.990–0.996 | 0.986, 0.999 |  |
| hero.mid_insert.ord | 229.7 | 227.9 | 0.992 | 0.991–0.994 | 0.992, 0.992 |  |
| hero.mid_insert.plain | 1248 | 1253 | 1.004 | 1.002–1.006 | 1.005, 1.003 | worse |
| hero.push_back_grow.ord | 31.97 | 31.77 | 0.994 | 0.992–0.995 | 0.993, 0.994 |  |
| hero.push_back_grow.plain | 35.2 | 35.37 | 1.005 | 1.004–1.006 | 1.003, 1.006 | worse |
| map.erase.fresh | 33.02 | 33.01 | 1.000 | 0.999–1.001 | 1.000, 0.999 |  |
| map.erase.reused | 29.63 | 29.58 | 0.998 | 0.997–0.999 | 0.999, 0.997 |  |
| map.find.fresh | 5.285 | 5.289 | 1.001 | 0.999–1.002 | 1.000, 1.001 |  |
| map.find.reused | 5.303 | 5.299 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.fresh | 32.24 | 31.96 | 0.991 | 0.990–0.993 | 0.990, 0.992 |  |
| map.insert.reused | 29.47 | 30.35 | 1.030 | 1.028–1.032 | 1.028, 1.031 | worse |
| nested.build.ord | 267 | 267.7 | 1.002 | 1.000–1.005 | 0.996, 1.009 |  |
| nested.build.plain | 274.4 | 275.6 | 1.004 | 1.002–1.006 | 0.999, 1.010 |  |
| reset | 30.11 | 29.87 | 0.992 | 0.987–0.997 | 0.949, 1.037 |  |
| set.erase.fresh | 29.78 | 29.83 | 1.001 | 0.998–1.005 | 0.998, 1.004 |  |
| set.erase.reused | 23.17 | 23.1 | 0.997 | 0.994–1.000 | 0.998, 0.996 |  |
| set.find.fresh | 4.878 | 4.882 | 1.001 | 1.000–1.002 | 1.000, 1.002 |  |
| set.find.reused | 4.879 | 4.88 | 1.000 | 0.999–1.002 | 1.000, 1.000 |  |
| set.insert.fresh | 28.87 | 28.75 | 0.996 | 0.995–0.996 | 0.996, 0.996 |  |
| set.insert.reused | 22.82 | 23.38 | 1.024 | 1.023–1.026 | 1.027, 1.022 | worse |
| str.append | 1.579 | 1.558 | 0.987 | 0.982–0.992 | 0.986, 0.987 |  |
| str.assign_long | 2.942 | 2.95 | 1.003 | 1.000–1.005 | 1.002, 1.003 | worse |
| str.assign_short | 0.9344 | 0.9419 | 1.008 | 1.004–1.012 | 1.011, 1.005 | worse |
| str.assign_short.copy | 0.6248 | 0.6285 | 1.006 | 1.001–1.011 | 1.009, 1.003 | worse |
| str.assign_short.view | 0.9362 | 0.947 | 1.012 | 1.008–1.015 | 1.010, 1.013 | worse |
| vec.iterate | 0.03329 | 0.03324 | 0.998 | 0.998–0.999 | 0.996, 1.000 |  |
| vec.mid_erase | 40.7 | 40.7 | 1.000 | 0.998–1.002 | 0.993, 1.007 |  |
| vec.mid_insert | 40.67 | 40.68 | 1.000 | 0.999–1.002 | 1.001, 1.000 |  |
| vec.push_back | 0.2718 | 0.2721 | 1.001 | 1.000–1.002 | 1.000, 1.003 | worse |
| vec.push_back_grow | 0.2695 | 0.2697 | 1.001 | 0.999–1.003 | 1.000, 1.001 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_insert.plain 1.004; hero.push_back_grow.plain 1.005; map.insert.reused 1.030; set.insert.reused 1.024; str.assign_long 1.003; str.assign_short 1.008; str.assign_short.copy 1.006; str.assign_short.view 1.012; vec.push_back 1.001

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.erase.fresh (XOffset ns/op) | 1.007 | 1.000 | 0.999–1.001 | not worse |
| map.find.fresh (XOffset ns/op) | 1.009 | 1.001 | 0.999–1.002 | not worse |
| map.find.reused (XOffset ns/op) | 1.009 | 0.999 | 0.998–1.001 | not worse |
| map.insert.reused (XOffset ns/op) | 1.027 | 1.030 | 1.028–1.032 | still worse |
| reset (XOffset ns/op) | 1.019 | 0.992 | 0.987–0.997 | not worse |
| set.erase.fresh (XOffset ns/op) | 1.003 | 1.001 | 0.998–1.005 | not worse |
| set.find.fresh (XOffset ns/op) | 1.004 | 1.001 | 1.000–1.002 | not worse |
| set.insert.reused (XOffset ns/op) | 1.028 | 1.024 | 1.023–1.026 | still worse |
| str.append (XOffset ns/op) | 1.006 | 0.987 | 0.982–0.992 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.008 | 1.000 | 0.999–1.002 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.015 | 1.001 | 0.999–1.003 | not worse |
| map.find.fresh / std::flat_map (G9) | 1.007 | 1.000 | 0.998–1.002 | not worse |
| map.find.reused / std::flat_map (report) | 1.008 | 1.000 | 0.997–1.002 | not worse |
| map.find.reused / std::map (report) | 1.191 | 1.015 | 1.010–1.021 | worse? |
| map.insert.reused / std::flat_map (report) | 1.029 | 1.031 | 1.029–1.033 | still worse |
| map.insert.reused / std::map (report) | 1.041 | 1.044 | 1.040–1.048 | still worse |
| reset / std (G9) | 1.106 | 0.918 | 0.904–0.932 | not worse |
| set.erase.reused / std::set (report) | 1.030 | 1.006 | 0.999–1.013 | not worse |
| set.find.reused / std::set (report) | 1.073 | 0.965 | 0.960–0.970 | not worse |
| set.insert.reused / std::flat_set (report) | 1.025 | 1.020 | 1.017–1.023 | still worse |
| set.insert.reused / std::set (report) | 1.047 | 1.035 | 1.027–1.044 | still worse |
| str.append / std::string (G9) | 1.042 | 0.988 | 0.983–0.993 | not worse |
| vec.mid_insert / std::vector (G9) | 1.006 | 1.000 | 0.999–1.002 | not worse |
| vec.push_back_grow / std::vector (G9) | 1.017 | 0.995 | 0.992–0.998 | not worse |
