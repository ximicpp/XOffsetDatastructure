// G1 regression: appending to a full XVector of non-trivially-copyable records
// builds the new element in the grown block (no staged element), for both the
// SHIFT path (declared ordinary records) and the per-element path. Checks:
// arguments that alias old elements, the tail-expansion and fresh-block cases,
// an allocation-failure sweep (contents unchanged, wire still verifies, retry
// succeeds), and byte-identical wires between the two relocation paths.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace ag {
struct alignas(8) Rec {
    std::int64_t id{};
    xo::XString name;
    xo::XVector<std::int32_t> values;
};
struct Root {
    xo::XVector<Rec> recs;
    xo::XString spacer;
};
// Same layout without the declaration: per-element relocation reference.
struct alignas(8) PlainRec {
    std::int64_t id{};
    xo::XString name;
    xo::XVector<std::int32_t> values;
};
struct PlainRoot {
    xo::XVector<PlainRec> recs;
    xo::XString spacer;
};
} // namespace ag

XOFFSET_REGISTER_SCHEMA_NAME(ag::Root, "test.append_grow.root")
XOFFSET_REGISTER_SCHEMA_NAME(ag::PlainRoot, "test.append_grow.plain")

namespace XOffsetDatastructure {
template<> struct wire_record_layout<ag::Rec> { static constexpr bool ordinary_members = true; };
}

static_assert(xo::detail::shift_relocate_supported<ag::Rec>(), "Rec takes the SHIFT path");
static_assert(!xo::detail::shift_relocate_supported<ag::PlainRec>());
static_assert(xo::fixed_detail::can_relocate_without_allocation<ag::PlainRec>());

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_append_grow FAILED: " << message << "\n";
    std::exit(1);
}

struct Model {
    std::int64_t id;
    std::string name;
    std::vector<std::int32_t> values;
};

template <class R> Model model_of(const R& r) {
    return {r.id, std::string(r.name.view()), std::vector<std::int32_t>(r.values.begin(), r.values.end())};
}

template <class Root> std::vector<Model> snapshot(const Root& root) {
    std::vector<Model> out;
    for (const auto& r : root.recs) out.push_back(model_of(r));
    return out;
}

bool same(const std::vector<Model>& a, const std::vector<Model>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].id != b[i].id || a[i].name != b[i].name || a[i].values != b[i].values) return false;
    return true;
}

template <class R> void fill(R& r, std::int64_t id, std::size_t name_len, int values) {
    r.id = id;
    r.name = std::string(name_len, char('a' + id % 26));
    for (int k = 0; k < values; ++k) r.values.push_back(std::int32_t(id * 10 + k));
}

auto growable() {
    return xo::XBuffer::maximum_capacity(std::size_t(8) << 20).with_growth(xo::XGrowPolicy::Automatic);
}

template <class Root> void round_trip(xo::XBuffer& buf, const std::vector<Model>& expected, const char* where) {
    std::string wire = buf.save_verified<Root>();
    xo::XBuffer loaded = xo::XBuffer::load_verified<Root>(wire, growable());
    if (!same(snapshot(loaded.root<Root>()), expected)) fail(std::string(where) + ": reload content");
    if (loaded.save_verified<Root>() != wire) fail(std::string(where) + ": write-read-write bytes");
}

// Appends whose argument aliases an existing element, at every growth step.
// `inline_names` keeps every allocation inside the vector block, so it stays
// the arena tail and grows by tail expansion; otherwise each element owns
// out-of-line storage and growth takes a fresh block.
template <class Root> std::vector<std::string> aliasing(bool inline_names) {
    std::vector<std::string> wires;
    xo::XBuffer buf = xo::XBuffer::create<Root>(4096, growable());
    auto& recs = buf.root<Root>().recs;
    std::vector<Model> model;
    const std::size_t name_len = inline_names ? 6 : 24;
    const int values = inline_names ? 0 : 3;
    fill(recs.emplace_back(), 1, name_len, values);
    model.push_back(model_of(recs[0]));
    for (int step = 0; step < 40; ++step) {
        const bool full = recs.size() == recs.capacity();
        const void* block = recs.data();
        switch (step % 3) {
        case 0: recs.push_back(recs[0]); model.push_back(model[0]); break;
        case 1: recs.push_back(recs.back()); model.push_back(model.back()); break;
        default: {
            auto& added = recs.emplace_back();
            fill(added, 100 + step, name_len, values);
            model.push_back(model_of(added));
        }
        }
        if (!same(snapshot(buf.root<Root>()), model))
            fail(std::string("aliasing step ") + std::to_string(step) + (full ? " (grew)" : ""));
        if (full && (recs.data() == block) != inline_names)
            fail(inline_names ? "expected tail expansion" : "expected a fresh block");
        if (full) {
            round_trip<Root>(buf, model, "aliasing");
            wires.push_back(buf.save_verified<Root>());
        }
    }
    return wires;
}

// Every append into a full vector under a shrinking free budget: an
// allocation failure (fresh block, or the new element's own storage after
// the fresh block) must leave contents unchanged, keep the wire verifiable
// and let a retry after growth succeed.
template <class Root> void failure_sweep() {
    int failed = 0, succeeded = 0;
    for (std::size_t slack = 0; slack <= 1536; slack += 8) {
        xo::XBuffer buf = xo::XBuffer::create<Root>(16384);
        auto& r = buf.root<Root>();
        r.recs.reserve(4);
        for (int i = 0; i < 4; ++i) fill(r.recs.emplace_back(), i, 24, 5);
        r.spacer = std::string(40, 's');  // vector block is not the arena tail
        if (r.recs.size() != r.recs.capacity()) fail("setup must leave the vector full");
        const auto before = snapshot(r);
        auto free = buf.get_free_memory();
        void* filler = nullptr;
        std::uint32_t filler_bytes = 0;
        if (free > slack + 32) {
            filler_bytes = static_cast<std::uint32_t>(free - slack - 32);
            filler = buf.arena()->allocate_aligned(filler_bytes, 8);
        }
        auto release_filler = [&] {
            if (filler) buf.arena()->deallocate(filler, filler_bytes);
            filler = nullptr;
        };
        try {
            // Grows: fresh block first, then the copy's own name/values storage.
            r.recs.push_back(r.recs[1]);
            release_filler();
            auto expected = before;
            expected.push_back(before[1]);
            if (!same(snapshot(r), expected)) fail("sweep success content");
            round_trip<Root>(buf, expected, "sweep success");
            ++succeeded;
        } catch (const xo::XBadAlloc&) {
            release_filler();
            if (!same(snapshot(r), before)) fail("sweep failure changed contents");
            round_trip<Root>(buf, before, "sweep failure");
            if (!buf.grow(65536)) fail("grow after failure");
            buf.root<Root>().recs.push_back(buf.root<Root>().recs[1]);
            auto expected = before;
            expected.push_back(before[1]);
            if (!same(snapshot(buf.root<Root>()), expected)) fail("retry content");
            round_trip<Root>(buf, expected, "retry");
            ++failed;
        }
    }
    if (failed == 0 || succeeded == 0) fail("sweep did not reach both outcomes");
    std::cout << "  sweep: " << failed << " failures recovered, " << succeeded << " successes\n";
}

void same_wires(const std::vector<std::string>& a, const std::vector<std::string>& b, const char* what) {
    if (a.size() != b.size() || a.empty()) fail(std::string(what) + ": checkpoint count");
    constexpr std::size_t header = sizeof(xo::XWireHeaderV2);
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].size() != b[i].size() || a[i].compare(header, std::string::npos, b[i], header, std::string::npos) != 0)
            fail(std::string(what) + ": SHIFT and per-element wires differ at checkpoint " + std::to_string(i));
}
} // namespace

int main() {
    same_wires(aliasing<ag::Root>(true), aliasing<ag::PlainRoot>(true), "tail expansion");
    same_wires(aliasing<ag::Root>(false), aliasing<ag::PlainRoot>(false), "fresh block");
    failure_sweep<ag::Root>();
    failure_sweep<ag::PlainRoot>();
    std::cout << "test_append_grow passed\n";
    return 0;
}
