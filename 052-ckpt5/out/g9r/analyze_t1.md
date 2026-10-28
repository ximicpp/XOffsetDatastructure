files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.122 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.28 |       0.35 |  0.787 |  0.790 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.69 |      41.02 |  0.991 |  0.994 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.36 |      39.47 |  1.023 |  1.025 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.26 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.28 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.41 |      87.08 |  0.383 |  0.384 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     224.61 |  4.066 |  4.081 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1216.80 |     305.34 |  4.076 |  4.093 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.70 |      87.04 |  0.420 |  0.421 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.775 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.93 |  0.317 |  0.318 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.60 |  0.428 |  0.430 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.19 |  0.867 |  0.868 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.89 |  0.743 |  0.746 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.786 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.36 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.84 |  0.732 |  0.734 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.998 |  1.000 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.43 |  0.649 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.89 |       6.47 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.89 |      16.32 |  0.302 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.34 |  1.085 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.68 |  0.661 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.001 |  1.005 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.846 |  1.859 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.787 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.31 |  0.714 |  0.718 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.078 |  1.082 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.82 |  1.617 |  1.628 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.084 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.46 |  1.459 |  1.470 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.756 |  0.759 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       6.81 |  0.723 |  0.725 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.35 |  1.140 |  1.143 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      17.25 |  1.321 |  1.330 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     843.75 |  0.349 |  0.351 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     121.75 |  0.223 |  0.227 | pass    |
| nested.build.plain           | std            | report |  808 |    300.78 |     839.84 |  0.358 |  0.360 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
