# Roadmap

Work that is decided but not scheduled. Nothing on this page is implemented; the current release
requires a 64-bit little-endian target ([BUILD_AND_PLATFORMS.md](BUILD_AND_PLATFORMS.md)).

## Deferred: big-endian hosts

Decided 2026-11-24, not scheduled. No current target needs it: the Linux servers, the Windows,
iOS, Android and macOS clients and current game consoles are all little-endian. Start it only when
a real big-endian target (IBM Z s390x, AIX on POWER) appears.

### Approach: one little-endian image on every host

- Every host keeps the image in little-endian byte order. The wire, deltas
  ([DELTA_SYNC.md](DELTA_SYNC.md)), shared deltas ([SHARED_DELTA.md](SHARED_DELTA.md)),
  `relayout()` and `save_compact` keep their bytes, and big-endian and little-endian peers
  interoperate byte for byte with no conversion step. A big-endian receiver still reads in place.
- Multi-byte scalars in wire records are declared with library aliases (for example `le_i32`,
  `le<float>`; names to be decided). On a little-endian target an alias is the plain type itself,
  so user code and generated code do not change. On a big-endian target it is a wrapper that
  byte-swaps on every read and write, and a plain multi-byte scalar in a wire record does not
  compile there.
- The library's own image fields (container heads, the header, the free directory, delta and
  shared-delta frames) use the same aliases or `load_le` / `store_le` helpers. The little-endian
  branch of every changed site keeps its current source text.
- The schema signature treats `le<T>` as `T` in both schema backends (C++26 reflection and
  `XOFFSET_RECORD` field lists), so the schema hash stays the same on both byte orders. It does not
  depend on byte order today.

Rejected alternatives:

- **Big-endian native images, exchanged only between big-endian peers.** It needs a second image
  encoding: the inline-string flag (bit 31 of `capacity_`) lies in head byte 8 on a big-endian
  host and overlaps the characters. It serves no target.
- **Byte-swapping the whole image on send and receive.** Big-endian peers would pay a typed walk
  per message, close to serialization, and every delta would cost a whole-image pass.

### Cost

| Item | Estimate |
| --- | --- |
| Little-endian hot paths | None by construction. Every PR shows Ir 1.000 on both Ir gates and the same code on the `asm` step ([PERF_GATE.md](PERF_GATE.md)) |
| Big-endian hosts | Integer fields: no extra instructions on s390x (`lrv` / `strv`). A float read-modify-write: 3 to 6 instructions |
| Sites that build words from bytes in little-endian order | About 50: inline-string packing, head writes, delta byte masks and touch ranges, the free directory's end marker. Each needs a big-endian branch |
| References to container head fields to review | About 700 |
| API | Big-endian builds spell multi-byte scalar fields with the aliases |
| Size | 4 to 6 PRs |

### Testing

A big-endian step in `tools/local_validate.sh`: the C++20 field-list build cross-compiled for
s390x (`g++-14-s390x-linux-gnu`) and the full CTest under `qemu-s390x`. On a 4-core container the
build takes 8 min 23 s; an aarch64 control under the same QEMU flow passes all 215 tests in
2 min 14 s. The C++26 reflection backend needs a GCC 16 s390x cross compiler. Big-endian timings
need real s390x or POWER hardware; QEMU timings are not meaningful.

### Baseline (main 9b3edc0, 2026-11-24)

With the platform check bypassed, 169 of 215 tests fail on s390x (GCC 14.2, qemu-user 8.2): images
fail their own host's magic check (the magic is written as an integer and checked as bytes),
inline strings read back wrong, and the delta byte masks are mirrored within each word. A wire of
the other byte order is rejected at the header: a little-endian wire on a big-endian host gives
`unsupported header size 0x28000000`, a big-endian wire on a little-endian host `bad magic`.
