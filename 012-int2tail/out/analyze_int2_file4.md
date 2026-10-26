files: 1  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  101 |      2.06 |       0.31 |  6.606 |  6.657 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  101 |      1.81 |       0.39 |  4.553 |  4.590 | fail    |
| vec.iterate                  | std::vector    | G9     |  101 |      0.03 |       0.03 |  1.002 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  101 |     44.76 |      40.69 |  1.087 |  1.115 | fail    |
| vec.mid_erase                | std::vector    | G9     |  101 |     42.07 |      39.63 |  1.051 |  1.059 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  101 |   1022.80 |     228.52 |  4.505 |  4.574 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  101 |   1401.69 |     294.92 |  4.677 |  4.760 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  101 |     46.43 |      87.65 |  0.527 |  0.532 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  101 |   1050.78 |     229.16 |  4.612 |  4.686 | report  |
| hero.mid_insert.plain        | std::vector    | report |  101 |   1408.20 |     292.97 |  4.752 |  4.808 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  101 |     46.71 |      88.42 |  0.535 |  0.545 | report  |
| str.assign_short             | std::string    | G9     |  101 |      1.28 |       0.50 |  2.520 |  2.570 | fail    |
| str.assign_long              | std::string    | G9     |  101 |      3.40 |       3.40 |  0.997 |  1.009 | pass    |
| str.append                   | std::string    | G9     |  101 |      1.54 |       3.72 |  0.418 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  101 |     35.48 |      36.78 |  0.969 |  0.988 | pass    |
| map.insert.fresh             | std::map       | report |  101 |     35.48 |      43.70 |  0.815 |  0.834 | report  |
| map.find.fresh               | std::flat_map  | G9     |  101 |      5.14 |       6.54 |  0.786 |  0.796 | pass    |
| map.find.fresh               | std::map       | report |  101 |      5.14 |      16.34 |  0.315 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  101 |     33.85 |      35.07 |  0.966 |  0.974 | pass    |
| map.erase.fresh              | std::map       | report |  101 |     33.85 |      47.04 |  0.718 |  0.726 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  101 |     32.23 |      28.81 |  1.122 |  1.133 | fail    |
| set.insert.fresh             | std::set       | report |  101 |     32.23 |      44.60 |  0.724 |  0.732 | report  |
| set.find.fresh               | std::flat_set  | G9     |  101 |      4.87 |       6.45 |  0.764 |  0.781 | pass    |
| set.find.fresh               | std::set       | report |  101 |      4.87 |      16.30 |  0.304 |  0.311 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  101 |     30.68 |      27.34 |  1.123 |  1.137 | fail    |
| set.erase.fresh              | std::set       | report |  101 |     30.68 |      46.31 |  0.660 |  0.665 | report  |
| map.insert.reused            | std::flat_map  | report |  101 |     33.20 |      29.38 |  1.130 |  1.141 | report  |
| map.insert.reused            | std::map       | report |  101 |     33.20 |      17.74 |  1.864 |  1.903 | report  |
| map.find.reused              | std::flat_map  | report |  101 |      5.15 |       6.55 |  0.776 |  0.796 | report  |
| map.find.reused              | std::map       | report |  101 |      5.15 |       7.22 |  0.726 |  0.735 | report  |
| map.erase.reused             | std::flat_map  | report |  101 |     30.03 |      27.43 |  1.091 |  1.100 | report  |
| map.erase.reused             | std::map       | report |  101 |     30.03 |      23.27 |  1.283 |  1.293 | report  |
| set.insert.reused            | std::flat_set  | report |  101 |     26.69 |      21.00 |  1.281 |  1.304 | report  |
| set.insert.reused            | std::set       | report |  101 |     26.69 |      16.93 |  1.549 |  1.614 | report  |
| set.find.reused              | std::flat_set  | report |  101 |      4.87 |       6.45 |  0.758 |  0.763 | report  |
| set.find.reused              | std::set       | report |  101 |      4.87 |       7.23 |  0.674 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  101 |     23.19 |      20.26 |  1.156 |  1.182 | report  |
| set.erase.reused             | std::set       | report |  101 |     23.19 |      22.46 |  1.015 |  1.041 | report  |
| nested.build.ord             | std            | G9     |  101 |    388.03 |     841.16 |  0.457 |  0.461 | pass    |
| reset                        | std            | G9     |  101 |     30.59 |     164.72 |  0.184 |  0.187 | pass    |
| nested.build.plain           | std            | report |  101 |    382.81 |     830.72 |  0.461 |  0.464 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
