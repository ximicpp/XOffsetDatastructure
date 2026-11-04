files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.111 |  1.114 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.776 |  0.779 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.83 |  0.971 |  0.972 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.39 |  1.018 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.757 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.62 |      87.56 |  0.363 |  0.364 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    906.25 |     224.61 |  4.033 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.03 |     304.69 |  4.222 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.10 |      87.00 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.72 |  0.418 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.870 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.21 |  0.743 |  0.745 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.54 |  0.321 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.97 |  0.996 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.43 |  0.646 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.24 |  0.302 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.090 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.60 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.28 |  1.780 |  1.789 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.69 |  0.702 |  0.706 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.74 |  1.623 |  1.634 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.62 |  1.412 |  1.425 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.28 |  0.667 |  0.670 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.136 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      17.09 |  1.334 |  1.343 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     829.44 |  0.328 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     157.88 |  0.205 |  0.209 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     826.81 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
