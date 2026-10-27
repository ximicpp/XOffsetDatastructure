// Canonical-v2 registry fixture (receive owners on the V2 wire).
#include "xoffsetdatastructure.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>

namespace host_fault {
inline thread_local bool fail_next=false;
inline thread_local int fail_after=-1; // fail the (fail_after+1)-th request
inline thread_local unsigned attempts=0;
}
void* operator new(std::size_t n){
    bool fail=std::exchange(host_fault::fail_next,false);
    if(host_fault::fail_after==0){host_fault::fail_after=-1;fail=true;}
    else if(host_fault::fail_after>0)--host_fault::fail_after;
    if(fail){
        ++host_fault::attempts;
        if(auto h=std::get_new_handler())h();else throw std::bad_alloc();
    }
    if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

namespace xo=XOffsetDatastructure;
struct CanonicalRegistryRoot{
    std::uint64_t marker{};
    xo::XString label;
    xo::XVector<std::uint32_t> values;
};
XOFFSET_REGISTER_SCHEMA_NAME(CanonicalRegistryRoot,"receive.canonical-registry-v2.fixture")
namespace XOffsetDatastructure{
template<>struct wire_record_layout<CanonicalRegistryRoot>{static constexpr bool ordinary_members=true;};
}
using Buffer=xo::TypedXBuffer<CanonicalRegistryRoot>;
using Token=xo::VerifiedReceiveToken<CanonicalRegistryRoot>;
namespace{
void need(bool p,const char* why){if(!p)throw std::runtime_error(why);}
auto policy(){return xo::XBuffer::maximum_capacity(131072).with_growth(xo::XGrowPolicy::Automatic);}
std::string wire(){auto b=Buffer::create(32768,policy());auto& r=b.root();r.marker=0xa1200345;r.label="canonical registry";r.values.resize(5);for(unsigned i=0;i<5;++i)r.values[i]=101+i;return b.save_verified();}
// V2.1 dense wire: [XWireHeaderV2][payload bytes [48, used_bytes)][hole table: hole_count x 8].
Token receive(const std::string& bytes){
    xo::XWireHeaderV2 header{};need(bytes.size()>=sizeof(header),"fixture wire too small");
    std::memcpy(&header,bytes.data(),sizeof(header));
    const auto body=std::size_t(header.used_bytes);const auto holes=std::size_t(header.hole_count);
    need(header.extent_count==0&&bytes.size()==sizeof(header)+body-xo::XWIRE_PAYLOAD_BEGIN+
             holes*sizeof(xo::XBufferCore::WireHole),"fixture is not a dense V2.1 wire");
    auto t=xo::begin_verified_receive<CanonicalRegistryRoot>(body,policy(),holes);
    auto op=t.acquire_operation();auto h=op.header_bytes(),p=op.payload_bytes(),table=op.hole_bytes();
    for(std::size_t i=0;i<h.size();++i)h[i]=std::byte(static_cast<unsigned char>(bytes[i]));
    for(std::size_t i=0;i<p.size();++i)p[i]=std::byte(static_cast<unsigned char>(bytes[h.size()+i]));
    for(std::size_t i=0;i<table.size();++i)table[i]=std::byte(static_cast<unsigned char>(bytes[h.size()+p.size()+i]));
    op.complete(h.size(),p.size(),table.size());return t;
}
void check(Buffer& b){
    auto& r=b.root();need(r.marker==0xa1200345&&r.label=="canonical registry"&&r.values.size()==5&&r.values[4]==105,"complete owner values differ");
    const auto base=reinterpret_cast<std::uintptr_t>(b.get_address());
    const auto target=reinterpret_cast<std::uintptr_t>(r.values.data());
    auto* recovered=xo::XBufferCore::canonical_bytes_at(&r.values,target,5*sizeof(std::uint32_t));
    need(recovered&&reinterpret_cast<std::uintptr_t>(recovered)==target,"canonical exact array address differs");
    auto* end=xo::XBufferCore::canonical_bytes_at(&r,base+b.maximum_capacity(),0);
    need(end&&reinterpret_cast<std::uintptr_t>(end)==base+b.maximum_capacity(),"canonical one-past extent recovery differs");
    auto a=r.values.binding();auto* p=xo::fixed_detail::allocate_dynamic_bytes(a,24,8);
    xo::fixed_detail::retire_dynamic_bytes(a,p,24);
    auto* q=xo::fixed_detail::allocate_dynamic_bytes(a,24,8);need(q==p,"free-table reuse did not find original block");
    xo::fixed_detail::retire_dynamic_bytes(a,q,24);
}
std::array<std::optional<Buffer>,7>* live_owners=nullptr;
inline bool handler_called=false;
void handler(){
    handler_called=true;
    // The injection is one-shot, so all nested host requests are normal. This
    // would deadlock if pending record allocation held the registry mutex.
    for(auto& item:*live_owners)if(item)check(*item);
    std::swap((*live_owners)[0],(*live_owners)[1]);
    auto normal=Buffer::create(4096,policy());normal.root().marker=7;
    need(normal.root().marker==7,"handler normal owner publication failed");
}
void many_owners_and_pending_oom(){
    const auto input=wire();std::array<std::optional<Buffer>,7> owners;
    for(auto& item:owners){auto token=receive(input);item.emplace(xo::finish_verified(token));}
    for(unsigned round=0;round<5;++round)for(unsigned i=0;i<7;++i)check(*owners[(i*3+round)%7]);
    auto moved=std::move(*owners[0]);owners[0].reset();std::swap(moved,*owners[1]);owners[0].emplace(std::move(moved));
    const void* retired_anchor=owners[3]->get_address();const auto retired_number=reinterpret_cast<std::uintptr_t>(retired_anchor);
    owners[3].reset();
    need(xo::XBufferCore::canonical_bytes_at(retired_anchor,retired_number,1)==nullptr,"terminal cached record resurrected");
    {auto token=receive(input);owners[3].emplace(xo::finish_verified(token));}
    for(auto& item:owners)check(*item);

    auto pending=receive(input);const auto* address=pending.payload_address();
    auto previous=std::set_new_handler(nullptr);host_fault::fail_next=true;bool rejected=false;
    try{auto out=xo::finish_verified(pending);(void)out;}catch(const std::bad_alloc&){rejected=true;}catch(const xo::XBadAlloc&){rejected=true;}
    host_fault::fail_next=false;std::set_new_handler(previous);
    need(rejected&&pending&&pending.payload_address()==address,"pending registration OOM changed token/owner");
    auto recovered=xo::finish_verified(pending);check(recovered);

    auto reentrant=receive(input);live_owners=&owners;handler_called=false;
    previous=std::set_new_handler(handler);host_fault::fail_next=true;
    auto out=xo::finish_verified(reentrant);
    host_fault::fail_next=false;std::set_new_handler(previous);live_owners=nullptr;
    need(handler_called&&host_fault::attempts==2,"actual new_handler injection was not reached twice");check(out);
    out=Buffer{};recovered=Buffer{};for(auto& item:owners)item.reset();
    // Inert TLS shared records can remain, but no active C registration or VM
    // owner remains. Do not require final global C++ live requested bytes zero.
    need(xo::XBufferCore::canonical_bytes_at(retired_anchor,retired_number,1)==nullptr,"retired C anchor found after all owners released");
    auto normal=Buffer::create(8192,policy());normal.root().marker=91;normal.root().values.resize(4);normal.root().values[3]=71;
    need(normal.root().marker==91&&normal.root().values[3]==71,"normal reads after last C owner differ");
    need(xo::XBufferCore::canonical_bytes_at(&normal.root(),reinterpret_cast<std::uintptr_t>(normal.root().values.data()),sizeof(std::uint32_t))==nullptr,"ordinary VM falsely claimed canonical origin");
}
// Direct-receive owners are the only indexed owners, so their record and
// index node are the only registry allocations left. Fail each host request
// of finish_verified in turn: every failure leaves the token intact and
// retryable, and the sweep must pass both registry allocations.
void every_finish_allocation_failure(){
    const auto input=wire();
    for(int k=0;;++k){
        need(k<64,"finish_verified allocation sweep did not terminate");
        auto pending=receive(input);const auto* address=pending.payload_address();
        auto previous=std::set_new_handler(nullptr);host_fault::fail_after=k;
        bool rejected=false;std::optional<Buffer> out;
        try{out.emplace(xo::finish_verified(pending));}
        catch(const std::bad_alloc&){rejected=true;}catch(const xo::XBadAlloc&){rejected=true;}
        const bool reached=host_fault::fail_after==-1;host_fault::fail_after=-1;std::set_new_handler(previous);
        if(!reached){
            need(out&&!rejected,"unfailed finish did not publish");check(*out);
            need(k>=2,"direct receive registration lost its record or index-node allocation");
            return;
        }
        if(out){check(*out);continue;} // the failed request was absorbed (e.g. free table)
        need(rejected&&pending&&pending.payload_address()==address,"failed finish changed token/owner");
        auto recovered=xo::finish_verified(pending);check(recovered);
    }
}
}
int main(){try{many_owners_and_pending_oom();every_finish_allocation_failure();std::cout<<"canonical-v2: seven domains, retire/move/swap, cold OOM/new_handler and final normal reads passed\n";return 0;}
catch(const std::exception& e){host_fault::fail_next=false;std::set_new_handler(nullptr);std::cerr<<e.what()<<'\n';return 1;}}
