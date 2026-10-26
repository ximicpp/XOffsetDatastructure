files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 3; one-sided 95% UB, Bonferroni t quantile 0.98333; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |   1279.30 |     283.20 |  4.538 |  4.565 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |   1544.27 |     335.94 |  4.585 |  4.622 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     43.01 |      90.31 |  0.476 |  0.479 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1033.86 |     227.86 |  4.582 |  4.625 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1377.93 |     293.62 |  4.595 |  4.639 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     42.40 |      88.18 |  0.477 |  0.481 | report  |

G9: 1 of 3 comparisons have UB <= 1.05; above: hero.mid_erase.ord, hero.mid_insert.ord
