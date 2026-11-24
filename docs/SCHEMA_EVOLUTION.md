# Schema evolution (since 3.2.0)

Programs on old and new versions interoperate: the server ships the new version first, and old clients
still in use keep sending and receiving as before. All conversion happens on the server; the client only
adds a handshake. Wire format 3.0 is unchanged; an old version's wire is simply the wire written by the
old program.

| Header | Which end | Contents |
| --- | --- | --- |
| `xoffset_handshake.hpp` | Both | Handshake messages `XSchemaHello` / `XSchemaHelloAck`, `peek_wire_fingerprint` |
| `xoffset_evolution.hpp` | Server | Conversion, version chains, sending and receiving per peer |
| `xoffset_schema_freeze.hpp` | Server build | Freezing a version at release, checking the lock file in CTest |
| `xoffset_schema_family.hpp` (in `xoffsetdatastructure.hpp`) | Both | Add-only versions without conversion: `XSpare<N>`, `XOFFSET_SCHEMA_FAMILY` (section 9) |

`xoffset_evolution.hpp` and `xoffset_schema_freeze.hpp` are server-only and need the C++26 reflection
backend (GCC 16, P2996 Clang); including them under the field-list backend fails to compile. Clients only
need `xoffset_handshake.hpp`, which compiles in both the C++20 field-list build (`XOFFSET_RECORD`) and the
reflection build. The field-name digest is the same under both backends (field names come from
`schema::member_t<T, I>::name`).

## 1. Model

- The versions of one root type form a **version chain**, ordered by release; a program works in one of
  them (the working version) and uses the others only for sending and receiving.
- The `schema_hash` in the wire header says which version on the chain the wire is. Receiving an old
  version: first fully validate it against the **old version's type** (`load_verified`), then upgrade
  step by step to the working version. Sending to a peer on an old version: downgrade step by step, then
  write with `save_compact`.
- Fields correspond by **name**, regardless of offset and declaration order.

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

Conversion failures (enum value does not exist, `checked_narrow` out of range, duplicate keys, the target
buffer exceeding its limit, a hook calling `ctx.fail`) all throw `XException` with the field path in the
message, and no half-built result is produced.

## 2. Writing a new version

1. At release, use the freeze tool to save the current version as a snapshot header (section 5), for
   example `game_v12::Player`. Records and enums that did not change are not copied; the snapshot writes
   `using Item = game_v11::Item;`.
2. Change the definition of the working version.
3. Register the version chain (global scope):

   ```cpp
   XOFFSET_SCHEMA_LINEAGE(game::Player, game_v11::Player, game_v12::Player)
   ```

   When the working version is not in the list, it goes last. All versions on the chain must register the
   same schema name and have pairwise different `schema_hash` values, or compilation fails: renaming
   fields without changing the layout is not a new version; use `XOFFSET_SCHEMA_NAME_ALIAS` (section 4).
4. Write hooks for fields the default rules cannot handle, one per pair of adjacent versions:

   ```cpp
   template <> struct XOffsetDatastructure::xevolve<game_v12::Player, game::Player> {
       static constexpr std::string_view handles[] = {"gold", "nick", "display_name"};
       static void upgrade(const game_v12::Player& from, game::Player& to, XEvolveContext& ctx) {
           to.gold = ctx.widen<std::int64_t>(from.gold);
           to.display_name.assign(from.nick.view());
       }
       static void downgrade(const game::Player& from, game_v12::Player& to, XEvolveContext& ctx) {
           to.gold = ctx.checked_narrow<std::int32_t>(from.gold, "gold");
           to.nick.assign(from.display_name.view());
       }
   };
   ```

   The default copy runs first, skipping the fields in `handles`, then the hook is called. A name in
   `handles` that neither version has fails to compile. Nested records can have their own hooks too.
5. Commit the output of `evolve_plan<game_v12::Player, game::Player>()` (for each field: copy, convert,
   check enum, add, drop or hook) together with the snapshot, so reviewers can see which values a step
   drops.

`evolve<To>(buffer, options)` converts an already validated buffer to another version and can also be
used on its own (reading old snapshots, offline migration). `XEvolveOptions::with_base(&obj)` hands the
server's own object to the hook of the last step (`ctx.base<T>()`), to keep fields an old version cannot
carry back.

## 3. Handshake, sending and receiving

On each connection the client first sends a hello, the server answers with an ack, and only then come
ordinary messages. The frame format is up to the application; `examples/schema_handshake.cpp` is the
simplest length-prefixed version.

Client (does not throw; no-exception builds are supported once SC-8's `try_load_verified` lands):

```cpp
send(make_schema_hello<game::Player, game::Chat>("client-1.4.2"));
XSchemaAckResult r = read_schema_ack<game::Player, game::Chat>(receive());
if (!r) show_update_prompt(r.message);   // the server does not know this version
```

Server:

```cpp
std::string ack;
XPeerSchemas peer = XPeerSchemas::accept<game::Player, game::Chat>(hello, ack, "server-12");
send(ack);
if (!peer.accepted()) close();

auto msg = peer.load_verified<game::Player>(wire, XBuffer::maximum_capacity(1 << 20));  // any version on the chain
send(peer.save(state));                                                               // the peer's version
```

- For each root type the hello carries three items: root type id, `schema_hash` and the field-name
  digest `name_digest`. For each the server answers `same`, `convert`, `reject_unknown` (not on the
  chain) or `reject_names` (same layout but different field names, see section 4).
- When one message goes to many peers, use `XBroadcast<W>`: each version is converted and written only
  once.
- Without a handshake: `XPeerSchemas::from_fingerprints` or `learn(peek_wire_fingerprint(wire).fingerprint)`.
- Reading old snapshots only: `evolve_load_verified<W>(wire)`.

A server's own hello (between servers) uses `make_server_schema_hello<Roots...>`: it also lists the other
versions on the chain as `known`. When an older server sees that the other side knows its working
version, it judges `convert`, and the newer side does the conversion.

## 4. Field-name digest

`schema_hash` covers only the layout, not field names. When two versions only swap the names of two
fields of the same type, `schema_hash` is the same, and reading by bytes would read hp as mp. Three
things stop this:

- The handshake compares `name_digest` (`schema_name_digest_v<T>`), and judges same layout with
  different names as `reject_names`;
- The version chain requires pairwise different `schema_hash` values (at compile time);
- The lock file check (section 5).

When it really is only a rename and the meaning of the bytes is unchanged, use an alias so the new
version also accepts builds with the old names:

```cpp
XOFFSET_SCHEMA_NAME_ALIAS(game::Player, ::XOffsetDatastructure::schema_name_digest_v<game_v12::Player>)
```

The digest is fnv1a of a name signature that lists, for every record reachable from the root in the
order `wire_abi_signature()` walks it, each member as `name:offset`. The text is part of the handshake
compatibility promise: two programs agree on a version only if they compute the same digest for it.

```
record          {name:offset<field>,name:offset<field>}
array           [element]   (also std::array)
vector, set     <element>
map             <key,value>
scalar, enum, string: empty
```

## 5. The freeze tool and the lock file

The project links a small program of its own:

```cpp
#include "game_schema.hpp"
#include "xoffset_schema_freeze.hpp"
int main(int argc, char** argv) {
    return XOffsetDatastructure::schema_freeze_main<game::Player, game::Chat>(argc, argv);
}
```

- `schema_freeze freeze <label> <namespace> <header> <lock>`: run at release; it writes the snapshot
  header (with schema name registration and `XOFFSET_PIN_SCHEMA`, so changing a frozen type fails to
  compile) and appends this version to the end of the lock file. Every record newly defined in the
  snapshot has an `XOFFSET_RECORD` line, so clients without reflection (the C++20 field-list build) can
  include the snapshot directly and send and receive in that version; C++26 builds check these lines with
  reflection. `tests/test_schema_snapshot.cpp` checks this in every build and verifies that the
  `schema_hash` and field-name digest computed by the client match the lock file.
- `schema_freeze check <lock>`: put it in CTest so it runs on every build. It fails when the layout
  equals a frozen version but the field names differ (with no alias), and when the layout changed but the
  previous frozen version is not on the version chain.

Paths in the lock file and the `#include` lines of snapshots are written as given on the command line:
run the tool from the directory the project's include paths start at. The lock file is text, one line
per item, tab-separated, oldest version first:

```
version <label> <namespace> <header>
root    <schema name> <snapshot type> <schema_hash> <name_digest> <wire signature>
record  <snapshot type> <name> <wire signature> <name signature>
enum    <snapshot type> <name> <definition>
```

A complete example in the tests: `tests/schema_freeze/`.

## 6. Release rules

1. **Servers release before clients.** Clients only know their own version; all conversion is on the
   server.
2. **The server's version chain covers every client version still in use.** How many versions to keep is
   a business decision; after a version is dropped, clients on it get `reject_unknown` at handshake and
   should prompt for an update.
3. **Release in two steps when the server must be able to roll back.** First release a server that knows
   the new version but still works in the old one (the new version is on the chain, the working version
   unchanged); switch the working version in the next release. A rollback to the previous server then
   still knows the already upgraded clients. The working version need not be the newest on the chain.

## 7. Costs and limits

- Receiving an old version: one full validation of the old version, plus one conversion per step (field
  by field, reading the source object and building in a new buffer). Records with the same shape, same
  field names, no padding and no enums are copied as a whole.
- Sending an old version: one conversion per step plus one `save_compact`; for broadcasts, once per
  version.
- The converted object can be larger than the source (added fields, wider types). The target buffer's
  limit defaults to the source buffer's `maximum_capacity` and can be set with
  `XEvolveOptions::maximum_capacity`; exceeding it fails the conversion. When receiving untrusted data,
  always pass a limit to `load_verified`; it also bounds the conversion.

## 8. Tests

| Test | Contents |
| --- | --- |
| `test_schema_handshake` | hello / ack encoding and decoding, every verdict, malformed input |
| `test_schema_evolution` | Default rules, hooks, enums, containers, capacity limits, `evolve_plan` |
| `test_schema_lineage` | A v3 server against v1/v2/v3 clients, mixed-version servers, broadcast, aliases, two-step releases |
| `test_schema_freeze` | The freeze tool's snapshots and lock files are byte-identical to the committed versions; every failure of check |
| `tests/schema_evolution/*_fail.cpp` | Code that must fail to compile, and its error messages |
| `fuzz_schema_evolution` | libFuzzer: receiving arbitrary wires, sending random objects to every version, arbitrary handshake bytes (see `tests/fuzz/regressions/README.md`) |
| `test_xspare`, `test_schema_family`, `test_schema_family_xs`, `schema_family_*_fail` | Add-only versions (section 9) |
| `fuzz_schema_family` | libFuzzer, both backends: the in-place pair of section 9 |

## 9. Add-only versions (no conversion)

Sections 1 to 8 convert between versions on the server. When a type only gains fields, there is a
cheaper way: reserve bytes in advance and take new fields from them. Two such versions have the same
image bytes and one fingerprint, so they read each other's data with no run-time mechanism at all, in
both directions (an old program reads a new one's data and the other way round), on every path: whole
wires (full check and admit), image deltas, shared deltas (XS), relayout, compact and stored images.
Clients and servers may update in any order, and clients need nothing but the field-list build.

### 9.1 The rule

A later version only adds fields, and only inside bytes the previous version reserved with `XSpare<N>`.

| Change from the previous version | Allowed |
| --- | --- |
| A new field of a plain type (scalar, enum, array or record of them) inside an `XSpare<N>` member, the rest of the spare bytes stay `XSpare` | Yes |
| The element type `XSpare<N>` of a container (for example `XVector<XSpare<16>>`) becomes a plain record of N bytes | Yes: this is how a list is reserved |
| Two later versions that each add other fields to one version (branches) | Each is valid; they reject each other at the handshake |
| A field removed, renamed, moved to another offset, retyped (also `int32` to `uint32`, another enum underlying type, another container kind or element size) | No |
| A new field in padding, after the end of the record, or overlapping a field | No: the record's size and every offset stay |
| A new field that holds a container (string, vector, set, map), even inside a record | No: an older program's compact walk would drop its block |
| Spare bytes that become padding | No: they stay a field or `XSpare` |

Any other change is a new version with its own fingerprint: both ends update together, or the server
converts as in sections 1 to 8.

### 9.2 Reserving

`XSpare<N>` is N bytes, alignment 1, zero by default. A program never writes anything but zero into its
own spare members and never reads them; every check treats them as opaque bytes, so a newer peer may keep
fields there. It has its own token in the wire signature (`spare[N]`), so `XSpare<8>` and
`std::uint8_t[8]` are different layouts.

Put the spare member last, where it also fills the tail padding of the record, and size it for the fields
you expect: 8 bytes per frequently repeated element record and 16 at the root are a common choice. The
bytes cost in every image and on the wire: in the SLG example (8 bytes per hero, 16 at the root) the
image grows by 6.3%, by 3.0% after zstd level 3. Zero bytes compress well, and `XWireEmission::zero_runs`
does not send long runs of them. A list a later version will need is reserved as
`XVector<XSpare<16>>` (an empty vector costs its 12-byte head).

### 9.3 Declaring

Keep the previous version's types in the code (by hand, or as a snapshot header written by the freeze
tool of section 5) and declare the new one on top of it, at global scope, after the field lists:

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
- A version that breaks the rule does not compile. The message names the rule and the field, for
  example `XOFFSET_SCHEMA_FAMILY: a field of the previous version moved to another offset; fields keep
  their place (at stats.def)` (C++26; C++20 and MSVC report each rule in its own `static_assert`).
  `schema_family_successor_v<New, Prev>` asks the same question without failing.
- Do not put two versions of one family on one version chain of section 2: they have one fingerprint.

### 9.4 What follows from the declaration

- **Fingerprint.** `wire_schema_hash<New>()` is the hash of the family's first version; a type that
  declares no family keeps its own hash and its own code. Image headers, delta key frames and the
  handshake carry it.
- **Handshake.** `make_schema_hello` also lists the earlier versions of the family as `known` entries
  (their field-name digests); `accept_schema_hello` and `XPeerSchemas::accept` answer `same` to a peer on
  any version of the working type's family, earlier (its digest is in this program's family) or later
  (its hello lists this program's digest). Two branches of one version and another family under the same
  layout are `reject_names`, as before. The hello and ack formats do not change.
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

### 9.5 Two things the rule cannot check

1. **A new field must mean "unset" at zero**, and must not have a non-zero default member initializer:
   data written by an earlier version holds zero there, and a later program reads that zero.
2. **An earlier program that replaces a whole record resets the newer fields.** Editing fields of an
   existing record, resending, compacting and relayout keep every byte of the later version; but an
   earlier program that builds a record anew (assigns a default-constructed record, clears and refills a
   container of records) writes its own zeros into the spare bytes, and the later version reads its new
   fields as unset. When an earlier program may do that to data a later one owns, keep that data out of
   its reach or update both together.

### 9.6 Tests

| Test | Contents |
| --- | --- |
| `test_xspare` | `XSpare<N>`: layout, signature token, opaque on every tier, plain bytes in shared deltas |
| `test_schema_family` | Three versions of one type tree and a branch, every pair both ways: whole wires (full check and admit, every emission), an earlier program's edits and resends (compact, relayout), image deltas (byte-identical), stored images, handshake verdicts, the server side |
| `test_schema_family_xs` | Shared deltas between three versions: whole and change bodies byte-identical both ways, own-numbered masks, bool checks per version, diff ops, whole key and delta messages on clients of every version (full and admit) |
| `schema_family_<rule>_fail` | One declaration per rule that must not compile, and its message |
| `fuzz_schema_family` | libFuzzer, both backends: every input read by three versions of one family; a later version accepts nothing an earlier one rejects, and versions that accept an input hold the same bytes |
