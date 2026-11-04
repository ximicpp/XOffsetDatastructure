files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.117 |  1.121 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.780 |  0.782 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.32 |  0.961 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.63 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    162.75 |     224.61 |  0.722 |  0.724 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     300.13 |  0.760 |  0.767 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.66 |      86.55 |  0.367 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    907.55 |     224.61 |  4.050 |  4.062 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1244.46 |     304.69 |  4.170 |  4.186 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.08 |      86.38 |  0.408 |  0.409 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.762 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.153 |  1.157 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.53 |  0.435 |  0.437 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.98 |      37.27 |  0.857 |  0.859 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.98 |      42.56 |  0.749 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.34 |       6.55 |  0.814 |  0.815 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.34 |      16.47 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.11 |  0.747 |  0.750 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.97 |  0.995 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.19 |  0.652 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.45 |  0.756 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.37 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.086 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.674 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.30 |  1.032 |  1.035 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      15.87 |  1.901 |  1.911 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.35 |       6.56 |  0.814 |  0.815 | report  |
| map.find.reused              | std::map       | report |  808 |      5.35 |       6.49 |  0.822 |  0.825 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.078 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.50 |  1.680 |  1.688 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.16 |  1.108 |  1.112 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.38 |  1.508 |  1.520 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.75 |  0.712 |  0.716 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.34 |  1.134 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      16.68 |  1.362 |  1.371 | report  |
| nested.build.ord             | std            | G9     |  808 |    268.25 |     829.41 |  0.325 |  0.326 | pass    |
| reset                        | std            | G9     |  808 |     29.94 |     117.84 |  0.256 |  0.257 | pass    |
| nested.build.plain           | std            | report |  808 |    277.34 |     826.84 |  0.336 |  0.337 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
