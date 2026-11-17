Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 87.3 | 187.6 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 265.4 | 875.8 | 0.329 | 0.214 |
| MessagePack | 9,577.0 | 5,334.8 | 5,609.9 | 0.016 | 0.033 |
| Protocol Buffers | 15,780.6 | 6,891.7 | 12,293.6 | 0.013 | 0.015 |
| FlatBuffers | 6,794.4 | 543.4 | 944.1 | 0.161 | 0.199 |
| Cap'n Proto | 2,592.2 | 1,924.3 | 2,268.1 | 0.045 | 0.083 |
| cista raw | 5,315.6 | 539.9 | 630.8 | 0.162 | 0.297 |
| cista offset | 5,845.5 | 547.4 | 643.3 | 0.159 | 0.292 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 202.0 | 302.9 |
| FlatBuffers view | 421.7 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 170.5 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 273.9 | 0.622 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 273.9 | 0.622 |
| Protocol Buffers | google::protobuf message (removed messages freed) | 565.4 | 450.7 | 0.378 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 273.9 | 0.622 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 273.9 | 0.622 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 184.3 | 0.925 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 207.0 | 0.824 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 164.0 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,112 | 3,006 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,804 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 8,272 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 170.5 | 51.1 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 273.9 | 70.0 | 0.730 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message); removed messages freed | 450.7 | 288.2 | 0.177 |
| cista raw inline | inline attributes, range insert, erase at position | 184.3 | 54.3 | 0.941 |
| cista offset inline | inline attributes, range insert, erase at position | 207.0 | 56.5 | 0.904 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 49.1 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 96.7 |
| Protocol Buffers reuse (2024 layout, RemoveLast keeps removed messages for the next add_) | 65.5 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 71.1 | 168.2 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 257.1 | 688.7 | 0.277 | 0.244 | 2,948 | 2,958 |
| cista raw inline | 523.6 | 627.6 | 0.136 | 0.268 | 3,280 | 2,994 |
| cista offset inline | 534.0 | 628.9 | 0.133 | 0.267 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 8,536 B (zstd-3 3,213), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 257.8 | 122.2 | 1.000 | 1.000 |
| zpp::bits (std model) | 539.3 | 327.1 | 0.478 | 0.374 |
| Protocol Buffers | 7,342.4 | 7,179.9 | 0.035 | 0.017 |
| cista raw | 724.2 | 577.9 | 0.356 | 0.211 |
| cista offset | 754.4 | 590.5 | 0.342 | 0.207 |
