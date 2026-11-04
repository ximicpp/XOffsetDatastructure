files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.111 |  1.115 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.26 |  0.976 |  0.979 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.63 |  1.021 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.70 |     224.61 |  0.730 |  0.732 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.47 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.06 |     223.95 |  4.080 |  4.093 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1264.65 |     303.71 |  4.240 |  4.256 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.12 |      86.67 |  0.405 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.767 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.66 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.70 |  0.875 |  0.877 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      43.13 |  0.742 |  0.744 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.806 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.28 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.935 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.51 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.997 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.27 |  0.645 |  0.648 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.41 |  0.298 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.086 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.672 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.005 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.11 |  1.789 |  1.803 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.807 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.99 |  0.753 |  0.756 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.081 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.42 |  1.680 |  1.691 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.71 |  1.410 |  1.424 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.83 |  0.620 |  0.622 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.139 |  1.142 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.345 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     834.66 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     29.94 |     117.19 |  0.248 |  0.250 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     828.12 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
