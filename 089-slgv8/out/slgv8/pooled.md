# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=85. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.529 / 0.536 | 0.188 / 0.191 | 0.230 / 0.235 | 0.150 / 0.153 | 0.053 / 0.054 |
| 1 场景↔场景 | 0.449 / 0.464 | 0.225 / 0.231 | 0.253 / 0.259 | 0.138 / 0.142 | 0.058 / 0.060 |
| 1 场景↔存储（整份） | 0.387 / 0.392 | 0.171 / 0.173 | 0.307 / 0.311 | 0.113 / 0.115 | 0.058 / 0.059 |
| 1 场景↔存储（只存脏模块） | 0.345 / 0.348 | 0.199 / 0.204 | 0.365 / 0.370 | 0.146 / 0.148 | 0.079 / 0.082 |
| 2 登录全量 | 0.483 / 0.497 | 0.165 / 0.169 | 0.172 / 0.177 | 0.125 / 0.127 | 0.046 / 0.047 |
| 2 浏览地图·视口全量 | 0.471 / 0.475 | 0.192 / 0.194 | 0.205 / 0.207 | 0.126 / 0.127 | 0.147 / 0.149 |
| 2 浏览地图·视口全量（单设备热视图：EB.send + EBW.recv） | 0.449 / 0.453 | 0.183 / 0.184 | 0.202 / 0.203 | 0.118 / 0.118 | 0.143 / 0.146 |
| 3 自身增量 | 0.706 / 0.720 | 0.489 / 0.509 | 0.562 / 0.574 | 0.357 / 0.366 | 0.141 / 0.144 |
| 3 静止视口增量·和平 | 1.012 / 1.050 | 0.706 / 0.732 | 0.743 / 0.773 | 0.599 / 0.626 | 0.718 / 0.738 |
| 3 静止视口增量·交战 | 0.514 / 0.521 | 0.330 / 0.339 | 0.332 / 0.337 | 0.212 / 0.215 | 0.269 / 0.276 |
| 3 静止视口增量·压力（只报） | 0.602 / 0.611 | 0.310 / 0.317 | 0.323 / 0.328 | 0.217 / 0.221 | 0.242 / 0.248 |
| 3 静止视口增量·和平（XOffset 原生） | 1.587 / 1.648 | 1.108 / 1.144 | 1.165 / 1.202 | 0.939 / 0.963 | 1.126 / 1.168 |
| 3 静止视口增量·交战（XOffset 原生） | 0.783 / 0.789 | 0.502 / 0.512 | 0.505 / 0.511 | 0.323 / 0.326 | 0.409 / 0.418 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 0.707 / 0.716 | 0.365 / 0.373 | 0.380 / 0.384 | 0.255 / 0.257 | 0.285 / 0.289 |
| 2 登录全量（XOffset 原生 key） | 1.845 / 1.881 | 0.630 / 0.645 | 0.656 / 0.674 | 0.476 / 0.483 | 0.175 / 0.178 |
| 3 自身增量（XOffset 原生） | 0.471 / 0.482 | 0.326 / 0.340 | 0.375 / 0.381 | 0.238 / 0.243 | 0.094 / 0.096 |
| 1 存储（XOffset 原生增量） | 1.895 / 1.927 | 1.095 / 1.120 | 2.008 / 2.054 | 0.803 / 0.816 | 0.434 / 0.449 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 0.74 | 1.44 | 3.96 | 3.31 | 4.96 | 14.22 |
| B per player µs | 2.34 | 5.19 | 10.28 | 9.19 | 16.73 | 40.18 |
| B 5,000 players, s | 0.012 | 0.026 | 0.051 | 0.046 | 0.084 | 0.201 |
| S per player µs (save + store + load) | 2.85 | 7.39 | 16.61 | 9.31 | 25.12 | 49.47 |
| SD per player µs | 1.15 | 3.36 | 5.81 | 3.17 | 7.91 | 14.69 |
| SN per player µs (native) | 6.41 |  |  |  |  |  |
| EB per pull µs (send + recv) | 14.7 | 31.5 | 76.6 | 71.9 | 116.7 | 100.5 |
| EBW per pull µs (send + warm single-view recv) | 13.7 | 30.9 | 75.2 | 68.0 | 116.5 | 95.7 |
| EB.recv / EBW.recv per pull µs (1,500 cold views / one warm view) | 5.03 / 4.02 | 10.28 / 9.62 | 42.05 / 40.61 | 26.88 / 22.93 | 47.98 / 47.79 | 48.76 / 43.93 |
| EB bytes per pull (raw / zstd) | 53840 / 19977 | 46167 / 18039 | 55668 / 19891 | 55167 / 20547 | 48999 / 18530 | 30512 / 19735 |
| UP per delivery µs (send + recv + apply) | 0.62 | 0.62 | 0.88 | 0.83 | 1.03 | 0.86 |
| NP per delivery µs (native send + diff + recv) | 0.95 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 316 / 186 | 215 / 140 | 335 / 164 | 341 / 199 | 241 / 144 | 123 / 131 |
| NP bytes per delivery (native) | 85 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1456 | 1456 | 1456 | 1456 | 1456 | 1456 |
| UC per delivery µs (send + recv + apply) | 1.76 | 3.43 | 5.35 | 5.30 | 8.29 | 6.53 |
| NC per delivery µs (native send + diff + recv) | 2.65 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 3093 / 1257 | 2558 / 1105 | 3220 / 1235 | 3311 / 1322 | 2746 / 1137 | 1773 / 1201 |
| NC bytes per delivery (native) | 280 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 8.93 | 14.84 | 28.82 | 27.53 | 40.86 | 36.62 |
| NS per delivery µs (native send + diff + recv) | 10.32 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 16507 / 6508 | 13814 / 5912 | 17154 / 6436 | 17540 / 6821 | 14801 / 6027 | 9579 / 6357 |
| NS bytes per delivery (native) | 790 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 1.90 | 4.00 | 11.77 | 11.29 | 15.59 | 42.86 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 0.72 | 1.04 | 1.51 | 1.30 | 2.06 | 5.34 |
| LN per delta µs (native) | 0.47 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 14.7 | 32.2 | 52.4 | 68.0 | 103.9 | 79.8 |  |
| 浏览（全员拖动）: egress KB/s (map) | 78867 raw / 29263 zstd | 67627 raw / 26424 zstd | 81546 raw / 29137 zstd | 80811 raw / 30099 zstd | 71776 raw / 27143 zstd | 44695 raw / 28909 zstd |  |
| 静止·和平: server CPU ms/s | 0.3 | 0.5 | 0.8 | 0.9 | 1.3 | 2.4 | 1.2 |
| 静止·和平: egress KB/s (map) | 450 raw / 265 zstd | 306 raw / 199 zstd | 476 raw / 234 zstd | 485 raw / 283 zstd | 342 raw / 205 zstd | 175 raw / 186 zstd | 121 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 1.2 | 2.6 | 4.4 | 5.3 | 7.6 | 9.0 | 3.2 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 4523 raw / 1839 zstd | 3741 raw / 1616 zstd | 4709 raw / 1806 zstd | 4842 raw / 1933 zstd | 4016 raw / 1662 zstd | 2592 raw / 1756 zstd | 410 raw |
| 静止·压力上限: server CPU ms/s | 5.3 | 11.6 | 18.4 | 22.5 | 33.4 | 27.1 | 12.0 |
| 静止·压力上限: egress KB/s (map) | 24179 raw / 9533 zstd | 20235 raw / 8661 zstd | 25128 raw / 9427 zstd | 25694 raw / 9992 zstd | 21681 raw / 8829 zstd | 14032 raw / 9312 zstd | 1157 raw |
