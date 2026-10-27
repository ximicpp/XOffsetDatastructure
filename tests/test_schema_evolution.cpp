// Schema evolution, conversion core (xoffset_evolution.hpp): one case per row
// of the rule table, hooks and `handles`, enum checks, keyed containers,
// capacity, the plan text, and that every converted object saves to a wire
// its own version verifies.
#include "../xoffset_evolution.hpp"

#include <cstdio>
#include <cstdlib>
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

// ---- two versions of a small game schema ------------------------------------

namespace v1 {
enum class Rarity : std::uint8_t { common = 0, rare = 1 };
struct Vec3 { float x, y, z; };
struct Item {
    std::int32_t id;
    std::int32_t count;
    Rarity rarity;
    XString label;
};
struct Player {
    XString name;
    std::int32_t level;
    std::int32_t gold;          // becomes int64 in v2 (hook)
    std::int32_t mp;            // reordered in v2
    std::int32_t hp;
    std::int32_t legacy_flags;  // removed in v2
    XString nick;               // renamed to display_name in v2 (hook)
    Vec3 pos;                   // same shape in v2: one block copy
    std::int32_t slots[3];
    XVector<Item> bag;
    XSet<Rarity> seen;
    XMap<Rarity, XString> titles;
    XMap<std::int32_t, Item> stash;
};
} // namespace v1

namespace v2 {
enum class Rarity : std::uint8_t { common = 0, rare = 1, epic = 2 };
struct Vec3 { float x, y, z; };
struct Item {
    std::int32_t id;
    std::int32_t count;
    Rarity rarity;
    XString label;
    std::int32_t durability;    // added
};
struct Player {
    XString name;
    std::int32_t level;
    std::int64_t gold;
    std::int32_t hp;
    std::int32_t mp;
    XString display_name;
    Vec3 pos;
    std::int32_t slots[3];
    XVector<Item> bag;
    XSet<Rarity> seen;
    XMap<Rarity, XString> titles;
    XMap<std::int32_t, Item> stash;
    XVector<std::int32_t> achievements;  // added
};
} // namespace v2

XOFFSET_REGISTER_SCHEMA_NAME(v1::Player, "test.evolution.Player")
XOFFSET_REGISTER_SCHEMA_NAME(v2::Player, "test.evolution.Player")

template <> struct XOffsetDatastructure::xevolve<v1::Player, v2::Player> {
    static constexpr std::string_view handles[] = {"gold", "nick", "display_name"};
    static void upgrade(const v1::Player& from, v2::Player& to, XEvolveContext& ctx) {
        to.gold = ctx.widen<std::int64_t>(from.gold);
        to.display_name.assign(from.nick.view());
        if (const v2::Player* base = ctx.base<v2::Player>())  // keep what v1 cannot carry
            to.achievements = base->achievements;
    }
    static void downgrade(const v2::Player& from, v1::Player& to, XEvolveContext& ctx) {
        to.gold = ctx.checked_narrow<std::int32_t>(from.gold, "gold");
        to.nick.assign(from.display_name.view());
    }
};

// A nested hook: durability defaults to 100 when an old item arrives.
template <> struct XOffsetDatastructure::xevolve<v1::Item, v2::Item> {
    static void upgrade(const v1::Item&, v2::Item& to, XEvolveContext&) { to.durability = 100; }
    static void downgrade(const v2::Item&, v1::Item&, XEvolveContext&) {}
};

static_assert(wire_root_type_id_v<v1::Player>() == wire_root_type_id_v<v2::Player>());
static_assert(wire_schema_hash_v<v1::Player>() != wire_schema_hash_v<v2::Player>());
static_assert(detail::evolve::block_copy<v1::Vec3, v2::Vec3>(), "same shape and names: one memcpy");
static_assert(!detail::evolve::block_copy<v1::Item, v2::Item>());

// Same layout, swapped names: schema_hash cannot tell them apart, the name
// digest can.
namespace swap_a { struct S { std::int32_t hp; std::int32_t mp; }; }
namespace swap_b { struct S { std::int32_t mp; std::int32_t hp; }; }
XOFFSET_REGISTER_SCHEMA_NAME(swap_a::S, "test.evolution.Swap")
XOFFSET_REGISTER_SCHEMA_NAME(swap_b::S, "test.evolution.Swap")
static_assert(wire_schema_hash_v<swap_a::S>() == wire_schema_hash_v<swap_b::S>());
static_assert(schema_name_digest_v<swap_a::S> != schema_name_digest_v<swap_b::S>);
static_assert(!detail::evolve::block_copy<swap_a::S, swap_b::S>());

namespace {

void fill_v1(v1::Player& p) {
    p.name = "susu";
    p.level = 42;
    p.gold = -7;
    p.mp = 11;
    p.hp = 22;
    p.legacy_flags = 0x55;
    p.nick = "a nickname longer than the inline limit";
    p.pos = {1.5f, -2.0f, 3.25f};
    for (int i = 0; i < 3; ++i) p.slots[i] = 10 + i;
    for (int i = 0; i < 20; ++i) {
        auto& it = p.bag.emplace_back();
        it.id = i;
        it.count = i * 3;
        it.rarity = i % 2 ? v1::Rarity::rare : v1::Rarity::common;
        it.label = "item-" + std::to_string(i) + (i % 3 ? "" : " with a long label");
    }
    p.seen.insert(v1::Rarity::common);
    p.seen.insert(v1::Rarity::rare);
    p.titles.try_emplace(v1::Rarity::rare).first->second = "the rare one";
    for (int k : {5, -3, 99}) {
        auto& it = p.stash.try_emplace(k).first->second;
        it.id = k;
        it.count = 1;
        it.rarity = v1::Rarity::rare;
        it.label = "stash";
    }
}

template <typename T>
TypedXBuffer<T> reload(TypedXBuffer<T>& b) {
    // Defense in depth: the converted object saves to a compact wire that its
    // own version verifies.
    return TypedXBuffer<T>::load_verified(b.save_compact());
}

void upgrade_rules() {
    auto src = TypedXBuffer<v1::Player>::create(1 << 14);
    fill_v1(src.root());
    auto wire = src.save_compact();
    auto received = TypedXBuffer<v1::Player>::load_verified(wire);  // the old version, fully verified

    auto up = evolve<v2::Player>(received);
    auto checked = reload(up);
    const v2::Player& p = checked.root();
    CHECK(p.name.view() == "susu");                     // same name, same type
    CHECK(p.level == 42);
    CHECK(p.gold == -7);                                // hook: widened
    CHECK(p.hp == 22 && p.mp == 11);                    // reordered, matched by name
    CHECK(p.display_name.view() == "a nickname longer than the inline limit");  // rename via hook
    CHECK(p.pos.x == 1.5f && p.pos.y == -2.0f && p.pos.z == 3.25f);
    CHECK(p.slots[0] == 10 && p.slots[2] == 12);
    CHECK(p.bag.size() == 20);
    for (int i = 0; i < 20; ++i) {
        const auto& it = p.bag[i];
        CHECK(it.id == i && it.count == i * 3);
        CHECK(it.rarity == (i % 2 ? v2::Rarity::rare : v2::Rarity::common));
        CHECK(it.label.view() == "item-" + std::to_string(i) + (i % 3 ? "" : " with a long label"));
        CHECK(it.durability == 100);                    // added, nested hook
    }
    CHECK(p.seen.size() == 2 && p.seen.contains(v2::Rarity::rare));
    CHECK(p.titles.size() == 1 && p.titles.find(v2::Rarity::rare)->second.view() == "the rare one");
    CHECK(p.stash.size() == 3 && p.stash.find(-3)->second.durability == 100);
    CHECK(p.achievements.empty());                      // added: initial value
    CHECK(checked.save_compact() == up.save_compact());
}

v2::Player& make_v2(TypedXBuffer<v2::Player>& b) {
    auto& p = b.root();
    p.name = "server";
    p.level = 7;
    p.gold = 1234567;
    p.hp = 1;
    p.mp = 2;
    p.display_name = "dn";
    p.slots[1] = 5;
    auto& it = p.bag.emplace_back();
    it.id = 1;
    it.rarity = v2::Rarity::rare;
    it.durability = 3;
    p.seen.insert(v2::Rarity::common);
    p.achievements.push_back(9);
    return p;
}

void downgrade_rules() {
    auto src = TypedXBuffer<v2::Player>::create(1 << 14);
    make_v2(src);
    auto down = evolve<v1::Player>(src);
    auto checked = reload(down);
    const v1::Player& p = checked.root();
    CHECK(p.name.view() == "server" && p.level == 7 && p.gold == 1234567);
    CHECK(p.hp == 1 && p.mp == 2);
    CHECK(p.legacy_flags == 0);                         // removed in v2: initial value
    CHECK(p.nick.view() == "dn");
    CHECK(p.slots[1] == 5);
    CHECK(p.bag.size() == 1 && p.bag[0].rarity == v1::Rarity::rare);
    CHECK(p.seen.size() == 1);

    // checked_narrow: out of range fails the whole conversion, naming the field.
    src.root().gold = std::int64_t(1) << 40;
    auto why = failure_of([&] { (void)evolve<v1::Player>(src); });
    CHECK(why.find("schema evolution") != std::string::npos);
    CHECK(why.find("gold: value out of range") != std::string::npos);
    src.root().gold = 0;

    // Enum value the old version does not have: fails, with the path.
    src.root().bag[0].rarity = v2::Rarity::epic;
    why = failure_of([&] { (void)evolve<v1::Player>(src); });
    CHECK(why.find("bag.rarity") != std::string::npos);
    CHECK(why.find("not an enumerator") != std::string::npos);
    src.root().bag[0].rarity = v2::Rarity::common;

    // Same for a set key and a map key.
    src.root().seen.insert(v2::Rarity::epic);
    why = failure_of([&] { (void)evolve<v1::Player>(src); });
    CHECK(why.find("seen") != std::string::npos);
    src.root().seen.erase(v2::Rarity::epic);
    src.root().titles.try_emplace(v2::Rarity::epic);
    why = failure_of([&] { (void)evolve<v1::Player>(src); });
    CHECK(why.find("titles") != std::string::npos);
    src.root().titles.erase(v2::Rarity::epic);
    CHECK(failure_of([&] { (void)evolve<v1::Player>(src); }).empty());
}

void round_trip_and_base() {
    // Downgrade then upgrade: every field both versions have comes back.
    auto src = TypedXBuffer<v2::Player>::create(1 << 14);
    auto& orig = make_v2(src);
    auto down = evolve<v1::Player>(src);
    auto back = evolve<v2::Player>(down);
    const auto& p = back.root();
    CHECK(p.name.view() == orig.name.view() && p.gold == orig.gold && p.hp == orig.hp);
    CHECK(p.display_name.view() == orig.display_name.view());
    CHECK(p.bag.size() == 1 && p.bag[0].durability == 100);   // v1 cannot carry it
    CHECK(p.achievements.empty());

    // With the server's own object as base, the hook keeps what v1 lost.
    XEvolveOptions o;
    o.with_base(&orig);
    auto merged = evolve<v2::Player>(down, o);
    CHECK(merged.root().achievements.size() == 1 && merged.root().achievements[0] == 9);
}

void identity_is_compact_copy() {
    auto src = TypedXBuffer<v1::Player>::create(1 << 14);
    fill_v1(src.root());
    auto same = evolve<v1::Player>(src);
    CHECK(same.save_compact() == src.save_compact());
}

void capacity_limit() {
    auto src = TypedXBuffer<v1::Player>::create(1 << 16);
    fill_v1(src.root());
    for (int i = 0; i < 400; ++i) src.root().bag.emplace_back().label = std::string(64, 'x');
    XEvolveOptions o;
    o.maximum_capacity = 8192;
    auto why = failure_of([&] { (void)evolve<v2::Player>(src, o); });
    CHECK(why.find("maximum capacity") != std::string::npos);
}

void swapped_fields_by_name() {
    auto a = TypedXBuffer<swap_a::S>::create();
    a.root().hp = 1;
    a.root().mp = 2;
    auto b = evolve<swap_b::S>(a);
    CHECK(b.root().hp == 1 && b.root().mp == 2);
}

void plan_text() {
    const std::string plan = evolve_plan<v1::Player, v2::Player>();
    std::fputs(plan.c_str(), stdout);
    CHECK(plan.find("gold: hook") != std::string::npos);
    CHECK(plan.find("display_name: hook") != std::string::npos);
    CHECK(plan.find("nick: hook (removed)") != std::string::npos);
    CHECK(plan.find("legacy_flags: drop (removed)") != std::string::npos);
    CHECK(plan.find("achievements: init (added)") != std::string::npos);
    CHECK(plan.find("pos: copy") != std::string::npos);
    CHECK(plan.find("bag: convert") != std::string::npos);
    CHECK(plan.find("durability: init (added)") != std::string::npos);
    CHECK(plan.find("rarity: enum (checked)") != std::string::npos);
    CHECK(plan.find("seen: convert (keys checked)") != std::string::npos);
    CHECK(plan.find("then xevolve::upgrade") != std::string::npos);
    const std::string down = evolve_plan<v2::Player, v1::Player>();
    CHECK(down.find("then xevolve::downgrade") != std::string::npos);
    CHECK(down.find("legacy_flags: init (added)") != std::string::npos);
}

} // namespace

int main() {
    upgrade_rules();
    downgrade_rules();
    round_trip_and_base();
    identity_is_compact_copy();
    capacity_limit();
    swapped_fields_by_name();
    plan_text();
    if (failures) {
        std::fprintf(stderr, "test_schema_evolution: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_evolution: all passed");
    return 0;
}
