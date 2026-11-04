files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.112 |  1.116 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.769 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.001 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      42.32 |  0.955 |  0.957 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.20 |      39.71 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.30 |  0.728 |  0.730 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     294.92 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.86 |      86.59 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    908.86 |     224.61 |  4.051 |  4.065 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.67 |     303.38 |  4.242 |  4.261 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.16 |      86.83 |  0.406 |  0.407 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.762 |  1.769 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.864 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.65 |  0.424 |  0.426 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.03 |  0.868 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.81 |  0.749 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.54 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.27 |  0.326 |  0.327 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.35 |  0.739 |  0.741 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.19 |  0.652 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.45 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.42 |  1.087 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.03 |  0.670 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.46 |  0.999 |  1.001 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.832 |  1.843 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.807 |  0.808 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.40 |  0.721 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.085 |  1.089 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.58 |  1.648 |  1.659 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.16 |  1.080 |  1.083 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.54 |  1.454 |  1.465 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.23 |  0.677 |  0.681 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.34 |  1.138 |  1.140 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.09 |  1.309 |  1.321 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     822.94 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     120.44 |  0.235 |  0.238 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     824.22 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
