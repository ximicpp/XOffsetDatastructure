files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 3; one-sided 95% UB, Bonferroni t quantile 0.98333; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |   1248.37 |     275.39 |  4.527 |  4.542 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |   1562.50 |     334.64 |  4.716 |  4.737 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     44.23 |      92.02 |  0.448 |  0.455 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1036.46 |     228.52 |  4.543 |  4.567 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1414.06 |     305.99 |  4.707 |  4.733 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     43.99 |      91.29 |  0.451 |  0.459 | report  |

G9: 1 of 3 comparisons have UB <= 1.05; above: hero.mid_erase.ord, hero.mid_insert.ord
