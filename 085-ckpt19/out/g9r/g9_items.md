## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.721 | 0.727 | 1.008 | 1.006–1.010 | 1.003, 1.013 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.757 | 0.759 | 1.004 | 0.995–1.012 | 0.999, 1.008 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.381 | 1.047 | 1.036–1.059 | 1.012, 1.083 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.938 | 0.997 | 0.994–1.001 | 0.996, 0.998 |  |
| map.find.fresh | std::flat_map | G9 | 0.806 | 0.808 | 1.002 | 1.000–1.005 | 1.001, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.876 | 0.871 | 0.995 | 0.991–0.998 | 1.000, 0.990 |  |
| nested.build.ord | std | G9 | 0.321 | 0.329 | 1.026 | 1.021–1.032 | 1.005, 1.048 | worse |
| reset | std | G9 | 0.224 | 0.237 | 1.058 | 1.040–1.077 | 1.108, 1.012 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.089 | 1.088 | 0.999 | 0.995–1.002 | 0.999, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| set.insert.fresh | std::flat_set | G9 | 0.996 | 0.997 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| str.append | std::string | G9 | 0.424 | 0.417 | 0.983 | 0.978–0.989 | 0.972, 0.995 |  |
| str.assign_long | std::string | G9 | 0.801 | 0.865 | 1.080 | 1.078–1.083 | 1.081, 1.080 | worse |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.998–1.005 | 1.003, 1.000 |  |
| vec.iterate | std::vector | G9 | 0.983 | 1.001 | 1.018 | 1.015–1.022 | 1.019, 1.018 | worse |
| vec.mid_erase | std::vector | G9 | 1.015 | 1.016 | 1.001 | 0.997–1.004 | 0.998, 1.003 |  |
| vec.mid_insert | std::vector | G9 | 0.975 | 0.956 | 0.981 | 0.976–0.985 | 0.972, 0.989 |  |
| vec.push_back | std::vector | G9 | 1.143 | 1.111 | 0.972 | 0.968–0.976 | 0.971, 0.973 |  |
| vec.push_back_grow | std::vector | G9 | 0.762 | 0.767 | 1.007 | 1.003–1.011 | 1.008, 1.006 | worse |
| hero.mid_erase.plain | std::vector | report | 4.011 | 4.044 | 1.008 | 1.000–1.017 | 0.999, 1.017 |  |
| hero.mid_insert.plain | std::vector | report | 4.157 | 4.226 | 1.017 | 1.012–1.022 | 1.011, 1.023 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.405 | 0.409 | 1.011 | 1.007–1.014 | 0.992, 1.029 | worse? |
| map.erase.fresh | std::map | report | 0.737 | 0.740 | 1.005 | 1.001–1.008 | 1.005, 1.004 | worse |
| map.erase.reused | std::flat_map | report | 1.077 | 1.077 | 1.000 | 0.996–1.003 | 1.001, 0.998 |  |
| map.erase.reused | std::map | report | 1.617 | 1.626 | 1.005 | 0.998–1.013 | 1.013, 0.998 |  |
| map.find.fresh | std::map | report | 0.323 | 0.326 | 1.007 | 1.003–1.012 | 1.007, 1.008 | worse |
| map.find.reused | std::flat_map | report | 0.807 | 0.806 | 0.999 | 0.996–1.002 | 0.998, 1.000 |  |
| map.find.reused | std::map | report | 0.730 | 0.746 | 1.021 | 1.017–1.025 | 1.021, 1.021 | worse |
| map.insert.fresh | std::map | report | 0.743 | 0.755 | 1.016 | 1.012–1.021 | 1.017, 1.016 | worse |
| map.insert.reused | std::flat_map | report | 1.003 | 1.003 | 1.000 | 0.997–1.003 | 0.999, 1.002 |  |
| map.insert.reused | std::map | report | 1.763 | 1.773 | 1.006 | 0.998–1.014 | 0.983, 1.029 |  |
| nested.build.plain | std | report | 0.334 | 0.337 | 1.009 | 1.003–1.015 | 0.991, 1.027 |  |
| set.erase.fresh | std::set | report | 0.668 | 0.670 | 1.002 | 0.998–1.006 | 1.001, 1.003 |  |
| set.erase.reused | std::flat_set | report | 1.139 | 1.138 | 0.999 | 0.994–1.004 | 1.000, 0.997 |  |
| set.erase.reused | std::set | report | 1.331 | 1.343 | 1.010 | 1.000–1.019 | 1.010, 1.010 | worse |
| set.find.fresh | std::set | report | 0.301 | 0.301 | 1.001 | 0.997–1.006 | 1.003, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.755 | 0.998 | 0.995–1.002 | 0.997, 1.000 |  |
| set.find.reused | std::set | report | 0.657 | 0.711 | 1.081 | 1.076–1.087 | 1.084, 1.078 | worse |
| set.insert.fresh | std::set | report | 0.650 | 0.650 | 1.001 | 0.997–1.004 | 0.992, 1.009 |  |
| set.insert.reused | std::flat_set | report | 1.082 | 1.086 | 1.003 | 0.999–1.007 | 1.000, 1.007 |  |
| set.insert.reused | std::set | report | 1.402 | 1.404 | 1.001 | 0.992–1.010 | 0.980, 1.023 |  |
| str.assign_short | std::string | report | 1.768 | 1.776 | 1.004 | 1.000–1.009 | 0.999, 1.009 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.142 | 0.993 | 0.988–0.997 | 0.993, 0.992 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.008; hero.push_back_grow.ord / std::vector (G9) 1.047; nested.build.ord / std (G9) 1.026; reset / std (G9) 1.058; str.assign_long / std::string (G9) 1.080; vec.iterate / std::vector (G9) 1.018; vec.push_back_grow / std::vector (G9) 1.007; hero.mid_insert.plain / std::vector (report) 1.017; map.erase.fresh / std::map (report) 1.005; map.find.fresh / std::map (report) 1.007; map.find.reused / std::map (report) 1.021; map.insert.fresh / std::map (report) 1.016; set.erase.reused / std::set (report) 1.010; set.find.reused / std::set (report) 1.081

above 1.01 but not worse by the rule: hero.push_back_grow.plain / std::vector (report) 1.011

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.1 | 164.3 | 1.008 | 1.007–1.008 | 1.006, 1.009 | worse |
| hero.mid_erase.plain | 914.5 | 965.6 | 1.056 | 1.037–1.075 | 0.988, 1.128 | worse? |
| hero.mid_insert.ord | 228.7 | 244.6 | 1.069 | 1.049–1.090 | 0.993, 1.152 | worse? |
| hero.mid_insert.plain | 1261 | 1270 | 1.007 | 1.004–1.011 | 0.998, 1.017 |  |
| hero.push_back_grow.ord | 32.21 | 35.92 | 1.115 | 1.089–1.142 | 0.993, 1.253 | worse? |
| hero.push_back_grow.plain | 36.04 | 35.41 | 0.982 | 0.980–0.985 | 0.967, 0.998 |  |
| map.erase.fresh | 33 | 33.09 | 1.003 | 1.000–1.005 | 1.001, 1.004 | worse |
| map.erase.reused | 29.58 | 29.69 | 1.004 | 1.002–1.006 | 1.004, 1.004 | worse |
| map.find.fresh | 5.285 | 5.296 | 1.002 | 1.000–1.004 | 1.002, 1.002 | worse |
| map.find.reused | 5.304 | 5.302 | 1.000 | 0.998–1.002 | 0.999, 1.000 |  |
| map.insert.fresh | 32.27 | 32.34 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| map.insert.reused | 29.54 | 29.64 | 1.004 | 1.001–1.006 | 1.005, 1.002 | worse |
| nested.build.ord | 274.1 | 274.9 | 1.003 | 1.001–1.006 | 1.004, 1.002 | worse |
| nested.build.plain | 283.1 | 281.4 | 0.994 | 0.990–0.998 | 0.995, 0.994 |  |
| reset | 29.21 | 29.6 | 1.014 | 1.004–1.023 | 1.020, 1.008 | worse |
| set.erase.fresh | 29.86 | 29.87 | 1.000 | 0.997–1.004 | 1.002, 0.999 |  |
| set.erase.reused | 23.29 | 23.29 | 1.000 | 0.996–1.004 | 1.000, 0.999 |  |
| set.find.fresh | 4.878 | 4.895 | 1.004 | 1.002–1.006 | 1.002, 1.006 | worse |
| set.find.reused | 4.888 | 4.888 | 1.000 | 0.998–1.003 | 0.999, 1.001 |  |
| set.insert.fresh | 28.93 | 28.87 | 0.998 | 0.996–1.000 | 0.997, 0.999 |  |
| set.insert.reused | 22.91 | 22.9 | 1.000 | 0.996–1.003 | 0.997, 1.002 |  |
| str.append | 1.605 | 1.564 | 0.974 | 0.970–0.979 | 0.969, 0.979 |  |
| str.assign_long | 2.765 | 2.948 | 1.066 | 1.063–1.070 | 1.067, 1.066 | worse |
| str.assign_short | 0.9644 | 0.9581 | 0.994 | 0.988–0.999 | 0.992, 0.995 |  |
| str.assign_short.copy | 0.6395 | 0.6314 | 0.987 | 0.982–0.993 | 0.995, 0.980 |  |
| str.assign_short.view | 0.9691 | 0.959 | 0.990 | 0.985–0.995 | 0.992, 0.987 |  |
| vec.iterate | 0.03259 | 0.03323 | 1.020 | 1.016–1.023 | 1.022, 1.017 | worse |
| vec.mid_erase | 40.29 | 40.37 | 1.002 | 0.999–1.004 | 1.001, 1.003 |  |
| vec.mid_insert | 40.93 | 40.42 | 0.988 | 0.984–0.991 | 0.980, 0.995 |  |
| vec.push_back | 0.2978 | 0.2906 | 0.976 | 0.964–0.987 | 0.983, 0.969 |  |
| vec.push_back_grow | 0.2767 | 0.2764 | 0.999 | 0.993–1.005 | 1.001, 0.997 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.008; map.erase.fresh 1.003; map.erase.reused 1.004; map.find.fresh 1.002; map.insert.reused 1.004; nested.build.ord 1.003; reset 1.014; set.find.fresh 1.004; str.assign_long 1.066; vec.iterate 1.020

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.729 | 0.727 | 0.997 | 0.994–1.000 | 1.007, 0.987 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.757 | 0.768 | 1.014 | 1.011–1.018 | 1.016, 1.013 | worse |
| hero.push_back_grow.ord | std::vector | G9 | 0.363 | 0.369 | 1.017 | 1.014–1.020 | 1.022, 1.012 | worse |
| map.erase.fresh | std::flat_map | G9 | 0.944 | 0.940 | 0.996 | 0.993–0.998 | 0.996, 0.996 |  |
| map.find.fresh | std::flat_map | G9 | 0.808 | 0.808 | 1.001 | 0.998–1.004 | 1.001, 1.001 |  |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.871 | 1.006 | 1.002–1.010 | 1.008, 1.003 | worse |
| nested.build.ord | std | G9 | 0.321 | 0.327 | 1.018 | 1.014–1.022 | 1.007, 1.029 | worse |
| reset | std | G9 | 0.227 | 0.246 | 1.087 | 1.077–1.098 | 1.088, 1.087 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.094 | 1.093 | 0.999 | 0.996–1.002 | 1.001, 0.997 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 0.999 | 0.996–1.003 | 0.999, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 0.998 | 0.998 | 0.996–1.001 | 0.997, 0.999 |  |
| str.append | std::string | G9 | 0.435 | 0.430 | 0.989 | 0.983–0.995 | 0.979, 0.999 |  |
| str.assign_long | std::string | G9 | 0.867 | 0.865 | 0.999 | 0.995–1.002 | 0.999, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.344 | 0.343 | 0.998 | 0.994–1.002 | 0.997, 0.999 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.001 | 0.999 | 0.996–1.002 | 0.999, 0.999 |  |
| vec.mid_erase | std::vector | G9 | 1.021 | 1.016 | 0.995 | 0.992–0.998 | 0.997, 0.993 |  |
| vec.mid_insert | std::vector | G9 | 0.959 | 0.960 | 1.001 | 0.999–1.004 | 1.000, 1.003 |  |
| vec.push_back | std::vector | G9 | 1.115 | 1.119 | 1.004 | 1.001–1.007 | 1.012, 0.996 |  |
| vec.push_back_grow | std::vector | G9 | 0.760 | 0.770 | 1.012 | 1.008–1.017 | 1.012, 1.013 | worse |
| hero.mid_erase.plain | std::vector | report | 4.057 | 4.010 | 0.988 | 0.984–0.993 | 0.988, 0.989 |  |
| hero.mid_insert.plain | std::vector | report | 4.227 | 4.182 | 0.989 | 0.984–0.995 | 0.983, 0.996 |  |
| hero.push_back_grow.plain | std::vector | report | 0.403 | 0.408 | 1.014 | 1.008–1.019 | 1.014, 1.013 | worse |
| map.erase.fresh | std::map | report | 0.745 | 0.743 | 0.997 | 0.994–1.000 | 1.008, 0.985 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.084 | 1.004 | 1.000–1.007 | 1.005, 1.002 | worse |
| map.erase.reused | std::map | report | 1.573 | 1.639 | 1.042 | 1.034–1.050 | 1.031, 1.054 | worse |
| map.find.fresh | std::map | report | 0.323 | 0.324 | 1.004 | 0.999–1.008 | 1.007, 1.001 |  |
| map.find.reused | std::flat_map | report | 0.807 | 0.806 | 0.999 | 0.995–1.002 | 1.001, 0.997 |  |
| map.find.reused | std::map | report | 0.743 | 0.729 | 0.981 | 0.977–0.985 | 0.971, 0.991 |  |
| map.insert.fresh | std::map | report | 0.732 | 0.735 | 1.004 | 1.000–1.008 | 1.014, 0.994 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 0.998 | 0.994 | 0.989–0.998 | 0.988, 0.999 |  |
| map.insert.reused | std::map | report | 1.827 | 1.830 | 1.002 | 0.994–1.010 | 1.018, 0.986 |  |
| nested.build.plain | std | report | 0.334 | 0.335 | 1.002 | 0.998–1.007 | 0.986, 1.019 |  |
| set.erase.fresh | std::set | report | 0.669 | 0.670 | 1.002 | 0.998–1.007 | 1.001, 1.003 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.140 | 1.002 | 0.998–1.007 | 1.003, 1.001 |  |
| set.erase.reused | std::set | report | 1.304 | 1.359 | 1.042 | 1.033–1.051 | 1.049, 1.035 | worse |
| set.find.fresh | std::set | report | 0.299 | 0.300 | 1.002 | 0.997–1.007 | 1.002, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 0.999 | 0.997–1.002 | 0.997, 1.001 |  |
| set.find.reused | std::set | report | 0.681 | 0.667 | 0.979 | 0.975–0.983 | 1.004, 0.956 |  |
| set.insert.fresh | std::set | report | 0.653 | 0.655 | 1.003 | 1.000–1.006 | 1.009, 0.997 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.083 | 1.002 | 0.999–1.005 | 0.999, 1.005 |  |
| set.insert.reused | std::set | report | 1.402 | 1.451 | 1.035 | 1.026–1.044 | 1.043, 1.027 | worse |
| str.assign_short | std::string | report | 1.767 | 1.766 | 0.999 | 0.995–1.003 | 0.997, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.152 | 1.001 | 0.999–1.004 | 1.002, 1.000 |  |

worse at t1a than t0a (by the rule): hero.mid_insert.ord / std::vector (G9) 1.014; hero.push_back_grow.ord / std::vector (G9) 1.017; map.insert.fresh / std::flat_map (G9) 1.006; nested.build.ord / std (G9) 1.018; reset / std (G9) 1.087; vec.push_back_grow / std::vector (G9) 1.012; hero.push_back_grow.plain / std::vector (report) 1.014; map.erase.reused / std::flat_map (report) 1.004; map.erase.reused / std::map (report) 1.042; set.erase.reused / std::set (report) 1.042; set.insert.reused / std::set (report) 1.035

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.8 | 165 | 1.001 | 0.999–1.004 | 1.008, 0.995 |  |
| hero.mid_erase.plain | 926.3 | 916.5 | 0.989 | 0.985–0.993 | 0.998, 0.981 |  |
| hero.mid_insert.ord | 230 | 232 | 1.008 | 1.006–1.011 | 1.012, 1.005 | worse |
| hero.mid_insert.plain | 1289 | 1263 | 0.980 | 0.976–0.984 | 0.981, 0.978 |  |
| hero.push_back_grow.ord | 31.88 | 32.11 | 1.007 | 1.004–1.010 | 1.014, 1.000 | worse |
| hero.push_back_grow.plain | 35.46 | 35.56 | 1.003 | 0.997–1.008 | 0.997, 1.008 |  |
| map.erase.fresh | 33.02 | 33.02 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.erase.reused | 29.65 | 29.73 | 1.003 | 1.000–1.005 | 1.003, 1.002 |  |
| map.find.fresh | 5.283 | 5.299 | 1.003 | 1.001–1.005 | 1.001, 1.005 | worse |
| map.find.reused | 5.308 | 5.305 | 1.000 | 0.997–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | 32.34 | 32.23 | 0.997 | 0.994–0.999 | 0.997, 0.997 |  |
| map.insert.reused | 29.65 | 29.52 | 0.996 | 0.993–0.998 | 0.992, 0.999 |  |
| nested.build.ord | 268.2 | 270.9 | 1.010 | 1.007–1.013 | 1.001, 1.019 | worse |
| nested.build.plain | 277.8 | 278.6 | 1.003 | 1.000–1.007 | 0.999, 1.007 |  |
| reset | 28.96 | 29.34 | 1.013 | 1.007–1.019 | 0.995, 1.031 | worse? |
| set.erase.fresh | 29.8 | 29.81 | 1.000 | 0.998–1.003 | 1.001, 0.999 |  |
| set.erase.reused | 23.24 | 23.31 | 1.003 | 0.999–1.006 | 1.005, 1.001 |  |
| set.find.fresh | 4.891 | 4.889 | 1.000 | 0.997–1.003 | 0.999, 1.001 |  |
| set.find.reused | 4.884 | 4.886 | 1.000 | 0.998–1.002 | 0.999, 1.002 |  |
| set.insert.fresh | 28.91 | 28.88 | 0.999 | 0.998–1.001 | 0.998, 1.000 |  |
| set.insert.reused | 22.8 | 22.87 | 1.003 | 1.002–1.004 | 1.002, 1.004 | worse |
| str.append | 1.605 | 1.583 | 0.986 | 0.981–0.991 | 0.984, 0.988 |  |
| str.assign_long | 2.996 | 2.979 | 0.994 | 0.990–0.999 | 0.995, 0.993 |  |
| str.assign_short | 0.966 | 0.9566 | 0.990 | 0.985–0.996 | 0.996, 0.984 |  |
| str.assign_short.copy | 0.6426 | 0.6332 | 0.985 | 0.981–0.990 | 0.995, 0.976 |  |
| str.assign_short.view | 0.9686 | 0.9603 | 0.992 | 0.986–0.997 | 0.998, 0.985 |  |
| vec.iterate | 0.03329 | 0.03326 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| vec.mid_erase | 40.64 | 40.59 | 0.999 | 0.997–1.000 | 1.004, 0.993 |  |
| vec.mid_insert | 40.6 | 40.66 | 1.002 | 1.000–1.003 | 1.000, 1.003 |  |
| vec.push_back | 0.272 | 0.272 | 1.000 | 0.999–1.001 | 1.000, 1.001 |  |
| vec.push_back_grow | 0.2708 | 0.2711 | 1.001 | 0.998–1.005 | 1.001, 1.001 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_insert.ord 1.008; hero.push_back_grow.ord 1.007; map.find.fresh 1.003; nested.build.ord 1.010; set.insert.reused 1.003

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.008 | 1.001 | 0.999–1.004 | not worse |
| map.erase.fresh (XOffset ns/op) | 1.003 | 1.000 | 0.998–1.002 | not worse |
| map.erase.reused (XOffset ns/op) | 1.004 | 1.003 | 1.000–1.005 | not worse |
| map.find.fresh (XOffset ns/op) | 1.002 | 1.003 | 1.001–1.005 | still worse |
| map.insert.reused (XOffset ns/op) | 1.004 | 0.996 | 0.993–0.998 | not worse |
| nested.build.ord (XOffset ns/op) | 1.003 | 1.010 | 1.007–1.013 | still worse |
| reset (XOffset ns/op) | 1.014 | 1.013 | 1.007–1.019 | worse? |
| set.find.fresh (XOffset ns/op) | 1.004 | 1.000 | 0.997–1.003 | not worse |
| str.assign_long (XOffset ns/op) | 1.066 | 0.994 | 0.990–0.999 | not worse |
| vec.iterate (XOffset ns/op) | 1.020 | 0.999 | 0.997–1.001 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.008 | 0.997 | 0.994–1.000 | not worse |
| hero.mid_insert.plain / std::vector (report) | 1.017 | 0.989 | 0.984–0.995 | not worse |
| hero.push_back_grow.ord / std::vector (G9) | 1.047 | 1.017 | 1.014–1.020 | still worse |
| map.erase.fresh / std::map (report) | 1.005 | 0.997 | 0.994–1.000 | not worse |
| map.find.fresh / std::map (report) | 1.007 | 1.004 | 0.999–1.008 | not worse |
| map.find.reused / std::map (report) | 1.021 | 0.981 | 0.977–0.985 | not worse |
| map.insert.fresh / std::map (report) | 1.016 | 1.004 | 1.000–1.008 | not worse |
| nested.build.ord / std (G9) | 1.026 | 1.018 | 1.014–1.022 | still worse |
| reset / std (G9) | 1.058 | 1.087 | 1.077–1.098 | still worse |
| set.erase.reused / std::set (report) | 1.010 | 1.042 | 1.033–1.051 | still worse |
| set.find.reused / std::set (report) | 1.081 | 0.979 | 0.975–0.983 | not worse |
| str.assign_long / std::string (G9) | 1.080 | 0.999 | 0.995–1.002 | not worse |
| vec.iterate / std::vector (G9) | 1.018 | 0.999 | 0.996–1.002 | not worse |
| vec.push_back_grow / std::vector (G9) | 1.007 | 1.012 | 1.008–1.017 | still worse |
