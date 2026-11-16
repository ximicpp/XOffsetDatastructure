| Metric | PR #286 | Audited original | Inline layout | Fastest rival | Median ratio | Wins |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| sr_reuse | 197.7 | 203.1 | 154.7 | 826.5 | 0.187 | 9/9 |
| sr_fresh | 520.3 | 521.4 | 453.0 | 1901.3 | 0.238 | 9/9 |
| rw | 690.7 | 691.5 | 155.8 | 150.5 | 1.044 | 3/9 |

- RW vs std (shared): 0.239 (rival median 654.4 ns)
- RW vs Protocol Buffers: 1.044 (rival median 150.5 ns)
- RW vs cista raw: 0.322 (rival median 499.7 ns)
- RW vs cista offset: 0.292 (rival median 529.5 ns)

- bytes_zstd3: 2977 B (max ratio 1.017)
- bytes_after_rw_zstd3: 3063 B (max ratio 1.047)
- bytes_after_rw_compact_zstd3: 3006 B (max ratio 1.027)

FAIL
