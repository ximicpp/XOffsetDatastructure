files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.127 |  1.132 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.24 |  0.960 |  0.965 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.79 |  1.012 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.27 |  0.725 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     297.53 |  0.755 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.10 |      86.55 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    898.44 |     224.61 |  4.000 |  4.022 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.86 |     304.69 |  4.201 |  4.224 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.60 |      87.16 |  0.410 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.803 |  0.807 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.346 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.53 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.67 |  0.417 |  0.419 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      42.89 |  0.748 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.52 |  0.320 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.939 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.994 |  0.996 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.95 |  0.656 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.31 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.26 |  1.090 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.19 |  0.670 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.005 |  1.010 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.796 |  1.810 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.77 |  0.679 |  0.682 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.79 |      27.34 |  1.085 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.79 |      17.66 |  1.661 |  1.671 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.082 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.54 |  1.411 |  1.424 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.20 |  0.786 |  0.789 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.43 |  1.136 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.345 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    268.25 |     832.03 |  0.324 |  0.325 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.19 |  0.252 |  0.255 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     826.81 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
