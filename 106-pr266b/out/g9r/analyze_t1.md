files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.761 |  0.763 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      40.85 |  0.990 |  0.993 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.53 |      39.47 |  1.029 |  1.031 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    166.02 |     225.25 |  0.735 |  0.737 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.27 |      87.24 |  0.347 |  0.348 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     225.25 |  4.054 |  4.067 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1253.27 |     304.69 |  4.205 |  4.218 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     34.02 |      86.91 |  0.391 |  0.392 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.57 |  0.428 |  0.429 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      37.35 |  0.846 |  0.848 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.89 |  0.734 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.63 |  0.319 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.735 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.64 |      28.89 |  0.989 |  0.992 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.64 |      44.19 |  0.647 |  0.649 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.29 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.34 |  1.088 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.27 |  0.668 |  0.670 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.022 |  1.026 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      15.87 |  1.891 |  1.904 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.70 |  0.697 |  0.701 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.077 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.646 |  1.656 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.16 |  1.108 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.22 |  1.524 |  1.536 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.18 |  0.784 |  0.787 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.34 |  1.137 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.76 |  1.346 |  1.356 | report  |
| nested.build.ord             | std            | G9     |  808 |    259.12 |     822.94 |  0.316 |  0.318 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     118.48 |  0.207 |  0.209 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     824.22 |  0.324 |  0.325 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
