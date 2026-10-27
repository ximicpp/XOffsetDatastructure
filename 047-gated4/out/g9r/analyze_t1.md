files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.201 |  1.213 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.767 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.977 |  0.981 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      40.69 |  0.991 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.71 |  1.018 |  1.024 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.86 |  0.717 |  0.719 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    230.47 |     296.88 |  0.764 |  0.769 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.04 |      89.76 |  0.367 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.81 |     227.88 |  4.029 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1265.62 |     306.64 |  4.209 |  4.231 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.66 |      88.38 |  0.414 |  0.416 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.828 |  1.847 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.797 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.193 |  1.202 | report  |
| str.append                   | std::string    | G9     |  808 |      1.68 |       3.61 |  0.462 |  0.464 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.79 |  0.874 |  0.877 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      44.03 |  0.729 |  0.733 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.54 |  0.784 |  0.787 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.34 |  0.316 |  0.318 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.943 |  0.947 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.28 |  0.696 |  0.699 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  1.000 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.92 |  0.640 |  0.642 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.753 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.33 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.093 |  1.099 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.88 |  0.634 |  0.636 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      17.74 |  1.657 |  1.675 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.783 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.25 |  0.698 |  0.704 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.18 |  1.081 |  1.087 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      20.02 |  1.425 |  1.440 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.083 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.74 |  1.296 |  1.305 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.56 |  0.731 |  0.736 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.143 |  1.148 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      19.41 |  1.159 |  1.170 | report  |
| nested.build.ord             | std            | G9     |  808 |    291.66 |     860.02 |  0.340 |  0.342 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     163.42 |  0.184 |  0.186 | pass    |
| nested.build.plain           | std            | report |  808 |    294.28 |     858.06 |  0.344 |  0.346 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
