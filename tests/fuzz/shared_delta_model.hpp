// A small shared-delta (XS 2.0) model for test_shared_delta_sync and
// fuzz_shared_delta: a server that writes messages with XSyncWriter and a
// receiver target for XReceiver::apply_sync.
//
// Tables: 0 the view's head (the root record), 1 units (keyed by id), 2
// cells (fixed, id = index). The server keeps its records in one buffer and
// its baseline (what the client was last sent) in another, and writes unit
// bodies into a body store first (as a server shares them between clients);
// a Replica is a client view (its own buffer, slots and id index) with its
// cursor.
#ifndef XOFFSET_TESTS_SHARED_DELTA_MODEL_HPP
#define XOFFSET_TESTS_SHARED_DELTA_MODEL_HPP

#include "../../xoffset_receiver.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sdm {
namespace xo = XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

struct Pos { std::int32_t x{}; std::int32_t y{}; };
struct Head { std::int64_t tick{}; std::uint32_t seq{}; bool paused{}; };
struct Unit {
    std::uint32_t id{};
    std::int32_t hp{};
    bool alive{};
    Pos pos;
    xo::XString name;
    xo::XVector<std::uint16_t> buffs;
    xo::XVector<Pos> path;
    std::uint8_t level{};
    Pos ring[2];
};
struct Cell { std::int32_t terrain{}; std::int64_t owner{}; bool lit{}; };
struct View {
    Head head;
    xo::XVector<Unit> units;
    xo::XVector<Cell> cells;
    xo::XString junk;
};
} // namespace sdm
XOFFSET_RECORD(sdm::Pos, x, y)
XOFFSET_RECORD(sdm::Head, tick, seq, paused)
XOFFSET_RECORD(sdm::Unit, id, hp, alive, pos, name, buffs, path, level, ring)
XOFFSET_RECORD(sdm::Cell, terrain, owner, lit)
XOFFSET_RECORD(sdm::View, head, units, cells, junk)
XOFFSET_REGISTER_SCHEMA_NAME(sdm::View, "test.shared_delta.view")

namespace sdm {
enum : unsigned { kHead = 0, kUnit = 1, kCell = 2 };
inline constexpr std::size_t kCells = 6;

inline xo::XBuffer make_view(std::size_t junk = 0) {
    auto b = xo::XBuffer::create<View>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(xo::XGrowPolicy::Automatic));
    // Another arena position for the client's blocks.
    if (junk) b.root<View>().junk = std::string_view(std::string(junk, 'j'));
    b.root<View>().cells.resize(kCells);
    return b;
}

// The receiver's records: a view buffer, unit slots by id (a slot that
// leaves is reused), cells by index.
struct Target {
    using tables = std::tuple<Head, Unit, Cell>;
    template <unsigned K> static constexpr bool keyed = K == kUnit;
    xo::XBuffer* buf = nullptr;
    std::map<std::uint32_t, std::int64_t> idx;
    std::vector<std::int64_t> free;
    std::size_t applied_ops = 0;

    View& view() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kHead) return id == 0 ? 0 : -1;
        else if constexpr (K == kUnit) {
            const auto it = idx.find(id);
            return it == idx.end() ? -1 : it->second;
        } else return id < view().cells.size() ? std::int64_t(id) : -1;
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kHead) return view().head;
        else if constexpr (K == kUnit) return view().units[std::size_t(h)];
        else return view().cells[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {
        if (idx.count(id)) return -1;   // admit asks enter() for an id held
        std::int64_t s;
        if (!free.empty()) { s = free.back(); free.pop_back(); }
        else { s = std::int64_t(view().units.size()); view().units.resize(view().units.size() + 1); }
        idx.emplace(id, s);
        return s;
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t h) {
        idx.erase(id);
        free.push_back(h);
    }
    template <unsigned K> void applied(std::uint32_t, std::int64_t) { ++applied_ops; }
};

// A client: its view, its target and its cursor.
struct Replica {
    xo::XBuffer buf;
    Target t;
    xs::XSyncCursor cur;
    explicit Replica(std::size_t junk = 40) : buf(make_view(junk)) { t.buf = &buf; }
    Replica(const Replica& o) : buf(o.buf.clone()), t(o.t), cur(o.cur) { t.buf = &buf; }
    Replica& operator=(const Replica&) = delete;
    // Empties the keyed table (before a key message): every slot is free.
    void reset() {
        View& v = t.view();
        for (const auto& e : t.idx) xs::XRecordBody<Unit>::clear(v.units[std::size_t(e.second)]);
        t.idx.clear();
        t.free.clear();
        for (std::size_t s = v.units.size(); s-- > 0;) t.free.push_back(std::int64_t(s));
    }
    bool verifies() {
        std::string w;
        buf.save_verified_into<View>(w);
        return bool(xo::XBuffer::try_load_verified<View>(w));
    }
};

template <class T> bool same(const T& a, const T& b) { return xs::XRecordBody<T>::same(a, b); }

// Whether two replicas hold the same records (by id; slots may differ).
inline bool same_state(Replica& a, Replica& b) {
    View& x = a.t.view();
    View& y = b.t.view();
    if (!same(x.head, y.head) || a.t.idx.size() != b.t.idx.size()) return false;
    for (std::size_t i = 0; i < kCells; ++i) if (!same(x.cells[i], y.cells[i])) return false;
    for (const auto& [id, h] : a.t.idx) {
        const auto it = b.t.idx.find(id);
        if (it == b.t.idx.end() || !same(x.units[std::size_t(h)], y.units[std::size_t(it->second)])) return false;
    }
    return a.cur.empty() == b.cur.empty() && (a.cur.empty() || (a.cur.version() == b.cur.version() && a.cur.stream() == b.cur.stream()));
}

// The server: its records (units by server slot, ids in `ids`), the
// baseline of one client, and the stream it writes.
struct Server {
    xo::XBuffer now = make_view();
    xo::XBuffer base = make_view();
    std::vector<std::uint32_t> ids;     // the id of each live server slot
    std::vector<std::uint32_t> left;    // ids that left since the last message
    std::vector<std::uint32_t> entered; // ids that entered since the last message
    std::uint64_t stream = 0x5d3;
    std::uint32_t version = 0;
    std::uint32_t next_id = 1;
    xs::XSyncOut msg, body;
    bool changes_first = false;   // section order of the next delta (any order applies)

    View& v() { return now.root<View>(); }
    View& b() { return base.root<View>(); }
    std::size_t slot_of(std::uint32_t id) const {
        return std::size_t(std::find(ids.begin(), ids.end(), id) - ids.begin());
    }

    template <class Pick> void mutate_unit(Unit& u, Pick& pick) {
        const unsigned changes = 1 + pick(3);
        for (unsigned i = 0; i < changes; ++i) {
            switch (pick(10)) {
            case 0: u.hp = std::int32_t(pick(1000)) - 200; break;
            case 1: u.alive = !u.alive; break;
            case 2: u.pos.x += 1; u.pos.y = std::int32_t(pick(64)); break;
            case 3: {
                std::string s(pick(4) == 0 ? 17 + pick(20) : pick(9), 'a');
                for (auto& c : s) c = char('a' + pick(26));
                u.name = std::string_view(s);
                break;
            }
            case 4: {
                u.buffs.resize(pick(6));
                for (auto& x : u.buffs) x = std::uint16_t(pick(60000));
                break;
            }
            case 5: if (!u.buffs.empty()) u.buffs[pick(unsigned(u.buffs.size()))] ^= 0x55; break;
            case 6: {
                u.path.resize(pick(5));
                for (auto& p : u.path) { p.x = std::int32_t(pick(100)); p.y = -std::int32_t(pick(100)); }
                break;
            }
            case 7: if (!u.path.empty()) u.path[pick(unsigned(u.path.size()))].x += 7; break;
            case 8: u.level = std::uint8_t(pick(256)); break;
            default: u.ring[pick(2)].y += 3; break;
            }
        }
    }
    // One step of the world, driven by pick(n) in [0, n).
    template <class Pick> void step(Pick& pick) {
        const unsigned what = pick(8);
        if (what == 0 && ids.size() < 12) {   // a unit enters
            const std::uint32_t id = pick(4) == 0 ? 0x10000 + pick(0x30000) : next_id++;
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) return;
            if (std::find(left.begin(), left.end(), id) != left.end()) return;   // no leave and enter in one message
            ids.push_back(id);
            v().units.resize(ids.size());
            b().units.resize(ids.size());
            Unit& u = v().units[ids.size() - 1];
            xs::XRecordBody<Unit>::clear(u);
            u.id = id;
            mutate_unit(u, pick);
            entered.push_back(id);
        } else if (what == 1 && !ids.empty()) {   // a unit leaves (its slot takes the last one)
            const std::size_t s = pick(unsigned(ids.size()));
            const std::uint32_t id = ids[s];
            const auto e = std::find(entered.begin(), entered.end(), id);
            if (e != entered.end()) entered.erase(e);   // entered and left: never sent
            else left.push_back(id);
            const std::size_t last = ids.size() - 1;
            if (s != last) {
                copy(v().units[s], v().units[last]);
                copy(b().units[s], b().units[last]);
                ids[s] = ids[last];
            }
            ids.pop_back();
            v().units.resize(ids.size());
            b().units.resize(ids.size());
        } else if (what == 2) {
            Cell& c = v().cells[pick(unsigned(kCells))];
            c.terrain = std::int32_t(pick(5));
            c.owner += 1;
            c.lit = pick(2);
        } else if (what == 3) {
            v().head.tick += 1;
            v().head.paused = pick(2);
            changes_first = pick(2);
        } else if (!ids.empty()) {
            mutate_unit(v().units[pick(unsigned(ids.size()))], pick);
        }
    }
    bool is_entered(std::uint32_t id) const { return std::find(entered.begin(), entered.end(), id) != entered.end(); }
    // dst := src through a whole body (every plain byte and container).
    template <class T> void copy(T& dst, const T& src) {
        body.clear();
        xs::put_whole_op(body, 0, src);
        xs::XRecordBody<T>::apply_whole(body.data() + 4, dst);
    }
    // Unit s's enter op, shared: written into the body store, then copied
    // into the message under its id; the baseline takes the record.
    void put_enter(xs::XSyncWriter& m, std::size_t s) {
        body.clear();
        const std::size_t n = xs::put_whole_op(body, 0xffffffffu, v().units[s]);
        m.op(body.data(), n, ids[s]);
        xs::XRecordBody<Unit>::apply_whole(body.data() + 4, b().units[s]);
    }
    // Unit s's change op of the members that differ from the baseline (none:
    // no op); the baseline takes them.
    void put_change(xs::XSyncWriter& m, std::size_t s) {
        const std::uint64_t mask = xs::XRecordBody<Unit>::changed(v().units[s], b().units[s]);
        if (!mask) return;
        body.clear();
        const std::size_t n = xs::put_change_op(body, ids[s], v().units[s], mask);
        m.op(body.data(), n);
        xs::XRecordBody<Unit>::apply_change(body.data() + 4, b().units[s]);
    }
    // A key message: the head, then every record enters.
    std::string_view key() {
        v().head.seq = version + 1;
        xs::XSyncWriter m(msg, v().head, ++version, stream);
        b().head = v().head;
        m.begin(kUnit, xs::enter);
        for (std::size_t s = 0; s < ids.size(); ++s) put_enter(m, s);
        m.end();
        m.begin(kCell, xs::enter);
        for (std::size_t i = 0; i < kCells; ++i) {
            m.whole(std::uint32_t(i), v().cells[i]);
            b().cells[i] = v().cells[i];
        }
        m.end();
        m.finish();
        left.clear();
        entered.clear();
        return msg.view();
    }
    // The next message: the head, leaves, units (enter, change), cells.
    std::string_view delta() {
        v().head.seq = version + 1;
        xs::XSyncWriter m(msg, v().head, ++version);
        b().head = v().head;
        m.leaves(kUnit, left.data(), left.size());
        const auto enters = [&] {
            m.begin(kUnit, xs::enter);
            for (std::size_t s = 0; s < ids.size(); ++s) if (is_entered(ids[s])) put_enter(m, s);
            m.end();
        };
        const auto changes = [&] {
            m.begin(kUnit, xs::change);
            for (std::size_t s = 0; s < ids.size(); ++s) if (!is_entered(ids[s])) put_change(m, s);
            m.end();
        };
        if (changes_first) { changes(); enters(); }
        else { enters(); changes(); }
        m.begin(kCell, xs::change);
        for (std::size_t i = 0; i < kCells; ++i) m.diff(std::uint32_t(i), v().cells[i], b().cells[i]);
        m.end();
        m.finish();
        left.clear();
        entered.clear();
        return msg.view();
    }
    // Whether replica r holds exactly the server's records.
    bool matches(Replica& r) {
        View& x = r.t.view();
        if (!same(v().head, x.head) || r.t.idx.size() != ids.size()) return false;
        for (std::size_t i = 0; i < kCells; ++i) if (!same(v().cells[i], x.cells[i])) return false;
        for (std::size_t s = 0; s < ids.size(); ++s) {
            const auto it = r.t.idx.find(ids[s]);
            if (it == r.t.idx.end() || !same(v().units[s], x.units[std::size_t(it->second)])) return false;
        }
        return true;
    }
};

using FromServer = xo::XReceiver<xo::XLink::from_server>;
inline FromServer& receiver() {
    static FromServer rx(xo::XBuffer::maximum_capacity(std::size_t(1) << 22));
    return rx;
}
inline xo::XStatus apply_full(Replica& r, std::string_view m) {
    return receiver().apply_sync<xo::XCheck::full>(m, r.cur, r.t);
}
inline xo::XStatus apply_admit(Replica& r, std::string_view m) {
    return receiver().apply_sync<xo::XCheck::admit>(m, r.cur, r.t);
}

} // namespace sdm

#endif
