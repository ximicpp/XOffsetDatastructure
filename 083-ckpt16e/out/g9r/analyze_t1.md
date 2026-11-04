files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.141 |  1.144 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.772 |  0.774 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.980 |  0.985 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.32 |  0.969 |  0.971 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.024 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.97 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     297.53 |  0.759 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      87.97 |  0.363 |  0.363 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    894.53 |     224.61 |  3.992 |  4.003 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.214 |  4.231 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.68 |      87.97 |  0.405 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.774 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.339 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.149 |  1.151 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.64 |  0.423 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.19 |      36.78 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.19 |      42.81 |  0.748 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.41 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.742 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.11 |  0.653 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.670 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.007 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.36 |  1.757 |  1.770 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.43 |  0.715 |  0.720 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.637 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.082 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.87 |  1.394 |  1.406 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.39 |  0.659 |  0.661 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.35 |  1.140 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.347 |  1.358 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     839.84 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     117.84 |  0.232 |  0.234 | pass    |
| nested.build.plain           | std            | report |  808 |    282.56 |     834.66 |  0.339 |  0.340 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
