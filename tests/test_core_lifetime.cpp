#include "xoffsetdatastructure.hpp"
#include "host_failpoint.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xo = XOffsetDatastructure;
namespace dt = xo::detail;
namespace fd = xo::fixed_detail;

struct LifetimeNested { xo::XString text; };
struct LifetimeRecord {
    bool enabled{};
    std::uint32_t id{};
    xo::XString name;
    xo::XVector<std::uint32_t> samples;
    LifetimeNested nested;
};
struct LifetimeRoot {
    xo::XString source0, source1;
    xo::XVector<xo::XString> words;
    xo::XVector<std::uint32_t> numbers;
    xo::XVector<LifetimeRecord> records;
};
namespace XOffsetDatastructure {
template<> struct wire_record_layout<LifetimeNested> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<LifetimeRecord> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<LifetimeRoot> { static constexpr bool ordinary_members = true; };
}
XOFFSET_REGISTER_SCHEMA_NAME(LifetimeRoot, "receive.complete.core-lifetime.v1")
static_assert(dt::receive_raw::future_supported<LifetimeRoot>());
static_assert(dt::ordinary_outer_lifetime_supported<LifetimeRecord>());
static_assert(xo::is_v1_wire_admitted_v<LifetimeRoot>);

namespace {
void need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
auto policy() { return xo::XBuffer::maximum_capacity(2u<<20).with_growth(xo::XGrowPolicy::Automatic); }
auto make() { return xo::XBuffer::create<LifetimeRoot>(65536, policy()); }
// V2 has no in-arena allocator state (no free_bytes/epoch). Retirement and
// reuse are observable through the host free table; growth through used_end.
struct AllocatorSnapshot {
    std::size_t blocks; std::uint64_t bytes;
    bool operator==(const AllocatorSnapshot&) const = default;
};
AllocatorSnapshot allocator_snapshot(const xo::XBufferCore& buffer) {
    return {buffer.free_block_count(), buffer.free_block_bytes()};
}
std::uint64_t offset_of(const xo::XBufferCore& buffer, const void* p) {
    return reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(buffer.get_address());
}

void dirty_bool_spare_is_never_typed_read() {
    alignas(bool) std::byte storage[64];
    std::memset(storage, 2, sizeof(storage));
    auto* first = dt::begin_dynamic_array<bool>(storage, 8);
    std::construct_at(first, true); std::construct_at(first + 1, false);
    const auto address = reinterpret_cast<std::uintptr_t>(first);
    auto* expanded = dt::rebuild_dynamic_array<bool>(storage, 64, 2);
    need(reinterpret_cast<std::uintptr_t>(expanded) == address && expanded[0] && !expanded[1],
         "scalar prefix representation or address changed");
    unsigned char dirty{};
    std::memcpy(&dirty, storage + 41, 1);
    need(dirty == 2, "helper initialized/read-and-normalized dirty spare");
    for (std::size_t i=2; i<64; ++i) std::construct_at(expanded+i, false);
    for (std::size_t i=1; i<64; ++i) need(!expanded[i], "new bool initialization failed");
    need(dt::begin_dynamic_array<bool>(nullptr, 0) == nullptr, "zero count created an array");
}

void scalar_tail_has_no_host_or_retirement_work() {
    auto buffer=make(); auto& v=buffer.root<LifetimeRoot>().numbers;
    v.resize(3); v[0]=11; v[1]=22; v[2]=33;
    const auto address=reinterpret_cast<std::uintptr_t>(v.data());
    const auto free=allocator_snapshot(buffer);
    need(offset_of(buffer,v.data())+16==buffer.arena()->used_end, "scalar fixture is not the arena tail");
    {
        index_host::Arm deny(0);
        v.reserve(5);
        v.resize(9);
        need(index_host::attempts==0, "tail lifetime seam allocated host scratch");
    }
    need(reinterpret_cast<std::uintptr_t>(v.data())==address && v.capacity()==9 && v.size()==9,
         "scalar tail became fresh or published wrong descriptor");
    need(v[0]==11 && v[1]==22 && v[2]==33, "scalar prefix changed");
    for(unsigned i=3;i<9;++i) need(v[i]==0, "default scalar suffix not value initialized");
    need(allocator_snapshot(buffer)==free && offset_of(buffer,v.data())+40==buffer.arena()->used_end,
         "tail lifetime seam retired or reused an allocation");
}

void owning_head_tail_reactivation_keeps_child_allocations() {
    auto buffer=make(); auto& r=buffer.root<LifetimeRoot>();
    r.source0="earlier-one"; r.source1="earlier-two";
    const auto* payload0=r.source0.data(); const auto* payload1=r.source1.data();
    r.words.reserve(2); r.words.resize(2); // this allocation is now arena tail
    r.words[0]=std::move(r.source0); r.words[1]=std::move(r.source1);
    const auto address=reinterpret_cast<std::uintptr_t>(r.words.data());
    const auto free=allocator_snapshot(buffer);
    need(offset_of(buffer,r.words.data())+2*sizeof(xo::XString)==buffer.arena()->used_end,
         "owning-head fixture is not the arena tail");
    {
        index_host::Arm deny(0);
        r.words.reserve(19);
        need(index_host::attempts==0, "prefix activation allocated");
    }
    need(reinterpret_cast<std::uintptr_t>(r.words.data())==address && r.words.capacity()==19 && r.words.size()==2,
         "owning-head tail was replaced by a fresh allocation");
    need(r.words[0]=="earlier-one" && r.words[1]=="earlier-two" &&
         r.words[0].data()==payload0 && r.words[1].data()==payload1, "out-of-line ownership changed");
    need(allocator_snapshot(buffer)==free &&
         offset_of(buffer,r.words.data())+19*sizeof(xo::XString)==buffer.arena()->used_end,
         "prefix activation retired an owned child");
    r.words[0].assign("still-mutable");
    auto loaded=xo::XBuffer::load_verified<LifetimeRoot>(buffer.save_verified<LifetimeRoot>(), policy());
    need(loaded.root<LifetimeRoot>().words[0]=="still-mutable", "head reactivation did not round trip");
}

void string_growth_reacquires_self_view_including_terminator() {
    auto buffer=make(); auto& value=buffer.root<LifetimeRoot>().source0;
    // Out-of-line (> 10 bytes, not I1-inline) tail block of 16 bytes: the
    // self view including its terminator needs 17, a real tail expansion.
    value.assign("abcdefghijklmno"); const auto address=reinterpret_cast<std::uintptr_t>(value.data());
    need(!value.is_inline() && value.capacity()==15 && value.owned_extent()==16, "self-view fixture capacity differs");
    need(offset_of(buffer,value.data())+16==buffer.arena()->used_end, "self-view fixture is not the arena tail");
    const std::string_view expected("abcdefghijklmno\0",16);
    value.assign(std::string_view(value.data(), value.capacity()+1u));
    need(value.size()==16 && value.capacity()==16 && value.view()==expected &&
         value.c_str()[16]=='\0' && reinterpret_cast<std::uintptr_t>(value.data())==address &&
         offset_of(buffer,value.data())+24==buffer.arena()->used_end,
         "same-address self terminator view was read through old array");
    value.reserve(50); need(value.size()==16 && value.view()==expected, "string reserve lost prefix");
}

void free_header_reuse_changes_real_element_array_type() {
    auto buffer=make(); auto alloc=buffer.root<LifetimeRoot>().words.binding();
    auto* raw=fd::allocate_dynamic_bytes(alloc,64,8);
    auto* integers=dt::begin_dynamic_array<std::uint64_t>(raw,8);
    for(unsigned i=0;i<8;++i)std::construct_at(integers+i, std::uint64_t(100+i));
    const auto address=reinterpret_cast<std::uintptr_t>(raw);
    fd::retire_dynamic_bytes(alloc,raw,64); // V2: host free table, no in-arena header
    need(buffer.free_block_count()==1 && buffer.free_block_bytes()==64, "retired block not in the host free table");
    auto* reused=fd::allocate_dynamic_bytes(alloc,64,8); // exact-size bin reuse
    need(reinterpret_cast<std::uintptr_t>(reused)==address && buffer.free_block_count()==0,
         "exact-size free-table reuse not exercised");
    auto* heads=dt::begin_dynamic_array<xo::XString>(reused,4);
    for(unsigned i=0;i<4;++i)std::construct_at(heads+i,alloc);
    heads[3]="new-array-kind"; need(heads[3]=="new-array-kind", "fresh head parent array missing");
    std::destroy_n(heads,4); fd::retire_dynamic_bytes(alloc,reused,64);
    auto* again=dt::begin_dynamic_array<std::uint64_t>(fd::allocate_dynamic_bytes(alloc,64,8),8);
    need(reinterpret_cast<std::uintptr_t>(again)==address, "second cross-type reuse not exercised");
    for(unsigned i=0;i<8;++i)std::construct_at(again+i,std::uint64_t(i));
    for(unsigned i=0;i<8;++i)need(again[i]==i,"new scalar array data differs");
    fd::retire_dynamic_bytes(alloc,again,64);
}

void ordinary_outer_relocation_and_old_fill_read_order() {
    auto buffer=make(); auto& r=buffer.root<LifetimeRoot>();
    r.records.resize(2);
    // Out-of-line name (> 10 bytes): its payload must survive outer relocation.
    for(unsigned i=0;i<2;++i){auto& v=r.records[i];v.enabled=true;v.id=7+i;v.name="record-payload";
        v.samples.push_back(30+i);v.nested.text="nested";}
    const auto* child=r.records[0].name.data();
    need(!r.records[0].name.is_inline(),"record name fixture is inline");
    r.records.resize(5,r.records[0]); // read every tail fill before old prefix ends
    need(r.records.size()==5 && r.records[0].name.data()==child,"fill invalidated original payload");
    for(unsigned i=2;i<5;++i)need(r.records[i].id==7 && r.records[i].enabled &&
        r.records[i].name=="record-payload" && r.records[i].samples[0]==30 && r.records[i].nested.text=="nested",
        "self-record fill changed before final read");
    r.words.resize(2);r.words[0]="owned-subobject";r.words[1]="second";
    r.words.resize(7,r.words[0].view());
    for(unsigned i=2;i<7;++i)need(r.words[i]=="owned-subobject","owned-subobject fill alias changed");
    auto remote=make();auto& foreign=remote.root<LifetimeRoot>().words;foreign.resize(1);foreign[0]="cross-arena";
    r.words.resize(10,foreign[0]);
    for(unsigned i=7;i<10;++i)need(r.words[i]=="cross-arena" &&
        r.words[i].binding().segment==r.words.binding().segment,"cross-arena transfer binding differs");
    r.records.erase(r.records.begin()+1);r.records.reserve(20); // whole outer relocation
    auto wire=buffer.save_verified<LifetimeRoot>();auto loaded=xo::XBuffer::load_verified<LifetimeRoot>(wire,policy());
    auto& after=loaded.root<LifetimeRoot>();
    need(after.records.size()==4 && after.records[0].id==7 && after.records[1].id==7 &&
        after.records[3].nested.text=="nested" && after.words[9]=="cross-arena", "complete outer roundtrip differs");
    after.records[0].name="after-reload";
    auto twice=xo::XBuffer::load_verified<LifetimeRoot>(loaded.save_verified<LifetimeRoot>(),policy());
    need(twice.root<LifetimeRoot>().records[0].name=="after-reload","loaded state stopped being mutable");
}
}

int main(){try{
    dirty_bool_spare_is_never_typed_read();scalar_tail_has_no_host_or_retirement_work();
    owning_head_tail_reactivation_keeps_child_allocations();string_growth_reacquires_self_view_including_terminator();
    free_header_reuse_changes_real_element_array_type();ordinary_outer_relocation_and_old_fill_read_order();
    std::cout<<"core lifetime: six semantic groups passed; no timing claim\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
