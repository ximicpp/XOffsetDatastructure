// A frozen schema snapshot (tools/schema_freeze output) used by a client: only
// the snapshot headers and the handshake, no reflection needed. Built in every
// configuration, so a C++20 field-list build proves that the generated
// XOFFSET_RECORD lines are complete, and that the client computes the
// schema_hash and field-name digest the server's lock file recorded.
#include "schema_freeze/game_v2.hpp"
#include "../xoffset_handshake.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace XOffsetDatastructure;

namespace {
int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)

// The root lines of tests/schema_freeze/schema.lock (written by a C++26 build).
constexpr std::uint64_t v1_hash = 0x3a2d14bca3ed4d8cull, v1_digest = 0x38258b0366f6944eull;
constexpr std::uint64_t v2_hash = 0x3f97b5b93fc5601full, v2_digest = 0xa731926e9a4a8adeull;

template <typename Player>
std::string old_client_message() {
    auto b = TypedXBuffer<Player>::create(4096, XBuffer::maximum_capacity(1 << 20).with_growth(XGrowPolicy::Automatic));
    auto& p = b.root();
    p.name = "old client";
    p.level = 7;
    p.slots[1][2] = 5;
    auto& item = p.items.emplace_back();
    item.id = 3;
    item.rarity = game_v1::Rarity::legendary;
    item.label = "a label longer than an inline string";
    p.notes[1] = "note";
    p.seen.insert(game_v1::Rarity::rare);
    return b.save_compact();
}

template <typename Player>
void round_trip() {
    const std::string wire = old_client_message<Player>();
    auto back = TypedXBuffer<Player>::load_verified(wire, XBuffer::maximum_capacity(1 << 20));
    const auto& p = back.root();
    CHECK(p.name.view() == "old client" && p.level == 7 && p.slots[1][2] == 5);
    CHECK(p.items.size() == 1 && p.items[0].rarity == game_v1::Rarity::legendary);
    CHECK(p.items[0].label.view() == "a label longer than an inline string");
    CHECK(p.notes.size() == 1 && p.seen.size() == 1);
    CHECK(peek_wire_fingerprint(wire).fingerprint == wire_fingerprint_v<Player>);
}
} // namespace

static_assert(wire_schema_hash_v<game_v1::Player>() == v1_hash);
static_assert(wire_schema_hash_v<game_v2::Player>() == v2_hash);
static_assert(schema_name_digest_v<game_v1::Player> == v1_digest);
static_assert(schema_name_digest_v<game_v2::Player> == v2_digest);

int main() {
    round_trip<game_v1::Player>();
    round_trip<game_v2::Player>();
    // The hello of an old client carries the version the server froze.
    const std::string hello = make_schema_hello<game_v1::Player>("old-client");
    CHECK(!hello.empty() && peek_wire_fingerprint(hello));
    if (failures) {
        std::fprintf(stderr, "test_schema_snapshot: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_snapshot: all passed");
    return 0;
}
