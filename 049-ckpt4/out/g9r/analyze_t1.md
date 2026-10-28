files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.219 |  1.226 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.28 |       0.36 |  0.774 |  0.779 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.006 |  1.014 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.49 |      40.77 |  0.994 |  0.997 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.79 |  1.016 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.92 |  0.726 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     305.01 |  0.756 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.45 |      87.97 |  0.379 |  0.380 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    899.75 |     224.61 |  4.019 |  4.042 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.17 |     305.33 |  4.218 |  4.238 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.78 |      88.30 |  0.417 |  0.419 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.61 |  0.750 |  0.753 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.153 |  1.162 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.430 |  0.432 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.35 |  0.863 |  0.867 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.29 |  0.739 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.55 |  0.785 |  0.789 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.39 |  0.315 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.931 |  0.936 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.31 |  0.741 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.993 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.86 |  0.658 |  0.660 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.755 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.53 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.42 |  1.084 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.11 |  0.672 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.46 |  1.000 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.03 |  1.823 |  1.839 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.25 |  0.718 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.083 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.66 |  1.657 |  1.670 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.080 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.54 |  1.439 |  1.454 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.754 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.46 |  0.747 |  0.752 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.140 |  1.145 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.93 |  1.351 |  1.365 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     850.25 |  0.347 |  0.348 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     117.83 |  0.258 |  0.259 | pass    |
| nested.build.plain           | std            | report |  808 |    299.47 |     850.25 |  0.352 |  0.354 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
