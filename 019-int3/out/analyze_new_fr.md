files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  404 |      2.05 |       0.31 |  6.368 |  6.453 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  404 |      1.76 |       0.39 |  4.494 |  4.533 | fail    |
| vec.iterate                  | std::vector    | G9     |  404 |      0.03 |       0.03 |  1.002 |  1.007 | pass    |
| vec.mid_insert               | std::vector    | G9     |  404 |     45.08 |      40.69 |  1.098 |  1.108 | fail    |
| vec.mid_erase                | std::vector    | G9     |  404 |     41.91 |      39.71 |  1.051 |  1.059 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |    271.48 |     229.16 |  1.184 |  1.195 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |    366.53 |     306.64 |  1.216 |  1.227 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     39.06 |      87.85 |  0.443 |  0.447 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1029.30 |     227.86 |  4.551 |  4.578 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1389.97 |     294.27 |  4.649 |  4.695 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.68 |      87.83 |  0.488 |  0.493 | report  |
| str.assign_short             | std::string    | G9     |  404 |      1.28 |       0.50 |  2.538 |  2.566 | fail    |
| str.assign_long              | std::string    | G9     |  404 |      3.40 |       3.40 |  1.000 |  1.008 | pass    |
| str.append                   | std::string    | G9     |  404 |      1.54 |       3.57 |  0.434 |  0.438 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     35.48 |      36.87 |  0.963 |  0.971 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     35.48 |      43.46 |  0.818 |  0.826 | report  |
| map.find.fresh               | std::flat_map  | G9     |  404 |      5.14 |       6.55 |  0.784 |  0.793 | pass    |
| map.find.fresh               | std::map       | report |  404 |      5.14 |      16.36 |  0.316 |  0.320 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  404 |     33.69 |      35.24 |  0.953 |  0.960 | pass    |
| map.erase.fresh              | std::map       | report |  404 |     33.69 |      46.79 |  0.720 |  0.725 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  404 |     32.31 |      28.89 |  1.120 |  1.124 | fail    |
| set.insert.fresh             | std::set       | report |  404 |     32.31 |      45.08 |  0.715 |  0.719 | report  |
| set.find.fresh               | std::flat_set  | G9     |  404 |      4.87 |       6.45 |  0.758 |  0.763 | pass    |
| set.find.fresh               | std::set       | report |  404 |      4.87 |      16.33 |  0.302 |  0.305 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  404 |     30.76 |      27.43 |  1.133 |  1.147 | fail    |
| set.erase.fresh              | std::set       | report |  404 |     30.76 |      46.88 |  0.656 |  0.665 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     33.20 |      29.30 |  1.134 |  1.147 | report  |
| map.insert.reused            | std::map       | report |  404 |     33.20 |      17.74 |  1.873 |  1.898 | report  |
| map.find.reused              | std::flat_map  | report |  404 |      5.15 |       6.56 |  0.789 |  0.800 | report  |
| map.find.reused              | std::map       | report |  404 |      5.15 |       7.61 |  0.708 |  0.720 | report  |
| map.erase.reused             | std::flat_map  | report |  404 |     29.95 |      27.18 |  1.105 |  1.119 | report  |
| map.erase.reused             | std::map       | report |  404 |     29.95 |      22.54 |  1.331 |  1.353 | report  |
| set.insert.reused            | std::flat_set  | report |  404 |     26.78 |      21.00 |  1.271 |  1.283 | report  |
| set.insert.reused            | std::set       | report |  404 |     26.78 |      16.93 |  1.550 |  1.586 | report  |
| set.find.reused              | std::flat_set  | report |  404 |      4.87 |       6.45 |  0.757 |  0.761 | report  |
| set.find.reused              | std::set       | report |  404 |      4.87 |       7.18 |  0.679 |  0.685 | report  |
| set.erase.reused             | std::flat_set  | report |  404 |     23.19 |      20.18 |  1.145 |  1.158 | report  |
| set.erase.reused             | std::set       | report |  404 |     23.19 |      20.10 |  1.137 |  1.149 | report  |
| nested.build.ord             | std            | G9     |  404 |    385.44 |     834.64 |  0.463 |  0.466 | pass    |
| reset                        | std            | G9     |  404 |     33.20 |     159.52 |  0.208 |  0.210 | pass    |
| nested.build.plain           | std            | report |  404 |    390.62 |     834.62 |  0.469 |  0.474 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
