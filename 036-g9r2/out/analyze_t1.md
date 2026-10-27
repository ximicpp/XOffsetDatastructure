files: 2  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  202 |      0.41 |       0.31 |  1.290 |  1.321 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  202 |      0.32 |       0.41 |  0.778 |  0.804 | pass    |
| vec.iterate                  | std::vector    | G9     |  202 |      0.04 |       0.04 |  0.993 |  1.021 | pass    |
| vec.mid_insert               | std::vector    | G9     |  202 |     48.66 |      43.78 |  1.101 |  1.107 | fail    |
| vec.mid_erase                | std::vector    | G9     |  202 |     40.94 |      39.79 |  1.027 |  1.042 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  202 |    172.53 |     225.91 |  0.755 |  0.771 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  202 |    242.83 |     293.62 |  0.804 |  0.822 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  202 |     38.04 |      88.77 |  0.432 |  0.438 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  202 |   1037.11 |     227.86 |  4.549 |  4.632 | report  |
| hero.mid_insert.plain        | std::vector    | report |  202 |   1401.69 |     307.28 |  4.689 |  4.756 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  202 |     42.56 |      92.94 |  0.464 |  0.470 | report  |
| str.assign_short             | std::string    | report |  202 |      0.97 |       0.50 |  1.959 |  2.020 | report  |
| str.assign_long              | std::string    | G9     |  202 |      2.93 |       3.40 |  0.855 |  0.870 | pass    |
| str.assign_short.view        | std::string    | G9     |  202 |      0.99 |       2.71 |  0.364 |  0.366 | pass    |
| str.assign_short.copy        | std::string    | report |  202 |      0.81 |       0.50 |  1.601 |  1.609 | report  |
| str.append                   | std::string    | G9     |  202 |      1.50 |       3.46 |  0.437 |  0.442 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  202 |     35.52 |      36.83 |  0.955 |  0.975 | pass    |
| map.insert.fresh             | std::map       | report |  202 |     35.52 |      43.70 |  0.815 |  0.821 | report  |
| map.find.fresh               | std::flat_map  | G9     |  202 |      5.14 |       6.55 |  0.773 |  0.789 | pass    |
| map.find.fresh               | std::map       | report |  202 |      5.14 |      16.28 |  0.315 |  0.319 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  202 |     34.02 |      34.99 |  0.970 |  0.986 | pass    |
| map.erase.fresh              | std::map       | report |  202 |     34.02 |      46.96 |  0.725 |  0.733 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  202 |     32.23 |      28.89 |  1.119 |  1.129 | fail    |
| set.insert.fresh             | std::set       | report |  202 |     32.23 |      44.68 |  0.719 |  0.730 | report  |
| set.find.fresh               | std::flat_set  | G9     |  202 |      4.87 |       6.45 |  0.762 |  0.777 | pass    |
| set.find.fresh               | std::set       | report |  202 |      4.87 |      16.31 |  0.303 |  0.310 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  202 |     30.68 |      27.18 |  1.139 |  1.152 | fail    |
| set.erase.fresh              | std::set       | report |  202 |     30.68 |      46.88 |  0.655 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  202 |     33.28 |      29.30 |  1.123 |  1.138 | report  |
| map.insert.reused            | std::map       | report |  202 |     33.28 |      17.99 |  1.796 |  1.875 | report  |
| map.find.reused              | std::flat_map  | report |  202 |      5.15 |       6.55 |  0.783 |  0.794 | report  |
| map.find.reused              | std::map       | report |  202 |      5.15 |       7.22 |  0.715 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  202 |     29.95 |      27.26 |  1.089 |  1.099 | report  |
| map.erase.reused             | std::map       | report |  202 |     29.95 |      22.22 |  1.312 |  1.353 | report  |
| set.insert.reused            | std::flat_set  | report |  202 |     26.86 |      21.08 |  1.269 |  1.285 | report  |
| set.insert.reused            | std::set       | report |  202 |     26.86 |      17.50 |  1.492 |  1.546 | report  |
| set.find.reused              | std::flat_set  | report |  202 |      4.88 |       6.45 |  0.759 |  0.767 | report  |
| set.find.reused              | std::set       | report |  202 |      4.88 |       7.10 |  0.686 |  0.704 | report  |
| set.erase.reused             | std::flat_set  | report |  202 |     23.11 |      20.18 |  1.137 |  1.156 | report  |
| set.erase.reused             | std::set       | report |  202 |     23.11 |      19.21 |  1.197 |  1.217 | report  |
| nested.build.ord             | std            | G9     |  202 |    372.41 |     889.31 |  0.417 |  0.425 | pass    |
| reset                        | std            | G9     |  202 |     33.20 |     168.61 |  0.200 |  0.203 | pass    |
| nested.build.plain           | std            | report |  202 |    384.11 |     843.75 |  0.455 |  0.460 | report  |

G9: 15 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, set.insert.fresh, set.erase.fresh
