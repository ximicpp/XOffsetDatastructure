// Stored data across generations (xoffset_evolution.hpp): a program working in
// generation 3 of a message loads images saved by every version of generations
// 1 and 2 through evolve_load_verified. Generation 2 is an add-only family of
// two versions (g2b takes fields from g2a's XSpare bytes), so its lineage entry
// is g2b. Checked: values arrive by name, every version's image loads, XSpare
// bytes are never copied (sizes may differ between generations), an image of
// the working generation takes the plain load_verified path, an image of no
// listed generation is rejected as before, capacity limits hold, and a peer of
// another generation is refused at the handshake (nothing converts messages).
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

// Generation 1: one version.
namespace g1 {
struct Item { std::int32_t id; std::int32_t n; XSpare<8> spare{}; };
struct Player { std::int32_t hp; XString name; XVector<Item> items; XSpare<16> spare{}; };
}
// Generation 2: hp widened to 64 bits (a new fingerprint). Its first version
// keeps generation 1's Item (as tools/schema_freeze writes it); the second
// takes `mp` and Item::quality from the first one's spare bytes.
namespace g2a {
using Item = g1::Item;
struct Player { std::int64_t hp; XString name; XVector<Item> items; XSpare<24> spare{}; };
}
namespace g2b {
struct Item { std::int32_t id; std::int32_t n; std::int32_t quality; XSpare<4> spare{}; };
struct Player { std::int64_t hp; XString name; XVector<Item> items; std::int32_t mp; XSpare<20> spare{}; };
}
// Generation 3, the working version: fields reordered, Item::weight added,
// other spare sizes.
namespace g3 {
struct Item { std::int32_t id; std::int32_t quality; std::int32_t n; std::int32_t weight; XSpare<8> spare{}; };
struct Player { XString name; std::int32_t mp; std::int64_t hp; XVector<Item> items; XSpare<20> spare{}; };
}
// A generation nobody listed.
namespace g0 {
struct Player { std::int32_t hp; XString name; };
}
// The same layout as g3 under other field names (a rename).
namespace g3_renamed {
using Item = g3::Item;
struct Player { XString nick; std::int32_t mp; std::int64_t hp; XVector<Item> items; XSpare<20> spare{}; };
}

XOFFSET_REGISTER_SCHEMA_NAME(g1::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(g2a::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(g2b::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(g3::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(g0::Player, "test.lineage.Player")
XOFFSET_REGISTER_SCHEMA_NAME(g3_renamed::Player, "test.lineage.Player")

XOFFSET_SCHEMA_FAMILY(g2b::Player, g2a::Player)

// Generation 1 -> 2 changes the type of hp: a hook. Generation 2 -> 3 needs
// none: every field keeps its type, and XSpare members are not copied.
template <> struct XOffsetDatastructure::xevolve<g1::Player, g2b::Player> {
    static constexpr std::string_view handles[] = {"hp"};
    static void upgrade(const g1::Player& from, g2b::Player& to, XEvolveContext& ctx) {
        to.hp = ctx.widen<std::int64_t>(from.hp);
    }
};

// The program loads stored data of generations 1 and 2; each entry is the last
// version of its generation.
XOFFSET_SCHEMA_LINEAGE(g3::Player, g1::Player, g2b::Player)

static_assert(detail::evolve::version_count<g3::Player> == 3);
static_assert(detail::evolve::working_index<g3::Player> == 2);
static_assert(wire_schema_hash_v<g2a::Player>() == wire_schema_hash_v<g2b::Player>(),
              "one family, one fingerprint");

// Two versions of one family are one generation: listing both fails
// (XOFFSET_SCHEMA_LINEAGE would not compile; checked here without it).
namespace g4 { struct Player { std::int64_t hp; XString name; XSpare<4> spare{}; }; }
XOFFSET_REGISTER_SCHEMA_NAME(g4::Player, "test.lineage.Player")
template <> struct XOffsetDatastructure::xschema_lineage<g4::Player> {
    using versions = detail::evolve::lineage_list<g4::Player, g2a::Player, g2b::Player>;
};
static_assert(!detail::evolve::lineage_ok<g4::Player>());
static_assert(detail::evolve::lineage_same_hash<g4::Player>()[0] == 0 &&
              detail::evolve::lineage_same_hash<g4::Player>()[1] == 1);

namespace {

auto big_cap() { return XBuffer::maximum_capacity(1 << 22).with_growth(XGrowPolicy::Automatic); }

template <typename P>
TypedXBuffer<P> make_player(std::string_view name, std::int32_t hp, int items) {
    auto b = TypedXBuffer<P>::create(1 << 14, big_cap());
    auto& p = b.plain_root();
    p.name.assign(name);
    p.hp = hp;
    if constexpr (requires { p.mp; }) p.mp = 7;
    for (int i = 0; i < items; ++i) {
        auto& it = p.items.emplace_back();
        it.id = i;
        it.n = i * 10;
        if constexpr (requires { it.quality; }) it.quality = i + 100;
    }
    return b;
}

template <std::size_t N>
bool zero(const XSpare<N>& s) {
    for (const std::uint8_t b : s.bytes)
        if (b != 0) return false;
    return true;
}

bool all_spare_zero(const g3::Player& p) {
    if (!zero(p.spare)) return false;
    for (const auto& it : p.items)
        if (!zero(it.spare)) return false;
    return true;
}

void every_version_loads() {
    // Generation 1: hp through the hook, the rest by name; added fields 0.
    {
        auto old = make_player<g1::Player>("one", 11, 3);
        auto now = evolve_load_verified<g3::Player>(old.save_verified());
        const g3::Player& p = now.plain_root();
        CHECK(p.name.view() == "one" && p.hp == 11 && p.mp == 0);
        CHECK(p.items.size() == 3 && p.items[2].id == 2 && p.items[2].n == 20);
        CHECK(p.items[2].quality == 0 && p.items[2].weight == 0);
        CHECK(all_spare_zero(p));
    }
    // Generation 2, first version: read as g2b (one fingerprint); the fields
    // g2b took from spare read 0.
    {
        auto old = make_player<g2a::Player>("two-a", 22, 2);
        auto now = evolve_load_verified<g3::Player>(old.save_compact());
        const g3::Player& p = now.plain_root();
        CHECK(p.name.view() == "two-a" && p.hp == 22 && p.mp == 0);
        CHECK(p.items.size() == 2 && p.items[1].n == 10 && p.items[1].quality == 0);
        CHECK(all_spare_zero(p));
    }
    // Generation 2, second version: mp and quality arrive by name.
    {
        auto old = make_player<g2b::Player>("two-b", 33, 4);
        auto now = evolve_load_verified<g3::Player>(old.save_verified());
        const g3::Player& p = now.plain_root();
        CHECK(p.name.view() == "two-b" && p.hp == 33 && p.mp == 7);
        CHECK(p.items.size() == 4 && p.items[3].id == 3 && p.items[3].n == 30 && p.items[3].quality == 103);
        CHECK(p.items[3].weight == 0);
        CHECK(all_spare_zero(p));
    }
    // The working generation: the plain load_verified path, the same object.
    {
        auto cur = make_player<g3::Player>("three", 44, 2);
        cur.plain_root().items[1].weight = 5;
        const std::string w = cur.save_verified();
        auto now = evolve_load_verified<g3::Player>(w);
        CHECK(now.save_verified() == TypedXBuffer<g3::Player>::load_verified(w).save_verified());
        CHECK(now.plain_root().items[1].weight == 5);
    }
}

void spare_bytes_are_not_copied() {
    // Bytes a later version of generation 2 would have used (never listed in
    // the lineage) stay behind: the working version's spare bytes are 0, so a
    // later version of generation 3 finds its new fields unset. The same size
    // (Player::spare, 20 bytes both) and other sizes (Item::spare) alike.
    auto old = make_player<g2b::Player>("spare", 1, 2);
    for (auto& b : old.plain_root().spare.bytes) b = 0xab;
    for (auto& it : old.plain_root().items)
        for (auto& b : it.spare.bytes) b = 0xcd;
    auto now = evolve_load_verified<g3::Player>(old.save_compact());
    CHECK(now.plain_root().mp == 7 && now.plain_root().items[1].quality == 101);
    CHECK(all_spare_zero(now.plain_root()));

    // The conversion on its own, outside a lineage: the same.
    auto direct = evolve<g3::Player>(old);
    CHECK(all_spare_zero(direct.plain_root()));
}

void rejections() {
    // An image of no listed generation: the plain load's message.
    auto other = TypedXBuffer<g0::Player>::create();
    other.plain_root().hp = 1;
    CHECK(failure_of([&] { (void)evolve_load_verified<g3::Player>(other.save_compact()); })
              .find("schema hash mismatch") != std::string::npos);
    // Same layout under other names: the bytes cannot tell, so an image loads
    // under the working names. The handshake and the freeze check are what
    // stop such a build (tests below and test_schema_freeze).
    auto renamed = TypedXBuffer<g3_renamed::Player>::create();
    renamed.plain_root().nick.assign("n");
    CHECK(evolve_load_verified<g3::Player>(renamed.save_compact()).plain_root().name.view() == "n");
}

void capacity() {
    auto big = make_player<g1::Player>("big", 1, 2000);
    const std::string w = big.save_compact();
    CHECK(failure_of([&] {
              (void)evolve_load_verified<g3::Player>(w, XBuffer::maximum_capacity(w.size() + 4096));
          }).find("maximum capacity") != std::string::npos);
    CHECK(evolve_load_verified<g3::Player>(w, XBuffer::maximum_capacity(1 << 20)).plain_root().items.size() == 2000);
}

template <typename Client, typename Server>
XSchemaVerdict verdict() {
    std::string ack;
    const auto r = accept_schema_hello<Server>(make_schema_hello<Client>("client"), ack, "server");
    CHECK(r.valid && r.entries.size() == 1);
    const auto read = read_schema_ack<Client>(ack);
    const XSchemaVerdict v = r.entries.empty() ? XSchemaVerdict::reject_unknown : r.entries[0].verdict;
    CHECK(bool(read) == accepted(v));
    return v;
}

void handshake() {
    // Peers of other generations are refused both ways; the lineage changes
    // nothing on the network.
    CHECK((verdict<g1::Player, g3::Player>() == XSchemaVerdict::reject_unknown));
    CHECK((verdict<g2b::Player, g3::Player>() == XSchemaVerdict::reject_unknown));
    CHECK((verdict<g3::Player, g2b::Player>() == XSchemaVerdict::reject_unknown));
    // Versions of one family talk.
    CHECK((verdict<g2a::Player, g2b::Player>() == XSchemaVerdict::same));
    CHECK((verdict<g2b::Player, g2a::Player>() == XSchemaVerdict::same));
    CHECK((verdict<g3::Player, g3::Player>() == XSchemaVerdict::same));
    // A rename is refused.
    CHECK((verdict<g3_renamed::Player, g3::Player>() == XSchemaVerdict::reject_names));
}

void plans() {
    const std::string a = evolve_plan<g1::Player, g2b::Player>();
    CHECK(a.find("hp: hook") != std::string::npos && a.find("mp: init (added)") != std::string::npos);
    CHECK(a.find("quality: init (added)") != std::string::npos);
    CHECK(a.find("spare: spare (not copied)") != std::string::npos);
    const std::string b = evolve_plan<g2b::Player, g3::Player>();
    CHECK(b.find("weight: init (added)") != std::string::npos && b.find("hp: copy") != std::string::npos);
    CHECK(b.find("spare: copy") == std::string::npos);
}

} // namespace

int main() {
    every_version_loads();
    spare_bytes_are_not_copied();
    rejections();
    capacity();
    handshake();
    plans();
    if (failures) {
        std::fprintf(stderr, "test_schema_lineage: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_lineage: all passed");
    return 0;
}
