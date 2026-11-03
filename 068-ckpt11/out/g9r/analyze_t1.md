files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.25 |  1.131 |  1.134 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.956 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.71 |  1.014 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.95 |  0.728 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     297.53 |  0.756 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      87.24 |  0.366 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    897.14 |     224.61 |  4.001 |  4.012 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1248.70 |     294.92 |  4.185 |  4.203 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.36 |      87.00 |  0.406 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.771 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.341 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.749 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.27 |       6.55 |  0.805 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.27 |      16.54 |  0.320 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.08 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.19 |  0.747 |  0.749 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.99 |  0.654 |  0.656 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.32 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.090 |  1.093 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.674 |  0.675 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.006 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      15.87 |  1.840 |  1.852 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.63 |  0.694 |  0.697 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.082 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.66 |  1.662 |  1.671 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.08 |  1.085 |  1.090 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.46 |  1.449 |  1.464 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.23 |  0.781 |  0.783 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.137 |  1.142 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.60 |  1.365 |  1.377 | report  |
| nested.build.ord             | std            | G9     |  808 |    269.53 |     828.12 |  0.326 |  0.327 | pass    |
| reset                        | std            | G9     |  808 |     30.61 |     117.19 |  0.261 |  0.264 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     828.12 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
