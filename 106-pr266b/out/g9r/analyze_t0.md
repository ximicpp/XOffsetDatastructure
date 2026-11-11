files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.147 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.982 |  0.986 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.77 |  0.996 |  0.998 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.55 |  1.019 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.67 |     226.56 |  0.738 |  0.740 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     294.92 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.72 |      86.06 |  0.357 |  0.358 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     223.95 |  4.075 |  4.090 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1262.38 |     291.02 |  4.250 |  4.263 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.94 |      86.63 |  0.394 |  0.395 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.780 |  1.784 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.799 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.420 |  0.422 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.52 |  0.846 |  0.848 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.72 |  0.741 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.26 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.08 |  0.732 |  0.734 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.988 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      44.03 |  0.646 |  0.647 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.45 |  0.757 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.26 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.668 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.11 |      29.62 |  1.015 |  1.020 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.11 |      16.28 |  1.817 |  1.827 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.70 |  0.693 |  0.696 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.079 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.58 |  1.653 |  1.665 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.107 |  1.109 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.79 |  1.437 |  1.450 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.42 |  0.760 |  0.762 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.26 |  1.135 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.60 |  1.356 |  1.366 | report  |
| nested.build.ord             | std            | G9     |  808 |    260.44 |     824.22 |  0.309 |  0.312 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     117.19 |  0.196 |  0.199 | pass    |
| nested.build.plain           | std            | report |  808 |    265.62 |     822.91 |  0.316 |  0.318 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
