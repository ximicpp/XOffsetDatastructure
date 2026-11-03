# SLG demo cases v4 (viewport browse / static viewport deltas, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=80. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 0.664 / 0.683 | 0.264 / 0.275 | 0.283 / 0.288 | 0.218 / 0.224 | 0.084 / 0.087 |
| 1 场景↔场景 | 0.330 / 0.352 | 0.348 / 0.369 | 0.246 / 0.255 | 0.192 / 0.204 | 0.103 / 0.109 |
| 1 场景↔存储（整份） | 0.190 / 0.207 | 0.209 / 0.227 | 0.137 / 0.147 | 0.098 / 0.106 | 0.051 / 0.055 |
| 1 场景↔存储（只存脏模块） | 0.214 / 0.225 | 0.274 / 0.284 | 0.167 / 0.176 | 0.131 / 0.137 | 0.066 / 0.070 |
| 2 登录全量 | 0.264 / 0.283 | 0.238 / 0.248 | 0.145 / 0.152 | 0.127 / 0.133 | 0.058 / 0.061 |
| 2 浏览地图·视口全量 | 0.521 / 0.529 | 0.169 / 0.173 | 0.197 / 0.200 | 0.097 / 0.099 | 0.146 / 0.152 |
| 3 自身增量 | 0.277 / 0.287 | 0.646 / 0.679 | 0.248 / 0.258 | 0.216 / 0.219 | 0.125 / 0.130 |
| 3 静止视口增量·和平 | 1.189 / 1.236 | 0.855 / 0.892 | 0.853 / 0.874 | 0.746 / 0.766 | 0.925 / 0.958 |
| 3 静止视口增量·交战 | 1.046 / 1.089 | 0.723 / 0.745 | 0.677 / 0.696 | 0.563 / 0.579 | 0.708 / 0.734 |
| 3 静止视口增量·压力（只报） | 0.812 / 0.848 | 0.543 / 0.555 | 0.475 / 0.492 | 0.383 / 0.391 | 0.489 / 0.497 |
| 3 静止视口增量·和平（XOffset 原生） | 1.833 / 1.975 | 1.318 / 1.390 | 1.315 / 1.421 | 1.149 / 1.251 | 1.426 / 1.575 |
| 3 静止视口增量·交战（XOffset 原生） | 1.487 / 1.599 | 1.027 / 1.104 | 0.962 / 1.040 | 0.800 / 0.860 | 1.007 / 1.102 |
| 3 静止视口增量·压力（XOffset 原生，只报） | 1.395 / 1.471 | 0.932 / 0.960 | 0.815 / 0.848 | 0.657 / 0.683 | 0.839 / 0.873 |
| 2 登录全量（XOffset 原生 key） | 0.552 / 0.596 | 0.498 / 0.515 | 0.304 / 0.317 | 0.265 / 0.279 | 0.121 / 0.123 |
| 3 自身增量（XOffset 原生） | 0.186 / 0.193 | 0.434 / 0.456 | 0.167 / 0.173 | 0.145 / 0.147 | 0.084 / 0.087 |
| 1 存储（XOffset 原生增量） | 0.635 / 0.756 | 0.814 / 0.947 | 0.497 / 0.592 | 0.389 / 0.458 | 0.197 / 0.233 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 1.07 | 1.65 | 4.16 | 3.95 | 5.04 | 13.41 |
| B per player µs | 4.90 | 15.37 | 14.43 | 20.29 | 26.40 | 49.47 |
| B 5,000 players, s | 0.025 | 0.077 | 0.072 | 0.101 | 0.132 | 0.247 |
| S per player µs (save + store + load) | 3.33 | 17.82 | 16.09 | 24.55 | 34.32 | 66.91 |
| SD per player µs | 1.54 | 7.29 | 5.69 | 9.28 | 11.90 | 23.60 |
| SN per player µs (native) | 4.44 |  |  |  |  |  |
| EB per pull µs (send + recv) | 2.1 | 4.1 | 12.8 | 10.9 | 22.3 | 14.6 |
| EB bytes per pull (raw / zstd) | 11511 / 4219 | 10786 / 4036 | 11503 / 4186 | 12825 / 5117 | 11266 / 4060 | 6402 / 4207 |
| UP per delivery µs (send + recv + apply) | 0.56 | 0.47 | 0.66 | 0.66 | 0.76 | 0.62 |
| NP per delivery µs (native send + diff + recv) | 0.85 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 228 / 141 | 141 / 100 | 242 / 120 | 246 / 145 | 162 / 103 | 72 / 81 |
| NP bytes per delivery (native) | 80 |  |  |  |  |  |
| UP deliveries per tick (of 1,500 clients) | 1461 | 1461 | 1461 | 1461 | 1461 | 1461 |
| UC per delivery µs (send + recv + apply) | 0.93 | 0.88 | 1.28 | 1.36 | 1.64 | 1.30 |
| NC per delivery µs (native send + diff + recv) | 1.27 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 540 / 292 | 420 / 241 | 555 / 271 | 629 / 339 | 464 / 248 | 272 / 238 |
| NC bytes per delivery (native) | 140 |  |  |  |  |  |
| UC deliveries per tick (of 1,500 clients) | 1498 | 1498 | 1498 | 1498 | 1498 | 1498 |
| US per delivery µs (send + recv + apply) | 2.15 | 2.64 | 3.99 | 4.50 | 5.65 | 4.44 |
| NS per delivery µs (native send + diff + recv) | 3.66 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 1975 / 979 | 1671 / 880 | 1991 / 944 | 2271 / 1214 | 1846 / 908 | 1178 / 954 |
| NS bytes per delivery (native) | 364 |  |  |  |  |  |
| US deliveries per tick (of 1,500 clients) | 1500 | 1500 | 1500 | 1500 | 1500 | 1500 |
| L per login µs | 2.66 | 10.54 | 11.58 | 19.17 | 21.91 | 48.84 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 0.94 | 3.49 | 1.48 | 3.88 | 4.44 | 7.77 |
| LN per delta µs (native) | 0.62 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s; map: 1,500 clients, one tick = 1 s, either all browsing (EB) or all static (U*). XOffset native columns replace the application-level delta stages with N*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 浏览（全员拖动）: server CPU ms/s | 2.2 | 4.6 | 8.6 | 12.1 | 20.3 | 14.3 |  |
| 浏览（全员拖动）: egress KB/s (map) | 16862 raw / 6181 zstd | 15799 raw / 5912 zstd | 16850 raw / 6131 zstd | 18786 raw / 7496 zstd | 16504 raw / 5948 zstd | 9378 raw / 6163 zstd |  |
| 静止·和平: server CPU ms/s | 0.4 | 1.0 | 0.7 | 1.5 | 1.7 | 2.9 | 1.1 |
| 静止·和平: egress KB/s (map) | 325 raw / 201 zstd | 201 raw / 143 zstd | 346 raw / 172 zstd | 350 raw / 207 zstd | 230 raw / 147 zstd | 103 raw / 116 zstd | 114 raw |
| 静止·交战（攻城 200 场/秒）: server CPU ms/s | 0.7 | 1.4 | 1.7 | 2.7 | 3.2 | 5.7 | 1.8 |
| 静止·交战（攻城 200 场/秒）: egress KB/s (map) | 790 raw / 427 zstd | 614 raw / 352 zstd | 811 raw / 396 zstd | 920 raw / 496 zstd | 679 raw / 362 zstd | 398 raw / 348 zstd | 205 raw |
| 静止·压力上限: server CPU ms/s | 0.8 | 1.9 | 2.2 | 4.3 | 4.8 | 5.2 | 4.0 |
| 静止·压力上限: egress KB/s (map) | 2894 raw / 1434 zstd | 2447 raw / 1289 zstd | 2917 raw / 1383 zstd | 3327 raw / 1778 zstd | 2704 raw / 1330 zstd | 1726 raw / 1398 zstd | 533 raw |
