# HFT demo, phase 1: one process, a real ITCH window (record only)

A trading state service built on a real Nasdaq feed, once with XOffset and
once with each format an HFT reader would compare it with. It measures the
four claims of the HFT demo design (Claude doc "高频交易数据协议调研与对比
demo 设计"):

| Claim | What is measured here |
| --- | --- |
| A, the gate: the hot path is not slower, and a strategy built from other types is refused | `hot`: publish every packet of the window into a ring, 4 readers read every field. `handshake`: strategies from another build connect |
| B: snapshots and late joins do not encode | `join`: a client joins in the middle of the window and applies the rest |
| C: the state is kept in place in the format it is sent in | `state`: the books and the metrics table apply every update in place |
| D: one set of types through the whole chain | the same `Order` record is the hot-path payload, the book's queue element and the snapshot's content (`include/hft/model.hpp`) |

Not a gate, and not a timing reference: the cloud VM's clock varies by tens
of percent between runs and its page faults cost about 2 us each, so the
instruction counts are the result here and the times are indicative. P99
latencies and the formal times are a later Mac batch.

## Data

Nasdaq TotalView-ITCH 5.0, 2019-01-30, 09:30:00 to 09:40:00
(`https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz`, not
committed). `tools/itch_extract` cuts the window out of the day file, with
one add per order resting when the window opens:

    curl -sSL <day file> | gunzip -c | itch_extract --from 09:30:00 --to 09:40:00 --out L_0930_0940.itch

A feed handler (`include/hft/itch.hpp`) normalizes it once at start-up, which
no case measures (the exchange's big-endian protocol is the one conversion
every format pays alike): 383,789 resting orders, then 15,947,338 updates
(add, fill, cancel, delete, trade; a replace is a delete and an add) of
8,714 instruments in 12,750,021 packets. A packet takes the messages within
1 us of its first one, up to 1,400 bytes of ITCH payload (one MoldUDP64
datagram): 1.25 updates on average, at most 45. The first run caches the
normalized window in a `.feed` file.

## The chain

    ITCH --feed handler--> Update (48 B) --ring--> 4 strategy readers          [hot]
                                    \--> state service: books + metrics table  [state]
                                           \--snapshot--> late-joining client  [join]

- **State** (`include/hft/model.hpp`): the order books of the 64 busiest
  instruments (L3: price levels, each with its queue of `Order` in time
  priority; at the end of the window 57,945 levels, 283,285 orders, the
  longest queue 2,852) and one metrics row per instrument (8,714 rows of 104
  B: best bid and ask, last price, volume, live orders...). Every update
  changes its row and, for a book, its level and queue.
- **Models of the state**, each the natural one for its format:
  - `xo`: `hft::State`, all books and the table in one XBuffer (one XBuffer per
    book is 1.7-2.2x slower from cache-set conflicts between arenas; see
    `/mnt/project-files/hft-demo-c-20261104/`).
  - `std`: the same shape with `std::vector`, what zpp_bits, FlatBuffers and SBE
    encode from and decode into.
  - `cista`: the same shape with cista offset vectors (v0.16).
  - `pool`: the hand-written fixed-capacity pool HFT code commonly uses for
    resident state: per book a sorted array of price levels, an order pool with
    a free list, each queue an intrusive doubly linked list, and an
    open-addressing order id -> slot hash. Its arrays copy as they are; the
    receiver rebuilds the hash (`include/hft/pool.hpp`).
  The models other than the pool share one apply routine
  (`include/hft/apply.hpp`); every model reaches the same digest.

## Files

| File | |
| --- | --- |
| `hft_bench.cpp` | the program: `--case prepare / state / hot / join / handshake` |
| `include/hft/model.hpp` | the shared records (`Order`, `Update`, `Row`) and the XOffset and std state |
| `include/hft/itch.hpp` | the feed handler: ITCH -> `Update`, packets, the `.feed` cache |
| `include/hft/apply.hpp` | applying an update (every model but the pool), digests |
| `include/hft/pool.hpp`, `cista_model.hpp` | the pool and the cista model |
| `include/hft/case_*.hpp` | the cases |
| `schema/hft.fbs`, `schema/hft-sbe.xml` | FlatBuffers and SBE schemas (code generated at build time) |
| `run.sh` | build everything and run every case (`--time`, `--ir`) |
| `tools/itch_extract.cpp`, `tools/summarize.py` | the window cutter; the tables and the digest check |

## Run

    bench/hft/run.sh --feed L_0930_0940.itch --out build/hft --time --ir --reps 3

Needs the P2996 toolchain, cista v0.16, zpp_bits v4.7.6, FlatBuffers
v25.12.19 (headers and `flatc`) and SBE 1.41.0 (`sbe-all` jar, Java); paths
are environment variables (see `run.sh`). The tables below are its
`summary.md`.

RESULTS_PLACEHOLDER
