| build | case | side | file 1 | file 2 | file 3 | file 4 | GM |
| --- | --- | --- | --- | --- | --- | --- | --- |
| plain | map.insert.fresh | r414 | 46.63 | 46.02 | 46.22 | 44.27 | 45.78 |
| plain | map.insert.fresh | f8ee | 44.19 | 45.37 | 45.69 | 40.45 | 43.87 |
| plain | map.insert.reused | r414 | 37.44 | 37.27 | 37.19 | 36.21 | 37.02 |
| plain | map.insert.reused | f8ee | 39.14 | 39.14 | 39.23 | 36.05 | 38.37 |
| align | map.insert.fresh | r414 | 45.25 | 46.43 | 45.65 | 46.14 | 45.87 |
| align | map.insert.fresh | f8ee | 45.41 | 45.33 | 46.06 | 44.23 | 45.25 |
| align | map.insert.reused | r414 | 36.70 | 36.91 | 36.70 | 36.79 | 36.77 |
| align | map.insert.reused | f8ee | 39.22 | 39.14 | 39.22 | 38.00 | 38.90 |

f8eebf4 / 414dea4 (GM of per-file medians, ns per op):
- plain: map.insert.fresh 0.958, map.insert.reused 1.036
- align: map.insert.fresh 0.987, map.insert.reused 1.058
