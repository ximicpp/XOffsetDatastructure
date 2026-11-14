// The shared-delta publisher's checks (xoffset_shared_publisher.hpp): the
// two every build has (a tick that does not grow, publish after the tick's
// first message) and those of XOFFSET_SYNC_CHECKS builds (a slot the
// connection does not hold or that another entity holds, a slot twice in a
// message, a slot left twice, a message used across ticks, a leave or release
// of a unit nobody holds, a holder that missed a change body: contract 2).
// Each throws XException(invalid_state) and the next case starts afresh.
#define XOFFSET_SYNC_CHECKS 1
#include "xoffset_shared_publisher.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

namespace pe {
struct Root { std::int64_t tick{}; };
struct Unit { std::int32_t hp{}; std::int32_t x{}; };
} // namespace pe
XOFFSET_RECORD(pe::Root, tick)
XOFFSET_RECORD(pe::Unit, hp, x)

namespace {
using namespace pe;
namespace xs = XOffsetDatastructure::xsync;
using XErrc = XOffsetDatastructure::XErrc;
using Conn = xs::XSyncConn<xs::XSyncFreeSlots<1, 8>>;
using Msg = xs::XSyncMessage<Conn>;
using Table = xs::XSyncDirtyTable<1, Unit, 64, 64>;

int failures = 0;

struct World {
    std::vector<Unit> now = std::vector<Unit>(8), img = std::vector<Unit>(8);
    std::vector<std::uint64_t> bits = std::vector<std::uint64_t>(8);
    std::uint64_t dirty(std::uint32_t i) const { return bits[i]; }
    Unit& image(std::uint32_t i) { return img[i]; }
    void load(std::uint32_t i, Unit& r, std::uint64_t m) { if (m & 1) r.hp = now[i].hp; if (m & 2) r.x = now[i].x; }
};

struct Setup {
    xs::XSyncPublisher pub;
    Table units;
    World w;
    Conn a{}, b{};
    Conn::cold_type ac, bc;
    xs::XSyncOut out;
    std::uint32_t tick = 1;
    Setup() {
        units.resize(8);
        pub.begin_tick(tick);
        std::vector<std::int32_t> all{0, 1, 2, 3};
        Msg m(pub, a, ac, out, Root{}, 1);   // connection a holds units 0..3 at slots 0..3
        units.enters(m, w, all, [](std::size_t, std::uint32_t) {});
        m.finish();
    }
    void next() { pub.begin_tick(++tick); }
    std::vector<std::int32_t> v(std::initializer_list<std::int32_t> x) { return x; }
};

void expect_throw(const char* what, const std::function<void(Setup&)>& f) {
    Setup s;
    try {
        f(s);
    } catch (const XOffsetDatastructure::XException& e) {
        if (e.code() != XErrc::invalid_state) { std::fprintf(stderr, "FAIL %s: wrong code (%s)\n", what, e.what()); ++failures; }
        return;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL %s: %s\n", what, e.what());
        ++failures;
        return;
    }
    std::fprintf(stderr, "FAIL %s: no error\n", what);
    ++failures;
}

} // namespace

int main() {
    expect_throw("a tick that does not grow", [](Setup& s) { s.pub.begin_tick(s.tick); });
    expect_throw("tick 0xFFFFFFFF", [](Setup& s) { s.pub.begin_tick(0xFFFFFFFFu); });
    expect_throw("publish after the first message", [](Setup& s) {
        std::vector<std::int32_t> c{0};
        s.units.publish(s.pub, c, s.w);
    });
    expect_throw("a slot not held", [](Setup& s) {
        s.next();
        Msg m(s.pub, s.a, s.ac, s.out, Root{});
        s.units.changes(m, s.w, s.v({0}), s.v({6}));
    });
    expect_throw("a slot held by another unit", [](Setup& s) {
        s.next();
        Msg m(s.pub, s.a, s.ac, s.out, Root{});
        s.units.changes(m, s.w, s.v({0, 1}), s.v({1, 0}));
    });
    expect_throw("a slot twice in a message", [](Setup& s) {
        s.next();
        Msg m(s.pub, s.a, s.ac, s.out, Root{});
        s.units.changes(m, s.w, s.v({2}), s.v({2}));
        s.units.leaves(m, s.v({2}), s.v({2}));
    });
    expect_throw("a slot left twice", [](Setup& s) {
        s.next();
        { Msg m(s.pub, s.a, s.ac, s.out, Root{}); s.units.leaves(m, s.v({3}), s.v({3})); m.finish(); }
        s.next();
        { Msg m(s.pub, s.a, s.ac, s.out, Root{}); s.units.leaves(m, s.v({3}), s.v({3})); m.finish(); }
    });
    expect_throw("a message used across ticks", [](Setup& s) {
        s.next();
        Msg m(s.pub, s.a, s.ac, s.out, Root{});
        s.next();
        s.units.changes(m, s.w, s.v({0}), s.v({0}));
    });
    expect_throw("a release of a unit nobody holds", [](Setup& s) { s.units.release(7); });
    expect_throw("a holder missed a change body (verify)", [](Setup& s) {
        s.next();
        s.w.now[1].hp = 5; s.w.bits[1] = 1;
        std::vector<std::int32_t> c{1};
        s.units.publish(s.pub, c, s.w);
        { Msg m(s.pub, s.a, s.ac, s.out, Root{}); m.finish(); }   // connection a holds unit 1 and gets nothing
        s.units.verify(s.pub);
    });
    expect_throw("a holder missed a change body (next first use)", [](Setup& s) {
        s.next();
        s.w.now[1].hp = 5; s.w.bits[1] = 1;
        std::vector<std::int32_t> c{1};
        s.units.publish(s.pub, c, s.w);
        s.w.bits[1] = 0;
        s.next();
        s.w.now[1].hp = 6; s.w.bits[1] = 1;
        s.units.publish(s.pub, c, s.w);
    });
    {   // no error: every holder got the body; a leave counts
        Setup s;
        s.next();
        s.w.now[1].hp = 5; s.w.bits[1] = 1;
        std::vector<std::int32_t> c{1};
        s.units.publish(s.pub, c, s.w);
        { Msg m(s.pub, s.a, s.ac, s.out, Root{}); s.units.changes(m, s.w, s.v({1}), s.v({1})); m.finish(); }
        s.units.verify(s.pub);
        s.next();
        s.w.now[2].hp = 5; s.w.bits[2] = 1;
        c = {2};
        s.units.publish(s.pub, c, s.w);
        { Msg m(s.pub, s.a, s.ac, s.out, Root{}); s.units.leaves(m, s.v({2}), s.v({2})); m.finish(); }
        s.units.verify(s.pub);
    }
    if (failures) return 1;
    std::puts("test_shared_delta_publisher_errors: ok");
    return 0;
}
