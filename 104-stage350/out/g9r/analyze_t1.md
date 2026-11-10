files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.129 |  1.134 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.48 |  0.951 |  0.954 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.71 |  1.017 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     297.20 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.70 |      86.59 |  0.360 |  0.362 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.06 |     224.61 |  4.067 |  4.079 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1261.72 |     303.39 |  4.234 |  4.249 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.91 |      86.87 |  0.398 |  0.400 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.758 |  1.760 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.149 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.436 |  0.437 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.11 |  0.855 |  0.857 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.81 |  0.737 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.39 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.00 |  0.727 |  0.729 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.89 |  0.989 |  0.991 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      44.43 |  0.640 |  0.643 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.36 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.43 |  0.665 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.38 |  1.023 |  1.025 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.11 |  1.832 |  1.841 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.35 |  0.720 |  0.722 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.079 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.82 |  1.601 |  1.613 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.113 |  1.116 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.79 |  1.432 |  1.445 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.15 |  0.679 |  0.682 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.26 |  1.133 |  1.137 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.60 |  1.351 |  1.360 | report  |
| nested.build.ord             | std            | G9     |  808 |    263.03 |     825.53 |  0.313 |  0.315 | pass    |
| reset                        | std            | G9     |  808 |     24.09 |     117.83 |  0.191 |  0.194 | pass    |
| nested.build.plain           | std            | report |  808 |    269.53 |     824.22 |  0.322 |  0.324 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
