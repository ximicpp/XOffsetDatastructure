files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      0.42 |       0.32 |  1.291 |  1.301 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      0.32 |       0.41 |  0.788 |  0.794 | pass    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.04 |       0.04 |  0.999 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     48.58 |      43.78 |  1.098 |  1.109 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     42.24 |      39.79 |  1.056 |  1.067 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    172.52 |     227.20 |  0.754 |  0.758 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    242.19 |     306.64 |  0.803 |  0.813 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     38.13 |      88.26 |  0.432 |  0.436 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1040.36 |     228.52 |  4.639 |  4.680 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1409.50 |     306.64 |  4.668 |  4.726 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.44 |      89.31 |  0.478 |  0.483 | report  |
| str.assign_short             | std::string    | report |  404 |      0.97 |       0.50 |  1.922 |  1.953 | report  |
| str.assign_long              | std::string    | G9     |  404 |      2.93 |       3.40 |  0.860 |  0.867 | pass    |
| str.assign_short.view        | std::string    | G9     |  404 |      0.99 |       2.71 |  0.364 |  0.365 | pass    |
| str.assign_short.copy        | std::string    | report |  404 |      0.80 |       0.50 |  1.601 |  1.604 | report  |
| str.append                   | std::string    | G9     |  404 |      1.61 |       3.48 |  0.460 |  0.467 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.56 |      36.70 |  0.969 |  0.979 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.56 |      43.29 |  0.823 |  0.831 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.15 |       6.55 |  0.790 |  0.796 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.15 |      16.40 |  0.315 |  0.319 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.94 |      35.07 |  0.963 |  0.972 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.94 |      47.04 |  0.719 |  0.727 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.97 |  1.112 |  1.120 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      44.92 |  0.716 |  0.719 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.88 |       6.45 |  0.754 |  0.760 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.88 |      16.26 |  0.301 |  0.304 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.76 |      27.34 |  1.122 |  1.131 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.76 |      46.96 |  0.650 |  0.657 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.30 |  1.133 |  1.137 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.882 |  1.898 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.779 |  0.785 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       7.76 |  0.682 |  0.692 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     29.95 |      27.18 |  1.088 |  1.097 | report  |
| map.erase.reused             | std::map       | report |  404 |     29.95 |      21.69 |  1.336 |  1.357 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.86 |      21.16 |  1.264 |  1.277 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.86 |      17.50 |  1.502 |  1.534 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.88 |       6.45 |  0.761 |  0.769 | report  |
| set.find.reused              | std::set       | report |  404 |      4.88 |       7.38 |  0.661 |  0.666 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.11 |      20.18 |  1.147 |  1.155 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.11 |      19.86 |  1.153 |  1.168 | report  |
| nested.build.ord             | std            | G9     |  404 |    369.80 |     856.78 |  0.435 |  0.438 | pass    |
| reset                        | std            | G9     |  404 |     34.50 |     159.52 |  0.210 |  0.212 | pass    |
| nested.build.plain           | std            | report |  404 |    386.72 |     845.06 |  0.462 |  0.467 | report  |

G9: 14 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, vec.mid_erase, set.insert.fresh, set.erase.fresh
