files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.118 |  1.120 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.003 |  1.006 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.963 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.63 |  1.017 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.91 |  0.723 |  0.724 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.27 |  0.761 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.94 |      86.06 |  0.372 |  0.372 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.45 |     225.25 |  4.065 |  4.079 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1266.92 |     304.69 |  4.251 |  4.267 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.40 |      85.82 |  0.414 |  0.415 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.767 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.153 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.54 |  0.428 |  0.430 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.98 |      37.11 |  0.861 |  0.863 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.98 |      42.48 |  0.751 |  0.753 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.34 |  0.324 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.996 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.99 |  0.652 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.28 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.669 |  0.671 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.35 |      29.38 |  1.032 |  1.034 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.35 |      16.03 |  1.868 |  1.876 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.08 |  0.746 |  0.749 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.078 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.74 |  1.634 |  1.643 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.111 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.54 |  1.462 |  1.475 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.55 |  0.653 |  0.655 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.134 |  1.137 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.85 |  1.340 |  1.348 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     826.81 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     118.48 |  0.214 |  0.218 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     822.91 |  0.340 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
