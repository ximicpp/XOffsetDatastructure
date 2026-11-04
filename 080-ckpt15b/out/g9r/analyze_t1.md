files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.26 |  1.100 |  1.104 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.24 |      39.47 |  1.022 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.95 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.22 |  0.756 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      87.12 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.30 |     224.61 |  4.041 |  4.056 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1250.00 |     304.03 |  4.192 |  4.206 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.83 |  0.408 |  0.409 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.53 |  1.149 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.66 |  0.425 |  0.427 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.89 |  0.747 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.08 |  0.940 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.43 |  0.737 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.95 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.752 |  0.754 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.48 |  0.297 |  0.298 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.087 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.60 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.38 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.38 |      16.36 |  1.772 |  1.781 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.55 |  0.701 |  0.704 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.081 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.625 |  1.635 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.079 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.95 |  1.393 |  1.406 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.44 |  0.755 |  0.758 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.144 |  1.150 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.354 |  1.365 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     835.94 |  0.318 |  0.320 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     118.48 |  0.237 |  0.240 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     830.72 |  0.331 |  0.334 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
