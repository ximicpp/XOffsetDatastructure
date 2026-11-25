# Instructions and simulated L1 data misses per message (cachegrind, 2026-11-25)

`valgrind --tool=cachegrind --cache-sim=yes --I1=32768,8,64 --D1=49152,12,64 --LL=2097152,16,64
premises_bench --only <cell> --iters N` at two N (2,000 and 4,000; 100 and 200 at k = 16), difference
divided by the difference in N, so setup is excluded. The simulated L1d is the host's: 48 KiB,
12-way, 64-byte lines. Library v4.3.0 (tree `5a54d9d1`), GCC 16.2 `-O3`, the binary of
[summary.md](summary.md).

At k = 16 XOffset's counts grow by two per wire byte (93,904 = 2 × 46,840 + 224): the two `memcpy`
calls run as `rep movsb`, which valgrind counts byte by byte, so these counts say nothing about time
there. They show the structure: trusted send + receive is two copies of the wire plus a few hundred
instructions, however many fields the message has. A churned sender (`dirt`, `churn`) goes out
compact under the default emission, so its count includes the compact walk; `xo_admit_zero_runs`
sends the churned image with zero runs skipped.

| Cell | Bytes sent | Instructions | L1d misses |
| --- | ---: | ---: | ---: |
| `size/xo_admit/0/clean` | 152 | 157 | 0 |
| `size/xo_full/0/clean` | 152 | 489 | 0 |
| `size/zpp_bits/0/clean` | 68 | 680 | 0 |
| `size/xo_admit/1/16/clean` | 552 | 275 | 0 |
| `size/xo_full/1/16/clean` | 552 | 801 | 0 |
| `size/zpp_bits/1/16/clean` | 276 | 1,373 | 0 |
| `size/xo_admit/1/clean` | 3,160 | 755 | 0 |
| `size/xo_full/1/clean` | 3,160 | 2,924 | 0 |
| `size/zpp_bits/1/clean` | 2,916 | 6,264 | 0 |
| `size/xo_admit/16/clean` | 46,840 | 93,904 | 2,958 |
| `size/xo_full/16/clean` | 46,840 | 114,014 | 3,543 |
| `size/zpp_bits/16/clean` | 45,636 | 138,801 | 3,367 |
| `dirt/xo_admit/1/rw1` | 3,552 | 2,119 | 0 |
| `dirt/xo_full/1/rw1` | 3,552 | 3,438 | 0 |
| `dirt/xo_admit_zero_runs/1/rw1` | 3,440 | 6,727 | 0 |
| `dirt/xo_admit/1/g8` | 3,552 | 2,331 | 0 |
| `churn/xo_admit/1/churn100000` | 3,824 | 2,373 | 0 |
| `churn/zpp_bits/1/churn100000` | 3,153 | 6,766 | 0 |
