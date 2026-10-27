// Schema evolution with a version lineage (xoffset_evolution.hpp): one v3
// server talking to v1, v2 and v3 clients through the handshake, mixed v2/v3
// servers, broadcast, old snapshots, a server that already knows a newer
// version but still works in the older one, name digests and aliases.
#include "../xoffset_evolution.hpp"

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

template <typename F>
std::string failure_of(F&& f) {
    try {
        f();
    } catch (const XException& e) {
        return e.what();
    }
    return {};
}
} // namespace

// Three releases of one message. Item did not change from v1 to v2, so v2
// refers to v1's (as tools/schema_freeze does).
namespace v1 {
struct Item { std::int32_t id; std::int32_t n; };
struct Player { XString name; std::int32_t hp; XVector<Item> items; };
}
namespace v2 {
using Item = v1::Item;
struct Player { XString name; std::int32_t hp; std::int32_t mp; XVector<Item> items; };
}
namespace v3 {
struct Item { std::int32_t id; std::int32_t n; std::int32_t quality; };
struct Player { XString name; std::int64_t hp; std::int32_t mp; XVector<Item> items; XString guild; };
}
// A build of v2 whose fields are named differently (same layout).
namespace v2_renamed {
using Item = v1::Item;
struct Player { XString name; std::int32_t health; std::int32_t mp; XVector<Item> items; };
}
// v3 with `guild` renamed to `clan`: same bytes, same meaning.
namespace v3_clan {
using Item = v3::Item;
struct Player { XString name; std::int64_t hp; std::int32_t mp; XVector<Item> items; XString clan; };
}

XOFFSET_REGISTER_SCHEMA_NAME(v1::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(v2::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(v3::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(v2_renamed::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(v3_clan::Player, "test.lineage.Player")

template <> struct XOffsetDatastructure::xevolve<v2::Player, v3::Player> {
    static constexpr std::string_view handles[] = {"hp"};
    static void upgrade(const v2::Player& from, v3::Player& to, XEvolveContext& ctx) {
        to.hp = ctx.widen<std::int64_t>(from.hp);
    }
    static void downgrade(const v3::Player& from, v2::Player& to, XEvolveContext& ctx) {
        to.hp = ctx.checked_narrow<std::int32_t>(from.hp, "hp");
    }
};

// The v3 server: works in v3, keeps v1 and v2.
XOFFSET_SCHEMA_LINEAGE(v3::Player, v1::Player, v2::Player)
// The v2 server (before the v3 release).
XOFFSET_SCHEMA_LINEAGE(v2::Player, v1::Player)
// v3 accepts builds that call the field `clan`.
XOFFSET_SCHEMA_NAME_ALIAS(v3::Player, ::XOffsetDatastructure::schema_name_digest_v<v3_clan::Player>)

// The first step of a two-step release: a server that already knows s3 but
// still works in s2, so rolling back to it later keeps s3 clients working.
namespace s1 { struct T { std::int32_t a; std::int32_t pad; }; }
namespace s2 { struct T { std::int32_t a; std::int32_t b; std::int32_t pad; std::int32_t pad2; }; }
namespace s3 { struct T { std::int32_t a; std::int32_t b; std::int32_t c; std::int32_t pad; std::int64_t d; }; }
XOFFSET_REGISTER_SCHEMA_NAME(s1::T, "test.lineage.Staged")
XOFFSET_REGISTER_SCHEMA_NAME(s2::T, "test.lineage.Staged")
XOFFSET_REGISTER_SCHEMA_NAME(s3::T, "test.lineage.Staged")
XOFFSET_SCHEMA_LINEAGE(s2::T, s1::T, s2::T, s3::T)
static_assert(::XOffsetDatastructure::detail::evolve::working_index<s2::T> == 1);

static_assert(detail::evolve::version_count<v3::Player> == 3);
static_assert(detail::evolve::working_index<v3::Player> == 2);
static_assert(detail::evolve::working_index<v2::Player> == 1);

namespace {

template <typename P>
TypedXBuffer<P> make_player(std::string_view name, std::int32_t hp, int items) {
    auto b = TypedXBuffer<P>::create(1 << 14, XBuffer::maximum_capacity(1 << 22).with_growth(XGrowPolicy::Automatic));
    auto& p = b.root();
    p.name.assign(name);
    if constexpr (requires { p.hp; }) p.hp = hp;
    if constexpr (requires { p.mp; }) p.mp = 5;
    for (int i = 0; i < items; ++i) {
        auto& it = p.items.emplace_back();
        it.id = i;
        it.n = i * 10;
    }
    return b;
}

// Client of version P connects to the v3 server.
template <typename P>
XPeerSchemas connect(XSchemaVerdict expected) {
    const std::string hello = make_schema_hello<P>("client");
    std::string ack;
    XPeerSchemas peer = XPeerSchemas::accept<v3::Player>(hello, ack, "server-v3");
    CHECK(peer.entries().size() == 1 && peer.entries()[0].verdict == expected);
    CHECK(bool(read_schema_ack<P>(ack)) == accepted(expected));
    return peer;
}

void one_server_three_clients() {
    auto p1 = connect<v1::Player>(XSchemaVerdict::convert);
    auto p2 = connect<v2::Player>(XSchemaVerdict::convert);
    auto p3 = connect<v3::Player>(XSchemaVerdict::same);
    CHECK(p1.send_index<v3::Player>() == 0 && p2.send_index<v3::Player>() == 1 && p3.sends_working_version<v3::Player>());

    // Receive: each client sends its own version; the server gets v3.
    {
        auto c1 = make_player<v1::Player>("one", 11, 3);
        auto got = p1.load_verified<v3::Player>(c1.save_compact());
        CHECK(got.root().name.view() == "one" && got.root().hp == 11 && got.root().mp == 0);
        CHECK(got.root().items.size() == 3 && got.root().items[2].n == 20 && got.root().items[2].quality == 0);
        CHECK(got.root().guild.empty());
    }
    {
        auto c2 = make_player<v2::Player>("two", 22, 2);
        auto got = p2.load_verified<v3::Player>(c2.save_verified());
        CHECK(got.root().hp == 22 && got.root().mp == 5 && got.root().items.size() == 2);
    }

    // Send: one v3 object, a wire per version, each verified by its client.
    auto server = make_player<v3::Player>("state", 33, 4);
    server.root().guild = "g";
    server.root().items[1].quality = 9;
    {
        auto w1 = p1.save(server);
        CHECK(peek_wire_fingerprint(w1).fingerprint == wire_fingerprint_v<v1::Player>);
        auto c = TypedXBuffer<v1::Player>::load_verified(w1);
        CHECK(c.root().name.view() == "state" && c.root().hp == 33 && c.root().items.size() == 4);
        auto w2 = p2.save(server);
        auto c2 = TypedXBuffer<v2::Player>::load_verified(w2);
        CHECK(c2.root().mp == 5 && c2.root().items[3].n == 30);
        auto w3 = p3.save(server);
        CHECK(w3 == server.save_verified());
    }

    // Downgrade that cannot be represented fails the message, nothing is sent.
    server.root().hp = std::int64_t(1) << 40;
    CHECK(failure_of([&] { (void)p2.save(server); }).find("hp: value out of range") != std::string::npos);
    CHECK(failure_of([&] { (void)p3.save(server); }).empty());
    server.root().hp = 33;

    // Broadcast: each version written once, shared by its peers.
    XBroadcast<v3::Player> b(server);
    auto p1b = connect<v1::Player>(XSchemaVerdict::convert);
    const std::string& a = b.wire_for(p1);
    const std::string& a2 = b.wire_for(p1b);
    CHECK(&a == &a2 && a == p1.save(server));
    CHECK(b.wire_for(p3) == server.save_verified());
    CHECK(b.wire_for(p2) == p2.save(server));
}

void rejections() {
    std::string ack;
    // Same layout as v2, other names: reject_names (the hazard).
    auto peer = XPeerSchemas::accept<v3::Player>(make_schema_hello<v2_renamed::Player>("bad build"), ack);
    CHECK(!peer.accepted() && peer.entries()[0].verdict == XSchemaVerdict::reject_names);
    CHECK(read_schema_ack<v2_renamed::Player>(ack).verdict == XSchemaVerdict::reject_names);
    // A rename registered as an alias is accepted as v3.
    peer = XPeerSchemas::accept<v3::Player>(make_schema_hello<v3_clan::Player>("clan build"), ack);
    CHECK(peer.accepted() && peer.entries()[0].verdict == XSchemaVerdict::same);
    // Not in the lineage (the v2 server does not know v3).
    peer = XPeerSchemas::accept<v2::Player>(make_schema_hello<v3::Player>("new client"), ack);
    CHECK(!peer.accepted() && peer.entries()[0].verdict == XSchemaVerdict::reject_unknown);
    // A malformed hello throws on the server.
    CHECK(failure_of([&] { (void)XPeerSchemas::accept<v3::Player>(std::string_view("junk"), ack); })
              .find("schema hello") != std::string::npos);
    CHECK(ack.empty());
    auto renamed = make_player<v2_renamed::Player>("r", 1, 0);
    auto w = renamed.save_compact();
    // Same fingerprint as v2 (the wire cannot tell): read as v2. The handshake
    // is what stops this build.
    auto got = XPeerSchemas{}.load_verified<v3::Player>(w);
    CHECK(got.root().hp == 0);  // `health` is not `hp`
    auto v3c = make_player<v3::Player>("v3", 1, 0);
    CHECK(failure_of([&] { (void)XPeerSchemas{}.load_verified<v2::Player>(v3c.save_compact()); })
              .find("schema hash mismatch") != std::string::npos);
}

void servers_mixed() {
    // v3 server <-> v2 server during a rolling upgrade. Each sends a server hello.
    std::string ack_from_v3, ack_from_v2;
    auto at_v3 = XPeerSchemas::accept<v3::Player>(make_server_schema_hello<v2::Player>("srv-v2"), ack_from_v3);
    auto at_v2 = XPeerSchemas::accept<v2::Player>(make_server_schema_hello<v3::Player>("srv-v3"), ack_from_v2);
    CHECK(at_v3.accepted() && at_v3.entries()[0].verdict == XSchemaVerdict::convert);
    CHECK(at_v3.role() == XSchemaRole::server && at_v3.label() == "srv-v2");
    // The v2 server does not know v3, but the v3 server listed v2 as known: it converts.
    CHECK(at_v2.accepted() && at_v2.entries()[0].verdict == XSchemaVerdict::convert);
    CHECK(read_schema_ack<v2::Player>(ack_from_v3) && read_schema_ack<v3::Player>(ack_from_v2));
    CHECK(at_v3.send_index<v3::Player>() == 1);           // v3 sends v2
    CHECK(at_v2.sends_working_version<v2::Player>());     // v2 sends v2
    auto s3 = make_player<v3::Player>("from-v3", 7, 2);
    auto wire = at_v3.save(s3);
    auto in_v2 = at_v2.load_verified<v2::Player>(wire);
    CHECK(in_v2.root().name.view() == "from-v3" && in_v2.root().items.size() == 2);
    auto back = at_v3.load_verified<v3::Player>(at_v2.save(in_v2));
    CHECK(back.root().hp == 7 && back.root().items[1].n == 10);
}

void snapshots() {
    auto old = make_player<v1::Player>("snap", 3, 5);
    auto now = evolve_load_verified<v3::Player>(old.save_verified());
    CHECK(now.root().name.view() == "snap" && now.root().items.size() == 5 && now.root().hp == 3);
    // Capacity of the receive bounds the conversion too.
    auto big = make_player<v1::Player>("big", 1, 2000);
    const std::string w = big.save_compact();
    CHECK(failure_of([&] {
              (void)evolve_load_verified<v3::Player>(w, XBuffer::maximum_capacity(w.size() + 4096));
          }).find("maximum capacity") != std::string::npos);
    CHECK(evolve_load_verified<v3::Player>(w, XBuffer::maximum_capacity(1 << 20)).root().items.size() == 2000);
}

void without_handshake() {
    // Learned from the first message's header.
    auto c1 = make_player<v1::Player>("h", 1, 0);
    const std::string w = c1.save_compact();
    XPeerSchemas peer;
    peer.learn(peek_wire_fingerprint(w).fingerprint);
    CHECK(peer.send_index<v3::Player>() == 0);
    const XWireFingerprint fps[] = {wire_fingerprint_v<v2::Player>};
    CHECK(XPeerSchemas::from_fingerprints(fps).send_index<v3::Player>() == 1);
    CHECK(XPeerSchemas{}.sends_working_version<v3::Player>());
}

void staged_release() {
    std::string ack;
    auto newer = XPeerSchemas::accept<s2::T>(make_schema_hello<s3::T>("s3 client"), ack);
    CHECK(newer.accepted() && newer.entries()[0].verdict == XSchemaVerdict::convert);
    CHECK(newer.send_index<s2::T>() == 2);
    auto older = XPeerSchemas::accept<s2::T>(make_schema_hello<s1::T>("s1 client"), ack);
    CHECK(older.accepted() && older.send_index<s2::T>() == 0);
    // Receive from s3: downgraded to the working s2.
    auto c = TypedXBuffer<s3::T>::create();
    c.root().a = 1; c.root().b = 2; c.root().c = 3;
    auto got = newer.load_verified<s2::T>(c.save_compact());
    CHECK(got.root().a == 1 && got.root().b == 2);
    // Send to s3: upgraded; to s1: downgraded.
    auto w = newer.save(got);
    CHECK(peek_wire_fingerprint(w).fingerprint == wire_fingerprint_v<s3::T>);
    auto at3 = TypedXBuffer<s3::T>::load_verified(w);
    CHECK(at3.root().a == 1 && at3.root().b == 2 && at3.root().c == 0);
    auto at1 = TypedXBuffer<s1::T>::load_verified(older.save(got));
    CHECK(at1.root().a == 1);
    // The server's hello lists s1 and s3 as known.
    auto h = TypedXBuffer<XSchemaHello>::load_verified(make_server_schema_hello<s2::T>("srv"));
    CHECK(h.root().entries.size() == 3 && h.root().entries[0].schema_hash == wire_schema_hash_v<s2::T>() &&
          h.root().entries[0].kind == 0 && h.root().entries[2].kind == 1);
}

void plans() {
    const std::string p = evolve_plan<v2::Player, v3::Player>();
    CHECK(p.find("hp: hook") != std::string::npos && p.find("guild: init (added)") != std::string::npos);
    CHECK(p.find("quality: init (added)") != std::string::npos);
    const std::string q = evolve_plan<v1::Player, v2::Player>();
    CHECK(q.find("items: copy") != std::string::npos);   // same element type
}

} // namespace

int main() {
    one_server_three_clients();
    rejections();
    servers_mixed();
    snapshots();
    without_handshake();
    staged_release();
    plans();
    if (failures) {
        std::fprintf(stderr, "test_schema_lineage: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_lineage: all passed");
    return 0;
}
