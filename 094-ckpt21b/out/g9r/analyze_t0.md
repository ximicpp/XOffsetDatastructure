files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.145 |  1.154 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.764 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.960 |  0.963 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.55 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.20 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      86.51 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     224.61 |  4.079 |  4.091 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.03 |  4.215 |  4.232 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      86.22 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.772 |  1.777 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.797 |  0.799 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.340 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.49 |  0.434 |  0.436 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      36.95 |  0.861 |  0.865 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      43.13 |  0.733 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.808 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.33 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.743 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.27 |  0.650 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.752 |  0.754 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.39 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.083 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.19 |  0.670 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.027 |  1.031 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.11 |  1.845 |  1.857 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.806 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.39 |  0.831 |  0.835 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.38 |      27.42 |  1.071 |  1.074 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.38 |      17.50 |  1.654 |  1.664 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.109 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.54 |  1.470 |  1.483 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.07 |  0.683 |  0.687 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.127 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.93 |  1.338 |  1.347 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     822.94 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     28.66 |     117.84 |  0.232 |  0.236 | pass    |
| nested.build.plain           | std            | report |  808 |    279.97 |     817.72 |  0.343 |  0.345 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
