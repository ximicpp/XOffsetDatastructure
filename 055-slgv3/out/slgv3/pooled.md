# SLG demo cases v3 (block windows, login, dirty save)

8 processes, M, 4 ticks per cycle; xoffset ÷ rival, geometric mean / one-sided 95% upper bound, Bonferroni k=105. Below 1: XOffset faster.

| 用例 | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | --- | --- | --- | --- | --- |
| 1 场景↔战斗 | 1.185 / 1.298 | 0.546 / 0.615 | 0.546 / 0.611 | 0.434 / 0.482 | 0.167 / 0.184 |
| 1 场景↔场景 | 0.419 / 0.462 | 0.414 / 0.461 | 0.304 / 0.343 | 0.243 / 0.271 | 0.130 / 0.144 |
| 1 场景↔存储（整份） | 0.259 / 0.286 | 0.269 / 0.301 | 0.182 / 0.204 | 0.132 / 0.146 | 0.068 / 0.075 |
| 1 场景↔存储（只存脏模块） | 0.240 / 0.262 | 0.303 / 0.327 | 0.189 / 0.202 | 0.145 / 0.155 | 0.074 / 0.079 |
| 2 登录全量 | 0.286 / 0.316 | 0.307 / 0.334 | 0.190 / 0.214 | 0.179 / 0.196 | 0.089 / 0.094 |
| 3 自身增量 | 0.504 / 0.563 | 1.003 / 1.130 | 0.451 / 0.499 | 0.386 / 0.432 | 0.225 / 0.250 |
| 2 九宫格全量·和平 | 0.922 / 0.937 | 0.722 / 0.737 | 0.769 / 0.782 | 0.619 / 0.625 | 0.702 / 0.711 |
| 3 块增量·和平 | 1.199 / 1.295 | 0.997 / 1.098 | 1.132 / 1.234 | 1.044 / 1.093 | 1.111 / 1.174 |
| 2 九宫格全量·交战 | 0.932 / 0.953 | 0.742 / 0.756 | 0.789 / 0.799 | 0.640 / 0.653 | 0.726 / 0.740 |
| 3 块增量·交战 | 1.130 / 1.168 | 0.923 / 0.968 | 1.031 / 1.084 | 0.888 / 0.950 | 0.973 / 1.060 |
| 2 九宫格全量·压力 | 1.057 / 1.076 | 0.831 / 0.844 | 0.899 / 0.915 | 0.746 / 0.763 | 0.832 / 0.849 |
| 3 块增量·压力 | 1.077 / 1.108 | 0.813 / 0.833 | 0.943 / 0.962 | 0.755 / 0.771 | 0.854 / 0.876 |
| 3 块增量·和平（XOffset 原生） | 0.692 / 0.777 | 0.575 / 0.655 | 0.653 / 0.728 | 0.602 / 0.651 | 0.641 / 0.703 |
| 3 块增量·交战（XOffset 原生） | 0.586 / 0.622 | 0.478 / 0.518 | 0.534 / 0.582 | 0.460 / 0.508 | 0.504 / 0.569 |
| 3 块增量·压力（XOffset 原生） | 0.476 / 0.498 | 0.359 / 0.377 | 0.417 / 0.437 | 0.334 / 0.347 | 0.377 / 0.392 |
| 2 九宫格·和平（XOffset 原生 key） | 0.286 / 0.297 | 0.224 / 0.233 | 0.239 / 0.248 | 0.192 / 0.198 | 0.218 / 0.226 |
| 2 九宫格·交战（XOffset 原生 key） | 0.272 / 0.281 | 0.216 / 0.222 | 0.230 / 0.236 | 0.187 / 0.193 | 0.212 / 0.219 |
| 2 九宫格·压力（XOffset 原生 key） | 0.233 / 0.237 | 0.184 / 0.186 | 0.199 / 0.202 | 0.165 / 0.169 | 0.184 / 0.187 |
| 2 登录全量（XOffset 原生 key） | 0.524 / 0.580 | 0.561 / 0.612 | 0.347 / 0.382 | 0.327 / 0.356 | 0.162 / 0.174 |
| 3 自身增量（XOffset 原生） | 0.363 / 0.413 | 0.723 / 0.830 | 0.325 / 0.366 | 0.278 / 0.316 | 0.162 / 0.184 |
| 1 存储（XOffset 原生增量） | 1.744 / 1.976 | 2.201 / 2.549 | 1.370 / 1.596 | 1.054 / 1.221 | 0.537 / 0.619 |

## Per message (µs; bytes)

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A per battle µs | 2.49 | 1.93 | 4.52 | 4.44 | 5.59 | 15.05 |
| B per player µs | 6.10 | 15.18 | 15.19 | 20.78 | 26.31 | 49.78 |
| B 5,000 players, s | 0.031 | 0.076 | 0.076 | 0.104 | 0.132 | 0.249 |
| S per player µs (save + store + load) | 4.28 | 17.29 | 16.45 | 24.56 | 33.66 | 66.66 |
| SD per player µs | 1.69 | 7.28 | 5.75 | 9.25 | 12.05 | 23.50 |
| SN per player µs (native) | 12.88 |  |  |  |  |  |
| EP per pull µs (send share + 9 recv + index) | 64.5 | 70.0 | 88.6 | 83.6 | 103.5 | 91.6 |
| UP per delivery µs (recv + apply) | 0.97 | 0.80 | 0.97 | 0.86 | 0.97 | 0.89 |
| NP per delivery µs (native recv) | 0.51 |  |  |  |  |  |
| UP bytes per delivery (raw / zstd) | 208 / 132 | 133 / 95 | 214 / 112 | 230 / 136 | 150 / 97 | 69 / 78 |
| NP bytes per delivery (native) | 79 |  |  |  |  |  |
| EP bytes per pull (raw / zstd) | 49413 / 22793 | 43830 / 21366 | 49286 / 22526 | 59747 / 29711 | 47557 / 21662 | 32061 / 23078 |
| EC per pull µs (send share + 9 recv + index) | 76.7 | 81.8 | 101.3 | 95.5 | 116.1 | 103.3 |
| UC per delivery µs (recv + apply) | 1.39 | 1.22 | 1.50 | 1.34 | 1.58 | 1.42 |
| NC per delivery µs (native recv) | 0.60 |  |  |  |  |  |
| UC bytes per delivery (raw / zstd) | 501 / 275 | 393 / 227 | 507 / 253 | 592 / 320 | 434 / 233 | 260 / 227 |
| NC bytes per delivery (native) | 138 |  |  |  |  |  |
| EC bytes per pull (raw / zstd) | 50204 / 23000 | 44594 / 21565 | 50077 / 22746 | 60740 / 30044 | 48339 / 21856 | 32322 / 23243 |
| ES per pull µs (send share + 9 recv + index) | 94.1 | 88.4 | 111.1 | 103.5 | 122.6 | 110.7 |
| US per delivery µs (recv + apply) | 3.35 | 2.98 | 4.03 | 3.38 | 4.27 | 3.77 |
| NS per delivery µs (native recv) | 1.19 |  |  |  |  |  |
| US bytes per delivery (raw / zstd) | 1846 / 916 | 1557 / 824 | 1852 / 883 | 2134 / 1142 | 1725 / 852 | 1116 / 901 |
| NS bytes per delivery (native) | 344 |  |  |  |  |  |
| ES bytes per pull (raw / zstd) | 51177 / 23354 | 45563 / 21916 | 51049 / 23094 | 61992 / 30540 | 49312 / 22216 | 32824 / 23558 |
| L per login µs | 3.98 | 17.22 | 15.65 | 25.91 | 26.78 | 53.10 |
| L bytes per login | 9392 | 6794 | 10153 | 8735 | 7535 | 4554 |
| LD per patch µs | 1.47 | 3.24 | 1.58 | 3.58 | 4.22 | 7.33 |
| LN per delta µs (native) | 0.95 |  |  |  |  |  |
| LD bytes per patch | 738 | 500 | 757 | 670 | 550 | 320 |
| LN bytes per delta (native) | 99 |  |  |  |  |  |

## Per server per second (server CPU ms/s; egress KB/s to clients)

Frequency model: battles 20 / 200 per s (peace / siege), migrations 8/s at a season start, saves 17/s plus 1 login load/s, 1 login/s, self deltas 500/s, views and deltas as simulated (1,500 views; one tick = 1 s). XOffset native columns replace the application-level delta and snapshot stages with N*/K*/LN/LK/SN.

| | xoffset | zpp_bits | cista | flatbuffers | cereal | protobuf | xoffset native |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 和平: server CPU ms/s | 0.5 | 1.2 | 0.9 | 1.8 | 2.1 | 3.2 | 0.8 |
| 和平: egress KB/s (views + deltas) | 13386 raw / 6575 zstd | 11306 raw / 5839 zstd | 13424 raw / 6290 zstd | 15945 raw / 8164 zstd | 12329 raw / 5934 zstd | 7965 raw / 6045 zstd | 7326 raw |
| 交战（攻城 200 场/秒）: server CPU ms/s | 1.0 | 1.5 | 1.8 | 2.7 | 3.2 | 5.9 | 1.2 |
| 交战（攻城 200 场/秒）: egress KB/s (views + deltas) | 17099 raw / 8351 zstd | 14592 raw / 7482 zstd | 17139 raw / 8044 zstd | 20521 raw / 10460 zstd | 15901 raw / 7618 zstd | 10290 raw / 7857 zstd | 8126 raw |
| 压力上限: server CPU ms/s | 0.6 | 1.3 | 1.1 | 2.1 | 2.4 | 3.4 | 0.9 |
| 压力上限: egress KB/s (views + deltas) | 32767 raw / 15804 zstd | 28184 raw / 14418 zstd | 32807 raw / 15361 zstd | 38527 raw / 20020 zstd | 30962 raw / 14814 zstd | 20232 raw / 15679 zstd | 10602 raw |
