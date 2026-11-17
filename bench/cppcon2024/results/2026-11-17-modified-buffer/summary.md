Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 78.4 | 204.9 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 319.4 | 1,063.4 | 0.245 | 0.193 |
| MessagePack | 9,577.0 | 6,605.6 | 6,832.9 | 0.012 | 0.030 |
| Protocol Buffers | 15,780.6 | 8,766.7 | 14,995.7 | 0.009 | 0.014 |
| FlatBuffers | 6,794.4 | 649.1 | 1,137.1 | 0.121 | 0.180 |
| Cap'n Proto | 2,592.2 | 2,360.5 | 2,817.6 | 0.033 | 0.073 |
| cista raw | 5,315.6 | 672.2 | 788.0 | 0.117 | 0.260 |
| cista offset | 5,845.5 | 660.0 | 769.4 | 0.119 | 0.266 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 191.2 | 300.9 |
| FlatBuffers view | 492.9 |  |

### Send + receive of a modified XOffset buffer (reuse, report-only)

| Row | Wire bytes | zstd-3 | ns per message | ÷ zpp::bits | ÷ cista raw |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0, as filled | 3,064 | 2,986 | 78.4 | 0.245 | 0.117 |
| xoffset 4.0, after the read/write rounds | 7,928 | 3,173 | 180.7 | 0.566 | 0.269 |
| xoffset 4.0 full, as filled | 3,064 | 2,986 | 191.2 | 0.599 | 0.284 |
| xoffset 4.0 full, after the read/write rounds | 7,928 | 3,173 | 331.0 | 1.036 | 0.492 |
| xoffset inline, as filled | 3,128 | 2,977 | 78.1 | 0.250 | 0.122 |
| xoffset inline, after the read/write rounds | 8,536 | 3,213 | 187.6 | 0.602 | 0.293 |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 150.7 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 336.8 | 0.447 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 336.8 | 0.447 |
| Protocol Buffers | google::protobuf message (removed messages freed) | 565.4 | 544.7 | 0.277 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 336.8 | 0.447 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 336.8 | 0.447 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 224.3 | 0.672 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 250.4 | 0.602 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 147.6 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,064 | 2,986 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,803 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 7,928 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 150.7 | 62.4 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 336.8 | 87.4 | 0.714 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message); removed messages freed | 544.7 | 349.3 | 0.179 |
| cista raw inline | inline attributes, range insert, erase at position | 224.3 | 62.6 | 0.997 |
| cista offset inline | inline attributes, range insert, erase at position | 250.4 | 69.6 | 0.897 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 59.4 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 118.1 |
| Protocol Buffers reuse (2024 layout, RemoveLast keeps removed messages for the next add_) | 81.8 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 78.1 | 202.8 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 311.8 | 820.9 | 0.250 | 0.247 | 2,948 | 2,958 |
| cista raw inline | 641.3 | 763.9 | 0.122 | 0.265 | 3,280 | 2,994 |
| cista offset inline | 631.2 | 750.6 | 0.124 | 0.270 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 8,536 B (zstd-3 3,213), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 229.1 | 140.5 | 1.000 | 1.000 |
| zpp::bits (std model) | 656.2 | 399.2 | 0.349 | 0.352 |
| Protocol Buffers | 9,311.4 | 9,116.0 | 0.025 | 0.015 |
| cista raw | 896.5 | 703.9 | 0.256 | 0.200 |
| cista offset | 910.4 | 700.8 | 0.252 | 0.200 |

With XOffset sending the buffer the loop has modified (the rivals' rows are the same):

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset, modified buffer | 331.4 | 250.0 | 1.000 | 1.000 |
| zpp::bits (std model) | 656.2 | 399.2 | 0.505 | 0.626 |
| Protocol Buffers | 9,311.4 | 9,116.0 | 0.036 | 0.027 |
| cista raw | 896.5 | 703.9 | 0.370 | 0.355 |
| cista offset | 910.4 | 700.8 | 0.364 | 0.357 |
