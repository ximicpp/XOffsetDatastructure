# Instructions and simulated L1 data misses per message (cachegrind, 2026-11-17)

`valgrind --tool=cachegrind --cache-sim=yes premises_bench --only <cell> --iters N` at two N
(2,000 and 4,000; 500 and 1,000 at k = 4; 100 and 200 at k = 16; 5 and 10 at k = 256), difference
divided by the difference in N, so setup is excluded. The simulated L1d is the host's: 48 KiB,
12-way, 64-byte lines. Library v4.1.0, GCC 16.2 `-O3`.

From k = 4 (12 KB of wire) XOffset's counts grow by two per wire byte: 24,788 = 2 × 11,952 + 884,
95,853 = 2 × 47,296 + 1,261, 1,504,079 = 2 × 745,968 + 12,143. That is the signature of the two
`memcpy` calls running as `rep movsb`, which valgrind counts byte by byte, so these counts say
nothing about time. They do show the structure: trusted send + receive is two copies of the wire
(sender to string, string to receiver) plus about 400–1,300 instructions, however many fields the
message has.

| Cell | Bytes sent | Instructions | L1d misses |
| --- | ---: | ---: | ---: |
| `size/xo_admit/0/clean` | 152 | 387 | 0 |
| `size/xo_full/0/clean` | 152 | 655 | 0 |
| `size/zpp_bits/0/clean` | 68 | 672 | 0 |
| `size/cista_raw_unchecked/0/clean` | 176 | 676 | 0 |
| `size/cista_raw_checked/0/clean` | 176 | 1,487 | 0 |
| `size/xo_admit/1/16/clean` | 552 | 497 | 0 |
| `size/xo_full/1/16/clean` | 552 | 958 | 0 |
| `size/zpp_bits/1/16/clean` | 276 | 1,363 | 0 |
| `size/cista_raw_unchecked/1/16/clean` | 408 | 1,838 | 0 |
| `size/cista_raw_checked/1/16/clean` | 408 | 3,852 | 0 |
| `size/xo_admit/1/clean` | 3,064 | 976 | 0 |
| `size/xo_full/1/clean` | 3,064 | 2,712 | 0 |
| `size/zpp_bits/1/clean` | 2,916 | 6,246 | 0 |
| `size/cista_raw_unchecked/1/clean` | 3,216 | 10,083 | 0 |
| `size/cista_raw_checked/1/clean` | 3,216 | 23,365 | 0 |
| `size/xo_admit/4/clean` | 11,952 | 24,788 | 0 |
| `size/xo_full/4/clean` | 11,952 | 30,693 | 0 |
| `size/zpp_bits/4/clean` | 11,460 | 29,264 | 18 |
| `size/cista_raw_unchecked/4/clean` | 12,336 | 55,268 | 0 |
| `size/cista_raw_checked/4/clean` | 12,336 | 105,054 | 0 |
| `size/xo_admit/16/clean` | 47,296 | 95,853 | 3,038 |
| `size/xo_full/16/clean` | 47,296 | 115,981 | 3,594 |
| `size/zpp_bits/16/clean` | 45,636 | 138,777 | 3,365 |
| `size/cista_raw_unchecked/16/clean` | 48,816 | 255,944 | 4,360 |
| `size/xo_admit/256/clean` | 745,968 | 1,504,079 | 46,900 |
| `size/xo_full/256/clean` | 745,968 | 1,836,614 | 56,893 |
| `size/zpp_bits/256/clean` | 729,156 | 2,087,616 | 58,748 |
| `size/cista_raw_unchecked/256/clean` | 778,416 | 4,259,965 | 81,735 |
| `size/flatbuffers_noverify/1/clean` | 3,680 | 9,763 | 0 |
| `size/flatbuffers_verify/1/clean` | 3,680 | 12,064 | 0 |
| `size/protobuf/1/clean` | 4,095 | 125,863 | 0 |
| `size/cereal/1/clean` | 2,972 | 61,908 | 0 |
| `dirt/xo_admit/1/rw1` | 7,928 | 2,681 | 0 |
| `dirt/xo_full/1/rw1` | 7,928 | 5,070 | 0 |
| `dirt/xo_admit/1/g2` | 13,840 | 28,953 | 51 |
| `dirt/xo_full/1/g2` | 13,840 | 31,959 | 142 |
| `dirt/xo_admit/1/g8` | 47,560 | 97,202 | 3,042 |
| `dirt/xo_full/1/g8` | 47,560 | 102,250 | 3,114 |
| `dirt/xo_admit_zero_runs/1/rw1` | 3,304 | 6,928 | 0 |
| `dirt/xo_compact_admit/1/rw1` | 3,552 | 2,493 | 0 |
| `dirt/xo_compact0_admit/1/rw1` | 3,064 | 2,219 | 0 |
| `churn/xo_admit/1/churn100000` | 8,120 | 3,372 | 0 |
| `churn/zpp_bits/1/churn100000` | 3,153 | 6,755 | 0 |
