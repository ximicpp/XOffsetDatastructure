files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.109 |  1.112 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.767 |  0.769 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.956 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.55 |  1.017 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.42 |     224.61 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     298.17 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      87.32 |  0.366 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     224.61 |  4.062 |  4.075 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1262.38 |     304.69 |  4.224 |  4.241 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.16 |      87.32 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.769 |  1.775 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.867 |  0.869 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.344 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.67 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.869 |  0.871 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.13 |  0.741 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.32 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.24 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.746 |  0.748 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.19 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.37 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.95 |  0.677 |  0.678 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.000 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.821 |  1.829 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.29 |  0.718 |  0.721 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.70 |      27.34 |  1.083 |  1.087 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.70 |      17.42 |  1.683 |  1.693 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.16 |  1.079 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.62 |  1.432 |  1.445 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.23 |  0.680 |  0.682 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.142 |  1.145 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.85 |  1.344 |  1.356 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     828.12 |  0.327 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     119.14 |  0.254 |  0.256 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     825.53 |  0.337 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
