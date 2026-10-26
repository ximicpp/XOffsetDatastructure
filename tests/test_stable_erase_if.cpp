// Decision-first production API regression; root executes normal/SAN only.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <malloc.h>
#endif

namespace host {
thread_local long fail_after = -1;
thread_local bool armed = false;
thread_local unsigned attempts{}, failures{}, handler_calls{};
void before() {
    if (!armed) return;
    ++attempts;
    if (fail_after == 0) { fail_after = -1; ++failures; throw std::bad_alloc(); }
    if (fail_after > 0) --fail_after;
}
void* allocate(std::size_t size, std::size_t alignment) {
    for (;;) {
        bool fail = false;
        try { before(); } catch (const std::bad_alloc&) { fail = true; }
        void* p = nullptr;
        if (!fail) {
            if (alignment == 0) p = std::malloc(size ? size : 1); // ordinary new; aligned new pairs with _aligned_free
            else {
                const auto n = size ? size : 1;
                if (n > (std::numeric_limits<std::size_t>::max)() - alignment + 1) throw std::bad_alloc();
#if defined(_WIN32)
                p = _aligned_malloc(n, alignment);
#else
                p = std::aligned_alloc(alignment, (n + alignment - 1) / alignment * alignment);
#endif
            }
        }
        if (p) return p;
        if (auto handler = std::get_new_handler()) { ++handler_calls; handler(); }
        else throw std::bad_alloc();
    }
}
void release(void* p, bool aligned) noexcept {
#if defined(_WIN32)
    if (aligned) _aligned_free(p); else std::free(p);
#else
    (void)aligned; std::free(p);
#endif
}
struct Arm {
    explicit Arm(long point) { attempts = failures = handler_calls = 0; fail_after = point; armed = true; }
    ~Arm() { armed = false; fail_after = -1; }
};
}
void* operator new(std::size_t n) { return host::allocate(n,0); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { host::release(p,false); }
void operator delete[](void* p) noexcept { host::release(p,false); }
void operator delete(void* p,std::size_t) noexcept { host::release(p,false); }
void operator delete[](void* p,std::size_t) noexcept { host::release(p,false); }
void* operator new(std::size_t n,std::align_val_t a) { return host::allocate(n,std::size_t(a)); }
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { host::release(p,true); }
void operator delete[](void* p,std::align_val_t) noexcept { host::release(p,true); }
void operator delete(void* p,std::size_t,std::align_val_t) noexcept { host::release(p,true); }
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept { host::release(p,true); }
void* operator new(std::size_t n,const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void operator delete(void* p,const std::nothrow_t&) noexcept { host::release(p,false); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { host::release(p,false); }
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&) noexcept { try { return ::operator new(n,a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t&) noexcept { try { return ::operator new[](n,a); } catch (...) { return nullptr; } }
void operator delete(void* p,std::align_val_t,const std::nothrow_t&) noexcept { host::release(p,true); }
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&) noexcept { host::release(p,true); }

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace track {
inline bool active = false;
inline std::array<unsigned,64> destructors{};
inline const std::uint32_t* observed_size{};
inline std::uint32_t expected_size{};
inline unsigned early_size_publications{};
}
struct EraseRecord {
    std::uint32_t id{};
    xo::XString name;
    xo::XVector<std::uint32_t> values;
    ~EraseRecord() noexcept {
        if (track::active) {
            if (track::observed_size && *track::observed_size != track::expected_size) ++track::early_size_publications;
            if (id < track::destructors.size()) ++track::destructors[id];
        }
    }
};
namespace XOffsetDatastructure {
template<> struct is_byte_copy_safe<EraseRecord> : std::true_type {};
}
// User-provided destructor makes this outer class nonimplicit-lifetime. The
// explicit customization, not member-only placement, starts the COMPLETE T.
namespace XOffsetDatastructure {
template<> struct StableEraseRelocator<EraseRecord> {
    static constexpr bool enabled = true;
    static void relocate_construct(EraseRecord* destination, EraseRecord& source) noexcept {
        static_assert(std::is_nothrow_default_constructible_v<EraseRecord>);
        std::construct_at(destination);
        destination->id = source.id;
        std::destroy_at(&destination->name);
        fixed_detail::stable_erase_relocate(&destination->name, source.name);
        std::destroy_at(&destination->values);
        fixed_detail::stable_erase_relocate(&destination->values, source.values);
    }
};
}
// Ordinary nested records use the existing TypeLayout member-recursion domain.
// Its internal recursion does not use the public array specialization for an
// owning-head C-array member; do not widen V1 admission to repair this fixture.
struct ImplicitNested { xo::XString first; xo::XString second; };
struct ImplicitRecord { std::uint32_t id{}; xo::XString name; xo::XVector<std::uint32_t> values;
                        ImplicitNested nested; std::uint32_t raw[3]{}; };
XOFFSET_RECORD_DERIVED(ImplicitNested, (), first, second)
XOFFSET_RECORD_DERIVED(EraseRecord, (), id, name, values)
XOFFSET_RECORD_DERIVED(ImplicitRecord, (), id, name, values, nested, raw)
namespace XOffsetDatastructure {
// Audited source above has no bases or [[no_unique_address]]. Offsets below
// independently confirm non-overlap for the actual compiler's ordinary layout.
template<> struct StableEraseOrdinaryMembers<ImplicitRecord> : std::true_type {};
template<> struct StableEraseOrdinaryMembers<ImplicitNested> : std::true_type {};
}
static_assert(offsetof(ImplicitRecord,id)+sizeof(ImplicitRecord::id)<=offsetof(ImplicitRecord,name));
static_assert(offsetof(ImplicitRecord,name)+sizeof(ImplicitRecord::name)<=offsetof(ImplicitRecord,values));
static_assert(offsetof(ImplicitRecord,values)+sizeof(ImplicitRecord::values)<=offsetof(ImplicitRecord,nested));
static_assert(offsetof(ImplicitRecord,nested)+sizeof(ImplicitRecord::nested)<=offsetof(ImplicitRecord,raw));
struct VolatileRecord { volatile std::uint32_t value; };
struct OwningBase { xo::XString value; };
struct DerivedOwning : OwningBase { std::uint32_t id; };
struct OverlapUnapproved { [[no_unique_address]] xo::XString value; };
namespace XOffsetDatastructure {
template<> struct StableEraseOrdinaryMembers<VolatileRecord> : std::true_type {};
template<> struct StableEraseOrdinaryMembers<DerivedOwning> : std::true_type {};
}
struct UncustomizedRecord { xo::XString name; ~UncustomizedRecord() noexcept {} };
struct OptedUserDestructor { xo::XString name; ~OptedUserDestructor() noexcept {} };
namespace XOffsetDatastructure {
template<> struct StableEraseOrdinaryMembers<OptedUserDestructor> : std::true_type {};
}
namespace XOffsetDatastructure {
template<> struct is_byte_copy_safe<UncustomizedRecord> : std::true_type {};
template<> struct is_byte_copy_safe<OptedUserDestructor> : std::true_type {};
}
struct EraseRoot { xo::XVector<EraseRecord> records; xo::XVector<std::uint32_t> numbers;
                   xo::XVector<ImplicitRecord> implicit_records; };
XOFFSET_RECORD_DERIVED(OwningBase, (), value)
XOFFSET_RECORD_DERIVED(VolatileRecord, (), value)
XOFFSET_RECORD_DERIVED(DerivedOwning, (OwningBase), id)
XOFFSET_RECORD_DERIVED(OverlapUnapproved, (), value)
XOFFSET_RECORD_DERIVED(UncustomizedRecord, (), name)
XOFFSET_RECORD_DERIVED(OptedUserDestructor, (), name)
XOFFSET_RECORD_DERIVED(EraseRoot, (), records, numbers, implicit_records)
XOFFSET_REGISTER_SCHEMA_NAME(EraseRoot,"four-paths.stable-erase.v1")
struct Always { template<class T> bool operator()(const T&) const { return true; } };
template<class V> concept Erasable = requires(V& values) { values.stable_erase_if(Always{}); };
struct ThrowingDestructor { ~ThrowingDestructor() noexcept(false) {} };
static_assert(Erasable<xo::XVector<std::uint32_t>>);
static_assert(Erasable<xo::XVector<EraseRecord>>);
static_assert(!Erasable<xo::XVector<ThrowingDestructor>>);
static_assert(!Erasable<xo::XVector<std::string>>);
static_assert(!Erasable<xo::XVector<UncustomizedRecord>>);
#if XOFFSET_SCHEMA_REFLECTION // its user destructor is a promise without reflection (XString member)
static_assert(!Erasable<xo::XVector<OptedUserDestructor>>);
#endif
static_assert(!fd::stable_erase_supported<volatile std::uint32_t>());
static_assert(!fd::stable_erase_supported<VolatileRecord>());
static_assert(!fd::stable_erase_supported<DerivedOwning>());
static_assert(!fd::stable_erase_supported<OverlapUnapproved>());
#if defined(__cpp_lib_is_implicit_lifetime) // C++23 library trait; GCC 13/Clang 18 C++20 lack it
static_assert(!std::is_implicit_lifetime_v<EraseRecord>);
#endif
// Source + reflection prove the standard aggregate/destructor branch. This
// does not assert a different value for the actual SDK's std builtin trait.
static_assert(std::is_aggregate_v<ImplicitRecord>);
static_assert(fd::stable_erase_implicit_lifetime<ImplicitRecord>());
#if XOFFSET_SCHEMA_REFLECTION // its user destructor is a promise without reflection (XString member)
static_assert(!fd::stable_erase_implicit_lifetime<OptedUserDestructor>());
#endif
static_assert(XOffsetDatastructure::is_byte_copy_safe_v<ImplicitRecord>);
static_assert(xo::is_v1_wire_admitted_v<EraseRoot>);
static_assert(Erasable<xo::XVector<ImplicitRecord>>);

namespace {
void need(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
auto policy() { return xo::XBuffer::maximum_capacity(4u<<20).with_growth(xo::XGrowPolicy::Automatic); }
auto make(unsigned count=17) {
    auto buffer=xo::XBuffer::create<EraseRoot>(65536,policy());
    auto& root=buffer.root<EraseRoot>();
    root.records.resize(count); root.numbers.resize(count);
    for(unsigned i=0;i<count;++i) {
        auto& record=root.records[i];record.id=i;
        record.name=std::string_view("surviving-payload");
        record.values.push_back(i);record.values.push_back(i+100);
        root.numbers[i]=i;
    }
    return buffer;
}
// V2: a binding is exactly the payload base (no separate allocator state).
bool same_binding(fd::allocator_ref a,fd::allocator_ref b) { return a.segment && a.segment==b.segment; }
void verify(const EraseRecord& r,unsigned id) {
    need(r.id==id && r.name=="surviving-payload" && r.values.size()==2 &&
         r.values[0]==id && r.values[1]==id+100,"survivor contents differ");
}
void unchanged_failure_transactions() {
    auto buffer=make();auto& records=buffer.root<EraseRoot>().records;
    const auto bytes=buffer.copy_bytes();
    const auto* address=records.data();const auto capacity=records.capacity();const auto binding=records.binding();
    unsigned predicates=0;bool caught=false;
    {
        host::Arm arm(0);
        try { (void)records.stable_erase_if([&](const EraseRecord&){++predicates;return true;}); }
        catch(const std::bad_alloc&) { caught=true; }
    }
    need(caught && host::failures==1 && predicates==0,"decision bitmap bad_alloc not real or predicate ran");
    need(buffer.copy_bytes()==bytes,"scratch OOM changed arena bytes");
    struct PredicateError {};
    for(unsigned point=0;point<17;++point) {
        unsigned visits=0;track::destructors={};track::active=true;caught=false;
        try {
            (void)records.stable_erase_if([&](const EraseRecord& r){
                need(r.id==visits,"predicate order differs");
                if(visits++==point)throw PredicateError{};
                return r.id%2==0;
            });
        } catch(const PredicateError&) {caught=true;}
        track::active=false;
        need(caught && visits==point+1,"predicate throw point not reached exactly once");
        for(auto n:track::destructors)need(n==0,"destructor ran before all decisions completed");
        need(buffer.copy_bytes()==bytes && records.data()==address && records.capacity()==capacity &&
             records.size()==17 && same_binding(records.binding(),binding),"predicate failure committed state");
    }
}
void stable_values_payloads_and_counts() {
    for(unsigned mode=0;mode<5;++mode) {
        auto buffer=make();auto& records=buffer.root<EraseRoot>().records;
        std::array<const void*,17> names{},values{};
        for(unsigned i=0;i<17;++i){names[i]=records[i].name.data();values[i]=records[i].values.data();}
        auto* data=records.data();const auto capacity=records.capacity();const auto binding=records.binding();
        std::array<unsigned,17> survivors{};unsigned kept=0,removed=0,visits=0;
        auto erase=[&](unsigned id){return mode==0 ? false : mode==1 ? true : mode==2 ? id%2==0 : mode==3 ? id<3 : id>=14;};
        for(unsigned id=0;id<17;++id)if(erase(id))++removed;else survivors[kept++]=id;
        track::destructors={};track::observed_size=&records.size_;track::expected_size=17;
        track::early_size_publications=0;track::active=true;
        const auto result=records.stable_erase_if([&](const EraseRecord& r){
            need(r.id==visits,"predicate visited a moved/changed element");++visits;return erase(r.id);
        });
        track::active=false;
        need(track::early_size_publications==0,"size published before destructive commit completed");
        need(result==removed && visits==17 && records.size()==kept,"stable deletion cardinality differs");
        need(records.data()==data && records.capacity()==capacity && same_binding(records.binding(),binding),
             "stable deletion changed data/capacity/binding");
        for(unsigned i=0;i<kept;++i){const auto id=survivors[i];verify(records[i],id);
            need(records[i].name.data()==names[id] && records[i].values.data()==values[id],"surviving payload copied or replaced");}
        for(unsigned id=0;id<17;++id){
            unsigned expected=erase(id)?1:0;
            if(!erase(id)){unsigned position=0;while(survivors[position]!=id)++position;if(position!=id)++expected;}
            need(track::destructors[id]==expected,"removed/moved-source destructor count differs");
        }
        auto wire=buffer.save_verified<EraseRoot>();
        auto loaded=xo::XBuffer::load_verified<EraseRoot>(wire,policy());
        need(loaded.root<EraseRoot>().records.size()==kept,"reload size differs");
        for(unsigned i=0;i<kept;++i)verify(loaded.root<EraseRoot>().records[i],survivors[i]);
        // Both old and reloaded containers remain mutable under public APIs.
        loaded.root<EraseRoot>().records.emplace_back().name="usable-after-erase";
        auto twice=xo::XBuffer::load_verified<EraseRoot>(loaded.save_verified<EraseRoot>(),policy());
        need(twice.root<EraseRoot>().records.size()==kept+1 &&
             twice.root<EraseRoot>().records.back().name=="usable-after-erase","post-reload mutation failed");
    }
}
void scalar_tail_alias_and_empty() {
    xo::XVector<std::uint32_t> unbound;
    unsigned calls=0;
    need(unbound.stable_erase_if([&](const auto&){++calls;return true;})==0 && calls==0,"empty unbound called predicate");
    auto buffer=make(33);auto& values=buffer.root<EraseRoot>().numbers;
    const auto& cutoff=values.back(); // external alias remains live during all decisions
    auto* data=values.data();const auto cap=values.capacity();
    need(values.stable_erase_if([&](const std::uint32_t& v){return v<cutoff;})==32,"tail alias was read after compaction");
    need(values.size()==1 && values[0]==32 && values.data()==data && values.capacity()==cap,"scalar stable result differs");
    calls=0;
    need(values.stable_erase_if([&](const auto&){++calls;return false;})==0 && calls==1,"none-removed predicate count differs");
    struct Stateful {
        unsigned calls=0;
        bool operator()(const std::uint32_t&) { ++calls;return false; }
    } stateful;
    need(values.stable_erase_if(stateful)==0 && stateful.calls==1,"stateful predicate was copied or not called exactly once");
}
void implicit_outer_lifetime_path() {
    auto buffer=make(); auto& values=buffer.root<EraseRoot>().implicit_records;
    values.resize(11);
    std::array<const void*,11> labels{}, payloads{};
    std::array<std::array<const void*,2>,11> nested{};
    for(unsigned i=0;i<11;++i){values[i].id=i;values[i].name="implicit-outer";values[i].values.push_back(i);
        labels[i]=values[i].name.data();payloads[i]=values[i].values.data();
        values[i].nested.first="nested-first";values[i].nested.second="nested-second";
        nested[i][0]=values[i].nested.first.data();nested[i][1]=values[i].nested.second.data();
        for(unsigned j=0;j<3;++j)values[i].raw[j]=i*10+j;}
    need(values.stable_erase_if([](const ImplicitRecord& r){return r.id%3==0;})==4,"implicit aggregate erase count");
    unsigned at=0;
    for(unsigned id=0;id<11;++id)if(id%3!=0){const auto& r=values[at++];
        need(r.id==id && r.name=="implicit-outer" && r.values.size()==1 && r.values[0]==id &&
             r.name.data()==labels[id] && r.values.data()==payloads[id],"implicit outer/member lifetime relocation differs");
        need(r.nested.first=="nested-first" && r.nested.second=="nested-second" &&
             r.nested.first.data()==nested[id][0] && r.nested.second.data()==nested[id][1],
             "ordinary nested outer lifetime/rebase differs");
        for(unsigned j=0;j<3;++j)need(r.raw[j]==id*10+j,"trivial array contents differ");}
    auto loaded=xo::XBuffer::load_verified<EraseRoot>(buffer.save_verified<EraseRoot>(),policy());
    need(loaded.root<EraseRoot>().implicit_records.size()==7,"implicit aggregate reload failed");
}
xo::XBuffer* handler_source{};xo::XBuffer* handler_destination{};
void move_owner_handler() { *handler_destination=std::move(*handler_source); }
void bitmap_new_handler_owner_move() {
    auto source=make();xo::XBuffer destination;
    auto& records=source.root<EraseRoot>().records;const auto* payload=records[3].name.data();
    handler_source=&source;handler_destination=&destination;
    const auto old=std::set_new_handler(move_owner_handler);
    unsigned removed=0;
    try { host::Arm arm(0);removed=records.stable_erase_if([](const EraseRecord& r){return r.id<3;}); }
    catch(...){std::set_new_handler(old);throw;}
    std::set_new_handler(old);
    need(host::handler_calls==1 && host::failures==1 && removed==3,"real bitmap new_handler path not exercised");
    const auto& retained=destination.root<EraseRoot>().records;
    need(retained.size()==14 && retained[0].id==3 && retained[0].name.data()==payload,"owner move invalidated payload or witness");
    auto reloaded=xo::XBuffer::load_verified<EraseRoot>(destination.save_verified<EraseRoot>(),policy());
    need(reloaded.root<EraseRoot>().records.front().id==3,"moved owner result failed verified reload");
}
}
int main(){try{
    unchanged_failure_transactions();stable_values_payloads_and_counts();scalar_tail_alias_and_empty();
    implicit_outer_lifetime_path();bitmap_new_handler_owner_move();
    std::cout<<"stable_erase_if: decision OOM/17 predicate failures,5 stable patterns,payload/lifetime/alias/reload/Core-move passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
