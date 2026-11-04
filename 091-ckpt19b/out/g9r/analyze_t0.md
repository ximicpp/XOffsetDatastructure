files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.138 |  1.142 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.982 |  0.983 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.34 |  0.976 |  0.978 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.39 |  1.022 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.93 |  0.723 |  0.726 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     298.17 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      87.52 |  0.366 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    902.34 |     225.27 |  4.000 |  4.014 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.17 |     305.01 |  4.206 |  4.224 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.68 |      87.52 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.772 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.802 |  0.804 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.62 |       0.53 |  1.150 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.59 |       3.69 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.78 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      42.97 |  0.748 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.43 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.738 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.11 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.26 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.43 |  0.666 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.004 |  1.007 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.28 |  1.808 |  1.813 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.50 |  0.718 |  0.721 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.082 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.635 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.082 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.46 |  1.451 |  1.463 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.41 |  0.657 |  0.659 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.138 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.68 |  1.344 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.16 |     828.12 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     28.66 |     118.50 |  0.223 |  0.226 | pass    |
| nested.build.plain           | std            | report |  808 |    281.25 |     822.91 |  0.342 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
