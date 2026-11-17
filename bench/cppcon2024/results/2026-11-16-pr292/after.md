Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 95.8 | 224.9 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 343.3 | 1,094.4 | 0.279 | 0.206 |
| MessagePack | 9,577.0 | 6,510.2 | 6,926.9 | 0.015 | 0.032 |
| Protocol Buffers | 15,780.6 | 8,675.7 | 15,414.9 | 0.011 | 0.015 |
| FlatBuffers | 6,794.4 | 662.7 | 1,184.0 | 0.145 | 0.190 |
| Cap'n Proto | 2,592.2 | 2,380.6 | 2,823.3 | 0.040 | 0.080 |
| cista raw | 5,315.6 | 654.0 | 773.9 | 0.146 | 0.291 |
| cista offset | 5,845.5 | 659.8 | 793.4 | 0.145 | 0.283 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 231.5 | 360.7 |
| FlatBuffers view | 514.5 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 239.0 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 339.5 | 0.704 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 339.5 | 0.704 |
| Protocol Buffers | google::protobuf message | 565.4 | 78.0 | 3.064 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 339.5 | 0.704 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 339.5 | 0.704 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 224.4 | 1.065 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 245.4 | 0.974 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 197.9 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,232 | 3,001 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,802 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 8,600 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 239.0 | 62.1 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 339.5 | 79.0 | 0.786 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message) | 78.0 | 72.2 | 0.860 |
| cista raw inline | inline attributes, range insert, erase at position | 224.4 | 46.4 | 1.338 |
| cista offset inline | inline attributes, range insert, erase at position | 245.4 | 53.4 | 1.163 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 61.2 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 133.2 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 81.0 | 208.2 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 314.4 | 832.9 | 0.258 | 0.250 | 2,948 | 2,958 |
| cista raw inline | 668.7 | 770.0 | 0.121 | 0.270 | 3,280 | 2,994 |
| cista offset inline | 635.0 | 789.7 | 0.128 | 0.264 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 9,048 B (zstd-3 3,128), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 334.8 | 143.1 | 1.000 | 1.000 |
| zpp::bits (std model) | 682.8 | 393.4 | 0.490 | 0.364 |
| Protocol Buffers | 8,753.7 | 8,747.9 | 0.038 | 0.016 |
| cista raw | 878.4 | 715.1 | 0.381 | 0.200 |
| cista offset | 905.2 | 688.4 | 0.370 | 0.208 |
