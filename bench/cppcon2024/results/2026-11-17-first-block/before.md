Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 102.7 | 237.7 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 336.5 | 1,035.6 | 0.305 | 0.230 |
| MessagePack | 9,577.0 | 6,281.9 | 6,859.1 | 0.016 | 0.035 |
| Protocol Buffers | 15,780.6 | 8,877.1 | 15,305.2 | 0.012 | 0.016 |
| FlatBuffers | 6,794.4 | 616.4 | 1,106.3 | 0.167 | 0.215 |
| Cap'n Proto | 2,592.2 | 2,239.3 | 2,696.5 | 0.046 | 0.088 |
| cista raw | 5,315.6 | 634.1 | 742.0 | 0.162 | 0.320 |
| cista offset | 5,845.5 | 626.4 | 755.0 | 0.164 | 0.315 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 218.1 | 354.2 |
| FlatBuffers view | 494.6 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 202.6 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 343.9 | 0.589 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 343.9 | 0.589 |
| Protocol Buffers | google::protobuf message (removed messages freed) | 565.4 | 584.1 | 0.347 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 343.9 | 0.589 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 343.9 | 0.589 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 242.4 | 0.836 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 252.5 | 0.802 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 197.8 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,112 | 3,006 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,801 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 8,272 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 202.6 | 61.3 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 343.9 | 84.8 | 0.723 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message); removed messages freed | 584.1 | 370.1 | 0.166 |
| cista raw inline | inline attributes, range insert, erase at position | 242.4 | 63.8 | 0.961 |
| cista offset inline | inline attributes, range insert, erase at position | 252.5 | 67.1 | 0.914 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 58.3 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 113.8 |
| Protocol Buffers reuse (2024 layout, RemoveLast keeps removed messages for the next add_) | 79.8 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 84.1 | 205.3 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 313.9 | 788.3 | 0.268 | 0.260 | 2,948 | 2,958 |
| cista raw inline | 607.7 | 717.9 | 0.138 | 0.286 | 3,280 | 2,994 |
| cista offset inline | 595.4 | 722.1 | 0.141 | 0.284 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 8,536 B (zstd-3 3,213), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 305.3 | 145.4 | 1.000 | 1.000 |
| zpp::bits (std model) | 680.4 | 398.7 | 0.449 | 0.365 |
| Protocol Buffers | 9,461.2 | 9,247.2 | 0.032 | 0.016 |
| cista raw | 876.5 | 671.5 | 0.348 | 0.217 |
| cista offset | 878.9 | 662.5 | 0.347 | 0.219 |
