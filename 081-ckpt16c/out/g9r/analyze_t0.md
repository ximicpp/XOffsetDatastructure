files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.116 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.771 |  0.774 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.999 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.957 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.55 |  1.021 |  1.024 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     298.83 |  0.758 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      86.75 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     224.61 |  4.058 |  4.072 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.02 |     304.69 |  4.226 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      86.71 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.763 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.66 |       0.57 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.56 |       3.72 |  0.416 |  0.418 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.871 |  0.874 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.750 |  0.753 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.07 |  0.942 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  1.002 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.657 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.03 |  0.674 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.38 |  1.007 |  1.011 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.11 |  1.820 |  1.829 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.44 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.086 |  1.090 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.50 |  1.662 |  1.672 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.084 |  1.089 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.62 |  1.437 |  1.449 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.753 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.47 |  0.662 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.138 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.09 |  1.300 |  1.315 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     832.03 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.83 |  0.250 |  0.252 | pass    |
| nested.build.plain           | std            | report |  808 |    278.62 |     826.17 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
