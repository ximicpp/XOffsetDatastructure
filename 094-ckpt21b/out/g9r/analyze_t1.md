files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.147 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.763 |  0.766 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.63 |  1.023 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     229.17 |  0.717 |  0.720 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      85.77 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    915.36 |     224.61 |  4.073 |  4.088 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1253.27 |     295.58 |  4.211 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      85.69 |  0.412 |  0.413 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.802 |  0.804 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.152 |  1.157 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.57 |  0.430 |  0.432 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.862 |  0.864 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.97 |  0.736 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.935 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.735 |  0.738 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.97 |  0.996 |  0.997 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.19 |  0.650 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.665 |  0.667 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.028 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.36 |  1.828 |  1.837 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.806 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       6.33 |  0.828 |  0.831 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.43 |  1.076 |  1.078 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.637 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.112 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.463 |  1.475 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.16 |  0.668 |  0.672 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.126 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.76 |  1.337 |  1.347 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     826.17 |  0.327 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.235 |  0.238 | pass    |
| nested.build.plain           | std            | report |  808 |    281.25 |     828.12 |  0.340 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
