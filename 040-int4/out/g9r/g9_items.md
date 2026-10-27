## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.753 | 0.718 | 0.954 | 0.948–0.959 | 0.947, 0.960 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.806 | 0.754 | 0.934 | 0.927–0.942 | 0.934, 0.935 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.422 | 0.364 | 0.862 | 0.855–0.870 | 0.864, 0.861 |  |
| map.erase.fresh | std::flat_map | G9 | 0.964 | 0.938 | 0.973 | 0.967–0.979 | 0.970, 0.976 |  |
| map.find.fresh | std::flat_map | G9 | 0.784 | 0.784 | 1.000 | 0.995–1.006 | 0.998, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.966 | 0.871 | 0.902 | 0.898–0.906 | 0.898, 0.906 |  |
| nested.build.ord | std | G9 | 0.430 | 0.333 | 0.773 | 0.767–0.779 | 0.780, 0.766 |  |
| reset | std | G9 | 0.208 | 0.186 | 0.893 | 0.886–0.901 | 0.911, 0.876 |  |
| set.erase.fresh | std::flat_set | G9 | 1.130 | 1.092 | 0.966 | 0.959–0.973 | 0.962, 0.970 |  |
| set.find.fresh | std::flat_set | G9 | 0.757 | 0.755 | 0.997 | 0.992–1.003 | 1.001, 0.994 |  |
| set.insert.fresh | std::flat_set | G9 | 1.111 | 0.995 | 0.895 | 0.888–0.902 | 0.895, 0.895 |  |
| str.append | std::string | G9 | 0.442 | 0.417 | 0.945 | 0.939–0.950 | 0.958, 0.931 |  |
| str.assign_long | std::string | G9 | 0.862 | 0.841 | 0.975 | 0.969–0.982 | 0.997, 0.955 |  |
| str.assign_short.view | std::string | G9 | 0.366 | 0.343 | 0.938 | 0.933–0.943 | 0.938, 0.938 |  |
| vec.iterate | std::vector | G9 | 1.002 | 0.951 | 0.949 | 0.946–0.953 | 0.951, 0.948 |  |
| vec.mid_erase | std::vector | G9 | 1.043 | 1.013 | 0.971 | 0.967–0.976 | 0.970, 0.972 |  |
| vec.mid_insert | std::vector | G9 | 1.092 | 0.954 | 0.873 | 0.868–0.878 | 0.871, 0.876 |  |
| vec.push_back | std::vector | G9 | 1.287 | 1.128 | 0.877 | 0.871–0.882 | 0.895, 0.858 |  |
| vec.push_back_grow | std::vector | G9 | 0.782 | 0.764 | 0.977 | 0.968–0.987 | 0.977, 0.978 |  |
| hero.mid_erase.plain | std::vector | report | 4.638 | 4.016 | 0.866 | 0.861–0.871 | 0.861, 0.871 |  |
| hero.mid_insert.plain | std::vector | report | 4.717 | 4.196 | 0.890 | 0.885–0.894 | 0.895, 0.884 |  |
| hero.push_back_grow.plain | std::vector | report | 0.469 | 0.406 | 0.866 | 0.862–0.870 | 0.866, 0.866 |  |
| map.erase.fresh | std::map | report | 0.713 | 0.697 | 0.978 | 0.971–0.984 | 0.969, 0.987 |  |
| map.erase.reused | std::flat_map | report | 1.100 | 1.090 | 0.991 | 0.986–0.997 | 0.999, 0.984 |  |
| map.erase.reused | std::map | report | 1.303 | 1.397 | 1.072 | 1.059–1.085 | 1.064, 1.080 | worse |
| map.find.fresh | std::map | report | 0.316 | 0.314 | 0.993 | 0.988–0.999 | 0.993, 0.993 |  |
| map.find.reused | std::flat_map | report | 0.783 | 0.784 | 1.001 | 0.996–1.006 | 1.001, 1.001 |  |
| map.find.reused | std::map | report | 0.707 | 0.725 | 1.024 | 1.016–1.033 | 0.990, 1.060 | worse? |
| map.insert.fresh | std::map | report | 0.811 | 0.733 | 0.904 | 0.899–0.909 | 0.907, 0.901 |  |
| map.insert.reused | std::flat_map | report | 1.132 | 1.004 | 0.887 | 0.883–0.890 | 0.885, 0.888 |  |
| map.insert.reused | std::map | report | 1.855 | 1.629 | 0.878 | 0.872–0.884 | 0.880, 0.877 |  |
| nested.build.plain | std | report | 0.456 | 0.341 | 0.749 | 0.743–0.754 | 0.754, 0.743 |  |
| set.erase.fresh | std::set | report | 0.654 | 0.636 | 0.972 | 0.964–0.979 | 0.970, 0.973 |  |
| set.erase.reused | std::flat_set | report | 1.144 | 1.149 | 1.004 | 0.999–1.009 | 0.998, 1.011 |  |
| set.erase.reused | std::set | report | 1.175 | 1.181 | 1.005 | 0.996–1.014 | 1.023, 0.986 |  |
| set.find.fresh | std::set | report | 0.301 | 0.301 | 1.001 | 0.995–1.006 | 1.003, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.752 | 0.994 | 0.989–0.999 | 0.993, 0.995 |  |
| set.find.reused | std::set | report | 0.681 | 0.687 | 1.009 | 1.002–1.017 | 1.021, 0.998 |  |
| set.insert.fresh | std::set | report | 0.722 | 0.638 | 0.885 | 0.879–0.890 | 0.879, 0.891 |  |
| set.insert.reused | std::flat_set | report | 1.274 | 1.081 | 0.849 | 0.844–0.854 | 0.847, 0.851 |  |
| set.insert.reused | std::set | report | 1.499 | 1.261 | 0.841 | 0.834–0.849 | 0.829, 0.854 |  |
| str.assign_short | std::string | report | 1.931 | 1.831 | 0.949 | 0.944–0.953 | 0.946, 0.951 |  |
| str.assign_short.copy | std::string | report | 1.596 | 1.187 | 0.744 | 0.739–0.748 | 0.744, 0.743 |  |

worse at t1 than t0 (by the rule): map.erase.reused / std::map (report) 1.072

above 1.01 but not worse by the rule: map.find.reused / std::map (report) 1.024

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 173.7 | 166.4 | 0.958 | 0.952–0.963 | 0.968, 0.948 |  |
| hero.mid_erase.plain | 1055 | 915.7 | 0.868 | 0.864–0.872 | 0.864, 0.872 |  |
| hero.mid_insert.ord | 246.9 | 231.2 | 0.936 | 0.930–0.942 | 0.936, 0.936 |  |
| hero.mid_insert.plain | 1426 | 1268 | 0.889 | 0.885–0.893 | 0.890, 0.888 |  |
| hero.push_back_grow.ord | 38.62 | 33.47 | 0.867 | 0.858–0.875 | 0.872, 0.862 |  |
| hero.push_back_grow.plain | 42.63 | 36.54 | 0.857 | 0.853–0.861 | 0.855, 0.859 |  |
| map.erase.fresh | 34.09 | 33.17 | 0.973 | 0.970–0.976 | 0.975, 0.971 |  |
| map.erase.reused | 30.3 | 29.93 | 0.988 | 0.982–0.994 | 0.994, 0.982 |  |
| map.find.fresh | 5.165 | 5.155 | 0.998 | 0.994–1.002 | 0.997, 0.999 |  |
| map.find.reused | 5.167 | 5.177 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| map.insert.fresh | 35.78 | 32.48 | 0.908 | 0.903–0.912 | 0.913, 0.903 |  |
| map.insert.reused | 33.34 | 29.66 | 0.890 | 0.886–0.893 | 0.891, 0.888 |  |
| nested.build.ord | 378.1 | 293.9 | 0.777 | 0.771–0.783 | 0.771, 0.784 |  |
| nested.build.plain | 393 | 297.8 | 0.758 | 0.753–0.763 | 0.757, 0.758 |  |
| reset | 34.31 | 30.62 | 0.892 | 0.885–0.900 | 0.902, 0.882 |  |
| set.erase.fresh | 30.97 | 30.14 | 0.973 | 0.967–0.980 | 0.970, 0.976 |  |
| set.erase.reused | 23.35 | 23.42 | 1.003 | 0.998–1.008 | 0.997, 1.009 |  |
| set.find.fresh | 4.912 | 4.912 | 1.000 | 0.996–1.004 | 1.002, 0.998 |  |
| set.find.reused | 4.903 | 4.902 | 1.000 | 0.996–1.003 | 0.998, 1.001 |  |
| set.insert.fresh | 32.45 | 29.17 | 0.899 | 0.894–0.904 | 0.899, 0.900 |  |
| set.insert.reused | 27.1 | 23.03 | 0.850 | 0.844–0.856 | 0.851, 0.848 |  |
| str.append | 1.574 | 1.539 | 0.978 | 0.972–0.983 | 0.999, 0.957 |  |
| str.assign_long | 2.965 | 2.96 | 0.999 | 0.994–1.003 | 0.998, 0.999 |  |
| str.assign_short | 1.015 | 0.9273 | 0.914 | 0.908–0.920 | 0.918, 0.910 |  |
| str.assign_short.copy | 0.8301 | 0.608 | 0.732 | 0.729–0.736 | 0.736, 0.729 |  |
| str.assign_short.view | 1.017 | 0.9372 | 0.922 | 0.915–0.929 | 0.916, 0.928 |  |
| vec.iterate | 0.03395 | 0.03258 | 0.960 | 0.952–0.967 | 0.970, 0.949 |  |
| vec.mid_erase | 41.88 | 40.7 | 0.972 | 0.968–0.976 | 0.968, 0.976 |  |
| vec.mid_insert | 45.33 | 41.02 | 0.905 | 0.901–0.909 | 0.901, 0.908 |  |
| vec.push_back | 0.3348 | 0.2961 | 0.884 | 0.875–0.894 | 0.886, 0.883 |  |
| vec.push_back_grow | 0.292 | 0.2849 | 0.976 | 0.967–0.984 | 0.974, 0.977 |  |

XOffset slower at t1 than t0 (worse by the rule): none

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.749 | 0.716 | 0.956 | 0.948–0.963 | 0.964, 0.947 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.807 | 0.761 | 0.943 | 0.937–0.949 | 0.941, 0.945 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.423 | 0.364 | 0.860 | 0.854–0.867 | 0.891, 0.830 |  |
| map.erase.fresh | std::flat_map | G9 | 0.966 | 0.939 | 0.972 | 0.966–0.978 | 0.971, 0.973 |  |
| map.find.fresh | std::flat_map | G9 | 0.781 | 0.786 | 1.006 | 1.001–1.011 | 1.009, 1.003 | worse |
| map.insert.fresh | std::flat_map | G9 | 0.965 | 0.873 | 0.904 | 0.899–0.910 | 0.912, 0.896 |  |
| nested.build.ord | std | G9 | 0.436 | 0.340 | 0.781 | 0.777–0.785 | 0.783, 0.779 |  |
| reset | std | G9 | 0.210 | 0.189 | 0.897 | 0.890–0.904 | 0.896, 0.899 |  |
| set.erase.fresh | std::flat_set | G9 | 1.126 | 1.093 | 0.971 | 0.965–0.976 | 0.965, 0.976 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.757 | 1.002 | 0.998–1.006 | 1.004, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 1.120 | 0.998 | 0.890 | 0.884–0.897 | 0.892, 0.889 |  |
| str.append | std::string | G9 | 0.426 | 0.423 | 0.993 | 0.988–0.999 | 0.986, 1.001 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.864 | 0.999 | 0.995–1.004 | 1.001, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.365 | 0.343 | 0.940 | 0.935–0.945 | 0.941, 0.939 |  |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 0.999 | 0.995–1.004 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.044 | 1.020 | 0.978 | 0.972–0.983 | 0.979, 0.976 |  |
| vec.mid_insert | std::vector | G9 | 1.092 | 0.962 | 0.881 | 0.876–0.886 | 0.887, 0.875 |  |
| vec.push_back | std::vector | G9 | 1.190 | 1.115 | 0.937 | 0.931–0.943 | 0.934, 0.940 |  |
| vec.push_back_grow | std::vector | G9 | 0.782 | 0.763 | 0.974 | 0.966–0.983 | 0.979, 0.970 |  |
| hero.mid_erase.plain | std::vector | report | 4.588 | 4.018 | 0.876 | 0.871–0.880 | 0.879, 0.872 |  |
| hero.mid_insert.plain | std::vector | report | 4.658 | 4.172 | 0.896 | 0.890–0.902 | 0.894, 0.897 |  |
| hero.push_back_grow.plain | std::vector | report | 0.476 | 0.407 | 0.855 | 0.850–0.860 | 0.838, 0.871 |  |
| map.erase.fresh | std::map | report | 0.724 | 0.694 | 0.958 | 0.951–0.965 | 0.959, 0.957 |  |
| map.erase.reused | std::flat_map | report | 1.103 | 1.081 | 0.980 | 0.976–0.985 | 0.973, 0.988 |  |
| map.erase.reused | std::map | report | 1.351 | 1.324 | 0.980 | 0.969–0.991 | 0.960, 1.000 |  |
| map.find.fresh | std::map | report | 0.314 | 0.315 | 1.003 | 0.996–1.010 | 1.004, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.782 | 0.786 | 1.005 | 1.000–1.010 | 1.007, 1.003 |  |
| map.find.reused | std::map | report | 0.709 | 0.716 | 1.009 | 1.001–1.017 | 1.047, 0.973 |  |
| map.insert.fresh | std::map | report | 0.819 | 0.742 | 0.906 | 0.900–0.912 | 0.908, 0.904 |  |
| map.insert.reused | std::flat_map | report | 1.136 | 1.009 | 0.888 | 0.884–0.892 | 0.888, 0.888 |  |
| map.insert.reused | std::map | report | 1.934 | 1.694 | 0.876 | 0.870–0.882 | 0.880, 0.871 |  |
| nested.build.plain | std | report | 0.461 | 0.345 | 0.748 | 0.743–0.752 | 0.734, 0.761 |  |
| set.erase.fresh | std::set | report | 0.658 | 0.641 | 0.974 | 0.969–0.979 | 0.973, 0.975 |  |
| set.erase.reused | std::flat_set | report | 1.151 | 1.154 | 1.003 | 0.998–1.008 | 1.002, 1.004 |  |
| set.erase.reused | std::set | report | 1.094 | 1.106 | 1.011 | 1.000–1.023 | 1.028, 0.994 | worse? |
| set.find.fresh | std::set | report | 0.299 | 0.289 | 0.965 | 0.959–0.971 | 0.968, 0.961 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.757 | 0.999 | 0.995–1.003 | 1.002, 0.997 |  |
| set.find.reused | std::set | report | 0.677 | 0.661 | 0.975 | 0.967–0.983 | 0.937, 1.015 |  |
| set.insert.fresh | std::set | report | 0.714 | 0.645 | 0.904 | 0.897–0.910 | 0.902, 0.906 |  |
| set.insert.reused | std::flat_set | report | 1.270 | 1.084 | 0.854 | 0.849–0.859 | 0.849, 0.859 |  |
| set.insert.reused | std::set | report | 1.534 | 1.285 | 0.838 | 0.828–0.848 | 0.823, 0.853 |  |
| str.assign_short | std::string | report | 1.923 | 1.829 | 0.951 | 0.943–0.959 | 0.950, 0.952 |  |
| str.assign_short.copy | std::string | report | 1.599 | 1.193 | 0.746 | 0.745–0.747 | 0.747, 0.745 |  |

worse at t1a than t0a (by the rule): map.find.fresh / std::flat_map (G9) 1.006

above 1.01 but not worse by the rule: set.erase.reused / std::set (report) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 173.4 | 165.9 | 0.957 | 0.949–0.964 | 0.958, 0.955 |  |
| hero.mid_erase.plain | 1047 | 919.2 | 0.878 | 0.874–0.882 | 0.877, 0.878 |  |
| hero.mid_insert.ord | 247.7 | 231.5 | 0.934 | 0.929–0.940 | 0.936, 0.933 |  |
| hero.mid_insert.plain | 1408 | 1262 | 0.896 | 0.892–0.900 | 0.900, 0.892 |  |
| hero.push_back_grow.ord | 38.32 | 33.26 | 0.868 | 0.863–0.873 | 0.872, 0.864 |  |
| hero.push_back_grow.plain | 42.7 | 36.64 | 0.858 | 0.854–0.862 | 0.860, 0.856 |  |
| map.erase.fresh | 34.2 | 33.21 | 0.971 | 0.965–0.976 | 0.970, 0.972 |  |
| map.erase.reused | 30.4 | 29.89 | 0.983 | 0.978–0.988 | 0.975, 0.992 |  |
| map.find.fresh | 5.152 | 5.161 | 1.002 | 0.997–1.007 | 1.005, 0.999 |  |
| map.find.reused | 5.174 | 5.178 | 1.001 | 0.997–1.005 | 1.004, 0.998 |  |
| map.insert.fresh | 35.71 | 32.68 | 0.915 | 0.909–0.922 | 0.921, 0.909 |  |
| map.insert.reused | 33.46 | 29.64 | 0.886 | 0.881–0.890 | 0.879, 0.892 |  |
| nested.build.ord | 372.8 | 294.7 | 0.791 | 0.787–0.794 | 0.790, 0.791 |  |
| nested.build.plain | 390.4 | 299.2 | 0.766 | 0.759–0.773 | 0.756, 0.777 |  |
| reset | 34.51 | 30.7 | 0.890 | 0.883–0.896 | 0.894, 0.886 |  |
| set.erase.fresh | 30.87 | 30.31 | 0.982 | 0.977–0.987 | 0.973, 0.991 |  |
| set.erase.reused | 23.48 | 23.66 | 1.008 | 1.001–1.014 | 1.007, 1.008 | worse |
| set.find.fresh | 4.901 | 4.917 | 1.003 | 0.999–1.007 | 1.004, 1.003 |  |
| set.find.reused | 4.911 | 4.897 | 0.997 | 0.993–1.001 | 1.000, 0.994 |  |
| set.insert.fresh | 32.46 | 29.24 | 0.901 | 0.894–0.908 | 0.903, 0.899 |  |
| set.insert.reused | 27.1 | 23.21 | 0.857 | 0.851–0.863 | 0.843, 0.870 |  |
| str.append | 1.555 | 1.528 | 0.982 | 0.977–0.987 | 0.978, 0.987 |  |
| str.assign_long | 2.97 | 2.94 | 0.990 | 0.985–0.994 | 0.990, 0.990 |  |
| str.assign_short | 0.9994 | 0.9363 | 0.937 | 0.929–0.945 | 0.939, 0.934 |  |
| str.assign_short.copy | 0.8199 | 0.6211 | 0.758 | 0.754–0.761 | 0.760, 0.755 |  |
| str.assign_short.view | 0.9982 | 0.9469 | 0.949 | 0.942–0.956 | 0.950, 0.947 |  |
| vec.iterate | 0.03327 | 0.0337 | 1.013 | 1.006–1.020 | 1.028, 0.998 | worse? |
| vec.mid_erase | 41.97 | 40.76 | 0.971 | 0.967–0.975 | 0.970, 0.972 |  |
| vec.mid_insert | 44.77 | 40.93 | 0.914 | 0.910–0.919 | 0.917, 0.912 |  |
| vec.push_back | 0.2926 | 0.2725 | 0.931 | 0.928–0.935 | 0.939, 0.924 |  |
| vec.push_back_grow | 0.2832 | 0.2752 | 0.972 | 0.964–0.979 | 0.977, 0.966 |  |

XOffset slower at t1a than t0a (worse by the rule): set.erase.reused 1.008

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.erase.reused / std::map (report) | 1.072 | 0.980 | 0.969–0.991 | not worse |
