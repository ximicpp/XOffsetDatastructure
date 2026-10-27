// Deterministic replaceable-delete regression. No timer or performance claim.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <meta>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace host {
inline thread_local std::size_t allocations{},deletions{};
inline thread_local bool fail_next{};
inline thread_local void* delete_address{};
inline thread_local unsigned delete_calls{},handler_calls{};
inline thread_local void (*delete_callback)() noexcept = nullptr;
void* allocate(std::size_t bytes) {
    for (;;) {
        if (!std::exchange(fail_next,false)) {
            if (auto* p=std::malloc(bytes?bytes:1)) {++allocations;return p;}
        }
        if (auto fn=std::get_new_handler()) fn(); else throw std::bad_alloc();
    }
}
void release(void* p) noexcept {
    if (!p) return;
    void (*callback)() noexcept = nullptr;
    if (p==delete_address) {
        delete_address=nullptr; ++delete_calls; callback=delete_callback;
    }
    ++deletions;std::free(p);
    if (callback) callback();
}
void disarm() noexcept {fail_next=false;delete_address=nullptr;delete_callback=nullptr;}
}
void* operator new(std::size_t n){return host::allocate(n);}
void* operator new[](std::size_t n){return host::allocate(n);}
void operator delete(void* p)noexcept{host::release(p);}
void operator delete[](void* p)noexcept{host::release(p);}
void operator delete(void* p,std::size_t)noexcept{host::release(p);}
void operator delete[](void* p,std::size_t)noexcept{host::release(p);}

namespace xo=XOffsetDatastructure;
namespace fd=xo::fixed_detail;
struct Root {std::uint64_t marker{};xo::XString name;xo::XVector<std::uint32_t> values;};
XOFFSET_REGISTER_SCHEMA_NAME(Root,"receive.canonical-cold-no-callback.v4")
namespace XOffsetDatastructure {
template<>struct wire_record_layout<Root>{static constexpr bool ordinary_members=true;};
}

namespace test {
using Core=xo::XBufferCore;
using Buffer=xo::TypedXBuffer<Root>;
consteval std::meta::info member(std::string_view name){
    auto result=^^void;std::size_t count=0;
    for(auto item:std::meta::members_of(^^Core,std::meta::access_context::unchecked()))
        if(std::meta::has_identifier(item)&&std::meta::identifier_of(item)==name){result=item;++count;}
    if(count!=1)throw "exact private canonical fixture member missing";
    return result;
}
inline constexpr auto cache_member=member("owner_cache");
inline constexpr auto available_member=member("owner_cache_available");
inline constexpr auto registry_member=member("registry_state");
decltype(auto) cache(){return (&[:cache_member:])();}
decltype(auto) available(){return (&[:available_member:])();}
using Cache=std::remove_reference_t<decltype(cache())>;
using RecordPtr=std::remove_cvref_t<decltype(std::declval<Cache&>().records[0])>;
using Record=typename RecordPtr::element_type;
void need(bool v,const char* text){if(!v)throw std::runtime_error(text);}
void cleanup_need(bool v)noexcept{if(!v){std::fputs("cold-callback fixture cleanup failed\n",stderr);std::abort();}}

auto policy(){return xo::XBuffer::maximum_capacity(131072).with_growth(xo::XGrowPolicy::Automatic);}
std::string wire(){
    auto b=Buffer::create(32768,policy());auto& r=b.root();r.marker=0x99887766;
    r.name="cold canonical callback";r.values.resize(8);
    for(unsigned i=0;i<8;++i)r.values[i]=200+i;
    return b.save_verified();
}
// V2.1 dense wire: [XWireHeaderV2][payload bytes [48, used_bytes)][hole table: hole_count x 8].
Buffer received(const std::string& bytes){
    xo::XWireHeaderV2 header{};need(bytes.size()>=sizeof(header),"fixture wire too small");
    std::memcpy(&header,bytes.data(),sizeof(header));
    const auto body=std::size_t(header.used_bytes);const auto holes=std::size_t(header.hole_count);
    need(header.extent_count==0&&bytes.size()==sizeof(header)+body-xo::XWIRE_PAYLOAD_BEGIN+
             holes*sizeof(xo::XBufferCore::WireHole),"fixture is not a dense V2.1 wire");
    auto token=xo::begin_verified_receive<Root>(body,policy(),holes);
    {auto op=token.acquire_operation();auto h=op.header_bytes(),p=op.payload_bytes(),table=op.hole_bytes();
     for(std::size_t i=0;i<h.size();++i)h[i]=std::byte(static_cast<unsigned char>(bytes[i]));
     for(std::size_t i=0;i<p.size();++i)p[i]=std::byte(static_cast<unsigned char>(bytes[h.size()+i]));
     for(std::size_t i=0;i<table.size();++i)table[i]=std::byte(static_cast<unsigned char>(bytes[h.size()+p.size()+i]));
     op.complete(h.size(),p.size(),table.size());}
    return xo::finish_verified(token);
}
void check(Buffer& b){auto& r=b.root();need(r.marker==0x99887766&&r.name=="cold canonical callback"&&
    r.values.size()==8&&r.values[0]==200&&r.values[7]==207,"actual received values differ");}

class CacheScope {
    Cache& value_;
    std::array<RecordPtr,4> saved_{};
    std::array<RecordPtr,4> pending_{};
    std::array<std::byte,4> sentinels_{};
    std::size_t next_,last_;
public:
    explicit CacheScope():value_(cache()),next_(value_.next),last_(value_.last){
        need(available(),"main cache is unavailable");
        for(std::size_t i=0;i<4;++i)saved_[i]=value_.records[i];
    }
    void prepare(){
        // Two ordinary host requests per shared_ptr, all before the fault arm.
        // Each terminal Record object itself has a known exact delete address.
        for(std::size_t i=0;i<4;++i)pending_[i]=RecordPtr(new Record(&sentinels_[i],nullptr));
    }
    void* target()const noexcept{return pending_[0].get();}
    std::array<const void*,4> pending_fingerprint()const noexcept{
        std::array<const void*,4> result{};
        for(std::size_t i=0;i<4;++i)result[i]=pending_[i].get();
        return result;
    }
    void install()noexcept{
        for(std::size_t i=0;i<4;++i){cleanup_need(bool(pending_[i]));value_.records[i]=std::move(pending_[i]);}
        value_.next=0;value_.last=3;
    }
    std::array<const void*,4> fingerprint()const noexcept{
        std::array<const void*,4> result{};
        for(std::size_t i=0;i<4;++i)result[i]=value_.records[i].get();
        return result; // raw identities only: never retain terminal records
    }
    bool same(const std::array<const void*,4>& expected)const noexcept{
        return fingerprint()==expected&&value_.next==0&&value_.last==3;
    }
    void exercise_delete()noexcept{value_.records[0].reset();}
    ~CacheScope()noexcept{
        host::disarm();
        for(std::size_t i=0;i<4;++i)value_.records[i]=saved_[i];
        value_.next=next_;value_.last=last_;
    }
};
inline std::unique_ptr<Buffer>* source{};
inline Buffer* destination{};
inline CacheScope* pending_cache{};
inline bool moved{};
void move_and_destroy_source()noexcept{
    try{*destination=std::move(**source);source->reset();moved=true;}
    catch(...){moved=false;}
}
void rotate_on_new_handler(){
    ++host::handler_calls;cleanup_need(pending_cache!=nullptr);
    pending_cache->install();
}
class Arm {
    std::new_handler old_;
public:
    Arm(CacheScope& scope,std::unique_ptr<Buffer>& from,Buffer& to,bool fail_alloc)
      :old_(std::set_new_handler(rotate_on_new_handler)){
        source=&from;destination=&to;pending_cache=&scope;moved=false;
        host::delete_calls=host::handler_calls=0;host::delete_address=scope.target();
        host::delete_callback=move_and_destroy_source;host::fail_next=fail_alloc;
        need(host::delete_address!=nullptr,"no exact terminal delete target");
    }
    ~Arm(){host::disarm();std::set_new_handler(old_);source=nullptr;destination=nullptr;pending_cache=nullptr;}
};

void direct_cold_has_no_eviction(const std::string& bytes){
    CacheScope scope;auto a=std::make_unique<Buffer>(received(bytes));Buffer b;
    const auto address=reinterpret_cast<std::uintptr_t>(a->get_address());
    scope.prepare();Arm arm(scope,a,b,false);scope.install();
    const auto old=scope.fingerprint();
    const auto allocations=host::allocations,deletions=host::deletions;
    auto* p=Core::canonical_bytes_at(a->get_address(),address+64,1);
    need(p&&reinterpret_cast<std::uintptr_t>(p)==address+64,"cold canonical bytes differ");
    need(scope.same(old)&&host::allocations==allocations&&host::deletions==deletions&&
         host::delete_calls==0&&bool(a)&&!moved,"cold recovery introduced allocation/delete/cache mutation");
    check(*a);
    // Control proves the exact armed terminal delete really moves the owner
    // and destroys the old heap Core while the original mapping remains live.
    scope.exercise_delete();
    need(host::delete_calls==1&&moved&&!a&&reinterpret_cast<std::uintptr_t>(b.get_address())==address,
         "terminal replaceable-delete move/destroy control was not reached");
    check(b);
}

// V2: there is no in-arena cache drain (release_cached_blocks). The host-side
// free table's own growth is the allocator's host request; its new_handler
// installs terminal TLS records. Retirement resolves the owner from the host
// page, so it must not perform a canonical/TLS lookup, evict a record, run the
// terminal delete callback, move A or lose the retired block.
void real_free_table_growth_has_no_callback(const std::string& bytes){
    CacheScope scope;auto a=std::make_unique<Buffer>(received(bytes));Buffer b;check(*a);
    auto binding=a->root().values.binding();
    // A new bin reserves 8 slots; fill it so the next retire must grow it.
    void* filled[8];
    for(auto*& p:filled) p=fd::allocate_dynamic_bytes(binding,24,8);
    auto* second=fd::allocate_dynamic_bytes(binding,24,8);
    auto* first=filled[7];
    need(first!=second,"two real allocator blocks required");
    for(auto* p:filled) fd::retire_dynamic_bytes(binding,p,24); // table + a full 8-slot 24-byte bin
    need(a->free_block_count()==8&&a->free_block_bytes()==192,"real free-table block count differs");
    const auto address=reinterpret_cast<std::uintptr_t>(a->get_address());
    scope.prepare();const auto expected=scope.pending_fingerprint();
    Arm arm(scope,a,b,true);
    // No fixture/user operation is interposed: the bin's growth is the only
    // host request, and the armed failure reaches the real new_handler.
    fd::retire_dynamic_bytes(binding,second,24);
    need(host::handler_calls==1&&!host::fail_next,"actual free-table new_handler was not exercised exactly once");
    need(host::delete_calls==0&&!moved&&bool(a)&&a->free_block_count()==9&&a->free_block_bytes()==216&&
         a->free_table_complete(),"free-table growth evicted terminal record, used invalidated owner or lost a block");
    need(expected[0]==host::delete_address&&scope.same(expected),"free-table growth rotated terminal TLS cache");
    // The callback remains armed until this deliberate, out-of-allocator control.
    scope.exercise_delete();
    need(host::delete_calls==1&&moved&&!a&&reinterpret_cast<std::uintptr_t>(b.get_address())==address,
         "terminal delete move/destroy control was not exercised");
    check(b);
    // Host free-table blocks remain usable on the new Core.
    need(b.free_block_count()==9,"free table did not move with the owner");
    auto* reused=fd::allocate_dynamic_bytes(binding,24,8);
    need(reused==second,"retired host-table block was not reusable (LIFO) after owner move");
    fd::retire_dynamic_bytes(binding,reused,24);check(b);
    auto again=xo::TypedXBuffer<Root>::load_verified(b.save_verified(),policy());check(again);
}
}

int main(){try{const auto bytes=test::wire();
    test::direct_cold_has_no_eviction(bytes);test::real_free_table_growth_has_no_callback(bytes);
    std::cout<<"canonical-v4: cold no-eviction/no-C++-allocation; real free-table growth and exact terminal-delete owner-move controls passed\n";
    return 0;
}catch(const std::exception& e){host::disarm();std::set_new_handler(nullptr);std::cerr<<e.what()<<'\n';return 1;}}
