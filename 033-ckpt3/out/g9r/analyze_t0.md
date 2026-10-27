files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.05 |       0.31 |  6.512 |  6.569 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.76 |       0.39 |  4.476 |  4.502 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.001 |  1.008 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     45.08 |      40.69 |  1.102 |  1.112 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.83 |      39.39 |  1.057 |  1.065 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    270.19 |     227.87 |  1.184 |  1.196 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    366.53 |     306.00 |  1.220 |  1.233 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     39.31 |      90.17 |  0.437 |  0.439 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1032.55 |     224.61 |  4.592 |  4.632 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1392.58 |     294.60 |  4.624 |  4.687 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.68 |      88.46 |  0.484 |  0.489 | report  |
| str.assign_short             | std::string    | report |  404 |      1.28 |       0.50 |  2.522 |  2.550 | report  |
| str.assign_long              | std::string    | G9     |  404 |      3.39 |       3.40 |  0.970 |  0.978 | pass    |
| str.assign_short.view        | std::string    | G9     |  404 |      1.28 |       2.71 |  0.472 |  0.473 | pass    |
| str.assign_short.copy        | std::string    | report |  404 |      1.59 |       0.51 |  3.148 |  3.178 | report  |
| str.append                   | std::string    | G9     |  404 |      1.51 |       4.35 |  0.350 |  0.353 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.48 |      36.87 |  0.961 |  0.968 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.48 |      43.70 |  0.809 |  0.815 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.15 |       6.55 |  0.785 |  0.792 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.15 |      16.78 |  0.307 |  0.310 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.24 |  0.961 |  0.965 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      47.53 |  0.712 |  0.717 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.89 |  1.121 |  1.130 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      45.25 |  0.716 |  0.723 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.88 |       6.45 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.88 |      16.34 |  0.299 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.68 |      27.34 |  1.124 |  1.135 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.68 |      46.71 |  0.656 |  0.662 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.38 |  1.130 |  1.135 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.872 |  1.905 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.783 |  0.789 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       6.68 |  0.780 |  0.786 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     30.11 |      27.34 |  1.099 |  1.110 | report  |
| map.erase.reused             | std::map       | report |  404 |     30.11 |      20.67 |  1.426 |  1.448 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     27.02 |      21.16 |  1.274 |  1.285 | report  |
| set.insert.reused            | std::set       | report |  404 |     27.02 |      17.50 |  1.519 |  1.542 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.87 |       6.45 |  0.760 |  0.769 | report  |
| set.find.reused              | std::set       | report |  404 |      4.87 |       6.79 |  0.715 |  0.723 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.26 |  1.145 |  1.153 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      21.81 |  1.054 |  1.068 | report  |
| nested.build.ord             | std            | G9     |  404 |    388.00 |     853.52 |  0.455 |  0.460 | pass    |
| reset                        | std            | G9     |  404 |     32.56 |     159.52 |  0.202 |  0.204 | pass    |
| nested.build.plain           | std            | report |  404 |    393.22 |     839.84 |  0.465 |  0.471 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, set.insert.fresh, set.erase.fresh
