files: 2  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  202 |      2.06 |       0.31 |  6.565 |  6.639 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  202 |      1.80 |       0.40 |  4.500 |  4.542 | fail    |
| vec.iterate                  | std::vector    | G9     |  202 |      0.03 |       0.03 |  1.000 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  202 |     45.00 |      40.69 |  1.096 |  1.118 | fail    |
| vec.mid_erase                | std::vector    | G9     |  202 |     42.16 |      39.63 |  1.054 |  1.060 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  202 |    270.19 |     228.52 |  1.190 |  1.221 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  202 |    366.53 |     306.64 |  1.219 |  1.226 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  202 |     39.35 |      89.40 |  0.442 |  0.450 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  202 |   1025.39 |     227.88 |  4.559 |  4.608 | report  |
| hero.mid_insert.plain        | std::vector    | report |  202 |   1388.67 |     305.34 |  4.637 |  4.735 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  202 |     42.83 |      91.31 |  0.474 |  0.480 | report  |
| str.assign_short             | std::string    | report |  202 |      1.28 |       0.50 |  2.538 |  2.541 | report  |
| str.assign_long              | std::string    | G9     |  202 |      3.18 |       3.40 |  0.940 |  0.955 | pass    |
| str.assign_short.view        | std::string    | G9     |  202 |      1.28 |       2.71 |  0.473 |  0.486 | pass    |
| str.assign_short.copy        | std::string    | report |  202 |      1.59 |       0.50 |  3.142 |  3.145 | report  |
| str.append                   | std::string    | G9     |  202 |      1.50 |       4.35 |  0.350 |  0.353 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  202 |     35.48 |      36.87 |  0.961 |  0.967 | pass    |
| map.insert.fresh             | std::map       | report |  202 |     35.48 |      43.74 |  0.812 |  0.816 | report  |
| map.find.fresh               | std::flat_map  | G9     |  202 |      5.15 |       6.55 |  0.777 |  0.793 | pass    |
| map.find.fresh               | std::map       | report |  202 |      5.15 |      16.88 |  0.305 |  0.310 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  202 |     33.86 |      35.16 |  0.964 |  0.978 | pass    |
| map.erase.fresh              | std::map       | report |  202 |     33.86 |      47.28 |  0.712 |  0.718 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  202 |     32.31 |      28.97 |  1.118 |  1.125 | fail    |
| set.insert.fresh             | std::set       | report |  202 |     32.31 |      45.45 |  0.714 |  0.718 | report  |
| set.find.fresh               | std::flat_set  | G9     |  202 |      4.88 |       6.45 |  0.761 |  0.772 | pass    |
| set.find.fresh               | std::set       | report |  202 |      4.88 |      16.41 |  0.298 |  0.305 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  202 |     30.68 |      27.26 |  1.127 |  1.139 | fail    |
| set.erase.fresh              | std::set       | report |  202 |     30.68 |      46.55 |  0.659 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  202 |     33.20 |      29.38 |  1.113 |  1.139 | report  |
| map.insert.reused            | std::map       | report |  202 |     33.20 |      18.23 |  1.799 |  1.841 | report  |
| map.find.reused              | std::flat_map  | report |  202 |      5.15 |       6.55 |  0.784 |  0.802 | report  |
| map.find.reused              | std::map       | report |  202 |      5.15 |       6.56 |  0.785 |  0.796 | report  |
| map.erase.reused             | std::flat_map  | report |  202 |     30.11 |      27.34 |  1.103 |  1.106 | report  |
| map.erase.reused             | std::map       | report |  202 |     30.11 |      20.59 |  1.459 |  1.477 | report  |
| set.insert.reused            | std::flat_set  | report |  202 |     26.94 |      21.08 |  1.276 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  202 |     26.94 |      17.74 |  1.520 |  1.541 | report  |
| set.find.reused              | std::flat_set  | report |  202 |      4.88 |       6.45 |  0.751 |  0.762 | report  |
| set.find.reused              | std::set       | report |  202 |      4.88 |       6.92 |  0.715 |  0.725 | report  |
| set.erase.reused             | std::flat_set  | report |  202 |     23.11 |      20.26 |  1.130 |  1.154 | report  |
| set.erase.reused             | std::set       | report |  202 |     23.11 |      22.38 |  1.014 |  1.046 | report  |
| nested.build.ord             | std            | G9     |  202 |    388.03 |     865.88 |  0.457 |  0.460 | pass    |
| reset                        | std            | G9     |  202 |     33.84 |     158.86 |  0.211 |  0.213 | pass    |
| nested.build.plain           | std            | report |  202 |    389.33 |     855.47 |  0.461 |  0.464 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, set.insert.fresh, set.erase.fresh
