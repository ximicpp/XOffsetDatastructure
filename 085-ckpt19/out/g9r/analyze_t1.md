files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.111 |  1.115 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.956 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.55 |  1.016 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     298.17 |  0.759 |  0.768 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.02 |      86.51 |  0.381 |  0.387 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    903.66 |     225.27 |  4.044 |  4.091 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1264.98 |     304.03 |  4.226 |  4.248 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      86.18 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.776 |  1.785 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.64 |       0.55 |  1.142 |  1.148 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.71 |  0.417 |  0.419 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.31 |      36.87 |  0.871 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.31 |      42.56 |  0.755 |  0.758 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.29 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.35 |  0.740 |  0.743 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.997 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.27 |  0.650 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.30 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.088 |  1.092 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.670 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.38 |  1.003 |  1.007 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.36 |  1.773 |  1.788 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.806 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.08 |  0.746 |  0.749 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.077 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.626 |  1.637 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.086 |  1.089 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.87 |  1.404 |  1.417 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.84 |  0.711 |  0.715 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.35 |  1.138 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.76 |  1.343 |  1.355 | report  |
| nested.build.ord             | std            | G9     |  808 |    274.72 |     830.72 |  0.329 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.237 |  0.240 | pass    |
| nested.build.plain           | std            | report |  808 |    279.97 |     830.75 |  0.337 |  0.338 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
