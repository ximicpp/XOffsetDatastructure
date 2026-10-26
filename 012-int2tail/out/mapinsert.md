| build | case | side | file 1 | file 2 | file 3 | file 4 | GM |
| --- | --- | --- | --- | --- | --- | --- | --- |
| plain | map.insert.fresh | anchor | 35.56 | 35.97 | 35.64 | 35.48 | 35.66 |
| plain | map.insert.fresh | int2 | 35.48 | 35.56 | 35.48 | 35.52 | 35.51 |
| plain | map.insert.reused | anchor | 33.45 | 33.61 | 33.29 | 33.37 | 33.43 |
| plain | map.insert.reused | int2 | 33.20 | 33.20 | 33.45 | 33.20 | 33.26 |
| align64 | map.insert.fresh | anchor | 43.91 | 45.65 | 43.70 | 45.25 | 44.62 |
| align64 | map.insert.fresh | int2 | 43.86 | 43.86 | 45.25 | 43.25 | 44.05 |
| align64 | map.insert.reused | anchor | 37.96 | 39.31 | 37.84 | 39.31 | 38.60 |
| align64 | map.insert.reused | int2 | 38.05 | 39.14 | 39.06 | 37.52 | 38.44 |

int2 / anchor (GM of per-file medians, ns per op):
- plain: map.insert.fresh 0.996, map.insert.reused 0.995
- align64: map.insert.fresh 0.987, map.insert.reused 0.996
