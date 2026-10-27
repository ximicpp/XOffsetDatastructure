files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.128 |  1.136 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.764 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.951 |  0.955 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.32 |  0.954 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.013 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     228.52 |  0.718 |  0.722 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     307.95 |  0.754 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.84 |      90.74 |  0.364 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.92 |  4.016 |  4.041 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1256.50 |     305.34 |  4.196 |  4.219 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.21 |      89.27 |  0.406 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.831 |  1.837 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.40 |  0.841 |  0.848 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.187 |  1.192 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.417 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.871 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.70 |  0.733 |  0.737 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.54 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.37 |  0.314 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.07 |  0.938 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      47.12 |  0.697 |  0.702 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.995 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      45.25 |  0.638 |  0.643 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.760 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.35 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.092 |  1.102 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      47.04 |  0.636 |  0.641 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.004 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      18.15 |  1.629 |  1.641 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.789 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.30 |  0.725 |  0.732 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.090 |  1.096 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      20.75 |  1.397 |  1.412 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.081 |  1.089 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      17.90 |  1.261 |  1.272 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.752 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.02 |  0.687 |  0.691 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.19 |      20.18 |  1.149 |  1.155 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.19 |      19.53 |  1.181 |  1.192 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.38 |     878.91 |  0.333 |  0.335 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     164.72 |  0.186 |  0.187 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     865.88 |  0.341 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
