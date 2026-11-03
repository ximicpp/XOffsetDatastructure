# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=85. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.649 / 0.676 | 0.236 / 0.249 | 0.247 / 0.256 | 0.186 / 0.199 | 0.068 / 0.071 |
| 1 场景↔场景 | 0.209 / 0.239 | 0.246 / 0.286 | 0.142 / 0.161 | 0.104 / 0.120 | 0.053 / 0.060 |
| 1 场景↔存储（整份） | 0.177 / 0.186 | 0.192 / 0.203 | 0.126 / 0.134 | 0.089 / 0.095 | 0.046 / 0.048 |
| 1 场景↔存储（只存脏模块） | 0.185 / 0.191 | 0.244 / 0.252 | 0.149 / 0.154 | 0.114 / 0.119 | 0.058 / 0.060 |
| 2 登录全量 | 0.247 / 0.269 | 0.226 / 0.259 | 0.134 / 0.149 | 0.120 / 0.134 | 0.055 / 0.062 |
| 2 浏览地图·视口全量 | 0.469 / 0.476 | 0.193 / 0.196 | 0.193 / 0.196 | 0.126 / 0.128 | 0.145 / 0.147 |
| 2 浏览地图·视口全量（单设备热视图：EB.send + EBW.recv） | 0.440 / 0.447 | 0.181 / 0.184 | 0.188 / 0.191 | 0.117 / 0.119 | 0.140 / 0.142 |
| 3 自身增量 | 0.246 / 0.269 | 0.599 / 0.659 | 0.221 / 0.243 | 0.192 / 0.212 | 0.111 / 0.122 |
| 3 静止视口增量·和平 | 1.193 / 1.316 | 0.824 / 0.924 | 0.878 / 1.020 | 0.721 / 0.842 | 0.883 / 1.043 |
| 3 静止视口增量·交战 | 0.576 / 0.632 | 0.373 / 0.410 | 0.358 / 0.400 | 0.244 / 0.271 | 0.309 / 0.344 |
| 3 静止视口增量·压力（只报） | 0.611 / 0.621 | 0.316 / 0.326 | 0.311 / 0.318 | 0.225 / 0.233 | 0.251 / 0.261 |
| 3 静止视口增量·和平（XOffset 原生） | 1.260 / 1.447 | 0.871 / 1.054 | 0.928 / 1.025 | 0.762 / 0.835 | 0.933 / 1.048 |
| 3 静止视口增量·交战（XOffset 原生） | 0.675 / 0.709 | 0.437 / 0.458 | 0.419 / 0.433 | 0.285 / 0.293 | 0.362 / 0.376 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 0.643 / 0.668 | 0.333 / 0.342 | 0.328 / 0.335 | 0.237 / 0.242 | 0.264 / 0.270 |
| 2 登录全量（XOffset 原生 key） | 0.658 / 0.734 | 0.603 / 0.626 | 0.358 / 0.386 | 0.321 / 0.346 | 0.145 / 0.151 |
| 3 自身增量（XOffset 原生） | 0.134 / 0.137 | 0.325 / 0.336 | 0.120 / 0.124 | 0.104 / 0.109 | 0.060 / 0.063 |
| 1 存储（XOffset 原生增量） | 0.752 / 0.788 | 0.989 / 1.033 | 0.603 / 0.622 | 0.464 / 0.483 | 0.237 / 0.245 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 0.93 | 1.46 | 4.00 | 3.85 | 5.02 | 14.10 |
| B per player µs | 2.39 | 12.41 | 10.50 | 17.88 | 24.74 | 49.78 |
| B 5,000 players, s | 0.012 | 0.062 | 0.052 | 0.089 | 0.124 | 0.249 |
| S per player µs (save + store + load) | 3.13 | 18.54 | 16.76 | 25.61 | 35.91 | 71.46 |
| SD per player µs | 1.44 | 7.71 | 5.87 | 9.58 | 12.43 | 24.67 |
| SN per player µs (native) | 6.05 |  |  |  |  |  |
| EB per pull µs (send + recv) | 14.9 | 32.1 | 77.9 | 78.1 | 118.9 | 103.9 |
| EBW per pull µs (send + warm single-view recv) | 13.8 | 31.7 | 76.5 | 73.8 | 118.8 | 99.0 |
| EB.recv / EBW.recv per pull µs (1,500 cold views / one warm view) | 5.16 / 4.05 | 10.07 / 9.65 | 42.19 / 40.80 | 27.23 / 22.94 | 48.23 / 48.13 | 49.82 / 44.91 |
| EB bytes per pull (raw / zstd) | 53840 / 19977 | 46167 / 18039 | 55668 / 19891 | 55167 / 20547 | 48999 / 18530 | 30512 / 19735 |
| UP per delivery µs (send + recv + apply) | 0.76 | 0.66 | 0.94 | 0.91 | 1.11 | 0.91 |
| NP per delivery µs (native send + diff + recv) | 0.86 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 316 / 186 | 215 / 140 | 335 / 164 | 341 / 199 | 241 / 144 | 123 / 131 |
| NP bytes per delivery (native) | 85 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1456 | 1456 | 1456 | 1456 | 1456 | 1456 |
| UC per delivery µs (send + recv + apply) | 1.99 | 3.59 | 5.52 | 5.77 | 8.49 | 6.73 |
| NC per delivery µs (native send + diff + recv) | 2.42 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 3093 / 1257 | 2558 / 1105 | 3220 / 1235 | 3311 / 1322 | 2746 / 1137 | 1773 / 1201 |
| NC bytes per delivery (native) | 280 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 9.25 | 15.25 | 29.60 | 29.97 | 41.51 | 37.37 |
| NS per delivery µs (native send + diff + recv) | 9.76 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 16507 / 6508 | 13814 / 5912 | 17154 / 6436 | 17540 / 6821 | 14801 / 6027 | 9579 / 6357 |
| NS bytes per delivery (native) | 790 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 2.60 | 11.65 | 12.36 | 21.20 | 23.56 | 52.91 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 0.87 | 3.77 | 1.53 | 4.22 | 4.86 | 8.51 |
| LN per delta µs (native) | 0.48 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 14.8 | 34.0 | 54.1 | 77.6 | 107.5 | 84.0 |  |
| 浏览（全员拖动）: egress KB/s (map) | 78867 raw / 29263 zstd | 67627 raw / 26424 zstd | 81546 raw / 29137 zstd | 80811 raw / 30099 zstd | 71776 raw / 27143 zstd | 44695 raw / 28909 zstd |  |
| 静止·和平: server CPU ms/s | 0.4 | 1.1 | 0.8 | 1.7 | 2.0 | 3.2 | 1.0 |
| 静止·和平: egress KB/s (map) | 450 raw / 265 zstd | 306 raw / 199 zstd | 476 raw / 234 zstd | 485 raw / 283 zstd | 342 raw / 205 zstd | 175 raw / 186 zstd | 121 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 1.4 | 3.2 | 4.5 | 6.7 | 8.4 | 9.8 | 2.8 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 4523 raw / 1839 zstd | 3741 raw / 1616 zstd | 4709 raw / 1806 zstd | 4842 raw / 1933 zstd | 4016 raw / 1662 zstd | 2592 raw / 1756 zstd | 410 raw |
| 静止·压力上限: server CPU ms/s | 5.7 | 12.5 | 19.4 | 26.4 | 34.8 | 28.5 | 11.1 |
| 静止·压力上限: egress KB/s (map) | 24179 raw / 9533 zstd | 20235 raw / 8661 zstd | 25128 raw / 9427 zstd | 25694 raw / 9992 zstd | 21681 raw / 8829 zstd | 14032 raw / 9312 zstd | 1157 raw |
