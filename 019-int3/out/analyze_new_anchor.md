files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.06 |       0.31 |  6.393 |  6.495 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.79 |       0.39 |  4.491 |  4.544 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.004 |  1.010 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     45.13 |      41.02 |  1.084 |  1.090 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.02 |      39.55 |  1.049 |  1.060 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    270.17 |     228.52 |  1.190 |  1.198 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    367.19 |     298.17 |  1.223 |  1.234 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     39.96 |      90.13 |  0.429 |  0.434 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1031.25 |     228.52 |  4.558 |  4.593 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1396.48 |     306.64 |  4.592 |  4.648 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     43.50 |      90.78 |  0.473 |  0.478 | report  |
| str.assign_short             | std::string    | G9     |  404 |      1.28 |       0.50 |  2.531 |  2.546 | fail    |
| str.assign_long              | std::string    | G9     |  404 |      3.39 |       3.40 |  0.972 |  0.981 | pass    |
| str.append                   | std::string    | G9     |  404 |      1.54 |       3.61 |  0.435 |  0.439 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.56 |      36.79 |  0.959 |  0.968 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.56 |      44.52 |  0.791 |  0.797 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.13 |       6.54 |  0.787 |  0.790 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.13 |      16.56 |  0.311 |  0.313 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.24 |  0.967 |  0.975 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      46.88 |  0.722 |  0.727 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.23 |      28.89 |  1.112 |  1.124 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.23 |      45.49 |  0.707 |  0.713 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.87 |       6.46 |  0.755 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.87 |      16.40 |  0.298 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.68 |      27.18 |  1.130 |  1.135 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.68 |      46.47 |  0.659 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.28 |      29.30 |  1.131 |  1.139 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.28 |      18.31 |  1.750 |  1.789 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.13 |       6.55 |  0.785 |  0.790 | report  |
| map.find.reused              | std::map       | report |  404 |      5.13 |       8.05 |  0.646 |  0.651 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     30.03 |      27.26 |  1.089 |  1.100 | report  |
| map.erase.reused             | std::map       | report |  404 |     30.03 |      22.13 |  1.312 |  1.332 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.78 |      21.08 |  1.269 |  1.277 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.78 |      18.47 |  1.430 |  1.454 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.87 |       6.45 |  0.752 |  0.760 | report  |
| set.find.reused              | std::set       | report |  404 |      4.87 |       7.35 |  0.671 |  0.679 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.34 |  1.139 |  1.150 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      22.79 |  1.046 |  1.060 | report  |
| nested.build.ord             | std            | G9     |  404 |    388.03 |     859.38 |  0.443 |  0.448 | pass    |
| reset                        | std            | G9     |  404 |     35.16 |     159.52 |  0.216 |  0.218 | pass    |
| nested.build.plain           | std            | report |  404 |    393.22 |     852.88 |  0.453 |  0.461 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
