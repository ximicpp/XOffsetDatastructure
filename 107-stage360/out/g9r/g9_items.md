## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.738 | 1.013 | 1.008–1.019 | 1.004, 1.023 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.761 | 0.997 | 0.992–1.002 | 0.989, 1.006 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.346 | 0.945 | 0.940–0.950 | 0.924, 0.966 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.941 | 0.998 | 0.994–1.003 | 1.003, 0.993 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.806 | 0.997 | 0.993–1.001 | 0.996, 0.998 |  |
| map.insert.fresh | std::flat_map | G9 | 0.854 | 0.842 | 0.986 | 0.984–0.988 | 0.986, 0.986 |  |
| nested.build.ord | std | G9 | 0.321 | 0.308 | 0.957 | 0.951–0.964 | 0.954, 0.960 |  |
| reset | std | G9 | 0.194 | 0.184 | 0.944 | 0.928–0.960 | 0.854, 1.044 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.082 | 0.995 | 0.991–0.999 | 0.993, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.753 | 0.753 | 0.999 | 0.995–1.004 | 0.997, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 0.987 | 0.989 | 1.002 | 0.998–1.006 | 1.000, 1.005 |  |
| str.append | std::string | G9 | 0.431 | 0.419 | 0.972 | 0.968–0.976 | 0.970, 0.974 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.864 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.002 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.017 | 0.997 | 0.993–1.001 | 0.995, 0.998 |  |
| vec.mid_insert | std::vector | G9 | 0.953 | 0.992 | 1.041 | 1.037–1.045 | 1.039, 1.043 | worse |
| vec.push_back | std::vector | G9 | 1.122 | 1.128 | 1.006 | 1.002–1.009 | 1.009, 1.002 | worse |
| vec.push_back_grow | std::vector | G9 | 0.773 | 0.764 | 0.988 | 0.984–0.993 | 0.983, 0.993 |  |
| hero.mid_erase.plain | std::vector | report | 4.077 | 4.050 | 0.993 | 0.989–0.997 | 0.987, 1.000 |  |
| hero.mid_insert.plain | std::vector | report | 4.202 | 4.192 | 0.998 | 0.993–1.002 | 0.993, 1.003 |  |
| hero.push_back_grow.plain | std::vector | report | 0.405 | 0.380 | 0.939 | 0.935–0.943 | 0.919, 0.959 |  |
| map.erase.fresh | std::map | report | 0.735 | 0.735 | 1.001 | 0.996–1.006 | 0.978, 1.024 |  |
| map.erase.reused | std::flat_map | report | 1.083 | 1.075 | 0.992 | 0.988–0.997 | 0.987, 0.998 |  |
| map.erase.reused | std::map | report | 1.617 | 1.610 | 0.996 | 0.988–1.003 | 0.952, 1.041 |  |
| map.find.fresh | std::map | report | 0.324 | 0.319 | 0.984 | 0.979–0.989 | 0.982, 0.986 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.995–1.004 | 1.000, 0.999 |  |
| map.find.reused | std::map | report | 0.729 | 0.708 | 0.971 | 0.965–0.977 | 0.982, 0.960 |  |
| map.insert.fresh | std::map | report | 0.741 | 0.736 | 0.992 | 0.989–0.995 | 0.988, 0.996 |  |
| map.insert.reused | std::flat_map | report | 1.022 | 1.017 | 0.996 | 0.991–1.000 | 0.993, 0.998 |  |
| map.insert.reused | std::map | report | 1.852 | 1.818 | 0.982 | 0.973–0.990 | 0.967, 0.996 |  |
| nested.build.plain | std | report | 0.327 | 0.316 | 0.966 | 0.960–0.971 | 0.955, 0.977 |  |
| set.erase.fresh | std::set | report | 0.669 | 0.664 | 0.992 | 0.987–0.997 | 0.964, 1.021 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.135 | 0.998 | 0.993–1.002 | 0.993, 1.002 |  |
| set.erase.reused | std::set | report | 1.343 | 1.306 | 0.972 | 0.964–0.980 | 0.947, 0.998 |  |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.007 | 1.001–1.013 | 1.008, 1.005 | worse |
| set.find.reused | std::flat_set | report | 0.754 | 0.750 | 0.994 | 0.990–0.999 | 0.989, 1.000 |  |
| set.find.reused | std::set | report | 0.659 | 0.765 | 1.162 | 1.155–1.168 | 1.141, 1.182 | worse |
| set.insert.fresh | std::set | report | 0.648 | 0.644 | 0.994 | 0.990–0.999 | 0.988, 1.000 |  |
| set.insert.reused | std::flat_set | report | 1.113 | 1.108 | 0.995 | 0.991–0.999 | 0.994, 0.996 |  |
| set.insert.reused | std::set | report | 1.475 | 1.463 | 0.992 | 0.982–1.001 | 0.972, 1.011 |  |
| str.assign_short | std::string | report | 1.757 | 1.771 | 1.008 | 1.005–1.012 | 1.013, 1.004 | worse |
| str.assign_short.copy | std::string | report | 1.148 | 1.151 | 1.003 | 0.999–1.006 | 1.000, 1.006 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.013; vec.mid_insert / std::vector (G9) 1.041; vec.push_back / std::vector (G9) 1.006; set.find.fresh / std::set (report) 1.007; set.find.reused / std::set (report) 1.162; str.assign_short / std::string (report) 1.008

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 165.8 | 167.3 | 1.009 | 1.005–1.013 | 0.996, 1.022 |  |
| hero.mid_erase.plain | 923.1 | 910.2 | 0.986 | 0.982–0.990 | 0.970, 1.003 |  |
| hero.mid_insert.ord | 231 | 228.6 | 0.990 | 0.985–0.994 | 0.978, 1.002 |  |
| hero.mid_insert.plain | 1270 | 1251 | 0.986 | 0.983–0.989 | 0.975, 0.996 |  |
| hero.push_back_grow.ord | 32.23 | 30.66 | 0.951 | 0.948–0.955 | 0.936, 0.966 |  |
| hero.push_back_grow.plain | 35.3 | 33.74 | 0.956 | 0.954–0.958 | 0.947, 0.964 |  |
| map.erase.fresh | 32.99 | 33.31 | 1.010 | 1.006–1.014 | 1.019, 1.000 | worse |
| map.erase.reused | 29.65 | 29.82 | 1.006 | 1.002–1.009 | 1.012, 1.000 |  |
| map.find.fresh | 5.292 | 5.344 | 1.010 | 1.006–1.013 | 1.018, 1.001 | worse |
| map.find.reused | 5.307 | 5.364 | 1.011 | 1.007–1.015 | 1.023, 0.998 | worse? |
| map.insert.fresh | 31.76 | 31.68 | 0.998 | 0.996–0.999 | 0.998, 0.998 |  |
| map.insert.reused | 30.05 | 30.33 | 1.009 | 1.005–1.013 | 1.019, 1.000 |  |
| nested.build.ord | 264.1 | 262.3 | 0.993 | 0.988–0.998 | 1.009, 0.977 |  |
| nested.build.plain | 270.3 | 269.9 | 0.998 | 0.994–1.003 | 1.012, 0.985 |  |
| reset | 25.06 | 24.83 | 0.991 | 0.985–0.996 | 1.032, 0.951 |  |
| set.erase.fresh | 29.76 | 29.93 | 1.006 | 1.003–1.009 | 1.013, 0.999 |  |
| set.erase.reused | 23.16 | 23.39 | 1.010 | 1.006–1.014 | 1.016, 1.005 | worse |
| set.find.fresh | 4.882 | 4.935 | 1.011 | 1.007–1.015 | 1.022, 1.000 | worse |
| set.find.reused | 4.877 | 4.923 | 1.009 | 1.006–1.013 | 1.017, 1.002 | worse |
| set.insert.fresh | 28.59 | 28.93 | 1.012 | 1.008–1.016 | 1.022, 1.002 | worse |
| set.insert.reused | 23.5 | 23.62 | 1.005 | 1.001–1.009 | 1.014, 0.997 |  |
| str.append | 1.52 | 1.517 | 0.998 | 0.994–1.001 | 1.006, 0.990 |  |
| str.assign_long | 2.936 | 2.938 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| str.assign_short | 0.9225 | 0.9274 | 1.005 | 1.003–1.007 | 1.013, 0.998 |  |
| str.assign_short.copy | 0.6087 | 0.6069 | 0.997 | 0.993–1.001 | 0.999, 0.995 |  |
| str.assign_short.view | 0.9299 | 0.9326 | 1.003 | 1.001–1.005 | 1.005, 1.001 | worse |
| vec.iterate | 0.03357 | 0.03316 | 0.988 | 0.983–0.992 | 0.976, 1.000 |  |
| vec.mid_erase | 40.84 | 40.38 | 0.989 | 0.985–0.993 | 0.976, 1.001 |  |
| vec.mid_insert | 40.81 | 40.36 | 0.989 | 0.985–0.993 | 0.978, 1.000 |  |
| vec.push_back | 0.2923 | 0.2886 | 0.987 | 0.977–0.997 | 0.973, 1.002 |  |
| vec.push_back_grow | 0.2779 | 0.274 | 0.986 | 0.981–0.992 | 0.981, 0.991 |  |

XOffset slower at t1 than t0 (worse by the rule): map.erase.fresh 1.010; map.find.fresh 1.010; set.erase.reused 1.010; set.find.fresh 1.011; set.find.reused 1.009; set.insert.fresh 1.012; str.assign_short.view 1.003

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.736 | 1.016 | 1.013–1.018 | 1.012, 1.019 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.757 | 0.992 | 0.988–0.996 | 0.992, 0.992 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.349 | 0.954 | 0.951–0.957 | 0.969, 0.939 |  |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.945 | 1.004 | 1.001–1.007 | 1.002, 1.006 | worse |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.807 | 1.001 | 0.998–1.003 | 1.001, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.856 | 0.845 | 0.987 | 0.985–0.990 | 0.987, 0.988 |  |
| nested.build.ord | std | G9 | 0.308 | 0.311 | 1.007 | 1.001–1.013 | 1.021, 0.993 |  |
| reset | std | G9 | 0.189 | 0.178 | 0.942 | 0.927–0.958 | 0.956, 0.929 |  |
| set.erase.fresh | std::flat_set | G9 | 1.095 | 1.095 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.998 | 0.996–1.001 | 0.997, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.989 | 0.988 | 0.999 | 0.996–1.001 | 0.997, 1.001 |  |
| str.append | std::string | G9 | 0.430 | 0.430 | 0.999 | 0.995–1.003 | 0.994, 1.004 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.811 | 0.938 | 0.936–0.940 | 0.939, 0.937 |  |
| str.assign_short.view | std::string | G9 | 0.341 | 0.343 | 1.006 | 1.002–1.009 | 0.996, 1.016 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.022 | 1.001 | 0.999–1.003 | 0.994, 1.008 |  |
| vec.mid_insert | std::vector | G9 | 0.956 | 0.994 | 1.040 | 1.038–1.042 | 1.038, 1.043 | worse |
| vec.push_back | std::vector | G9 | 1.120 | 1.113 | 0.994 | 0.991–0.996 | 0.990, 0.998 |  |
| vec.push_back_grow | std::vector | G9 | 0.775 | 0.762 | 0.983 | 0.980–0.986 | 0.987, 0.979 |  |
| hero.mid_erase.plain | std::vector | report | 4.042 | 4.025 | 0.996 | 0.993–0.999 | 0.996, 0.995 |  |
| hero.mid_insert.plain | std::vector | report | 4.205 | 4.180 | 0.994 | 0.991–0.998 | 0.993, 0.996 |  |
| hero.push_back_grow.plain | std::vector | report | 0.408 | 0.388 | 0.952 | 0.950–0.954 | 0.959, 0.946 |  |
| map.erase.fresh | std::map | report | 0.735 | 0.737 | 1.003 | 1.000–1.006 | 0.978, 1.028 |  |
| map.erase.reused | std::flat_map | report | 1.082 | 1.079 | 0.997 | 0.995–0.999 | 0.995, 0.998 |  |
| map.erase.reused | std::map | report | 1.595 | 1.568 | 0.983 | 0.976–0.989 | 0.950, 1.017 |  |
| map.find.fresh | std::map | report | 0.324 | 0.323 | 0.999 | 0.995–1.002 | 0.999, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.805 | 0.996 | 0.994–0.998 | 1.000, 0.992 |  |
| map.find.reused | std::map | report | 0.730 | 0.729 | 0.998 | 0.994–1.002 | 1.002, 0.994 |  |
| map.insert.fresh | std::map | report | 0.740 | 0.731 | 0.987 | 0.984–0.990 | 1.000, 0.975 |  |
| map.insert.reused | std::flat_map | report | 1.034 | 1.018 | 0.985 | 0.982–0.987 | 0.987, 0.982 |  |
| map.insert.reused | std::map | report | 1.847 | 1.844 | 0.999 | 0.992–1.005 | 0.999, 0.998 |  |
| nested.build.plain | std | report | 0.316 | 0.316 | 1.000 | 0.994–1.006 | 1.011, 0.990 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.669 | 0.998 | 0.995–1.002 | 0.977, 1.020 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.134 | 0.997 | 0.994–0.999 | 0.993, 1.001 |  |
| set.erase.reused | std::set | report | 1.291 | 1.286 | 0.996 | 0.987–1.006 | 0.988, 1.005 |  |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.999 | 0.995–1.004 | 0.999, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.755 | 0.998 | 0.995–1.001 | 0.997, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.687 | 1.015 | 1.011–1.019 | 1.017, 1.013 | worse |
| set.insert.fresh | std::set | report | 0.643 | 0.641 | 0.997 | 0.994–1.000 | 0.997, 0.997 |  |
| set.insert.reused | std::flat_set | report | 1.108 | 1.111 | 1.003 | 1.000–1.005 | 1.003, 1.002 | worse |
| set.insert.reused | std::set | report | 1.459 | 1.462 | 1.002 | 0.993–1.012 | 0.993, 1.011 |  |
| str.assign_short | std::string | report | 1.765 | 1.764 | 0.999 | 0.997–1.002 | 0.998, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.150 | 0.999 | 0.996–1.001 | 0.998, 1.000 |  |

worse at t1a than t0a (by the rule): hero.mid_erase.ord / std::vector (G9) 1.016; map.erase.fresh / std::flat_map (G9) 1.004; vec.mid_insert / std::vector (G9) 1.040; set.find.reused / std::set (report) 1.015; set.insert.reused / std::flat_set (report) 1.003

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 166.8 | 1.018 | 1.017–1.020 | 1.017, 1.019 | worse |
| hero.mid_erase.plain | 906.4 | 905.2 | 0.999 | 0.996–1.001 | 1.000, 0.997 |  |
| hero.mid_insert.ord | 229.3 | 228.8 | 0.998 | 0.996–1.000 | 0.994, 1.002 |  |
| hero.mid_insert.plain | 1253 | 1250 | 0.998 | 0.996–0.999 | 0.993, 1.003 |  |
| hero.push_back_grow.ord | 31.67 | 30.56 | 0.965 | 0.963–0.967 | 0.967, 0.963 |  |
| hero.push_back_grow.plain | 35.18 | 33.96 | 0.965 | 0.964–0.967 | 0.962, 0.968 |  |
| map.erase.fresh | 33.05 | 33 | 0.999 | 0.996–1.001 | 1.001, 0.997 |  |
| map.erase.reused | 29.68 | 29.6 | 0.997 | 0.996–0.999 | 0.997, 0.997 |  |
| map.find.fresh | 5.286 | 5.283 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.find.reused | 5.307 | 5.303 | 0.999 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.fresh | 31.77 | 31.76 | 1.000 | 0.998–1.001 | 0.999, 1.000 |  |
| map.insert.reused | 30.4 | 30.1 | 0.990 | 0.988–0.993 | 0.995, 0.985 |  |
| nested.build.ord | 260.7 | 260.3 | 0.999 | 0.997–1.000 | 1.006, 0.991 |  |
| nested.build.plain | 268.1 | 266.4 | 0.994 | 0.991–0.996 | 0.993, 0.995 |  |
| reset | 24.06 | 24.92 | 1.035 | 1.030–1.041 | 1.038, 1.033 | worse |
| set.erase.fresh | 29.77 | 29.8 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| set.erase.reused | 23.19 | 23.07 | 0.995 | 0.992–0.998 | 0.990, 0.999 |  |
| set.find.fresh | 4.888 | 4.881 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| set.find.reused | 4.887 | 4.877 | 0.998 | 0.995–1.001 | 0.996, 1.000 |  |
| set.insert.fresh | 28.56 | 28.53 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.insert.reused | 23.45 | 23.46 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| str.append | 1.528 | 1.538 | 1.006 | 1.002–1.010 | 1.015, 0.998 |  |
| str.assign_long | 2.936 | 2.936 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| str.assign_short | 0.9218 | 0.925 | 1.003 | 1.001–1.005 | 1.007, 1.000 |  |
| str.assign_short.copy | 0.6114 | 0.6144 | 1.005 | 1.001–1.009 | 1.013, 0.997 |  |
| str.assign_short.view | 0.9326 | 0.9336 | 1.001 | 0.998–1.005 | 1.003, 0.999 |  |
| vec.iterate | 0.03323 | 0.03326 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| vec.mid_erase | 40.67 | 40.65 | 0.999 | 0.998–1.001 | 0.993, 1.006 |  |
| vec.mid_insert | 40.67 | 40.66 | 1.000 | 0.998–1.001 | 0.997, 1.002 |  |
| vec.push_back | 0.272 | 0.2716 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| vec.push_back_grow | 0.2716 | 0.2685 | 0.988 | 0.986–0.990 | 0.990, 0.987 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.018; reset 1.035

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.erase.fresh (XOffset ns/op) | 1.010 | 0.999 | 0.996–1.001 | not worse |
| map.find.fresh (XOffset ns/op) | 1.010 | 0.999 | 0.998–1.001 | not worse |
| set.erase.reused (XOffset ns/op) | 1.010 | 0.995 | 0.992–0.998 | not worse |
| set.find.fresh (XOffset ns/op) | 1.011 | 0.999 | 0.997–1.000 | not worse |
| set.find.reused (XOffset ns/op) | 1.009 | 0.998 | 0.995–1.001 | not worse |
| set.insert.fresh (XOffset ns/op) | 1.012 | 0.999 | 0.997–1.001 | not worse |
| str.assign_short.view (XOffset ns/op) | 1.003 | 1.001 | 0.998–1.005 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.013 | 1.016 | 1.013–1.018 | still worse |
| set.find.fresh / std::set (report) | 1.007 | 0.999 | 0.995–1.004 | not worse |
| set.find.reused / std::set (report) | 1.162 | 1.015 | 1.011–1.019 | still worse |
| str.assign_short / std::string (report) | 1.008 | 0.999 | 0.997–1.002 | not worse |
| vec.mid_insert / std::vector (G9) | 1.041 | 1.040 | 1.038–1.042 | still worse |
| vec.push_back / std::vector (G9) | 1.006 | 0.994 | 0.991–0.996 | not worse |
