# Device mesh demo: results

## 1. Types and self-test (types thread)

Commit 23db369c on `claude/xmesh-demo-20261028` (Linux columns built from
338c483d; the only difference is a local variable renamed for the Windows
headers). Every column builds `xmesh_selftest` and `xmesh_selftest_noexc`
(no exceptions: `-fno-exceptions`, MSVC `/EHs-c-`) and runs both with
`--corpus-check demo/mesh/corpus`: 60 checks each, among them the section 3
fingerprints, six-stop routes equal to the reference model and to `replay()`
in both save modes, 64-stop routes within the limits, jobs through
`start_job` / `handle_hop` / `read_report`, all seven EvilCases giving
`expected_verdict`, and the corpus written on Linux x86-64 (g++-13) checked
byte for byte.

| Column | Compiler, schema backend | Runner | selftest | selftest_noexc | schema.lock |
| --- | --- | --- | --- | --- | --- |
| Linux x86-64 | g++-13, C++20 field lists | native | 60/60 | 60/60 | matches |
| Linux x86-64 | clang++-18, C++20 field lists | native | 60/60 | 60/60 | matches |
| Linux x86-64 | GCC 16, C++20 field lists | docker gcc:16 | 60/60 | 60/60 | matches |
| Linux x86-64 | GCC 16, **C++26 reflection** | docker gcc:16 | 60/60 | 60/60 | matches |
| Linux x86-64 | P2996 clang 21 + libc++, **C++26 reflection** | native | 60/60 | 60/60 | matches |
| Linux arm64 | aarch64-linux-gnu-g++-13, C++20, static | qemu-aarch64 | 60/60 | 60/60 | (device build) |
| Windows x64 | MSVC 19.44, C++20 | wine | 60/60 | 60/60 | (device build) |
| Windows x64 | clang-cl 19 + MSVC STL, C++20 | wine | 60/60 | 60/60 | (device build) |
| Windows x64 | MinGW-w64 g++ 13 (posix), C++20 | wine | 60/60 | 60/60 | (device build) |
| Android arm64 | NDK r29 clang, C++20, static libc++ | qemu-aarch64 | 60/60 | 60/60 | (device build) |
| (sanitizers) | g++-13 Debug, ASan + UBSan | native | 48/48 (small, medium) | | |

Device builds (`XMESH_PEER_ONLY=ON`) do not build `xmesh_schema_freeze`; the
self-test compares the same fingerprints with the frozen table on every
column. Both C++26 reflection builds give the section 3 fingerprints.

### State 0 sizes (bytes)

Data part of state 0 as route[0] sends it (`save_verified` / `save_compact`
with the default slack), and the payload in use; from the GCC 16 C++26 run
(every column computes the same bytes).

| Type | Scale | verified | compact | payload |
| --- | --- | ---: | ---: | ---: |
| Document | small | 2,216 | 2,096 | 2,216 |
| Document | medium | 61,472 | 60,992 | 61,440 |
| Document | large | 971,232 | 971,256 | 984,720 |
| Order | small | 1,000 | 880 | 1,008 |
| Order | medium | 63,536 | 67,528 | 63,544 |
| Order | large | 1,027,160 | 1,092,152 | 1,027,168 |
| SensorSeries | small | 2,672 | 2,944 | 2,680 |
| SensorSeries | medium | 65,200 | 73,288 | 65,208 |
| SensorSeries | large | 4,176,768 | 4,698,816 | 4,176,776 |
| KvConfig | small | 1,688 | 1,624 | 1,680 |
| KvConfig | medium | 77,176 | 51,624 | 76,800 |
| KvConfig | large | 1,191,872 | 828,840 | 1,224,440 |
| SceneTree | small | 1,864 | 1,896 | 1,864 |
| SceneTree | medium | 58,984 | 60,728 | 58,992 |
| SceneTree | large | 974,744 | 1,007,464 | 974,752 |
| ChatRoom | small | 2,768 | 2,792 | 2,776 |
| ChatRoom | medium | 63,888 | 62,760 | 63,896 |
| ChatRoom | large | 1,073,040 | 1,030,696 | 1,073,048 |

Compact is larger where vectors of 8 or more elements get their default
slack (size / 8 + 1 spare elements, so the receiver's first insert does
not relocate), and smaller where building left holes: KvConfig's map grows
by doubling while state 0 is built, and the verified wire carries the
freed blocks.

### Limits (objects.cpp)

| Scale | data_limit | receive_cap | headroom per mutation |
| --- | ---: | ---: | ---: |
| small | 64 KiB | 192 KiB | 64 KiB |
| medium | 512 KiB | 1.25 MiB | 256 KiB |
| large | 4 MiB (SensorSeries 8 MiB) | 8 MiB (SensorSeries 12 MiB) | 2 MiB |

Set from `xmesh_selftest --limits` over 64-stop routes in both save modes
with a margin of 2x or more. Largest values measured: wire 1.56 MB (Order
large), 4.70 MB (SensorSeries large); one mutation's growth 1.04 MB (Order
large: its 13,000-line vector doubles). `mutate` refuses to start with less
than the headroom left under the object's capacity, so a device built
without exceptions never reaches the fatal arena-exhausted path; the
self-test checks the refusal.
