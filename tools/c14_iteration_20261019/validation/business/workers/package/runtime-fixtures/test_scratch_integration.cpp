#include "xoffset_validation_scratch.hpp"
#include <functional>
#include <cstring>
#include <iostream>
#include <utility>
namespace xo=XOffsetDatastructure;
struct ScratchRoot {xo::XString name;xo::XVector<std::uint64_t> values;};
XOFFSET_REGISTER_SCHEMA_NAME(ScratchRoot,"test.explicit-scratch.v1")
void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct Upstream:std::pmr::memory_resource{
    bool fail=false;std::function<void()> callback;std::size_t active=0,allocations=0;
    void* do_allocate(std::size_t n,std::size_t a)override{
        if(std::exchange(fail,false))throw std::bad_alloc();
        if(auto fn=std::exchange(callback,{}))fn();
        auto p=std::pmr::new_delete_resource()->allocate(n,a);active+=n;++allocations;return p;
    }
    void do_deallocate(void* p,std::size_t n,std::size_t a)override{active-=n;std::pmr::new_delete_resource()->deallocate(p,n,a);}
    bool do_is_equal(const std::pmr::memory_resource& r)const noexcept override{return this==&r;}
};
int main(){try{
    using Buffer=xo::TypedXBuffer<ScratchRoot>;
    auto source=Buffer::create(8192);source.root().name="wire-value";source.root().values.assign_range(std::array<std::uint64_t,4>{1,3,5,7});
    const auto wire=source.save_verified();const auto base=source.get_address();auto handle=source.handle();
    Upstream upstream;
    {
        xo::ValidationScratch scratch(&upstream);
        for(unsigned i=0;i<3;++i){auto b=xo::load_verified_with_scratch<ScratchRoot>(wire,scratch);need(b.root().name.view()=="wire-value"&&b.root().values[3]==7,"valid wire");}
        const auto allocated=upstream.allocations;need(allocated>0&&xo::ValidationScratch::statistics().cache_hits>0,"no real raw reuse");
        auto broken=wire;const auto terminator=sizeof(xo::XWireHeaderV1)+static_cast<const char*>(source.root().name.data())-static_cast<const char*>(base)+source.root().name.size();broken[terminator]='x';
        bool rejected=false;try{auto b=xo::load_verified_with_scratch<ScratchRoot>(broken,scratch);(void)b;}catch(const xo::XException&){rejected=true;}
        need(rejected,"cached blocks hid invalid input");need(broken[terminator]=='x',"failure changed wire");
        auto b=xo::load_verified_with_scratch<ScratchRoot>(wire,scratch);need(b.root().values.size()==4,"fresh validation state");
        scratch.trim();need(upstream.active==0,"trim still holds original blocks");
        upstream.fail=true;bool failed=false;try{auto q=xo::load_verified_with_scratch<ScratchRoot>(wire,scratch);(void)q;}catch(const std::bad_alloc&){failed=true;}
        need(failed&&upstream.active==0,"primary scratch OOM or rollback");
        bool nested=false;upstream.callback=[&]{auto q=xo::load_verified_with_scratch<ScratchRoot>(wire,scratch);nested=q.root().values[0]==1;};
        auto q=xo::load_verified_with_scratch<ScratchRoot>(wire,scratch);need(nested&&q.root().name.view()=="wire-value","busy reentry not independent");
        need(source.get_address()==base&&source.save_verified()==wire,"source changed");(void)handle;
    }
    need(upstream.active==0,"context did not release raw provenance");
    // The old path does not acquire the explicit context or its registry.
    auto before=xo::ValidationScratch::statistics();auto old=Buffer::load(wire);auto after=xo::ValidationScratch::statistics();
    need(before.fresh_allocations==after.fresh_allocations&&before.cache_hits==after.cache_hits&&old.root().values[2]==5,"old load hidden pool");
    std::cout<<"scratch typed full-revalidation/OOM/reentry/source-transaction groups=7\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
