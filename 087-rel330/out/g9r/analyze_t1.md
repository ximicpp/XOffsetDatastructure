files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.147 |  1.151 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.770 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.962 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.39 |  1.025 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.95 |  0.729 |  0.732 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      86.34 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.72 |     224.61 |  4.075 |  4.088 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.215 |  4.232 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      86.14 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.62 |       0.53 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.57 |  0.434 |  0.436 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      37.03 |  0.862 |  0.865 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.97 |  0.732 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.29 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.19 |  0.649 |  0.651 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.68 |  0.661 |  0.664 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.19 |      29.38 |  1.029 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.19 |      16.28 |  1.811 |  1.820 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.27 |  0.839 |  0.841 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.076 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.90 |  1.620 |  1.629 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.107 |  1.109 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.449 |  1.461 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.21 |  0.673 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.130 |  1.133 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.01 |  1.322 |  1.332 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     824.22 |  0.330 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     121.09 |  0.224 |  0.228 | pass    |
| nested.build.plain           | std            | report |  808 |    279.97 |     822.94 |  0.340 |  0.342 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
