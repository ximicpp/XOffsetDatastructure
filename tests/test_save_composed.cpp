// save_compact_into<T>(out, options, xassign(target, source)...): the wire of
// the root with each target subobject replaced by a copy of its source,
// written straight from the parts. For random fuzz models (sources and the
// draft in different buffers, holes, slack, inline and heap strings):
//   - byte-identical to: clone the draft, assign every target from its
//     source, save_compact with the same options (tight and slack);
//   - the draft and every source are unchanged;
//   - any output string (empty, tiny, reused, holding stale bytes, one byte
//     short) gives
//     the same bytes;
//   - a target that is the root, a nested member, or reads from the draft
//     itself works; overlapping targets, a target outside the root and a
//     target that is not a member subobject are rejected, and after every
//     rejection the output is empty.
// Fixed cases: a target with implicit padding is written with zero padding
// (whatever the source's padding bytes hold); heap strings of every length
// class (copied in 8-byte words: the source block holds capacity + 1 bytes
// padded to 8) give the same bytes as save_compact.
#include "fuzz/v2_fuzz_core.hpp"

namespace cmp {
struct Pod { std::uint8_t tag{}; std::uint64_t value{}; };                 // 7 padding bytes
struct Named { std::uint16_t id{}; xo::XString name; xo::XVector<std::uint32_t> ids; }; // 6 padding bytes
struct Root {
    std::uint32_t version{};                                                // 4 padding bytes
    Pod pod;
    Pod pods[3];
    Named named;
    Named other;
    xo::XVector<Named> list;
};
} // namespace cmp
XOFFSET_REGISTER_SCHEMA_NAME(cmp::Root, "test.save_composed.root")
static_assert(xo::has_implicit_padding_v<cmp::Pod> && xo::has_implicit_padding_v<cmp::Named>);

namespace {
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

constexpr xo::XCompactOptions kTight{.slack_div = 0};

template <class Assign>
std::string reference(Buffer& draft, xo::XCompactOptions options, Assign&& assign) {
    auto clone = typed_load(draft.save_verified());
    if (!clone) fail("clone of the draft rejected");
    assign(clone->root());
    return clone->save_compact(options);
}

template <class Compose>
void outputs_agree(const std::string& want, Compose&& compose, const std::string& where) {
    std::string empty;
    compose(empty);
    need(empty == want, where + ": composed wire differs (empty output)");
    std::string tiny;
    tiny.reserve(1);
    compose(tiny);
    need(tiny == want, where + ": composed wire differs (tiny output)");
    std::string stale(want.size() * 2 + 64, char(0xA5)); // stale bytes must not survive
    compose(stale);
    need(stale == want, where + ": composed wire differs (stale output)");
    compose(stale);                                       // reused capacity
    need(stale == want, where + ": composed wire differs (reused output)");
    std::string near;                                     // runs out at the end
    near.reserve(want.size() - 1);
    near.assign(near.capacity(), char(0x5A));
    compose(near);
    need(near == want, where + ": composed wire differs (near-miss output)");
}

template <class F>
bool throws(F&& f) {
    try { f(); } catch (const xo::XException&) { return true; }
    return false;
}

std::size_t composed_cases = 0;

void check(Buffer& draft, Buffer& a, Buffer& b, const std::string& where) {
    // save_verified (used by the references) normalizes the logical length
    // first; after that nothing here may change any of the three buffers.
    (void)draft.save_verified(); (void)a.save_verified(); (void)b.save_verified();
    const auto draft_before = draft.copy_bytes();
    const auto a_before = a.copy_bytes();
    const auto b_before = b.copy_bytes();
    auto& d = draft.root();
    for (const auto options : {kTight, xo::XCompactOptions{}}) {
        const std::string w = where + (options.slack_div ? " (slack)" : " (tight)");
        // Members from two other buffers.
        {
            const auto want = reference(draft, options, [&](fz::Root& r) { r.items = a.root().items; r.title = b.root().title; });
            outputs_agree(want, [&](std::string& out) {
                draft.save_compact_into(out, options, xo::xassign(d.items, a.root().items), xo::xassign(d.title, b.root().title));
            }, w + " items+title");
            auto loaded = typed_load(want);
            need(bool(loaded), w + ": composed wire rejected by load_verified");
            ++composed_cases;
        }
        // The whole root.
        {
            const auto want = a.save_compact(options);
            outputs_agree(want, [&](std::string& out) { draft.save_compact_into(out, options, xo::xassign(d, a.root())); },
                          w + " root");
        }
        // Scalars and a source inside the draft itself (simultaneous: the
        // swap reads both sources as they were).
        {
            const auto want = reference(draft, options, [&](fz::Root& r) {
                auto items_copy = typed_load(draft.save_verified());
                r.epoch = 77;
                r.tags = items_copy->root().tags;
                r.items = a.root().items;
            });
            const std::uint64_t epoch = 77;
            outputs_agree(want, [&](std::string& out) {
                draft.save_compact_into(out, options, xo::xassign(d.epoch, epoch), xo::xassign(d.tags, d.tags),
                                        xo::xassign(d.items, a.root().items));
            }, w + " scalar+self");
        }
    }
    // Every rejection happens before a byte is written and leaves the
    // output empty (it held a previous wire).
    const auto previous = draft.save_compact(kTight);
    auto rejected = [&](auto&& compose, const std::string& what) {
        std::string o = previous;
        need(throws([&] { compose(o); }), where + ": " + what + " accepted");
        need(o.empty(), where + ": " + what + " left bytes in the output");
    };
    rejected([&](std::string& o) { draft.save_compact_into(o, kTight, xo::xassign(d, a.root()), xo::xassign(d.items, a.root().items)); },
             "nested targets");
    rejected([&](std::string& o) { draft.save_compact_into(o, kTight, xo::xassign(d.items, a.root().items), xo::xassign(d.items, b.root().items)); },
             "duplicate targets");
    rejected([&](std::string& o) { draft.save_compact_into(o, kTight, xo::xassign(a.root().items, b.root().items)); },
             "target outside the root");
    // Inside the root's bytes, but a field of a container head, not a member
    // subobject the walk reaches.
    const std::uint32_t size_source = 3;
    rejected([&](std::string& o) { draft.save_compact_into(o, xo::XCompactOptions{}, xo::xassign(d.items, a.root().items), xo::xassign(d.title.size_, size_source)); },
             "target inside a container head");
    if (!d.items.empty())
        rejected([&](std::string& o) { draft.save_compact_into(o, kTight, xo::xassign(d.items[0].name, b.root().title)); },
                 "target inside a container");
    need(draft.copy_bytes() == draft_before, where + ": draft changed");
    need(a.copy_bytes() == a_before && b.copy_bytes() == b_before, where + ": a source changed");
}
using CmpBuffer = xo::TypedXBuffer<cmp::Root>;

CmpBuffer make_cmp(std::uint32_t seed) {
    auto b = CmpBuffer::create(4096, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.root();
    r.version = seed;
    r.pod.tag = std::uint8_t(seed);
    r.pod.value = seed * 11u;
    auto fill = [&](cmp::Named& n, std::uint32_t k) {
        n.id = std::uint16_t(k);
        n.name = std::string(k % 23, char('a' + k % 26));
        for (std::uint32_t i = 0; i < k % 5; ++i) n.ids.push_back(k * 100 + i);
    };
    for (std::uint32_t i = 0; i < 3; ++i) r.pods[i] = cmp::Pod{std::uint8_t(seed + i), seed * 7u + i};
    fill(r.named, seed + 1);
    fill(r.other, seed + 2);
    for (std::uint32_t i = 0; i < 6; ++i) fill(r.list.emplace_back(), seed + 10 + i);
    return b;
}

// A target with implicit padding: the composed wire carries zero padding
// bytes even when the source's padding holds garbage (a Pod on the stack, a
// buffer member whose padding was scribbled), and otherwise equals assign +
// save_compact.
void padded_targets() {
    auto draft = make_cmp(1);
    auto src = make_cmp(40);
    (void)draft.save_verified(); (void)src.save_verified();
    // Garbage in the padding of a source that lives in a buffer.
    auto* named_bytes = reinterpret_cast<unsigned char*>(&src.root().other);
    for (const auto& r : xo::detail::padding_ranges<cmp::Named>)
        for (std::size_t k = 0; k < r.count; ++k) std::memset(named_bytes + r.offset + k * r.stride, 0xC3, r.bytes);
    cmp::Pod pod;
    std::memset(static_cast<void*>(&pod), 0x5A, sizeof pod);
    pod.tag = 9;
    pod.value = 99;
    const auto draft_before = draft.copy_bytes();
    for (const auto options : {kTight, xo::XCompactOptions{}}) {
        auto clone = *typed_load<cmp::Root>(draft.save_verified());
        auto& c = clone.root();
        c.pod = pod;
        c.pods[1] = pod;
        c.named = src.root().other;
        c.other = src.root().list[3];
        xo::detail::zero_implicit_padding(&c.pod);
        xo::detail::zero_implicit_padding(&c.pods[1]);
        xo::detail::zero_implicit_padding(&c.named);
        xo::detail::zero_implicit_padding(&c.other);
        const auto want = clone.save_compact(options);
        std::string out(want.size() + 32, char(0xA5));
        auto& d = draft.root();
        draft.save_compact_into(out, options, xo::xassign(d.pod, pod), xo::xassign(d.pods[1], pod),
                                xo::xassign(d.named, src.root().other), xo::xassign(d.other, src.root().list[3]));
        need(out == want, std::string("padded targets: composed wire differs") + (options.slack_div ? " (slack)" : " (tight)"));
        auto loaded = *typed_load<cmp::Root>(out);
        const auto& l = loaded.root();
        need(l.pod.tag == 9 && l.pod.value == 99 && l.pods[1].value == 99 && l.pods[2].value == d.pods[2].value &&
                 l.named.id == src.root().other.id && l.named.name.view() == src.root().other.name.view() &&
                 l.other.ids.size() == src.root().list[3].ids.size(),
             "padded targets: loaded values differ");
        // The padding of every target in the wire is zero.
        const char* root = out.data() + sizeof(xo::XWireHeaderV2); // the root is the first payload byte on the wire
        auto zero_padding = [&](std::size_t at, const auto& ranges) {
            for (const auto& r : ranges)
                for (std::size_t k = 0; k < r.count; ++k)
                    for (std::size_t i = 0; i < r.bytes; ++i)
                        need(root[at + r.offset + k * r.stride + i] == 0, "padded targets: nonzero padding byte in the wire");
        };
        zero_padding(offsetof(cmp::Root, pod), xo::detail::padding_ranges<cmp::Pod>);
        zero_padding(offsetof(cmp::Root, pods) + sizeof(cmp::Pod), xo::detail::padding_ranges<cmp::Pod>);
        zero_padding(offsetof(cmp::Root, named), xo::detail::padding_ranges<cmp::Named>);
        zero_padding(offsetof(cmp::Root, other), xo::detail::padding_ranges<cmp::Named>);
    }
    need(draft.copy_bytes() == draft_before, "padded targets: draft changed");
}

// Heap strings are copied in 8-byte words: the source block of a string
// with capacity c holds block_bytes_of(c + 1) bytes, a multiple of 8 and at
// least the c + 1 rounded up to 8 that the copy reads. Every length 11..80
// (each residue mod 8, tight capacity and spare capacity) gives the
// save_compact bytes.
void heap_string_words() {
    for (std::uint32_t c = 1; c < 200; ++c) {
        const auto block = xo::fixed_detail::block_bytes_of(c + 1);
        need(block % 8 == 0 && block >= ((std::size_t(c) + 8u) & ~std::size_t(7u)), "string block not padded to 8");
    }
    auto draft = make_cmp(2);
    auto src = make_cmp(3);
    std::string out;
    for (std::uint32_t n = 11; n <= 80; ++n) {
        auto& s = src.root();
        s.named.name = std::string(n, char('A' + n % 26));
        s.other.name.reserve(n + n % 13);
        s.other.name = std::string(n, char('a' + n % 26));
        (void)src.save_verified();
        auto clone = *typed_load<cmp::Root>(draft.save_verified());
        clone.root().named = s.named;
        clone.root().other = s.other;
        const auto want = clone.save_compact(kTight);
        auto& d = draft.root();
        draft.save_compact_into(out, kTight, xo::xassign(d.named, s.named), xo::xassign(d.other, s.other));
        need(out == want, "heap string of " + std::to_string(n) + " bytes: composed wire differs");
    }
}
} // namespace

int main(int argc, char** argv) {
    padded_targets();
    heap_string_words();
    const std::size_t cases = argc > 1 ? std::stoul(argv[1]) : 300;
    std::mt19937_64 rng(argc > 2 ? std::stoull(argv[2]) : 20261024);
    for (std::size_t i = 0; i < cases; ++i) {
        auto make = [&] {
            auto w = build_wire(rng);
            auto b = typed_load(w);
            need(bool(b), "case " + std::to_string(i) + ": generated wire rejected");
            if (rng() % 2) mutate_valid(b->root(), rng);
            return std::move(*b);
        };
        auto draft = make();
        auto a = make();
        auto b = make();
        check(draft, a, b, "case " + std::to_string(i));
    }
    std::cout << "test_save_composed: " << cases << " random cases, " << composed_cases
              << " composed wires byte-identical to assign + save_compact\n";
    return 0;
}
