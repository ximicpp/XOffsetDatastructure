files: 1  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  101 |      2.05 |       0.31 |  6.483 |  6.726 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  101 |      1.78 |       0.39 |  4.521 |  4.601 | fail    |
| vec.iterate                  | std::vector    | G9     |  101 |      0.03 |       0.03 |  0.993 |  1.014 | pass    |
| vec.mid_insert               | std::vector    | G9     |  101 |     44.27 |      40.69 |  1.068 |  1.103 | fail    |
| vec.mid_erase                | std::vector    | G9     |  101 |     41.75 |      39.31 |  1.053 |  1.058 | fail    |
| hero.mid_erase.ord           | std::vector    | G9     |  101 |   1031.25 |     227.88 |  4.563 |  4.643 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  101 |   1404.30 |     294.28 |  4.735 |  4.835 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  101 |     46.14 |      89.27 |  0.527 |  0.540 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  101 |   1057.30 |     223.95 |  4.786 |  4.894 | report  |
| hero.mid_insert.plain        | std::vector    | report |  101 |   1401.69 |     293.62 |  4.670 |  4.745 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  101 |     46.26 |      88.83 |  0.523 |  0.529 | report  |
| str.assign_short             | std::string    | G9     |  101 |      1.28 |       0.50 |  2.590 |  2.710 | fail    |
| str.assign_long              | std::string    | G9     |  101 |      3.40 |       3.40 |  1.001 |  1.007 | pass    |
| str.append                   | std::string    | G9     |  101 |      1.54 |       3.46 |  0.438 |  0.444 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  101 |     35.48 |      36.87 |  0.963 |  0.971 | pass    |
| map.insert.fresh             | std::map       | report |  101 |     35.48 |      46.22 |  0.772 |  0.779 | report  |
| map.find.fresh               | std::flat_map  | G9     |  101 |      5.15 |       6.55 |  0.778 |  0.789 | pass    |
| map.find.fresh               | std::map       | report |  101 |      5.15 |      16.43 |  0.312 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  101 |     33.85 |      34.83 |  0.958 |  0.977 | pass    |
| map.erase.fresh              | std::map       | report |  101 |     33.85 |      46.71 |  0.719 |  0.728 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  101 |     32.23 |      28.81 |  1.117 |  1.120 | fail    |
| set.insert.fresh             | std::set       | report |  101 |     32.23 |      44.43 |  0.723 |  0.728 | report  |
| set.find.fresh               | std::flat_set  | G9     |  101 |      4.87 |       6.46 |  0.759 |  0.773 | pass    |
| set.find.fresh               | std::set       | report |  101 |      4.87 |      16.39 |  0.299 |  0.306 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  101 |     30.76 |      27.26 |  1.128 |  1.133 | fail    |
| set.erase.fresh              | std::set       | report |  101 |     30.76 |      46.55 |  0.660 |  0.663 | report  |
| map.insert.reused            | std::flat_map  | report |  101 |     33.45 |      29.30 |  1.119 |  1.155 | report  |
| map.insert.reused            | std::map       | report |  101 |     33.45 |      17.74 |  1.837 |  1.915 | report  |
| map.find.reused              | std::flat_map  | report |  101 |      5.15 |       6.56 |  0.788 |  0.797 | report  |
| map.find.reused              | std::map       | report |  101 |      5.15 |       7.25 |  0.710 |  0.715 | report  |
| map.erase.reused             | std::flat_map  | report |  101 |     29.95 |      27.10 |  1.102 |  1.105 | report  |
| map.erase.reused             | std::map       | report |  101 |     29.95 |      21.16 |  1.367 |  1.401 | report  |
| set.insert.reused            | std::flat_set  | report |  101 |     26.86 |      21.08 |  1.274 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  101 |     26.86 |      16.85 |  1.581 |  1.612 | report  |
| set.find.reused              | std::flat_set  | report |  101 |      4.87 |       6.45 |  0.746 |  0.768 | report  |
| set.find.reused              | std::set       | report |  101 |      4.87 |       7.08 |  0.690 |  0.694 | report  |
| set.erase.reused             | std::flat_set  | report |  101 |     23.11 |      20.10 |  1.156 |  1.181 | report  |
| set.erase.reused             | std::set       | report |  101 |     23.11 |      20.67 |  1.102 |  1.130 | report  |
| nested.build.ord             | std            | G9     |  101 |    384.09 |     848.97 |  0.453 |  0.457 | pass    |
| reset                        | std            | G9     |  101 |     30.59 |     163.42 |  0.186 |  0.194 | pass    |
| nested.build.plain           | std            | report |  101 |    381.53 |     839.84 |  0.456 |  0.459 | report  |

G9: 10 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, vec.mid_erase, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
