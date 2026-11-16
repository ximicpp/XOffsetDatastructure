| Metric | PR #286 | Audited original | Inline layout | Fastest rival | Median ratio | Wins |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| sr_reuse | 199.9 | 201.6 | 152.5 | 808.1 | 0.190 | 9/9 |
| sr_fresh | 520.8 | 515.1 | 447.9 | 1902.6 | 0.235 | 9/9 |
| rw | 717.6 | 699.0 | 138.7 | 154.6 | 0.889 | 8/9 |

- RW vs std (shared): 0.219 (rival median 637.8 ns)
- RW vs Protocol Buffers: 0.889 (rival median 154.6 ns)
- RW vs cista raw: 0.292 (rival median 476.8 ns)
- RW vs cista offset: 0.257 (rival median 549.4 ns)

- bytes_zstd3: 2977 B (max ratio 1.017)
- bytes_after_rw_zstd3: 3059 B (max ratio 1.045)
- bytes_after_rw_compact_zstd3: 3006 B (max ratio 1.027)

PASS
