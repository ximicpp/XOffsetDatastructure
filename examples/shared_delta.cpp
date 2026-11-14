// Shared deltas from a server to its clients over a trusted link
// (docs/SHARED_DELTA.md). The server encodes each changed unit once per
// tick; every client that holds the unit gets a copy of the same bytes.
// Each client sees the units inside its own range of x. The server gives
// each client's units their slots in that client's view and writes the slot
// as the op's id, so the client indexes its view directly (no id lookup);
// it applies the messages with XReceiver<XLink::from_server>::apply_sync.
// The server side is the publisher (xoffset_shared_publisher.hpp): a
// dirty-bit table for the units, a connection state per client, and one
// message per client and tick.
//
// Client B joins two ticks late (a key message), then loses a message: the
// next one is rejected with delta_base_mismatch, B asks for a key message and
// is back in step one tick later.
#include "../xoffset_receiver.hpp"
#include "../xoffset_shared_publisher.hpp"

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

// The publisher (xoffset_shared_publisher.hpp) keeps what the server needs
// per connection: the version and the client's free slots (a hot part of a
// cache line or two, and a cold part for the stream and the rest).
using Conn = xs::XSyncConn<xs::XSyncFreeSlots<kUnit, 16>>;   // up to 16 free slots in the hot part
using Units = xs::XSyncDirtyTable<kUnit, Unit>;
using Message = xs::XSyncMessage<Conn>;

struct ClientState {
    std::int32_t x0, x1;                    // the client sees units with x0 <= x < x1
    std::uint64_t stream;                   // one stream per connection
    bool need_key = true;                   // joining, or asked for one
    Conn conn{};
    Conn::cold_type cold;
    std::map<std::int32_t, std::int32_t> held;   // world index -> the slot enters() gave (contract 4)
    ClientState(std::int32_t lo, std::int32_t hi, std::uint64_t s) : x0(lo), x1(hi), stream(s) {}
};

struct Server {
    XBuffer world = make_view();
    XBuffer base = make_view();   // the images: what every holder of a unit has
    xs::XSyncPublisher pub;
    Units units;
    xs::XSyncOut msg;
    std::vector<std::uint64_t> dirty;   // per world index: this tick's dirty members (bit i: member i)

    View& w() { return world.root<View>(); }
    View& b() { return base.root<View>(); }

    // The world as the table's source. A light view: the publisher passes it
    // to its slow paths by value.
    struct Src {
        Server* s;
        std::uint64_t dirty(std::uint32_t i) const { return s->dirty[i]; }
        Unit& image(std::uint32_t i) const { return s->b().units[i]; }
        void load(std::uint32_t i, Unit& r, std::uint64_t m) const {
            const Unit& u = s->w().units[i];
            if (m & 1) r.id = u.id;
            if (m & 2) r.x = u.x;
            if (m & 4) r.hp = u.hp;
            if (m & 8) r.name = std::string_view(u.name);
        }
    };

    void spawn(std::uint32_t id, std::int32_t x, const char* name) {
        w().units.emplace_back();
        Unit& u = w().units.back();
        u.id = id;
        u.x = x;
        u.hp = 100;
        u.name = name;
        b().units.emplace_back();   // the image starts empty: nobody holds the unit yet
        dirty.push_back(0xF);       // its four members differ from the empty image
        units.resize(w().units.size());
    }
    void set_x(std::size_t i, std::int32_t x) { w().units[i].x = x; dirty[i] |= 2; }
    void set_hp(std::size_t i, std::int32_t hp) { w().units[i].hp = hp; dirty[i] |= 4; }
    void set_name(std::size_t i, const char* n) { w().units[i].name = n; dirty[i] |= 8; }

    // Once per tick, before its first message, whatever the number of
    // clients: each changed unit that somebody holds is encoded once.
    void publish(std::uint64_t tick) {
        w().head.tick = tick;
        pub.begin_tick(std::uint32_t(tick));
        std::vector<std::int32_t> changed;
        for (std::size_t i = 0; i < dirty.size(); ++i)
            if (dirty[i]) changed.push_back(std::int32_t(i));
        units.publish(pub, changed, Src{this});
        dirty.assign(dirty.size(), 0);   // taken: publish reads each bit once
    }

    // The client's message for this tick: leaves, changes, enters, each a
    // copy of the shared op under the client's slot. The publisher gives the
    // slots, writes the sections and the version.
    std::string message(ClientState& c) {
        const bool key = c.need_key;
        if (key) {
            // A key message: the connection starts over, so it releases every
            // unit it held (contract 3) and every unit in range enters.
            for (const auto& held : c.held) units.release(std::uint32_t(held.first));
            c.held.clear();
        }
        std::vector<std::int32_t> stay, stay_slot, gone, gone_slot, enter;
        for (const auto& [i, slot] : c.held) {
            const Unit& u = w().units[std::size_t(i)];
            const bool in = u.x >= c.x0 && u.x < c.x1;
            (in ? stay : gone).push_back(i);
            (in ? stay_slot : gone_slot).push_back(slot);
        }
        for (const std::int32_t i : gone) c.held.erase(i);
        for (std::size_t i = 0; i < w().units.size(); ++i) {
            const Unit& u = w().units[i];
            if (u.x >= c.x0 && u.x < c.x1 && !c.held.count(std::int32_t(i))) enter.push_back(std::int32_t(i));
        }
        std::optional<Message> m;
        if (key) m.emplace(pub, c.conn, c.cold, msg, w().head, c.stream);
        else m.emplace(pub, c.conn, c.cold, msg, w().head);
        units.leaves(*m, gone, gone_slot);
        units.changes(*m, Src{this}, stay, stay_slot);
        units.enters(*m, Src{this}, enter, [&](std::size_t r, std::uint32_t slot) { c.held[enter[r]] = std::int32_t(slot); });
        m->finish();
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
        // Server: units move and fight, in place; each change marks its member dirty.
        View& v = server.w();
        for (std::size_t i = 0; i < v.units.size(); ++i) server.set_x(i, v.units[i].x + ((i % 2) ? 4 : -3));
        server.set_hp(std::size_t(tick % 6), v.units[std::size_t(tick % 6)].hp - 9);
        if (tick == 4) server.set_name(2, "knight-captain");
        server.publish(std::uint64_t(tick));

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
