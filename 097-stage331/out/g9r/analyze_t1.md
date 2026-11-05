files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.115 |  1.119 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.757 |  0.760 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      42.24 |  0.960 |  0.962 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.47 |  1.019 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    162.77 |     224.61 |  0.721 |  0.723 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     298.19 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.79 |  0.356 |  0.359 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     225.25 |  4.057 |  4.071 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.20 |     304.69 |  4.208 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.57 |  0.402 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.58 |  0.436 |  0.438 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.98 |      37.11 |  0.862 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.98 |      42.48 |  0.750 |  0.752 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.39 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.83 |  0.944 |  0.946 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.41 |  0.730 |  0.733 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.655 |  0.657 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.088 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      45.33 |  0.661 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.035 |  1.037 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      15.95 |  1.900 |  1.907 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.34 |  0.722 |  0.725 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.079 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      18.55 |  1.611 |  1.622 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.113 |  1.117 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.71 |  1.478 |  1.491 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.47 |  0.654 |  0.657 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.133 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.82 |  1.307 |  1.317 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     833.31 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     119.80 |  0.204 |  0.208 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     834.62 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
