// Map state demo: headless self-test (MAP_DEMO_DESIGN section 6.2). OWNER: common-library thread.
//
//   mapdemo_selftest [--csv-dir DIR]
//
// Separate from the timed runs and without observation hooks: evidence comes
// from public XBuffer APIs only (for_each_free_block offsets relative to the
// arena, arena()->used_end, free_block_count). Writes DIR/selftest_checks.csv
// (check,result,detail). Exit 0 all pass, 1 a check failed, 2 usage/IO error.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mapdemo/checks.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/std_model.hpp"

// ---- allocation probe -------------------------------------------------------
// Replaces the plain global operator new/delete so the frame-limit test can
// prove that a rejected header never sized a payload buffer.
namespace {
bool g_track = false;
std::size_t g_largest = 0;

void* counted_alloc(std::size_t n) {
    if (g_track && n > g_largest) g_largest = n;
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
} // namespace

void* operator new(std::size_t n) { return counted_alloc(n); }
void* operator new[](std::size_t n) { return counted_alloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace mapdemo;
using XB = XOffsetDatastructure::XBuffer;

namespace {

// ---- result rows ---------------------------------------------------------------

struct Row {
    std::string check;
    bool ok;
    std::string detail;
};
std::vector<Row> g_rows;

std::string csv_safe(std::string s) {
    std::replace(s.begin(), s.end(), ',', ';');
    std::replace(s.begin(), s.end(), '\n', ' ');
    return s;
}

void record(const std::string& check, bool ok, const std::string& detail) {
    g_rows.push_back(Row{check, ok, csv_safe(detail)});
    std::printf("%-28s %s  %s\n", check.c_str(), ok ? "pass" : "FAIL", detail.c_str());
    std::fflush(stdout);
}

// Collects failures inside one check; the first one becomes the detail.
struct Expect {
    std::string first;
    int failures = 0;
    void operator()(bool cond, const std::string& what) {
        if (cond) return;
        if (failures++ == 0) first = what;
    }
    bool ok() const { return failures == 0; }
};

std::string fmt(const char* f, auto... args) {
    char buf[512];
    std::snprintf(buf, sizeof buf, f, args...);
    return buf;
}

// ---- XBuffer helpers ---------------------------------------------------------------

std::uint32_t rel(const XB& b, const void* p) {
    return std::uint32_t(reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(b.get_address()));
}
std::uint64_t used_end(XB& b) { return b.arena()->used_end; }

std::vector<XB::WireHole> holes(const XB& b) {
    std::vector<XB::WireHole> v;
    b.for_each_free_block([&](XB::WireHole h) { v.push_back(h); });
    std::sort(v.begin(), v.end(), [](XB::WireHole a, XB::WireHole c) { return a.offset < c.offset; });
    return v;
}

// Hole table straight from the wire: [header][extents][payload][holes].
std::vector<XB::WireHole> wire_holes(const std::string& w) {
    XOffsetDatastructure::XWireHeaderV2 h{};
    std::memcpy(&h, w.data(), sizeof h);
    std::vector<XB::WireHole> v(h.hole_count);
    std::memcpy(v.data(), w.data() + w.size() - h.hole_count * sizeof(XB::WireHole), h.hole_count * sizeof(XB::WireHole));
    std::sort(v.begin(), v.end(), [](XB::WireHole a, XB::WireHole c) { return a.offset < c.offset; });
    return v;
}

bool same_holes(const std::vector<XB::WireHole>& a, const std::vector<XB::WireHole>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].offset != b[i].offset || a[i].bytes != b[i].bytes) return false;
    return true;
}

std::string holes_text(const std::vector<XB::WireHole>& hs) {
    std::string s;
    for (auto h : hs) s += fmt("[%u+%u)", h.offset, h.bytes);
    return s.empty() ? "none" : s;
}

const XB::WireHole* hole_containing(const std::vector<XB::WireHole>& hs, std::uint32_t off, std::uint32_t bytes) {
    for (const auto& h : hs)
        if (off >= h.offset && std::uint64_t(off) + bytes <= std::uint64_t(h.offset) + h.bytes) return &h;
    return nullptr;
}

XB::MaximumCapacity small_cap(std::size_t bytes) { return XB::maximum_capacity(bytes); }

// ---- 1. allocation / free / reuse across save and load ---------------------------

// via_reload = false: every round loads the wire into a new buffer
// (--buffers fresh). true: one buffer is saved and then reloaded from its own
// wire with reload_verified, as B does with --buffers reuse.
void test_reuse(bool via_reload) {
    const std::string prefix = via_reload ? "reload_round_" : "reuse_round_";
    Expect ex;
    XB b = XB::create<UnitStateBatch>(4096, cap_batch());
    auto& batch = b.root<UnitStateBatch>();
    batch.units.reserve(4);
    for (std::uint32_t i = 0; i < 2; ++i) {
        auto& u = batch.units.emplace_back();
        u.id = i;
        u.hp = 100;
        u.name = std::string_view(rules::short_name(i));   // inline
        u.path.reserve(4);
        for (int k = 0; k < 4; ++k) u.path.push_back(Point{float(10 * k), float(i)});
    }
    // Unit 0: inline -> 20-byte name (24 B block). Unit 1: path 4 -> 5 points
    // (new block, old 32 B block freed). Unit 0: 32-byte name cannot grow in
    // place (not at the tail) -> new block, old 24 B block freed.
    auto& u0 = batch.units[0];
    u0.name = std::string_view("U00000-long-marker-1");
    const std::uint32_t name_block0 = rel(b, u0.name.view().data());
    batch.units[1].path.push_back(Point{99, 1});
    u0.name = std::string_view("U00000-long-marker-number-two-xx");
    const auto src = holes(b);
    ex(hole_containing(src, name_block0, 24) != nullptr, "first name block was not freed");
    bool has24 = false, has32 = false;
    for (auto h : src) {
        has24 |= h.bytes == 24;
        has32 |= h.bytes == 32;
    }
    ex(has24 && has32, "source holes lack the 24 B and 32 B blocks: " + holes_text(src));
    if (!via_reload) record("reuse_setup", ex.ok(),
           ex.ok() ? fmt("source used_end=%llu free_count=%zu free_bytes=%llu holes=%s",
                         (unsigned long long)used_end(b), b.free_block_count(),
                         (unsigned long long)b.free_block_bytes(), holes_text(src).c_str())
                   : ex.first);

    for (int round = 1; round <= 3; ++round) {
        Expect rx;
        const auto before_save = holes(b);
        const std::string wire = b.save_verified<UnitStateBatch>();
        const auto wh = wire_holes(wire);
        rx(same_holes(wh, before_save), "wire hole table " + holes_text(wh) + " != in-memory " + holes_text(before_save));
        XB fresh;
        if (via_reload) b.reload_verified<UnitStateBatch>(wire);
        else fresh = XB::load_verified<UnitStateBatch>(wire, cap_batch());
        XB& in = via_reload ? b : fresh;
        const auto carried = holes(in);
        rx(same_holes(carried, before_save), "loaded holes " + holes_text(carried) + " != saved " + holes_text(before_save));
        const std::uint64_t end_before = used_end(in);
        const std::size_t count_before = in.free_block_count();
        auto& r = in.root<UnitStateBatch>();

        // A unit joins with an inline name and an empty path. Its first heap
        // name (19 chars -> exactly 24 B) and first 4-point path (exactly 32 B)
        // must land in holes carried over by the wire.
        auto& n = r.units.emplace_back();
        n.id = 2;
        n.hp = 77;
        n.name = std::string_view(rules::short_name(2));
        std::string nm = "U00002-round-" + std::to_string(round);
        nm.resize(19, 'x');
        n.name = std::string_view(nm);
        const std::uint32_t at_name = rel(in, n.name.view().data());
        n.path.reserve(4);
        for (int k = 0; k < 4; ++k) n.path.push_back(Point{float(round), float(k)});
        const std::uint32_t at_path = rel(in, n.path.data());
        const auto* hn = hole_containing(carried, at_name, 24);
        const auto* hp = hole_containing(carried, at_path, 32);
        rx(hn != nullptr, fmt("name block at %u is not inside a carried hole", at_name));
        rx(hp != nullptr, fmt("path block at %u is not inside a carried hole", at_path));
        const std::uint64_t end_after = used_end(in);
        const std::size_t count_after = in.free_block_count();
        rx(end_after == end_before, fmt("used_end grew %llu -> %llu", (unsigned long long)end_before,
                                        (unsigned long long)end_after));
        rx(count_after + 2 == count_before, fmt("free_block_count %zu -> %zu (expected -2)", count_before, count_after));

        const std::string detail = fmt(
            "round=%d wire=%zuB carried=%s name_block=%u in_hole=[%u+%u) path_block=%u in_hole=[%u+%u) "
            "used_end=%llu->%llu free_count=%zu->%zu",
            round, wire.size(), holes_text(carried).c_str(), at_name, hn ? hn->offset : 0u, hn ? hn->bytes : 0u,
            at_path, hp ? hp->offset : 0u, hp ? hp->bytes : 0u, (unsigned long long)end_before,
            (unsigned long long)end_after, count_before, count_after);

        // Next round: unit 2 leaves (its name and path blocks become holes);
        // unit 1's path grows by one point. Then verify every field through a
        // further save/load.
        r.units[1].path.push_back(Point{float(100 + round), 1});
        r.units.pop_back();
        const std::string w2 = in.save_verified<UnitStateBatch>();
        XB chk = XB::load_verified<UnitStateBatch>(w2, cap_batch());
        const auto& c = chk.root<UnitStateBatch>();
        rx(c.units.size() == 2, "unit count after the round is not 2");
        if (c.units.size() == 2) {
            rx(c.units[0].name.view() == "U00000-long-marker-number-two-xx", "unit 0 name changed");
            rx(c.units[1].name.view() == rules::short_name(1), "unit 1 name changed");
            rx(c.units[1].path.size() == std::size_t(5 + round), "unit 1 path size");
            rx(c.units[1].path.size() > 4 && c.units[1].path[4].x == 99.0f, "unit 1 path[4]");
            rx(c.units[1].path.size() == std::size_t(5 + round) &&
                   c.units[1].path[std::size_t(4 + round)].x == float(100 + round),
               "unit 1 last path point");
        }
        record(prefix + std::to_string(round), rx.ok(), rx.ok() ? detail : rx.first + " | " + detail);
        if (!via_reload) b = std::move(fresh);
    }
}

// ---- 1b. --buffers fresh and --buffers reuse send the same bytes ----------------

// The whole A <-> B chain in one process, both ways side by side.
//   fresh: A save_verified -> B load_verified, step, save_verified -> A load_verified (new buffers)
//   reuse: A save_verified_into -> B reload_verified (kept buffer), step,
//          save_verified_into -> A reload_verified into spare, swap with cur
// Every request and result wire must be byte-identical, tick by tick.
void test_buffer_modes(std::uint32_t units, std::uint64_t ticks) {
    Expect ex;
    const std::uint64_t seed = kDefaultSeed;
    XB f_cur = XB::create<UnitStateBatch>(4096 + std::size_t(units) * 128, cap_batch());
    rules::init_batch(f_cur.root<UnitStateBatch>(), units, seed);
    XB r_cur = XB::create<UnitStateBatch>(4096 + std::size_t(units) * 128, cap_batch());
    rules::init_batch(r_cur.root<UnitStateBatch>(), units, seed);
    XB r_spare, b_kept;
    bool r_spare_ready = false, b_kept_ready = false;
    std::string a_out, b_out;
    std::uint64_t compared = 0, last_digest = 0;
    for (std::uint64_t t = 0; t < ticks && ex.ok(); ++t) {
        // fresh
        const std::string f_req = f_cur.save_verified<UnitStateBatch>();
        XB fb = XB::load_verified<UnitStateBatch>(f_req, cap_batch());
        rules::step_batch(fb.root<UnitStateBatch>(), seed);
        const std::string f_res = fb.save_verified<UnitStateBatch>();
        f_cur = XB::load_verified<UnitStateBatch>(f_res, cap_batch());
        // reuse
        r_cur.save_verified_into<UnitStateBatch>(a_out);
        if (b_kept_ready) b_kept.reload_verified<UnitStateBatch>(a_out);
        else { b_kept = XB::load_verified<UnitStateBatch>(a_out, cap_batch()); b_kept_ready = true; }
        rules::step_batch(b_kept.root<UnitStateBatch>(), seed);
        b_kept.save_verified_into<UnitStateBatch>(b_out);
        if (r_spare_ready) r_spare.reload_verified<UnitStateBatch>(b_out);
        else { r_spare = XB::load_verified<UnitStateBatch>(b_out, cap_batch()); r_spare_ready = true; }
        r_cur.swap(r_spare);

        ex(f_req == a_out, fmt("tick %llu: request wires differ (%zu vs %zu bytes)", (unsigned long long)t,
                               f_req.size(), a_out.size()));
        ex(f_res == b_out, fmt("tick %llu: result wires differ (%zu vs %zu bytes)", (unsigned long long)(t + 1),
                               f_res.size(), b_out.size()));
        ++compared;
        last_digest = checks::wire_digest(f_res);
    }
    record("buffer_modes_same_bytes_" + std::to_string(units), ex.ok(),
           ex.ok() ? fmt("units=%u ticks=%llu request and result wires byte-identical, last result digest %016llx",
                         units, (unsigned long long)compared, (unsigned long long)last_digest)
                   : ex.first);
}

// ---- 2. independent output ---------------------------------------------------------

void test_independent_output() {
    Expect ex;
    XB b = XB::create<UnitStateBatch>(4096, cap_batch());
    auto& u = b.root<UnitStateBatch>().units.emplace_back();
    u.id = 7;
    u.hp = 90;
    u.name = std::string_view("U00007");
    u.path.push_back(Point{1, 2});
    const std::string w = b.save_verified<UnitStateBatch>();
    u.hp = 55;
    u.name = std::string_view("U00007-changed-after-save");
    u.path.push_back(Point{3, 4});
    u.path[0] = Point{5, 6};
    XB r = XB::load_verified<UnitStateBatch>(w, cap_batch());
    const auto& s = r.root<UnitStateBatch>().units[0];
    ex(s.hp == 90, "hp changed in the saved copy");
    ex(s.name.view() == "U00007", "name changed in the saved copy");
    ex(s.path.size() == 1 && s.path[0].x == 1.0f && s.path[0].y == 2.0f, "path changed in the saved copy");
    record("independent_output", ex.ok(),
           ex.ok() ? fmt("source changed after save; saved copy keeps hp=%d name=U00007 path=1 point", s.hp) : ex.first);
}

// ---- 3. failure inputs at the four receive points ----------------------------------

template <class T>
std::string expect_reject(Expect& ex, const char* what, const std::string& wire, XB::MaximumCapacity cap) {
    try {
        (void)XB::load_verified<T>(wire, cap);
        ex(false, std::string(what) + " was ACCEPTED");
        return std::string(what) + "=ACCEPTED";
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (msg.size() > 60) msg.resize(60);
        return std::string(what) + "=rejected(" + msg + ")";
    }
}

// `good` is a valid wire of T; `other` a valid wire of another root type.
template <class T>
void failure_inputs(const char* point, const std::string& good, const std::string& other, XB::MaximumCapacity real_cap) {
    Expect ex;
    std::string detail;
    try {
        (void)XB::load_verified<T>(good, real_cap);
    } catch (const std::exception& e) {
        ex(false, std::string("valid wire rejected: ") + e.what());
    }
    detail += expect_reject<T>(ex, "truncated_1", good.substr(0, good.size() - 1), real_cap) + " ";
    detail += expect_reject<T>(ex, "header_only", good.substr(0, sizeof(XOffsetDatastructure::XWireHeaderV2)), real_cap) + " ";
    detail += expect_reject<T>(ex, "empty", std::string(), real_cap) + " ";
    detail += expect_reject<T>(ex, "wrong_schema", other, real_cap) + " ";
    // used_bytes over the receiver's maximum_capacity (a cap one byte below the wire's used size).
    XOffsetDatastructure::XWireHeaderV2 h{};
    std::memcpy(&h, good.data(), sizeof h);
    detail += expect_reject<T>(ex, "over_cap", good, small_cap(std::size_t(h.used_bytes) - 1)) + " ";
    // A header that claims a huge used size.
    std::string forged = good;
    h.used_bytes = 0xfffffff8u;
    std::memcpy(forged.data(), &h, sizeof h);
    detail += expect_reject<T>(ex, "forged_used_bytes", forged, real_cap);
    record(std::string("failure_inputs_") + point, ex.ok(), ex.ok() ? detail : ex.first + " | " + detail);
}

std::string batch_wire(std::uint32_t units) {
    XB b = XB::create<UnitStateBatch>(4096, cap_batch());
    rules::init_batch(b.root<UnitStateBatch>(), units, kDefaultSeed);
    return b.save_verified<UnitStateBatch>();
}

std::string catalog_wire() {
    XB b = XB::create<MapCatalog>(4096, cap_catalog());
    rules::build_catalog(b.root<MapCatalog>(), kDefaultSeed);
    return b.save_verified<MapCatalog>();
}

std::string view_request_wire(std::uint64_t id, Rect rect) {
    XB b = XB::create<ViewRequest>(256, cap_view_request());
    auto& r = b.root<ViewRequest>();
    r.request_id = id;
    r.scene_id = kSceneId;
    r.rect = rect;
    return b.save_verified<ViewRequest>();
}

// Snapshot built the way A builds one: filter, reserve exact, copy fields.
template <class Batch>
std::string snapshot_wire(const Batch& batch, std::uint64_t request_id, const Rect& rect) {
    std::vector<std::size_t> hits;
    for (std::size_t i = 0; i < batch.units.size(); ++i)
        if (batch.units[i].scene_id == kSceneId && rules::contains(rect, batch.units[i].pos)) hits.push_back(i);
    XB s = XB::create<ViewSnapshot>(4096 + hits.size() * 48, cap_snapshot());
    auto& v = s.root<ViewSnapshot>();
    v.request_id = request_id;
    v.tick = batch.tick;
    v.scene_id = kSceneId;
    v.rect = rect;
    v.units.reserve(std::uint32_t(hits.size()));
    for (std::size_t i : hits) {
        const auto& u = batch.units[i];
        auto& o = v.units.emplace_back();
        o.id = u.id;
        o.hp = u.hp;
        o.pos = u.pos;
        o.behavior = u.behavior;
        o.name = u.name.view();
    }
    return s.save_verified<ViewSnapshot>();
}

void test_failure_inputs() {
    const std::string batch = batch_wire(100);
    const std::string catalog = catalog_wire();
    const std::string request = view_request_wire(1, Rect{{0, 0}, {1024, 768}});
    XB bb = XB::load_verified<UnitStateBatch>(batch, cap_batch());
    const std::string snapshot = snapshot_wire(bb.root<UnitStateBatch>(), 1, scene_bounds());
    failure_inputs<UnitStateBatch>("batch", batch, snapshot, cap_batch());
    failure_inputs<ViewSnapshot>("snapshot", snapshot, batch, cap_snapshot());
    failure_inputs<MapCatalog>("catalog", catalog, request, cap_catalog());
    failure_inputs<ViewRequest>("view_request", request, snapshot, cap_view_request());
}

// ---- 4. frame layer: limits checked before the payload is allocated -----------------

void write_all(int fd, const void* p, std::size_t n) {
    const char* c = static_cast<const char*>(p);
    while (n > 0) {
        const ssize_t w = ::write(fd, c, n);
        if (w <= 0) throw std::runtime_error("write failed");
        c += w;
        n -= std::size_t(w);
    }
}

struct SocketPair {
    int a = -1, b = -1;
    SocketPair() {
        int fds[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) throw std::runtime_error("socketpair failed");
        a = fds[0];
        b = fds[1];
    }
};

std::string frame_case(Expect& ex, const char* what, net::FrameHeader h, bool nonblocking,
                       std::initializer_list<net::MsgType> allowed) {
    SocketPair sp;
    net::Conn rx(sp.a);
    write_all(sp.b, &h, sizeof h);
    if (nonblocking) rx.set_nonblocking(true);
    g_largest = 0;
    g_track = true;
    std::string out;
    try {
        if (nonblocking) {
            std::optional<net::Frame> f;
            for (int i = 0; i < 100 && !f; ++i) f = rx.recv_nowait(allowed);
        } else {
            (void)rx.recv(allowed);
        }
        g_track = false;
        ex(false, std::string(what) + " was ACCEPTED");
        out = std::string(what) + "=ACCEPTED";
    } catch (const net::ProtocolError& e) {
        g_track = false;
        // The rejection itself allocates only small strings (the message).
        ex(g_largest < 4096, fmt("%s: largest allocation during receive %zu B", what, g_largest));
        out = fmt("%s=rejected(largest_alloc=%zuB)", what, g_largest);
    }
    ::close(sp.b);
    return out;
}

void test_frame_limits() {
    Expect ex;
    std::string detail;
    using net::FrameHeader;
    using net::MsgType;
    const auto hdr = [](MsgType t, std::uint32_t len, std::uint32_t magic = net::kMagic) {
        return FrameHeader{magic, std::uint16_t(t), 0, len, 0, 1};
    };
    // Limits table.
    ex(net::max_payload(MsgType::ViewReq) == kCapViewRequest && net::max_payload(MsgType::ViewSnapshot) == kCapSnapshot &&
           net::max_payload(MsgType::StepReq) == kCapBatch && net::max_payload(MsgType::StepRes) == kCapBatch &&
           net::max_payload(MsgType::Catalog) == kCapCatalog && net::max_payload(MsgType::Hello) == 32 &&
           net::max_payload(MsgType::HelloOk) == 0 && net::max_payload(MsgType::Bye) == 0 &&
           net::max_payload(MsgType::Error) == 4096,
       "max_payload table differs from INTERFACES section 4");
    for (bool nb : {false, true}) {
        detail += nb ? "nonblocking: " : "blocking: ";
        detail += frame_case(ex, "viewreq_1GiB", hdr(MsgType::ViewReq, 1u << 30), nb, {MsgType::ViewReq}) + " ";
        detail += frame_case(ex, "viewreq_4KiB+1", hdr(MsgType::ViewReq, 4097), nb, {MsgType::ViewReq}) + " ";
        detail += frame_case(ex, "step_64MiB+1", hdr(MsgType::StepRes, std::uint32_t(kCapBatch + 1)), nb, {MsgType::StepRes}) + " ";
        detail += frame_case(ex, "bad_magic", hdr(MsgType::ViewReq, 16, 0x12345678u), nb, {MsgType::ViewReq}) + " ";
        detail += frame_case(ex, "unexpected_type", hdr(MsgType::StepReq, 16), nb, {MsgType::ViewReq}) + " ";
        detail += frame_case(ex, "unknown_type", hdr(MsgType(77), 16), nb, {MsgType::ViewReq}) + " ";
    }
    // Exactly-at-limit and an Error frame from the peer.
    {
        SocketPair sp;
        net::Conn tx(sp.b), rx(sp.a);
        const std::string at_limit(std::size_t(kCapViewRequest), 'r');
        tx.send(MsgType::ViewReq, 9, at_limit, 2);
        net::Frame f = rx.recv({MsgType::ViewReq});
        ex(f.payload == at_limit && f.correlation == 9 && f.flags == 2, "4 KiB ViewReq payload not delivered intact");
        tx.send(MsgType::Error, 0, "nope");
        try {
            (void)rx.recv({MsgType::ViewReq});
            ex(false, "unexpected Error frame was returned");
        } catch (const net::ProtocolError& e) {
            ex(std::string(e.what()) == "peer error: nope", std::string("error text: ") + e.what());
        }
        tx.send(MsgType::Error, 0, "listed");
        net::Frame ef = rx.recv({MsgType::ViewReq, MsgType::Error});
        ex(ef.type == MsgType::Error && ef.payload == "listed", "listed Error frame not returned");
        try {
            tx.send(MsgType::ViewReq, 0, std::string(4097, 'x'));
            ex(false, "sender accepted an oversize payload");
        } catch (const net::ProtocolError&) {
        }
        detail += "at_limit=ok peer_error=ok send_oversize=refused";
    }
    record("frame_limits", ex.ok(), ex.ok() ? detail : ex.first + " | " + detail);
}

// ---- 5. rect rule -------------------------------------------------------------------

void test_rect_rule() {
    Expect ex;
    const Rect scene = scene_bounds();
    const Rect r{{100, 200}, {300, 400}};
    ex(rules::contains(r, Point{100, 200}), "min corner must be inside");
    ex(!rules::contains(r, Point{300, 300}), "max x edge must be outside");
    ex(!rules::contains(r, Point{200, 400}), "max y edge must be outside");
    ex(rules::contains(r, Point{299.99f, 399.99f}), "just below max must be inside");
    ex(!rules::contains(r, Point{99.99f, 300}), "just below min must be outside");
    const Rect zero{{50, 50}, {50, 50}};
    ex(rules::rect_is_legal(zero, scene), "zero-area rect must be legal");
    ex(!rules::contains(zero, Point{50, 50}), "zero-area rect contains nothing");
    ex(rules::rect_is_legal(scene, scene), "scene bounds must be legal");
    ex(rules::rect_is_legal(empty_probe_rect(), scene), "empty probe rect must be legal");
    ex(!rules::contains(empty_probe_rect(), Point{1000, kMaxCoord}), "empty probe rect must exclude y = 4095");
    ex(!rules::rect_is_legal(Rect{{-1, 0}, {10, 10}}, scene), "rect left of the scene must be illegal");
    ex(!rules::rect_is_legal(Rect{{0, 0}, {4097, 10}}, scene), "rect right of the scene must be illegal");
    ex(!rules::rect_is_legal(Rect{{10, 10}, {5, 20}}, scene), "min > max must be illegal");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    ex(!rules::rect_is_legal(Rect{{nan, 0}, {10, 10}}, scene), "NaN must be illegal");
    ex(!rules::rect_is_legal(Rect{{0, 0}, {inf, 10}}, scene), "inf must be illegal");
    ex(!rules::rect_is_legal(Rect{{-inf, 0}, {10, 10}}, scene), "-inf must be illegal");
    record("rect_rule", ex.ok(), ex.ok() ? "16 boundary/legality cases" : ex.first);
}

// ---- 6. rules: XOffset path (save/load like B and A) vs std reference ----------------

void test_rules(std::uint32_t units, std::uint64_t steps) {
    Expect ex;
    XB cur = XB::create<UnitStateBatch>(4096, cap_batch());
    rules::init_batch(cur.root<UnitStateBatch>(), units, kDefaultSeed);
    stdm::UnitStateBatch ref;
    rules::init_batch(ref, units, kDefaultSeed);
    checks::Coverage cov;
    auto inv = checks::batch_invariants(cur.root<UnitStateBatch>(), units, 0);
    ex(inv.ok, "initial invariants: " + inv.detail);
    auto cmp = checks::compare_with_reference(cur.root<UnitStateBatch>(), ref);
    ex(cmp.ok, "initial reference: " + cmp.detail);
    std::size_t max_wire = 0;
    std::uint64_t compared = 0;
    for (std::uint64_t t = 0; t < steps && ex.ok(); ++t) {
        // A -> B
        const std::string req = cur.save_verified<UnitStateBatch>();
        XB at_b = XB::load_verified<UnitStateBatch>(req, cap_batch());
        rules::step_batch(at_b.root<UnitStateBatch>(), kDefaultSeed);
        // B -> A
        const std::string res = at_b.save_verified<UnitStateBatch>();
        max_wire = std::max(max_wire, res.size());
        XB next = XB::load_verified<UnitStateBatch>(res, cap_batch());
        rules::step_batch(ref, kDefaultSeed);
        const auto& nb = next.root<UnitStateBatch>();
        inv = checks::batch_invariants(nb, units, t + 1);
        ex(inv.ok, fmt("tick %llu invariants: ", (unsigned long long)(t + 1)) + inv.detail);
        cmp = checks::compare_with_reference(nb, ref);
        ex(cmp.ok, fmt("tick %llu reference: ", (unsigned long long)(t + 1)) + cmp.detail);
        ++compared;
        cov.observe(cur.root<UnitStateBatch>(), nb);
        cur = std::move(next);
    }
    ex(cov.complete(), "modification coverage incomplete");
    // Snapshot path at the final tick: full, normal, empty.
    std::string snaps;
    const Rect rects[3] = {scene_bounds(), Rect{{512, 512}, {512 + kViewWidth, 512 + kViewHeight}}, empty_probe_rect()};
    for (int i = 0; i < 3 && ex.ok(); ++i) {
        const std::uint64_t rid = 100 + std::uint64_t(i);
        const std::string w = snapshot_wire(cur.root<UnitStateBatch>(), rid, rects[i]);
        XB s = XB::load_verified<ViewSnapshot>(w, cap_snapshot());
        const auto& v = s.root<ViewSnapshot>();
        XB rq = XB::load_verified<ViewRequest>(view_request_wire(rid, rects[i]), cap_view_request());
        auto sc = checks::compare_snapshot(v, rq.root<ViewRequest>(), ref);
        ex(sc.ok, "compare_snapshot: " + sc.detail);
        auto ss = checks::snapshot_sanity(v, rid, rects[i]);
        ex(ss.ok, "snapshot_sanity: " + ss.detail);
        if (i == 0) ex(v.units.size() == units, "full-scene snapshot misses units");
        if (i == 2) ex(v.units.size() == 0, "empty probe snapshot is not empty");
        snaps += fmt(" %s=%u", i == 0 ? "full" : i == 1 ? "normal" : "empty", unsigned(v.units.size()));
    }
    const std::string detail =
        fmt("units=%u ticks=%llu compared=%llu bitwise max_wire=%zuB name_to_long=%llu name_to_short=%llu "
            "path_grow=%llu path_shrink=%llu hp_changes=%llu behavior_changes=%llu grow_shrink_grow=%d",
            units, (unsigned long long)steps, (unsigned long long)compared, max_wire,
            (unsigned long long)cov.name_to_long, (unsigned long long)cov.name_to_short,
            (unsigned long long)cov.path_grow, (unsigned long long)cov.path_shrink,
            (unsigned long long)cov.hp_changes, (unsigned long long)cov.behavior_changes, int(cov.grow_shrink_grow)) +
        " visible:" + snaps;
    record("rules_vs_reference_" + std::to_string(units), ex.ok(), ex.ok() ? detail : ex.first + " | " + detail);
}

// ---- 7. checks catch faults; digest is stable and sensitive ----------------------------

void test_checks_detect_faults() {
    Expect ex;
    XB b = XB::create<UnitStateBatch>(4096, cap_batch());
    auto& batch = b.root<UnitStateBatch>();
    rules::init_batch(batch, 3, kDefaultSeed);
    stdm::UnitStateBatch ref;
    rules::init_batch(ref, 3, kDefaultSeed);
    ex(checks::compare_with_reference(batch, ref).ok, "identical batches compare unequal");
    ex(checks::batch_invariants(batch, 3, 0).ok, "valid batch fails invariants");

    batch.units[1].pos.x = std::nextafter(batch.units[1].pos.x, 5000.0f);
    ex(!checks::compare_with_reference(batch, ref).ok, "1-ulp position change not detected");
    batch.units[1].pos = ref.units[1].pos;
    batch.units[2].name = std::string_view("U00009");
    ex(!checks::compare_with_reference(batch, ref).ok, "name change not detected");
    ex(!checks::batch_invariants(batch, 3, 0).ok, "foreign name not detected by invariants");
    batch.units[2].name = std::string_view(ref.units[2].name);
    batch.units[0].behavior = Behavior(7);
    ex(!checks::batch_invariants(batch, 3, 0).ok, "out-of-range behavior not detected");
    batch.units[0].behavior = Behavior::Patrol;
    batch.units[0].pos.y = std::numeric_limits<float>::quiet_NaN();
    ex(!checks::batch_invariants(batch, 3, 0).ok, "NaN position not detected");
    batch.units[0].pos = ref.units[0].pos;
    ex(!checks::batch_invariants(batch, 3, 1).ok, "wrong tick not detected");
    ex(!checks::batch_invariants(batch, 4, 0).ok, "wrong unit count not detected");
    ex(checks::compare_with_reference(batch, ref).ok, "restored batch compares unequal");

    // Snapshot checks: duplicate id, outside rect, digest sensitivity.
    const Rect full = scene_bounds();
    const std::string w = snapshot_wire(batch, 5, full);
    XB s1 = XB::load_verified<ViewSnapshot>(w, cap_snapshot());
    XB s2 = XB::load_verified<ViewSnapshot>(w, cap_snapshot());
    const std::uint64_t d1 = checks::snapshot_digest(s1.root<ViewSnapshot>());
    ex(d1 == checks::snapshot_digest(s2.root<ViewSnapshot>()), "digest differs for equal snapshots");
    auto& v2 = s2.root<ViewSnapshot>();
    v2.units[0].hp += 1;
    ex(d1 != checks::snapshot_digest(v2), "digest ignores hp");
    v2.units[0].hp -= 1;
    v2.units[1].id = v2.units[0].id;
    ex(!checks::snapshot_sanity(v2, 5, full).ok, "duplicate id not detected by snapshot_sanity");
    XB rq = XB::load_verified<ViewRequest>(view_request_wire(5, full), cap_view_request());
    ex(!checks::compare_snapshot(v2, rq.root<ViewRequest>(), ref).ok, "duplicate id not detected by compare_snapshot");
    ex(!checks::snapshot_sanity(s1.root<ViewSnapshot>(), 6, full).ok, "wrong request id not detected");
    const Rect small{{0, 0}, {1, 1}};
    ex(!checks::snapshot_sanity(s1.root<ViewSnapshot>(), 5, small).ok, "units outside the rect not detected");
    // in_buffer.
    const auto& v1 = s1.root<ViewSnapshot>();
    ex(checks::in_buffer(s1, &v1.units[0], sizeof(VisibleUnit)), "snapshot unit not reported inside its buffer");
    ex(!checks::in_buffer(s2, &v1.units[0], sizeof(VisibleUnit)), "unit reported inside another buffer");
    ex(!checks::in_buffer(s1, &ex, 1), "stack object reported inside the buffer");
    record("checks_detect_faults", ex.ok(), ex.ok() ? fmt("14 injected faults detected; digest=%016llx", (unsigned long long)d1) : ex.first);
}

// ---- 8. net round trip between two processes ------------------------------------------

// Child = a minimal B: handshake, then load / step / save / reply until Bye.
[[noreturn]] void child_b(int fd) {
    int code = 0;
    try {
        net::Conn c(fd);
        net::Frame h = c.recv({net::MsgType::Hello});
        if (!net::hello_matches(h.payload)) {
            c.send(net::MsgType::Error, 0, "schema mismatch");
            std::_Exit(2);
        }
        c.send(net::MsgType::HelloOk, 0, {});
        for (;;) {
            net::Frame f = c.recv({net::MsgType::StepReq, net::MsgType::Bye});
            if (f.type == net::MsgType::Bye) break;
            XB batch = XB::load_verified<UnitStateBatch>(f.payload, cap_batch());
            auto& root = batch.root<UnitStateBatch>();
            rules::step_batch(root, kDefaultSeed);
            const std::string out = batch.save_verified<UnitStateBatch>();
            c.send(net::MsgType::StepRes, root.tick, out);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: selftest child: %s\n", e.what());
        code = 2;
    }
    std::_Exit(code);
}

void test_net_two_process() {
    Expect ex;
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        record("net_two_process", false, "socketpair failed");
        return;
    }
    std::fflush(nullptr);
    const pid_t pid = ::fork();
    if (pid < 0) {
        record("net_two_process", false, "fork failed");
        return;
    }
    if (pid == 0) {
        ::close(fds[0]);
        child_b(fds[1]);
    }
    ::close(fds[1]);
    const std::uint32_t units = 10000;   // ~1 MB per batch: forces partial writes
    const std::uint64_t steps = 25;
    std::string detail;
    try {
        net::Conn c(fds[0]);
        c.send(net::MsgType::Hello, 0, net::hello_payload());
        (void)c.recv({net::MsgType::HelloOk});
        XB cur = XB::create<UnitStateBatch>(4096, cap_batch());
        rules::init_batch(cur.root<UnitStateBatch>(), units, kDefaultSeed);
        stdm::UnitStateBatch ref;
        rules::init_batch(ref, units, kDefaultSeed);
        c.set_nonblocking(true);   // A's mode: enqueue / flush / try_recv driven by poll
        for (std::uint64_t t = 0; t < steps && ex.ok(); ++t) {
            c.enqueue(net::MsgType::StepReq, t, cur.save_verified<UnitStateBatch>());
            std::optional<net::Frame> f;
            while (!f) {
                pollfd p{int(c.fd()), short(POLLIN | (c.want_write() ? POLLOUT : 0)), 0};
                ::poll(&p, 1, 5000);
                if (c.want_write()) c.flush();
                f = c.recv_nowait({net::MsgType::StepRes});
            }
            ex(f->correlation == t + 1, "StepRes correlation is not tick + 1");
            XB next = XB::load_verified<UnitStateBatch>(f->payload, cap_batch());
            rules::step_batch(ref, kDefaultSeed);
            auto cmp = checks::compare_with_reference(next.root<UnitStateBatch>(), ref);
            ex(cmp.ok, fmt("tick %llu: ", (unsigned long long)(t + 1)) + cmp.detail);
            cur = std::move(next);
        }
        c.enqueue(net::MsgType::Bye, 0, {});
        while (!c.flush()) {
            pollfd p{int(c.fd()), POLLOUT, 0};
            ::poll(&p, 1, 5000);
        }
        detail = fmt("units=%u steps=%llu bytes_sent=%llu bytes_received=%llu", units, (unsigned long long)steps,
                     (unsigned long long)c.bytes_sent(), (unsigned long long)c.bytes_received());
    } catch (const std::exception& e) {
        ex(false, std::string("parent: ") + e.what());
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    ex(WIFEXITED(status) && WEXITSTATUS(status) == 0, fmt("child exit status %d", status));
    record("net_two_process", ex.ok(), ex.ok() ? detail : ex.first + " | " + detail);
}

void run_guarded(const char* name, void (*fn)()) {
    try {
        fn();
    } catch (const std::exception& e) {
        record(name, false, std::string("exception: ") + e.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string csv_dir = ".";
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--csv-dir" && i + 1 < argc) {
            csv_dir = argv[++i];
        } else {
            std::fprintf(stderr, "error: usage: mapdemo_selftest [--csv-dir DIR]\n");
            return 2;
        }
    }

    run_guarded("reuse", [] { test_reuse(false); });
    run_guarded("reload", [] { test_reuse(true); });
    run_guarded("buffer_modes_same_bytes_100", [] { test_buffer_modes(100, 300); });
    run_guarded("buffer_modes_same_bytes_10000", [] { test_buffer_modes(10000, 200); });
    run_guarded("independent_output", test_independent_output);
    run_guarded("failure_inputs", test_failure_inputs);
    run_guarded("frame_limits", test_frame_limits);
    run_guarded("rect_rule", test_rect_rule);
    run_guarded("rules_vs_reference_1", [] { test_rules(1, 500); });
    run_guarded("rules_vs_reference_100", [] { test_rules(100, 500); });
    run_guarded("rules_vs_reference_10000", [] { test_rules(10000, 200); });
    run_guarded("checks_detect_faults", test_checks_detect_faults);
    run_guarded("net_two_process", test_net_two_process);

    metrics::CsvFile csv;
    const std::string path = csv_dir + "/selftest_checks.csv";
    if (!csv.open(path, "check,result,detail")) {
        std::fprintf(stderr, "error: cannot write %s\n", path.c_str());
        return 2;
    }
    int failed = 0;
    for (const Row& r : g_rows) {
        csv.row(r.check + "," + (r.ok ? "pass" : "fail") + "," + r.detail);
        failed += r.ok ? 0 : 1;
    }
    std::printf("selftest: %zu checks, %d failed\n", g_rows.size(), failed);
    return failed == 0 ? 0 : 1;
}
