// Record bodies of shared deltas (xoffset_shared_delta.hpp, XS 2.0).
//
// One server record set, its shared baseline, and a client that holds the
// same records in another layout (other slots, other arena positions).
// Random changes to scalars, bools, nested records, arrays, inline and heap
// strings, vectors, and a nested record that holds a string: every
// change body the server writes (from dirty member flags, as a game's) checks
// and applies on the client, after which the client record holds the server
// record's content; changed() names no member outside the flags. Whole
// bodies set a record that holds anything else. The client buffer passes
// load_verified throughout. Corrupted bodies (flipped, cut, extended) are
// either rejected by check_change / check_whole or, when accepted, leave a
// buffer that still verifies. Bools other than 0 and 1, mask bits past the
// members or containers, and lengths past the end are rejected. put_diff
// writes the bytes changed() + put_change write. Then XSyncWriter: the bytes
// of a message (head, root, sections; ops copied, under another id, up to N
// bytes, in reserved room; encoded in place). No try/catch: also built as
// test_shared_delta_noexc.
#include "xoffsetdatastructure.hpp"
#include "xoffset_shared_delta.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

namespace sdt {
struct Inner { std::int32_t a{}; bool f{}; std::int64_t b{}; };
struct Elem { std::int32_t k{}; bool on{}; std::int16_t s{}; };
struct Bag { std::int16_t n{}; xo::XString label; bool on{}; };
struct Rec {
    std::int32_t id{};
    bool flag{};
    double d{};
    Inner in;
    xo::XString name;
    xo::XVector<Elem> list;
    xo::XVector<std::int32_t> nums;
    xo::XString tag;
    std::uint8_t small{};
    Inner arr[2];
    Bag bag;
};
struct Holder { std::int64_t tick{}; xo::XVector<Rec> recs; xo::XString junk; };
} // namespace sdt
XOFFSET_RECORD(sdt::Inner, a, f, b)
XOFFSET_RECORD(sdt::Elem, k, on, s)
XOFFSET_RECORD(sdt::Bag, n, label, on)
XOFFSET_RECORD(sdt::Rec, id, flag, d, in, name, list, nums, tag, small, arr, bag)
XOFFSET_RECORD(sdt::Holder, tick, recs, junk)
XOFFSET_REGISTER_SCHEMA_NAME(sdt::Holder, "test.shared_delta.holder")

namespace {
using RB = xs::XRecordBody<sdt::Rec>;
using IB = xs::XRecordBody<sdt::Inner>;

[[noreturn]] void fail(const char* what, int step) {
    std::fprintf(stderr, "test_shared_delta: %s (step %d)\n", what, step);
    std::exit(1);
}

constexpr std::size_t kRecs = 12;

xo::XBuffer make(std::size_t recs, std::string_view junk = {}) {
    auto b = xo::XBuffer::create<sdt::Holder>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(xo::XGrowPolicy::Automatic));
    if (!junk.empty()) b.root<sdt::Holder>().junk = junk;
    b.root<sdt::Holder>().recs.resize(recs);
    return b;
}

bool verifies(xo::XBuffer& b) {
    std::string w;
    b.save_verified_into<sdt::Holder>(w);
    return bool(xo::XBuffer::try_load_verified<sdt::Holder>(w));
}

std::string random_text(std::mt19937& rng) {
    const int n = int(rng() % 4 == 0 ? rng() % 31 : rng() % 11);
    std::string s(std::size_t(n), 'a');
    for (auto& c : s) c = char('a' + rng() % 26);
    return s;
}

// Changes a few members of r; returns the bit of each member it may have
// changed (member index, declaration order), as a game's dirty flags.
std::uint64_t mutate(sdt::Rec& r, std::mt19937& rng) {
    const int changes = 1 + int(rng() % 4);
    std::uint64_t m = 0;
    for (int i = 0; i < changes; ++i) {
        const unsigned c = unsigned(rng() % 14);
        constexpr std::uint64_t kMembers[14] = {1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 3, 1u << 4, 1u << 7, 1u << 5,
                                                1u << 5, 1u << 6, 1u << 6, 1u << 10, 1u << 10, 1u << 8 | 1u << 9};
        m |= kMembers[c];
        switch (c) {
        case 0: r.id = int(rng()); break;
        case 1: r.flag = !r.flag; break;
        case 2: r.d = double(rng() % 1000) / 7.0; break;
        case 3: r.in.a += 1; r.in.f = rng() % 2; break;
        case 4: r.in.b = std::int64_t(rng()) << 20; break;
        case 5: r.name = std::string_view(random_text(rng)); break;
        case 6: r.tag = std::string_view(random_text(rng)); break;
        case 7: {   // resize the list
            const std::size_t n = rng() % 7;
            r.list.resize(n);
            for (auto& e : r.list) { e.k = int(rng() % 100); e.on = rng() % 2; e.s = std::int16_t(rng() % 300); }
            break;
        }
        case 8:     // change list elements in place
            for (auto& e : r.list) if (rng() % 2) { e.k += 1; e.on = !e.on; }
            break;
        case 9: {
            const std::size_t n = rng() % 5;
            r.nums.resize(n);
            for (auto& v : r.nums) v = int(rng() % 1000);
            break;
        }
        case 10: if (!r.nums.empty()) r.nums[rng() % r.nums.size()] += 3; break;
        case 11: r.bag.n = std::int16_t(rng()); r.bag.on = !r.bag.on; if (rng() % 2) r.bag.label = std::string_view(random_text(rng)); break;
        case 12: r.bag.label = std::string_view(random_text(rng)); break;
        default: r.small = std::uint8_t(rng()); r.arr[rng() % 2].b += 5; r.arr[1].f = rng() % 2; break;
        }
    }
    return m;
}

std::string corrupt(std::string_view body, std::mt19937& rng) {
    std::string bad(body);
    switch (rng() % 3) {
    case 0: bad[rng() % bad.size()] ^= char(1u << (rng() % 8)); break;
    case 1: bad.resize(rng() % bad.size()); break;
    default: bad.push_back(char(rng())); break;
    }
    return bad;
}

// The body of an op in out (after its u32 id).
std::string_view body_of(const xs::XSyncOut& out) { return out.view().substr(4); }

// Bodies that must be rejected: bools, mask bits, lengths.
void rejections() {
    auto b = make(2);
    sdt::Rec& r = b.root<sdt::Holder>().recs[0];
    r.name = std::string_view("hello");
    r.list.resize(2);
    r.bag.label = std::string_view("bag");
    xs::XSyncOut o;
    auto check_change = [](const std::string& s) { return RB::check_change(s.data(), s.data() + s.size()); };
    auto check_whole = [](const std::string& s) { return RB::check_whole(s.data(), s.data() + s.size()); };
    // A valid change of every member, then broken copies of it.
    xs::put_change_op(o, 0, r, RB::members::all);
    const std::string all(body_of(o));
    if (check_change(all) != all.data() + all.size()) fail("change of every member rejected", -1);
    static_assert(sizeof(RB::members::Mask) == 2 && RB::members::N == 11);
    {
        std::string bad = all;
        bad[1] = char(bad[1] | 0x08);   // bit 11: past the members
        if (check_change(bad)) fail("member mask bit past the members accepted", -1);
    }
    {
        // flag (member 1): the byte after the mask and id (4 bytes).
        std::string bad = all;
        bad[2 + 4] = 2;
        if (check_change(bad)) fail("plain bool of 2 accepted", -1);
        bad[2 + 4] = 1;
        if (check_change(bad) != bad.data() + bad.size()) fail("plain bool of 1 rejected", -1);
    }
    {
        // in.f (a bool inside a nested plain member): member 3 alone.
        o.clear();
        xs::put_change_op(o, 0, r, 1u << 3);
        std::string bad(body_of(o));
        bad[2 + 4] = 7;
        if (check_change(bad)) fail("nested bool of 7 accepted", -1);
    }
    {
        // list (member 5): two elements, the first's bool set to 2.
        o.clear();
        xs::put_change_op(o, 0, r, 1u << 5);
        std::string bad(body_of(o));
        if (check_change(bad) != bad.data() + bad.size()) fail("list change rejected", -1);
        bad[2 + 4 + 4] = 2;   // mask, count, Elem::k, then Elem::on
        if (check_change(bad)) fail("element bool of 2 accepted", -1);
        std::string cut(body_of(o));
        cut.resize(cut.size() - 1);
        if (check_change(cut)) fail("vector cut short accepted", -1);
        std::string big(body_of(o));
        big[2] = char(0xff); big[3] = char(0xff); big[4] = char(0xff); big[5] = char(0x0f);
        if (check_change(big)) fail("vector count past the end accepted", -1);
    }
    {
        // name (member 4): a length past the end.
        o.clear();
        xs::put_change_op(o, 0, r, 1u << 4);
        std::string bad(body_of(o));
        bad[2] = 6;
        if (check_change(bad)) fail("string length past the end accepted", -1);
    }
    {
        // bag (member 10, a nested record with a string): its own whole body.
        o.clear();
        xs::put_change_op(o, 0, r, 1u << 10);
        std::string bad(body_of(o));
        if (check_change(bad) != bad.data() + bad.size()) fail("bag change rejected", -1);
        // n (2), on (1), then the container mask (u8): bit 1 is past its one container.
        bad[2 + 3] = char(bad[2 + 3] | 2);
        if (check_change(bad)) fail("container mask bit past the containers accepted", -1);
        std::string b2(body_of(o));
        b2[2 + 2] = 3;
        if (check_change(b2)) fail("bool of 3 in a nested whole body accepted", -1);
    }
    {
        // Whole bodies: a bool, a container mask bit past the containers.
        o.clear();
        xs::put_whole_op(o, 0, r);
        const std::string whole(body_of(o));
        if (check_whole(whole) != whole.data() + whole.size()) fail("whole body rejected", -1);
        std::string bad = whole;
        bad[4] = 2;   // flag: the byte after id
        if (check_whole(bad)) fail("whole body bool of 2 accepted", -1);
        constexpr std::size_t plain = xs::sd_detail::Info<sdt::Rec>::plain_bytes;
        static_assert(xs::sd_detail::Info<sdt::Rec>::container_count == 5);   // name list nums tag bag.label
        bad = whole;
        bad[plain] = char(bad[plain] | 0x20);
        if (check_whole(bad)) fail("whole body container mask bit past the containers accepted", -1);
        for (std::size_t n = 0; n < whole.size(); ++n)
            if (RB::check_whole(whole.data(), whole.data() + n)) fail("whole body cut short accepted", int(n));
    }
}

// put_diff (records without containers) writes the bytes changed() and
// put_change write, and prev takes the changed members.
void diffs(std::mt19937& rng) {
    sdt::Inner prev{}, mirror{};
    std::size_t written = 0;
    for (int step = 0; step < 2000; ++step) {
        sdt::Inner now = prev;
        if (rng() % 2) now.a = int(rng() % 4);
        if (rng() % 2) now.f = rng() % 2;
        if (rng() % 3 == 0) now.b = std::int64_t(rng()) << (rng() % 40);
        const std::uint64_t mask = IB::changed(now, mirror);
        xs::XSyncOut a, b;
        const std::size_t n = xs::put_diff_op(a, 9, now, prev);
        if ((n != 0) != (mask != 0)) fail("put_diff and changed() disagree", step);
        if (n) {
            xs::put_change_op(b, 9, now, mask);
            if (a.view() != b.view()) fail("put_diff bytes differ from changed() + put_change", step);
            IB::apply_change(b.data() + 4, mirror);
            ++written;
        } else if (!a.empty()) fail("an empty diff wrote bytes", step);
        if (!IB::same(prev, now)) fail("put_diff left prev behind", step);
        if (!IB::same(mirror, now)) fail("apply_change left the mirror behind", step);
    }
    std::printf("test_shared_delta: put_diff matches changed() + put_change (%zu bodies)\n", written);
}

// XSyncWriter: the bytes of a message.
void writer() {
    const sdt::Inner root{7, true, -3};
    xs::XSyncOut store;
    sdt::Inner e1{1, false, 2}, e2{3, true, 4};
    const std::size_t at1 = store.size(), n1 = xs::put_whole_op(store, 100, e1);
    const std::size_t at2 = store.size(), n2 = xs::put_whole_op(store, 200, e2);
    if (n1 != 4 + 13 || n2 != 17) fail("whole op length", -1);
    xs::XSyncOut msg;
    const std::uint32_t gone[2] = {5, 6};
    {
        xs::XSyncWriter m(msg, root, 42, 0x1122334455667788ull);
        m.leaves(1, gone, 2);
        m.leaves(2, gone, 0);                     // nothing
        m.begin(2, xs::enter);
        m.op(store.data() + at1, n1);             // under its own id
        m.op(store.data() + at2, n2, 9);          // under id 9
        m.end();
        m.begin(3, xs::change);                   // empty: dropped
        m.end();
        m.begin(2, xs::change);
        m.reserve(2 * (32 + xs::kSyncSlack));
        m.op_in_room(store.data() + at1, n1, 11);
        m.op_upto<32>(store.data() + at2, n2, 12);
        m.op_upto<32>(store.data() + at2, 0, 13); // len 0: no op
        m.end();
        m.begin(4, xs::enter);
        m.whole(77, e2);
        m.end();
        m.begin(4, xs::change);
        sdt::Inner prev{};
        if (!m.diff(78, e1, prev) || prev.a != e1.a) fail("writer diff", -1);
        if (m.diff(79, e1, prev)) fail("writer diff of equal records wrote an op", -1);
        if (m.change(80, e1, 0)) fail("writer change of no members wrote an op", -1);
        m.change(81, e2, 1u << 1);
        m.end();
        if (m.finish() != msg.size()) fail("writer length", -1);
    }
    std::string want("XS\x02\x01", 4);
    auto u32 = [&](std::uint32_t v) { want.append(reinterpret_cast<const char*>(&v), 4); };
    auto bytes = [&](const void* p, std::size_t n) { want.append(static_cast<const char*>(p), n); };
    auto inner = [&](const sdt::Inner& x) { bytes(&x.a, 4); bytes(&x.f, 1); bytes(&x.b, 8); };   // plain runs, no padding
    u32(42);
    const std::uint64_t stream = 0x1122334455667788ull;
    bytes(&stream, 8);
    inner(root);
    u32(2u << 8 | 1u << 2 | xs::leave); u32(5); u32(6);
    u32(2u << 8 | 2u << 2 | xs::enter); u32(100); inner(e1); u32(9); inner(e2);
    u32(2u << 8 | 2u << 2 | xs::change); u32(11); inner(e1); u32(12); inner(e2);
    u32(1u << 8 | 4u << 2 | xs::enter); u32(77); inner(e2);
    u32(2u << 8 | 4u << 2 | xs::change);
    u32(78); want += char(0x05); bytes(&e1.a, 4); bytes(&e1.b, 8);   // a and b differ from zero, f does not (mask u8)
    u32(81); want += char(0x02); want += char(e2.f);
    if (msg.view() != want) fail("writer bytes", -1);
    if (!xs::is_key(msg.view())) fail("is_key on the writer's key", -1);
    // A delta: no stream; the head and root only when no section has ops.
    xs::XSyncWriter d(msg, root, 43);
    d.begin(1, xs::change);
    d.end();
    d.finish();
    std::string want2("XS\x02\x00", 4);
    want.swap(want2);
    u32(43);
    inner(root);
    if (msg.view() != want) fail("writer delta bytes", -1);
    if (xs::is_key(msg.view())) fail("is_key on a delta", -1);
}
} // namespace

int main() {
    static_assert(RB::members::N == 11);
    std::mt19937 rng(20261104);
    auto server = make(kRecs);
    auto base = make(kRecs);
    // The client: other slots (a permutation) and other arena positions (a
    // heap string allocated first, the records in reverse order).
    auto client = make(kRecs + 3, "a string that takes a heap block first");
    std::vector<std::size_t> slot(kRecs);
    std::iota(slot.begin(), slot.end(), 0);
    std::shuffle(slot.begin(), slot.end(), rng);
    for (auto& s : slot) s += 3;

    xs::XSyncOut body;
    std::size_t bodies = 0, wholes = 0, rejected = 0, accepted_corrupt = 0, bytes = 0;
    for (int step = 0; step < 4000; ++step) {
        const std::size_t i = rng() % kRecs;
        auto& S = server.root<sdt::Holder>();
        auto& B = base.root<sdt::Holder>();
        auto& C = client.root<sdt::Holder>();
        const std::uint64_t dirty = rng() % 8 ? mutate(S.recs[i], rng) : 0;
        const std::uint64_t changed = RB::changed(S.recs[i], B.recs[i]);
        if (changed & ~dirty) fail("changed() names a member outside the dirty flags", step);
        const bool whole = rng() % 16 == 0;
        body.clear();
        const std::size_t n = whole ? xs::put_whole_op(body, std::uint32_t(i), S.recs[i])
                                    : xs::put_change_op(body, std::uint32_t(i), S.recs[i], dirty);
        if (n != body.size() || n < 4) fail("op length", step);
        const char* const b = body.data() + 4;
        const char* const end = body.data() + body.size();
        if (body.capacity() < body.size() + xs::kSyncSlack) fail("body store without slack", step);
        bytes += n;
        if (rng() % 3 == 0) {
            // A corrupted copy: rejected, or applied to a clone that still verifies.
            const std::string bad = corrupt(std::string_view(b, std::size_t(end - b)), rng);
            auto twin = client.clone();
            auto& T = twin.root<sdt::Holder>();
            const char* bend = bad.data() + bad.size();
            const char* got = whole ? RB::check_whole(bad.data(), bend) : RB::check_change(bad.data(), bend);
            if (got != bend) ++rejected;
            else {
                if (whole) RB::apply_whole(bad.data(), T.recs[slot[i]]);
                else RB::apply_change(bad.data(), T.recs[slot[i]]);
                if (!verifies(twin)) fail("accepted corrupt body left an invalid buffer", step);
                ++accepted_corrupt;
            }
        }
        if ((whole ? RB::check_whole(b, end) : RB::check_change(b, end)) != end) fail("valid body rejected", step);
        if ((whole ? RB::apply_whole(b, C.recs[slot[i]]) : RB::apply_change(b, C.recs[slot[i]])) != end) fail("apply length differs", step);
        if ((whole ? RB::apply_whole(b, B.recs[i]) : RB::apply_change(b, B.recs[i])) != end) fail("baseline apply length differs", step);
        if (!RB::same(S.recs[i], C.recs[slot[i]])) fail("client differs after apply", step);
        if (!RB::same(S.recs[i], B.recs[i])) fail("baseline differs after apply", step);
        if (RB::changed(S.recs[i], B.recs[i]) != 0) fail("changed() after the baseline caught up", step);
        if (whole) ++wholes; else ++bodies;
        if (step % 100 == 0 && !verifies(client)) fail("client buffer does not verify", step);
    }
    // clear(): plain bytes zero, containers empty; then a whole body sets it back.
    {
        auto& S = server.root<sdt::Holder>();
        auto& C = client.root<sdt::Holder>();
        RB::clear(C.recs[slot[0]]);
        if (!RB::same(C.recs[slot[0]], C.recs[0])) fail("clear() differs from a new record", -1);
        body.clear();
        xs::put_whole_op(body, 0, S.recs[0]);
        RB::apply_whole(body.data() + 4, C.recs[slot[0]]);
        if (!RB::same(S.recs[0], C.recs[slot[0]])) fail("whole body after clear()", -1);
    }
    rejections();
    diffs(rng);
    writer();
    if (!verifies(client)) fail("client buffer does not verify at the end", -1);
    for (std::size_t i = 0; i < kRecs; ++i)
        if (!RB::same(server.root<sdt::Holder>().recs[i], client.root<sdt::Holder>().recs[slot[i]])) fail("final state differs", int(i));
    std::printf("test_shared_delta: %zu change and %zu whole ops (%zu bytes); corrupt: %zu rejected, %zu accepted and verified\n",
                bodies, wholes, bytes, rejected, accepted_corrupt);
    std::printf("Rec: %zu bytes, %zu plain, %zu containers, %zu members\n", sizeof(sdt::Rec), xs::sd_detail::Info<sdt::Rec>::plain_bytes,
                RB::container_count, RB::members::N);
    return 0;
}
