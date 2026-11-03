# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=85. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.521 / 0.563 | 0.212 / 0.225 | 0.215 / 0.231 | 0.163 / 0.172 | 0.060 / 0.065 |
| 1 场景↔场景 | 0.378 / 0.407 | 0.464 / 0.498 | 0.267 / 0.290 | 0.198 / 0.213 | 0.100 / 0.108 |
| 1 场景↔存储（整份） | 0.260 / 0.274 | 0.284 / 0.303 | 0.187 / 0.198 | 0.133 / 0.141 | 0.068 / 0.072 |
| 1 场景↔存储（只存脏模块） | 0.213 / 0.226 | 0.282 / 0.288 | 0.169 / 0.173 | 0.131 / 0.136 | 0.067 / 0.069 |
| 2 登录全量 | 0.280 / 0.316 | 0.327 / 0.373 | 0.184 / 0.212 | 0.171 / 0.189 | 0.087 / 0.095 |
| 2 浏览地图·视口全量 | 0.478 / 0.486 | 0.199 / 0.201 | 0.200 / 0.203 | 0.130 / 0.132 | 0.152 / 0.154 |
| 2 浏览地图·视口全量（单设备热视图：EB.send + EBW.recv） | 0.445 / 0.449 | 0.182 / 0.184 | 0.191 / 0.192 | 0.117 / 0.118 | 0.144 / 0.145 |
| 3 自身增量 | 0.400 / 0.451 | 0.934 / 1.043 | 0.359 / 0.404 | 0.310 / 0.351 | 0.178 / 0.200 |
| 3 静止视口增量·和平 | 1.236 / 1.387 | 0.815 / 0.910 | 0.975 / 1.081 | 0.872 / 0.943 | 0.985 / 1.091 |
| 3 静止视口增量·交战 | 0.634 / 0.685 | 0.417 / 0.456 | 0.409 / 0.444 | 0.282 / 0.309 | 0.355 / 0.385 |
| 3 静止视口增量·压力（只报） | 0.621 / 0.643 | 0.328 / 0.339 | 0.322 / 0.334 | 0.232 / 0.241 | 0.261 / 0.271 |
| 3 静止视口增量·和平（XOffset 原生） | 4.570 / 5.084 | 3.013 / 3.354 | 3.605 / 4.018 | 3.226 / 3.498 | 3.642 / 4.038 |
| 3 静止视口增量·交战（XOffset 原生） | 1.683 / 1.869 | 1.108 / 1.249 | 1.087 / 1.214 | 0.747 / 0.842 | 0.941 / 1.049 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 1.189 / 1.268 | 0.629 / 0.668 | 0.617 / 0.659 | 0.444 / 0.474 | 0.499 / 0.533 |
| 2 登录全量（XOffset 原生 key） | 0.465 / 0.502 | 0.542 / 0.592 | 0.305 / 0.337 | 0.283 / 0.307 | 0.145 / 0.156 |
| 3 自身增量（XOffset 原生） | 0.245 / 0.287 | 0.572 / 0.659 | 0.220 / 0.256 | 0.190 / 0.224 | 0.109 / 0.127 |
| 1 存储（XOffset 原生增量） | 1.691 / 1.922 | 2.237 / 2.477 | 1.342 / 1.484 | 1.037 / 1.169 | 0.529 / 0.583 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 0.80 | 1.50 | 4.01 | 3.88 | 5.07 | 14.28 |
| B per player µs | 4.88 | 13.08 | 10.52 | 18.14 | 25.00 | 50.11 |
| B 5,000 players, s | 0.024 | 0.065 | 0.053 | 0.091 | 0.125 | 0.251 |
| S per player µs (save + store + load) | 4.68 | 18.59 | 16.71 | 25.58 | 35.79 | 70.82 |
| SD per player µs | 1.61 | 7.58 | 5.76 | 9.58 | 12.42 | 24.57 |
| SN per player µs (native) | 12.63 |  |  |  |  |  |
| EB per pull µs (send + recv) | 15.1 | 32.1 | 77.4 | 77.4 | 118.6 | 102.1 |
| EBW per pull µs (send + warm single-view recv) | 13.7 | 31.2 | 75.5 | 72.5 | 117.9 | 96.1 |
| EB.recv / EBW.recv per pull µs (1,500 cold views / one warm view) | 5.40 / 4.01 | 10.55 / 9.59 | 42.50 / 40.50 | 27.60 / 22.70 | 48.83 / 48.20 | 49.82 / 43.86 |
| EB bytes per pull (raw / zstd) | 53840 / 19977 | 46167 / 18039 | 55668 / 19891 | 55167 / 20547 | 48999 / 18530 | 30512 / 19735 |
| UP per delivery µs (send + recv + apply) | 1.08 | 0.89 | 1.29 | 1.09 | 1.25 | 1.11 |
| NP per delivery µs (native send + diff + recv) | 3.95 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 316 / 186 | 215 / 140 | 335 / 164 | 341 / 199 | 241 / 144 | 123 / 131 |
| NP bytes per delivery (native) | 85 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1456 | 1456 | 1456 | 1456 | 1456 | 1456 |
| UC per delivery µs (send + recv + apply) | 2.25 | 3.73 | 5.63 | 5.88 | 8.64 | 6.82 |
| NC per delivery µs (native send + diff + recv) | 6.13 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 3093 / 1257 | 2558 / 1105 | 3220 / 1235 | 3311 / 1322 | 2746 / 1137 | 1773 / 1201 |
| NC bytes per delivery (native) | 280 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 9.25 | 15.42 | 29.05 | 30.02 | 41.53 | 36.93 |
| NS per delivery µs (native send + diff + recv) | 17.33 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 16507 / 6508 | 13814 / 5912 | 17154 / 6436 | 17540 / 6821 | 14801 / 6027 | 9579 / 6357 |
| NS bytes per delivery (native) | 790 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 4.66 | 18.29 | 15.20 | 27.31 | 30.39 | 57.89 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 1.27 | 3.75 | 1.56 | 4.16 | 4.81 | 8.48 |
| LN per delta µs (native) | 0.71 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 14.8 | 33.4 | 53.0 | 75.9 | 106.1 | 81.3 |  |
| 浏览（全员拖动）: egress KB/s (map) | 78867 raw / 29263 zstd | 67627 raw / 26424 zstd | 81546 raw / 29137 zstd | 80811 raw / 30099 zstd | 71776 raw / 27143 zstd | 44695 raw / 28909 zstd |  |
| 静止·和平: server CPU ms/s | 0.4 | 1.1 | 0.8 | 1.7 | 2.0 | 3.3 | 4.5 |
| 静止·和平: egress KB/s (map) | 450 raw / 265 zstd | 306 raw / 199 zstd | 476 raw / 234 zstd | 485 raw / 283 zstd | 342 raw / 205 zstd | 175 raw / 186 zstd | 121 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 1.4 | 3.2 | 4.3 | 6.6 | 8.4 | 9.8 | 6.1 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 4523 raw / 1839 zstd | 3741 raw / 1616 zstd | 4709 raw / 1806 zstd | 4842 raw / 1933 zstd | 4016 raw / 1662 zstd | 2592 raw / 1756 zstd | 410 raw |
| 静止·压力上限: server CPU ms/s | 4.8 | 12.3 | 18.3 | 25.9 | 34.4 | 28.0 | 17.6 |
| 静止·压力上限: egress KB/s (map) | 24179 raw / 9533 zstd | 20235 raw / 8661 zstd | 25128 raw / 9427 zstd | 25694 raw / 9992 zstd | 21681 raw / 8829 zstd | 14032 raw / 9312 zstd | 1157 raw |
