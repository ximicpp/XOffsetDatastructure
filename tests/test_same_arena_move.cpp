// O3 contract regression. Root builds/runs normal and sanitizer targets.
// The bad_alloc hook is armed only around the operation, never fixture setup.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <span>
#include <string>
#include <utility>
#ifdef _WIN32
#include <malloc.h>
#endif
#include "xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

namespace new_failure {
thread_local bool armed = false;
thread_local long remaining = -1;
thread_local std::size_t attempts = 0, hits = 0;
void check() {
    if (!armed) return;
    ++attempts;
    if (remaining == 0) {
        remaining = -1;
        ++hits;
        throw std::bad_alloc();
    }
    if (remaining > 0) --remaining;
}
struct Arm {
    explicit Arm(long point) {
        attempts = hits = 0;
        remaining = point;
        armed = true;
    }
    ~Arm() { armed = false; remaining = -1; }
    Arm(const Arm&) = delete;
    Arm& operator=(const Arm&) = delete;
};
void* allocate(std::size_t n) {
    check();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* aligned_allocate(std::size_t n, std::size_t alignment) {
    check();
    n = n ? n : 1;
#ifdef _WIN32
    if (auto* p = _aligned_malloc(n, alignment)) return p;
#else
    if (n > std::numeric_limits<std::size_t>::max() - (alignment - 1)) throw std::bad_alloc();
    n = (n + alignment - 1) / alignment * alignment;
    if (auto* p = std::aligned_alloc(alignment, n)) return p;
#endif
    throw std::bad_alloc();
}
void aligned_free(void* p) noexcept {
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
void* operator new(std::size_t n) { return new_failure::allocate(n); }
void* operator new[](std::size_t n) { return new_failure::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try { return new_failure::allocate(n); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    try { return new_failure::allocate(n); } catch (...) { return nullptr; }
}
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) {
    return new_failure::aligned_allocate(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return new_failure::aligned_allocate(n, static_cast<std::size_t>(a));
}
void operator delete(void* p, std::align_val_t) noexcept { new_failure::aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { new_failure::aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { new_failure::aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { new_failure::aligned_free(p); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return new_failure::aligned_allocate(n, static_cast<std::size_t>(a)); }
    catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return new_failure::aligned_allocate(n, static_cast<std::size_t>(a)); }
    catch (...) { return nullptr; }
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { new_failure::aligned_free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { new_failure::aligned_free(p); }

struct MoveRecord {
    std::uint64_t id;
    XString name;
    XVector<std::uint32_t> values;
    XVector<XString> labels;
};
struct MoveRoot {
    XString left, right;
    XVector<std::uint32_t> a, b;
    XVector<MoveRecord> records;
};
XOFFSET_REGISTER_SCHEMA_NAME(MoveRoot, "test.O3.same-arena-move.v1")

// A finite local tree exercises public-container ancestor/descendant assignment.
// It is deliberately NOT registered/loaded as a V1 recursive wire schema.
struct LocalNode { std::uint32_t id; XVector<LocalNode> children; };
namespace XOffsetDatastructure {
template <> struct is_byte_copy_safe<LocalNode> : std::true_type {};
}
struct LocalTree { XVector<LocalNode> nodes; };

namespace {
std::size_t cases = 0, fast_new_attempts = 0, retire_new_attempts = 0, retire_new_hits = 0;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
auto policy(std::size_t bytes = 16u * 1024u * 1024u) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}
auto make_buffer() { return XBuffer::create<MoveRoot>(1024u * 1024u, policy()); }
// V2: a binding is exactly the payload base (no separate allocator state).
bool same_binding(fixed_detail::allocator_ref a, fixed_detail::allocator_ref b) {
    return a.segment && a.segment == b.segment;
}
// V2 retirement records the block in the host-side free table. Its first use
// of a bin allocates host memory (the table/bin vector), unlike V1's in-arena
// free lists; warm the bin so the move itself is observed allocation-free.
// The returned block is live and must be released before any save (an
// unreferenced live block is dead space that exact coverage rejects).
void* warm_free_bin(XBuffer& buffer, std::uint32_t bytes) {
    auto* arena = buffer.arena();
    auto* block = arena->allocate_aligned(bytes, 8);
    arena->deallocate(block, bytes);
    require(arena->allocate_aligned(bytes, 8) == block, "free-table warm-up did not reuse its block");
    require(buffer.free_block_count() == 0, "free-table warm-up left a hole");
    return block;
}
template <typename Head>
std::array<unsigned char, 16> descriptor(const Head& h) {
    static_assert(sizeof(Head) == 16);
    std::array<unsigned char, 16> result{};
    std::memcpy(result.data(), &h, result.size());
    return result;
}
template <typename Head>
void require_empty_reusable_binding(const Head& head, fixed_detail::allocator_ref old) {
    require(head.data() == nullptr && head.empty() && head.capacity() == 0, "moved-from descriptor not empty");
    require(same_binding(head.binding(), old), "moved-from binding was lost");
}
void fill(MoveRecord& r, std::uint32_t value) {
    r.id = value;
    r.name.assign("nested-name");
    r.values.push_back(value);
    r.values.push_back(value + 100);
    r.labels.emplace_back().assign("nested-label");
}
void check(const MoveRecord& r, std::uint32_t value) {
    require(r.id == value && r.name.view() == "nested-name", "record scalar/string mismatch");
    require(r.values.size() == 2 && r.values[0] == value && r.values[1] == value + 100, "record values mismatch");
    require(r.labels.size() == 1 && r.labels[0].view() == "nested-label", "record nested label mismatch");
}
void verified_roundtrip(XBuffer& b) {
    auto wire = b.save_verified<MoveRoot>();
    auto loaded = XBuffer::load_verified<MoveRoot>(wire, policy());
    auto wire2 = loaded.save_verified<MoveRoot>();
    auto loaded2 = XBuffer::load_verified<MoveRoot>(wire2, policy());
    (void)loaded2.root<MoveRoot>();
}

void same_arena_and_self() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    // "transferred" (11 bytes) is out of line; "old" is I1-inline (no block).
    r.left.assign("old"); r.right.assign("transferred");
    r.a.push_back(9); r.b.push_back(31); r.b.push_back(32);
    require(r.left.is_inline() && !r.right.is_inline(), "string fixture representation differs");
    auto* text = r.right.data(); auto* values = r.b.data();
    const auto sb = r.right.binding(), vb = r.b.binding();
    const auto db = r.left.binding(), ab = r.a.binding();
    const auto retired = static_cast<std::uint32_t>(r.a.capacity() * sizeof(std::uint32_t));
    auto* warm = warm_free_bin(buffer, retired); // the bin of r.a's block, retired by the move
    { new_failure::Arm arm(0); r.left = std::move(r.right); r.a = std::move(r.b); }
    fast_new_attempts += new_failure::attempts;
    require(new_failure::attempts == 0 && new_failure::hits == 0, "small same-arena move allocated host memory");
    require(buffer.free_block_count() == 1 && buffer.free_block_bytes() == retired, "destination block was not retired");
    buffer.arena()->deallocate(warm, retired);
    require(r.left.data() == text && r.left.view() == "transferred", "string ownership not transferred");
    require(r.a.data() == values && r.a.size() == 2 && r.a[1] == 32, "vector ownership not transferred");
    require(same_binding(r.left.binding(), db) && same_binding(r.a.binding(), ab), "destination binding changed");
    require_empty_reusable_binding(r.right, sb); require_empty_reusable_binding(r.b, vb);
    r.right.assign("again"); r.b.push_back(77);
    require(r.right.view() == "again" && r.b[0] == 77, "moved-from head not reusable");
    auto sd = descriptor(r.left), vd = descriptor(r.a);
    r.left = std::move(r.left); r.a = std::move(r.a);
    require(sd == descriptor(r.left) && vd == descriptor(r.a), "self move changed descriptor");
    require(r.left.view() == "transferred" && r.a[1] == 32, "self move changed values");
    verified_roundtrip(buffer); ++cases;
}

void empty_capacity_transfer() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    r.right.reserve(80); r.right.clear(); r.b.reserve(16);
    auto* text = r.right.data(); auto* values = r.b.data();
    const auto sc = r.right.capacity(), vc = r.b.capacity();
    r.left.assign("retire-me"); r.a.push_back(5);
    r.left = std::move(r.right); r.a = std::move(r.b);
    require(r.left.empty() && r.left.data() == text && r.left.capacity() == sc && r.left.c_str()[0] == 0,
        "empty allocated string did not transfer");
    require(r.a.empty() && r.a.data() == values && r.a.capacity() == vc, "empty allocated vector did not transfer");
    const auto sb = r.right.binding(), vb = r.b.binding();
    r.left = std::move(r.right); r.a = std::move(r.b);
    require(r.left.data() == nullptr && r.a.data() == nullptr, "zero-capacity source did not retire destination");
    require_empty_reusable_binding(r.right, sb); require_empty_reusable_binding(r.b, vb);
    r.left.assign("new"); r.right.assign("source"); r.a.push_back(2); r.b.push_back(3);
    verified_roundtrip(buffer); ++cases;
}

void cross_arena_and_provenance() {
    auto destination = make_buffer(), source = make_buffer();
    auto& d = destination.root<MoveRoot>(); auto& s = source.root<MoveRoot>();
    s.right.assign("cross-arena"); s.b.push_back(123);
    auto* sp = s.right.data(); auto* vp = s.b.data();
    const auto db = d.left.binding(), ab = d.a.binding();
    d.left.assign("before"); d.a.push_back(1);
    d.left = std::move(s.right); d.a = std::move(s.b);
    require(d.left.view() == "cross-arena" && d.a[0] == 123, "cross-arena copy values wrong");
    require(d.left.data() != sp && d.a.data() != vp, "cross-arena stole foreign allocation");
    require(s.right.data() == sp && s.right.view() == "cross-arena" && s.b.data() == vp && s.b[0] == 123,
        "cross-arena changed source copy semantics");
    require(same_binding(d.left.binding(), db) && same_binding(d.a.binding(), ab), "cross-arena destination rebound");
    d.left.assign("different"); d.a[0] = 444;
    require(s.right.view() == "cross-arena" && s.b[0] == 123, "cross-arena storage aliases");
    verified_roundtrip(destination); verified_roundtrip(source);
    // V2 has a single provenance: every head binds to its payload base. (V1's
    // state-bound vs legacy segment-bound distinction no longer exists.) An
    // explicit rebind to the same arena is therefore the same allocator.
    auto local = make_buffer(); auto& r = local.root<MoveRoot>();
    r.right.bind(fixed_detail::allocator_ref_of(local.arena()));
    r.b.bind(fixed_detail::allocator_ref_of(local.arena()));
    require(same_binding(r.right.binding(), r.left.binding()) && same_binding(r.b.binding(), r.a.binding()) &&
            r.right.binding().segment == local.arena(), "rebind to the payload base differs");
    r.right.assign("segment-bound"); r.b.push_back(42);
    auto* old_text = r.right.data(); auto* old_values = r.b.data();
    r.left = std::move(r.right); r.a = std::move(r.b);
    require(r.left.data() == old_text && r.a.data() == old_values, "equal V2 provenance did not transfer");
    require_empty_reusable_binding(r.right, r.left.binding()); require_empty_reusable_binding(r.b, r.a.binding());
    verified_roundtrip(local);
    ++cases;
}

void unbound_fallback() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    std::destroy_at(&r.left); std::construct_at(&r.left);
    std::destroy_at(&r.a); std::construct_at(&r.a);
    r.right.assign("bound-source"); r.b.push_back(88);
    auto* text = r.right.data(); auto* values = r.b.data();
    r.left = std::move(r.right); r.a = std::move(r.b);
    require(r.left.view() == "bound-source" && r.a[0] == 88, "unbound destination fallback failed");
    require(r.right.data() == text && r.b.data() == values, "unbound fallback stole source payload");
    require(same_binding(r.left.binding(), r.right.binding()) && same_binding(r.a.binding(), r.b.binding()),
        "unbound destination not rebound by existing copy path");
    std::destroy_at(&r.right); std::construct_at(&r.right);
    std::destroy_at(&r.b); std::construct_at(&r.b);
    r.left = std::move(r.right); r.a = std::move(r.b);
    require(r.left.empty() && r.a.empty() && r.left.is_bound() && r.a.is_bound(),
        "empty unbound source changed bound destination semantics");
    require(!r.right.is_bound() && !r.b.is_bound(), "empty unbound source acquired ownership");
    r.right.bind(r.left.binding()); r.b.bind(r.a.binding());
    verified_roundtrip(buffer); ++cases;
}

void nested_compaction_and_rebuild() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    r.records.resize(3);
    for (std::uint32_t i = 0; i < 3; ++i) fill(r.records[i], i + 10);
    auto* name1 = r.records[1].name.data(); auto* values1 = r.records[1].values.data();
    auto* labels1 = r.records[1].labels.data(); auto* label1 = r.records[1].labels[0].data();
    auto* name2 = r.records[2].name.data(); auto* values2 = r.records[2].values.data();
    r.records[0] = std::move(r.records[1]); r.records[1] = std::move(r.records[2]);
    require(r.records[0].name.data() == name1 && r.records[0].values.data() == values1 &&
        r.records[0].labels.data() == labels1 && r.records[0].labels[0].data() == label1,
        "nested stable compaction copied first survivor");
    require(r.records[1].name.data() == name2 && r.records[1].values.data() == values2,
        "overlapping stable compaction copied second survivor");
    require(r.records[2].name.empty() && r.records[2].values.empty() && r.records[2].labels.empty(),
        "last compacted record still owns payload");
    r.records.resize(2); check(r.records[0], 11); check(r.records[1], 12);
    r.records.reserve(r.records.capacity() + 8);
    require(r.records[0].name.data() == name1 && r.records[0].values.data() == values1,
        "existing reserve relocation broke transferred ownership");
    check(r.records[0], 11); check(r.records[1], 12);
    // Existing resize/rebuild remains the original copy path, not an O3 claim.
    r.records.resize(r.records.capacity() + 1);
    check(r.records[0], 11); check(r.records[1], 12);
    r.records.resize(2); verified_roundtrip(buffer); ++cases;
}

void parent_child_alias_guards() {
    {
        auto buffer = XBuffer::create<LocalTree>(1024u * 1024u, policy());
        auto& r = buffer.root<LocalTree>(); r.nodes.resize(2);
        r.nodes[0].id = 7; r.nodes[0].children.resize(1); r.nodes[0].children[0].id = 91;
        r.nodes[1].id = 8;
        auto* inner = r.nodes[0].children.data();
        r.nodes = std::move(r.nodes[0].children);
        require(r.nodes.size() == 1 && r.nodes[0].id == 91 && r.nodes.data() != inner,
            "ancestor destination failed copy-before-retire fallback");
    }
    {
        auto buffer = XBuffer::create<LocalTree>(1024u * 1024u, policy());
        auto& r = buffer.root<LocalTree>(); r.nodes.resize(2);
        r.nodes[0].id = 7; r.nodes[0].children.resize(1); r.nodes[0].children[0].id = 91;
        r.nodes[1].id = 8;
        auto* outer = r.nodes.data(); auto& child = r.nodes[0].children;
        child = std::move(r.nodes);
        require(r.nodes.data() == outer && r.nodes.size() == 2 && child.data() != outer && child.size() == 2,
            "descendant destination stole its own owning allocation");
        require(child[0].id == 7 && child[0].children.size() == 1 && child[0].children[0].id == 91 && child[1].id == 8,
            "descendant fallback failed finite deep copy");
    }
    ++cases;
}

// MV: the inline leaf owns evaluation, the reused source binding and the
// split release/set_rel32 paths must be result-identical to the generic code.
template <typename Head>
void owns_equivalence_for(const Head& head, const unsigned char* lo, const unsigned char* hi) {
    for (const unsigned char* p = lo; p <= hi; ++p) {
        const bool generic = fixed_detail::move_assignment_owns_address(head, p);
        const bool fast = fixed_detail::move_assignment_owns_address_fast(head, p);
        require(generic == fast, "inline leaf owns differs from generic owns");
    }
}
void leaf_owns_and_binding_reuse() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    // Exhaustive containment around storage edges: string (capacity+1 bytes),
    // u32 vector (capacity*4 bytes), empty, zero-capacity-with-storage and
    // a non-leaf vector (generic path through the wrapper).
    r.left.assign("edge-string-payload"); r.a.resize(9, 3u); r.records.resize(2); fill(r.records[0], 1);
    auto window = [](const void* data, std::size_t n) {
        auto* p = static_cast<const unsigned char*>(data);
        return std::pair{p - 24, p + n + 24};
    };
    { auto [lo, hi] = window(r.left.data(), r.left.capacity() + 1u); owns_equivalence_for(r.left, lo, hi); }
    { auto [lo, hi] = window(r.a.data(), r.a.capacity() * 4u); owns_equivalence_for(r.a, lo, hi); }
    { auto [lo, hi] = window(r.records.data(), r.records.capacity() * sizeof(MoveRecord));
      owns_equivalence_for(r.records, lo, hi); }
    { auto [lo, hi] = window(r.records[0].labels[0].data(), 32); owns_equivalence_for(r.records, lo, hi); }
    owns_equivalence_for(r.right, reinterpret_cast<const unsigned char*>(&r), reinterpret_cast<const unsigned char*>(&r) + 64);
    r.b.reserve(8); r.b.clear();
    { auto [lo, hi] = window(r.b.data(), 32); owns_equivalence_for(r.b, lo, hi); }
    // Alias guard on a leaf vector: a destination head placed inside the
    // source's own element storage must still take the copy fallback.
    r.b.resize(8, 0x11111111u); auto* storage = r.b.data();
    auto* inner = std::construct_at(reinterpret_cast<XFixedVector<std::uint32_t>*>(storage + 4), r.b.binding());
    *inner = std::move(r.b);
    require(r.b.data() == storage && r.b.size() == 8 && inner->data() != storage && inner->size() == 8,
        "leaf descendant destination stole its own owning allocation");
    std::destroy_at(inner);
    // (V1 case "two distinct allocator states in one arena" retired: V2 has no
    // allocator state; distinct allocators are distinct arenas, covered by
    // cross_arena_and_provenance.)
    // Same allocator: transfer, live destination retires through the out-of-line path.
    r.b.resize(0); r.a.assign_range(std::span<const std::uint32_t>(std::array<std::uint32_t,3>{1,2,3}));
    r.b.resize(5, 9u); auto* values = r.b.data();
    r.a = std::move(r.b);
    require(r.a.data() == values && r.a.size() == 5 && r.b.data() == nullptr, "same-state transfer");
    ++cases;
}

void reloaded_moves() {
    auto original = make_buffer(); auto& r = original.root<MoveRoot>();
    r.left.assign("replace"); r.right.assign("loaded-value"); r.a.push_back(1); r.b.push_back(55);
    r.records.resize(1); fill(r.records[0], 40);
    auto wire = original.save_verified<MoveRoot>();
    auto loaded = XBuffer::load_verified<MoveRoot>(wire, policy()); auto& q = loaded.root<MoveRoot>();
    auto* text = q.right.data(); auto* values = q.b.data();
    q.left = std::move(q.right); q.a = std::move(q.b);
    require(q.left.data() == text && q.a.data() == values, "reloaded same-arena move did not transfer");
    q.right.assign("after-load"); q.b.push_back(66); q.records[0].labels[0].assign("changed");
    auto wire2 = loaded.save_verified<MoveRoot>();
    auto checked = XBuffer::load_verified<MoveRoot>(wire2, policy()); auto& z = checked.root<MoveRoot>();
    require(z.left.view() == "loaded-value" && z.right.view() == "after-load" && z.a[0] == 55 && z.b[0] == 66 &&
        z.records[0].labels[0].view() == "changed", "post-load mutation/reload mismatch");
    require(original.root<MoveRoot>().right.view() == "loaded-value", "detached source wire owner changed");
    ++cases;
}

void retire_metadata_failure() {
    auto buffer = make_buffer(); auto& r = buffer.root<MoveRoot>();
    // V2 retirement records every block in the host-side free table (here the
    // large-block table). Its host bookkeeping allocation may fail; the noexcept
    // retirement swallows it and marks the table incomplete, and save_verified
    // recomputes the hole table from the live graph.
    r.a.resize(100000); r.a[99999] = 8; r.b.push_back(500); r.b.push_back(501);
    auto* source = r.b.data(); const auto sb = r.b.binding();
    require(buffer.free_table_complete(), "fixture free table already incomplete");
    { new_failure::Arm arm(0); r.a = std::move(r.b); }
    retire_new_attempts += new_failure::attempts; retire_new_hits += new_failure::hits;
    require(new_failure::hits == 1, "fresh large-retirement host allocation failpoint not reached");
    require(r.a.data() == source && r.a.size() == 2 && r.a[0] == 500 && r.a[1] == 501,
        "retirement metadata OOM broke ownership commit");
    require(!buffer.free_table_complete(), "lost retirement was not recorded as an incomplete free table");
    require_empty_reusable_binding(r.b, sb); r.b.push_back(999);
    verified_roundtrip(buffer);
    require(buffer.free_table_complete() && buffer.free_block_bytes() >= 400000,
        "save_verified did not recompute the lost hole");
    // The fresh owner has no earlier core-retired block. This observes an actual
    // hook exception, not an allocation callsite inferred from its request size.
    ++cases;
}

struct VmArm {
    VmArm() { detail::clear_vm_failures(); detail::fail_next_vm_operation(detail::VmOperation::Enable); }
    ~VmArm() { detail::clear_vm_failures(); }
};
void cross_arena_failure_guarantees() {
    {
        auto destination = XBuffer::create<MoveRoot>(4096, policy()); auto source = make_buffer();
        auto& d = destination.root<MoveRoot>(); auto& s = source.root<MoveRoot>();
        d.left.assign("old-string"); d.a.push_back(3); // disallow tail-only expansion
        const std::string big(100000, 'x'); s.right.assign(big);
        auto dd = descriptor(d.left), sd = descriptor(s.right); bool rejected = false;
        { VmArm arm; try { d.left = std::move(s.right); } catch (const XBadAlloc&) { rejected = true; } }
        require(rejected, "cross-arena string VM failpoint not reached");
        require(descriptor(d.left) == dd && descriptor(s.right) == sd && d.left.view() == "old-string" &&
            s.right.view() == big && d.a[0] == 3, "cross-arena string allocation failure changed live state");
        verified_roundtrip(destination); verified_roundtrip(source);
    }
    {
        auto destination = XBuffer::create<MoveRoot>(4096, policy()); auto source = make_buffer();
        auto& d = destination.root<MoveRoot>(); auto& s = source.root<MoveRoot>();
        d.a.push_back(3); s.b.resize(32768); s.b[0] = 90; s.b[32767] = 91;
        auto dd = descriptor(d.a), sd = descriptor(s.b); bool rejected = false;
        { VmArm arm; try { d.a = std::move(s.b); } catch (const XBadAlloc&) { rejected = true; } }
        require(rejected, "cross-arena vector VM failpoint not reached");
        require(descriptor(d.a) == dd && descriptor(s.b) == sd && d.a.size() == 1 && d.a[0] == 3 &&
            s.b.size() == 32768 && s.b[0] == 90 && s.b[32767] == 91,
            "cross-arena vector allocation failure changed live state");
        verified_roundtrip(destination); verified_roundtrip(source);
    }
    ++cases;
}
}

int main() {
    try {
        static_assert(!std::is_move_constructible_v<XFixedString>);
        static_assert(!std::is_move_constructible_v<XFixedVector<std::uint32_t>>);
        static_assert(sizeof(XFixedString) == 16 && sizeof(XFixedVector<std::uint32_t>) == 16);
        same_arena_and_self(); empty_capacity_transfer(); cross_arena_and_provenance();
        unbound_fallback(); nested_compaction_and_rebuild(); parent_child_alias_guards();
        reloaded_moves(); retire_metadata_failure(); cross_arena_failure_guarantees();
        leaf_owns_and_binding_reuse();
        std::cout << "{\"status\":\"passed\",\"cases\":" << cases
                  << ",\"small_fast_new_attempts\":" << fast_new_attempts
                  << ",\"retire_new_attempts\":" << retire_new_attempts
                  << ",\"retire_new_hits\":" << retire_new_hits
                  << ",\"scope\":\"C++new retirement and VM Enable; no universal OS/transaction guarantee\"}\n";
    } catch (const std::exception& e) {
        new_failure::armed = false; detail::clear_vm_failures();
        std::cerr << e.what() << '\n'; return 1;
    }
}
