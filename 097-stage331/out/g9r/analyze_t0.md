files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.140 |  1.143 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.959 |  0.961 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.55 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.728 |  0.730 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     294.27 |  0.758 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.74 |      86.30 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.43 |     224.61 |  4.055 |  4.068 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1246.41 |     304.69 |  4.192 |  4.208 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      85.98 |  0.410 |  0.411 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.47 |  0.433 |  0.435 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.861 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.89 |  0.737 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.54 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.31 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.995 |  0.997 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.34 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.68 |  0.662 |  0.665 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.19 |      29.38 |  1.027 |  1.029 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.19 |      16.28 |  1.847 |  1.852 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.809 |  0.812 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       6.46 |  0.825 |  0.829 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.82 |  1.629 |  1.639 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.75 |  1.470 |  1.481 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.19 |  0.664 |  0.668 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.09 |  1.315 |  1.324 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     825.50 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     119.14 |  0.209 |  0.212 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     824.86 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
