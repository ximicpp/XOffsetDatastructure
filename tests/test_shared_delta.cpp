// Record-relative shared deltas (xoffset_shared_delta.hpp, EXPERIMENTAL D3
// prototype).
//
// One server record set, one shared baseline, and a client that holds the
// same records in another layout (other slots, other arena positions). Random
// changes to scalars, bools, nested records, arrays, inline and heap strings
// and vectors (resized, patched): every body the server writes is checked and
// applied on the client, after which the client record holds the server
// record's content and the baseline equals the server. The client buffer
// passes load_verified throughout. Key bodies rebuild a cleared record.
// Corrupted bodies (flipped, truncated, extended) are either rejected by
// check() or, when accepted, leave a buffer that still verifies; a vector
// patch naming bytes past the vector's end is rejected. No try/catch:
// also built as test_shared_delta_noexc.
#include "xoffsetdatastructure.hpp"
#include "xoffset_shared_delta.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

namespace sdt {
struct Inner { std::int32_t a{}; bool f{}; std::int64_t b{}; };
struct Elem { std::int32_t k{}; bool on{}; std::int16_t s{}; };
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
};
struct Holder { std::int64_t tick{}; xo::XVector<Rec> recs; xo::XString junk; };
} // namespace sdt
XOFFSET_RECORD(sdt::Inner, a, f, b)
XOFFSET_RECORD(sdt::Elem, k, on, s)
XOFFSET_RECORD(sdt::Rec, id, flag, d, in, name, list, nums, tag, small, arr)
XOFFSET_RECORD(sdt::Holder, tick, recs, junk)
XOFFSET_REGISTER_SCHEMA_NAME(sdt::Holder, "test.shared_delta.holder")

namespace {
using RD = xs::XRecordDelta<sdt::Rec>;

[[noreturn]] void fail(const char* what, int step) {
    std::fprintf(stderr, "test_shared_delta: %s (step %d)\n", what, step);
    std::exit(1);
}

constexpr std::size_t kRecs = 12;

xo::XBuffer make(std::size_t recs) {
    auto b = xo::XBuffer::create<sdt::Holder>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(xo::XGrowPolicy::Automatic));
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

void mutate(sdt::Rec& r, std::mt19937& rng) {
    const int changes = 1 + int(rng() % 4);
    for (int i = 0; i < changes; ++i) {
        switch (rng() % 12) {
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
        case 8:     // patch list elements in place
            for (auto& e : r.list) if (rng() % 2) { e.k += 1; e.on = !e.on; }
            break;
        case 9: {
            const std::size_t n = rng() % 5;
            r.nums.resize(n);
            for (auto& v : r.nums) v = int(rng() % 1000);
            break;
        }
        case 10: if (!r.nums.empty()) r.nums[rng() % r.nums.size()] += 3; break;
        default: r.small = std::uint8_t(rng()); r.arr[rng() % 2].b += 5; r.arr[1].f = rng() % 2; break;
        }
    }
}
} // namespace

int main() {
    std::mt19937 rng(20261104);
    auto server = make(kRecs);
    auto base = make(kRecs);
    // The client: other slots (a permutation) and other arena positions (a
    // heap string allocated first, the records in reverse order).
    auto client = xo::XBuffer::create<sdt::Holder>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(xo::XGrowPolicy::Automatic));
    client.root<sdt::Holder>().junk = std::string_view("a string that takes a heap block first");
    client.root<sdt::Holder>().recs.resize(kRecs + 3);
    std::vector<std::size_t> slot(kRecs);
    std::iota(slot.begin(), slot.end(), 0);
    std::shuffle(slot.begin(), slot.end(), rng);
    for (auto& s : slot) s += 3;

    std::string body;
    std::size_t bodies = 0, empties = 0, keys = 0, rejected = 0, accepted_corrupt = 0, bytes = 0;
    for (int step = 0; step < 3000; ++step) {
        const std::size_t i = rng() % kRecs;
        auto& S = server.root<sdt::Holder>();
        auto& B = base.root<sdt::Holder>();
        auto& C = client.root<sdt::Holder>();
        if (rng() % 8) mutate(S.recs[i], rng);
        body.clear();
        if (!RD::diff(S.recs[i], B.recs[i], body)) {
            if (!body.empty()) fail("empty diff wrote bytes", step);
            ++empties;
            if (!RD::same(S.recs[i], C.recs[slot[i]])) fail("unchanged record differs on the client", step);
            continue;
        }
        if (!RD::same(S.recs[i], B.recs[i])) fail("baseline not synced", step);
        ++bodies;
        bytes += body.size();
        const char* end = body.data() + body.size();
        if (rng() % 3 == 0) {
            // A corrupted copy: rejected, or applied to a clone that still verifies.
            std::string bad = body;
            switch (rng() % 3) {
            case 0: bad[rng() % bad.size()] ^= char(1u << (rng() % 8)); break;
            case 1: bad.resize(rng() % bad.size()); break;
            default: bad.push_back(char(rng())); break;
            }
            auto twin = client.clone();
            auto& T = twin.root<sdt::Holder>();
            const char* bend = bad.data() + bad.size();
            const char* got = RD::check(bad.data(), bend, &T.recs[slot[i]]);
            if (!got || got != bend) ++rejected;
            else {
                RD::apply(bad.data(), T.recs[slot[i]]);
                if (!verifies(twin)) fail("accepted corrupt body left an invalid buffer", step);
                ++accepted_corrupt;
            }
        }
        const char* got = RD::check(body.data(), end, &C.recs[slot[i]]);
        if (got != end) fail("valid body rejected", step);
        if (RD::apply(body.data(), C.recs[slot[i]]) != end) fail("apply length differs", step);
        if (!RD::same(S.recs[i], C.recs[slot[i]])) fail("client differs after apply", step);
        if (rng() % 16 == 0) {
            // Key: clear the client's record and rebuild it from a key body.
            RD::clear(C.recs[slot[i]]);
            body.clear();
            RD::key(S.recs[i], body);
            const char* kend = body.data() + body.size();
            sdt::Rec* dst = &C.recs[slot[i]];
            if (RD::check(body.data(), kend, nullptr) != kend) fail("key body rejected", step);
            RD::apply(body.data(), *dst);
            if (!RD::same(S.recs[i], *dst)) fail("client differs after key", step);
            ++keys;
        }
        if (step % 100 == 0 && !verifies(client)) fail("client buffer does not verify", step);
    }
    {
        // A patch of a vector whose bytes end inside a word (one int32): mask
        // bits past the end would write past the vector and are rejected.
        sdt::Rec& r = client.root<sdt::Holder>().recs[0];   // a slot no server record maps to
        r.nums.resize(1);
        std::string bad;
        bad.push_back(0);                     // no inline blocks
        bad.push_back(1);                     // one container op
        bad.push_back(char(2 << 2 | 2));      // container 2 (nums), VPATCH
        bad.push_back(1); bad.push_back(0); bad.push_back(1);   // one block, skip 0, selector word 0
        bad.push_back(char(0xf0));            // bytes 4..7: past the end
        bad.append(4, 'x');
        if (RD::check(bad.data(), bad.data() + bad.size(), &r)) fail("patch past a vector's end accepted", -1);
        bad[6] = char(0x0f);                  // bytes 0..3: the element
        if (RD::check(bad.data(), bad.data() + bad.size(), &r) != bad.data() + bad.size()) fail("patch inside a vector rejected", -1);
        RD::apply(bad.data(), r);
        if (r.nums.size() != 1 || r.nums[0] != 0x78787878) fail("patch inside a vector not applied", -1);
    }
    if (!verifies(client)) fail("client buffer does not verify at the end", -1);
    for (std::size_t i = 0; i < kRecs; ++i)
        if (!RD::same(server.root<sdt::Holder>().recs[i], client.root<sdt::Holder>().recs[slot[i]])) fail("final state differs", int(i));
    std::printf("test_shared_delta: %zu bodies (%zu bytes, %.1f per body), %zu empty, %zu keys; corrupt: %zu rejected, %zu accepted and verified\n",
                bodies, bytes, bodies ? double(bytes) / double(bodies) : 0.0, empties, keys, rejected, accepted_corrupt);
    std::printf("Rec: %zu bytes, %zu plain, %zu containers\n", sizeof(sdt::Rec), xs::sd_detail::Info<sdt::Rec>::plain_bytes,
                RD::container_count);
    return 0;
}
