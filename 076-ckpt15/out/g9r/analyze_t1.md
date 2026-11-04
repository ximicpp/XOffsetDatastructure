files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.105 |  1.110 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.769 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.958 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.019 |  1.021 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.61 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     298.19 |  0.754 |  0.757 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      87.02 |  0.363 |  0.364 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    905.59 |     225.25 |  4.017 |  4.029 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1252.61 |     304.69 |  4.188 |  4.204 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      87.04 |  0.402 |  0.403 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.771 |  1.777 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.66 |       0.57 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.57 |       3.68 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.869 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.746 |  0.749 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.938 |  0.940 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.35 |  0.739 |  0.742 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.48 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.51 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.811 |  1.818 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.809 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.71 |  0.697 |  0.700 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.080 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.98 |  1.611 |  1.621 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.079 |  1.081 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.79 |  1.421 |  1.432 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.44 |  0.764 |  0.766 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.137 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.341 |  1.349 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     830.72 |  0.327 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.19 |  0.228 |  0.232 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     825.50 |  0.337 |  0.338 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
