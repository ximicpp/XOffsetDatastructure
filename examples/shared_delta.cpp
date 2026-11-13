// Shared deltas from a server to its clients over a trusted link
// (docs/SHARED_DELTA.md). The server encodes each changed unit once per
// tick; every client that holds the unit gets a copy of the same bytes.
// Each client sees the units inside its own range of x. The server gives
// each client's units their slots in that client's view and writes the slot
// as the op's id, so the client indexes its view directly (no id lookup);
// it applies the messages with XReceiver<XLink::from_server>::apply_sync.
//
// Client B joins two ticks late (a key message), then loses a message: the
// next one is rejected with delta_base_mismatch, B asks for a key message and
// is back in step one tick later.
#include "../xoffset_receiver.hpp"

#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

// The root record (table 0): carried whole by every message.
struct Head {
    std::uint64_t tick;
};
XOFFSET_RECORD(Head, tick)

struct Unit {
    std::uint32_t id;
    std::int32_t x;
    std::int32_t hp;
    XString name;
};
XOFFSET_RECORD(Unit, id, x, hp, name)

struct View {
    Head head;
    XVector<Unit> units;
};
XOFFSET_RECORD(View, head, units)
XOFFSET_REGISTER_SCHEMA_NAME(View, "example.SharedView")

// Table numbers: part of the protocol between server and client.
enum : unsigned { kHead = 0, kUnit = 1 };
using UnitBody = xs::XRecordBody<Unit>;

static XBuffer make_view() {
    return XBuffer::create<View>(4096, XBuffer::maximum_capacity(1 << 20).with_growth(XGrowPolicy::Automatic));
}

// ---- server -------------------------------------------------------------------

struct ClientState {
    std::int32_t x0, x1;                    // the client sees units with x0 <= x < x1
    std::uint64_t stream;                   // one stream per connection
    std::uint32_t version = 0;
    bool need_key = true;                   // joining, or asked for one
    std::map<std::uint32_t, std::uint32_t> held;   // unit id -> its slot in the client's view
    std::vector<std::uint32_t> free;        // slots free in the client's view
    std::uint32_t used = 0;                 // slots given out so far
    ClientState(std::int32_t lo, std::int32_t hi, std::uint64_t s) : x0(lo), x1(hi), stream(s) {}

    std::uint32_t take() {
        if (free.empty()) return used++;
        const std::uint32_t s = free.back();
        free.pop_back();
        return s;
    }
};

struct Server {
    XBuffer world = make_view();
    XBuffer base = make_view();   // the shared baseline: what every holder of a unit holds (same slots)
    xs::XSyncOut bodies, msg;     // this tick's ops (id, body), shared by every message
    struct Ops { std::size_t change_at = 0, change_len = 0, whole_at = 0, whole_len = 0; };
    std::vector<Ops> ops;         // per world slot: this tick's ops

    View& w() { return world.root<View>(); }
    View& b() { return base.root<View>(); }

    void spawn(std::uint32_t id, std::int32_t x, const char* name) {
        w().units.emplace_back();
        Unit& u = w().units.back();
        u.id = id;
        u.x = x;
        u.hp = 100;
        u.name = name;
        b().units.emplace_back();   // cleared: nobody holds the unit yet
    }

    // Once per tick, whatever the number of clients: each unit's change (the
    // members that differ from the baseline) and its whole body are encoded
    // once, and the baseline catches up.
    void encode_all() {
        bodies.clear();
        ops.assign(w().units.size(), Ops{});
        for (std::size_t i = 0; i < w().units.size(); ++i) {
            Unit& u = w().units[i];
            Ops& o = ops[i];
            if (const std::uint64_t mask = UnitBody::changed(u, b().units[i])) {
                o.change_at = bodies.size();
                o.change_len = xs::put_change_op(bodies, u.id, u, mask);
                UnitBody::apply_change(bodies.data() + o.change_at + 4, b().units[i]);
            }
            o.whole_at = bodies.size();
            o.whole_len = xs::put_whole_op(bodies, u.id, u);
        }
    }

    // The client's message for this tick: leaves, changes and enters (whole
    // bodies), each a copy of the shared op under the client's slot.
    std::string message(ClientState& c) {
        View& v = w();
        std::optional<xs::XSyncWriter> m;
        if (c.need_key) {
            m.emplace(msg, v.head, ++c.version, c.stream);
            c.held.clear();
            c.free.clear();
            c.used = 0;
        } else {
            m.emplace(msg, v.head, ++c.version);
        }
        std::set<std::uint32_t> now;
        for (const Unit& u : v.units)
            if (u.x >= c.x0 && u.x < c.x1) now.insert(u.id);
        std::vector<std::uint32_t> gone;   // slots of the units that left
        for (auto it = c.held.begin(); it != c.held.end();) {
            if (now.count(it->first)) { ++it; continue; }
            gone.push_back(it->second);
            it = c.held.erase(it);
        }
        m->leaves(kUnit, gone.data(), gone.size());
        // Units that stay and changed: the shared change op.
        m->begin(kUnit, xs::change);
        for (std::size_t i = 0; i < v.units.size(); ++i) {
            const auto it = c.held.find(v.units[i].id);
            if (it == c.held.end() || !ops[i].change_len) continue;
            m->op(bodies.data() + ops[i].change_at, ops[i].change_len, it->second);
        }
        m->end();
        // Units that entered the client's range: the shared whole op, in a slot of its own.
        m->begin(kUnit, xs::enter);
        for (std::size_t i = 0; i < v.units.size(); ++i) {
            const Unit& u = v.units[i];
            if (!now.count(u.id) || c.held.count(u.id)) continue;
            const std::uint32_t slot = c.take();
            c.held.emplace(u.id, slot);
            m->op(bodies.data() + ops[i].whole_at, ops[i].whole_len, slot);
        }
        m->end();
        m->finish();
        // A slot that left is given out again from the next message on: an
        // entry appears at most once in a message.
        c.free.insert(c.free.end(), gone.begin(), gone.end());
        c.need_key = false;
        return std::string(msg.view());
    }
};

// ---- client -------------------------------------------------------------------

// The records apply_sync writes: the head, and units at the slots the server
// gave them (a slot that leaves is cleared and reused).
struct ViewTarget {
    using tables = std::tuple<Head, Unit>;
    template <unsigned K> static constexpr bool keyed = K == kUnit;

    XBuffer* buf;
    std::vector<bool> held;   // slot held (the full check asks; admit takes the server's word)

    View& v() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kHead) return id == 0 ? 0 : -1;
        else return id < held.size() && held[id] ? std::int64_t(id) : -1;
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kHead) return v().head;
        else return v().units[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t slot) {
        if (slot >= v().units.size()) {
            v().units.resize(std::size_t(slot) + 1);
            held.resize(std::size_t(slot) + 1);
        }
        held[slot] = true;
        return std::int64_t(slot);
    }
    template <unsigned K> void leave(std::uint32_t slot, std::int64_t h) {
        UnitBody::clear(v().units[std::size_t(h)]);   // free slots read as empty
        held[slot] = false;
    }
};

struct Client {
    const char* name;
    XBuffer view = make_view();
    xs::XSyncCursor cursor;
    ViewTarget t{&view, {}};

    explicit Client(const char* n) : name(n) {}

    std::size_t units() const {
        std::size_t n = 0;
        for (bool h : t.held) n += h;
        return n;
    }

    XStatus receive(XReceiver<XLink::from_server>& rx, std::string_view m) {
        if (xs::is_key(m)) {
            // A key message enters every unit: start from no units.
            for (std::size_t s = 0; s < t.held.size(); ++s)
                if (t.held[s]) UnitBody::clear(t.v().units[s]);
            t.held.assign(t.held.size(), false);
        }
        return rx.apply_sync(m, cursor, t);
    }

    // Whether the client holds exactly the server's units in its range.
    bool matches(Server& s, const ClientState& c) {
        if (t.v().head.tick != s.w().head.tick) return false;
        std::map<std::uint32_t, const Unit*> mine;
        for (std::size_t slot = 0; slot < t.held.size(); ++slot)
            if (t.held[slot]) mine.emplace(t.v().units[slot].id, &t.v().units[slot]);
        std::size_t n = 0;
        for (const Unit& u : s.w().units) {
            if (u.x < c.x0 || u.x >= c.x1) continue;
            ++n;
            const auto it = mine.find(u.id);
            if (it == mine.end() || !UnitBody::same(*it->second, u)) return false;
        }
        return n == mine.size();
    }
};

int main() {
    Server server;
    const char* names[] = {"scout", "archer", "knight", "mage", "rider", "guard"};
    for (std::uint32_t i = 0; i < 6; ++i) server.spawn(100 + i, std::int32_t(i) * 10, names[i]);

    ClientState sa(0, 30, 0xA11CE), sb(20, 60, 0xB0B);
    Client a("A"), b("B");
    XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));

    bool ok = true;
    for (int tick = 1; tick <= 8; ++tick) {
        // Server: units move and fight, in place.
        View& v = server.w();
        v.head.tick = std::uint64_t(tick);
        for (std::size_t i = 0; i < v.units.size(); ++i) v.units[i].x += (i % 2) ? 4 : -3;
        v.units[std::size_t(tick % 6)].hp -= 9;
        if (tick == 4) v.units[2].name = "knight-captain";
        server.encode_all();

        std::string ma = server.message(sa);
        const XStatus ra = a.receive(rx, ma);
        std::printf("tick %d: A %s %3zu bytes -> %s, holds %zu units\n", tick, xs::is_key(ma) ? "key  " : "delta", ma.size(),
                    ra ? "applied" : ra.error().message.c_str(), a.units());
        ok = ok && ra && a.matches(server, sa);

        if (tick < 3) continue;   // B connects at tick 3
        std::string mb = server.message(sb);
        if (tick == 5) {
            std::printf("tick %d: B's message is lost\n", tick);
            continue;
        }
        const XStatus rb = b.receive(rx, mb);
        std::printf("tick %d: B %s %3zu bytes -> %s, holds %zu units\n", tick, xs::is_key(mb) ? "key  " : "delta", mb.size(),
                    rb ? "applied" : to_string(rb.code()), b.units());
        if (!rb) {
            // The view is a version behind: nothing changed. Ask for a key message.
            ok = ok && rb.code() == XErrc::delta_base_mismatch && tick == 6;
            sb.need_key = true;
            continue;
        }
        ok = ok && b.matches(server, sb);
    }
    std::printf("%s\n", ok ? "both clients hold the server's units in their range" : "MISMATCH");
    return ok ? 0 : 1;
}
