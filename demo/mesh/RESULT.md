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


## 2. Cloud mesh: hub and devices (network thread)

Run `cloud-full1`, 10-28 16:08Z: `demo/mesh/tools/run_mesh.sh` on one cloud
host, built from 00be24c (types code 23db369). One hub (g++-13, C++20 field
lists) and 16 devices, every column with exceptions and without
(`xo_peer_noexc`), all on loopback through the hub (pure relay): Linux
x86-64 g++-13 and clang++-18, Linux arm64 g++-13 static under
qemu-aarch64, Windows MinGW-w64 g++ 13, MSVC 19.44 and clang-cl 19 under
wine, Android NDK r29 arm64 under qemu-aarch64 and x86-64 natively (both
static, android-28). Every device took evil jobs.

**Result: PASS.** 7,452 jobs, 7,452 passed, in 84 s: 5,760 pair jobs (every
ordered pair of the 16 devices x 6 types x small, medium x save_verified,
save_compact), 12 chains (each type and scale once through all 16 devices
and back, 17 stops), 1,680 evil jobs (every ordered pair x 7 cases). For
every normal job the hub replayed the job itself and every device's
received content digest, sent wire digest and sent length matched the
replay, so each device sent byte-identical data. All 16 devices exited 0
after Bye; all 16 self-tests passed (59 checks; the corpus check is in
section 1).

### Devices

Median per-hop times in microseconds over the device's ok hops. Columns
under wine run at near native speed; qemu columns are emulated (Android
arm64's model check is the slowest step there), so their times are not
device timings.

| id | device | jobs passed | hops ok | exit | median us verify / check / mutate / save |
| --- | --- | --- | --- | --- | --- |
| 1 | linux-x64-gcc | 942/942 | 1197/1302 | 0 | 6.1 / 38.6 / 13.3 / 3.8 |
| 2 | linux-x64-gcc-noexc | 942/942 | 1198/1303 | 0 | 6.2 / 38.6 / 13.0 / 3.9 |
| 3 | linux-x64-clang | 942/942 | 1199/1304 | 0 | 5.4 / 38.2 / 11.6 / 3.8 |
| 4 | linux-x64-clang-noexc | 942/942 | 1198/1303 | 0 | 5.9 / 39.1 / 11.6 / 3.3 |
| 5 | android-x64 | 942/942 | 1197/1302 | 0 | 7.3 / 46.1 / 13.2 / 3.9 |
| 6 | android-x64-noexc | 942/942 | 1198/1303 | 0 | 7.2 / 43.9 / 13.7 / 3.5 |
| 7 | linux-arm64-gcc | 942/942 | 1197/1302 | 0 | 52.4 / 736.3 / 152.7 / 37.4 |
| 8 | linux-arm64-gcc-noexc | 942/942 | 1198/1303 | 0 | 58.7 / 726.6 / 157.3 / 38.9 |
| 9 | android-arm64 | 942/942 | 1198/1303 | 0 | 78.2 / 5076.3 / 307.7 / 47.4 |
| 10 | windows-mingw | 942/942 | 1198/1303 | 0 | 7.3 / 50.4 / 17.5 / 4.3 |
| 11 | android-arm64-noexc | 942/942 | 1197/1302 | 0 | 85.3 / 5108.9 / 330.7 / 47.3 |
| 12 | windows-mingw-noexc | 942/942 | 1198/1303 | 0 | 7.2 / 49.6 / 17.0 / 4.3 |
| 13 | windows-clang-cl-noexc | 942/942 | 1198/1303 | 0 | 5.9 / 42.1 / 12.3 / 3.8 |
| 14 | windows-msvc | 942/942 | 1198/1303 | 0 | 7.3 / 45.2 / 14.3 / 3.9 |
| 15 | windows-clang-cl | 942/942 | 1198/1303 | 0 | 5.1 / 41.4 / 12.5 / 3.7 |
| 16 | windows-msvc-noexc | 942/942 | 1197/1302 | 0 | 9.4 / 44.6 / 15.0 / 3.8 |

### Pairs: sender (row) to receiver (column), passed/jobs

Sender makes state 0, receiver loads, checks, changes and sends it back,
sender checks again. 24 jobs per ordered pair: 6 types x 2 scales x 2 save
modes.

| sender \ receiver | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 linux-x64-gcc | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 2 linux-x64-gcc-noexc | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 3 linux-x64-clang | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 4 linux-x64-clang-noexc | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 5 android-x64 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 6 android-x64-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 7 linux-arm64-gcc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 8 linux-arm64-gcc-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 9 android-arm64 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 10 windows-mingw | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 11 android-arm64-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 |
| 12 windows-mingw-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 | 24/24 |
| 13 windows-clang-cl-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 | 24/24 |
| 14 windows-msvc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 | 24/24 |
| 15 windows-clang-cl | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - | 24/24 |
| 16 windows-msvc-noexc | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | 24/24 | - |

### Pairs by type, scale and save mode

| type | small verified | small compact | medium verified | medium compact |
| --- | --- | --- | --- | --- |
| ChatRoom | 240/240 | 240/240 | 240/240 | 240/240 |
| Document | 240/240 | 240/240 | 240/240 | 240/240 |
| KvConfig | 240/240 | 240/240 | 240/240 | 240/240 |
| Order | 240/240 | 240/240 | 240/240 | 240/240 |
| SceneTree | 240/240 | 240/240 | 240/240 | 240/240 |
| SensorSeries | 240/240 | 240/240 | 240/240 | 240/240 |

### Chains

| job | type | scale | save mode | stops | result | ms |
| --- | --- | --- | --- | --- | --- | --- |
| 5761 | Document | small | verified | 17 | pass | 47 |
| 5763 | Order | small | verified | 17 | pass | 44 |
| 5762 | Document | medium | compact | 17 | pass | 454 |
| 5764 | Order | medium | compact | 17 | pass | 457 |
| 5765 | SensorSeries | small | verified | 17 | pass | 74 |
| 5767 | KvConfig | small | verified | 17 | pass | 253 |
| 5766 | SensorSeries | medium | compact | 17 | pass | 653 |
| 5769 | SceneTree | small | verified | 17 | pass | 232 |
| 5768 | KvConfig | medium | compact | 17 | pass | 642 |
| 5771 | ChatRoom | small | verified | 17 | pass | 229 |
| 5770 | SceneTree | medium | compact | 17 | pass | 468 |
| 5772 | ChatRoom | medium | compact | 17 | pass | 445 |

### Evil cases

Route[0] sends a bad data part; the first receiver must refuse it with
`expected_verdict` and send nothing on.

| case | refused as expected | receivers' verdicts |
| --- | --- | --- |
| bad_offset | 240/240 | load_rejected |
| model_drift | 240/240 | model_mismatch |
| schema_hash | 240/240 | load_rejected |
| stale_digest | 240/240 | content_digest |
| too_large | 240/240 | too_large |
| truncated | 240/240 | load_rejected |
| wrong_type | 240/240 | load_rejected |

Files: `/mnt/project-files/cross-device-demo-20261028/mesh-full1/`
(`MESH_RESULT.md`; `mesh-full1.tgz` sha256 `acb5eee06c0f0395581f225b35ccf2027d736606b09e3bf7efd7f18283e58f9a`
with the hub's CSV files, summary, logs, every device's log and self-test
output; the 424 MB hop archive is not kept).

## 3. Cross-site mailbox: cloud dry run (network thread)

The Mac run (phases 3 and 4, `tools/run_mesh_mac.sh`) is still to come;
this is the whole flow in the cloud with the Mac's eight devices played by
the linux-x64-gcc programs (`run_mailbox.sh fake-mac`), 10-28 16:4xZ.

`xo_hub --mailbox-plan` for the 16 cloud devices (ids 1-16) and the 8 Mac
devices of `tools/mac_devices.txt` (ids 101-108): 2,054 jobs. 1,536 `cross`
jobs, for every (cloud, Mac) pair and every type both [cloud, Mac, another
cloud] and [Mac, cloud, another cloud], one (scale, save mode) per pair in
rotation (each combination 64 times); 6 chains, one per type, [cloud, all 8
Mac devices shuffled, another cloud] (10 stops); 512 evil jobs, two cases
per pair in each direction (every receiver meets every case).

Flow: the cloud devices answer the 1,030 Job files of jobs that start in
the cloud (`pre`, 1 round); the 2,054 files for the Mac (28.9 MB,
`mac_inbox.tar.gz` 7.8 MB) are what the Mac gets; the Mac side answers in 7
rounds (the chains need one round per Mac stop) and returns 3,944 files
(`outbox.tar.gz` 15.8 MB); the cloud devices finish in 3 rounds (`post`) and
`xo_hub --mailbox-score` judges every job from the reports and checks every
hop file's data part against the replay (length and wire digest).

**Result: PASS.** 2,054 jobs, 2,054 passed (cross 1,536, chain 6, evil
512; each evil case refused as expected 312-314 times). Merged with the
cloud mesh of section 2 (`mesh_report.py --also`): 24 devices, 9,506 jobs,
all passed; each cloud-Mac cell of the matrix has 6/6 jobs in each
direction. A changed byte in a hop file's data part after the receiver
reported fails the job (`data part ... wire ...; replay ...`), and a
missing report fails it as `no report from <device>`.
