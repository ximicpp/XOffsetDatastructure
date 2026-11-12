files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.133 |  1.136 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.768 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.77 |  0.988 |  0.991 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.47 |  1.024 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.02 |     224.61 |  0.733 |  0.736 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.27 |      86.55 |  0.347 |  0.348 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    899.74 |     224.61 |  4.003 |  4.017 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1254.56 |     304.05 |  4.206 |  4.220 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.73 |      86.67 |  0.389 |  0.390 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.61 |  0.420 |  0.422 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.68 |  0.843 |  0.845 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      43.05 |  0.731 |  0.733 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.31 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.33 |  0.726 |  0.728 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.987 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      44.15 |  0.642 |  0.644 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.26 |  0.302 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.085 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      45.57 |  0.658 |  0.661 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.44 |      29.46 |  1.029 |  1.032 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.44 |      16.20 |  1.859 |  1.871 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.30 |  0.837 |  0.840 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      19.04 |  1.546 |  1.556 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.16 |  1.106 |  1.108 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.95 |  1.442 |  1.454 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.48 |  0.751 |  0.753 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.26 |  1.134 |  1.138 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      18.07 |  1.275 |  1.283 | report  |
| nested.build.ord             | std            | G9     |  808 |    257.81 |     821.62 |  0.313 |  0.314 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     153.64 |  0.171 |  0.174 | pass    |
| nested.build.plain           | std            | report |  808 |    264.34 |     830.72 |  0.319 |  0.320 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
