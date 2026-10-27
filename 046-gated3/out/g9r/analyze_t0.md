files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.099 |  1.108 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.020 |  1.025 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.69 |  0.997 |  0.999 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.36 |      39.63 |  1.015 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.72 |     227.86 |  0.719 |  0.721 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     307.94 |  0.754 |  0.757 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.00 |      89.60 |  0.370 |  0.371 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    909.50 |     227.86 |  4.014 |  4.028 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1266.27 |     295.24 |  4.227 |  4.240 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.46 |      89.36 |  0.410 |  0.412 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.834 |  1.837 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.40 |  0.798 |  0.800 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.341 |  0.341 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.188 |  1.190 | report  |
| str.append                   | std::string    | G9     |  808 |      1.69 |       3.61 |  0.472 |  0.474 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.70 |  0.877 |  0.879 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.95 |  0.729 |  0.731 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.13 |       6.54 |  0.786 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.13 |      16.42 |  0.313 |  0.315 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.936 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      47.20 |  0.697 |  0.699 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.73 |  1.001 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      45.08 |  0.638 |  0.640 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.30 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.093 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      46.79 |  0.634 |  0.637 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.30 |  1.007 |  1.009 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      17.33 |  1.703 |  1.710 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.17 |       6.55 |  0.786 |  0.789 | report  |
| map.find.reused              | std::map       | report |  808 |      5.17 |       7.42 |  0.693 |  0.696 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.086 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      21.65 |  1.353 |  1.361 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.085 |  1.087 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      17.46 |  1.294 |  1.305 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.37 |  0.762 |  0.764 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.26 |  1.146 |  1.149 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      19.37 |  1.168 |  1.176 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.34 |     864.59 |  0.337 |  0.338 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     164.06 |  0.182 |  0.183 | pass    |
| nested.build.plain           | std            | report |  808 |    294.28 |     854.16 |  0.346 |  0.348 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
