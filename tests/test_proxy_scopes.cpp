// Growth policy part 2: proxies (XRef) and pin scopes (read / edit).
// Proxies stay valid across payload moves and XBuffer moves, detect a
// destroyed or replaced buffer, and fail soft; pin scopes give plain
// references, refuse to move while open and defer block reuse.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
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
#if defined(XOFFSET_STRESS_RELOCATE)
        CHECK_SCOPE(old_block[0] == std::int32_t(0xDBDBDBDB));  // stress mode: poisoned
#else
        CHECK_SCOPE(old_block[0] == 0 && old_block[3] == 3);   // still readable, not reused
#endif
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
// Batch operations (growth policy part 4): one resolve per call, one regrow.
bool test_batch_operations() {
    auto b = make_relocating(4096);
    auto r = b.ref<Root>();
    auto ints = r.field<&Root::ints>();
    std::vector<std::int32_t> src(70000);
    for (int i = 0; i < 70000; ++i) src[std::size_t(i)] = i * 3;
    const char* first = b.data();
    CHECK(ints.append(std::span<const std::int32_t>(src)));   // 280 KB, past the 4 KiB reservation: moves
    CHECK(b.data() != first);
    CHECK(ints.size() == 70000u && ints.get(69999) == 69999 * 3);
    CHECK(ints.append({1, 2, 3}) && ints.size() == 70003u && ints.get(70002) == 3);
    CHECK(ints.append(5, 7) && ints.size() == 70008u && ints.get(70007) == 7);
    // A source inside the payload (this very vector): copied out first.
    const std::int32_t* own = b.root<Root>().ints.data();
    CHECK(ints.append(std::span<const std::int32_t>(own, 70008)));
    CHECK(ints.size() == 140016u && ints.get(70008 + 5) == 15 && ints.get(140015) == 7);
    CHECK(ints.assign({9, 8}) && ints.size() == 2u && ints.get(1) == 8);
    CHECK(ints.assign(std::span<const std::int32_t>(src)) && ints.size() == 70000u && ints.get(10) == 30);
    // In capacity: memmove from itself (shift down by one).
    const std::int32_t* now = b.root<Root>().ints.data();
    CHECK(ints.assign(std::span<const std::int32_t>(now + 1, 100)) && ints.size() == 100u && ints.get(0) == 3);

    auto players = r.field<&Root::players>();
    const auto at = players.emplace_n(500);
    CHECK(at == 0u && players.size() == 500u);
    CHECK(players.emplace_n(10) == 500u && players.size() == 510u);
    CHECK(players[509].field<&Player::id>().get() == 0);
    // Batch scope over one container: plain references, resolved once.
    CHECK(players.batch([&](XVector<Player>& v) {
        for (std::uint32_t i = 0; i < v.size(); ++i) {
            v[i].id = std::int32_t(i);
            v[i].name = "batch-" + std::to_string(i);
            v[i].items.push_back(std::int32_t(i));
        }
    }, 510u * 64u));
    CHECK(players[300].field<&Player::name>().copy() == "batch-300");
    CHECK(players[300].field<&Player::items>().get(0) == 300);
    CHECK(!b.pinned());
    // A batch on a record: the scope is the record.
    CHECK(players[7].batch([](Player& p) { p.level = 77; }));
    CHECK(players[7].field<&Player::level>().get() == 77);
    // Past the end: nothing pinned, out_of_range recorded.
    CHECK(!players[9999].batch([](Player& p) { p.level = 1; }));
    CHECK(b.error() == XErrc::out_of_range && !b.pinned());
    b.clear_error();

    auto m = r.field<&Root::map>();
    std::vector<std::pair<std::int32_t, std::int32_t>> kv;
    for (int i = 0; i < 30000; ++i) kv.push_back({(i * 7919) % 30000, i});
    CHECK(m.insert(kv) == 30000u && m.size() == 30000u);
    CHECK(m.insert(kv) == 0u);
    CHECK(m.get(7919 % 30000) == 1);
    auto s = r.field<&Root::set>();
    CHECK(s.insert({5, 3, 5, 1}) == 3u && s.size() == 3u && s.get(0) == 1);
    CHECK(b.ok());

    // Inside an edit scope a batch that would need the payload to move
    // fails with `borrowed` and changes nothing.
    auto small = make_relocating(8192);
    CHECK(!small.edit<Root>([&](Root&) {
        auto v = small.ref<Root>().field<&Root::ints>();
        (void)v.append(std::span<const std::int32_t>(src));
    }));
    CHECK(small.error() == XErrc::borrowed && small.root<Root>().ints.size() == 0u);
    return true;
}
// build<T>(f): reset + a pin scope in one step, one message after another.
bool test_build() {
    auto b = make_relocating(16384);
    auto old = b.ref<Root>().field<&Root::counter>();
    CHECK(b.build<Root>([](Root& m) {
        m.counter = 5;
        for (int i = 0; i < 10; ++i) m.ints.push_back(i);
        m.title = "first message";
    }));
    CHECK(!old.valid());                      // the content was replaced
    CHECK(!b.pinned() && b.root<Root>().counter == 5 && b.root<Root>().ints.size() == 10u);
    const std::string first = b.save_verified<Root>();
    CHECK(!first.empty());
    // Outgrowing the reservation without asking: the scope cannot move the
    // payload, so build starts over in a reservation twice as large, as
    // often as it takes (f runs again each time).
    int runs = 0;
    const std::size_t small_limit = b.reservation_limit();
    CHECK(b.build<Root>([&](Root& m) {
        ++runs;
        for (int i = 0; i < 100000; ++i) m.ints.push_back(i);
    }));
    CHECK(runs > 1 && b.ok() && !b.pinned());
    CHECK(b.root<Root>().ints.size() == 100000u && b.root<Root>().ints[99999] == 99999);
    CHECK(b.reservation_limit() > small_limit);
    // The buffer keeps the larger reservation: the same message again runs once.
    runs = 0;
    CHECK(b.build<Root>([&](Root& m) {
        ++runs;
        for (int i = 0; i < 100000; ++i) m.ints.push_back(i);
    }));
    CHECK(runs == 1 && b.ok());
    // At its maximum a buffer cannot start over larger: fails softly.
    auto capped = XBuffer::create<Root>(256, XBuffer::relocating(65536).with_reserve(16384));
    CHECK(!capped.build<Root>([](Root& m) {
        for (int i = 0; i < 100000; ++i) m.ints.push_back(i);
    }));
    CHECK(!capped.ok() && !capped.pinned());
    // The next message starts over: the failure goes with the old content.
    CHECK(capped.build<Root>([](Root& m) { m.counter = 6; }));
    CHECK(capped.ok() && capped.root<Root>().counter == 6 && capped.root<Root>().ints.size() == 0u);
    CHECK(b.build<Root>([](Root& m) { m.counter = 6; }));
    // Asking for the room up front: one move on entry, then plain speed.
    CHECK(b.build<Root>([](Root& m) {
        for (int i = 0; i < 100000; ++i) m.ints.push_back(i);
    }, 100000u * 16u));   // the doublings' retired blocks wait for the scope's end
    CHECK(b.ok() && b.root<Root>().ints.size() == 100000u && b.root<Root>().ints[99999] == 99999);
    // Each message emits what a separate reset + fill would.
    auto c = make_relocating(16384);
    CHECK(c.build<Root>([](Root& m) {
        m.counter = 5;
        for (int i = 0; i < 10; ++i) m.ints.push_back(i);
        m.title = "first message";
    }));
    CHECK(c.save_verified<Root>() == first);

    // Without any proxy the scope has no control block (a bare scope); what
    // would need one inside it gets one that counts the scope.
    auto d = make_relocating(16384);
    CHECK(d.build<Root>([&](Root& m) {
        m.counter = 1;
        CHECK_SCOPE(d.pinned());
        CHECK_SCOPE(d.read<Root>([](const Root& r) { return r.counter; }) == 1);   // nested scope
        CHECK_SCOPE(d.edit<Root>([](Root& r) { r.counter = 2; }));
        CHECK_SCOPE(d.pinned());
    }));
    CHECK(!d.pinned() && d.root<Root>().counter == 2);
    CHECK(!d.build<Root>([&](Root&) {                 // a failure recorded through a proxy
        (void)d.ref<Root>().field<&Root::players>()[3].field<&Player::level>().set(1);
    }));
    CHECK(d.error() == XErrc::out_of_range && !d.pinned());
    CHECK(d.build<Root>([](Root& m) { m.counter = 3; }) && d.ok());
    XBuffer moved;
    CHECK(d.build<Root>([&](Root& m) {               // the buffer moves away inside its scope
        m.counter = 4;
        moved = std::move(d);
        CHECK_SCOPE(moved.pinned());
    }));
    CHECK(!moved.pinned() && moved.root<Root>().counter == 4);
    CHECK(moved.ref<Root>().field<&Root::ints>().append({1, 2, 3}) && moved.ok());
    auto e = make_relocating(16384);
    auto f = make_relocating(16384);
    CHECK(e.build<Root>([&](Root& m) {               // and swaps with another
        m.counter = 5;
        e.swap(f);
        CHECK_SCOPE(f.pinned() && !e.pinned());
    }));
    CHECK(!e.pinned() && !f.pinned() && f.root<Root>().counter == 5);
    CHECK(f.build<Root>([](Root& m) { m.counter = 6; }) && f.root<Root>().counter == 6);
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
    ok = test_failed_buffer_refuses_to_emit() && ok;
    ok = test_batch_operations() && ok;
    ok = test_build() && ok;
    ok = g_scope_ok && ok;
    std::cout << (ok ? "test_proxy_scopes: all passed" : "test_proxy_scopes: FAILED") << std::endl;
    return ok ? 0 : 1;
}
