# Fixed messages: instruction counts (record only)

`run_ir.sh` builds `fixedmsg_bench.cpp` with the P2996 toolchain (-O2, libc++)
and counts instructions per packet under callgrind, with the glibc setting of
`tools/perf_gate`. A feed handler publishes packets of k normalized order-book
updates (40-byte records: timestamp, order id, price, quantity, instrument,
sequence, action, side) into a ring of 512 slots; 4 readers read every field
of every record of every slot. Only writing the packet into its slot (encode)
and receiving and reading it (read) are counted. Not a gate.

| Path | What a packet is |
| --- | --- |
| raw | a hand-written struct: header {seq_first, count} + records; the reader casts in place |
| fixed | `xoffset_fixed_message.hpp`: `write_fixed_batch` / `view_fixed_batch` (8-byte header {count, tag}) |
| copy_floor | raw, but each reader first copies the packet into its own buffer: the floor of any copy-in receive |
| xo | an XOffset root {seq_first, XVector of records}: `reset`, `assign_range`, `save_verified_into`; each reader `reload_verified` into its own buffer |
| xo_root | k = 0 only: the record itself is the XOffset root |

k = 0 sends single records with no batch header (the type is bound by the
connection's handshake).

## 2026-11-04, cloud x86_64, P2996 d77eff1, valgrind 3.22 (commit of this file)

| k | path | bytes | encode | read (per reader) |
| --- | --- | ---: | ---: | ---: |
| 0 | raw | 40 | 43 | 55 |
| 0 | fixed | 40 | 44 | 60 |
| 0 | copy_floor | 40 | 43 | 102 |
| 0 | xo_root | 80 | 509 | 377 |
| 1 | raw | 56 | 62 | 71 |
| 1 | fixed | 48 | 70 | 79 |
| 1 | copy_floor | 56 | 62 | 112 |
| 1 | xo | 104 | 660 | 437 |
| 4 | raw | 176 | 80 | 176 |
| 4 | fixed | 168 | 88 | 184 |
| 4 | copy_floor | 176 | 80 | 235 |
| 4 | xo | 224 | 710 | 560 |
| 16 | raw | 656 | 138 | 596 |
| 16 | fixed | 648 | 146 | 604 |
| 16 | copy_floor | 656 | 138 | 713 |
| 16 | xo | 704 | 864 | 1,050 |

Ir per packet. A fixed message costs what the hand-written struct costs, within
8 instructions to encode and 5 to 8 to read (the tag and size checks); the
records have no bool, so there is no value check. Against an XOffset root
carrying the same records, encode is 6 to 12 times fewer instructions and read
1.7 to 6 times fewer; read includes the fold over the fields, which every path
pays, so the ratio shrinks as k grows. copy_floor shows what copying in alone
costs (47 to 117 per reader).

Cloud wall time (median of three runs, noisy; formal timing would be on the
Mac), measured with the prototype of the header (same instruction counts),
agreed: fixed was level with raw, and the xo path was 1.9 to 6.2 times slower
to encode and 1.5 to 3.8 times slower to read.
