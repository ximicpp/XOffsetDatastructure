// v4 backward-compatible bulk replacement API; operation checks, no timer.
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <list>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "xoffsetdatastructure.hpp"
namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;
namespace host_oom { bool forbidden = false; unsigned attempts = 0; }
void* operator new(std::size_t n) {
    if (host_oom::forbidden) { ++host_oom::attempts; throw std::bad_alloc(); }
    if (void* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
struct BulkAttr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct BulkRoot {
    xo::XVector<std::uint32_t> numbers, donor;
    xo::XFixedVector<std::uint8_t> fixed_bytes;
    xo::XVector<std::uint8_t> bytes;
    xo::XString blocker;
    xo::XVector<BulkAttr> attrs;
};
XOFFSET_RECORD_DERIVED(BulkAttr, (), key, value)
XOFFSET_RECORD_DERIVED(BulkRoot, (), numbers, donor, fixed_bytes, bytes, blocker, attrs)
XOFFSET_REGISTER_SCHEMA_NAME(BulkRoot, "test.v4.bulk-replacement.v1")
struct PadRec { std::uint32_t id; std::uint64_t value; };
XOFFSET_RECORD_DERIVED(PadRec, (), id, value)
static_assert(xo::has_implicit_padding_v<PadRec> && sizeof(PadRec) == 16);
struct PadRoot { xo::XVector<PadRec> dst, src; };
XOFFSET_RECORD_DERIVED(PadRoot, (), dst, src)
XOFFSET_REGISTER_SCHEMA_NAME(PadRoot, "test.v4.bulk-copy-assign.v1")
struct WithDefaultCallback {
    static inline unsigned calls = 0;
    std::uint32_t value;
    WithDefaultCallback() : value(7) { ++calls; }
};
static_assert(std::is_trivially_copyable_v<WithDefaultCallback> && std::is_trivially_destructible_v<WithDefaultCallback>);
struct AssignmentOnly {
    std::uint32_t value;
    AssignmentOnly() : value(3) {}
    AssignmentOnly(const AssignmentOnly&) = delete;
    AssignmentOnly(AssignmentOnly&&) = delete;
    AssignmentOnly& operator=(const AssignmentOnly&) = default;
    ~AssignmentOnly() = default;
};
static_assert(std::is_trivially_copyable_v<AssignmentOnly> && std::is_trivially_destructible_v<AssignmentOnly>);
static_assert(!std::is_trivially_copy_constructible_v<AssignmentOnly>);
template<class V> concept HasBulk = requires(V& v, std::span<const typename V::value_type> s) { v.assign_range(s); };
static_assert(HasBulk<xo::XFixedVector<std::uint32_t>> && HasBulk<xo::XVector<std::uint32_t>>);
static_assert(HasBulk<xo::XVector<WithDefaultCallback>>);
static_assert(!HasBulk<xo::XVector<AssignmentOnly>>);
static_assert(!HasBulk<xo::XVector<xo::XString>> && !HasBulk<xo::XVector<BulkAttr>>);
static_assert(!HasBulk<xo::XVector<const std::uint32_t>> && !HasBulk<xo::XVector<volatile std::uint32_t>>);
static_assert(sizeof(xo::XFixedVector<std::uint8_t>) == 12 && sizeof(xo::XVector<std::uint32_t>) == 12);
namespace {
void need(bool value, const char* what) { if (!value) throw std::runtime_error(what); }
auto policy(std::size_t maximum = 32u << 20) {
    return xo::XBuffer::maximum_capacity(maximum).with_growth(xo::XGrowPolicy::Automatic);
}
auto make_buffer(std::size_t initial = 1u << 20) { return xo::XBuffer::create<BulkRoot>(initial, policy()); }
template<class T> auto descriptor(const T& value) {
    static_assert(sizeof(T) == 12); std::array<unsigned char, 12> result{};
    std::memcpy(result.data(), &value, 12); return result;
}
template<class V> auto values(const V& v) {
    using T = typename V::value_type;
    return v.size() ? std::vector<T>(v.begin(), v.end()) : std::vector<T>{};
}
template<class V, class T> void assign(V& v, const std::vector<T>& a) { v.assign_range(std::span<const T>(a)); }
// V2 has no allocator operation counters (FreelistIndex diagnostics are gone).
// Allocator operations are observed through the host free table instead:
// live bytes = used_end - free bytes, and a retired block is an exact hole.
struct Ledger { std::uint64_t used, free; std::size_t holes; };
Ledger ledger(const xo::XBufferCore& b) { return {b.arena()->used_end, b.free_block_bytes(), b.free_block_count()}; }
std::uint64_t live(const Ledger& l) { return l.used - l.free; }
std::uint32_t extent(std::size_t bytes) { return static_cast<std::uint32_t>((bytes + 7u) & ~std::size_t(7)); }
bool has_hole(const xo::XBufferCore& b, const void* p, std::uint32_t bytes) {
    const auto offset = static_cast<std::uint32_t>(static_cast<const char*>(p) - b.data()); bool found = false;
    b.for_each_free_block([&](xo::XBufferCore::WireHole h) { found |= h.offset == offset && h.bytes == bytes; });
    return found;
}
std::size_t holes_of_size(const xo::XBufferCore& b, std::uint32_t bytes) {
    std::size_t count = 0; b.for_each_free_block([&](xo::XBufferCore::WireHole h) { count += h.bytes == bytes; }); return count;
}
void roundtrip(xo::XBuffer& b) {
    auto& root = b.root<BulkRoot>(); const auto n = values(root.numbers); const auto v = values(root.bytes);
    const auto f = values(root.fixed_bytes); const auto nested = root.attrs.size() ? values(root.attrs[0].value) : std::vector<std::uint8_t>{};
    auto loaded = xo::XBuffer::load_verified<BulkRoot>(b.save_verified<BulkRoot>(), policy()); auto& copy = loaded.root<BulkRoot>();
    need(values(copy.numbers) == n && values(copy.bytes) == v && values(copy.fixed_bytes) == f, "bulk roundtrip");
    if (root.attrs.size()) need(values(copy.attrs[0].value) == nested, "bulk nested roundtrip");
    assign(copy.numbers, std::vector<std::uint32_t>(97, 13));
    auto again = xo::XBuffer::load_verified<BulkRoot>(loaded.save_verified<BulkRoot>(), policy());
    need(values(again.root<BulkRoot>().numbers) == std::vector<std::uint32_t>(97, 13), "bulk mutate reloaded");
}
void self_subrange_overlap_and_empty() {
    auto b = make_buffer(); auto& v = b.root<BulkRoot>().numbers;
    assign(v, std::vector<std::uint32_t>{1,2,3,4,5,6,7,8}); const auto cap = v.capacity(); auto* p = v.data();
    const auto binding = v.binding(); v.assign_range(std::span<const std::uint32_t>(v.data(), v.size()));
    need(values(v) == std::vector<std::uint32_t>({1,2,3,4,5,6,7,8}), "full self range");
    v.assign_range(std::span<const std::uint32_t>(v.data() + 2, 5));
    need(values(v) == std::vector<std::uint32_t>({3,4,5,6,7}) && v.data() == p && v.capacity() == cap, "overlapping subrange");
    // A live element beyond logical size after shrink may be supplied by a
    // caller that explicitly manages that element's lifetime. Normal self and
    // subrange cases above stay entirely within the public logical range.
    v.assign_range({}); need(v.empty() && v.data() == p && v.capacity() == cap, "empty range changed storage");
    need(v.binding().segment == binding.segment, "self changed binding");
    v.assign_range(std::span<const std::uint32_t>(std::array<std::uint32_t, 3>{9,8,7}));
    need(values(v) == std::vector<std::uint32_t>({9,8,7}), "external temporary live array"); roundtrip(b);
    xo::XVector<std::uint32_t> unbound; unbound.assign_range({}); need(!unbound.is_bound() && unbound.empty(), "unbound empty");
    bool caught = false; const std::array<std::uint32_t, 1> one{1};
    try { unbound.assign_range(one); } catch (const xo::XException&) { caught = true; }
    need(caught && !unbound.is_bound(), "unbound nonempty rejection");
}
void fresh_same_foreign_and_nested_sources() {
    auto b = make_buffer(), foreign = make_buffer(); auto& root = b.root<BulkRoot>();
    assign(root.numbers, std::vector<std::uint32_t>{7,8}); assign(root.donor, std::vector<std::uint32_t>(41, 71));
    root.blocker.assign(std::string(129, 'b')); auto* old = root.numbers.data(); const auto binding = root.numbers.binding();
    const auto before = ledger(b); root.numbers.assign_range({root.donor.data(), root.donor.size()});
    need(root.numbers.data() != old && root.numbers.capacity() == 41, "fresh exact capacity");
    // Exactly one exact-size allocation (41 * 4 -> 168 bytes) and one retire of the old 8-byte block.
    need(live(ledger(b)) == live(before) + extent(41 * 4) - extent(2 * 4) && has_hole(b, old, extent(2 * 4)), "fresh bulk allocate/retire");
    need(values(root.numbers) == std::vector<std::uint32_t>(41,71) && values(root.donor) == values(root.numbers), "same arena donor modified");
    auto& donor = foreign.root<BulkRoot>(); assign(donor.bytes, std::vector<std::uint8_t>(513, 42));
    root.bytes.assign_range({donor.bytes.data(), donor.bytes.size()});
    need(values(root.bytes) == values(donor.bytes), "foreign bulk values");
    root.attrs.resize(2); root.attrs[0].key.assign("k0000"); root.attrs[1].key.assign("k0001");
    root.attrs[0].value.assign_range({root.bytes.data()+3, 333}); root.attrs[1].value.assign_range({root.attrs[0].value.data(), 17});
    need(values(root.attrs[1].value) == std::vector<std::uint8_t>(17,42), "nested same arena source");
    need(root.numbers.binding().segment == binding.segment, "fresh binding changed");
    root.fixed_bytes.assign_range({root.bytes.data(), 87});
    need(values(root.fixed_bytes) == std::vector<std::uint8_t>(87,42), "fixed-vector API");
    roundtrip(b); roundtrip(foreign);
}
void no_default_constructor_and_no_alias_staging() {
    // The wrapper has a user-provided default constructor but trivial copying
    // and destruction. assign_range must not call that constructor at all.
    xo::XBufferCore core(1u<<20, xo::XBufferCore::maximum_capacity(1u<<20));
    auto* v = static_cast<xo::XVector<WithDefaultCallback>*>(core.arena()->allocate_aligned(
        sizeof(xo::XVector<WithDefaultCallback>), alignof(xo::XVector<WithDefaultCallback>)));
    std::construct_at(v, fd::allocator_ref_of(core.arena()));
    std::array<WithDefaultCallback, 9> source{}; for (unsigned i=0;i!=9;++i) source[i].value=19+i;
    WithDefaultCallback::calls=0; v->assign_range(source);
    need(WithDefaultCallback::calls==0 && (*v)[8].value==27, "bulk default-constructed before overwriting");
    v->assign_range(std::span<const WithDefaultCallback>(v->data()+2, 4));
    need(WithDefaultCallback::calls==0 && (*v)[0].value==21 && (*v)[3].value==24, "trivial class overlap");
    std::destroy_at(v);
}
void failed_growth_keeps_old_descriptor_and_source() {
    auto fixed=xo::XBuffer::create<BulkRoot>(32768,xo::XBuffer::maximum_capacity(32768));
    auto& root=fixed.root<BulkRoot>(); assign(root.bytes,std::vector<std::uint8_t>(17,9));
    auto before=descriptor(root.bytes); const auto source=std::vector<std::uint8_t>(65536,17); bool caught=false;
    try { assign(root.bytes,source); } catch(const xo::XBadAlloc&) { caught=true; }
    need(caught && descriptor(root.bytes)==before && values(root.bytes)==std::vector<std::uint8_t>(17,9),"fixed bulk OOM");
    for(auto operation:{dt::VmOperation::Enable,dt::VmOperation::Prepare}) {
        auto b=make_buffer(4096); auto& v=b.root<BulkRoot>().bytes; assign(v,std::vector<std::uint8_t>(17,9));
        const auto d=descriptor(v); dt::clear_vm_failures(); dt::fail_next_vm_operation(operation); caught=false;
        try { assign(v,source); } catch(const xo::XBadAlloc&) { caught=true; }
        dt::clear_vm_failures(); need(caught && descriptor(v)==d && values(v)==std::vector<std::uint8_t>(17,9),"VM bulk OOM");
        assign(v,source); need(values(v)==source,"bulk VM recovery"); roundtrip(b);
    }
    // Source ranges must describe actual live objects. A fabricated oversized
    // span backed by one element is invalid before assign_range is entered;
    // checked_count is also covered by the pure-text ordering regression.
}
void no_host_staging_under_host_oom() {
    auto b=make_buffer();auto& root=b.root<BulkRoot>();
    assign(root.numbers,std::vector<std::uint32_t>{1,2});
    assign(root.donor,std::vector<std::uint32_t>(257,89));
    root.blocker.assign(std::string(129,'b'));
    // V2 keeps free-table metadata in host memory, so the one retire below (the
    // old 8-byte block) may allocate a bin slot. Warm that bin first so the
    // gate isolates element staging from allocator metadata.
    auto* warm=b.arena()->allocate_aligned(8,8); b.arena()->deallocate(warm,8);
    need(b.arena()->allocate_aligned(8,8)==warm,"warm bin slot was not reused");
    host_oom::attempts=0;host_oom::forbidden=true;
    try {
        root.numbers.assign_range({root.donor.data(),root.donor.size()});
        root.numbers.assign_range({root.numbers.data()+3,211});
    } catch(...) {host_oom::forbidden=false;throw;}
    host_oom::forbidden=false;
    need(host_oom::attempts==0 && values(root.numbers)==std::vector<std::uint32_t>(211,89),
         "live same-arena bulk attempted host staging");
    need(b.free_table_complete(),"bulk retire lost a free-table entry");
    b.arena()->deallocate(warm,8); // V2 exact coverage: release before the verified save
    roundtrip(b);
}
void bulk_workload_operation_gate_and_lifecycle() {
    constexpr unsigned n=4096; auto b=make_buffer(8u<<20); auto& root=b.root<BulkRoot>(); root.attrs.resize(n);
    for(auto& a:root.attrs) { a.key.assign("k0001"); a.value.resize(16,std::uint8_t{3}); }
    root.blocker.assign(std::string(1024,'b')); const std::array<std::uint8_t,32> source=[] {std::array<std::uint8_t,32> x{};x.fill(91);return x;}();
    // Each replacement is one exact 32-byte allocation plus one retire of the
    // old 16-byte block (a 16-byte hole can never serve a 32-byte request).
    const auto before=ledger(b); const auto small_before=holes_of_size(b,16);
    for(auto& a:root.attrs) a.value.assign_range(source);
    need(live(ledger(b))==live(before)+std::uint64_t(n)*(32-16) && holes_of_size(b,16)==small_before+n,"bulk workload arena operation count");
    for(auto& a:root.attrs) need(values(a.value)==std::vector<std::uint8_t>(32,91),"bulk workload contents");
    auto moved=std::move(b); auto other=make_buffer(); swap(moved,other);
    other.root<BulkRoot>().attrs[0].value.assign_range({source.data()+1,17});
    need(other.root<BulkRoot>().attrs[0].value.size()==17,"bulk after move/swap");
    moved=make_buffer(); assign(moved.root<BulkRoot>().numbers,std::vector<std::uint32_t>(37,81));
    roundtrip(moved); roundtrip(other);
    std::cout<<"bulk gate: "<<n<<" replacements, skipped_zero_bytes="<<n*16u
             <<" skipped_old_copy_bytes="<<n*16u<<"; mechanism counts, no timing\n";
}
// Whole-container copy assignment of a padded POD element: in place when the
// capacity holds the source (storage kept, padding zeroed (M6), the shrunk
// tail zeroed (M4)), and by regrowth otherwise.
void copy_assign_in_place_and_regrow() {
    auto policy64 = policy(); auto b = xo::XBuffer::create<PadRoot>(1u << 16, policy64);
    auto foreign = xo::XBuffer::create<PadRoot>(1u << 16, policy64);
    auto& r = b.root<PadRoot>(); auto& f = foreign.root<PadRoot>();
    for (std::uint32_t i = 0; i < 6; ++i) r.dst.push_back(PadRec{i, 100u + i});
    r.dst.reserve(8);
    for (std::uint32_t i = 0; i < 3; ++i) f.src.push_back(PadRec{50u + i, 500u + i});
    for (std::uint32_t i = 0; i < 3; ++i) std::memset(reinterpret_cast<unsigned char*>(f.src.data() + i) + 4, 0xa5, 4);
    auto* p = r.dst.data(); const auto cap = r.dst.capacity();
    need(cap == 8, "reserve");
    r.dst = f.src;
    need(r.dst.data() == p && r.dst.capacity() == cap && r.dst.size() == 3, "in-place copy changed storage");
    const auto* bytes = reinterpret_cast<const unsigned char*>(r.dst.data());
    for (std::uint32_t i = 0; i < 3; ++i) {
        need(r.dst[i].id == 50u + i && r.dst[i].value == 500u + i, "in-place copy values");
        for (unsigned k = 4; k < 8; ++k) need(bytes[i * 16u + k] == 0, "in-place copy kept source padding");
    }
    for (std::size_t k = 3 * 16; k < std::size_t(cap) * 16; ++k) need(bytes[k] == 0, "in-place copy left the shrunk tail");
    r.dst = r.dst;
    need(r.dst.size() == 3 && r.dst[2].id == 52u && r.dst.data() == p, "self copy");
    r.dst = r.src;
    need(r.dst.empty() && r.dst.capacity() == cap && r.dst.data() == p, "empty source changed storage");
    for (std::size_t k = 0; k < std::size_t(cap) * 16; ++k) need(bytes[k] == 0, "empty source left elements");
    for (std::uint32_t i = 3; i < 12; ++i) f.src.push_back(PadRec{50u + i, 500u + i});
    r.dst = f.src;
    need(r.dst.size() == 12 && r.dst.capacity() >= 12 && r.dst[11].value == 511u && r.dst.data() != p,
         "regrowth copy");
    auto loaded = xo::XBuffer::load_verified<PadRoot>(b.save_verified<PadRoot>(), policy64);
    need(loaded.root<PadRoot>().dst.size() == 12 && loaded.root<PadRoot>().dst[5].id == 55u, "copy roundtrip");
}
void iterator_assign_paths() {
    auto b=make_buffer(); auto& v=b.root<BulkRoot>().numbers;
    assign(v,std::vector<std::uint32_t>{1,2});
    // Contiguous trivially copyable source: the assign_range path, one exact block.
    const std::vector<std::uint32_t> flat{5,6,7,8,9};
    auto* old=v.data(); v.assign(flat.begin(),flat.end());
    need(values(v)==flat && v.capacity()==5 && has_hole(b,old,extent(2*4)),"iterator assign contiguous path");
    // Non-contiguous forward source: the generic path builds a fresh exact block.
    const std::list<std::uint32_t> linked{11,12,13};
    old=v.data(); const auto cap=v.capacity(); v.assign(linked.begin(),linked.end());
    need(values(v)==std::vector<std::uint32_t>({11,12,13}) && v.capacity()==3 && has_hole(b,old,extent(cap*4)),
         "iterator assign generic path");
    // The generic path may read from the container it replaces.
    v.assign(std::make_reverse_iterator(v.data()+v.size()),std::make_reverse_iterator(v.data()));
    need(values(v)==std::vector<std::uint32_t>({13,12,11}),"iterator assign self-aliasing source");
    const std::list<std::uint32_t> none; v.assign(none.begin(),none.end());
    need(v.empty(),"iterator assign empty generic source");
    v.assign(linked.begin(),linked.end()); roundtrip(b);
}
}
int main() {
    try {
        self_subrange_overlap_and_empty(); fresh_same_foreign_and_nested_sources();
        no_default_constructor_and_no_alias_staging(); failed_growth_keeps_old_descriptor_and_source();
        no_host_staging_under_host_oom(); bulk_workload_operation_gate_and_lifecycle();
        iterator_assign_paths(); copy_assign_in_place_and_regrow();
        std::cout<<"8 bulk assignment groups passed; no timing\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
