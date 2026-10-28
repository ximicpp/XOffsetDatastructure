files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.145 |  1.151 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.779 |  0.785 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.950 |  0.958 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      42.24 |  0.962 |  0.967 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.79 |  1.012 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    162.77 |     224.61 |  0.725 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.20 |      86.85 |  0.383 |  0.384 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.25 |  4.053 |  4.073 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.69 |  4.229 |  4.249 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.70 |      87.04 |  0.422 |  0.424 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.780 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.869 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.423 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.11 |  0.864 |  0.869 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.56 |  0.756 |  0.760 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.38 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.937 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.27 |  0.743 |  0.747 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.995 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.78 |  0.659 |  0.661 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.752 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.34 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.091 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.675 |  0.678 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.002 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.808 |  1.825 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.29 |  0.708 |  0.713 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.43 |  1.074 |  1.079 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.50 |  1.674 |  1.690 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.79 |  1.410 |  1.427 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       6.75 |  0.719 |  0.724 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.140 |  1.147 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.60 |  1.383 |  1.394 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     847.66 |  0.349 |  0.352 | pass    |
| reset                        | std            | G9     |  808 |     31.25 |     116.55 |  0.265 |  0.267 | pass    |
| nested.build.plain           | std            | report |  808 |    303.38 |     837.22 |  0.363 |  0.365 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
