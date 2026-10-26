files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.05 |       0.30 |  6.602 |  6.656 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.76 |       0.39 |  4.531 |  4.570 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     44.84 |      41.10 |  1.086 |  1.090 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.02 |      39.71 |  1.032 |  1.041 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    271.48 |     225.92 |  1.193 |  1.200 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    367.19 |     306.64 |  1.230 |  1.243 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     40.04 |      87.28 |  0.457 |  0.461 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1028.66 |     227.54 |  4.574 |  4.613 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1388.67 |     305.98 |  4.644 |  4.680 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     43.50 |      88.66 |  0.496 |  0.500 | report  |
| str.assign_short             | std::string    | G9     |  404 |      1.28 |       0.50 |  2.537 |  2.541 | fail    |
| str.assign_long              | std::string    | G9     |  404 |      3.39 |       3.40 |  0.971 |  0.979 | pass    |
| str.append                   | std::string    | G9     |  404 |      1.50 |       3.61 |  0.422 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.48 |      36.79 |  0.964 |  0.969 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.48 |      44.07 |  0.803 |  0.808 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.12 |       6.54 |  0.785 |  0.791 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.12 |      16.56 |  0.310 |  0.313 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.07 |  0.967 |  0.976 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      46.79 |  0.726 |  0.731 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.97 |  1.117 |  1.122 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      45.25 |  0.714 |  0.717 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.87 |       6.46 |  0.755 |  0.761 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.87 |      16.42 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.68 |      27.18 |  1.131 |  1.137 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.68 |      46.47 |  0.656 |  0.660 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.21 |  1.130 |  1.142 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.25 |  1.900 |  1.930 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.13 |       6.56 |  0.784 |  0.788 | report  |
| map.find.reused              | std::map       | report |  404 |      5.13 |       7.83 |  0.674 |  0.681 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     29.95 |      27.10 |  1.096 |  1.102 | report  |
| map.erase.reused             | std::map       | report |  404 |     29.95 |      22.95 |  1.296 |  1.307 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.77 |      21.00 |  1.267 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.77 |      17.01 |  1.536 |  1.558 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.87 |       6.45 |  0.752 |  0.759 | report  |
| set.find.reused              | std::set       | report |  404 |      4.87 |       7.17 |  0.696 |  0.703 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.11 |      20.26 |  1.145 |  1.155 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.11 |      22.38 |  1.030 |  1.043 | report  |
| nested.build.ord             | std            | G9     |  404 |    394.53 |     837.25 |  0.467 |  0.472 | pass    |
| reset                        | std            | G9     |  404 |     34.52 |     160.16 |  0.214 |  0.216 | pass    |
| nested.build.plain           | std            | report |  404 |    391.27 |     835.30 |  0.470 |  0.475 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
