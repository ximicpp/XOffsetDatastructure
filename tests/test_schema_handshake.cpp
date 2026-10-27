// Schema handshake (xoffset_handshake.hpp): header peek without throwing,
// hello and ack round trips, every outcome of read_schema_ack, the limits of
// a handshake message, and the field-name digest telling apart two versions
// whose schema_hash is the same.
#include "../xoffset_handshake.hpp"

#include <cstdio>
#include <cstring>
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
} // namespace

namespace game {
struct Player { XString name; std::int32_t hp; std::int32_t mp; XVector<std::int32_t> bag; };
struct Chat { XString text; std::int64_t at; };
} // namespace game
namespace game_swapped {
struct Player { XString name; std::int32_t mp; std::int32_t hp; XVector<std::int32_t> bag; };
} // namespace game_swapped
XOFFSET_REGISTER_SCHEMA_NAME(game::Player, "test.handshake.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game::Chat, "test.handshake.Chat")
XOFFSET_REGISTER_SCHEMA_NAME(game_swapped::Player, "test.handshake.Player")

// The hazard the digest closes: same layout, so the same wire fingerprint.
static_assert(wire_fingerprint_v<game::Player> == wire_fingerprint_v<game_swapped::Player>);
static_assert(schema_name_digest_v<game::Player> != schema_name_digest_v<game_swapped::Player>);
static_assert(sizeof(XSchemaEntry) == 32 && sizeof(XSchemaAckEntry) == 24);

namespace {

void peek() {
    auto b = TypedXBuffer<game::Player>::create();
    b.root().hp = 7;
    const std::string wire = b.save_verified();
    auto p = peek_wire_fingerprint(wire);
    CHECK(p && p.fingerprint == wire_fingerprint_v<game::Player>);
    CHECK(peek_wire_fingerprint(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire.data()), wire.size())));

    CHECK(peek_wire_fingerprint(std::string_view{}).status == XPeekStatus::bad_magic);
    CHECK(peek_wire_fingerprint(std::string_view("XOFFV2 old wire")).status == XPeekStatus::unsupported_version);
    CHECK(peek_wire_fingerprint(std::string_view(wire.data(), 39)).status == XPeekStatus::truncated);
    std::string flagged = wire;
    flagged[7] = char(0x80);  // flags bit 31: reserved
    CHECK(peek_wire_fingerprint(flagged).status == XPeekStatus::unsupported_flags);
    // A wire whose payload is garbage still peeks: the peek only selects the
    // version, load_verified decides.
    std::string body = wire;
    for (std::size_t i = sizeof(XWireHeaderV2); i < body.size(); ++i) body[i] = char(0xEE);
    CHECK(peek_wire_fingerprint(body));
}

void hello_round_trip() {
    const std::string hello = make_schema_hello<game::Player, game::Chat>("ios-1.12.0");
    CHECK(peek_wire_fingerprint(hello).fingerprint == wire_fingerprint_v<XSchemaHello>);
    auto h = TypedXBuffer<XSchemaHello>::load_verified(hello, XBuffer::maximum_capacity(XSCHEMA_HELLO_MAX_BYTES));
    const auto& m = h.root();
    CHECK(m.wire_format == 0x0201 && m.role == 0 && m.label.view() == "ios-1.12.0");
    CHECK(m.entries.size() == 2);
    CHECK(m.entries[0].root_type_id == wire_root_type_id_v<game::Player>());
    CHECK(m.entries[0].schema_hash == wire_schema_hash_v<game::Player>());
    CHECK(m.entries[0].name_digest == schema_name_digest_v<game::Player>);
    CHECK(m.entries[0].kind == 0 && m.entries[0].reserved == 0);
    CHECK(m.entries[1].schema_hash == wire_schema_hash_v<game::Chat>());
    // The label is clipped to the limit, so the hello always passes it.
    const std::string long_hello = make_schema_hello<game::Player>(std::string(1000, 'L'));
    auto lh = TypedXBuffer<XSchemaHello>::load_verified(long_hello);
    CHECK(lh.root().label.size() == XSCHEMA_HELLO_MAX_LABEL);
}

struct AckLine { XWireFingerprint fp; XSchemaVerdict verdict; };

std::string make_ack(std::initializer_list<AckLine> lines, std::uint32_t format = XSCHEMA_HELLO_WIRE_FORMAT,
                     std::size_t padding_entries = 0) {
    auto b = TypedXBuffer<XSchemaHelloAck>::create(
        4096, XBuffer::maximum_capacity(1 << 20).with_growth(XGrowPolicy::Automatic));
    auto& a = b.root();
    a.wire_format = format;
    a.role = 1;
    for (const auto& l : lines)
        a.entries.push_back({l.fp.root_type_id, l.fp.schema_hash, static_cast<std::uint32_t>(l.verdict), 0});
    for (std::size_t i = 0; i < padding_entries; ++i) a.entries.push_back({1, 2, 0, 0});
    a.label = "server-3.2";
    return b.save_compact();
}

void ack_outcomes() {
    using S = XSchemaAckResult::Status;
    const auto player = wire_fingerprint_v<game::Player>;
    const auto chat = wire_fingerprint_v<game::Chat>;

    auto r = read_schema_ack<game::Player, game::Chat>(
        make_ack({{player, XSchemaVerdict::same}, {chat, XSchemaVerdict::convert}}));
    CHECK(r && r.status == S::ok && r.message.empty());

    r = read_schema_ack<game::Player, game::Chat>(
        make_ack({{player, XSchemaVerdict::same}, {chat, XSchemaVerdict::reject_names}}));
    CHECK(!r && r.status == S::rejected && r.root_type_id == chat.root_type_id &&
          r.verdict == XSchemaVerdict::reject_names);

    r = read_schema_ack<game::Player, game::Chat>(make_ack({{player, XSchemaVerdict::same}}));
    CHECK(r.status == S::missing && r.root_type_id == chat.root_type_id);

    // An entry for another version of the same root type is not an answer.
    r = read_schema_ack<game::Player>(make_ack({{{player.root_type_id, 12345}, XSchemaVerdict::same}}));
    CHECK(r.status == S::missing);

    // A verdict value the client does not know is a rejection.
    {
        auto b = TypedXBuffer<XSchemaHelloAck>::create();
        b.root().wire_format = XSCHEMA_HELLO_WIRE_FORMAT;
        b.root().entries.push_back({player.root_type_id, player.schema_hash, 77, 0});
        r = read_schema_ack<game::Player>(b.save_compact());
        CHECK(r.status == S::rejected && r.verdict == XSchemaVerdict::reject_unknown);
    }

    // Malformed: never throws.
    r = read_schema_ack<game::Player>(std::string_view("garbage"));
    CHECK(r.status == S::malformed && !r.message.empty());
    r = read_schema_ack<game::Player>(make_ack({{player, XSchemaVerdict::same}}, 0x0300));
    CHECK(r.status == S::malformed);
    r = read_schema_ack<game::Player>(make_ack({{player, XSchemaVerdict::same}}, XSCHEMA_HELLO_WIRE_FORMAT,
                                               XSCHEMA_HELLO_MAX_ENTRIES));
    CHECK(r.status == S::malformed);
    // A hello is not an ack (root type id mismatch).
    r = read_schema_ack<game::Player>(make_schema_hello<game::Player>("x"));
    CHECK(r.status == S::malformed);
    // Larger than the handshake limit: rejected before loading.
    std::string big(XSCHEMA_HELLO_MAX_BYTES + 1, '\0');
    r = read_schema_ack<game::Player>(big);
    CHECK(r.status == S::malformed);
    // Every truncation and every single-byte corruption of a valid ack.
    const std::string good = make_ack({{player, XSchemaVerdict::same}});
    for (std::size_t n = 0; n < good.size(); ++n) {
        r = read_schema_ack<game::Player>(std::string_view(good.data(), n));
        CHECK(r.status == S::malformed);
    }
    std::size_t still_ok = 0;
    for (std::size_t i = 0; i < good.size(); ++i) {
        std::string bad = good;
        bad[i] = char(bad[i] ^ 0x5A);
        r = read_schema_ack<game::Player>(bad);
        still_ok += r.status == S::ok;  // e.g. a label byte: still a valid ack
    }
    CHECK(still_ok < good.size());
}

void accept_exact() {
    using S = XSchemaAckResult::Status;
    std::string ack;
    // Same versions on both ends.
    auto res = accept_schema_hello<game::Player, game::Chat>(
        make_schema_hello<game::Player, game::Chat>("client-1"), ack, "server-1");
    CHECK(res.valid && res.accepted() && res.label == "client-1" && res.role == XSchemaRole::client);
    CHECK(res.entries.size() == 2 && res.find(wire_root_type_id_v<game::Chat>())->verdict == XSchemaVerdict::same);
    CHECK((read_schema_ack<game::Player, game::Chat>(ack)));
    {
        auto a = TypedXBuffer<XSchemaHelloAck>::load_verified(ack);
        CHECK(a.root().label.view() == "server-1" && a.root().role == 1);
    }

    // Same layout, swapped field names: reject_names on both ends.
    res = accept_schema_hello<game::Player>(make_schema_hello<game_swapped::Player>("old"), ack);
    CHECK(res.valid && !res.accepted() && res.entries[0].verdict == XSchemaVerdict::reject_names);
    auto r = read_schema_ack<game_swapped::Player>(ack);
    CHECK(r.status == S::rejected && r.verdict == XSchemaVerdict::reject_names);

    // A root type the accepting side does not have.
    res = accept_schema_hello<game::Player>(make_schema_hello<game::Player, game::Chat>("c"), ack);
    CHECK(res.valid && !res.accepted());
    CHECK(res.find(wire_root_type_id_v<game::Chat>())->verdict == XSchemaVerdict::reject_unknown);
    r = read_schema_ack<game::Player, game::Chat>(ack);
    CHECK(r.status == S::rejected && r.root_type_id == wire_root_type_id_v<game::Chat>());
    CHECK(read_schema_ack<game::Player>(ack));   // a client that only uses Player is fine

    // Not a hello: no ack, no exception.
    ack = "stale";
    res = accept_schema_hello<game::Player>(std::string_view("not a hello"), ack);
    CHECK(!res.valid && !res.accepted() && ack.empty() && !res.message.empty());
    res = accept_schema_hello<game::Player>(make_ack({}), ack);
    CHECK(!res.valid && ack.empty());
}

} // namespace

int main() {
    peek();
    hello_round_trip();
    ack_outcomes();
    accept_exact();
    if (failures) {
        std::fprintf(stderr, "test_schema_handshake: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_handshake: all passed");
    return 0;
}
