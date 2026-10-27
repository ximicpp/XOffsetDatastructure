// SHIFT regression: XVector erase/insert of records that own inline heads move
// the tail with one memmove and restart only the inline heads; growth and
// reserve move the whole live run to the new block with one memcpy. Content must
// match a std model after every operation, and every state must survive a
// full save_verified/load_verified round trip. test_v2_golden pins the bytes.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace shift {
// Explicit pad: brace-initialized temporaries would otherwise copy indeterminate
// padding bytes into the arena and make the two wires differ for that reason alone.
struct alignas(8) Troop { std::int32_t type{}; std::int32_t count{}; std::int32_t wounded{}; std::int32_t pad{}; };
struct alignas(8) Rec {
    std::int64_t id{};
    bool active{};
    xo::XString name;                      // inline and heap
    xo::XVector<Troop> troops;
    xo::XMap<std::int64_t, std::int64_t> res;  // int64 key: no padding inside entries
    xo::XString tag0;
    xo::XString tag1;
    std::int32_t tail{};
};
struct Root {
    std::uint64_t version{};
    xo::XVector<Rec> recs;
};
// Same layout without the opt-in: keeps the per-element relocation path,
// so its wire is the byte reference for the SHIFT path.
struct alignas(8) PlainRec {
    std::int64_t id{};
    bool active{};
    xo::XString name;
    xo::XVector<Troop> troops;
    xo::XMap<std::int64_t, std::int64_t> res;  // int64 key: no padding inside entries
    xo::XString tag0;
    xo::XString tag1;
    std::int32_t tail{};
};
struct PlainRoot {
    std::uint64_t version{};
    xo::XVector<PlainRec> recs;
};
struct StdRec {
    std::int64_t id{};
    bool active{};
    std::string name;
    std::vector<Troop> troops;
    std::map<std::int32_t, std::int64_t> res;
    std::string tag0;
    std::string tag1;
    std::int32_t tail{};
};
} // namespace shift

XOFFSET_REGISTER_SCHEMA_NAME(shift::Root, "test.shift.root")
XOFFSET_REGISTER_SCHEMA_NAME(shift::PlainRoot, "test.shift.plain")

namespace XOffsetDatastructure {
// Audited source above: ordinary members only, no bases, unions or
// [[no_unique_address]]. This is the raw/future domain's explicit contract,
// which the SHIFT path reuses.
template<> struct wire_record_layout<shift::Troop> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<shift::Rec> { static constexpr bool ordinary_members = true; };
}

// Each SHIFT precondition separately, so a failure names the missing one.
static_assert(xo::fixed_detail::can_relocate_without_allocation<shift::Rec>());
static_assert(xo::detail::receive_raw::future_supported<shift::Rec>());
static_assert(xo::detail::raw_needs_activation<shift::Rec>());
static_assert(xo::detail::ordinary_outer_lifetime_supported<shift::Rec>());
static_assert(xo::detail::shift_rebase_domain<shift::Rec>());
static_assert(xo::detail::shift_relocate_supported<shift::Rec>(), "Rec must take the SHIFT path");
static_assert(xo::detail::shift_relocate_supported<xo::XString>());
static_assert(xo::detail::shift_relocate_supported<xo::XVector<std::uint32_t>>());
static_assert(!xo::detail::shift_relocate_supported<shift::Troop>(), "trivially copyable keeps memmove");
static_assert(xo::fixed_detail::can_relocate_without_allocation<shift::PlainRec>());
static_assert(!xo::detail::shift_relocate_supported<shift::PlainRec>(), "PlainRec is the per-element reference");

namespace {
using shift::StdRec;
using shift::Troop;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_shift_relocate FAILED: " << message << "\n";
    std::exit(1);
}

struct Rng {
    std::uint64_t s;
    std::uint32_t below(std::uint32_t n) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return n ? std::uint32_t(s >> 33) % n : 0;
    }
};

std::string text(Rng& rng) {
    std::string s(rng.below(2) ? 4 + rng.below(6) : 12 + rng.below(30), 'a');
    for (auto& c : s) c = char('a' + rng.below(26));
    return s;
}

StdRec make(Rng& rng, std::int64_t id) {
    StdRec r;
    r.id = id;
    r.active = rng.below(2) != 0;
    r.name = text(rng);
    for (std::uint32_t k = 0, n = rng.below(5); k < n; ++k)
        r.troops.push_back({std::int32_t(rng.below(4)), std::int32_t(rng.below(5000)), std::int32_t(k)});
    for (std::uint32_t k = 0, n = rng.below(4); k < n; ++k) r.res[std::int32_t(rng.below(6))] = rng.below(100000);
    r.tag0 = text(rng);
    if (rng.below(2)) r.tag1 = text(rng);
    r.tail = std::int32_t(rng.below(1000));
    return r;
}

template <class R> void fill(R& d, const StdRec& s) {
    d.id = s.id; d.active = s.active; d.name = std::string_view(s.name);
    d.troops.clear();
    for (const auto& t : s.troops) d.troops.push_back(t);
    d.res.clear();
    for (const auto& [k, v] : s.res) d.res[k] = v;
    d.tag0 = std::string_view(s.tag0);
    d.tag1 = std::string_view(s.tag1);
    d.tail = s.tail;
}

template <class Root> void compare(const Root& root, const std::vector<StdRec>& model, const char* where) {
    if (root.recs.size() != model.size()) fail(std::string(where) + ": size");
    for (std::size_t i = 0; i < model.size(); ++i) {
        const auto& a = root.recs[i];
        const auto& b = model[i];
        bool ok = a.id == b.id && a.active == b.active && a.name.view() == b.name && a.tail == b.tail &&
                  a.tag0.view() == b.tag0 && a.tag1.view() == b.tag1 &&
                  a.troops.size() == b.troops.size() && a.res.size() == b.res.size();
        for (std::size_t k = 0; ok && k < b.troops.size(); ++k)
            ok = a.troops[k].type == b.troops[k].type && a.troops[k].count == b.troops[k].count &&
                 a.troops[k].wounded == b.troops[k].wounded;
        for (const auto& [k, v] : b.res) {
            auto it = a.res.find(k);
            ok = ok && it != a.res.end() && it->second == v;
        }
        if (!ok) fail(std::string(where) + ": record " + std::to_string(i));
    }
}

auto policy() {
    return xo::XBuffer::maximum_capacity(std::size_t(64) << 20).with_growth(xo::XGrowPolicy::Automatic);
}
} // namespace

// Runs the same deterministic operations; returns the wire at each checkpoint.
template <class Root> std::vector<std::string> run() {
    std::vector<std::string> wires;
    Rng rng{20261021};
    xo::XBuffer buf = xo::XBuffer::create<Root>(16 * 1024, policy());
    std::vector<StdRec> model;
    std::int64_t next_id = 1;
    for (int i = 0; i < 64; ++i) {
        model.push_back(make(rng, next_id++));
        fill(buf.root<Root>().recs.emplace_back(), model.back());
    }
    compare(buf.root<Root>(), model, "setup");

    for (int op = 0; op < 4000; ++op) {
        auto& recs = buf.root<Root>().recs;
        const auto n = std::uint32_t(model.size());
        switch (rng.below(4)) {
        case 0:
        case 1:
            if (n > 1) {
                const auto at = rng.below(n);
                recs.erase(recs.begin() + std::ptrdiff_t(at));
                model.erase(model.begin() + std::ptrdiff_t(at));
            }
            break;
        case 2: {
            const auto at = rng.below(n + 1);
            auto value = make(rng, next_id++);
            auto& slot = recs.emplace_back();
            fill(slot, value);
            if (at != n) {
                // Insert a copy of the new tail record in the middle, then drop the tail.
                recs.insert(recs.begin() + std::ptrdiff_t(at), recs[n]);
                recs.erase(recs.begin() + std::ptrdiff_t(n + 1));
            }
            model.insert(model.begin() + std::ptrdiff_t(at), value);
            break;
        }
        default: {
            if (n == 0) break;
            const auto at = rng.below(n);
            model[at].name = text(rng);
            recs[at].name = std::string_view(model[at].name);
            model[at].troops.push_back({1, 2, 3});
            recs[at].troops.push_back({1, 2, 3});
            break;
        }
        }
        if (op % 97 == 0) {
            // Explicit reserve past capacity: block relocation of the live run.
            if (rng.below(3) == 0) {
                auto& live = buf.root<Root>().recs;
                live.reserve(live.capacity() + 1 + rng.below(8));
            }
            compare(buf.root<Root>(), model, "mutate");
            std::string wire = buf.save_verified<Root>();
            xo::XBuffer loaded = xo::XBuffer::load_verified<Root>(wire, policy());
            compare(loaded.root<Root>(), model, "reload");
            if (loaded.save_verified<Root>() != wire) fail("write-read-write bytes differ");
            wires.push_back(wire);
            // Keep mutating the received arena half of the time.
            if (rng.below(2)) buf = std::move(loaded);
        }
    }
    compare(buf.root<Root>(), model, "final");
    buf.root<Root>().recs.clear();
    xo::XBuffer empty = xo::XBuffer::load_verified<Root>(buf.save_verified<Root>(), policy());
    if (empty.root<Root>().recs.size() != 0) fail("clear");
    wires.push_back(buf.save_verified<Root>());
    return wires;
}

// Erases whose moved tail sits on either side of the chunked-move threshold
// (16 KiB) and of its 4 KiB chunk boundaries (x86-64 glibc copies SHIFT
// tails above it in chunks; elsewhere, and for trivially copyable elements,
// it is one memmove). Each erase is checked against a std model; the SHIFT
// and per-element wires must match byte for byte.
template <class Root> std::string erase_tail_sizes() {
    Rng rng{20261026};
    xo::XBuffer buf = xo::XBuffer::create<Root>(64 * 1024, policy());
    std::vector<StdRec> model;
    for (std::int64_t id = 1; id <= 1400; ++id) {
        model.push_back(make(rng, id));
        fill(buf.root<Root>().recs.emplace_back(), model.back());
    }
    // Trivially copyable elements: Troop is 16 bytes, so the tail sizes are exact.
    std::vector<Troop> troops;
    auto& first = buf.root<Root>().recs[0];
    first.troops.clear();
    for (std::int32_t k = 0; k < 6000; ++k) {
        troops.push_back({k % 4, k, k * 3});
        first.troops.push_back(troops.back());
    }
    model[0].troops = troops;
    constexpr std::size_t threshold = 16384, chunk = 4096;
    const std::size_t byte_targets[] = {threshold - chunk, threshold, threshold + chunk,
                                        threshold + 3 * chunk, 4 * threshold};
    for (const auto bytes : byte_targets) {
        for (const auto element : {sizeof(Troop), sizeof(typename decltype(Root::recs)::value_type)}) {
            for (const std::ptrdiff_t delta : {-1, 0, 1}) {
                const auto moved = std::size_t(std::ptrdiff_t(bytes / element) + delta);
                auto& recs = buf.root<Root>().recs;
                if (element == sizeof(Troop)) {
                    auto& live = recs[0].troops;
                    const auto at = std::ptrdiff_t(live.size() - 1 - moved);
                    live.erase(live.begin() + at);
                    model[0].troops.erase(model[0].troops.begin() + at);
                } else {
                    const auto at = std::ptrdiff_t(recs.size() - 1 - moved);
                    recs.erase(recs.begin() + at);
                    model.erase(model.begin() + at);
                }
                compare(buf.root<Root>(), model, "erase tail sizes");
            }
        }
    }
    std::string wire = buf.save_verified<Root>();
    xo::XBuffer loaded = xo::XBuffer::load_verified<Root>(wire, policy());
    compare(loaded.root<Root>(), model, "erase tail sizes reload");
    return wire;
}

// The rel32 rebase of a moved run: deltas at the edge of int32 rebase exactly,
// null deltas stay null, and a delta the shift would push out of range ends
// the program (Clang checks the whole run once after it, GCC and MSVC each sum).
using RebaseHead = xo::XVector<std::uint32_t>;
static_assert(sizeof(RebaseHead) == 12);

struct RawHead { std::int32_t data_delta; std::uint32_t size, capacity; };

void rebase_run(RawHead* heads, std::size_t count, std::int32_t shift) {
    alignas(RebaseHead) unsigned char storage[4 * sizeof(RebaseHead)];
    std::memcpy(storage, heads, count * sizeof(RawHead));
    xo::detail::shift_rebase_moved(reinterpret_cast<RebaseHead*>(storage), count, shift);
    std::memcpy(heads, storage, count * sizeof(RawHead));
}

void rebase_edges() {
    constexpr auto max = (std::numeric_limits<std::int32_t>::max)();
    constexpr auto min = (std::numeric_limits<std::int32_t>::min)();
    RawHead up[3] = {{max - 16, 1, 2}, {0, 3, 4}, {-7, 0, 0}};
    rebase_run(up, 3, 16);
    if (up[0].data_delta != max || up[1].data_delta != 0 || up[2].data_delta != 9 ||
        up[0].size != 1 || up[0].capacity != 2 || up[1].size != 3 || up[1].capacity != 4)
        fail("rebase by +16 at the int32 edge");
    RawHead down[3] = {{min + 16, 0, 0}, {0, 0, 0}, {-5, 0, 0}};
    rebase_run(down, 3, -16);
    if (down[0].data_delta != min || down[1].data_delta != 0 || down[2].data_delta != -21)
        fail("rebase by -16 at the int32 edge");
}

// --overflow-up / --overflow-down / --overflow-mid: must end in std::terminate.
int rebase_overflow(std::string_view mode) {
    std::set_terminate([] {
        std::cout << "test_shift_relocate: overflow terminated as required" << std::endl;
        std::_Exit(0);
    });
    constexpr auto max = (std::numeric_limits<std::int32_t>::max)();
    constexpr auto min = (std::numeric_limits<std::int32_t>::min)();
    if (mode == "--overflow-up") {
        RawHead h[1] = {{max - 15, 0, 0}};
        rebase_run(h, 1, 16);
    } else if (mode == "--overflow-down") {
        RawHead h[1] = {{min + 15, 0, 0}};
        rebase_run(h, 1, -16);
    } else if (mode == "--overflow-mid") {
        // The out-of-range delta sits inside the run, between valid ones.
        RawHead h[3] = {{8, 0, 0}, {max - 1, 0, 0}, {8, 0, 0}};
        rebase_run(h, 3, 2);
    } else {
        fail("unknown mode");
    }
    std::cerr << "test_shift_relocate FAILED: " << mode << " did not terminate\n";
    return 1;
}

int main(int argc, char** argv) {
    if (argc > 1) return rebase_overflow(argv[1]);
    rebase_edges();
    {
        const auto shifted = erase_tail_sizes<shift::Root>();
        const auto plain = erase_tail_sizes<shift::PlainRoot>();
        constexpr std::size_t header = sizeof(xo::XWireHeaderV2);
        if (shifted.size() != plain.size() ||
            shifted.compare(header, std::string::npos, plain, header, std::string::npos) != 0)
            fail("erase tail sizes: SHIFT wire differs from the per-element path");
    }
    const auto shifted = run<shift::Root>();
    const auto plain = run<shift::PlainRoot>();
    if (shifted.size() != plain.size()) fail("checkpoint count");
    // Headers carry the schema identity; every payload byte after them must match.
    constexpr std::size_t header = sizeof(xo::XWireHeaderV2);
    for (std::size_t i = 0; i < shifted.size(); ++i) {
        if (shifted[i].size() != plain[i].size() ||
            shifted[i].compare(header, std::string::npos, plain[i], header, std::string::npos) != 0)
            fail("SHIFT wire differs from the per-element path at checkpoint " + std::to_string(i));
    }
    std::cout << "test_shift_relocate passed (" << shifted.size() << " checkpoints byte-identical)\n";
    return 0;
}
