// Growth policy parts 2 and 3: proxies (XRef), pin scopes (read / edit) and
// the named field accessors XOFFSET_RECORD generates.
// Proxies stay valid across payload moves and XBuffer moves, detect a
// destroyed or replaced buffer, and fail soft; pin scopes give plain
// references, refuse to move while open and defer block reuse.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Item {
    std::int32_t a{0};
    std::int32_t b{0};
};
XOFFSET_RECORD(Item, a, b)
struct Player {
    std::int32_t id{0};
    std::int32_t level{0};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD(Player, id, level, name, items)
struct Root {
    XVector<std::int32_t> ints;
    XVector<Player> players;
    XMap<std::int32_t, std::int32_t> map;
    XSet<std::int32_t> set;
    XString title;
    XMap<XString, Item> named;
    std::int64_t counter{0};
};
XOFFSET_RECORD(Root, ints, players, map, set, title, named, counter)
XOFFSET_REGISTER_SCHEMA_NAME(Root, "test.proxy.Root")

struct PartA {
    std::int32_t a{0};
};
XOFFSET_RECORD(PartA, a)
struct PartB {
    std::int64_t b{0};
};
XOFFSET_RECORD(PartB, b)
struct Both : PartA, PartB {
    std::int32_t c{0};
};
XOFFSET_RECORD_DERIVED(Both, (PartA, PartB), c)

// Part 3: named accessors. Field names that are also names of the proxy's
// state (a, path) or functions (get, error), bases at a nonzero offset, and
// C arrays.
struct Odd {
    std::int32_t get{0};
    std::int32_t error{0};
    std::int32_t path{0};
    std::int32_t a{0};
};
XOFFSET_RECORD(Odd, get, error, path, a)
struct BaseA {
    std::int64_t x{0};
};
XOFFSET_RECORD(BaseA, x)
struct BaseB {
    std::int32_t y{0};
    XString tag;
};
XOFFSET_RECORD(BaseB, y, tag)
struct Derived : BaseA, BaseB {
    std::int32_t z{0};
    std::int32_t grid[2][3]{};
    XVector<Odd> odds;
};
XOFFSET_RECORD_DERIVED(Derived, (BaseA, BaseB), z, grid, odds)

#define CHECK(c)                                                                     \
    do {                                                                             \
        if (!(c)) {                                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);        \
            return false;                                                            \
        }                                                                            \
    } while (0)

// CHECK inside a scope lambda (which returns void).
bool g_scope_ok = true;
#define CHECK_SCOPE(c)                                                               \
    do {                                                                             \
        if (!(c)) {                                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);        \
            g_scope_ok = false;                                                      \
        }                                                                            \
    } while (0)

namespace {
XBuffer make_relocating(std::size_t reserve = 0) {
    auto cap = XBuffer::relocating();
    if (reserve) cap = cap.with_reserve(reserve);
    return XBuffer::create<Root>(256, cap);
}

bool test_proxy_survives_moves() {
    auto b = make_relocating();
    auto r = b.ref<Root>();
    auto ints = r.field<&Root::ints>();
    auto alias = r.field<&Root::ints>();    // a second proxy of the same vector
    const char* first = b.data();
    for (int i = 0; i < 600000; ++i) {
        CHECK(ints.push_back(2 * i));
        if (i % 1000 == 0) CHECK(alias.push_back(2 * i + 1));
    }
    CHECK(b.data() != first);               // the payload moved (1 MiB reservation)
    CHECK(b.ok());
    const auto& raw = b.root<Root>().ints;
    CHECK(raw.size() == 600000u + 600u);
    std::uint32_t at = 0;
    for (int i = 0; i < 600000; ++i) {
        CHECK(raw[at++] == 2 * i);
        if (i % 1000 == 0) CHECK(raw[at++] == 2 * i + 1);
    }
    CHECK(ints.size() == raw.size() && alias.size() == raw.size());
    CHECK(ints.get(1) == 1 && ints.get(1u << 30) == 0);
    return true;
}

bool test_records_and_elements() {
    auto b = make_relocating();
    auto r = b.ref<Root>();
    auto players = r.field<&Root::players>();
    for (int i = 0; i < 3000; ++i) {
        auto p = players.emplace_back();
        CHECK(p.valid());
        p.field<&Player::id>() = i;
        p.field<&Player::level>() += 3;
        p.field<&Player::name>() = "player-name-long-" + std::to_string(i);
        auto items = p.field<&Player::items>();
        for (int k = 0; k < 4; ++k) CHECK(items.push_back(i + k));
    }
    CHECK(players.size() == 3000u);
    CHECK(players[7].field<&Player::name>().copy() == "player-name-long-7");
    CHECK(players[7].field<&Player::name>() == "player-name-long-7");
    CHECK(players[7].member<1>().get() == 3);
    CHECK(players[2999].field<&Player::items>().get(3) == 2999 + 3);
    std::int64_t total = 0;
    for (auto p : players) total += p.field<&Player::level>();
    CHECK(total == 3 * 3000);
    // Past the end: reads give a value-initialized result, writes fail.
    CHECK(!players[5000].valid());
    CHECK(players[5000].field<&Player::id>().get() == 0);
    CHECK(b.ok());
    CHECK(!players[5000].field<&Player::id>().set(1));
    CHECK(b.error() == XErrc::out_of_range);
    b.clear_error();
    // An element proxy names container + index: after an erase it names the
    // next element, never a retired block.
    auto e = players[10];
    CHECK(players.erase(10));
    CHECK(e.field<&Player::id>().get() == 11);
    CHECK(players.size() == 2999u);
    // Copy of an element of the same buffer (may move the payload).
    CHECK(players.push_back(players[0]));
    CHECK(players.back().field<&Player::name>().copy() == "player-name-long-0");
    CHECK(players.back().field<&Player::items>().size() == 4u);
    return true;
}

// A member of a base at a nonzero offset is found at that offset.
bool test_base_members() {
    auto b = XBuffer::create<Both>(256, XBuffer::relocating());
    auto r = b.ref<Both>();
    r.field<&PartA::a>() = 1;
    r.field<&PartB::b>() = 2;
    r.field<&Both::c>() = 3;
    const Both& raw = b.root<Both>();
    CHECK(raw.a == 1 && raw.b == 2 && raw.c == 3);
    return true;
}

bool test_maps_sets_strings() {
    auto b = make_relocating(4096);
    auto r = b.ref<Root>();
    auto m = r.field<&Root::map>();
    for (int i = 0; i < 20000; ++i) CHECK(m.try_emplace(i * 7 % 20000, i));
    CHECK(!m.try_emplace(7, -1));
    CHECK(m.size() == 20000u && m.get(7) == 1 && m.get(-5, 42) == 42);
    auto v = m.find(14);
    CHECK(v.get() == 2);
    v = 99;
    CHECK(m.get(14) == 99);
    CHECK(!m.find(-1).valid());
    CHECK(m.insert_or_assign(14, 100) == false && m.get(14) == 100);
    CHECK(m.erase(14) && !m.contains(14));
    CHECK(v.get() == 0 && !v.valid());       // the key is gone: the proxy names nothing
    auto s = r.field<&Root::set>();
    CHECK(s.insert(5) && !s.insert(5) && s.contains(5) && s.size() == 1u);
    CHECK(s.erase(5) && s.empty());
    auto named = r.field<&Root::named>();
    named["alpha"].field<&Item::a>() = 4;
    named["beta"].field<&Item::b>() = 6;
    CHECK(named.find("alpha").field<&Item::a>().get() == 4);
    CHECK(named.find("beta").field<&Item::b>().get() == 6);
    CHECK(named.entry(0).key().copy() == "alpha");
    CHECK(named.entry(1).value().field<&Item::b>().get() == 6);
    auto title = r.field<&Root::title>();
    title = "hello";
    title += " world, this is longer than inline";
    CHECK(title.copy() == "hello world, this is longer than inline");
    // A string assigned from a view into the same buffer survives a move
    // made by the assignment itself.
    std::string big(200000, 'x');
    title = big;
    title.with_view([&](std::string_view view) { (void)view; });
    auto title2 = r.field<&Root::players>().emplace_back().field<&Player::name>();
    title2 = title;                         // proxy to proxy: copied first
    CHECK(title2.size() == 200000u);
    r.field<&Root::counter>() += 5;
    ++r.field<&Root::counter>();
    CHECK(r.field<&Root::counter>().get() == 6);
    CHECK(b.ok());
    return true;
}

bool test_xbuffer_moves_and_lifetime() {
    auto b = make_relocating();
    auto ints = b.ref<Root>().field<&Root::ints>();
    CHECK(ints.push_back(1));
    XBuffer b2 = std::move(b);              // proxies follow the content
    CHECK(ints.size() == 1u && ints.push_back(2));
    CHECK(b2.root<Root>().ints.size() == 2u);
    auto other = make_relocating();
    auto other_ints = other.ref<Root>().field<&Root::ints>();
    CHECK(other_ints.push_back(7));
    b2.swap(other);
    CHECK(ints.size() == 2u && other_ints.size() == 1u);
    CHECK(other.root<Root>().ints.size() == 2u && b2.root<Root>().ints.size() == 1u);
    // Proxies made after the swap name the content each buffer now holds.
    CHECK(other.ref<Root>().field<&Root::ints>().get(1) == 2);
    CHECK(b2.ref<Root>().field<&Root::ints>().get(0) == 7);
    CHECK(other.ref<Root>().field<&Root::ints>().push_back(3) && other.root<Root>().ints.size() == 3u);
    CHECK(b2.root<Root>().ints.size() == 1u);
    {
        XBuffer gone = std::move(other);
    }
    CHECK(ints.size() == 0u && !ints.push_back(3) && !ints.valid());
    CHECK(ints.error() == XErrc::stale);
    // Replaced content (reset, reload): proxies of the old content are stale.
    auto t = TypedXBuffer<Root>::create(256, XBuffer::relocating());
    auto old = t.ref().field<&Root::counter>();
    old = 5;
    const std::string wire = t.save_verified();
    t.reset();
    CHECK(!old.valid() && old.get() == 0);
    CHECK(t.ok());
    auto fresh = t.ref().field<&Root::counter>();
    fresh = 9;
    t.reload_verified(wire);
    CHECK(!fresh.valid());
    CHECK(t.ref().field<&Root::counter>().get() == 5);
    return true;
}

bool test_pin_scopes() {
    auto b = make_relocating();
    auto r = b.ref<Root>();
    bool ok = b.edit<Root>([&](Root& root) {
        root.counter = 42;
        for (int i = 0; i < 1000; ++i) root.ints.push_back(i);
    });
    CHECK(ok && r.field<&Root::counter>().get() == 42);
    const auto n = b.read<Root>([](const Root& root) { return root.ints.size(); });
    CHECK(n == 1000u);
    CHECK(!b.pinned());

    // Inside a scope the payload does not move: a proxy operation that would
    // need a move fails with `borrowed` and changes nothing.
    auto small = make_relocating(16384);
    const char* where = small.data();
    ok = small.edit<Root>([&](Root&) {
        auto ints = small.ref<Root>().field<&Root::ints>();
        for (int i = 0; i < 100000; ++i)
            if (!ints.push_back(i)) break;
    });
    CHECK(!ok && small.error() == XErrc::borrowed);
#if !defined(XOFFSET_STRESS_RELOCATE)   // stress mode moves the payload at every scope exit
    CHECK(small.data() == where);
#endif
    const auto kept = small.root<Root>().ints.size();
    CHECK(kept > 0 && kept < 100000u);
    small.clear_error();
    auto ints = small.ref<Root>().field<&Root::ints>();
    for (int i = 0; i < 100000; ++i) CHECK(ints.push_back(i));   // outside: moves
    CHECK(small.data() != where && small.ok());

    // Blocks retired inside a scope are not reused before it closes.
    auto q = make_relocating();
    ok = q.edit<Root>([&](Root& root) {
        for (int i = 0; i < 4; ++i) root.ints.push_back(i);
        root.title = "a title long enough for a heap block";   // the ints block cannot grow in place
        const std::int32_t* old_block = root.ints.data();
        root.ints.push_back(4);             // regrows: the old block is retired
        CHECK_SCOPE(old_block != root.ints.data());
        for (int i = 0; i < 64; ++i) root.players.emplace_back().items.push_back(i);
        for (auto& p : root.players) CHECK_SCOPE(p.items.data() != old_block);
        CHECK_SCOPE(old_block[0] == 0 && old_block[3] == 3);   // still readable, not reused
    });
    CHECK(ok);
    // On entry a Relocating buffer whose reservation is more than half used
    // moves first.
    auto half = make_relocating();
    half.root<Root>().ints.resize(160000);   // 640 KB of the 1 MiB reservation, in place
    const char* before = half.data();
    CHECK(half.edit<Root>([](Root& root) { root.counter = 1; }));
    CHECK(half.data() != before && half.root<Root>().ints.size() == 160000u);
    return true;
}

// A scope that outgrows the reservation: the failing operations have no
// effect (with exceptions the rest of the scope is skipped), edit returns
// false and the failure is recorded; nothing crashes.
bool test_scope_overflow_fails_soft() {
    auto b = make_relocating(16384);
    const char* where = b.data();
    int reached = 0;
    const bool ok = b.edit<Root>([&](Root& root) {
        for (int i = 0; i < 100000; ++i) root.ints.push_back(i);
        Player& p = root.players.emplace_back();   // a sink when it fails (no exceptions)
        p.id = 5;
        p.name = "a name that needs a heap block in the payload";
        for (int i = 0; i < 1000; ++i) p.items.push_back(i);
        root.map.try_emplace(1, 2);
        root.title.append(std::string(100000, 'y'));
        reached = 1;
    });
    CHECK(!ok && !b.ok());
#if !defined(XOFFSET_STRESS_RELOCATE)
    CHECK(b.data() == where);
#endif
    CHECK(b.root<Root>().ints.size() < 100000u);
#if XOFFSET_NO_EXCEPTIONS
    CHECK(reached == 1);                    // every failing operation returned
    CHECK(b.error() == XErrc::borrowed);
#endif
    // The buffer is intact and usable after clear_error().
    b.clear_error();
    CHECK(!b.save_verified<Root>().empty());
    auto ints = b.ref<Root>().field<&Root::ints>();
    CHECK(ints.push_back(1) && b.ok());
    return true;
}

bool test_named_accessors() {
    // TypedXBuffer::root() is a proxy; each listed field has an accessor
    // that is field<&T::m>().
    auto t = TypedXBuffer<Root>::create(256, XBuffer::relocating().with_reserve(16384));
    auto r = t.root();
    static_assert(std::is_same_v<decltype(r), XRef<Root>>);
    // The accessor chain is empty: a record proxy is its state only.
    static_assert(sizeof(XRef<Root>) == sizeof(proxy_detail::RefBase<Root, proxy_detail::RootPath>));
    static_assert(std::is_same_v<decltype(r.ints()), decltype(r.field<&Root::ints>())>);
    static_assert(std::is_same_v<decltype(r.players()[0].name()),
                                 decltype(r.players()[0].field<&Player::name>())>);
    const char* first = t.data();
    auto players = r.players();
    for (int i = 0; i < 4000; ++i) {
        auto p = players.emplace_back();
        p.id() = i;
        p.level() += 2;
        p.name() = "accessor-player-" + std::to_string(i);
        CHECK(p.items().push_back(i));
    }
    CHECK(t.data() != first);                 // moved; every proxy still works
    CHECK(players[3999].name() == "accessor-player-3999");
    CHECK(players[17].items().get(0) == 17 && players[17].level().get() == 2);
    r.counter() = 7;
    r.title() = "named";
    r.named()["k"].a() = 3;                   // `a` is also the proxy's anchor
    r.named()["k"].b() = 4;
    CHECK(r.map().try_emplace(1, 2) && r.map().get(1) == 2);
    CHECK(r.field<&Root::set>().insert(9));   // `set` is the proxy's set(const T&)
    const auto& raw = t.plain_root();
    CHECK(raw.counter == 7 && raw.title.view() == "named" && raw.players.size() == 4000u);
    CHECK(raw.named.find(std::string("k"))->second.a == 3 && raw.named.find(std::string("k"))->second.b == 4);
    // reset() gives a proxy of the new root; proxies of the old one are stale.
    auto fresh = t.reset();
    CHECK(!players.valid() && !r.counter().valid() && fresh.valid());
    CHECK(fresh.counter().get() == 0 && fresh.players().size() == 0u);
    CHECK(t.ok());

    auto d = TypedXBuffer<Derived>::create(256, XBuffer::relocating());
    auto v = d.root();
    v.x() = 11;                               // BaseA, at offset 0
    v.y() = 22;                               // BaseB, at a nonzero offset
    v.tag() = "tagged";
    v.z() = 33;
    CHECK(v.field<&BaseB::y>().get() == 22);  // a base member through field<>
    v.grid()[1][2] = 5;
    v.grid()[0][0] += 1;
    CHECK(v.grid().size() == 2u && v.grid()[1].size() == 3u);
    CHECK(!v.grid()[2][0].set(1) && d.error() == XErrc::out_of_range);
    d.clear_error();
    CHECK(v.grid()[2][0].get() == 0);
    std::int32_t sum = 0;
    for (auto row : v.grid())
        for (auto cell : row) sum += cell;
    CHECK(sum == 6);
    auto odd = v.odds().emplace_back();
    odd.a() = 1;                              // field accessors win over the proxy state
    odd.path() = 2;
    odd.field<&Odd::get>() = 3;               // get / error keep the proxy's meaning
    odd.field<&Odd::error>() = 4;
    CHECK(odd.error() == XErrc::ok && odd.valid());
    CHECK(odd.get().get == 3 && odd.get().error == 4 && odd.get().path == 2 && odd.get().a == 1);
    const auto& dr = d.plain_root();
    CHECK(dr.x == 11 && dr.y == 22 && dr.tag.view() == "tagged" && dr.z == 33);
    CHECK(dr.grid[1][2] == 5 && dr.grid[0][0] == 1 && dr.odds[0].path == 2);
    // A loaded buffer: the same accessors.
    auto back = TypedXBuffer<Root>::load(TypedXBuffer<Root>::create().save_verified());
    CHECK(back.root().players().emplace_back().name().assign("loaded"));
    CHECK(back.root().players()[0].name() == "loaded" && back.ok());
    return true;
}

bool test_failed_buffer_refuses_to_emit() {
    auto b = make_relocating();
    auto r = b.ref<Root>();
    r.field<&Root::counter>() = 3;
    CHECK(!r.field<&Root::players>()[3].field<&Player::id>().set(1));
    CHECK(!b.ok());
    // Allocating operations fail once a failure is recorded.
    CHECK(!r.field<&Root::ints>().push_back(1) || r.field<&Root::ints>().capacity() > 0);
#if XOFFSET_NO_EXCEPTIONS
    CHECK(b.save_verified<Root>().empty());
#else
    bool refused = false;
    try {
        (void)b.save_verified<Root>();
    } catch (const XException& e) {
        refused = e.code() == XErrc::out_of_range;
    }
    CHECK(refused);
#endif
    b.clear_error();
    CHECK(!b.save_verified<Root>().empty());
    return true;
}
} // namespace

int main() {
    bool ok = true;
    ok = test_proxy_survives_moves() && ok;
    ok = test_records_and_elements() && ok;
    ok = test_base_members() && ok;
    ok = test_maps_sets_strings() && ok;
    ok = test_xbuffer_moves_and_lifetime() && ok;
    ok = test_pin_scopes() && ok;
    ok = g_scope_ok && ok;
    ok = test_scope_overflow_fails_soft() && ok;
    ok = test_named_accessors() && ok;
    ok = test_failed_buffer_refuses_to_emit() && ok;
    std::cout << (ok ? "test_proxy_scopes: all passed" : "test_proxy_scopes: FAILED") << std::endl;
    return ok ? 0 : 1;
}
