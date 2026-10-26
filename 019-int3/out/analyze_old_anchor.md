files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 3; one-sided 95% UB, Bonferroni t quantile 0.98333; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| hero.mid_erase.ord           | std::vector    | G9     |  404 |   1264.65 |     275.39 |  4.588 |  4.616 | fail    |
| hero.mid_insert.ord          | std::vector    | G9     |  404 |   1566.41 |     333.34 |  4.753 |  4.785 | fail    |
| hero.push_back_grow.ord      | std::vector    | G9     |  404 |     43.95 |      91.23 |  0.484 |  0.488 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  404 |   1044.27 |     227.88 |  4.621 |  4.653 | report  |
| hero.mid_insert.plain        | std::vector    | report |  404 |   1414.06 |     305.98 |  4.760 |  4.792 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  404 |     44.23 |      91.19 |  0.504 |  0.513 | report  |

G9: 1 of 3 comparisons have UB <= 1.05; above: hero.mid_erase.ord, hero.mid_insert.ord
