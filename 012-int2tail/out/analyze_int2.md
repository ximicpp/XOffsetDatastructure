files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.06 |       0.31 |  6.593 |  6.661 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.76 |       0.39 |  4.508 |  4.539 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.001 |  1.009 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     44.27 |      40.69 |  1.085 |  1.098 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.10 |      39.63 |  1.045 |  1.051 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |   1029.30 |     228.52 |  4.528 |  4.562 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |   1402.34 |     296.55 |  4.667 |  4.705 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     46.24 |      89.07 |  0.522 |  0.526 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1040.36 |     227.88 |  4.625 |  4.665 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1407.23 |     295.25 |  4.714 |  4.746 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     46.67 |      89.32 |  0.524 |  0.529 | report  |
| str.assign_short             | std::string    | G9     |  404 |      1.28 |       0.50 |  2.551 |  2.584 | fail    |
| str.assign_long              | std::string    | G9     |  404 |      3.39 |       3.40 |  0.983 |  0.989 | pass    |
| str.append                   | std::string    | G9     |  404 |      1.54 |       3.61 |  0.427 |  0.431 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.48 |      36.70 |  0.968 |  0.976 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.48 |      43.86 |  0.805 |  0.812 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.14 |       6.55 |  0.781 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.14 |      16.37 |  0.315 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.85 |      35.07 |  0.962 |  0.970 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.85 |      46.96 |  0.718 |  0.724 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.23 |      28.81 |  1.118 |  1.123 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.23 |      44.68 |  0.721 |  0.724 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.87 |       6.46 |  0.761 |  0.767 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.87 |      16.31 |  0.302 |  0.305 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.76 |      27.26 |  1.125 |  1.131 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.76 |      46.47 |  0.661 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.38 |  1.124 |  1.136 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.863 |  1.892 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.781 |  0.788 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       7.25 |  0.710 |  0.715 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     30.03 |      27.34 |  1.095 |  1.099 | report  |
| map.erase.reused             | std::map       | report |  404 |     30.03 |      21.24 |  1.358 |  1.373 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.77 |      21.08 |  1.277 |  1.284 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.77 |      16.85 |  1.554 |  1.576 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.88 |       6.45 |  0.753 |  0.760 | report  |
| set.find.reused              | std::set       | report |  404 |      4.88 |       7.03 |  0.694 |  0.700 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.26 |  1.156 |  1.169 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      22.30 |  1.056 |  1.072 | report  |
| nested.build.ord             | std            | G9     |  404 |    385.44 |     851.56 |  0.452 |  0.454 | pass    |
| reset                        | std            | G9     |  404 |     29.95 |     164.72 |  0.182 |  0.186 | pass    |
| nested.build.plain           | std            | report |  404 |    380.22 |     842.44 |  0.451 |  0.453 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
