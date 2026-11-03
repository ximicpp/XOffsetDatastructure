# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=85. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.653 / 0.688 | 0.237 / 0.255 | 0.291 / 0.310 | 0.188 / 0.199 | 0.067 / 0.072 |
| 1 场景↔场景 | 0.482 / 0.552 | 0.242 / 0.281 | 0.277 / 0.318 | 0.151 / 0.175 | 0.064 / 0.074 |
| 1 场景↔存储（整份） | 0.424 / 0.459 | 0.190 / 0.205 | 0.340 / 0.367 | 0.126 / 0.137 | 0.064 / 0.070 |
| 1 场景↔存储（只存脏模块） | 0.427 / 0.435 | 0.249 / 0.254 | 0.454 / 0.460 | 0.182 / 0.184 | 0.099 / 0.101 |
| 2 登录全量 | 0.607 / 0.638 | 0.225 / 0.251 | 0.234 / 0.257 | 0.172 / 0.190 | 0.065 / 0.073 |
| 2 浏览地图·视口全量 | 0.470 / 0.477 | 0.193 / 0.196 | 0.204 / 0.208 | 0.125 / 0.128 | 0.146 / 0.148 |
| 2 浏览地图·视口全量（单设备热视图：EB.send + EBW.recv） | 0.447 / 0.455 | 0.182 / 0.185 | 0.201 / 0.204 | 0.117 / 0.118 | 0.142 / 0.144 |
| 3 自身增量 | 0.853 / 0.928 | 0.593 / 0.650 | 0.688 / 0.748 | 0.435 / 0.473 | 0.171 / 0.186 |
| 3 静止视口增量·和平 | 1.146 / 1.231 | 0.822 / 0.902 | 0.861 / 0.946 | 0.702 / 0.784 | 0.839 / 0.890 |
| 3 静止视口增量·交战 | 0.559 / 0.580 | 0.366 / 0.384 | 0.363 / 0.382 | 0.233 / 0.246 | 0.295 / 0.308 |
| 3 静止视口增量·压力（只报） | 0.616 / 0.628 | 0.320 / 0.328 | 0.332 / 0.340 | 0.222 / 0.227 | 0.249 / 0.255 |
| 3 静止视口增量·和平（XOffset 原生） | 1.880 / 2.070 | 1.348 / 1.409 | 1.412 / 1.488 | 1.151 / 1.199 | 1.377 / 1.484 |
| 3 静止视口增量·交战（XOffset 原生） | 0.880 / 0.921 | 0.575 / 0.591 | 0.571 / 0.586 | 0.366 / 0.376 | 0.465 / 0.478 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 0.742 / 0.760 | 0.385 / 0.393 | 0.399 / 0.407 | 0.267 / 0.273 | 0.300 / 0.306 |
| 2 登录全量（XOffset 原生 key） | 1.646 / 1.954 | 0.609 / 0.640 | 0.634 / 0.677 | 0.466 / 0.493 | 0.176 / 0.183 |
| 3 自身增量（XOffset 原生） | 0.466 / 0.478 | 0.324 / 0.332 | 0.375 / 0.384 | 0.237 / 0.243 | 0.093 / 0.096 |
| 1 存储（XOffset 原生增量） | 1.878 / 1.941 | 1.093 / 1.122 | 1.994 / 2.052 | 0.798 / 0.819 | 0.435 / 0.443 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 0.92 | 1.46 | 3.96 | 3.29 | 4.99 | 14.24 |
| B per player µs | 2.38 | 5.28 | 10.49 | 9.18 | 16.75 | 40.48 |
| B 5,000 players, s | 0.012 | 0.026 | 0.052 | 0.046 | 0.084 | 0.202 |
| S per player µs (save + store + load) | 3.10 | 7.47 | 16.70 | 9.30 | 25.04 | 49.80 |
| SD per player µs | 1.45 | 3.39 | 5.79 | 3.17 | 7.92 | 14.67 |
| SN per player µs (native) | 6.41 |  |  |  |  |  |
| EB per pull µs (send + recv) | 14.7 | 31.7 | 76.6 | 72.9 | 118.0 | 101.7 |
| EBW per pull µs (send + warm single-view recv) | 13.7 | 31.0 | 75.3 | 68.8 | 118.0 | 96.7 |
| EB.recv / EBW.recv per pull µs (1,500 cold views / one warm view) | 5.04 / 4.03 | 10.28 / 9.62 | 41.96 / 40.61 | 27.11 / 23.01 | 48.47 / 48.43 | 49.06 / 44.14 |
| EB bytes per pull (raw / zstd) | 53840 / 19977 | 46167 / 18039 | 55668 / 19891 | 55167 / 20547 | 48999 / 18530 | 30512 / 19735 |
| UP per delivery µs (send + recv + apply) | 0.72 | 0.64 | 0.89 | 0.86 | 1.05 | 0.88 |
| NP per delivery µs (native send + diff + recv) | 1.23 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 316 / 186 | 215 / 140 | 335 / 164 | 341 / 199 | 241 / 144 | 123 / 131 |
| NP bytes per delivery (native) | 85 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1456 | 1456 | 1456 | 1456 | 1456 | 1456 |
| UC per delivery µs (send + recv + apply) | 1.91 | 3.52 | 5.31 | 5.37 | 8.38 | 6.59 |
| NC per delivery µs (native send + diff + recv) | 3.06 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 3093 / 1257 | 2558 / 1105 | 3220 / 1235 | 3311 / 1322 | 2746 / 1137 | 1773 / 1201 |
| NC bytes per delivery (native) | 280 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 9.14 | 14.99 | 28.72 | 27.69 | 41.30 | 36.91 |
| NS per delivery µs (native send + diff + recv) | 11.03 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 16507 / 6508 | 13814 / 5912 | 17154 / 6436 | 17540 / 6821 | 14801 / 6027 | 9579 / 6357 |
| NS bytes per delivery (native) | 790 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 2.57 | 4.33 | 12.33 | 11.84 | 16.16 | 43.66 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 0.85 | 1.05 | 1.50 | 1.30 | 2.07 | 5.33 |
| LN per delta µs (native) | 0.47 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 14.7 | 32.4 | 52.6 | 69.1 | 105.1 | 81.0 |  |
| 浏览（全员拖动）: egress KB/s (map) | 78867 raw / 29263 zstd | 67627 raw / 26424 zstd | 81546 raw / 29137 zstd | 80811 raw / 30099 zstd | 71776 raw / 27143 zstd | 44695 raw / 28909 zstd |  |
| 静止·和平: server CPU ms/s | 0.4 | 0.5 | 0.8 | 0.9 | 1.3 | 2.4 | 1.5 |
| 静止·和平: egress KB/s (map) | 450 raw / 265 zstd | 306 raw / 199 zstd | 476 raw / 234 zstd | 485 raw / 283 zstd | 342 raw / 205 zstd | 175 raw / 186 zstd | 121 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 1.4 | 2.6 | 4.3 | 5.3 | 7.7 | 9.0 | 3.8 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 4523 raw / 1839 zstd | 3741 raw / 1616 zstd | 4709 raw / 1806 zstd | 4842 raw / 1933 zstd | 4016 raw / 1662 zstd | 2592 raw / 1756 zstd | 410 raw |
| 静止·压力上限: server CPU ms/s | 5.6 | 11.7 | 18.4 | 22.6 | 33.9 | 27.4 | 13.0 |
| 静止·压力上限: egress KB/s (map) | 24179 raw / 9533 zstd | 20235 raw / 8661 zstd | 25128 raw / 9427 zstd | 25694 raw / 9992 zstd | 21681 raw / 8829 zstd | 14032 raw / 9312 zstd | 1157 raw |
