files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.118 |  1.122 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.781 |  0.782 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.94 |  0.992 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.37 |      39.55 |  1.023 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     224.61 |  0.729 |  0.732 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     305.34 |  0.755 |  0.758 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.45 |      87.32 |  0.384 |  0.385 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    912.11 |     225.25 |  4.056 |  4.071 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1220.70 |     305.01 |  4.077 |  4.093 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.82 |      87.12 |  0.423 |  0.424 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.771 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.93 |  0.317 |  0.318 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.61 |  0.428 |  0.430 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.31 |      37.27 |  0.870 |  0.874 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.31 |      42.64 |  0.753 |  0.758 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.787 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.43 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.939 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.27 |  0.746 |  0.749 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.999 |  1.003 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.03 |  0.657 |  0.660 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.89 |       6.47 |  0.760 |  0.764 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.89 |      16.35 |  0.302 |  0.304 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.673 |  0.675 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.38 |  1.007 |  1.010 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.03 |  1.845 |  1.852 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.22 |  0.723 |  0.726 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.078 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.58 |  1.669 |  1.678 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.16 |  1.084 |  1.088 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.46 |  1.448 |  1.461 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.46 |  0.757 |  0.759 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       7.02 |  0.697 |  0.699 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.19 |      20.35 |  1.139 |  1.144 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.19 |      16.77 |  1.363 |  1.373 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.25 |     841.16 |  0.349 |  0.351 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.19 |  0.258 |  0.260 | pass    |
| nested.build.plain           | std            | report |  808 |    301.42 |     843.75 |  0.357 |  0.359 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
