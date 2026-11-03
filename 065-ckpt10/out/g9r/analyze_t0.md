files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.123 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.778 |  0.782 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.30 |  0.984 |  0.986 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.55 |  1.012 |  1.014 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.727 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     297.53 |  0.763 |  0.765 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.70 |      87.04 |  0.364 |  0.365 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.47 |     224.61 |  4.072 |  4.085 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.11 |     304.03 |  4.207 |  4.226 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.99 |      86.79 |  0.403 |  0.403 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       4.40 |  0.344 |  0.345 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.19 |  0.869 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.13 |  0.744 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.937 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.742 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     29.30 |      28.97 |  1.014 |  1.016 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     29.30 |      43.95 |  0.666 |  0.668 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.32 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.088 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.671 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.46 |  1.006 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.36 |  1.805 |  1.816 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.21 |  0.743 |  0.747 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.70 |      27.34 |  1.084 |  1.086 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.70 |      17.58 |  1.655 |  1.667 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.084 |  1.088 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.62 |  1.455 |  1.466 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.28 |  0.662 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.139 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.76 |  1.342 |  1.353 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.16 |     830.08 |  0.329 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.51 |  0.247 |  0.250 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     833.34 |  0.335 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
