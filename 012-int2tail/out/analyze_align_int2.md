files: 4  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 1; one-sided 95% UB, Bonferroni t quantile 0.95000; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| map.insert.fresh             | std::flat_map  | G9     |  404 |     43.86 |      46.88 |  0.935 |  0.937 | pass    |
| map.insert.fresh             | std::map       | report |  404 |     43.86 |      53.79 |  0.818 |  0.819 | report  |
| map.insert.reused            | std::flat_map  | report |  404 |     38.17 |      34.10 |  1.129 |  1.131 | report  |
| map.insert.reused            | std::map       | report |  404 |     38.17 |      20.59 |  1.856 |  1.864 | report  |

G9: 1 of 1 comparisons have UB <= 1.05
