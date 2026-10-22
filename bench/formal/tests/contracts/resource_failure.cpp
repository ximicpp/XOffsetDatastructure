// Application failure-lifetime gate. Not a candidate allocator benchmark.
// Compile separately for each SC_ADAPTER_HEADER / SC_ADAPTER_TYPE.
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>

namespace controlled_new {
std::atomic<bool> armed{false};
std::atomic<unsigned> failures{0};
std::atomic<std::size_t> failed_bytes{0};
void check(std::size_t n){
    if(armed.exchange(false,std::memory_order_relaxed)){
        failed_bytes.store(n,std::memory_order_relaxed);failures.fetch_add(1,std::memory_order_relaxed);
        throw std::bad_alloc();
    }
}
void* allocate(std::size_t n){check(n);if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* aligned(std::size_t n,std::align_val_t a){
    check(n);auto align=static_cast<std::size_t>(a);n=n?n:1;
    if(!align||n>std::numeric_limits<std::size_t>::max()-(align-1))throw std::bad_alloc();
    n=(n+align-1)/align*align;
    if(auto* p=std::aligned_alloc(align,n))return p;throw std::bad_alloc();
}
}

void* operator new(std::size_t n){return controlled_new::allocate(n);}
void* operator new[](std::size_t n){return controlled_new::allocate(n);}
void* operator new(std::size_t n,std::align_val_t a){return controlled_new::aligned(n,a);}
void* operator new[](std::size_t n,std::align_val_t a){return controlled_new::aligned(n,a);}
void* operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void operator delete(void* p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void* p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete(void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void* p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}

#include "adapter.hpp"
#include SC_ADAPTER_HEADER
#include <iostream>

namespace {
using A=SC_ADAPTER_TYPE;
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void probe(){
    sc::Snapshot initial;initial.epoch=1;
    for(std::uint64_t id=1;id<=8;++id){sc::Record r;r.id=id;r.name="initial";r.samples={1,2,3,4};r.payload={0,1,2,3};r.attrs={{"a",{1,2,3,4}}};initial.records.push_back(r);}
    auto original=A::build(initial);auto initial_wire=A::emit(original);original={};
    const auto initial_sha=sc::sha256_hex(sc::wire_view(initial_wire));
    std::vector<sc::Mutation> growth(4);
    growth[0].code=sc::MutationCode::replace_name;growth[0].stage=2;growth[0].id=1;growth[0].text=std::string(4096,'g');
    growth[1].code=sc::MutationCode::append_samples;growth[1].stage=3;growth[1].id=2;growth[1].samples.assign(2048,0xffffffff);
    growth[2].code=sc::MutationCode::replace_payload;growth[2].stage=4;growth[2].id=3;growth[2].bytes.assign(8192,0xa5);
    growth[3].code=sc::MutationCode::insert_attr;growth[3].stage=6;growth[3].id=4;growth[3].text="b";growth[3].bytes.assign(4096,0x7f);
    for(std::size_t i=0;i<growth.size();++i)growth[i].sequence=i;
    auto expected=initial;for(auto& op:growth)need(sc::apply_reference(expected,op)==op.expected,"reference growth");
    std::vector<sc::Mutation> erase(2);
    for(std::size_t i=0;i<erase.size();++i){erase[i].sequence=i;erase[i].stage=7;erase[i].code=sc::MutationCode::erase_record;erase[i].id=1;}
    erase[1].expected=sc::Status::not_found;
    bool caught=false,partial_growth=false;
    try {
        auto modified=A::load(sc::wire_view(initial_wire));
        A::mutate(modified,growth);
        need(A::export_state(modified)==expected,"actual partial native growth");partial_growth=true;
        // Every stage7 adapter first allocates C++ bookkeeping. Fixture, trace,
        // oracle and the initial owning wire all exist before this one-shot arm.
        controlled_new::armed.store(true,std::memory_order_relaxed);
        A::mutate(modified,erase);
        controlled_new::armed.store(false,std::memory_order_relaxed);
        throw std::runtime_error("stage7 did not encounter controlled C++ new");
    } catch(const std::bad_alloc&) {
        // modified is out of scope and its native RAII owner already unwound.
        caught=true;
    } catch(...) {controlled_new::armed.store(false,std::memory_order_relaxed);throw;}
    controlled_new::armed.store(false,std::memory_order_relaxed);
    need(caught&&partial_growth&&controlled_new::failures.load()==1,"controlled failure count/scope");
    need(sc::sha256_hex(sc::wire_view(initial_wire))==initial_sha,"initial owning wire changed on failure");
    auto restored=A::load(sc::wire_view(initial_wire));need(A::export_state(restored)==initial,"initial wire cannot restore after failure");restored={};
    need(sc::sha256_hex(sc::wire_view(initial_wire))==initial_sha,"initial owning wire changed on recovery");
    std::cout<<"{\"schema\":\"sc-resource-lifecycle-v1\",\"library\":\""<<A::library_id<<"\",\"mechanism\":\"one_shot_CXX_operator_new_in_stage7_bookkeeping\",\"controlled_bad_allocs\":1,\"failed_request_bytes\":"<<controlled_new::failed_bytes.load()<<",\"actual_native_growth_before_failure\":true,\"modified_state\":\"RAII_unwound\",\"initial_wire_sha256\":\""<<initial_sha<<"\",\"initial_wire_unchanged_and_restorable\":true,\"does_not_cover\":[\"all_internal_allocators\",\"cista_malloc\",\"XOffset_OS_failure\",\"multi_operation_transaction\"],\"performance_samples\":0}\n";
}
}
int main(){try{probe();return 0;}catch(const std::exception& e){controlled_new::armed.store(false);std::cerr<<e.what()<<"\n";return 1;}catch(...){controlled_new::armed.store(false);std::cerr<<"resource lifetime probe error\n";return 1;}}
