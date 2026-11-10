files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.116 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.55 |  1.012 |  1.014 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.95 |  0.729 |  0.731 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     295.58 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.02 |      86.63 |  0.365 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    902.67 |     224.61 |  4.027 |  4.040 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1246.41 |     304.05 |  4.183 |  4.201 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.26 |  0.403 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.156 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.51 |  0.429 |  0.431 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.74 |      36.87 |  0.860 |  0.862 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.74 |      42.72 |  0.739 |  0.742 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.36 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.942 |  0.945 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.986 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      43.78 |  0.649 |  0.651 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.67 |  0.292 |  0.294 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.671 |  0.674 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.87 |      29.38 |  1.017 |  1.020 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.87 |      16.03 |  1.860 |  1.868 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.35 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.080 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.633 |  1.644 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.111 |  1.115 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.46 |  1.498 |  1.509 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.99 |  0.685 |  0.688 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.133 |  1.136 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.93 |  1.343 |  1.352 | report  |
| nested.build.ord             | std            | G9     |  808 |    265.62 |     825.53 |  0.321 |  0.322 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     117.19 |  0.199 |  0.201 | pass    |
| nested.build.plain           | std            | report |  808 |    273.44 |     822.94 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
