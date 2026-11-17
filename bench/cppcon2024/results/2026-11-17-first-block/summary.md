Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 83.2 | 204.0 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 339.8 | 1,066.2 | 0.245 | 0.191 |
| MessagePack | 9,577.0 | 6,111.7 | 6,549.9 | 0.014 | 0.031 |
| Protocol Buffers | 15,780.6 | 8,789.4 | 14,979.2 | 0.009 | 0.014 |
| FlatBuffers | 6,794.4 | 614.4 | 1,131.0 | 0.135 | 0.180 |
| Cap'n Proto | 2,592.2 | 2,268.3 | 2,707.9 | 0.037 | 0.075 |
| cista raw | 5,315.6 | 637.1 | 751.7 | 0.131 | 0.271 |
| cista offset | 5,845.5 | 638.9 | 752.3 | 0.130 | 0.271 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 170.5 | 282.7 |
| FlatBuffers view | 481.8 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 143.5 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 342.5 | 0.419 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 342.5 | 0.419 |
| Protocol Buffers | google::protobuf message (removed messages freed) | 565.4 | 592.4 | 0.242 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 342.5 | 0.419 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 342.5 | 0.419 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 235.5 | 0.609 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 259.1 | 0.554 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 140.4 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,064 | 2,986 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,800 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 7,928 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 143.5 | 60.4 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 342.5 | 83.4 | 0.724 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message); removed messages freed | 592.4 | 367.8 | 0.164 |
| cista raw inline | inline attributes, range insert, erase at position | 235.5 | 66.3 | 0.911 |
| cista offset inline | inline attributes, range insert, erase at position | 259.1 | 66.8 | 0.904 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 57.9 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 119.9 |
| Protocol Buffers reuse (2024 layout, RemoveLast keeps removed messages for the next add_) | 76.8 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 84.0 | 201.3 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 306.9 | 774.8 | 0.274 | 0.260 | 2,948 | 2,958 |
| cista raw inline | 615.5 | 734.0 | 0.136 | 0.274 | 3,280 | 2,994 |
| cista offset inline | 593.9 | 735.9 | 0.141 | 0.274 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 8,536 B (zstd-3 3,213), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 226.7 | 144.4 | 1.000 | 1.000 |
| zpp::bits (std model) | 682.3 | 390.3 | 0.332 | 0.370 |
| Protocol Buffers | 9,381.8 | 9,157.2 | 0.024 | 0.016 |
| cista raw | 872.6 | 681.8 | 0.260 | 0.212 |
| cista offset | 898.0 | 660.7 | 0.252 | 0.219 |
