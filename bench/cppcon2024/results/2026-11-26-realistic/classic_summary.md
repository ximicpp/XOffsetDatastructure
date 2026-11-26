Median of 9 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 120.5 | 230.9 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 774.6 | 2,165.8 | 0.156 | 0.107 |
| MessagePack | 9,577.0 | 12,334.5 | 13,374.0 | 0.010 | 0.017 |
| Protocol Buffers | 15,780.6 | 18,839.5 | 29,131.7 | 0.006 | 0.008 |
| FlatBuffers | 6,794.4 | 1,311.3 | 2,518.1 | 0.092 | 0.092 |
| Cap'n Proto | 2,592.2 | 5,201.8 | 6,006.8 | 0.023 | 0.038 |
| cista raw | 5,315.6 | 1,391.5 | 1,672.1 | 0.087 | 0.138 |
| cista offset | 5,845.5 | 1,376.7 | 1,646.8 | 0.088 | 0.140 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 385.7 | 508.0 |
| FlatBuffers view | 977.5 |  |

### Send + receive of a modified XOffset buffer (reuse, report-only)

| Row | Wire bytes | zstd-3 | ns per message | ÷ zpp::bits | ÷ cista raw |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0, as filled | 3,160 | 3,023 | 120.5 | 0.156 | 0.087 |
| xoffset 4.0, after the read/write rounds | 3,552 | 3,019 | 332.5 | 0.429 | 0.239 |
| xoffset 4.0 full, as filled | 3,160 | 3,023 | 385.7 | 0.498 | 0.277 |
| xoffset 4.0 full, after the read/write rounds | 3,552 | 3,019 | 476.4 | 0.615 | 0.342 |
| xoffset inline, as filled | 3,128 | 2,974 | 124.5 | 0.173 | 0.099 |
| xoffset inline, after the read/write rounds | 3,632 | 3,003 | 260.6 | 0.362 | 0.208 |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 309.7 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 675.0 | 0.459 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 675.0 | 0.459 |
| Protocol Buffers | google::protobuf message (removed messages freed) | 565.4 | 1,099.6 | 0.282 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 675.0 | 0.459 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 675.0 | 0.459 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 541.2 | 0.572 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 597.0 | 0.519 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 293.7 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,160 | 3,023 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,805 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 3,552 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 309.7 | 122.8 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 675.0 | 170.5 | 0.720 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message); removed messages freed | 1,099.6 | 697.4 | 0.176 |
| cista raw inline | inline attributes, range insert, erase at position | 541.2 | 122.4 | 1.003 |
| cista offset inline | inline attributes, range insert, erase at position | 597.0 | 135.5 | 0.906 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 122.5 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 208.4 |
| Protocol Buffers reuse (2024 layout, RemoveLast keeps removed messages for the next add_) | 156.6 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 124.5 | 224.3 | 1.000 | 1.000 | 3,128 | 2,974 |
| zpp::bits inline | 720.4 | 1,930.6 | 0.173 | 0.116 | 2,948 | 2,958 |
| cista raw inline | 1,253.5 | 1,553.4 | 0.099 | 0.144 | 3,280 | 2,994 |
| cista offset inline | 1,237.0 | 1,572.1 | 0.101 | 0.143 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 3,632 B (zstd-3 3,003), save_compact 3,632 B (zstd-3 3,003).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 430.2 | 247.3 | 1.000 | 1.000 |
| zpp::bits (std model) | 1,449.6 | 890.9 | 0.297 | 0.278 |
| Protocol Buffers | 19,939.1 | 19,536.9 | 0.022 | 0.013 |
| cista raw | 1,932.7 | 1,375.9 | 0.223 | 0.180 |
| cista offset | 1,973.7 | 1,372.5 | 0.218 | 0.180 |

With XOffset sending the buffer the loop has modified (the rivals' rows are the same):

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset, modified buffer | 642.2 | 383.4 | 1.000 | 1.000 |
| zpp::bits (std model) | 1,449.6 | 890.9 | 0.443 | 0.430 |
| Protocol Buffers | 19,939.1 | 19,536.9 | 0.032 | 0.020 |
| cista raw | 1,932.7 | 1,375.9 | 0.332 | 0.279 |
| cista offset | 1,973.7 | 1,372.5 | 0.325 | 0.279 |
