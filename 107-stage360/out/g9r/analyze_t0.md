files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.122 |  1.127 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.773 |  0.777 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.56 |  0.953 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.020 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.728 |  0.733 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     298.17 |  0.763 |  0.767 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      87.20 |  0.366 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.42 |     224.61 |  4.077 |  4.097 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.202 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      86.59 |  0.405 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.757 |  1.763 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.431 |  0.433 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      37.11 |  0.854 |  0.856 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.64 |  0.741 |  0.744 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.809 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.38 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.83 |  0.942 |  0.945 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.735 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.97 |  0.987 |  0.989 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      43.95 |  0.648 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.753 |  0.755 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.39 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.669 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.38 |  1.022 |  1.024 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.19 |  1.852 |  1.858 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.41 |  0.729 |  0.733 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.083 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.617 |  1.628 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.113 |  1.117 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.79 |  1.475 |  1.489 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.754 |  0.755 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.41 |  0.659 |  0.661 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.138 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.76 |  1.343 |  1.353 | report  |
| nested.build.ord             | std            | G9     |  808 |    263.03 |     824.22 |  0.321 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     118.48 |  0.194 |  0.197 | pass    |
| nested.build.plain           | std            | report |  808 |    269.53 |     824.22 |  0.327 |  0.328 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
