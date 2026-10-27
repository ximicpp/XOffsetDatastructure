files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.195 |  1.206 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.768 |  0.773 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.995 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      40.69 |  0.992 |  0.996 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.79 |  1.018 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     231.77 |  0.706 |  0.709 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    229.81 |     306.64 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.88 |      90.01 |  0.365 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    888.35 |     228.52 |  3.910 |  3.928 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.67 |     305.98 |  4.216 |  4.237 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.58 |      90.25 |  0.405 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.837 |  1.843 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.799 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.188 |  1.191 | report  |
| str.append                   | std::string    | G9     |  808 |      1.72 |       3.57 |  0.493 |  0.495 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.87 |  0.872 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      43.62 |  0.737 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.785 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.38 |  0.315 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.36 |  0.691 |  0.694 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.998 |  1.002 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.00 |  0.640 |  0.644 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.37 |  0.300 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.099 |  1.104 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.83 |  0.635 |  0.638 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.21 |  1.009 |  1.015 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      17.09 |  1.707 |  1.722 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.16 |  0.714 |  0.719 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.18 |  1.084 |  1.089 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      20.35 |  1.409 |  1.425 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.081 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      17.50 |  1.287 |  1.302 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.10 |  0.670 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.19 |      20.18 |  1.143 |  1.150 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.19 |      19.94 |  1.149 |  1.161 | report  |
| nested.build.ord             | std            | G9     |  808 |    292.97 |     875.00 |  0.337 |  0.339 | pass    |
| reset                        | std            | G9     |  808 |     31.25 |     160.16 |  0.190 |  0.191 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     852.88 |  0.346 |  0.348 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
