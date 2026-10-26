// In-place reuse of one XBuffer: reload_verified<T>, begin_verified_reload<T>,
// reset<T> and save_verified_into<T>. (Accept/reject equivalence with
// load_verified over mutated wires is covered by test_v2_fuzz acceptors 5, 6.)
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct ReloadRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
};
XOFFSET_REGISTER_SCHEMA_NAME(ReloadRoot, "test.ReloadRoot")

struct OtherRoot { std::uint32_t x{}; };
XOFFSET_REGISTER_SCHEMA_NAME(OtherRoot, "test.ReloadOtherRoot")

namespace {
void need(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
constexpr std::size_t kMax = 1u << 22;
auto cap() { return XBuffer::maximum_capacity(kMax).with_growth(XGrowPolicy::Automatic); }

std::string make_wire(std::uint64_t id, std::size_t n, std::size_t holes = 0) {
    auto b = XBuffer::create<ReloadRoot>(4096, cap());
    auto& r = b.root<ReloadRoot>();
    r.id = id;
    r.name = "reload-" + std::to_string(id) + std::string(40, 'x');
    for (std::size_t i = 0; i < n; ++i) r.values.push_back(std::uint32_t(id * 1000 + i));
    for (std::size_t i = 0; i < holes; ++i) {
        const auto bytes = std::uint32_t(64 + 8 * i);
        auto* p = b.arena()->allocate_aligned(bytes, 8);
        std::memset(p, 0xab, bytes);
        b.arena()->deallocate(p, bytes);
    }
    return b.save_verified<ReloadRoot>();
}

bool payload_equal(const XBuffer& a, const XBuffer& b) {
    return a.get_size() == b.get_size() && std::memcmp(a.data(), b.data(), a.get_size()) == 0;
}

bool root_refused(XBuffer& b) {
    try { (void)b.root<ReloadRoot>(); } catch (const XException&) { return true; }
    return false;
}

void reload_reuses_owner() {
    const auto big = make_wire(1, 50000, 8), small = make_wire(2, 3, 2);
    auto in = XBuffer::create<ReloadRoot>(4096, cap());
    const auto* address = in.get_address();
    in.reload_verified<ReloadRoot>(big);
    need(in.get_address() == address, "reload changed the payload address");
    const auto committed = in.accessible_bytes();
    const auto epoch = in.epoch();
    in.reload_verified<ReloadRoot>(small);
    need(in.epoch() != epoch, "reload did not invalidate typed references");
    need(in.get_address() == address, "reload changed the payload address");
    need(in.accessible_bytes() == committed, "reload returned committed pages implicitly");
    auto fresh = XBuffer::load_verified<ReloadRoot>(small, cap());
    need(payload_equal(in, fresh), "small reload after big reload leaked old bytes");
    need(in.root<ReloadRoot>().values.size() == 3 && in.root<ReloadRoot>().id == 2, "reload content");
    need(in.save_verified<ReloadRoot>() == small, "reload re-save not byte stable");
    // Every newly exposed byte reads as zero, though the big content was there.
    const auto size = in.get_size();
    need(in.grow(1u << 20), "grow after reload");
    for (std::size_t i = size; i < in.get_size(); ++i) need(in.data()[i] == 0, "stale byte after growth");
    // Mutation after reload works like after load.
    in.root<ReloadRoot>().values.push_back(7);
    auto out = XBuffer::load_verified<ReloadRoot>(in.save_verified<ReloadRoot>(), cap());
    need(out.root<ReloadRoot>().values.back() == 7, "mutate after reload");
    // Trimming is explicit.
    (void)in.reclaim_tail_pages();
    need(in.accessible_bytes() < committed, "reclaim_tail_pages did not release pages");
}

void failed_reload_is_rootless_then_recovers() {
    const auto good = make_wire(3, 100, 3);
    auto in = XBuffer::create<ReloadRoot>(4096, cap());
    in.reload_verified<ReloadRoot>(good);
    auto bad = good;
    bad[bad.size() / 2] ^= 0x5a;            // payload corruption
    std::string trunc = good.substr(0, 40);  // shorter than the wire header
    auto wrong = XBuffer::create<OtherRoot>(4096).save_verified<OtherRoot>();
    for (const std::string* w : {&trunc, &wrong, &bad}) {
        bool threw = false, fresh_threw = false;
        try { in.reload_verified<ReloadRoot>(*w); } catch (const XException&) { threw = true; }
        try { (void)XBuffer::load_verified<ReloadRoot>(*w, cap()); } catch (const XException&) { fresh_threw = true; }
        need(threw == fresh_threw, "reload and load disagree on a malformed wire");
        if (!threw) continue;
        need(root_refused(in), "root<T>() allowed after a failed reload");
        need(!in.has_root<ReloadRoot>(), "has_root after a failed reload");
        bool save_refused = false;
        try { (void)in.save_verified<ReloadRoot>(); } catch (const XException&) { save_refused = true; }
        need(save_refused, "save_verified allowed after a failed reload");
        in.reload_verified<ReloadRoot>(good);
        auto fresh = XBuffer::load_verified<ReloadRoot>(good, cap());
        need(payload_equal(in, fresh) && in.save_verified<ReloadRoot>() == good, "recovery reload");
    }
    // Over the buffer's maximum: rejected like load_verified with that maximum.
    auto tiny = XBuffer::create<ReloadRoot>(
        4096, XBuffer::maximum_capacity(8192).with_growth(XGrowPolicy::Automatic));
    const auto large = make_wire(4, 10000);
    bool too_big = false;
    try { tiny.reload_verified<ReloadRoot>(large); } catch (const XBadAlloc&) { too_big = true; }
    need(too_big, "reload above maximum_capacity not rejected with XBadAlloc");
    need(root_refused(tiny), "root<T>() allowed after an over-capacity reload");
    // A rootless buffer can also start over with reset<T>().
    tiny.reset<ReloadRoot>().id = 9;
    need(tiny.root<ReloadRoot>().id == 9, "reset after failed reload");
}

void reset_equals_create() {
    auto b = XBuffer::create<ReloadRoot>(4096, cap());
    const auto* address = b.get_address();
    for (int round = 0; round < 3; ++round) {
        auto& r = b.reset<ReloadRoot>();
        need(b.get_address() == address, "reset changed the payload address");
        need(r.id == 0 && r.name.size() == 0 && r.values.size() == 0, "reset root not default");
        need(b.free_block_count() == 0, "reset kept free blocks");
        auto fresh = XBuffer::create<ReloadRoot>(4096, cap());
        need(b.save_verified<ReloadRoot>() == fresh.save_verified<ReloadRoot>(), "reset wire differs from create");
        auto& again = b.reset<ReloadRoot>();
        again.id = std::uint64_t(round + 10);
        for (int i = 0; i < 2000; ++i) again.values.push_back(std::uint32_t(i));
        again.name = std::string(100, char('a' + round));
        const auto wire = b.save_verified<ReloadRoot>();
        auto in = XBuffer::load_verified<ReloadRoot>(wire, cap());
        need(in.root<ReloadRoot>().values.size() == 2000 && in.root<ReloadRoot>().id == std::uint64_t(round + 10),
             "reset buffer content");
    }
}

void save_into_is_an_independent_copy() {
    auto b = XBuffer::create<ReloadRoot>(4096, cap());
    b.root<ReloadRoot>().values.push_back(1);
    std::string out(1u << 16, '?');
    const auto capacity = out.capacity();
    b.save_verified_into<ReloadRoot>(out);
    need(out == b.save_verified<ReloadRoot>(), "save_verified_into differs from save_verified");
    need(out.capacity() >= capacity, "save_verified_into dropped the caller's capacity");
    const auto copy = out;
    b.root<ReloadRoot>().values.push_back(2);
    need(out == copy, "save_verified_into output changed with the buffer");
    auto t = TypedXBuffer<ReloadRoot>::create(4096, cap());
    t.root().id = 5;
    t.save_verified_into(out);
    TypedXBuffer<ReloadRoot> r = TypedXBuffer<ReloadRoot>::create(4096, cap());
    r.reload_verified(out);
    need(r.root().id == 5, "TypedXBuffer reload_verified");
    need(r.reset().id == 0, "TypedXBuffer reset");
}

void reload_scratch_survives_release_and_swap() {
    const auto good = make_wire(3, 100, 3);
    const auto other = make_wire(4, 50, 2);
    auto in = XBuffer::create<ReloadRoot>(4096, cap());
    for (int i = 0; i < 3; ++i) in.reload_verified<ReloadRoot>(i % 2 ? other : good);
    auto fresh = XBuffer::load_verified<ReloadRoot>(other, cap());
    in.reload_verified<ReloadRoot>(other);
    need(payload_equal(in, fresh) && in.save_verified<ReloadRoot>() == other, "reload with reused scratch");
    in.release_reload_scratch();
    in.reload_verified<ReloadRoot>(good);
    need(in.save_verified<ReloadRoot>() == good, "reload after releasing scratch");
    auto peer = XBuffer::create<ReloadRoot>(4096, cap());
    swap(in, peer);
    peer.reload_verified<ReloadRoot>(other);
    in.reload_verified<ReloadRoot>(good);
    need(peer.save_verified<ReloadRoot>() == other && in.save_verified<ReloadRoot>() == good, "reload after swap");
}
// Transport stand-in: header, extent table, each extent at its offset (or the
// dense payload), hole table; `payload_limit` stops the payload early.
template <class Token>
void deliver(Token& t, const std::string& w, std::size_t payload_limit = SIZE_MAX) {
    std::size_t at = 0;
    auto take = [&](std::byte* to, std::size_t n) { std::memcpy(to, w.data() + at, n); at += n; };
    take(t.header_bytes().data(), t.header_bytes().size());
    if (!t.extent_bytes().empty()) take(t.extent_bytes().data(), t.extent_bytes().size());
    const auto table = t.extent_bytes();
    if (table.empty()) {
        take(t.payload_bytes().data(), std::min(payload_limit, t.payload_bytes().size()));
    } else {
        for (std::size_t i = 0; i < table.size() / 8; ++i) {
            std::uint32_t e[2];
            std::memcpy(e, table.data() + i * 8, 8);
            take(t.payload_bytes().data() + e[0], e[1]);
        }
    }
    if (!t.hole_bytes().empty()) take(t.hole_bytes().data(), t.hole_bytes().size());
    if (payload_limit == SIZE_MAX) need(at == w.size(), "test transport framing");
}

XWireHeaderV2 header_of(const std::string& w) {
    XWireHeaderV2 h{};
    std::memcpy(&h, w.data(), sizeof(h));
    return h;
}

void direct_reload(XBuffer& in, const std::string& w) {
    const auto h = header_of(w);
    auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count, h.extent_count);
    deliver(t, w);
    t.complete(t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(), t.extent_bytes().size());
    t.finish();
}

// A wire with one large hole: emitted sparse when the library skips gaps.
std::string gap_wire(std::uint64_t id) {
    auto b = XBuffer::create<ReloadRoot>(4096, cap());
    auto& r = b.root<ReloadRoot>();
    r.id = id;
    auto* p = b.arena()->allocate_aligned(1u << 16, 8);
    std::memset(p, 0xcd, 1u << 16);
    for (std::uint32_t i = 0; i < 200; ++i) r.values.push_back(i);
    b.arena()->deallocate(p, 1u << 16);
    return b.save_verified<ReloadRoot>();
}

void direct_reload_equals_reload() {
    const auto big = make_wire(1, 50000, 8), small = make_wire(2, 3, 2), gaps = gap_wire(6);
    auto in = XBuffer::create<ReloadRoot>(4096, cap());
    const auto* address = in.get_address();
    for (const std::string* w : {&small, &big, &small, &big, &gaps, &small, &gaps}) {
        const auto epoch = in.epoch();
        direct_reload(in, *w);
        need(in.epoch() != epoch, "direct reload did not invalidate typed references");
        need(in.get_address() == address, "direct reload changed the payload address");
        auto fresh = XBuffer::load_verified<ReloadRoot>(*w, cap());
        need(payload_equal(in, fresh), "direct reload payload differs from load_verified (stale bytes?)");
        need(in.free_block_count() == fresh.free_block_count(), "direct reload free table");
        need(in.save_verified<ReloadRoot>() == *w, "direct reload re-save not byte stable");
    }
    // Sparse gaps were dirty from the big content and still read as zero.
    need(header_of(gaps).extent_count != 0 || header_of(gaps).hole_count != 0, "gap wire has no gap");
    const auto size = in.get_size();
    need(in.grow(1u << 20), "grow after direct reload");
    for (std::size_t i = size; i < in.get_size(); ++i) need(in.data()[i] == 0, "stale byte after growth");
    in.root<ReloadRoot>().values.push_back(7);
    auto out = XBuffer::load_verified<ReloadRoot>(in.save_verified<ReloadRoot>(), cap());
    need(out.root<ReloadRoot>().values.back() == 7, "mutate after direct reload");
    TypedXBuffer<ReloadRoot> typed = TypedXBuffer<ReloadRoot>::create(4096, cap());
    direct_reload(typed, small);
    need(typed.root().id == 2, "TypedXBuffer direct reload");
}

void direct_reload_rejects_interrupted_input() {
    const auto good = make_wire(3, 100, 3);
    const auto h = header_of(good);
    auto in = XBuffer::create<ReloadRoot>(4096, cap());
    auto rejects = [&](auto&& attempt, const char* what) {
        bool threw = false;
        try { attempt(); } catch (const XException&) { threw = true; }
        need(threw, what);
        need(root_refused(in) && !in.has_root<ReloadRoot>(), "root after a rejected direct reload");
        direct_reload(in, good);
        need(in.save_verified<ReloadRoot>() == good, "direct reload after a reject");
    };
    // Dropped half-way: the token's destructor leaves the buffer rootless.
    rejects([&] {
        { auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count, h.extent_count);
          deliver(t, good, h.used_bytes / 2); }
        (void)in.root<ReloadRoot>();
    }, "dropped direct reload left a root");
    // Received counts disagree with the declared sections.
    for (int k = 0; k < 4; ++k)
        rejects([&] {
            auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count, h.extent_count);
            deliver(t, good);
            std::size_t n[4] = {t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(),
                                t.extent_bytes().size()};
            n[k] = n[k] ? n[k] - 1 : 8;
            t.complete(n[0], n[1], n[2], n[3]);
            t.finish();
        }, "short count accepted");
    rejects([&] {
        auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count, h.extent_count);
        deliver(t, good);
        t.finish(); // no complete()
    }, "finish without complete accepted");
    // Declared lengths other than the header's.
    rejects([&] {
        auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count + 1, h.extent_count);
        deliver(t, good + std::string(8, '\0'));
        t.complete(t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(), t.extent_bytes().size());
        t.finish();
    }, "hole_count mismatch accepted");
    rejects([&] {
        auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes + 8, h.hole_count, h.extent_count);
        deliver(t, good, h.used_bytes);
        t.complete(t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(), t.extent_bytes().size());
        t.finish();
    }, "used_bytes mismatch accepted");
    rejects([&] { (void)in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.used_bytes, 0); },
            "impossible hole count accepted");
    // One token at a time; a consumed token cannot finish again.
    {
        auto t = in.begin_verified_reload<ReloadRoot>(h.used_bytes, h.hole_count, h.extent_count);
        bool second = false;
        try { (void)in.begin_verified_reload<ReloadRoot>(h.used_bytes); } catch (const XException&) { second = true; }
        need(second, "second open direct reload allowed");
        deliver(t, good);
        in.release_reload_scratch(); // keeps the open token's tables
        t.complete(t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(), t.extent_bytes().size());
        t.finish();
        need(!t, "token still open after finish");
        bool again = false;
        try { t.finish(); } catch (const XException&) { again = true; }
        need(again && in.save_verified<ReloadRoot>() == good, "finish on a consumed token");
    }
    // Over the buffer's maximum: XBadAlloc at begin, like reload_verified.
    auto tiny = XBuffer::create<ReloadRoot>(
        4096, XBuffer::maximum_capacity(8192).with_growth(XGrowPolicy::Automatic));
    bool too_big = false;
    try { (void)tiny.begin_verified_reload<ReloadRoot>(1u << 16); } catch (const XBadAlloc&) { too_big = true; }
    need(too_big && root_refused(tiny), "direct reload above maximum_capacity not rejected with XBadAlloc");
}
} // namespace

int main() {
    try {
        reload_reuses_owner();
        failed_reload_is_rootless_then_recovers();
        reset_equals_create();
        save_into_is_an_independent_copy();
        reload_scratch_survives_release_and_swap();
        direct_reload_equals_reload();
        direct_reload_rejects_interrupted_input();
        std::cout << "test_reload_reset passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "test_reload_reset failed: " << e.what() << '\n';
        return 1;
    }
}
