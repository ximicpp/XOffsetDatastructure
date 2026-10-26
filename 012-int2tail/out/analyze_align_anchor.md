files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 1; one-sided 95% UB, Bonferroni t quantile 0.95000; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     44.15 |      46.79 |  0.954 |  0.956 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     44.15 |      53.75 |  0.822 |  0.823 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     38.82 |      34.18 |  1.135 |  1.137 | report  |
| map.insert.reused            | std::map       | report |  404 |     38.82 |      20.67 |  1.874 |  1.878 | report  |

G9: 1 of 1 comparisons have UB <= 1.05
