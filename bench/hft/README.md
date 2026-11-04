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

## 2026-11-04, cloud x86_64 (4 vCPU Xeon 2.8 GHz), P2996 d77eff1 -O2 libc++, valgrind 3.22

Whole window; times are medians of 3 runs pinned to one CPU. Instruction
counts with the glibc setting of `tools/perf_gate`.

### Digests

- hot: equal (a3a5dbca36fbe79c ['fixed', 'raw', 'sbe', 'xo'])
- join: equal (0302b7ed8d030a96 ['cista', 'cista_checked', 'fb', 'fb_verified', 'pool', 'sbe', 'xo', 'xo_verified', 'zpp'])
- state: equal (0302b7ed8d030a96 ['cista', 'pool', 'std', 'xo'])
- join clients end with the state service's digest: yes

### State service (claim C): apply every update of the window in place

Ir per update (callgrind); ns per update (median of 3 timed runs, noisy on a cloud VM).

| model | Ir/update | Ir vs pool | ns/update | snapshot bytes | levels | orders | longest queue |
|---|---:|---:|---:|---:|---:|---:|---:|
| pool | 263.0 | 1.00x | 65.5 | 14352112 | 57945 | 283285 | 2852 |
| std | 319.3 | 1.21x | 44.7 |  | 57945 | 283285 | 2852 |
| xo | 356.3 | 1.36x | 51.0 | 13559928 | 57945 | 283285 | 2852 |
| cista | 636.0 | 2.42x | 80.6 | 12756888 | 57945 | 283285 | 2852 |

### Hot path (claim A): publish each packet into a ring slot, 4 readers read every field

| format | bytes/packet | encode Ir/packet | read Ir/packet/reader | encode ns/packet | read ns/packet/reader |
|---|---:|---:|---:|---:|---:|
| raw | 68.0 | 48.3 | 74.8 | 17.9 | 13.4 |
| fixed | 68.0 | 58.3 | 80.8 | 16.9 | 14.2 |
| sbe | 68.8 | 141.2 | 73.1 | 33.5 | 15.0 |
| xo | 124.0 | 742.7 | 459.4 | 99.9 | 50.9 |

### Late join (claim B): snapshot at the middle of the window, client applies the rest

| format | bytes | snapshot Ir | ready Ir | rest Ir/update | snapshot ms | ready ms | rest ns/update |
|---|---:|---:|---:|---:|---:|---:|---:|
| xo | 20855336 | 1,799,853 | 1,806,844 | 164.4 | 14.01 | 13.52 | 34.6 |
| xo_verified | 13460960 | 7,098,104 | 4,374,091 | 164.6 | 9.63 | 7.76 | 37.9 |
| pool | 14237328 | 3,759,520 | 26,765,866 | 115.5 | 19.42 | 27.02 | 51.4 |
| cista | 12664248 | 79,007,712 | 1,399,804 | 279.9 | 27.96 | 6.90 | 47.0 |
| cista_checked | 12664248 | 79,007,712 | 301,538,322 | 279.9 | 28.75 | 27.93 | 44.5 |
| zpp | 11511388 | 9,514,982 | 22,493,137 | 146.8 | 22.08 | 8.39 | 38.7 |
| fb | 12432224 | 41,424,688 | 19,931,930 | 146.8 | 22.31 | 10.45 | 37.0 |
| fb_verified | 12432224 | 41,424,688 | 30,332,072 | 146.8 | 21.47 | 11.19 | 36.7 |
| sbe | 10437974 | 14,319,145 | 27,819,822 | 146.8 | 10.22 | 8.92 | 32.2 |

### Handshake (claim A): a strategy from another build connects

The feed sends order id 1001.

| strategy | verdict | same batch tag | view_fixed_batch | a raw cast reads order id | hello bytes |
|---|---|---|---|---:|---:|
| same | same | True | ok | 1001 | 120 |
| swapped | reject_names | True | ok | 34200000000123 | 128 |
| widened | reject_unknown | False | wrong_tag | 1001 | 128 |


### Reading the results

**C, the state service.** XOffset keeps the books and the table in its sending
format at 1.36x the pool's instructions and 1.12x the std model's; in time it is
between them (51.0 ns per update against 44.7 for std and 65.5 for the pool,
whose order-id hash misses the cache). The suggested gate (at most 1.5x the
pool) holds in both. Most of every model's cost is common: finding the price
level by scanning from the best price and the order in its queue. cista pays
2.4x for inserting into its vectors (a rotate of offset-pointer elements).

**B, the late join.** In instructions XOffset needs no encoding: on the trusted
link 1.8 M to take the snapshot and 1.8 M to be ready, against 9.5 M + 22.5 M
for zpp_bits, 14 M + 28 M for SBE and 41 M + 20 M for FlatBuffers (decode into
a writable state included); verified, 7.1 M + 4.4 M. The pool takes 3.8 M but
needs 26.8 M to rebuild its order index; cista needs 79 M to serialize, and
302 M when it checks what it receives. In time on this VM it is not an order of
magnitude: XOffset verified, cista, zpp_bits, FlatBuffers and SBE all get ready
in 7-11 ms, because the receiver's first touch of fresh memory (about 2 us per
4 KiB page) costs more than the work, so the bytes touched decide. The trusted
XOffset path sends the raw segment, 20.9 MB with capacity slack and holes,
against 13.5 MB for the compact verified wire, and takes 13.5 ms to be ready,
almost twice the verified path's 7.8 ms. A trusted load of the compact wire into
memory the client has already touched (as HFT processes pre-fault) would keep
the instruction advantage in time; that needs library work. After joining, the
client applies updates at the same cost as the server (164 Ir per update in the
second half, std 147).

**A, the hot path.** Real packets are small (1.25 updates), so per-packet
constants count: the fixed message reads with 6 more instructions per packet
than a raw struct with a length check (+8%) and writes with 10 more (+21%),
for its tag and size checks; in time +0.8 ns per read here (+1.9 ns in an
earlier run: noise of the same size). SBE without
bounds checks reads like the raw struct and writes at 2.9x. The per-packet
XBuffer (`xo`, before fixed messages) costs 6-15x. Whether the fixed message
stays within 5% of the raw struct at P99 needs the Mac.

**A, the handshake.** A strategy whose `Order` declares `ts` before `order_id`
has the same layout, so the same wire fingerprint and batch tag: the fixed
batch view accepts its packets and a raw cast reads the timestamp
34200000000123 as the order id. The handshake rejects it before the first
packet (`reject_names`). A changed layout is caught both by the batch tag and
by the handshake (`reject_unknown`). The hello is 120-128 bytes once per
connection.

**D, one set of types.** `model.hpp` declares the records once; the hot path
sends `Update` as it is, the book stores its `Order` as it arrived and the
snapshot is the state's bytes, with no schema file and no conversion code.
The others need: FlatBuffers and SBE a schema file each (52 and 82 lines),
generated code and hand-written conversion both ways (about 80 and 95
lines); cista a parallel model on its own containers; the pool its own
structures, snapshot and restore code (`pool.hpp`, 260 lines) and no check of
what it receives.
