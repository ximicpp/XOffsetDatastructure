files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.119 |  1.122 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.45 |      40.69 |  0.992 |  0.995 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.47 |  1.016 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    167.31 |     225.91 |  0.741 |  0.743 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.760 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     30.56 |      86.38 |  0.352 |  0.353 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    909.50 |     224.61 |  4.058 |  4.070 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1256.50 |     292.31 |  4.230 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     33.61 |      86.26 |  0.387 |  0.388 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.150 |  1.151 | report  |
| str.append                   | std::string    | G9     |  808 |      1.52 |       3.61 |  0.419 |  0.421 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.70 |      37.44 |  0.847 |  0.850 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.70 |      42.89 |  0.735 |  0.740 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.810 |  0.812 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.63 |  0.320 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.07 |  0.940 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.737 |  0.739 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.56 |      28.89 |  0.987 |  0.989 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.56 |      44.19 |  0.646 |  0.647 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.47 |  0.753 |  0.755 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.33 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.34 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.19 |  0.669 |  0.672 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.03 |      29.46 |  1.018 |  1.021 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.03 |      15.87 |  1.892 |  1.898 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.56 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.40 |  0.714 |  0.717 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.078 |  1.081 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.70 |  1.622 |  1.633 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.107 |  1.110 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.30 |  1.513 |  1.525 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.15 |  0.785 |  0.789 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.132 |  1.135 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      16.77 |  1.331 |  1.341 | report  |
| nested.build.ord             | std            | G9     |  808 |    260.44 |     822.91 |  0.316 |  0.317 | pass    |
| reset                        | std            | G9     |  808 |     25.39 |     119.14 |  0.188 |  0.191 | pass    |
| nested.build.plain           | std            | report |  808 |    266.91 |     822.91 |  0.323 |  0.324 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
