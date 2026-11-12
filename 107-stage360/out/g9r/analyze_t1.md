files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.128 |  1.131 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.764 |  0.766 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.69 |  0.992 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.79 |  1.017 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    167.31 |     227.22 |  0.738 |  0.740 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    228.52 |     297.53 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.60 |      87.89 |  0.346 |  0.347 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.25 |  4.050 |  4.060 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1251.95 |     298.17 |  4.192 |  4.206 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.65 |      87.77 |  0.380 |  0.382 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.771 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      37.52 |  0.842 |  0.844 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.89 |  0.736 |  0.738 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.30 |       6.56 |  0.806 |  0.811 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.30 |      16.67 |  0.319 |  0.320 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.941 |  0.946 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.52 |  0.735 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.64 |      28.97 |  0.989 |  0.994 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.64 |      44.35 |  0.644 |  0.648 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.753 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.38 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.42 |  1.082 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.27 |  0.664 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.017 |  1.023 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      16.03 |  1.818 |  1.840 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.31 |       6.56 |  0.808 |  0.812 | report  |
| map.find.reused              | std::map       | report |  808 |      5.31 |       7.41 |  0.708 |  0.712 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.075 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.90 |  1.610 |  1.624 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.16 |  1.108 |  1.112 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.54 |  1.463 |  1.478 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.49 |  0.750 |  0.754 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.28 |  0.765 |  0.771 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.135 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.58 |  1.306 |  1.318 | report  |
| nested.build.ord             | std            | G9     |  808 |    259.12 |     824.22 |  0.308 |  0.310 | pass    |
| reset                        | std            | G9     |  808 |     24.75 |     117.84 |  0.184 |  0.187 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     825.53 |  0.316 |  0.319 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
