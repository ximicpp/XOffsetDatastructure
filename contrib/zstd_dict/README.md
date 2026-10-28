# xoffset_zstd_dict: zstd dictionaries for small messages

Optional component, **not part of the XOffset library**. The library stays
header-only with no dependencies; this directory is a standalone C++17
project that links zstd 1.5.x. Nothing in it throws, so it builds with
`-fno-exceptions` and MSVC `/EHs-c-`.

## When to use it

`docs/DEPLOYMENT.md` section 6.1 has the full choice. In short:

| Message | Use |
| --- | --- |
| Large snapshots (KB and up) | plain zstd level 3, no dictionary |
| Small messages (tens to hundreds of bytes) | a dictionary (this component); batch several messages into one frame when they go out together |
| CPU-bound clients | lz4 (not measured here) |

Plain zstd barely helps a 200-byte message: it has no history to match
against. A dictionary trained on the server's own messages supplies that
history. Measured on the SLG battle report (D class: XOffset wire 197 B,
FlatBuffers 158 B), same 16 KB dictionary size and framing for every format,
level 3, cloud x86-64:

| Bytes per message | Single | Batch of 8 | Batch of 32 | Batch of 128 |
| --- | ---: | ---: | ---: | ---: |
| XOffset `save_compact` (`XCompactOptions{0}`), bound dictionary | 56.2 | 40.4 | 37.2 | 34.5 |
| FlatBuffers | 56.3 | 41.9 | 38.1 | 35.3 |
| protobuf | 54.4 | 38.1 | 35.6 | 34.4 |
| XOffset ÷ FlatBuffers | 1.00 | 0.96 | 0.98 | 0.98 |

Without a dictionary: XOffset 135.9 B, FlatBuffers 114.0 B. Encoding takes
about 1.2 to 1.5 us per message and decoding 0.5 to 0.7 us.

## Pieces

`xoffset_zstd_dict.hpp` documents every byte. The short version:

- **Dictionary file (`.xozd`)**: a 48-byte header (magic `XOZD`, format 1,
  dictionary id, length, a 16-byte schema binding, FNV-1a 64 checksum), then
  a zstd dictionary whose own id equals the header's. `Dictionary::parse`
  checks every field.
- **Dictionary id = version.** Both ends carry it. Every frame names the id
  in its header and `Decoder` refuses another id before decoding anything
  (`dict_id_mismatch`). A protocol that negotiates should also compare ids
  up front (the demo's Subscribe does). A new dictionary gets a new id.
- **Binding.** `--binding auto` binds a dictionary to the schema fingerprint
  in its sample wires' header (bytes 24..39: schema hash and root type id).
  The encoder then refuses wires of any other schema, and leaves out the 24
  header bytes the binding fixes; the decoder puts them back. A schema change
  means a new dictionary.
- **Lean frame.** One zstd frame in the magicless format with the dictionary
  id and the content size and no checksum: 3 header bytes for a message under
  256 bytes. The decoder refuses a missing id or size, a declared size over
  the caller's cap (before allocating), and anything but exactly one frame.
  Integrity is left to the transport and to `load_verified`, which still runs
  on every decoded wire.
- **Batch.** Several messages in one frame: varint count, then varint length
  and message, each. `decode_batch` takes a total cap, a count cap and a
  per-message cap.

## Build

```bash
cmake -S contrib/zstd_dict -B build/zstd_dict -DXOZD_ZSTD_SOURCE_DIR=<unpacked zstd-1.5.7>
cmake --build build/zstd_dict
ctest --test-dir build/zstd_dict
```

Without `XOZD_ZSTD_SOURCE_DIR` an installed zstd is used. Targets:
`xoffset_zstd_dict` (encode/decode; receivers link only this),
`xoffset_zstd_dict_train` (adds zstd's dictBuilder), and the
`xoffset_zstd_dict` command line tool.

## Tool

```bash
# Train a 16 KB dictionary, id 1, bound to the samples' schema
xoffset_zstd_dict train --id 1 --size 16384 --binding auto --out events_v1.xozd records:events.bin
# Show a dictionary file
xoffset_zstd_dict info events_v1.xozd
# Bytes per message: raw, zstd, dictionary, batches; --time adds encode/decode times
xoffset_zstd_dict measure --dict events_v1.xozd --batch 8,32,128 --time records:held_out.bin
```

`INPUT` is a file (one message), a directory (every file one message) or
`records:FILE` (repeated little-endian u64 length + message). Train on a
few thousand real messages and measure on a different run. Training is
deterministic: the same samples give the same dictionary.

## Example

`demo/mapstate` uses it for its event stream: A trains on its own UnitEvent
wires (`mapdemo_a --events-dump DIR`), the dictionary is compiled into A and
every client (`demo/mapstate/dict/unit_event_v1.xozd`), the client names its
dictionary id when it subscribes, and A refuses a mismatch. See that
directory's INTERFACES.md.
