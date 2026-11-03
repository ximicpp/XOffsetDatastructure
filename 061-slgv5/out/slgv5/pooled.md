# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=85. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.642 / 0.662 | 0.279 / 0.308 | 0.285 / 0.306 | 0.219 / 0.236 | 0.084 / 0.092 |
| 1 场景↔场景 | 0.433 / 0.469 | 0.459 / 0.510 | 0.324 / 0.356 | 0.246 / 0.266 | 0.133 / 0.148 |
| 1 场景↔存储（整份） | 0.246 / 0.285 | 0.271 / 0.314 | 0.177 / 0.206 | 0.126 / 0.145 | 0.065 / 0.076 |
| 1 场景↔存储（只存脏模块） | 0.209 / 0.222 | 0.282 / 0.297 | 0.169 / 0.178 | 0.129 / 0.136 | 0.067 / 0.071 |
| 2 登录全量 | 0.283 / 0.309 | 0.312 / 0.342 | 0.182 / 0.198 | 0.166 / 0.183 | 0.080 / 0.091 |
| 2 浏览地图·视口全量 | 0.481 / 0.493 | 0.209 / 0.215 | 0.209 / 0.216 | 0.138 / 0.142 | 0.159 / 0.162 |
| 2 浏览地图·视口全量（单设备热视图：EB.send + EBW.recv） | 0.441 / 0.453 | 0.189 / 0.192 | 0.196 / 0.199 | 0.122 / 0.124 | 0.149 / 0.150 |
| 3 自身增量 | 0.365 / 0.469 | 0.879 / 1.085 | 0.331 / 0.424 | 0.285 / 0.364 | 0.164 / 0.208 |
| 3 静止视口增量·和平 | 1.181 / 1.341 | 0.782 / 0.908 | 0.925 / 1.073 | 0.815 / 0.927 | 0.941 / 1.102 |
| 3 静止视口增量·交战 | 0.621 / 0.687 | 0.400 / 0.443 | 0.396 / 0.453 | 0.275 / 0.314 | 0.342 / 0.388 |
| 3 静止视口增量·压力（只报） | 0.606 / 0.644 | 0.318 / 0.338 | 0.316 / 0.340 | 0.229 / 0.247 | 0.256 / 0.275 |
| 3 静止视口增量·和平（XOffset 原生） | 4.444 / 5.133 | 2.942 / 3.412 | 3.483 / 4.313 | 3.066 / 3.695 | 3.541 / 4.203 |
| 3 静止视口增量·交战（XOffset 原生） | 1.531 / 1.819 | 0.988 / 1.175 | 0.976 / 1.206 | 0.678 / 0.838 | 0.845 / 1.033 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 1.111 / 1.243 | 0.583 / 0.654 | 0.579 / 0.659 | 0.420 / 0.478 | 0.468 / 0.532 |
| 2 登录全量（XOffset 原生 key） | 0.474 / 0.532 | 0.522 / 0.570 | 0.305 / 0.328 | 0.278 / 0.304 | 0.134 / 0.146 |
| 3 自身增量（XOffset 原生） | 0.249 / 0.325 | 0.600 / 0.753 | 0.226 / 0.295 | 0.195 / 0.253 | 0.112 / 0.145 |
| 1 存储（XOffset 原生增量） | 1.422 / 1.846 | 1.921 / 2.473 | 1.150 / 1.480 | 0.876 / 1.140 | 0.453 / 0.585 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 1.09 | 1.73 | 4.27 | 4.17 | 5.32 | 14.63 |
| B per player µs | 6.74 | 16.19 | 15.12 | 21.28 | 28.22 | 53.32 |
| B 5,000 players, s | 0.034 | 0.081 | 0.076 | 0.106 | 0.141 | 0.267 |
| S per player µs (save + store + load) | 4.37 | 18.71 | 16.65 | 25.52 | 35.94 | 71.15 |
| SD per player µs | 1.59 | 7.77 | 5.74 | 9.62 | 12.57 | 24.62 |
| SN per player µs (native) | 10.40 |  |  |  |  |  |
| EB per pull µs (send + recv) | 16.1 | 33.8 | 78.7 | 79.3 | 119.6 | 103.7 |
| EBW per pull µs (send + warm single-view recv) | 14.3 | 32.9 | 76.8 | 74.3 | 118.8 | 97.7 |
| EB.recv / EBW.recv per pull µs (1,500 cold views / one warm view) | 5.78 / 4.03 | 10.71 / 9.78 | 42.57 / 40.66 | 27.93 / 22.93 | 48.78 / 47.95 | 50.02 / 44.04 |
| EB bytes per pull (raw / zstd) | 53840 / 19977 | 46167 / 18039 | 55668 / 20217 | 55167 / 20547 | 48999 / 18530 | 30512 / 19735 |
| UP per delivery µs (send + recv + apply) | 0.97 | 0.83 | 1.20 | 1.05 | 1.26 | 1.08 |
| NP per delivery µs (native send + diff + recv) | 3.62 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 316 / 186 | 215 / 140 | 335 / 164 | 341 / 199 | 241 / 144 | 123 / 131 |
| NP bytes per delivery (native) | 85 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1456 | 1456 | 1456 | 1456 | 1456 | 1456 |
| UC per delivery µs (send + recv + apply) | 2.22 | 3.76 | 5.74 | 5.88 | 8.63 | 6.87 |
| NC per delivery µs (native send + diff + recv) | 5.15 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 3093 / 1257 | 2558 / 1105 | 3220 / 1239 | 3311 / 1322 | 2746 / 1137 | 1773 / 1201 |
| NC bytes per delivery (native) | 280 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 9.32 | 15.89 | 30.09 | 30.46 | 42.02 | 37.71 |
| NS per delivery µs (native send + diff + recv) | 16.38 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 16507 / 6508 | 13814 / 5912 | 17154 / 6436 | 17540 / 6821 | 14801 / 6027 | 9579 / 6357 |
| NS bytes per delivery (native) | 790 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 3.99 | 15.59 | 13.70 | 23.92 | 26.70 | 56.16 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 1.23 | 3.76 | 1.51 | 4.16 | 4.80 | 8.49 |
| LN per delta µs (native) | 0.78 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 15.7 | 35.7 | 54.8 | 78.3 | 107.8 | 83.4 |  |
| 浏览（全员拖动）: egress KB/s (map) | 78867 raw / 29263 zstd | 67627 raw / 26424 zstd | 81546 raw / 29615 zstd | 80811 raw / 30099 zstd | 71776 raw / 27143 zstd | 44695 raw / 28909 zstd |  |
| 静止·和平: server CPU ms/s | 0.5 | 1.1 | 0.9 | 1.7 | 2.0 | 3.3 | 4.2 |
| 静止·和平: egress KB/s (map) | 450 raw / 265 zstd | 306 raw / 199 zstd | 476 raw / 234 zstd | 485 raw / 283 zstd | 342 raw / 205 zstd | 175 raw / 186 zstd | 121 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 1.5 | 3.4 | 4.6 | 6.8 | 8.5 | 9.9 | 5.7 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 4523 raw / 1839 zstd | 3741 raw / 1616 zstd | 4709 raw / 1811 zstd | 4842 raw / 1933 zstd | 4016 raw / 1662 zstd | 2592 raw / 1756 zstd | 410 raw |
| 静止·压力上限: server CPU ms/s | 5.0 | 12.8 | 19.7 | 26.6 | 34.9 | 28.8 | 16.0 |
| 静止·压力上限: egress KB/s (map) | 24179 raw / 9533 zstd | 20235 raw / 8661 zstd | 25128 raw / 9427 zstd | 25694 raw / 9992 zstd | 21681 raw / 8829 zstd | 14032 raw / 9312 zstd | 1157 raw |
