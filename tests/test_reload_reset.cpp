// In-place reuse of one XBuffer: reload_verified<T>, reset<T> and
// save_verified_into<T>. (Accept/reject equivalence with load_verified over
// mutated wires is covered by test_v2_fuzz acceptor 5.)
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
} // namespace

// Growth after an in-place reload over larger old content, and after the
// shrink of save_verified, exposes only zero bytes: the result equals the
// same edits on a fresh load of the same wire, byte for byte.
void growth_after_reload_exposes_zero() {
    const auto big = make_wire(7, 3000, 3), small = make_wire(8, 40);
    auto edit = [](XBuffer& b) {
        auto& r = b.root<ReloadRoot>();
        for (std::uint32_t i = 0; i < 2500; ++i) r.values.push_back(i);
        r.name = std::string(300, 'n');
    };
    auto fresh = XBuffer::load_verified<ReloadRoot>(small, cap());
    edit(fresh);
    const auto expected = fresh.save_verified<ReloadRoot>();
    auto b = XBuffer::create<ReloadRoot>(4096, cap());
    b.reload_verified<ReloadRoot>(big);
    edit(b);                                 // old content past used_end, grown
    (void)b.save_verified<ReloadRoot>();     // shrink_to_fit
    b.reload_verified<ReloadRoot>(big);
    b.reload_verified<ReloadRoot>(small);    // old content past the new length
    edit(b);
    need(b.save_verified<ReloadRoot>() == expected, "growth after reload exposed old bytes");
    b.reload_verified<ReloadRoot>(small);
    edit(b);
    (void)b.save_verified<ReloadRoot>();
    b.reload_verified<ReloadRoot>(small);
    edit(b);
    need(b.save_verified<ReloadRoot>() == expected, "growth after shrink and reload exposed old bytes");
}

int main() {
    try {
        growth_after_reload_exposes_zero();
        reload_reuses_owner();
        failed_reload_is_rootless_then_recovers();
        reset_equals_create();
        save_into_is_an_independent_copy();
        reload_scratch_survives_release_and_swap();
        std::cout << "test_reload_reset passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "test_reload_reset failed: " << e.what() << '\n';
        return 1;
    }
}
