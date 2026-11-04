## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.732 | 1.005 | 1.002–1.009 | 1.002, 1.009 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.759 | 0.998 | 0.994–1.001 | 0.996, 1.000 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.369 | 0.364 | 0.987 | 0.984–0.990 | 0.976, 0.997 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.936 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.807 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.872 | 1.006 | 1.003–1.008 | 1.006, 1.005 | worse |
| nested.build.ord | std | G9 | 0.329 | 0.330 | 1.005 | 1.002–1.008 | 1.014, 0.996 |  |
| reset | std | G9 | 0.235 | 0.230 | 0.977 | 0.965–0.991 | 0.891, 1.072 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.087 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.998 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| str.append | std::string | G9 | 0.424 | 0.349 | 0.825 | 0.821–0.829 | 0.848, 0.802 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.866 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.998–1.003 | 1.000, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 1.000 | 1.000–1.001 | 1.001, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.020 | 1.000 | 0.998–1.002 | 1.003, 0.997 |  |
| vec.mid_insert | std::vector | G9 | 0.955 | 0.977 | 1.022 | 1.017–1.028 | 1.032, 1.013 | worse |
| vec.push_back | std::vector | G9 | 1.112 | 1.108 | 0.996 | 0.993–0.999 | 0.991, 1.002 |  |
| vec.push_back_grow | std::vector | G9 | 0.769 | 0.482 | 0.626 | 0.625–0.628 | 0.620, 0.633 |  |
| hero.mid_erase.plain | std::vector | report | 4.051 | 4.050 | 1.000 | 0.996–1.003 | 0.998, 1.002 |  |
| hero.mid_insert.plain | std::vector | report | 4.242 | 4.256 | 1.003 | 0.999–1.007 | 0.997, 1.010 |  |
| hero.push_back_grow.plain | std::vector | report | 0.406 | 0.402 | 0.991 | 0.988–0.994 | 0.983, 0.998 |  |
| map.erase.fresh | std::map | report | 0.739 | 0.743 | 1.005 | 1.002–1.008 | 1.002, 1.008 | worse |
| map.erase.reused | std::flat_map | report | 1.085 | 1.079 | 0.995 | 0.992–0.998 | 0.997, 0.993 |  |
| map.erase.reused | std::map | report | 1.648 | 1.636 | 0.993 | 0.987–0.999 | 0.979, 1.007 |  |
| map.find.fresh | std::map | report | 0.326 | 0.325 | 1.000 | 0.996–1.004 | 0.999, 1.001 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.808 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| map.find.reused | std::map | report | 0.721 | 0.742 | 1.030 | 1.026–1.033 | 1.042, 1.017 | worse |
| map.insert.fresh | std::map | report | 0.749 | 0.752 | 1.004 | 1.002–1.007 | 1.005, 1.004 | worse |
| map.insert.reused | std::flat_map | report | 0.999 | 1.001 | 1.002 | 1.000–1.004 | 1.004, 1.001 | worse |
| map.insert.reused | std::map | report | 1.832 | 1.835 | 1.002 | 0.997–1.006 | 0.991, 1.012 |  |
| nested.build.plain | std | report | 0.338 | 0.339 | 1.004 | 1.000–1.008 | 1.001, 1.007 |  |
| set.erase.fresh | std::set | report | 0.670 | 0.667 | 0.995 | 0.992–0.999 | 0.984, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.137 | 0.999 | 0.996–1.002 | 0.996, 1.002 |  |
| set.erase.reused | std::set | report | 1.309 | 1.338 | 1.022 | 1.014–1.031 | 1.008, 1.037 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.301 | 1.008 | 1.003–1.013 | 1.011, 1.006 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| set.find.reused | std::set | report | 0.677 | 0.672 | 0.993 | 0.988–0.998 | 0.941, 1.047 |  |
| set.insert.fresh | std::set | report | 0.652 | 0.656 | 1.006 | 1.004–1.009 | 1.009, 1.003 | worse |
| set.insert.reused | std::flat_set | report | 1.080 | 1.078 | 0.998 | 0.995–1.000 | 0.991, 1.005 |  |
| set.insert.reused | std::set | report | 1.454 | 1.454 | 1.000 | 0.993–1.008 | 0.991, 1.010 |  |
| str.assign_short | std::string | report | 1.762 | 1.761 | 1.000 | 0.996–1.003 | 1.002, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.151 | 1.000 | 0.997–1.003 | 0.997, 1.003 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; map.insert.fresh / std::flat_map (G9) 1.006; vec.mid_insert / std::vector (G9) 1.022; map.erase.fresh / std::map (report) 1.005; map.find.reused / std::map (report) 1.030; map.insert.fresh / std::map (report) 1.004; map.insert.reused / std::flat_map (report) 1.002; set.erase.reused / std::set (report) 1.022; set.find.fresh / std::set (report) 1.008; set.insert.fresh / std::set (report) 1.006

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.8 | 165.2 | 1.009 | 1.006–1.011 | 1.012, 1.006 | worse |
| hero.mid_erase.plain | 910.6 | 912.2 | 1.002 | 0.999–1.004 | 1.004, 0.999 |  |
| hero.mid_insert.ord | 228.1 | 227.5 | 0.997 | 0.996–0.999 | 0.996, 0.999 |  |
| hero.mid_insert.plain | 1269 | 1271 | 1.002 | 1.000–1.004 | 0.997, 1.006 |  |
| hero.push_back_grow.ord | 31.95 | 32.1 | 1.005 | 1.003–1.007 | 1.007, 1.003 | worse |
| hero.push_back_grow.plain | 35.25 | 35.34 | 1.003 | 1.001–1.005 | 1.004, 1.001 | worse |
| map.erase.fresh | 33.04 | 33 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| map.erase.reused | 29.79 | 29.68 | 0.996 | 0.994–0.999 | 0.997, 0.996 |  |
| map.find.fresh | 5.286 | 5.286 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.find.reused | 5.298 | 5.3 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| map.insert.fresh | 32.24 | 32.3 | 1.002 | 1.000–1.004 | 1.001, 1.003 | worse |
| map.insert.reused | 29.52 | 29.47 | 0.998 | 0.997–1.000 | 0.999, 0.998 |  |
| nested.build.ord | 271.2 | 273.1 | 1.007 | 1.005–1.009 | 1.009, 1.005 | worse |
| nested.build.plain | 280 | 282.7 | 1.010 | 1.007–1.012 | 1.010, 1.009 | worse |
| reset | 30.37 | 29.85 | 0.983 | 0.979–0.987 | 0.963, 1.003 |  |
| set.erase.fresh | 29.81 | 29.77 | 0.999 | 0.997–1.000 | 0.998, 0.999 |  |
| set.erase.reused | 23.23 | 23.21 | 0.999 | 0.997–1.002 | 0.996, 1.003 |  |
| set.find.fresh | 4.92 | 4.881 | 0.992 | 0.989–0.995 | 0.985, 0.999 |  |
| set.find.reused | 4.879 | 4.885 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.insert.fresh | 28.88 | 28.84 | 0.999 | 0.998–1.000 | 0.998, 0.999 |  |
| set.insert.reused | 22.9 | 22.79 | 0.995 | 0.994–0.997 | 0.993, 0.997 |  |
| str.append | 1.568 | 1.523 | 0.971 | 0.967–0.975 | 0.984, 0.959 |  |
| str.assign_long | 2.939 | 2.943 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| str.assign_short | 0.9323 | 0.9323 | 1.000 | 0.997–1.003 | 1.002, 0.998 |  |
| str.assign_short.copy | 0.624 | 0.623 | 0.998 | 0.994–1.003 | 1.000, 0.997 |  |
| str.assign_short.view | 0.9404 | 0.9436 | 1.003 | 1.000–1.007 | 1.001, 1.006 |  |
| vec.iterate | 0.03315 | 0.03331 | 1.005 | 1.003–1.007 | 1.009, 1.001 | worse |
| vec.mid_erase | 40.44 | 40.53 | 1.002 | 1.000–1.004 | 1.004, 1.000 | worse |
| vec.mid_insert | 40.43 | 40.74 | 1.008 | 1.003–1.013 | 1.012, 1.003 | worse |
| vec.push_back | 0.2912 | 0.2911 | 1.000 | 0.988–1.012 | 1.009, 0.991 |  |
| vec.push_back_grow | 0.2769 | 0.2763 | 0.998 | 0.992–1.004 | 1.001, 0.995 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.009; hero.push_back_grow.ord 1.005; hero.push_back_grow.plain 1.003; map.insert.fresh 1.002; nested.build.ord 1.007; nested.build.plain 1.010; vec.iterate 1.005; vec.mid_erase 1.002; vec.mid_insert 1.008

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.729 | 0.725 | 0.995 | 0.992–0.998 | 1.000, 0.990 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.761 | 1.001 | 0.998–1.005 | 0.998, 1.005 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.357 | 0.358 | 1.002 | 0.997–1.007 | 1.008, 0.996 |  |
| map.erase.fresh | std::flat_map | G9 | 0.942 | 0.944 | 1.002 | 1.000–1.003 | 1.000, 1.003 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.998–1.003 | 1.000, 1.002 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.869 | 1.002 | 0.999–1.004 | 1.004, 0.999 |  |
| nested.build.ord | std | G9 | 0.308 | 0.310 | 1.006 | 0.998–1.014 | 1.044, 0.969 |  |
| reset | std | G9 | 0.211 | 0.221 | 1.049 | 1.032–1.066 | 1.158, 0.949 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.089 | 0.998 | 0.995–1.001 | 0.997, 0.999 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.001 | 0.999–1.003 | 1.000, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 0.998 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| str.append | std::string | G9 | 0.427 | 0.424 | 0.993 | 0.988–0.999 | 1.003, 0.984 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.867 | 1.003 | 1.000–1.005 | 1.002, 1.003 | worse |
| str.assign_short.view | std::string | G9 | 0.343 | 0.340 | 0.993 | 0.990–0.995 | 0.987, 0.998 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.001 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.mid_erase | std::vector | G9 | 1.014 | 1.017 | 1.002 | 1.000–1.004 | 1.002, 1.003 | worse |
| vec.mid_insert | std::vector | G9 | 0.962 | 0.961 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| vec.push_back | std::vector | G9 | 1.119 | 1.119 | 1.001 | 0.997–1.004 | 1.003, 0.998 |  |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.765 | 0.999 | 0.995–1.003 | 1.000, 0.998 |  |
| hero.mid_erase.plain | std::vector | report | 4.054 | 4.048 | 0.999 | 0.996–1.002 | 0.996, 1.001 |  |
| hero.mid_insert.plain | std::vector | report | 4.168 | 4.205 | 1.009 | 1.005–1.013 | 1.010, 1.008 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.399 | 0.399 | 1.000 | 0.995–1.005 | 0.997, 1.003 |  |
| map.erase.fresh | std::map | report | 0.739 | 0.744 | 1.007 | 1.004–1.010 | 1.012, 1.002 | worse |
| map.erase.reused | std::flat_map | report | 1.079 | 1.084 | 1.004 | 1.001–1.007 | 1.004, 1.004 | worse |
| map.erase.reused | std::map | report | 1.587 | 1.560 | 0.983 | 0.975–0.991 | 0.994, 0.972 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 1.000 | 0.996–1.004 | 0.997, 1.003 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |
| map.find.reused | std::map | report | 0.717 | 0.717 | 1.000 | 0.995–1.005 | 1.019, 0.982 |  |
| map.insert.fresh | std::map | report | 0.742 | 0.733 | 0.987 | 0.984–0.991 | 0.971, 1.004 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 1.002 | 0.998 | 0.996–1.001 | 0.998, 0.999 |  |
| map.insert.reused | std::map | report | 1.784 | 1.787 | 1.001 | 0.995–1.008 | 1.011, 0.992 |  |
| nested.build.plain | std | report | 0.318 | 0.320 | 1.006 | 0.998–1.014 | 1.050, 0.963 |  |
| set.erase.fresh | std::set | report | 0.665 | 0.665 | 1.001 | 0.997–1.005 | 0.995, 1.007 |  |
| set.erase.reused | std::flat_set | report | 1.140 | 1.136 | 0.996 | 0.992–1.000 | 0.996, 0.996 |  |
| set.erase.reused | std::set | report | 1.286 | 1.278 | 0.994 | 0.986–1.003 | 1.005, 0.983 |  |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.003 | 0.998–1.007 | 1.004, 1.001 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.755 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| set.find.reused | std::set | report | 0.676 | 0.684 | 1.012 | 1.008–1.015 | 1.013, 1.010 | worse |
| set.insert.fresh | std::set | report | 0.646 | 0.655 | 1.014 | 1.010–1.017 | 1.002, 1.026 | worse |
| set.insert.reused | std::flat_set | report | 1.077 | 1.080 | 1.002 | 1.000–1.005 | 1.002, 1.003 | worse |
| set.insert.reused | std::set | report | 1.396 | 1.388 | 0.994 | 0.985–1.004 | 1.016, 0.973 |  |
| str.assign_short | std::string | report | 1.767 | 1.767 | 1.000 | 0.998–1.002 | 1.002, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.152 | 1.151 | 1.000 | 0.996–1.003 | 0.996, 1.003 |  |

worse at t1a than t0a (by the rule): str.assign_long / std::string (G9) 1.003; vec.mid_erase / std::vector (G9) 1.002; hero.mid_insert.plain / std::vector (report) 1.009; map.erase.fresh / std::map (report) 1.007; map.erase.reused / std::flat_map (report) 1.004; set.find.reused / std::set (report) 1.012; set.insert.fresh / std::set (report) 1.014; set.insert.reused / std::flat_set (report) 1.002

above 1.01 but not worse by the rule: reset / std (G9) 1.049

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.6 | 164.1 | 0.997 | 0.994–0.999 | 0.997, 0.996 |  |
| hero.mid_erase.plain | 909.7 | 910.2 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| hero.mid_insert.ord | 229.8 | 229.3 | 0.998 | 0.995–1.000 | 0.998, 0.997 |  |
| hero.mid_insert.plain | 1250 | 1261 | 1.008 | 1.006–1.010 | 1.012, 1.005 | worse |
| hero.push_back_grow.ord | 31.73 | 31.73 | 1.000 | 0.998–1.002 | 1.007, 0.994 |  |
| hero.push_back_grow.plain | 35.36 | 35.24 | 0.997 | 0.994–0.999 | 0.994, 0.999 |  |
| map.erase.fresh | 33.04 | 33.03 | 1.000 | 0.998–1.001 | 1.000, 0.999 |  |
| map.erase.reused | 29.62 | 29.75 | 1.004 | 1.003–1.006 | 1.007, 1.002 | worse |
| map.find.fresh | 5.291 | 5.287 | 0.999 | 0.997–1.001 | 0.997, 1.001 |  |
| map.find.reused | 5.301 | 5.309 | 1.001 | 1.000–1.003 | 1.000, 1.003 |  |
| map.insert.fresh | 32.23 | 32.35 | 1.004 | 1.002–1.006 | 1.002, 1.005 | worse |
| map.insert.reused | 29.49 | 29.56 | 1.002 | 1.000–1.004 | 1.001, 1.003 | worse |
| nested.build.ord | 267.2 | 268.7 | 1.006 | 1.004–1.008 | 1.003, 1.008 | worse |
| nested.build.plain | 274.2 | 276.6 | 1.009 | 1.005–1.012 | 1.007, 1.010 | worse |
| reset | 29.91 | 29.26 | 0.978 | 0.973–0.984 | 0.982, 0.974 |  |
| set.erase.fresh | 29.75 | 29.79 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| set.erase.reused | 23.26 | 23.2 | 0.998 | 0.994–1.001 | 0.999, 0.996 |  |
| set.find.fresh | 4.879 | 4.888 | 1.002 | 1.000–1.003 | 1.000, 1.003 |  |
| set.find.reused | 4.878 | 4.884 | 1.001 | 1.000–1.003 | 1.001, 1.002 |  |
| set.insert.fresh | 28.88 | 28.93 | 1.002 | 1.000–1.003 | 1.002, 1.002 |  |
| set.insert.reused | 22.77 | 22.79 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| str.append | 1.559 | 1.566 | 1.005 | 1.000–1.009 | 1.005, 1.004 | worse |
| str.assign_long | 2.941 | 2.953 | 1.004 | 1.002–1.007 | 1.004, 1.004 | worse |
| str.assign_short | 0.9386 | 0.9406 | 1.002 | 0.998–1.006 | 1.002, 1.002 |  |
| str.assign_short.copy | 0.6303 | 0.6279 | 0.996 | 0.991–1.002 | 0.994, 0.998 |  |
| str.assign_short.view | 0.9442 | 0.9438 | 1.000 | 0.996–1.003 | 0.997, 1.002 |  |
| vec.iterate | 0.03324 | 0.03324 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | 40.41 | 40.37 | 0.999 | 0.998–1.000 | 1.001, 0.997 |  |
| vec.mid_insert | 40.7 | 40.65 | 0.999 | 0.997–1.000 | 0.999, 0.999 |  |
| vec.push_back | 0.2721 | 0.2724 | 1.001 | 0.999–1.004 | 0.999, 1.003 |  |
| vec.push_back_grow | 0.2706 | 0.2705 | 0.999 | 0.996–1.003 | 1.004, 0.995 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_insert.plain 1.008; map.erase.reused 1.004; map.insert.fresh 1.004; map.insert.reused 1.002; nested.build.ord 1.006; nested.build.plain 1.009; str.append 1.005; str.assign_long 1.004

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.009 | 0.997 | 0.994–0.999 | not worse |
| hero.push_back_grow.ord (XOffset ns/op) | 1.005 | 1.000 | 0.998–1.002 | not worse |
| hero.push_back_grow.plain (XOffset ns/op) | 1.003 | 0.997 | 0.994–0.999 | not worse |
| map.insert.fresh (XOffset ns/op) | 1.002 | 1.004 | 1.002–1.006 | still worse |
| nested.build.ord (XOffset ns/op) | 1.007 | 1.006 | 1.004–1.008 | still worse |
| nested.build.plain (XOffset ns/op) | 1.010 | 1.009 | 1.005–1.012 | still worse |
| vec.iterate (XOffset ns/op) | 1.005 | 1.000 | 0.999–1.001 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.002 | 0.999 | 0.998–1.000 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.008 | 0.999 | 0.997–1.000 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.995 | 0.992–0.998 | not worse |
| map.erase.fresh / std::map (report) | 1.005 | 1.007 | 1.004–1.010 | still worse |
| map.find.reused / std::map (report) | 1.030 | 1.000 | 0.995–1.005 | not worse |
| map.insert.fresh / std::flat_map (G9) | 1.006 | 1.002 | 0.999–1.004 | not worse |
| map.insert.fresh / std::map (report) | 1.004 | 0.987 | 0.984–0.991 | not worse |
| map.insert.reused / std::flat_map (report) | 1.002 | 0.998 | 0.996–1.001 | not worse |
| set.erase.reused / std::set (report) | 1.022 | 0.994 | 0.986–1.003 | not worse |
| set.find.fresh / std::set (report) | 1.008 | 1.003 | 0.998–1.007 | not worse |
| set.insert.fresh / std::set (report) | 1.006 | 1.014 | 1.010–1.017 | still worse |
| vec.mid_insert / std::vector (G9) | 1.022 | 0.999 | 0.997–1.001 | not worse |
