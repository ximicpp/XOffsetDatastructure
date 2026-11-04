files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.101 |  1.105 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.759 |  0.761 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.957 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.63 |  1.019 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.61 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     304.69 |  0.752 |  0.755 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      87.85 |  0.358 |  0.360 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.95 |     224.61 |  4.039 |  4.049 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1249.34 |     304.03 |  4.167 |  4.181 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      87.46 |  0.398 |  0.400 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.872 |  0.891 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.152 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.61 |  0.422 |  0.424 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.89 |  0.746 |  0.749 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.939 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.743 |  0.745 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  1.002 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.95 |  0.652 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.51 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.081 |  1.085 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.27 |  0.668 |  0.670 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.46 |  1.000 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.36 |  1.788 |  1.798 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.66 |  0.692 |  0.695 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.080 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.58 |  1.650 |  1.659 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.16 |  1.081 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      16.03 |  1.400 |  1.412 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.36 |  0.762 |  0.765 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.136 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.85 |  1.351 |  1.361 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     829.41 |  0.322 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.84 |  0.234 |  0.237 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     832.03 |  0.329 |  0.331 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
