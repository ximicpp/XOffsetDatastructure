files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.120 |  1.124 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.766 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.61 |  0.995 |  0.997 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.014 |  1.015 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     228.52 |  0.713 |  0.715 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     307.30 |  0.751 |  0.753 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.96 |      91.19 |  0.364 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.20 |     228.52 |  3.981 |  3.998 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1267.58 |     306.64 |  4.201 |  4.216 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.38 |      89.97 |  0.406 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.835 |  1.838 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.796 |  0.798 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.190 |  1.192 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.62 |  0.739 |  0.741 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.13 |       6.55 |  0.783 |  0.785 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.13 |      16.41 |  0.313 |  0.314 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.20 |  0.696 |  0.698 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.73 |  1.003 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.41 |  0.634 |  0.636 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.27 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.70 |      27.34 |  1.090 |  1.093 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.70 |      46.96 |  0.630 |  0.633 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.21 |  1.011 |  1.012 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      17.50 |  1.690 |  1.695 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.14 |       6.55 |  0.786 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.14 |       7.31 |  0.700 |  0.702 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.43 |  1.080 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      22.05 |  1.326 |  1.335 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.00 |  1.084 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.17 |  1.308 |  1.318 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.58 |  0.741 |  0.743 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.18 |  1.150 |  1.153 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      20.02 |  1.118 |  1.128 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.34 |     856.78 |  0.335 |  0.337 | pass    |
| reset                        | std            | G9     |  808 |     30.61 |     163.41 |  0.182 |  0.184 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     841.16 |  0.348 |  0.349 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
