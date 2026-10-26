files: 1  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  101 |      1.84 |       0.28 |  6.681 |  6.726 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  101 |      1.73 |       0.38 |  4.475 |  4.555 | fail    |
| vec.iterate                  | std::vector    | G9     |  101 |      0.03 |       0.03 |  1.001 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  101 |     44.19 |      40.61 |  1.097 |  1.113 | fail    |
| vec.mid_erase                | std::vector    | G9     |  101 |     41.02 |      39.71 |  1.043 |  1.062 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  101 |   1028.00 |     232.42 |  4.454 |  4.507 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  101 |   1412.77 |     308.59 |  4.667 |  4.724 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  101 |     46.83 |      88.91 |  0.528 |  0.534 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  101 |   1027.34 |     229.17 |  4.519 |  4.585 | report  |
| hero.mid_insert.plain        | std::vector    | report |  101 |   1407.55 |     304.69 |  4.711 |  4.783 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  101 |     46.63 |      89.72 |  0.519 |  0.528 | report  |
| str.assign_short             | std::string    | G9     |  101 |      1.29 |       0.50 |  2.554 |  2.602 | fail    |
| str.assign_long              | std::string    | G9     |  101 |      3.40 |       3.40 |  1.002 |  1.016 | pass    |
| str.append                   | std::string    | G9     |  101 |      1.61 |       3.72 |  0.431 |  0.440 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  101 |     35.48 |      36.54 |  0.973 |  0.988 | pass    |
| map.insert.fresh             | std::map       | report |  101 |     35.48 |      43.86 |  0.817 |  0.828 | report  |
| map.find.fresh               | std::flat_map  | G9     |  101 |      5.14 |       6.55 |  0.782 |  0.799 | pass    |
| map.find.fresh               | std::map       | report |  101 |      5.14 |      16.37 |  0.315 |  0.320 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  101 |     33.77 |      35.16 |  0.960 |  0.982 | pass    |
| map.erase.fresh              | std::map       | report |  101 |     33.77 |      47.12 |  0.715 |  0.732 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  101 |     32.23 |      28.81 |  1.117 |  1.128 | fail    |
| set.insert.fresh             | std::set       | report |  101 |     32.23 |      45.09 |  0.712 |  0.717 | report  |
| set.find.fresh               | std::flat_set  | G9     |  101 |      4.88 |       6.46 |  0.760 |  0.772 | pass    |
| set.find.fresh               | std::set       | report |  101 |      4.88 |      16.28 |  0.303 |  0.311 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  101 |     30.68 |      27.34 |  1.128 |  1.139 | fail    |
| set.erase.fresh              | std::set       | report |  101 |     30.68 |      46.55 |  0.660 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  101 |     33.20 |      29.38 |  1.118 |  1.150 | report  |
| map.insert.reused            | std::map       | report |  101 |     33.20 |      16.93 |  1.903 |  1.978 | report  |
| map.find.reused              | std::flat_map  | report |  101 |      5.15 |       6.56 |  0.779 |  0.793 | report  |
| map.find.reused              | std::map       | report |  101 |      5.15 |       7.30 |  0.694 |  0.707 | report  |
| map.erase.reused             | std::flat_map  | report |  101 |     30.03 |      27.18 |  1.100 |  1.103 | report  |
| map.erase.reused             | std::map       | report |  101 |     30.03 |      20.91 |  1.390 |  1.419 | report  |
| set.insert.reused            | std::flat_set  | report |  101 |     26.86 |      21.00 |  1.280 |  1.292 | report  |
| set.insert.reused            | std::set       | report |  101 |     26.86 |      17.74 |  1.505 |  1.538 | report  |
| set.find.reused              | std::flat_set  | report |  101 |      4.88 |       6.45 |  0.755 |  0.772 | report  |
| set.find.reused              | std::set       | report |  101 |      4.88 |       6.90 |  0.689 |  0.710 | report  |
| set.erase.reused             | std::flat_set  | report |  101 |     23.11 |      20.18 |  1.153 |  1.178 | report  |
| set.erase.reused             | std::set       | report |  101 |     23.11 |      20.59 |  1.098 |  1.135 | report  |
| nested.build.ord             | std            | G9     |  101 |    389.31 |     846.38 |  0.459 |  0.463 | pass    |
| reset                        | std            | G9     |  101 |     29.94 |     178.39 |  0.177 |  0.193 | pass    |
| nested.build.plain           | std            | report |  101 |    378.91 |     856.75 |  0.443 |  0.447 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
