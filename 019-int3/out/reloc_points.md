## new declarations

| case | anchor ns/op | P4m / anchor | FR / anchor |
| --- | ---: | ---: | ---: |
| hero.mid_erase.ord | 269.85 | 1.007 | 1.004 |
| hero.mid_erase.plain | 1031.32 | 0.998 | 0.998 |
| hero.mid_insert.ord | 367.35 | 1.000 | 0.997 |
| hero.mid_insert.plain | 1396.56 | 0.994 | 0.995 |
| hero.push_back_grow.ord | 39.96 | 1.002 | 0.980 |
| hero.push_back_grow.plain | 43.46 | 1.001 | 0.983 |
| map.erase.fresh | 33.90 | 1.001 | 0.994 |
| map.erase.reused | 30.01 | 0.999 | 0.999 |
| map.find.fresh | 5.12 | 0.999 | 1.004 |
| map.find.reused | 5.13 | 1.000 | 1.004 |
| map.insert.fresh | 35.53 | 0.999 | 0.999 |
| map.insert.reused | 33.26 | 0.998 | 0.999 |
| nested.build.ord | 387.04 | 1.014 | 0.997 |
| nested.build.plain | 393.06 | 0.996 | 0.994 |
| reset | 34.67 | 1.000 | 0.971 |
| set.erase.fresh | 30.68 | 1.001 | 1.002 |
| set.erase.reused | 23.17 | 0.998 | 1.000 |
| set.find.fresh | 4.88 | 0.999 | 1.001 |
| set.find.reused | 4.88 | 0.999 | 1.000 |
| set.insert.fresh | 32.25 | 1.001 | 1.001 |
| set.insert.reused | 26.79 | 0.999 | 1.002 |
| str.append | 1.56 | 0.967 | 0.985 |
| str.assign_long | 3.28 | 1.003 | 1.035 |
| str.assign_short | 1.28 | 1.000 | 1.000 |
| vec.iterate | 0.03 | 1.000 | 0.998 |
| vec.mid_erase | 41.41 | 0.996 | 1.004 |
| vec.mid_insert | 44.43 | 1.007 | 1.009 |
| vec.push_back | 2.01 | 1.000 | 1.003 |
| vec.push_back_grow | 1.78 | 0.992 | 0.985 |

## old declarations

| case | anchor ns/op | P4m / anchor | FR / anchor |
| --- | ---: | ---: | ---: |
| hero.mid_erase.ord | 1266.40 | 0.987 | 1.014 |
| hero.mid_erase.plain | 1044.90 | 0.991 | 0.992 |
| hero.mid_insert.ord | 1573.79 | 0.994 | 0.982 |
| hero.mid_insert.plain | 1413.90 | 0.999 | 0.974 |
| hero.push_back_grow.ord | 44.00 | 1.005 | 0.976 |
| hero.push_back_grow.plain | 56.14 | 0.783 | 0.753 |

