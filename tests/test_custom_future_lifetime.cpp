#include "core_custom_future_examples.hpp"
#include "host_failpoint.hpp"
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace xo = XOffsetDatastructure;
namespace dt = xo::detail;
namespace fd = xo::fixed_detail;
namespace ex = receive_custom_v2_examples;

struct FutureRoot {
    xo::XString early0, early1;
    xo::XVector<ex::Word> words;
    xo::XVector<ex::Owned> owned;
};
namespace XOffsetDatastructure {
template<> struct wire_record_layout<FutureRoot> { static constexpr bool ordinary_members=true; };
}
XOFFSET_REGISTER_SCHEMA_NAME(FutureRoot, "receive.complete.custom-lifetime.v2")
static_assert(xo::is_v1_wire_admitted_v<ex::Word> && xo::is_v1_wire_admitted_v<ex::Owned>);
static_assert(dt::receive_raw::future_supported<FutureRoot>());
static_assert(dt::future_custom_lifetime_supported<ex::Word>());
static_assert(dt::future_custom_lifetime_supported<ex::Owned>());
static_assert(!dt::ordinary_outer_lifetime_supported<ex::Word>());
static_assert(!dt::ordinary_outer_lifetime_supported<ex::Owned>());
static_assert(fd::can_relocate_without_allocation<ex::Word>());
static_assert(fd::can_relocate_without_allocation<ex::Owned>());
static_assert(!dt::future_custom_lifetime_supported<ex::RawOnly>());
static_assert(!dt::receive_raw::future_supported<ex::HiddenRawOnly>());

namespace {
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto policy(){return xo::XBuffer::maximum_capacity(2u<<20).with_growth(xo::XGrowPolicy::Automatic);}
auto make(){return xo::XBuffer::create<FutureRoot>(65536,policy());}

void whole_default_const_and_explicit_rvalue() {
    auto buffer=make(); auto& r=buffer.root<FutureRoot>();
    const auto default_before=ex::calls::defaults;
    r.words.resize(3);
    need(ex::calls::defaults==default_before+3,"default path bypassed complete-T policy");
    r.words[0].value=41; r.words[1].value=42; r.words[2].value=43;
    auto binding=r.words.binding();
    auto* raw=fd::allocate_dynamic_bytes(binding,sizeof(ex::Word),alignof(ex::Word));
    auto* dst=dt::begin_dynamic_array<ex::Word>(raw,1);
    const auto copies=ex::calls::copies, rvalues=ex::calls::rvalues;
    fd::transfer_construct(dst,std::move(r.words[0]),binding);
    need(dst->value==41 && r.words[0].value==41 && ex::calls::copies==copies+1 &&
         ex::calls::rvalues==rvalues,"existing fixed transfer became destructive/rvalue");
    std::destroy_at(dst);
    // This direct reflection entry genuinely carries an rvalue category.
    dt::reflect_transfer_init_all<ex::Word>(dst,std::move(r.words[1]),binding);
    need(dst->value==42 && r.words[1].value==42 && ex::calls::rvalues==rvalues+1,
         "explicit rvalue entry did not invoke complete rvalue hook");
    std::destroy_at(dst);
    dt::reflect_init_all<ex::Word>(dst,binding);
    need(dst->value==0,"direct reflection default bypassed complete constructor");
    std::destroy_at(dst); fd::retire_dynamic_bytes(binding,raw,sizeof(ex::Word));
}

void prefix_inline_is_not_initial_graph_activation() {
    auto buffer=make(); auto& r=buffer.root<FutureRoot>();
    r.early0="earlier-payload-0"; r.early1="earlier-payload-1";
    const auto* child0=r.early0.data(); const auto* child1=r.early1.data();
    r.owned.reserve(2); r.owned.resize(2); // head array is physical tail
    r.owned[0].text=std::move(r.early0);r.owned[0].value=70;
    r.owned[1].text=std::move(r.early1);r.owned[1].value=71;
    const auto address=reinterpret_cast<std::uintptr_t>(r.owned.data());
    // V2: no in-arena allocator state; the host free table and used_end
    // show any retirement, reuse or fresh allocation.
    const auto free_blocks=buffer.free_block_count();const auto free_bytes=buffer.free_block_bytes();
    const auto offset=address-reinterpret_cast<std::uintptr_t>(buffer.get_address());
    need(offset+2*sizeof(ex::Owned)==buffer.arena()->used_end,"custom head array is not the arena tail");
    const auto restarted=ex::calls::inline_restarts;
    {
        index_host::Arm deny(0);
        r.owned.reserve(19);
        need(index_host::attempts==0,"custom inline restart allocated scratch");
    }
    need(reinterpret_cast<std::uintptr_t>(r.owned.data())==address && r.owned.size()==2 &&
         r.owned.capacity()==19 && ex::calls::inline_restarts==restarted+2,
         "same-address custom prefix did not use exactly two whole-T hooks");
    need(r.owned[0].text.data()==child0 && r.owned[1].text.data()==child1 &&
         r.owned[0].text=="earlier-payload-0" && r.owned[1].text=="earlier-payload-1" &&
         r.owned[0].value==70 && r.owned[1].value==71,
         "inline-only restart reconstructed/lost out-of-line ownership");
    need(buffer.free_block_count()==free_blocks && buffer.free_block_bytes()==free_bytes &&
         offset+19*sizeof(ex::Owned)==buffer.arena()->used_end,
         "inline restart retired payload or cached array");
}

void throwing_fill_then_whole_relocation() {
    auto buffer=make(); auto& v=buffer.root<FutureRoot>().owned;
    // Out-of-line texts (> 10 bytes): whole-T relocation must keep their payloads.
    v.resize(2);v[0].text="kept-zero-payload";v[0].value=90;v[1].text="kept-one-payload";v[1].value=91;
    need(!v[0].text.is_inline()&&!v[1].text.is_inline(),"custom text fixture is inline");
    const auto descriptor=dt::raw_wire_pod<dt::RawOwningDescriptor>(reinterpret_cast<const char*>(&v));
    const auto child0=v[0].text.data(),child1=v[1].text.data();
    ex::calls::throw_after_owned_head=true;bool constructor_rejected=false;
    try{v.resize(5,v[0]);}catch(const std::runtime_error&){constructor_rejected=true;}
    ex::calls::throw_after_owned_head=false;
    auto constructor_after=dt::raw_wire_pod<dt::RawOwningDescriptor>(reinterpret_cast<const char*>(&v));
    need(constructor_rejected && std::memcmp(&descriptor,&constructor_after,sizeof(descriptor))==0 &&
         v[0].text.data()==child0 && v[1].text.data()==child1 && v[0].text=="kept-zero-payload",
         "real custom constructor unwind changed the prefix/descriptor");
    ex::calls::throw_after_copy=1;bool rejected=false;
    try{v.resize(5,v[0]);}catch(const std::runtime_error&){rejected=true;}
    ex::calls::throw_after_copy=-1;
    auto after=dt::raw_wire_pod<dt::RawOwningDescriptor>(reinterpret_cast<const char*>(&v));
    need(rejected && std::memcmp(&descriptor,&after,sizeof(after))==0 && v.size()==2 &&
         v[0].text.data()==child0 && v[1].text.data()==child1 &&
         v[0].text=="kept-zero-payload" && v[1].text=="kept-one-payload" && v[0].value==90 && v[1].value==91,
         "failed custom fill changed live prefix or descriptor");
    const auto relocations=ex::calls::relocations;
    v.resize(5,v[0]); // all potentially throwing fill reads precede relocation
    need(v.size()==5 && ex::calls::relocations==relocations+2 &&
         v[0].text.data()==child0 && v[1].text.data()==child1,
         "whole-T relocation copied payload or bypassed policy");
    for(unsigned i=2;i<5;++i)need(v[i].text=="kept-zero-payload" && v[i].value==90,
         "self fill read source after relocation");
    v.erase(v.begin()+1); // overlapping compaction, source destroyed once by caller
    need(v.size()==4 && v[0].value==90 && v[1].value==90 && v[3].text=="kept-zero-payload",
         "whole-T compaction differs");
    auto foreign=make();auto& f=foreign.root<FutureRoot>().owned;f.resize(1);f[0].text="remote";f[0].value=123;
    v.resize(7,f[0]);
    for(unsigned i=4;i<7;++i)need(v[i].text=="remote" && v[i].value==123 &&
         v[i].text.binding().segment==v.binding().segment && f[0].text=="remote",
         "cross-arena custom const transfer lost destination binding or source");
    auto loaded=xo::XBuffer::load_verified<FutureRoot>(buffer.save_verified<FutureRoot>(),policy());
    auto& l=loaded.root<FutureRoot>().owned;
    need(l.size()==7 && l[0].text=="kept-zero-payload" && l[3].value==90 && l[6].text=="remote",
         "custom complete-T graph did not preserve verified wire");
    l[0].text="after-reload";l.resize(9,l[0]);
    auto twice=xo::XBuffer::load_verified<FutureRoot>(loaded.save_verified<FutureRoot>(),policy());
    need(twice.root<FutureRoot>().owned[8].text=="after-reload",
         "reloaded custom owner lost future mutation");
}
}

int main(){try{
    whole_default_const_and_explicit_rvalue();
    prefix_inline_is_not_initial_graph_activation();
    throwing_fill_then_whole_relocation();
    std::cout<<"custom future lifetime: three semantic groups passed; no timing claim\n";return 0;
}catch(const std::exception& e){ex::calls::throw_after_copy=-1;ex::calls::throw_after_owned_head=false;
    std::cerr<<e.what()<<'\n';return 1;}}
