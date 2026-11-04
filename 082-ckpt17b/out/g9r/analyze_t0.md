files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.115 |  1.118 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.769 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.956 |  0.957 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.71 |  1.018 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     294.28 |  0.762 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.94 |      86.34 |  0.371 |  0.372 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.85 |     224.61 |  4.054 |  4.066 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.69 |  4.219 |  4.234 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.10 |  0.408 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.760 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.867 |  0.869 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.151 |  1.157 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.65 |  0.418 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.870 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.748 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.31 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.03 |  0.748 |  0.750 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.27 |  0.653 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.755 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.40 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.086 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.95 |  0.674 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.003 |  1.006 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.826 |  1.839 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.64 |  0.691 |  0.695 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.085 |  1.089 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.50 |  1.657 |  1.669 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.081 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.54 |  1.440 |  1.451 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.35 |  0.664 |  0.667 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.26 |  1.144 |  1.151 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      17.01 |  1.322 |  1.338 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     826.84 |  0.327 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.231 |  0.235 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     822.91 |  0.336 |  0.337 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
