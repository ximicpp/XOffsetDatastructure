files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.106 |  1.109 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.71 |  1.011 |  1.012 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     226.56 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     296.88 |  0.757 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      86.32 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    907.56 |     224.61 |  4.049 |  4.062 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1250.00 |     295.24 |  4.203 |  4.220 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.03 |      85.90 |  0.408 |  0.409 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.769 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.147 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.433 |  0.435 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      36.87 |  0.859 |  0.861 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.64 |  0.739 |  0.741 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.29 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.734 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.988 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      43.78 |  0.649 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.57 |  0.294 |  0.295 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.51 |  0.664 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.87 |      29.38 |  1.016 |  1.018 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.87 |      16.03 |  1.834 |  1.845 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.28 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.28 |       7.25 |  0.730 |  0.733 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.081 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      18.47 |  1.607 |  1.618 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.112 |  1.115 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.54 |  1.467 |  1.481 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.10 |  0.691 |  0.693 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.43 |  1.128 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.58 |  1.318 |  1.329 | report  |
| nested.build.ord             | std            | G9     |  808 |    265.62 |     820.95 |  0.322 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     26.03 |     119.46 |  0.189 |  0.192 | pass    |
| nested.build.plain           | std            | report |  808 |    273.44 |     816.41 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
