Median of 15 process runs (each metric: median of its rounds), ns per message / iteration.

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 99.4 | 259.8 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 324.6 | 1,112.6 | 0.306 | 0.234 |
| MessagePack | 9,577.0 | 6,277.5 | 6,884.9 | 0.016 | 0.038 |
| Protocol Buffers | 15,780.6 | 8,824.3 | 14,718.4 | 0.011 | 0.018 |
| FlatBuffers | 6,794.4 | 657.4 | 1,147.2 | 0.151 | 0.226 |
| Cap'n Proto | 2,592.2 | 2,409.4 | 2,882.4 | 0.041 | 0.090 |
| cista raw | 5,315.6 | 643.7 | 771.1 | 0.154 | 0.337 |
| cista offset | 5,845.5 | 656.7 | 785.2 | 0.151 | 0.331 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 235.3 | 376.2 |
| FlatBuffers view | 532.4 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 257.6 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 346.2 | 0.744 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 346.2 | 0.744 |
| Protocol Buffers | google::protobuf message | 565.4 | 78.6 | 3.277 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 346.2 | 0.744 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 346.2 | 0.744 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 225.0 | 1.145 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 245.5 | 1.049 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 215.2 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,232 | 3,001 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,803 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 8,600 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 257.6 | 67.7 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 346.2 | 80.0 | 0.846 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message) | 78.6 | 73.9 | 0.916 |
| cista raw inline | inline attributes, range insert, erase at position | 225.0 | 48.4 | 1.399 |
| cista offset inline | inline attributes, range insert, erase at position | 245.5 | 48.7 | 1.390 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 65.1 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 134.2 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 80.7 | 214.6 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 313.3 | 849.5 | 0.258 | 0.253 | 2,948 | 2,958 |
| cista raw inline | 648.7 | 760.6 | 0.124 | 0.282 | 3,280 | 2,994 |
| cista offset inline | 631.3 | 748.1 | 0.128 | 0.287 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 9,048 B (zstd-3 3,059), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 357.0 | 148.4 | 1.000 | 1.000 |
| zpp::bits (std model) | 670.8 | 393.3 | 0.532 | 0.377 |
| Protocol Buffers | 8,902.9 | 8,898.2 | 0.040 | 0.017 |
| cista raw | 868.7 | 697.1 | 0.411 | 0.213 |
| cista offset | 902.2 | 680.0 | 0.396 | 0.218 |
