# Canonical and trace V1 (P0 frozen, 2026-10-17)

This is experiment input/oracle syntax, not any candidate library's wire format.
All integers are little-endian. `u8/u32/u64/i64` occupy 1/4/8/8 bytes; i64 is two's
complement. `blob` is u64 byte length followed by exactly those bytes. `text` is a
blob containing strict UTF-8 without NUL, no implicit terminator. Lengths count
bytes, not codepoints. Collections use u64 element count. No native padding.

## Canonical

`SCCAN001` (8 ASCII bytes), schema_version:u32 (must be 1), epoch:u64,
record_count:u64, then each Record. A Record is id:u64, revision:u32, score:i64,
name:text, samples_count:u64, samples:u32[], payload:blob, attrs_count:u64,
then (key:text,value:blob) pairs. Minimum Record length is **52**, minimum Attr
length 16. IDs are positive and strictly increasing. Attr keys strictly increase
under unsigned UTF-8 byte lexicographic order; duplicates are forbidden. Empty
text/bytes/collections are allowed. Full digest is SHA-256 over the entire encoding.

## Deterministic fixtures

Seed is UTF-8 `xoffset-competitor-v1`. Each SHA counter block hashes the concatenation
of five components, **each** preceded by its u64 byte length:
`domain UTF-8, seed UTF-8, id:u64-as-8-bytes, field UTF-8, counter:u64-as-8-bytes`.
Counter begins at zero. Concatenate blocks then truncate to the requested bytes.
Domain strings: `initial-v1`, `append-v1`, `mutation-v1`, `permutation-v1`.

Initial epoch=1, IDs=1..N. For each record, draws under initial/append domain are:

| Field | Draw / conversion |
| --- | --- |
| revision | first 4 bytes of `revision`, LE u32 modulo 1024 |
| score | first 8 bytes of `score`, LE u64 modulo 2000001, minus 1000000 |
| name length | first 8 bytes of `name-length`, LE u64 modulo 57, plus 8 |
| name | requested length of `name`, each byte mapped to ASCII 97+(byte modulo 26) |
| samples | 128 bytes of `samples` interpreted as 32 LE u32 |
| payload | 128 bytes of `payload` |
| attrs | keys k0000..k0003; each value 16 bytes of field `attr/<key>` |

Permutation P sorts IDs by their 32-byte `permutation-v1`/`rank` digest, ties by ID.
S/M/L N=128/16384/131072. Small N=0/1/17/257 use floor(N/4), floor(N/8);
zero counts produce empty stages, but stage 9 always sets epoch to epoch+1.

## Trace

`SCTRAC01`, model_version:u32=1, input canonical SHA:32 raw bytes,
query_count:u64, queries, mutation_count:u64, mutations, stage_count:u64=9,
stage records, before-results, after-results. No embedded JSON or PRNG state.

Query: sequence:u64, code:u8, id:u64, key:text. Sequence is zero-based contiguous.
Codes: 1 record, 2 attr, 3 scan. Non-attr key is empty; scan id is zero. Fixed
query order is P IDs followed by appended IDs N+1..N+floor(N/8); for each ID,
emit record, attr k0001, attr k9999; then record misses 0 and N+floor(N/8)+1;
finally scan. Thus query_count=3*(N+floor(N/8))+3, unchanged before/after mutation.

Mutation header: sequence:u64, stage:u32, code:u8, id:u64, expected_status:u8.
Stage==code, nondecreasing, sequence zero-based contiguous. Status 0=ok,
1=not_found. Only set_epoch uses id=0. Encoded payloads by code:

| Code/stage | Target and encoded payload |
| --- | --- |
| 1 update_score | P first quarter; score:i64=old+1000001, revision:u32=old+1 |
| 2 replace_name | P second quarter; text=128 letters under mutation/name |
| 3 append_samples | P third quarter; count:u64 +32 u32 under mutation/samples |
| 4 replace_payload | P fourth quarter; blob=384 bytes under mutation/payload |
| 5 replace_attr | P first quarter; key:text=k0001, value:blob=32 bytes under mutation/attr/k0001 |
| 6 insert_attr | P first quarter; key:text=k0004, value:blob=32 bytes under mutation/attr/k0004 |
| 7 erase_record | P last floor(N/8); no payload |
| 8 append_record | IDs N+1..N+floor(N/8); complete Record under append-v1, must match header id |
| 9 set_epoch | epoch:u64=old+1 |

Every generated mutation expects ok. Negative tests may specify not_found, but
duplicate insert/append is an invalid operation, not an implicit overwrite.
Reference applies individual operations; native adapters may batch one complete
stage while preserving ordered statuses, state result and real work. No later-stage
values may be prepared into a candidate native state for free.

Stage record: stage:u32=1..9, mutation_count:u64 **for that stage**, complete
canonical digest:32 bytes after that stage. Counts are not cumulative.

## Query results

Results list is count:u64 followed by sequence:u64, status:u8, eight u64 slots.
not_found must have eight zero slots. All sums below are modulo 2^64; score maps
its mathematical signed value modulo 2^64. Text lengths/sums operate on UTF-8 bytes.

- record: revision, score, name length, name byte sum, sample count, sample sum,
  payload length, payload byte sum.
- attr: value length, value byte sum, first byte or 0, last byte or 0, 0,0,0,0.
- scan: record count, ID sum, revision sum, score sum, name byte-length sum,
  sample-value sum, payload byte-length sum, attr count sum.

Standalone query results: `SCQUERY1`, model_version:u32=1, results list.
Query pair: `SCQPAIR1`, model_version:u32=1, blob(standalone before),
blob(standalone after). Hash the complete respective encoding, including magic.
Semantic hashes do not replace wire hashes or authentication.

## APIs, limits, evidence

`reference.py` is standalone stdlib Python 3.10+. `generate_fixture(new_out,n,seed)`
writes input.canonical.bin, trace.bin, final.canonical.bin, query-before.bin,
query-after.bin, fixture.json; returns the manifest. Existing output is rejected.
Manifest schema `sc-fixture-v1` pins every binary's byte count/SHA, all stage hashes,
query/mutation counts, seed and model/trace versions. This is reference evidence,
not candidate functionality or performance. Fixture generation may occur in setup;
candidate native build/query/mutation cannot be hidden in this setup.

C++ `model.hpp` uses namespace sc and standard containers for DTO/oracle only.
Native adapters query/mutate their own representation; normal cereal STD state is
explicitly its native representation. Header functions encode/decode canonical,
trace, query results/pairs; query_reference; apply_reference; validate_snapshot.
`sha256.hpp` supplies Digest256, sha256, sha256_hex and hex_digest. No production
header or root dependency is changed.

Default parsing caps: total input 512 MiB; records 1048576; sequence 16777216;
text 1 MiB; blob 64 MiB; attrs/record 4096; queries 4194304; mutations 2097152;
aggregate collection elements 67108864. Decoder checks count against remaining
bytes and per/aggregate limits **before reserve/allocation**, rejects malformed
UTF-8, ordering, code/version, missing/trailing bytes and impossible result statuses.
These are experiment parser bounds, not a claim about all candidate U profiles.

Committed small goldens 0/1/17/257/S are suitable for cross-language oracle tests;
M/L generation is supported at the same field/count definition, with output in a
new prepared build directory. They are not silently resized to equal wire bytes.
