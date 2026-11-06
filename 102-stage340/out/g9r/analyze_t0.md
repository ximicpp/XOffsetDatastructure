files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.120 |  1.123 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.964 |  0.966 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.39 |  1.019 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.31 |  0.728 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.84 |      86.47 |  0.370 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.94 |     224.61 |  4.034 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.54 |     291.67 |  4.223 |  4.239 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      86.22 |  0.410 |  0.411 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.54 |  0.431 |  0.433 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      37.03 |  0.861 |  0.862 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.48 |  0.750 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.37 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.88 |      34.91 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.88 |      44.11 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.997 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.653 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.757 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.28 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.87 |  0.678 |  0.679 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.027 |  1.029 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.11 |  1.850 |  1.862 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.809 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.19 |  0.726 |  0.731 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.38 |      27.34 |  1.073 |  1.076 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.38 |      17.42 |  1.674 |  1.681 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.112 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.79 |  1.446 |  1.459 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.35 |  0.669 |  0.672 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.79 |      20.35 |  1.125 |  1.128 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.79 |      16.85 |  1.356 |  1.362 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     832.03 |  0.325 |  0.326 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     119.80 |  0.225 |  0.228 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     829.44 |  0.336 |  0.337 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
