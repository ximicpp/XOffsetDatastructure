files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.108 |  1.111 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.56 |  0.482 |  0.483 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      41.75 |  0.977 |  0.984 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.79 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.72 |     223.95 |  0.732 |  0.735 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     295.90 |  0.759 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.02 |      87.10 |  0.364 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     224.61 |  4.050 |  4.064 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1268.22 |     304.03 |  4.256 |  4.272 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.20 |      86.83 |  0.402 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.761 |  1.767 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.49 |       4.35 |  0.349 |  0.351 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.872 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.752 |  0.754 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.325 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.19 |  0.743 |  0.745 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      43.78 |  0.656 |  0.658 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.29 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.35 |  0.667 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.38 |      29.38 |  1.001 |  1.003 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.38 |      16.03 |  1.835 |  1.840 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.10 |  0.742 |  0.744 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.079 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.66 |  1.636 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.078 |  1.081 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.46 |  1.454 |  1.466 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.18 |  0.672 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.137 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.93 |  1.338 |  1.349 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     825.53 |  0.330 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     29.94 |     121.09 |  0.230 |  0.233 | pass    |
| nested.build.plain           | std            | report |  808 |    282.55 |     828.12 |  0.339 |  0.341 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
