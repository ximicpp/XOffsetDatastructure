files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.148 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.762 |  0.765 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.983 |  0.984 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.975 |  0.980 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.63 |  1.015 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     225.27 |  0.721 |  0.723 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     305.33 |  0.757 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.15 |      88.66 |  0.363 |  0.364 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    905.27 |     225.27 |  4.011 |  4.025 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1253.27 |     305.33 |  4.157 |  4.178 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.93 |      88.87 |  0.405 |  0.406 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.768 |  1.774 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.66 |       0.57 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.62 |       3.77 |  0.424 |  0.426 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.78 |  0.876 |  0.878 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.13 |  0.743 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.806 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.39 |  0.323 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.996 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.11 |  0.650 |  0.653 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.089 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.27 |  0.668 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.003 |  1.005 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.44 |  1.763 |  1.776 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.32 |  0.730 |  0.733 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.077 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.617 |  1.630 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.082 |  1.087 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.87 |  1.402 |  1.415 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.47 |  0.657 |  0.660 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.139 |  1.145 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.77 |  1.331 |  1.345 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     837.25 |  0.321 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     118.50 |  0.224 |  0.228 | pass    |
| nested.build.plain           | std            | report |  808 |    282.53 |     832.03 |  0.334 |  0.336 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
