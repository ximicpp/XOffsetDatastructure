files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.06 |       0.31 |  6.652 |  6.711 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.79 |       0.39 |  4.512 |  4.542 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  0.999 |  1.006 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     44.92 |      40.69 |  1.088 |  1.095 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.83 |      39.55 |  1.055 |  1.070 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |   1032.55 |     228.52 |  4.556 |  4.584 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |   1419.27 |     294.92 |  4.734 |  4.796 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     43.78 |      89.19 |  0.494 |  0.498 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1041.34 |     225.27 |  4.635 |  4.672 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1399.73 |     306.64 |  4.643 |  4.684 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     43.78 |      89.15 |  0.493 |  0.496 | report  |
| str.assign_short             | std::string    | G9     |  404 |      1.28 |       0.50 |  2.551 |  2.573 | fail    |
| str.assign_long              | std::string    | G9     |  404 |      3.40 |       3.39 |  1.000 |  1.012 | pass    |
| str.append                   | std::string    | G9     |  404 |      1.54 |       3.61 |  0.422 |  0.426 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.64 |      37.11 |  0.970 |  0.986 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.64 |      43.46 |  0.822 |  0.836 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.15 |       6.55 |  0.782 |  0.791 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.15 |      16.46 |  0.315 |  0.319 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.24 |  0.965 |  0.971 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      47.04 |  0.719 |  0.728 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.23 |      28.89 |  1.111 |  1.119 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.23 |      45.17 |  0.713 |  0.718 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.87 |       6.45 |  0.754 |  0.763 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.87 |      16.33 |  0.300 |  0.304 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.68 |      27.43 |  1.112 |  1.126 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.68 |      46.96 |  0.652 |  0.657 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.45 |      29.30 |  1.147 |  1.168 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.45 |      17.74 |  1.867 |  1.909 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.783 |  0.789 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       7.27 |  0.702 |  0.710 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     30.03 |      27.18 |  1.086 |  1.106 | report  |
| map.erase.reused             | std::map       | report |  404 |     30.03 |      20.67 |  1.389 |  1.422 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.86 |      21.16 |  1.272 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.86 |      17.09 |  1.570 |  1.589 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.88 |       6.45 |  0.755 |  0.764 | report  |
| set.find.reused              | std::set       | report |  404 |      4.88 |       6.88 |  0.718 |  0.727 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.18 |  1.139 |  1.151 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      19.12 |  1.174 |  1.193 | report  |
| nested.build.ord             | std            | G9     |  404 |    399.75 |     846.34 |  0.475 |  0.480 | pass    |
| reset                        | std            | G9     |  404 |     35.16 |     160.16 |  0.217 |  0.221 | pass    |
| nested.build.plain           | std            | report |  404 |    393.22 |     843.75 |  0.466 |  0.471 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
