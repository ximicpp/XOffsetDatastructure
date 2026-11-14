// The shared-delta publisher (xoffset_shared_publisher.hpp): a server with
// the three kinds of tables (units in an XSyncDirtyTable with their images in
// a buffer, mobs in a ring XSyncImageTable, cells in a dense fixed one) and
// connections whose views move, compared with its clients after every
// message.
//
// - World: a few hundred units changing member by member (dirty bits), mobs
//   born with growing ids and dying (the ring reuses and doubles), cells.
// - Every tick each connection gets one message: leaves, changes, enters
//   from its view; sometimes it is skipped (a disconnect: release, then a key
//   message next tick) or reconnects. Each message is applied by two clients
//   of the connection, XCheck::full and XCheck::admit, whose views must equal
//   the world (units, mobs and cells by the slots the server gave).
// - Two connection shapes: the hot stacks large enough, and stacks of two
//   with more than 256 units held (slots past a byte, every give and take
//   through the cold spill lists).
// - Pinned: the slot order (the last slot freed is taken first, never in the
//   message that freed it), a body written once per tick for all holders,
//   key bodies shared by the connections entering in one tick, the ring
//   reusing a dead mob's place and doubling for a live one, entry sizes.
// With XOFFSET_SYNC_CHECKS (on without NDEBUG) contract 2 is verified every
// tick. Built with XOFFSET_FORCE_FULL_CHECK=0 so admit is the real admit
// path. No exception of its own: also built as test_shared_delta_publisher_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include "xoffset_receiver.hpp"
#include "xoffset_shared_publisher.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <random>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace pt {
namespace xo = XOffsetDatastructure;
struct Pos { std::int32_t x{}; std::int32_t y{}; };
struct Root { std::int64_t tick{}; std::uint32_t seq{}; };
struct Unit {
    std::uint32_t id{};
    std::int32_t hp{};
    bool alive{};
    Pos pos;
    xo::XString name;
    xo::XVector<std::uint16_t> buffs;
    std::uint8_t level{};
};
struct Mob { std::int32_t id{}; std::int32_t x{}; std::int32_t y{}; std::int64_t t{}; };
struct Cell { std::int32_t terrain{}; std::int64_t owner{}; };
struct Units { xo::XVector<Unit> units; };
struct View {
    Root root;
    xo::XVector<Unit> units;
    xo::XVector<Mob> mobs;
    xo::XVector<Cell> cells;
};
} // namespace pt
XOFFSET_RECORD(pt::Pos, x, y)
XOFFSET_RECORD(pt::Root, tick, seq)
XOFFSET_RECORD(pt::Unit, id, hp, alive, pos, name, buffs, level)
XOFFSET_RECORD(pt::Mob, id, x, y, t)
XOFFSET_RECORD(pt::Cell, terrain, owner)
XOFFSET_RECORD(pt::Units, units)
XOFFSET_RECORD(pt::View, root, units, mobs, cells)
XOFFSET_REGISTER_SCHEMA_NAME(pt::Units, "test.shared_publisher.units")
XOFFSET_REGISTER_SCHEMA_NAME(pt::View, "test.shared_publisher.view")

namespace {
using namespace pt;
namespace xs = XOffsetDatastructure::xsync;
enum : unsigned { kRoot = 0, kUnit = 1, kMob = 2, kCell = 3 };
// Unit members by bit (declaration order).
enum : unsigned { bId, bHp, bAlive, bPos, bName, bBuffs, bLevel };
constexpr std::size_t kCells = 24;

[[noreturn]] void fail(const char* what, long step) {
    std::fprintf(stderr, "test_shared_delta_publisher: %s (step %ld)\n", what, step);
    std::exit(1);
}
template <class T> bool same(const T& a, const T& b) { return xs::XRecordBody<T>::same(a, b); }

xo::XBuffer make_units(std::size_t n) {
    auto b = xo::XBuffer::create<Units>(1 << 16, xo::XBuffer::maximum_capacity(std::size_t(1) << 24).with_growth(xo::XGrowPolicy::Automatic));
    b.root<Units>().units.resize(n);
    return b;
}

using UnitTable = xs::XSyncDirtyTable<kUnit, Unit, 96, 160>;
using MobTable = xs::XSyncImageTable<kMob, Mob, xs::XSyncRing<&Mob::id>, true, 64>;
using CellTable = xs::XSyncImageTable<kCell, Cell, xs::XSyncDense, false>;
static_assert(UnitTable::entry_size == (XOFFSET_SYNC_CHECKS ? 64 : 56));
static_assert(MobTable::entry_size == 64);
static_assert(CellTable::entry_size == 40);

// A client's view: units and mobs at the slots the server gave them, cells
// by id; with Full, which slots it holds.
template <bool Full> struct Target {
    using tables = std::tuple<Root, Unit, Mob, Cell>;
    template <unsigned K> static constexpr bool keyed = K == kUnit || K == kMob;
    xo::XBuffer* buf;
    std::vector<std::uint8_t> uheld, mheld;
    Root root{};
    View& v() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kRoot) { if (id) return -1; root = v().root; return 0; }
        else if constexpr (K == kCell) return id < kCells ? std::int64_t(id) : -1;
        else if constexpr (Full) {
            const auto& h = K == kUnit ? uheld : mheld;
            return id < h.size() && h[id] ? std::int64_t(id) : -1;
        } else return id < (K == kUnit ? v().units.size() : v().mobs.size()) ? std::int64_t(id) : -1;
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kRoot) return root;
        else if constexpr (K == kUnit) return v().units[std::size_t(h)];
        else if constexpr (K == kMob) return v().mobs[std::size_t(h)];
        else return v().cells[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {
        if constexpr (K == kUnit) { if (id >= v().units.size()) v().units.resize(std::size_t(id) + 1); }
        else { if (id >= v().mobs.size()) v().mobs.resize(std::size_t(id) + 1); }
        if constexpr (Full) {
            auto& h = K == kUnit ? uheld : mheld;
            if (id >= h.size()) h.resize(std::size_t(id) + 1);
            h[id] = 1;
        }
        return std::int64_t(id);
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t h) {
        xs::XRecordBody<std::tuple_element_t<K, tables>>::clear(record<K>(h));
        if constexpr (Full) (K == kUnit ? uheld : mheld)[id] = 0;
        else (void)id;
    }
    template <unsigned K> void applied(std::uint32_t, std::int64_t) { if constexpr (K == kRoot) v().root = root; }
};

template <bool Full> struct Client {
    xo::XBuffer buf = xo::XBuffer::create<View>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 24).with_growth(xo::XGrowPolicy::Automatic));
    Target<Full> t{&buf};
    xs::XSyncCursor cur;
    Client() { buf.root<View>().cells.resize(kCells); }
    void reset() {   // before a key message: nothing held
        View& v = buf.root<View>();
        v.units.clear(); v.mobs.clear();
        t.uheld.clear(); t.mheld.clear();
    }
};
using FromServer = XOffsetDatastructure::XReceiver<XOffsetDatastructure::XLink::from_server>;
FromServer& receiver() {
    static FromServer rx(XOffsetDatastructure::XBuffer::maximum_capacity(std::size_t(1) << 24));
    return rx;
}

template <class Conn> struct Server {
    using Msg = xs::XSyncMessage<Conn>;
    struct Link {
        Conn hot{};
        typename Conn::cold_type cold;
        std::map<std::int32_t, std::int32_t> units, mobs;   // what the client holds: unit index / mob id -> slot
        Client<true> full;
        Client<false> admit;
        std::int32_t ua = 0, mx = 0;    // the view: units [ua, ua + uw), mobs with x in [mx, mx + mw)
        bool key = true;                // the next message is a key message
    };
    std::mt19937 rng;
    std::size_t nu;
    std::int32_t uw, mw;
    xo::XBuffer world, images;
    std::vector<std::uint64_t> dirty;
    std::vector<std::int32_t> changed, changed_cells;
    std::vector<Mob> mobs;              // live, any order; the ref is the index
    std::int32_t next_mob = 0;
    std::vector<Cell> cells = std::vector<Cell>(kCells);
    std::uint32_t tick = 10;
    xs::XSyncPublisher pub;
    UnitTable units;
    MobTable ring;
    CellTable cell;
    std::vector<Link> links;
    xs::XSyncOut out;
    std::vector<std::int32_t> scratch[8];
    long msgs = 0, keys = 0;

    Server(unsigned seed, std::size_t n_units, std::int32_t uwin, std::int32_t mwin, std::size_t n_links)
        : rng(seed), nu(n_units), uw(uwin), mw(mwin), world(make_units(n_units)), images(make_units(n_units)),
          dirty(n_units, 0), links(n_links) {
        for (std::size_t i = 0; i < nu; ++i) wu(std::uint32_t(i)).id = std::uint32_t(i) + 1;
        for (std::size_t i = 0; i < nu; ++i) img(std::uint32_t(i)).id = std::uint32_t(i) + 1;
        units.resize(nu);
        ring.resize(4);   // small: the ring doubles
        cell.resize(kCells);
        for (int i = 0; i < 30; ++i) born();
        for (auto& l : links) { l.ua = pick(std::int32_t(nu)); l.mx = pick(200); }
    }
    std::int32_t pick(std::int32_t n) { return std::int32_t(rng() % std::uint32_t(n)); }
    Unit& wu(std::uint32_t i) { return world.root<Units>().units[i]; }
    Unit& img(std::uint32_t i) { return images.root<Units>().units[i]; }
    void born() { mobs.push_back(Mob{next_mob++, pick(220), pick(50), pick(1000)}); }

    struct UnitSrc {
        Server* s;
        std::uint64_t dirty(std::uint32_t i) const { return s->dirty[i]; }
        Unit& image(std::uint32_t i) const { return s->img(i); }
        void load(std::uint32_t i, Unit& r, std::uint64_t m) const {
            xs::XSyncOut o;
            xs::put_change_op(o, 0, s->wu(i), m);
            xs::XRecordBody<Unit>::apply_change(o.data() + 4, r);
        }
    };
    struct MobSrc {
        Server* s;
        std::uint32_t id(std::uint32_t r) const { return std::uint32_t(s->mobs[r].id); }
        void load(std::uint32_t r, Mob& m) const { m = s->mobs[r]; }
        std::int32_t min_live() const {
            std::int32_t v = std::numeric_limits<std::int32_t>::max();
            for (const Mob& m : s->mobs) v = std::min(v, m.id);
            return v;
        }
    };
    struct CellSrc {
        Server* s;
        void load(std::uint32_t t, Cell& c) const { c = s->cells[t]; }
    };

    void mutate_unit(std::uint32_t i) {
        Unit& u = wu(i);
        std::uint64_t m = 0;
        for (int k = 1 + pick(2); k-- > 0;) {
            switch (pick(6)) {
            case 0: u.hp = pick(1000) - 100; m |= 1u << bHp; break;
            case 1: u.alive = !u.alive; m |= 1u << bAlive; break;
            case 2: u.pos.x += 1; u.pos.y = pick(40); m |= 1u << bPos; break;
            case 3: { std::string s(std::size_t(pick(4) == 0 ? 20 + pick(30) : pick(8)), 'n'); for (auto& c : s) c = char('a' + pick(26)); u.name = std::string_view(s); m |= 1u << bName; break; }
            case 4: u.buffs.resize(std::size_t(pick(5))); for (auto& b : u.buffs) b = std::uint16_t(pick(60000)); m |= 1u << bBuffs; break;
            default: u.level = std::uint8_t(pick(256)); m |= 1u << bLevel; break;
            }
        }
        if (!dirty[i]) changed.push_back(std::int32_t(i));
        dirty[i] |= m;
    }
    void step_world() {
        for (std::int32_t i : changed) dirty[std::size_t(i)] = 0;
        changed.clear();
        changed_cells.clear();
        for (int k = pick(std::int32_t(nu / 8) + 1); k-- > 0;) mutate_unit(std::uint32_t(pick(std::int32_t(nu))));
        for (int k = pick(4); k-- > 0 && !mobs.empty();) {   // deaths (any order: refs move)
            const std::size_t j = std::size_t(pick(std::int32_t(mobs.size())));
            mobs[j] = mobs.back(); mobs.pop_back();
        }
        for (int k = pick(4); k-- > 0;) born();
        if (pick(20) == 0) { next_mob += 40; born(); }   // an id jump
        for (Mob& m : mobs) if (pick(3) == 0) { m.x = std::min(219, std::max(0, m.x + pick(9) - 4)); m.t += 1; }
        for (int k = pick(3); k-- > 0;) {
            const std::int32_t c = pick(std::int32_t(kCells));
            cells[std::size_t(c)].owner += 1; cells[std::size_t(c)].terrain = pick(4);
            if (std::find(changed_cells.begin(), changed_cells.end(), c) == changed_cells.end()) changed_cells.push_back(c);
        }
        for (auto& l : links) {
            l.ua = std::max(0, std::min(std::int32_t(nu) - uw, l.ua + pick(7) - 3));
            l.mx = std::max(0, std::min(220 - mw, l.mx + pick(5) - 2));
        }
    }
    bool in_view(const Link& l, std::int32_t i) const { return i >= l.ua && i < l.ua + uw; }
    bool mob_in_view(const Link& l, const Mob& m) const { return m.x >= l.mx && m.x < l.mx + mw; }

    template <bool Full> void apply(Client<Full>& c, long step) {
        if (xs::is_key(out.view())) c.reset();
        const auto r = Full ? receiver().apply_sync<XOffsetDatastructure::XCheck::full>(out.view(), c.cur, c.t)
                            : receiver().apply_sync(out.view(), c.cur, c.t);
        if (!r) {
            std::fprintf(stderr, "%s\n", r.error().message.c_str());
            if (std::FILE* f = std::fopen("/out/bad.bin", "wb")) { std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); }
            fail("a client rejected a message", step);
        }
    }
    // The client holds exactly the world in its view, at the server's slots.
    template <bool Full> void check(Link& l, Client<Full>& c, long step) {
        View& v = c.buf.template root<View>();
        if (v.root.tick != std::int64_t(tick)) fail("root", step);
        for (std::size_t t = 0; t < kCells; ++t) if (!same(v.cells[t], cells[t])) fail("a cell differs", step);
        std::size_t held = 0;
        for (const auto& [i, s] : l.units) {
            if (std::size_t(s) >= v.units.size() || !same(v.units[std::size_t(s)], wu(std::uint32_t(i)))) fail("a unit differs", step);
            ++held;
        }
        for (const auto& [id, s] : l.mobs) {
            const auto it = std::find_if(mobs.begin(), mobs.end(), [&](const Mob& m) { return m.id == id; });
            if (it == mobs.end() || std::size_t(s) >= v.mobs.size() || !same(v.mobs[std::size_t(s)], *it)) fail("a mob differs", step);
        }
        if constexpr (Full) {
            std::size_t n = 0;
            for (auto h : c.t.uheld) n += h;
            if (n != held) fail("the client holds other units", step);
            n = 0;
            for (auto h : c.t.mheld) n += h;
            if (n != l.mobs.size()) fail("the client holds other mobs", step);
        }
    }

    void send(Link& l, long step) {
        auto& [uleft_i, uleft_s, uch_i, uch_s, uen, mleft, mch_r, mch_s] = scratch;
        for (auto& v : scratch) v.clear();
        const UnitSrc us{this};
        const MobSrc ms{this};
        if (l.key) {
            for (const auto& e : l.units) units.release(std::uint32_t(e.first));   // contract 3
            l.units.clear(); l.mobs.clear();
            Msg m(pub, l.hot, l.cold, out, Root{std::int64_t(tick), std::uint32_t(step)}, 0xabc00 + std::uint64_t(&l - links.data()));
            for (std::int32_t i = l.ua; i < l.ua + uw; ++i) uen.push_back(i);
            units.enters(m, us, uen, [&](std::size_t r, std::uint32_t s) { l.units[uen[r]] = std::int32_t(s); });
            for (std::size_t r = 0; r < mobs.size(); ++r) if (mob_in_view(l, mobs[r])) mch_r.push_back(std::int32_t(r));
            ring.enters(m, ms, mch_r, [&](std::size_t r, std::uint32_t s) { l.mobs[mobs[std::size_t(mch_r[r])].id] = std::int32_t(s); });
            mleft.clear();
            for (std::size_t t = 0; t < kCells; ++t) mleft.push_back(std::int32_t(t));
            cell.enters(m, CellSrc{this}, mleft);
            m.finish();
            l.key = false;
            ++keys;
        } else {
            for (auto it = l.units.begin(); it != l.units.end();) {
                if (!in_view(l, it->first)) { uleft_i.push_back(it->first); uleft_s.push_back(it->second); it = l.units.erase(it); }
                else { uch_i.push_back(it->first); uch_s.push_back(it->second); ++it; }
            }
            for (std::int32_t i = l.ua; i < l.ua + uw; ++i) if (!l.units.count(i)) uen.push_back(i);
            for (auto it = l.mobs.begin(); it != l.mobs.end();) {
                const auto mt = std::find_if(mobs.begin(), mobs.end(), [&](const Mob& x) { return x.id == it->first; });
                if (mt == mobs.end() || !mob_in_view(l, *mt)) { mleft.push_back(it->second); it = l.mobs.erase(it); }
                else { mch_r.push_back(std::int32_t(mt - mobs.begin())); mch_s.push_back(it->second); ++it; }
            }
            std::vector<std::int32_t> men;
            for (std::size_t r = 0; r < mobs.size(); ++r) if (mob_in_view(l, mobs[r]) && !l.mobs.count(mobs[r].id)) men.push_back(std::int32_t(r));
            Msg m(pub, l.hot, l.cold, out, Root{std::int64_t(tick), std::uint32_t(step)});
            units.leaves(m, uleft_i, uleft_s);
            ring.leaves(m, mleft);
            units.changes(m, us, uch_i, uch_s);
            ring.changes(m, ms, mch_r, mch_s);
            cell.changes(m, CellSrc{this}, changed_cells);
            units.enters(m, us, uen, [&](std::size_t r, std::uint32_t s) { l.units[uen[r]] = std::int32_t(s); });
            ring.enters(m, ms, men, [&](std::size_t r, std::uint32_t s) { l.mobs[mobs[std::size_t(men[r])].id] = std::int32_t(s); });
            m.finish();
            Conn::prefetch(&l.hot + 1);
        }
        ++msgs;
        apply(l.full, step);
        apply(l.admit, step);
        check(l, l.full, step);
        check(l, l.admit, step);
    }

    void run(int ticks) {
        for (int t = 0; t < ticks; ++t) {
            ++tick;
            step_world();
            pub.begin_tick(tick);
            units.publish(pub, changed, UnitSrc{this});
            for (std::int32_t c : changed_cells) cell.prefetch(std::uint32_t(c));
            for (auto& l : links) {
                const long step = long(t) * 1000 + long(&l - links.data());
                if (!l.key && pick(40) == 0) {   // skipped: a disconnect (contract 2), a key message next tick
                    for (const auto& e : l.units) units.release(std::uint32_t(e.first));
                    l.units.clear(); l.mobs.clear();
                    l.key = true;
                    continue;
                }
                if (pick(60) == 0) l.key = true;   // reconnects now
                send(l, step);
            }
#if XOFFSET_SYNC_CHECKS
            units.verify(pub);
#endif
            for (std::size_t i = 0; i < nu; ++i) {   // hold counts = the connections holding the unit
                std::int32_t h = 0;
                for (auto& l : links) h += std::int32_t(l.units.count(std::int32_t(i)));
                if (units.holders(std::uint32_t(i)) != h) fail("hold count", t);
            }
        }
    }
};

// The slot order, slots past the hot stack and past a byte, key bodies
// shared between connections, one body per tick.
void pinned() {
    using Conn = xs::XSyncConn<xs::XSyncFreeSlots<kUnit, 2>, xs::XSyncFreeSlots<kMob, 3>>;
    static_assert(sizeof(Conn) == 64 && alignof(Conn) == 64);
    Server<Conn> s(1, 400, 300, 10, 2);
    s.tick = 100;
    s.pub.begin_tick(++s.tick);
    auto& l = s.links[0];
    const typename Server<Conn>::UnitSrc us{&s};
    std::vector<std::int32_t> idx{0, 1, 2, 3, 4}, got;
    auto take = [&](std::size_t, std::uint32_t slot) { got.push_back(std::int32_t(slot)); };
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{}, 7);
        s.units.enters(m, us, idx, take);
        m.finish();
    }
    if (got != std::vector<std::int32_t>{0, 1, 2, 3, 4} || l.hot.version != 1) fail("key slots", 1);
    const std::size_t bodies = s.pub.bodies().size();
    {   // a second connection in the same tick shares the key bodies
        xs::XSyncMessage<Conn> m(s.pub, s.links[1].hot, s.links[1].cold, s.out, Root{}, 8);
        got.clear();
        s.units.enters(m, us, idx, take);
        m.finish();
    }
    if (s.pub.bodies().size() != bodies) fail("key bodies written twice in a tick", 2);
    for (std::int32_t i : idx) if (s.units.holders(std::uint32_t(i)) != 2) fail("holders after two keys", 3);
    s.pub.begin_tick(++s.tick);
    std::vector<std::int32_t> li{1, 3}, ls{1, 3}, e{5, 6};
    {   // leave slots 1 and 3, then two enter: not 1 or 3 (free from the next message on)
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        s.units.leaves(m, li, ls);
        got.clear();
        s.units.enters(m, us, e, take);
        m.finish();
    }
    if (got != std::vector<std::int32_t>{5, 6}) fail("a slot reused in the message that freed it", 4);
    s.pub.begin_tick(++s.tick);
    {   // the last freed first: 3, then 1, then new ones
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> e2{7, 8, 9};
        got.clear();
        s.units.enters(m, us, e2, take);
        m.finish();
    }
    if (got != std::vector<std::int32_t>{3, 1, 7}) fail("slot order", 5);
    // A stack of 2: three slots left in one message, one spills and comes back last.
    s.pub.begin_tick(++s.tick);
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> i3{0, 2, 4}, s3{0, 2, 4};
        s.units.leaves(m, i3, s3);
        m.finish();
    }
    if (l.hot.n[0] != 2 || l.cold.spill[0].size() != 1 || !l.hot.spill[0]) fail("spill", 6);
    s.pub.begin_tick(++s.tick);
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> e3{10, 11, 12, 13};
        got.clear();
        s.units.enters(m, us, e3, take);
        m.finish();
    }
    if (got != std::vector<std::int32_t>{2, 0, 4, 8} || l.hot.spill[0]) fail("spilled slot order", 7);
    // One change body per tick for every holder.
    s.pub.begin_tick(++s.tick);
    s.mutate_unit(5);
    s.units.publish(s.pub, s.changed, us);
    const std::size_t b1 = s.pub.bodies().size();
    {   // connection 0 holds unit 5 at slot 5: the body is copied, not written again
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> ci{5}, cs{5};
        s.units.changes(m, us, ci, cs);
        m.finish();
        if (s.out.size() <= xs::kSyncHead + 12) fail("no change op for a held unit", 8);
    }
    if (s.pub.bodies().size() != b1) fail("a body written again in its tick", 9);
}

// The ring: a dead mob's place is reused, a live one makes it double.
void ring() {
    using Conn = xs::XSyncConn<xs::XSyncFreeSlots<kUnit, 4>, xs::XSyncFreeSlots<kMob, 4>>;
    Server<Conn> s(2, 8, 4, 220, 1);
    s.mobs.clear();
    s.mobs = {Mob{0, 1, 1, 1}, Mob{1, 2, 2, 2}};
    s.ring.resize(4);
    auto& l = s.links[0];
    const typename Server<Conn>::MobSrc ms{&s};
    std::vector<std::int32_t> got;
    auto take = [&](std::size_t, std::uint32_t slot) { got.push_back(std::int32_t(slot)); };
    s.pub.begin_tick(++s.tick);
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{}, 1);
        std::vector<std::int32_t> r{0, 1};
        s.ring.enters(m, ms, r, take);
        m.finish();
    }
    if (s.ring.capacity() != 4) fail("ring size", 1);
    s.mobs = {Mob{1, 2, 2, 2}, Mob{4, 3, 3, 3}};   // 0 died; 4 wants its place
    s.pub.begin_tick(++s.tick);
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> r{1};
        s.ring.enters(m, ms, r, take);
        m.finish();
    }
    if (s.ring.capacity() != 4) fail("a dead mob's place not reused", 2);
    s.mobs.push_back(Mob{5, 4, 4, 4});   // 1 is alive: 5 makes the ring double
    s.pub.begin_tick(++s.tick);
    {
        xs::XSyncMessage<Conn> m(s.pub, l.hot, l.cold, s.out, Root{});
        std::vector<std::int32_t> r{2};
        s.ring.enters(m, ms, r, take);
        m.finish();
    }
    if (s.ring.capacity() != 8) fail("the ring did not double", 3);
}

} // namespace

int main() {
    pinned();
    ring();
    {   // hot stacks large enough
        using Conn = xs::XSyncConn<xs::XSyncFreeSlots<kUnit, 48>, xs::XSyncFreeSlots<kMob, 64>>;
        static_assert(sizeof(Conn) == 128 && alignof(Conn) == 128);
        Server<Conn> s(20261114, 300, 40, 60, 5);
        s.run(150);
        if (s.ring.capacity() < 8) fail("the ring never doubled", 0);
    }
    {   // stacks of two, more than 256 units held: slots past a byte, spills
        using Conn = xs::XSyncConn<xs::XSyncFreeSlots<kUnit, 2>, xs::XSyncFreeSlots<kMob, 2>>;
        Server<Conn> s(7, 700, 330, 120, 3);
        s.run(60);
    }
    std::puts("test_shared_delta_publisher: ok");
    return 0;
}
