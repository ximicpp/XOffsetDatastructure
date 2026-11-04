files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.114 |  1.117 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.768 |  0.771 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.999 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.47 |  1.020 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     225.91 |  0.724 |  0.726 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.27 |  0.762 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.22 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.81 |     224.61 |  4.061 |  4.074 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1260.42 |     303.39 |  4.242 |  4.256 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.12 |      86.14 |  0.407 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.761 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.149 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.53 |       3.69 |  0.416 |  0.418 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.868 |  0.869 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.743 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.36 |  0.324 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.938 |  0.940 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.03 |  0.747 |  0.749 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.997 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.11 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.754 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.088 |  1.091 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.86 |  0.680 |  0.682 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.005 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.836 |  1.846 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.61 |  0.707 |  0.710 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.073 |  1.090 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.42 |  1.625 |  1.649 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.16 |  1.079 |  1.088 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.46 |  1.372 |  1.397 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.761 |  0.769 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.18 |  0.685 |  0.694 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.140 |  1.147 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.01 |  1.329 |  1.342 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.81 |     824.22 |  0.339 |  0.343 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.19 |  0.256 |  0.258 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     824.22 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
