files: 1  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  101 |      2.06 |       0.31 |  6.602 |  6.712 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  101 |      1.81 |       0.40 |  4.484 |  4.525 | fail    |
| vec.iterate                  | std::vector    | G9     |  101 |      0.03 |       0.03 |  1.009 |  1.031 | pass    |
| vec.mid_insert               | std::vector    | G9     |  101 |     44.19 |      40.69 |  1.090 |  1.105 | fail    |
| vec.mid_erase                | std::vector    | G9     |  101 |     40.77 |      39.71 |  1.035 |  1.045 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  101 |   1031.91 |     225.25 |  4.592 |  4.654 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  101 |   1388.67 |     306.64 |  4.590 |  4.648 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  101 |     45.78 |      91.72 |  0.507 |  0.512 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  101 |   1031.91 |     225.27 |  4.587 |  4.646 | report  |
| hero.mid_insert.plain        | std::vector    | report |  101 |   1408.20 |     305.33 |  4.723 |  4.777 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  101 |     47.00 |      89.64 |  0.520 |  0.530 | report  |
| str.assign_short             | std::string    | G9     |  101 |      1.28 |       0.50 |  2.539 |  2.544 | fail    |
| str.assign_long              | std::string    | G9     |  101 |      3.17 |       3.40 |  0.932 |  0.936 | pass    |
| str.append                   | std::string    | G9     |  101 |      1.54 |       3.61 |  0.422 |  0.427 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  101 |     35.56 |      36.62 |  0.969 |  0.983 | pass    |
| map.insert.fresh             | std::map       | report |  101 |     35.56 |      43.38 |  0.816 |  0.830 | report  |
| map.find.fresh               | std::flat_map  | G9     |  101 |      5.14 |       6.56 |  0.779 |  0.792 | pass    |
| map.find.fresh               | std::map       | report |  101 |      5.14 |      16.27 |  0.316 |  0.321 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  101 |     33.86 |      35.16 |  0.965 |  0.973 | pass    |
| map.erase.fresh              | std::map       | report |  101 |     33.86 |      46.96 |  0.721 |  0.727 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  101 |     32.23 |      28.81 |  1.118 |  1.121 | fail    |
| set.insert.fresh             | std::set       | report |  101 |     32.23 |      44.35 |  0.723 |  0.731 | report  |
| set.find.fresh               | std::flat_set  | G9     |  101 |      4.88 |       6.46 |  0.758 |  0.768 | pass    |
| set.find.fresh               | std::set       | report |  101 |      4.88 |      16.34 |  0.301 |  0.306 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  101 |     30.76 |      27.26 |  1.123 |  1.137 | fail    |
| set.erase.fresh              | std::set       | report |  101 |     30.76 |      46.39 |  0.662 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  101 |     33.20 |      29.38 |  1.130 |  1.132 | report  |
| map.insert.reused            | std::map       | report |  101 |     33.20 |      18.07 |  1.850 |  1.868 | report  |
| map.find.reused              | std::flat_map  | report |  101 |      5.15 |       6.56 |  0.782 |  0.790 | report  |
| map.find.reused              | std::map       | report |  101 |      5.15 |       7.25 |  0.710 |  0.720 | report  |
| map.erase.reused             | std::flat_map  | report |  101 |     30.03 |      27.51 |  1.086 |  1.098 | report  |
| map.erase.reused             | std::map       | report |  101 |     30.03 |      21.08 |  1.393 |  1.426 | report  |
| set.insert.reused            | std::flat_set  | report |  101 |     26.77 |      21.16 |  1.271 |  1.283 | report  |
| set.insert.reused            | std::set       | report |  101 |     26.77 |      16.68 |  1.582 |  1.624 | report  |
| set.find.reused              | std::flat_set  | report |  101 |      4.88 |       6.45 |  0.754 |  0.759 | report  |
| set.find.reused              | std::set       | report |  101 |      4.88 |       6.69 |  0.722 |  0.734 | report  |
| set.erase.reused             | std::flat_set  | report |  101 |     23.19 |      20.26 |  1.157 |  1.190 | report  |
| set.erase.reused             | std::set       | report |  101 |     23.19 |      23.11 |  1.011 |  1.047 | report  |
| nested.build.ord             | std            | G9     |  101 |    382.81 |     869.81 |  0.441 |  0.443 | pass    |
| reset                        | std            | G9     |  101 |     29.94 |     167.97 |  0.179 |  0.182 | pass    |
| nested.build.plain           | std            | report |  101 |    377.59 |     845.03 |  0.445 |  0.448 | report  |

G9: 11 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.push_back_grow, vec.mid_insert, hero.mid_erase.ord, hero.mid_insert.ord, str.assign_short, set.insert.fresh, set.erase.fresh
