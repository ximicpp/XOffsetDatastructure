// ============================================================================
// Test: T4 subtree copy (differential)
// Purpose: container copy assignment and allocator-extended copy construction
//          copy the source subtree into one fresh segment: memcpy + fix-up walk
//          when the source is one contiguous range, otherwise a compacting
//          preorder walk (packed, capacity = size). Each case clones one random arena three
//          times: A and A2 run with the fast path, B with the element-wise
//          reference path (XOFFSET_TEST_HOOKS switch). Checked:
//   a. determinism: A and A2 payload, free table and save bytes are equal
//      (A and B bytes are NOT compared);
//   b. legality: save_verified succeeds and its output loads again;
//   c. logical equality: destination digest == source digest == B's, and the
//      whole root digest of A == B (sizes, scalars, strings, key order only);
//   d. the source is unchanged (another arena byte for byte, the same arena
//      by digest);
//   e. hygiene: zero implicit padding (sources carry dirty padding), zero
//      spare capacity and block tails, zero inline string tails, zero holes;
//   f. the same random mutations on A and B keep b and c;
//   g. allocation failure in an Explicit arena: A ends byte-identical to B
//      and logically unchanged (the fast path falls back before any change);
//   h. aliasing: self, siblings, cousins in one root, another arena, element
//      push_back from the same vector;
//   i. coverage: whole and compact copies, every skip/fallback/retirement
//      kind, > 4096 B segments; a compacted copy is one packed range;
//   j. steady state: repeated same-shape assignment stops growing used_end
//      from the third assignment (reported for B as well).
//
// usage: test_subtree_copy [cases] [seed]
// ============================================================================

#define XOFFSET_TEST_HOOKS 1
#include "../xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace xd = XOffsetDatastructure::detail;

namespace sc {
struct Pad {             // trivially copyable, 4 padding bytes
    std::uint32_t a{};
    std::uint64_t b{};
};
struct Leaf {            // 6 padding bytes after k
    std::uint16_t k{};
    xo::XString s;
    xo::XVector<std::uint32_t> v;
};
struct Mid {
    std::uint32_t id{};
    xo::XString name;
    xo::XVector<Leaf> leaves;
    xo::XVector<Pad> pads;
    xo::XMap<std::uint32_t, xo::XString> tags;
    xo::XSet<std::uint32_t> keys;
    std::uint32_t tail[3]{};
};
struct Root {
    std::uint64_t epoch{};
    xo::XVector<Mid> ma, mb;
    xo::XVector<Pad> pa, pb;
    xo::XVector<xo::XVector<std::uint32_t>> na, nb;
    xo::XMap<xo::XString, xo::XString> da, db;
    xo::XVector<xo::XString> sa, sb;
    xo::XVector<xo::XString> junk;
};
} // namespace sc

XOFFSET_REGISTER_SCHEMA_NAME(sc::Root, "test.subtree_copy.root")

namespace XOffsetDatastructure {
template<> struct wire_record_layout<sc::Pad> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<sc::Leaf> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<sc::Mid> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<sc::Root> { static constexpr bool ordinary_members = true; };
}

static_assert(xd::subtree_copy_supported<sc::Pad>());
static_assert(xd::subtree_copy_supported<sc::Leaf>());
static_assert(xd::subtree_copy_supported<sc::Mid>());
static_assert(xd::subtree_copy_supported<xo::XString>());
static_assert(xd::subtree_copy_supported<xo::XVector<std::uint32_t>>());
static_assert(xd::subtree_copy_supported<xo::XKeyValue<xo::XString, xo::XString>>());
static_assert(xd::subtree_copy_supported<xo::XKeyValue<std::uint32_t, xo::XString>>());
static_assert(xo::has_implicit_padding_v<sc::Pad> && xo::has_implicit_padding_v<sc::Leaf>);

namespace {
using Buffer = xo::TypedXBuffer<sc::Root>;
constexpr std::size_t kLimit = std::size_t(1) << 23;
using Rng = std::mt19937_64;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_subtree_copy FAILED: " << message << "\n";
    std::abort();
}

xo::XBuffer::MaximumCapacity growable() {
    return xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
}

Buffer load(const std::string& wire) { return Buffer::load_verified(wire, growable()); }

// ---- logical digest: sizes, scalars, string bytes, element order ----------
template <class T> void digest(const T& value, std::string& out);

template <class T, std::size_t I>
void digest_member(const T& value, std::string& out) {
    constexpr auto member = std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unchecked())[I];
    digest(value.[:member:], out);
}

template <class T>
void digest(const T& value, std::string& out) {
    if constexpr (xd::is_fixed_string_v<T>) {
        const auto view = value.view();
        const auto n = static_cast<std::uint32_t>(view.size());
        out.append(reinterpret_cast<const char*>(&n), 4);
        out.append(view);
    } else if constexpr (xd::is_fixed_vector_v<T>) {
        const auto n = value.size();
        out.append(reinterpret_cast<const char*>(&n), 4);
        for (const auto& element : value) digest(element, out);
    } else if constexpr (std::is_array_v<T>) {
        for (const auto& element : value) digest(element, out);
    } else if constexpr (std::is_scalar_v<T>) {
        out.append(reinterpret_cast<const char*>(&value), sizeof(T));
    } else {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (digest_member<T, I>(value, out), ...);
        }(std::make_index_sequence<xd::reflected_member_count<T>()>{});
    }
}

template <class T> std::string digest_of(const T& value) {
    std::string out;
    digest(value, out);
    return out;
}

// ---- hygiene: padding, spare capacity, block tails, inline tails ----------
bool all_zero(const void* p, std::size_t n) {
    auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i) if (b[i]) return false;
    return true;
}

template <class T> void hygiene(const T& value, const std::string& where, bool padding = false);

template <class T, std::size_t I>
void hygiene_member(const T& value, const std::string& where, bool padding) {
    constexpr auto member = std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unchecked())[I];
    hygiene(value.[:member:], where, padding);
}

// Spare capacity, block tails and inline tails everywhere; padding only where
// asked (a source may carry dirty padding the caller wrote directly).
template <class T>
void hygiene(const T& value, const std::string& where, bool padding) {
    if constexpr (xd::is_fixed_string_v<T>) {
        const xo::XFixedString& head = value;
        auto* bytes = reinterpret_cast<const unsigned char*>(&head);
        if (head.capacity_ & xo::XFixedString::inline_flag) {
            const auto len = (head.capacity_ >> 24) & 0x7fu;
            if (!all_zero(bytes + 4 + len, 11 - len)) fail(where + ": inline string tail not zero");
        } else if (head.capacity_ != 0) {
            const auto* data = value.data();
            const auto block = xo::fixed_detail::block_bytes_of(head.capacity_ + 1u);
            if (!all_zero(data + head.size_, block - head.size_)) fail(where + ": string tail not zero");
        }
    } else if constexpr (xd::is_fixed_vector_v<T>) {
        using E = typename T::value_type;
        const xo::XFixedVector<E>& head = value;
        if (head.capacity_ != 0) {
            auto* data = reinterpret_cast<const unsigned char*>(head.data());
            const auto block = xo::fixed_detail::block_bytes_of(static_cast<std::uint32_t>(head.capacity_ * sizeof(E)));
            const auto live = std::size_t(head.size_) * sizeof(E);
            if (!all_zero(data + live, block - live)) fail(where + ": spare capacity not zero");
        }
        for (const auto& element : value) {
            auto* bytes = reinterpret_cast<const unsigned char*>(&element);
            if (padding)
                for (const auto& r : xd::padding_ranges<E>)
                    for (std::size_t i = 0; i < r.count; ++i)
                        if (!all_zero(bytes + r.offset + i * r.stride, r.bytes)) fail(where + ": padding not zero");
            hygiene(element, where, padding);
        }
    } else if constexpr (std::is_array_v<T>) {
        for (const auto& element : value) hygiene(element, where, padding);
    } else if constexpr (std::is_class_v<T>) {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (hygiene_member<T, I>(value, where, padding), ...);
        }(std::make_index_sequence<xd::reflected_member_count<T>()>{});
    }
}

void holes_zero(Buffer& buf, const std::string& where) {
    auto* base = static_cast<const unsigned char*>(buf.get_address());
    buf.for_each_free_block([&](xo::XBufferCore::WireHole hole) {
        if (!all_zero(base + hole.offset, hole.bytes)) fail(where + ": hole not zero");
    });
}

std::uint64_t used_end(Buffer& buf) { return buf.arena()->used_end; }

// Payload [0, used_end) and the free table in canonical order.
std::string state_bytes(Buffer& buf) {
    const auto used = used_end(buf);
    std::string out(static_cast<const char*>(buf.get_address()), std::size_t(used));
    buf.for_each_free_block([&](xo::XBufferCore::WireHole hole) {
        out.append(reinterpret_cast<const char*>(&hole), sizeof(hole));
    });
    return out;
}

void legal(Buffer& buf, const std::string& where) {
    std::string wire;
    try {
        wire = buf.save_verified();
    } catch (const xo::XException& e) {
        fail(where + ": save_verified threw: " + e.what());
    }
    try {
        auto again = Buffer::load_verified(wire);
        if (digest_of(again.root()) != digest_of(buf.root())) fail(where + ": reloaded digest differs");
    } catch (const xo::XException& e) {
        fail(where + ": load_verified of the saved wire threw: " + e.what());
    }
}

// ---- generator -------------------------------------------------------------
std::string text(Rng& rng) {
    static constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::size_t n;
    switch (rng() % 4) {
    case 0: n = 0; break;
    case 1: n = 1 + rng() % 10; break;        // inline
    default: n = 11 + rng() % 40; break;      // heap
    }
    std::string s(n, 'x');
    for (auto& c : s) c = alphabet[rng() % alphabet.size()];
    return s;
}

// Heap strings of at most 10 bytes: a long value then a short one.
void set_text(xo::XString& s, Rng& rng) {
    if (rng() % 6 == 0) s = std::string(20 + rng() % 20, 'h');
    s = text(rng);
}

void dirty_padding(void* element, std::size_t at, std::size_t n) {
    std::memset(static_cast<char*>(element) + at, 0xA5, n);
}

void fill_leaf(sc::Leaf& leaf, Rng& rng, bool slack, bool dirty) {
    leaf.k = static_cast<std::uint16_t>(rng());
    set_text(leaf.s, rng);
    const auto n = rng() % 7;
    if (slack) leaf.v.reserve(n + 16 + rng() % 32);
    else if (rng() % 8 == 0) leaf.v.reserve(1 + rng() % 4);  // capacity > size, possibly size 0
    for (std::size_t i = 0; i < n; ++i) leaf.v.push_back(static_cast<std::uint32_t>(rng()));
    if (dirty) dirty_padding(&leaf, 2, 6);
}

void fill_mid(sc::Mid& mid, Rng& rng, bool slack, bool dirty, std::size_t width) {
    mid.id = static_cast<std::uint32_t>(rng());
    set_text(mid.name, rng);
    const auto leaves = rng() % (width + 1);
    if (rng() % 2) mid.leaves.reserve(leaves);
    for (std::size_t i = 0; i < leaves; ++i) fill_leaf(mid.leaves.emplace_back(), rng, slack, dirty);
    const auto pads = rng() % 5;
    for (std::size_t i = 0; i < pads; ++i) {
        auto& pad = mid.pads.emplace_back();
        pad.a = static_cast<std::uint32_t>(rng());
        pad.b = rng();
        if (dirty) dirty_padding(&pad, 4, 4);
    }
    const auto tags = rng() % 4;
    for (std::size_t i = 0; i < tags; ++i) mid.tags[static_cast<std::uint32_t>(rng() % 64)] = text(rng);
    if (dirty) {
        for (auto& entry : mid.tags) dirty_padding(&entry, 4, 4);
    }
    const auto keys = rng() % 4;
    for (std::size_t i = 0; i < keys; ++i) mid.keys.insert(static_cast<std::uint32_t>(rng() % 64));
    for (auto& t : mid.tail) t = static_cast<std::uint32_t>(rng());
    if (dirty) dirty_padding(&mid, 4, 4);
}

// Layout modes: 0 fresh (reserved, preorder bump), 1 grown by push_back
// (holes), 2 interleaved with the sibling, 3 slack-heavy, 4 large (> 4096 B).
void build_mids(xo::XVector<sc::Mid>& a, xo::XVector<sc::Mid>& b, Rng& rng) {
    const int mode = int(rng() % 5);
    const bool dirty = rng() % 3 == 0;
    std::size_t n = 1 + rng() % 4;
    std::size_t width = 4;
    if (mode == 4) { n = 6 + rng() % 6; width = 10; }
    const bool reserved = mode == 0 || mode == 4;
    if (reserved) a.reserve(n);
    if (mode == 2) {
        for (std::size_t i = 0; i < n; ++i) {
            fill_mid(a.emplace_back(), rng, false, dirty, width);
            fill_mid(b.emplace_back(), rng, false, dirty, width);
        }
        return;
    }
    for (std::size_t i = 0; i < n; ++i) fill_mid(a.emplace_back(), rng, mode == 3, dirty, width);
    if (mode == 4) {
        // Built elsewhere and copied in: element-wise (or subtree) copies
        // lay a large source out as one range.
        auto scratch = Buffer::create(4096, growable());
        auto& big = scratch.root().mb;
        for (std::size_t i = 0; i < n; ++i) fill_mid(big.emplace_back(), rng, false, dirty, width);
        b = big;
        return;
    }
    const std::size_t m = 1 + rng() % 4;
    if (reserved) b.reserve(m);  // after a's subtree, so b's stays one range
    for (std::size_t i = 0; i < m; ++i) fill_mid(b.emplace_back(), rng, mode == 3, dirty, width);
}

void build_root(sc::Root& r, Rng& rng) {
    r.epoch = rng();
    // Junk first so some later blocks reuse its holes.
    const auto junk = rng() % 8;
    for (std::size_t i = 0; i < junk; ++i) r.junk.push_back(std::string(11 + rng() % 60, 'j'));
    build_mids(r.ma, r.mb, rng);
    for (int side = 0; side < 2; ++side) {
        auto& pads = side ? r.pb : r.pa;
        const auto n = rng() % 6;
        for (std::size_t i = 0; i < n; ++i) {
            auto& pad = pads.emplace_back();
            pad.a = static_cast<std::uint32_t>(rng());
            pad.b = rng();
            if (rng() % 2) dirty_padding(&pad, 4, 4);
        }
        auto& nested = side ? r.nb : r.na;
        const auto rows = rng() % 4;
        for (std::size_t i = 0; i < rows; ++i) {
            auto& row = nested.emplace_back();
            const auto cols = rng() % 6;
            for (std::size_t j = 0; j < cols; ++j) row.push_back(static_cast<std::uint32_t>(rng()));
        }
        auto& dict = side ? r.db : r.da;
        const auto entries = rng() % 4;
        for (std::size_t i = 0; i < entries; ++i) dict[text(rng)] = text(rng);
        auto& strs = side ? r.sb : r.sa;
        const auto count = rng() % 5;
        for (std::size_t i = 0; i < count; ++i) set_text(strs.emplace_back(), rng);
    }
    if (rng() % 2) r.junk.clear();
    else if (!r.junk.empty() && rng() % 2) r.junk.erase(r.junk.begin());
}

std::string build_wire(Rng& rng) {
    auto buf = Buffer::create(4096, growable());
    build_root(buf.root(), rng);
    return buf.save_verified();
}

// ---- copy operations -------------------------------------------------------
struct Op {
    int kind = 0;
    std::uint32_t i = 0, j = 0;
    bool cross = false;
};

constexpr int kKinds = 11;

std::uint32_t pick(Rng& rng, std::uint32_t n) { return n ? static_cast<std::uint32_t>(rng() % n) : 0; }

// Indices come from the destination and the source the op will use.
Op choose_op(Rng& rng, sc::Root& dst, sc::Root& other, bool allow_cross = true) {
    Op op;
    op.kind = int(rng() % kKinds);
    op.cross = allow_cross && rng() % 3 == 0;
    op.i = pick(rng, dst.ma.size());
    op.j = pick(rng, (op.cross ? other : dst).mb.size());
    return op;
}

// Applies op to dst (src is dst itself unless op.cross). Returns the source
// subtree's digest taken before the copy, or "" when nothing was copied.
std::string apply_op(sc::Root& dst, sc::Root& other, const Op& op) {
    sc::Root& src = op.cross ? other : dst;
    switch (op.kind) {
    case 0: { auto d = digest_of(src.mb); dst.ma = src.mb; return d; }
    case 1: {
        if (dst.ma.empty() || src.mb.empty()) return {};
        auto d = digest_of(src.mb[op.j].leaves);
        dst.ma[op.i].leaves = src.mb[op.j].leaves;
        return d;
    }
    case 2: { auto d = digest_of(src.pb); dst.pa = src.pb; return d; }
    case 3: { auto d = digest_of(src.nb); dst.na = src.nb; return d; }
    case 4: { auto d = digest_of(src.db); dst.da = src.db; return d; }
    case 5: { auto d = digest_of(src.sb); dst.sa = src.sb; return d; }
    case 6: {
        if (dst.ma.empty() || src.mb.empty()) return {};
        auto d = digest_of(src.mb[op.j].tags);
        dst.ma[op.i].tags = src.mb[op.j].tags;
        return d;
    }
    case 7: { // record copy assignment: memberwise container assignments
        if (dst.ma.empty() || src.mb.empty()) return {};
        auto d = digest_of(src.mb[op.j]);
        dst.ma[op.i] = src.mb[op.j];
        return d;
    }
    case 8: { // allocator-extended copy construction of every container member
        if (src.mb.empty()) return {};
        auto d = digest_of(src.mb[op.j]);
        dst.ma.push_back(src.mb[op.j]);
        return d;
    }
    case 9: { // cousins in one root (always the destination's own root)
        if (dst.ma.size() < 2) return {};
        const auto k = (op.i + 1) % dst.ma.size();
        auto d = digest_of(dst.ma[k].leaves);
        dst.ma[op.i].leaves = dst.ma[k].leaves;
        return d;
    }
    default: { // self assignment and push_back of an element of the same vector
        auto& v = dst.ma;
        v = v;
        if (v.empty()) return {};
        auto d = digest_of(v[op.i]);
        v.push_back(v[op.i]);
        return d;
    }
    }
}

// Calls f on the destination subtree an op wrote.
template <class F>
void visit_written(sc::Root& dst, const Op& op, std::size_t ma_size_before, F&& f) {
    switch (op.kind) {
    case 0: f(dst.ma); break;
    case 1: case 9: f(dst.ma[op.i].leaves); break;
    case 2: f(dst.pa); break;
    case 3: f(dst.na); break;
    case 4: f(dst.da); break;
    case 5: f(dst.sa); break;
    case 6: f(dst.ma[op.i].tags); break;
    case 7: f(dst.ma[op.i]); break;
    default: f(dst.ma[ma_size_before]); break;  // 8, 10
    }
}

std::string written_digest(sc::Root& dst, const Op& op, std::size_t ma_size_before) {
    std::string out;
    visit_written(dst, op, ma_size_before, [&](const auto& v) { out = digest_of(v); });
    return out;
}

// ---- mutations (f) ---------------------------------------------------------
void mutate(sc::Root& r, sc::Root& other, Rng& rng) {
    switch (rng() % 8) {
    case 0:
        if (!r.ma.empty()) fill_leaf(r.ma[pick(rng, r.ma.size())].leaves.emplace_back(), rng, false, false);
        break;
    case 1:
        if (!r.ma.empty()) {
            auto& leaves = r.ma[pick(rng, r.ma.size())].leaves;
            if (!leaves.empty()) leaves.erase(leaves.begin() + pick(rng, leaves.size()));
        }
        break;
    case 2:
        if (!r.ma.empty()) set_text(r.ma[pick(rng, r.ma.size())].name, rng);
        break;
    case 3:
        if (!r.ma.empty()) {
            auto& tags = r.ma[pick(rng, r.ma.size())].tags;
            if (rng() % 2) tags[static_cast<std::uint32_t>(rng() % 64)] = text(rng);
            else (void)tags.erase(static_cast<std::uint32_t>(rng() % 64));
        }
        break;
    case 4:
        if (rng() % 2) r.ma.clear(); else r.sa.clear();
        break;
    case 5:
        r.pa.push_back(sc::Pad{static_cast<std::uint32_t>(rng()), rng()});
        break;
    case 6:
        if (!r.na.empty()) r.na[pick(rng, r.na.size())].push_back(static_cast<std::uint32_t>(rng()));
        break;
    default: {
        auto op = choose_op(rng, r, other);
        (void)apply_op(r, other, op);
        break;
    }
    }
}

struct Coverage {
    std::size_t cases = 0, large_hits = 0, explicit_throws = 0, explicit_only_b = 0, packed_checks = 0;
};

xd::SubtreeCopyStats& stats() { return xd::subtree_copy_stats; }

void run_case(std::size_t index, Rng& rng, Coverage& cov) {
    const auto where = "case " + std::to_string(index);
    const auto wire = build_wire(rng);
    const auto source_wire = build_wire(rng);
    auto a = load(wire);
    auto a2 = load(wire);
    auto b = load(wire);
    auto s = load(source_wire);
    const auto s_before = state_bytes(s);
    const auto root_before = digest_of(a.root());

    const auto op = choose_op(rng, a.root(), s.root());
    const auto ma_before = a.root().ma.size();

    const auto hits_before = stats().whole_hits + stats().compact_hits;
    const auto compact_before = stats().compact_hits;
    const auto bytes_before = stats().bytes;
    std::string src_a, src_a2, src_b;
    src_a = apply_op(a.root(), s.root(), op);
    // A compacted copy is one packed preorder range with no spare capacity,
    // so the next copy from it can take the whole path.
    if (op.kind == 0 && stats().compact_hits == compact_before + 1) {
        const auto& ma = a.root().ma;
        xd::SubtreeExtent extent;
        xd::subtree_measure_elements<sc::Mid>(ma.data(), ma.size_, ma.capacity_, extent);
        if (!extent.contiguous() || extent.sum != extent.exact || extent.sum != stats().bytes - bytes_before)
            fail(where + ": compacted copy is not one packed range");
        ++cov.packed_checks;
    }
    if (stats().whole_hits + stats().compact_hits > hits_before && stats().bytes - bytes_before > 4096) ++cov.large_hits;
    src_a2 = apply_op(a2.root(), s.root(), op);
    xd::subtree_copy_hook_enabled = false;
    src_b = apply_op(b.root(), s.root(), op);
    xd::subtree_copy_hook_enabled = true;
    if (src_a != src_b || src_a != src_a2) fail(where + ": source digests differ before the copy");

    // a. determinism
    if (state_bytes(a) != state_bytes(a2)) fail(where + ": A and A2 states differ (kind " + std::to_string(op.kind) + ")");
    if (a.save_verified() != a2.save_verified()) fail(where + ": A and A2 saves differ");
    // b. legality
    legal(a, where + " A");
    legal(b, where + " B");
    // c. logical equality
    if (!src_a.empty()) {
        const auto written = written_digest(a.root(), op, ma_before);
        if (written != src_a) fail(where + ": destination digest != source digest (kind " + std::to_string(op.kind) + ")");
        if (written != written_digest(b.root(), op, ma_before)) fail(where + ": destination digest A != B");
    }
    if (digest_of(a.root()) != digest_of(b.root())) fail(where + ": root digest A != B (kind " + std::to_string(op.kind) + ")");
    (void)root_before;
    // d. source unchanged
    if (state_bytes(s) != s_before) fail(where + ": source arena changed");
    // e. hygiene (padding of the written subtree, which element-wise also zeroes)
    hygiene(a.root(), where + " A");
    if (!src_a.empty())
        visit_written(a.root(), op, ma_before, [&](const auto& v) { hygiene(v, where + " A written", true); });
    holes_zero(a, where + " A");

    // f. the same mutations on A and B
    const auto mseed = rng();
    Rng ma(mseed), mb(mseed);
    for (int step = 0; step < 6; ++step) {
        mutate(a.root(), s.root(), ma);
        xd::subtree_copy_hook_enabled = false;
        mutate(b.root(), s.root(), mb);
        xd::subtree_copy_hook_enabled = true;
        const auto at = where + " step " + std::to_string(step);
        if (digest_of(a.root()) != digest_of(b.root())) fail(at + ": root digest A != B after mutation");
        legal(a, at + " A");
        hygiene(a.root(), at + " A");
        holes_zero(a, at + " A");
    }
    if (state_bytes(s) != s_before) fail(where + ": source arena changed by mutations");
    ++cov.cases;
}

// g. Explicit arena with no room: the fast path falls back before any
// change, so A ends exactly as the element-wise B (success or failure).
void run_explicit_case(std::size_t index, Rng& rng, Coverage& cov) {
    const auto where = "explicit case " + std::to_string(index);
    const auto wire = build_wire(rng);
    xo::XWireHeaderV2 header{};
    std::memcpy(&header, wire.data(), sizeof(header));
    const auto room = std::size_t(header.used_bytes) + 8u * (rng() % 64);
    auto cap = xo::XBuffer::maximum_capacity(room);
    auto a = Buffer::load_verified(wire, cap);
    auto b = Buffer::load_verified(wire, cap);
    const auto before = digest_of(a.root());
    Op op = choose_op(rng, a.root(), a.root(), false);
    // One container assignment each (a record assignment is several).
    if (op.kind == 7 || op.kind == 8 || op.kind == 10) op.kind = 0;
    bool threw_a = false, threw_b = false;
    try { (void)apply_op(a.root(), a.root(), op); } catch (const xo::XBadAlloc&) { threw_a = true; }
    xd::subtree_copy_hook_enabled = false;
    try { (void)apply_op(b.root(), b.root(), op); } catch (const xo::XBadAlloc&) { threw_b = true; }
    xd::subtree_copy_hook_enabled = true;
    // One range may fit where many blocks do not (A succeeds, B throws);
    // when A throws, its fast path fell back first, so it is exactly B.
    if (threw_a) {
        ++cov.explicit_throws;
        if (!threw_b) fail(where + ": A threw XBadAlloc, B did not");
        if (state_bytes(a) != state_bytes(b)) fail(where + ": A differs from B after XBadAlloc");
        if (digest_of(a.root()) != before) fail(where + ": strong guarantee broken (kind " + std::to_string(op.kind) + ")");
    } else if (threw_b) {
        ++cov.explicit_only_b;
    } else if (digest_of(a.root()) != digest_of(b.root())) {
        fail(where + ": root digest A != B");
    }
    legal(a, where + " A");
    hygiene(a.root(), where + " A");
    holes_zero(a, where + " A");
}

// j. Steady state of one destination assigned the same-shape source.
void steady_state(Rng& rng) {
    auto source = Buffer::create(4096, growable());
    {
        auto scratch = Buffer::create(4096, growable());
        scratch.root().mb.reserve(4);
        for (int i = 0; i < 4; ++i) fill_mid(scratch.root().mb.emplace_back(), rng, false, false, 5);
        source.root().mb = scratch.root().mb;  // a contiguous copy
    }
    for (int side = 0; side < 2; ++side) {
        const bool fast = side == 0;
        auto dst = load(build_wire(rng));
        xd::subtree_copy_hook_enabled = fast;
        const auto hits_before = stats().whole_hits;
        std::vector<std::uint64_t> curve;
        for (int k = 0; k < 100; ++k) {
            dst.root().ma = source.root().mb;
            curve.push_back(used_end(dst));
        }
        xd::subtree_copy_hook_enabled = true;
        if (fast) {
            if (stats().whole_hits - hits_before != 100) fail("steady state: not every assignment took the whole path");
            for (std::size_t k = 2; k < curve.size(); ++k)
                if (curve[k] != curve[1]) fail("steady state: used_end grew at assignment " + std::to_string(k + 1));
        }
        legal(dst, "steady state");
        std::cout << "  steady state " << (fast ? "A" : "B") << ": used_end after 1/2/3/100 = "
                  << curve[0] << "/" << curve[1] << "/" << curve[2] << "/" << curve.back()
                  << " free_bytes=" << dst.free_block_bytes()
                  << " wire_bytes=" << dst.save_verified().size() << "\n";
    }
    // Random shapes: reported only.
    for (int side = 0; side < 2; ++side) {
        const bool fast = side == 0;
        Rng shapes(4242);
        auto dst = load(build_wire(shapes));
        auto src = load(build_wire(shapes));
        xd::subtree_copy_hook_enabled = fast;
        for (int k = 0; k < 100; ++k) {
            src.root().ma.clear();
            src.root().mb.clear();
            build_mids(src.root().ma, src.root().mb, shapes);
            dst.root().ma = src.root().mb;
        }
        xd::subtree_copy_hook_enabled = true;
        legal(dst, "random shapes");
        std::cout << "  random shapes " << (fast ? "A" : "B") << ": used_end=" << used_end(dst)
                  << " free_bytes=" << dst.free_block_bytes()
                  << " wire_bytes=" << dst.save_verified().size() << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::size_t cases = argc > 1 ? std::stoul(argv[1]) : 3000;
    const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 20261022;
    std::cout << "test_subtree_copy: cases=" << cases << " seed=" << seed << "\n";
    Rng rng(seed);
    Coverage cov;
    stats() = {};
    for (std::size_t c = 0; c < cases; ++c) run_case(c, rng, cov);
    for (std::size_t c = 0; c < cases / 4 + 50; ++c) run_explicit_case(c, rng, cov);
    const auto st = stats();
    std::cout << "  whole_hits=" << st.whole_hits << " compact_hits=" << st.compact_hits
              << " pod_hits=" << st.pod_hits << " bytes=" << st.bytes << " large_hits=" << cov.large_hits << "\n"
              << "  whole skipped: layout=" << st.whole_skipped_layout << " slack=" << st.whole_skipped_slack
              << "  element-wise: type=" << st.fallback_type << " alloc=" << st.fallback_alloc << "\n"
              << "  retire whole=" << st.whole_retires << " piecewise=" << st.piecewise_retires
              << " explicit_throws=" << cov.explicit_throws
              << " explicit_only_b=" << cov.explicit_only_b << " packed_checks=" << cov.packed_checks << "\n";
    // i. coverage
    if (!st.whole_hits || !st.compact_hits || !st.pod_hits || !st.whole_skipped_layout ||
        !st.whole_skipped_slack || !st.fallback_alloc || !st.whole_retires || !st.piecewise_retires || !cov.large_hits ||
        !cov.explicit_throws || !cov.packed_checks)
        fail("coverage: a hit, fallback or retirement kind never occurred");
    steady_state(rng);
    std::cout << "test_subtree_copy: " << cov.cases << " differential cases passed\n";
    return 0;
}
