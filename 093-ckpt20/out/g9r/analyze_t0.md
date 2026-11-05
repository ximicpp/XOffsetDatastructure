files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.148 |  1.150 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.003 |  1.007 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.961 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.47 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.25 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     294.92 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.47 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.76 |     224.61 |  4.069 |  4.080 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.03 |  4.229 |  4.246 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.28 |      86.06 |  0.411 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.773 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.46 |  0.436 |  0.438 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.87 |  0.863 |  0.864 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.89 |  0.734 |  0.737 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.809 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.32 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.935 |  0.937 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.51 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.666 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.46 |  1.028 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.28 |  1.847 |  1.856 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.42 |  0.824 |  0.826 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.071 |  1.073 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.636 |  1.645 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.114 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.480 |  1.491 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.06 |  0.687 |  0.690 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.130 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.85 |  1.333 |  1.341 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     822.94 |  0.327 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     118.50 |  0.221 |  0.224 | pass    |
| nested.build.plain           | std            | report |  808 |    280.61 |     821.62 |  0.342 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
