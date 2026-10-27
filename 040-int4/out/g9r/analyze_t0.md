files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.31 |       0.24 |  1.287 |  1.294 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.28 |       0.36 |  0.782 |  0.791 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     45.17 |      40.77 |  1.092 |  1.099 | fail    |
| vec.mid_erase                | std::vector    | G9     |  808 |     42.07 |      39.96 |  1.043 |  1.048 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    172.53 |     227.88 |  0.753 |  0.757 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    244.80 |     307.94 |  0.806 |  0.813 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     38.04 |      90.13 |  0.422 |  0.425 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |   1041.02 |     227.88 |  4.638 |  4.662 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1414.71 |     306.64 |  4.717 |  4.742 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     42.28 |      90.82 |  0.469 |  0.471 | report  |
| str.assign_short             | std::string    | report |  808 |      0.97 |       0.50 |  1.931 |  1.943 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.40 |  0.862 |  0.865 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.99 |       2.71 |  0.366 |  0.369 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.80 |       0.50 |  1.596 |  1.607 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.49 |  0.442 |  0.444 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     35.56 |      36.70 |  0.966 |  0.970 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     35.56 |      43.70 |  0.811 |  0.815 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.55 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.28 |  0.316 |  0.318 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.94 |      34.91 |  0.964 |  0.970 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.94 |      47.36 |  0.713 |  0.718 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     32.23 |      28.89 |  1.111 |  1.118 | fail    |
| set.insert.fresh             | std::set       | report |  808 |     32.23 |      44.68 |  0.722 |  0.725 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.757 |  0.761 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.30 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     30.76 |      27.26 |  1.130 |  1.136 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     30.76 |      46.88 |  0.654 |  0.659 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     33.20 |      29.30 |  1.132 |  1.136 | report  |
| map.insert.reused            | std::map       | report |  808 |     33.20 |      17.74 |  1.855 |  1.868 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.783 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.26 |  0.707 |  0.712 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     30.03 |      27.18 |  1.100 |  1.106 | report  |
| map.erase.reused             | std::map       | report |  808 |     30.03 |      22.13 |  1.303 |  1.321 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     26.94 |      21.16 |  1.274 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  808 |     26.94 |      17.66 |  1.499 |  1.512 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.757 |  0.760 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       7.21 |  0.681 |  0.686 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.18 |  1.144 |  1.149 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      19.29 |  1.175 |  1.186 | report  |
| nested.build.ord             | std            | G9     |  808 |    372.38 |     869.78 |  0.430 |  0.434 | pass    |
| reset                        | std            | G9     |  808 |     33.86 |     162.77 |  0.208 |  0.210 | pass    |
| nested.build.plain           | std            | report |  808 |    388.03 |     850.28 |  0.456 |  0.460 | report  |

G9: 15 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, set.insert.fresh, set.erase.fresh
