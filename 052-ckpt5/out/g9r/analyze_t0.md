files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.143 |  1.148 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.777 |  0.779 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.952 |  0.955 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      42.24 |  0.959 |  0.961 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.55 |  1.013 |  1.015 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     225.25 |  0.723 |  0.725 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     295.58 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.12 |      87.14 |  0.381 |  0.381 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.85 |     224.61 |  4.043 |  4.057 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1258.47 |     304.69 |  4.231 |  4.246 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.62 |      87.04 |  0.421 |  0.422 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.766 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.149 |  1.157 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.63 |  0.419 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.870 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.72 |  0.750 |  0.752 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.786 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.35 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.27 |  0.741 |  0.743 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.995 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.656 |  0.657 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.29 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.27 |  0.668 |  0.670 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.005 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.11 |  1.818 |  1.829 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.787 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.17 |  0.718 |  0.722 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.080 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.58 |  1.651 |  1.662 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.71 |      21.00 |  1.080 |  1.082 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.71 |      15.79 |  1.419 |  1.432 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.755 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.00 |  0.704 |  0.706 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.137 |  1.142 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.68 |  1.356 |  1.365 | report  |
| nested.build.ord             | std            | G9     |  808 |    292.97 |     847.66 |  0.347 |  0.349 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     117.19 |  0.224 |  0.229 | pass    |
| nested.build.plain           | std            | report |  808 |    300.78 |     843.11 |  0.358 |  0.359 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
