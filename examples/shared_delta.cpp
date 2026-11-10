// Shared deltas from a server to its clients over a trusted link
// (docs/SHARED_DELTA.md). The server compares each unit once per tick
// against one shared baseline of it; every client that holds the unit gets
// the same change bytes. Each client sees the units inside its own range of
// x, holds them in slots of its own view, and applies the messages with
// XReceiver<XLink::from_server>::apply_sync through a target that maps unit
// ids to its slots.
//
// Client B joins two ticks late (a key message), then loses a message: the
// next one is rejected with delta_base_mismatch, B asks for a key message and
// is back in step one tick later.
#include "../xoffset_receiver.hpp"

#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

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

static XBuffer make_view() {
    return XBuffer::create<View>(4096, XBuffer::maximum_capacity(1 << 20).with_growth(XGrowPolicy::Automatic));
}

// ---- server -------------------------------------------------------------------

struct ClientState {
    std::int32_t x0, x1;         // the client sees units with x0 <= x < x1
    std::uint64_t stream;        // one stream per connection
    std::uint32_t version = 0;
    bool need_key = true;        // joining, or asked for one
    std::set<std::uint32_t> held;
    ClientState(std::int32_t lo, std::int32_t hi, std::uint64_t s) : x0(lo), x1(hi), stream(s) {}
};

struct Server {
    XBuffer world = make_view();
    XBuffer base = make_view();  // the shared baseline: what every holder of a unit holds (same slots)
    xs::XSyncOut bodies, key, msg;
    std::vector<std::pair<std::size_t, std::size_t>> body;   // per slot: this tick's change (offset, length)
    std::pair<std::size_t, std::size_t> head_body{0, 0};

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

    // Once per tick, whatever the number of clients: each record is compared
    // once and the baseline catches up.
    void diff_all() {
        bodies.clear();
        auto put = [&](auto& now, auto& before) {
            const std::size_t at = bodies.size();
            const bool any = xs::XRecordDelta<std::remove_cvref_t<decltype(now)>>::diff(now, before, bodies);
            return std::pair<std::size_t, std::size_t>(at, any ? bodies.size() - at : 0);
        };
        head_body = put(w().head, b().head);
        body.resize(w().units.size());
        for (std::size_t i = 0; i < w().units.size(); ++i) body[i] = put(w().units[i], b().units[i]);
    }

    // The client's message for this tick: leaves, enters (the unit from
    // cleared) and the shared change bodies of the units it keeps.
    std::string message(ClientState& c) {
        View& v = w();
        xs::XSyncWriter m(msg, c.need_key ? xs::kFlagKey : 0, c.stream, ++c.version);
        auto enter = [&](unsigned table, std::uint32_t id, const auto& now) {
            key.clear();
            xs::XRecordDelta<std::remove_cvref_t<decltype(now)>>::key(now, key);
            m.op(table, xs::enter, id, key.data(), key.size());
        };
        if (c.need_key) {
            c.held.clear();
            enter(kHead, 0, v.head);
        } else if (head_body.second) {
            m.op(kHead, xs::change, 0, bodies.data() + head_body.first, head_body.second);
        }
        std::set<std::uint32_t> now;
        for (const Unit& u : v.units)
            if (u.x >= c.x0 && u.x < c.x1) now.insert(u.id);
        for (std::uint32_t id : c.held)
            if (!now.count(id)) m.op(kUnit, xs::leave, id);
        for (std::size_t i = 0; i < v.units.size(); ++i) {
            const Unit& u = v.units[i];
            if (!now.count(u.id)) continue;
            if (!c.held.count(u.id)) enter(kUnit, u.id, u);
            else if (body[i].second) m.op(kUnit, xs::change, u.id, bodies.data() + body[i].first, body[i].second);
        }
        m.finish();
        c.held = std::move(now);
        c.need_key = false;
        return std::string(msg.view());
    }
};

// ---- client -------------------------------------------------------------------

// The records apply_sync writes: the head, and units in slots of the view
// (a slot that leaves is cleared and reused).
struct ViewTarget {
    using tables = std::tuple<Head, Unit>;
    template <unsigned K> static constexpr bool keyed = K == kUnit;

    XBuffer* buf;
    std::map<std::uint32_t, std::int64_t> slot;   // unit id -> index in units
    std::vector<std::int64_t> free;

    View& v() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kHead) return id == 0 ? 0 : -1;
        else {
            const auto it = slot.find(id);
            return it == slot.end() ? -1 : it->second;
        }
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kHead) return v().head;
        else return v().units[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {
        if (slot.count(id)) return -1;
        std::int64_t h;
        if (!free.empty()) {
            h = free.back();
            free.pop_back();
        } else {
            h = std::int64_t(v().units.size());
            v().units.resize(v().units.size() + 1);
        }
        slot.emplace(id, h);
        return h;
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t h) {
        xs::XRecordDelta<Unit>::clear(v().units[std::size_t(h)]);   // free slots read as empty
        slot.erase(id);
        free.push_back(h);
    }
};

struct Client {
    const char* name;
    XBuffer view = make_view();
    xs::XSyncCursor cursor;
    ViewTarget t{&view, {}, {}};

    explicit Client(const char* n) : name(n) {}

    XStatus receive(XReceiver<XLink::from_server>& rx, std::string_view m) {
        if (xs::is_key(m)) {
            // A key message enters every unit: start from no units.
            for (const auto& [id, h] : t.slot) xs::XRecordDelta<Unit>::clear(t.v().units[std::size_t(h)]);
            t.slot.clear();
            t.free.clear();
            for (std::size_t s = t.v().units.size(); s-- > 0;) t.free.push_back(std::int64_t(s));
        }
        return rx.apply_sync(m, cursor, t);
    }

    // Whether the client holds exactly the server's units in its range.
    bool matches(Server& s, const ClientState& c) {
        if (t.v().head.tick != s.w().head.tick) return false;
        std::size_t n = 0;
        for (const Unit& u : s.w().units) {
            if (u.x < c.x0 || u.x >= c.x1) continue;
            ++n;
            const std::int64_t h = t.find<kUnit>(u.id);
            if (h < 0) return false;
            const Unit& mine = t.v().units[std::size_t(h)];
            if (mine.id != u.id || mine.x != u.x || mine.hp != u.hp || mine.name.view() != u.name.view()) return false;
        }
        return n == t.slot.size();
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
        server.diff_all();

        std::string ma = server.message(sa);
        const XStatus ra = a.receive(rx, ma);
        std::printf("tick %d: A %s %3zu bytes -> %s, holds %zu units\n", tick, xs::is_key(ma) ? "key  " : "delta", ma.size(),
                    ra ? "applied" : ra.error().message.c_str(), a.t.slot.size());
        ok = ok && ra && a.matches(server, sa);

        if (tick < 3) continue;   // B connects at tick 3
        std::string mb = server.message(sb);
        if (tick == 5) {
            std::printf("tick %d: B's message is lost\n", tick);
            continue;
        }
        const XStatus rb = b.receive(rx, mb);
        std::printf("tick %d: B %s %3zu bytes -> %s, holds %zu units\n", tick, xs::is_key(mb) ? "key  " : "delta", mb.size(),
                    rb ? "applied" : to_string(rb.code()), b.t.slot.size());
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
