files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.116 |  1.119 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.774 |  0.777 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.93 |  0.992 |  0.993 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.22 |  0.720 |  0.723 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     305.01 |  0.753 |  0.756 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.28 |      88.66 |  0.377 |  0.378 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     225.25 |  4.046 |  4.060 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1220.70 |     305.33 |  4.072 |  4.086 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.74 |      87.91 |  0.418 |  0.419 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.93 |  0.317 |  0.318 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.62 |  0.427 |  0.429 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.11 |  0.867 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.739 |  0.742 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.785 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.33 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.41 |  0.729 |  0.732 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.51 |  0.649 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.89 |       6.47 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.89 |      16.38 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.70 |      27.43 |  1.084 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.70 |      44.92 |  0.659 |  0.662 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.004 |  1.006 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.823 |  1.835 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.20 |  0.709 |  0.711 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.077 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.628 |  1.639 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.084 |  1.087 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.46 |  1.447 |  1.460 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.758 |  0.759 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       7.00 |  0.694 |  0.697 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.26 |  1.142 |  1.146 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      17.25 |  1.315 |  1.326 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     852.84 |  0.347 |  0.349 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.84 |  0.240 |  0.244 | pass    |
| nested.build.plain           | std            | report |  808 |    300.78 |     848.94 |  0.355 |  0.357 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
