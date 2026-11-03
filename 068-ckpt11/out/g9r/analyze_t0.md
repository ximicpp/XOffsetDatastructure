files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.25 |  1.115 |  1.118 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.775 |  0.778 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.26 |  0.982 |  0.983 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.63 |  1.016 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.726 |  0.728 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     298.17 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.78 |      86.87 |  0.365 |  0.366 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    911.45 |     224.61 |  4.066 |  4.076 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.86 |     304.69 |  4.185 |  4.202 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      87.18 |  0.403 |  0.404 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.867 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.150 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       4.35 |  0.350 |  0.351 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.19 |  0.866 |  0.868 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.38 |  0.736 |  0.739 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.34 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.936 |  0.939 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.741 |  0.743 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     29.22 |      28.89 |  1.011 |  1.013 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     29.22 |      44.11 |  0.659 |  0.661 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.754 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.085 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.671 |  0.673 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.54 |  1.001 |  1.002 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.815 |  1.821 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.807 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.19 |  0.734 |  0.737 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.26 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.66 |  1.641 |  1.650 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.95 |      21.16 |  1.085 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.95 |      15.71 |  1.446 |  1.457 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.755 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.49 |  0.663 |  0.667 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.26 |  1.141 |  1.144 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      17.01 |  1.322 |  1.335 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     833.31 |  0.326 |  0.328 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     118.50 |  0.241 |  0.245 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     828.12 |  0.337 |  0.338 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
