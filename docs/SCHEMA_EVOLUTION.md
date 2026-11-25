# Schema evolution

A type changes over releases in one of two ways, and the library treats them differently:

- A **compatible change** adds a field in bytes the previous version reserved. The versions keep one
  fingerprint and the same bytes, so programs of every version read and write each other's data with no
  run-time step, in both directions, on every path (whole wires, image deltas, shared deltas, stored
  images). Servers and clients update in any order.
- Any other change starts a **new generation** with a new fingerprint. Programs of different generations
  do not talk: the handshake refuses the peer, and both ends update together. Stored data of an older
  generation is upgraded once, when it is loaded.

Nothing converts messages per peer. The wire format (3.0) is the same for every version: an old version's
wire is simply the wire written by the old program.

| Header | Which end | Contents |
| --- | --- | --- |
| `xoffset_schema_family.hpp` (in `xoffsetdatastructure.hpp`) | Both | `XSpare<N>`, `XOFFSET_SCHEMA_FAMILY` (sections 2 to 5) |
| `xoffset_handshake.hpp` | Both | Handshake messages `XSchemaHello` / `XSchemaHelloAck`, `make_schema_hello`, `accept_schema_hello`, `read_schema_ack`, `peek_wire_fingerprint` |
| `xoffset_evolution.hpp` | Server or offline tool | Loading stored data of an older generation: `XOFFSET_SCHEMA_LINEAGE`, `evolve_load_verified`, `evolve`, hooks, `evolve_plan` (section 6) |
| `xoffset_schema_freeze.hpp` | Server build | Optional: freezing a version at release, checking the lock file in CTest (section 7) |

The first two compile in the C++20 field-list build (`XOFFSET_RECORD`) and the reflection build, with and
without exceptions; the field-name digest and every check are the same under both backends.
`xoffset_evolution.hpp` and `xoffset_schema_freeze.hpp` need the C++26 reflection backend (GCC 16, P2996
Clang); including them under the field-list backend fails to compile.

## 1. The rule

A later version only adds fields, and only inside bytes the previous version reserved with `XSpare<N>`.

| Change from the previous version | Kind |
| --- | --- |
| A new field of a plain type (scalar, enum, array or record of them) inside an `XSpare<N>` member; the rest of the spare bytes stay `XSpare` | Compatible |
| The element type `XSpare<N>` of a container (for example `XVector<XSpare<16>>`) becomes a plain record of N bytes | Compatible: this is how a list is reserved |
| Two later versions that each add other fields to one version (branches) | Each is compatible with that version; the two refuse each other at the handshake |
| A field removed, renamed, moved to another offset, retyped (also `int32` to `uint32`, another enum underlying type, another container kind or element size) | New generation |
| A new field in padding, after the end of the record, or overlapping a field | New generation: the record's size and every offset stay in a compatible change |
| A new field that holds a container (string, vector, set, map), even inside a record | New generation: an older program's compact walk would drop its block |
| Spare bytes that become padding | New generation: they stay a field or `XSpare` |

A compatible change needs one declaration and nothing else (section 3). A new generation needs both ends
updated together on the network, and a lineage for stored data (section 6).

## 2. The first release: reserve and handshake

Two things in the first release decide how cheaply later releases go.

**Reserve bytes.** `XSpare<N>` is N bytes, alignment 1, zero by default. A program never writes anything
but zero into its own spare members and never reads them; every check treats them as opaque bytes, so a
newer peer may keep fields there. It has its own token in the wire signature (`spare[N]`), so `XSpare<8>`
and `std::uint8_t[8]` are different layouts.

Put the spare member last, where it also fills the tail padding of the record, and size it for the fields
you expect: 8 bytes per frequently repeated element record and 16 at the root are a common choice. The
bytes cost in every image and on the wire: in the SLG example (8 bytes per hero, 16 at the root) the image
grows by 6.3%, by 3.0% after zstd level 3: zero bytes compress well. A list a later version will need
is reserved as `XVector<XSpare<16>>` (an empty vector costs its 12-byte head). A type without spare
bytes can only change by a new generation.

**Handshake on every connection.** Each side first says which version of each root type it works in;
only then come ordinary messages. The frame format is up to the application;
`examples/schema_handshake.cpp` is the simplest length-prefixed version.

Client (never throws; builds without exceptions):

```cpp
send(make_schema_hello<game::Player, game::Chat>("client-1.4.2"));
XSchemaAckResult r = read_schema_ack<game::Player, game::Chat>(receive());
if (!r) show_update_prompt(r.message);   // the server works in another generation
```

Server (never throws):

```cpp
std::string ack;
XSchemaHelloResult peer = accept_schema_hello<game::Player, game::Chat>(hello, ack, "server-12");
if (!peer.valid) close();                 // not a handshake message; ack is empty
send(ack);
if (!peer.accepted()) close();            // the ack names the refused root types
// from here on: the same load_verified / save calls as between two programs of one version
```

- For each root type the hello carries the root type id, the fingerprint (`schema_hash`) and the field-name
  digest (`name_digest`, section 8); `make_schema_hello` also lists the earlier versions of the type's
  family as `known` entries. The ack has one verdict per root type: `same` (the peer works in this
  program's version or another version of its family, earlier or later), `reject_unknown` (another
  generation: no version of the family has that fingerprint) or `reject_names` (the fingerprint of the
  family under field names no version has: a rename, or a branch).
- Between servers, the side that connects uses `make_schema_hello<Roots...>(label, XSchemaRole::server)`;
  the verdicts are the same.
- Verdict 1, `convert`, is retired: no program writes it, and `read_schema_ack` reads an ack that has it
  (from 4.2 or earlier) as a rejection.
- Without a handshake, `peek_wire_fingerprint(wire)` gives the root type id and fingerprint a wire
  carries, and `load_verified` refuses a wire of another fingerprint.

## 3. Adding a field

1. At release, keep the released version's types in the code: by hand, or as a snapshot header written by
   the freeze tool (section 7), for example `game_v1::Player`.
2. In the working version, take the new field from spare bytes.
3. Declare the working version the next version of the released one, at global scope, after the field
   lists:

```cpp
using namespace XOffsetDatastructure;
namespace game_v1 {  // as released: 24 and 40 bytes
struct Hero { std::int32_t id{}; XString name; XSpare<8> spare{}; };
struct Player { std::uint64_t uid{}; XVector<Hero> heroes; std::int32_t level{}; XSpare<16> spare{}; };
}
namespace game {     // the new version: star at byte 16 of Hero, vip at byte 24 of Player
struct Hero { std::int32_t id{}; XString name; std::int32_t star{}; XSpare<4> spare{}; };
struct Player { std::uint64_t uid{}; XVector<Hero> heroes; std::int32_t level{}; std::int64_t vip{}; XSpare<8> spare{}; };
}
XOFFSET_RECORD(game_v1::Hero, id, name, spare)
XOFFSET_RECORD(game_v1::Player, uid, heroes, level, spare)
XOFFSET_RECORD(game::Hero, id, name, star, spare)
XOFFSET_RECORD(game::Player, uid, heroes, level, vip, spare)
XOFFSET_REGISTER_SCHEMA_NAME(game_v1::Player, "game.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game::Player, "game.Player")   // the same name: the fingerprint covers it

XOFFSET_SCHEMA_FAMILY(game::Player, game_v1::Player)
```

- A family is a chain: each version names the one before it (`XOFFSET_SCHEMA_FAMILY(v3, v2)`,
  `XOFFSET_SCHEMA_FAMILY(v2, v1)`). The check recurses through nested records and container elements, so
  one declaration on the root covers the tree; the nested records need none for whole wires and image
  deltas.
- Declare it on every type whose numbering travels: the root (its fingerprint) and every record a shared
  delta (XS) sends as an entity or as the root record (its change bodies).
- A version that breaks the rule does not compile. The message names the rule and the field, for example
  `XOFFSET_SCHEMA_FAMILY: a field of the previous version moved to another offset; fields keep their place
  (at stats.def)` (C++26; C++20 and MSVC report each rule in its own `static_assert`).
  `schema_family_successor_v<New, Prev>` asks the same question without failing.
- The versions of one family are one generation: never list two of them in one lineage (section 6).

## 4. What follows from the declaration

- **Fingerprint.** `wire_schema_hash<New>()` is the hash of the family's first version; a type that
  declares no family keeps its own hash and its own code. Image headers, delta key frames and the
  handshake carry it.
- **Handshake.** `accept_schema_hello` answers `same` to a peer on any version of the working type's
  family: an earlier one (its digest is in this program's family) or a later one (its hello lists this
  program's digest as `known`). Two branches of one version and another family under the same layout are
  `reject_names`. The hello and ack formats do not change.
- **Freeze tool.** `check` reports an added field inside a family as such, not as a layout under other
  names.
- **Shared deltas.** A change body names the members of the family's first version: a spare region and
  every field later taken from it are one member, sent as its bytes. Every mask a program passes or gets
  (`changed()`, `put_change`, `XSyncWriter::change`, the publisher's dirty bits) counts the program's own
  members (bit i is member i of its type, in declaration order); `XRecordBody<T>::mask_bits` and
  `mask_all` give their number. Whole bodies need nothing: they are the same bytes.
- **Cost.** None: the same code and the same bytes. The Ir gate measures it (`family.*` rows of
  [PERF_GATE.md](PERF_GATE.md), `bench/evolution/family_ir.cpp`): a send to an older client, an older
  client's receive and a delta tick applied by an older replica cost exactly what they cost between two
  programs of one version.

## 5. What the rule cannot check

1. **A new field must mean "unset" at zero**, and must not have a non-zero default member initializer:
   data written by an earlier version holds zero there, and a later program reads that zero.
2. **An earlier program that replaces a whole record resets the newer fields.** Editing fields of an
   existing record, resending, compacting and relayout keep every byte of the later version; but an
   earlier program that builds a record anew (assigns a default-constructed record, clears and refills a
   container of records) writes its own zeros into the spare bytes, and the later version reads its new
   fields as unset. When an earlier program may do that to data a later one owns, keep that data out of
   its reach or update both together.
3. **Every released version stays in the chain, the first one forever.** The fingerprint is the first
   version's, and the handshake knows earlier versions by their field names: dropping a declaration (or
   the first version's types) changes the fingerprint or refuses the peers still on that version, which
   is a new generation.

## 6. A new generation

**Network.** Programs of different generations do not exchange messages: the handshake answers
`reject_unknown`, the client shows its update prompt, and the connection closes. Update both ends together,
or run the old and the new service side by side until the old clients are gone. A server that must still
serve a peer of the old generation can convert each message itself (`evolve<Old>(buffer)` below, then
`save_compact`); the library keeps no per-peer state for it.

**Stored data.** A program that loads saved images of older generations lists them in a lineage and loads
through it:

```cpp
// Global scope: the older generations whose stored data still loads, oldest first.
XOFFSET_SCHEMA_LINEAGE(game::Player, game_g1::Player, game_g2::Player)

TypedXBuffer<game::Player> p = evolve_load_verified<game::Player>(stored, XBuffer::maximum_capacity(1 << 24));
```

- Each entry is the **last** version of that generation's family (it names every field the generation
  ever had); images of its earlier versions have the same fingerprint and load as it. The working version
  goes last when it is not listed. Every entry registers the same schema name and their fingerprints
  differ pairwise, checked at compile time.
- `evolve_load_verified<W>` verifies an image as its own generation (`load_verified` of that type, with
  the cap), then upgrades it step by step to `W`. An image of `W`'s own generation is a plain
  `load_verified<W>`; an image of no listed generation is refused with `load_verified`'s message.
- **One-way.** The upgraded object is saved as the working generation; nothing writes the old one again.
  Keep a backup of the old image until the upgraded one is saved.

Each step converts field by field; fields correspond by **name**, regardless of offset and declaration
order:

| Change | Default behavior |
| --- | --- |
| Added field | Takes the initial value given by `make<T>()` |
| Removed field | Dropped |
| Reordered, inserted in the middle | Matched by name |
| Renamed | Equals a removal plus an addition; needs a hook |
| Same name, different type (including int32 → int64) | Compile error unless a hook handles it |
| Enum value | Copied if the target enum has that enumerator, otherwise conversion fails |
| Container element type | Converted element by element |
| Set / map keys | Inserted directly for the same type; enum keys checked by value; duplicate keys after conversion fail |
| Container kind, array length, string to/from other types | Compile error unless a hook handles it |
| `XSpare<N>` bytes | Never copied: spare members of the target stay zero, and a field of the target that was spare in the source starts at its initial value |

Fields the default rules cannot handle get a hook, one per pair of adjacent generations:

```cpp
template <> struct XOffsetDatastructure::xevolve<game_g2::Player, game::Player> {
    static constexpr std::string_view handles[] = {"gold", "nick", "display_name"};
    static void upgrade(const game_g2::Player& from, game::Player& to, XEvolveContext& ctx) {
        to.gold = ctx.widen<std::int64_t>(from.gold);
        to.display_name.assign(from.nick.view());
    }
};
```

The default copy runs first, skipping the fields in `handles`, then the hook is called. A name in
`handles` that neither version has fails to compile. Nested records can have their own hooks too; a hook
may also define `downgrade`, which only `evolve<Older>` uses. Commit the output of
`evolve_plan<game_g2::Player, game::Player>()` (for each field: copy, convert, enum (checked), init
(added), drop (removed), hook, spare (not copied)) with the new generation, so reviewers see which values a
step drops.

`evolve<To>(buffer, options)` converts an already verified buffer to another version on its own (offline
migration, a server converting for one old peer). Failures (an enum value that does not exist,
`checked_narrow` out of range, duplicate keys, the target buffer exceeding its limit, a hook calling
`ctx.fail`) throw `XException` with the field path in the message, and no half-built result is produced.
The target buffer's limit defaults to the source's `maximum_capacity` and can be set with
`XEvolveOptions::maximum_capacity`; `XEvolveOptions::with_base(&obj)` hands an object of the target version
to the hooks of the last step (`ctx.base<T>()`).

Cost: one full verification of the old generation, plus one conversion per step (reading the source
object, building in a new buffer); records with the same shape, same field names, no padding and no enums
are copied as a whole. In `bench/evolution` an upgrade costs two to three same-version loads, once per
image.

## 7. The freeze tool and the lock file (optional)

The tool writes the released version as a snapshot header and checks on every build that the working
types still fit what was released. The project links a small program of its own:

```cpp
#include "game_schema.hpp"
#include "xoffset_schema_freeze.hpp"
int main(int argc, char** argv) {
    return XOffsetDatastructure::schema_freeze_main<game::Player, game::Chat>(argc, argv);
}
```

- `schema_freeze freeze <label> <namespace> <header> <lock>`: run at release; it writes the snapshot
  header (with schema name registration and `XOFFSET_PIN_SCHEMA`, so changing a frozen type fails to
  compile) and appends this version to the end of the lock file. Records and enums that did not change
  are not copied: the snapshot writes `using Item = game_v1::Item;`. Every record newly defined in the
  snapshot has an `XOFFSET_RECORD` line, so clients without reflection (the C++20 field-list build) can
  include the snapshot directly and work in that version; C++26 builds check these lines with reflection.
  `tests/test_schema_snapshot.cpp` checks this in every build and verifies that the fingerprint and
  field-name digest computed by the client match the lock file.
- `schema_freeze check <lock>`: put it in CTest so it runs on every build. It fails when a root type has a
  frozen fingerprint under field names of no version of its family (a rename), and when its fingerprint
  changed since the last frozen version while it declares neither a family with that version
  (`XOFFSET_SCHEMA_FAMILY`) nor a new generation after it (`XOFFSET_SCHEMA_LINEAGE`). It reports each root
  as equal to, adding fields to, or a new generation after the last frozen version.

Paths in the lock file and the `#include` lines of snapshots are written as given on the command line: run
the tool from the directory the project's include paths start at. The lock file is text, one line per
item, tab-separated, oldest version first:

```
version <label> <namespace> <header>
root    <schema name> <snapshot type> <schema_hash> <name_digest> <wire signature>
record  <snapshot type> <name> <wire signature> <name signature>
enum    <snapshot type> <name> <definition>
```

A complete example in the tests: `tests/schema_freeze/`; a project using it: `demo/mapstate`
(`src/schema_freeze.cpp`, `schema.lock`).

## 8. Field-name digest

`schema_hash` covers only the layout, not field names. When two versions only swap the names of two fields
of the same type, `schema_hash` is the same, and reading by bytes would read hp as mp. Three things stop
this:

- The handshake compares `name_digest` (`schema_name_digest_v<T>`) with the digests of every version of
  its family, and answers `reject_names` when none matches;
- A lineage requires pairwise different fingerprints (at compile time);
- The lock file check (section 7).

The digest is fnv1a of a name signature that lists, for every record reachable from the root in the order
`wire_abi_signature()` walks it, each member as `name:offset`. The text is part of the handshake
compatibility promise: two programs agree on a version only if they compute the same digest for it.

```
record          {name:offset<field>,name:offset<field>}
array           [element]   (also std::array)
vector, set     <element>
map             <key,value>
scalar, enum, string: empty
```

## 9. Tests

| Test | Contents |
| --- | --- |
| `test_xspare` | `XSpare<N>`: layout, signature token, opaque on every tier, plain bytes in shared deltas |
| `test_schema_family` | Three versions of one type tree and a branch, every pair both ways: whole wires (full check and admit, every emission), an earlier program's edits and resends (compact, relayout), image deltas (byte-identical), stored images, handshake verdicts, the freeze check |
| `test_schema_family_xs` | Shared deltas between three versions: whole and change bodies byte-identical both ways, own-numbered masks, bool checks per version, diff ops, whole key and delta messages on clients of every version (full and admit) |
| `schema_family_<rule>_fail` | One declaration per rule that must not compile, and its message |
| `test_schema_handshake` | Hello / ack encoding and decoding, every verdict (the retired `convert` read as a rejection), malformed input |
| `test_schema_evolution` | Default rules, hooks, enums, containers, capacity limits, `evolve_plan` |
| `test_schema_lineage` | Stored data of every generation (each with an add-only family inside) loaded as the working one, spare bytes never copied, rejections, capacity, handshake verdicts across generations, plans |
| `test_schema_freeze` | The freeze tool's snapshots and lock files are byte-identical to the committed versions; every failure of check and its text |
| `tests/schema_evolution/*_fail.cpp` | Code that must fail to compile, and its error messages |
| `fuzz_schema_family` | libFuzzer, both backends: every input read by three versions of one family; a later version accepts nothing an earlier one rejects, and versions that accept an input hold the same bytes |
| `fuzz_schema_evolution` | libFuzzer: loading arbitrary stored images through the lineage, converting random objects between generations, arbitrary handshake bytes (see `tests/fuzz/regressions/README.md`) |
