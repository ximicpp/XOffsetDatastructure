| Metric | PR #286 | Audited original | Inline layout | Fastest rival | Median ratio | Wins |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| sr_reuse | 71.1 | 71.9 | 59.7 | 194.6 | 0.308 | 9/9 |
| sr_fresh | 198.3 | 198.4 | 176.7 | 667.8 | 0.266 | 9/9 |
| rw | 234.6 | 234.3 | 60.8 | 80.5 | 0.755 | 9/9 |

- RW vs std (shared): 0.196 (rival median 308.5 ns)
- RW vs Protocol Buffers: 0.755 (rival median 80.5 ns)
- RW vs cista raw: 0.317 (rival median 192.1 ns)
- RW vs cista offset: 0.300 (rival median 203.8 ns)

- bytes_zstd3: 2974 B (max ratio 1.016)
- bytes_after_rw_zstd3: 3059 B (max ratio 1.045)
- bytes_after_rw_compact_zstd3: 3003 B (max ratio 1.026)

PASS
