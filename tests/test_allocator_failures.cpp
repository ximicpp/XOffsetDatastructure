#include <algorithm>
// Real global-new failures, armed only around the allocator operation under test.
#include <array>
#include <cstdlib>
#include <new>
#include <iostream>
#include <cstring>
#include "allocator_model_support.hpp"
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;
using allocator_model::require;
namespace { thread_local long fail_after=-1; thread_local bool watch=false; thread_local std::size_t attempts=0,hits=0; }
void* operator new(std::size_t n) {
    if(watch) { ++attempts; if(fail_after==0) {fail_after=-1;++hits;throw std::bad_alloc();} if(fail_after>0)--fail_after; }
    if(auto* p=std::malloc(n?n:1))return p; throw std::bad_alloc();
}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
struct AllocatorFailureRoot {XVector<std::uint32_t> values;};
XOFFSET_RECORD_DERIVED(AllocatorFailureRoot, (), values)
XOFFSET_REGISTER_SCHEMA_NAME(AllocatorFailureRoot,"test.AllocatorFailureRoot")
namespace {
using Holes=std::vector<XBufferCore::WireHole>;
Holes holes_of(const XBufferCore& b) { Holes out; b.for_each_free_block([&](XBufferCore::WireHole h){out.push_back(h);}); return out; }
bool same(const Holes& a,const Holes& c) {
    if(a.size()!=c.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(a[i].offset!=c[i].offset||a[i].bytes!=c[i].bytes) return false;
    return true;
}
}
// The allocator keeps its state in the payload: no allocation, retirement (of
// any size, the free directory's growth included), save or verified load of
// the free blocks makes a host allocation, so there is no failure point to
// arm, and every entry is kept.
void inband_makes_no_host_allocation() {
    auto b=XBuffer::create<AllocatorFailureRoot>(4*1024*1024,XBuffer::maximum_capacity(4*1024*1024));
    std::vector<void*> blocks; blocks.reserve(4096);
    attempts=hits=0;fail_after=0;watch=true;
    for(unsigned i=0;i<4096;++i) blocks.push_back(b.arena()->allocate_aligned(8+8*(i%61)+(i%97==0?8192:0),8));
    for(unsigned i=0;i<4096;i+=2) b.arena()->deallocate(blocks[i],8+8*(i%61)+(i%97==0?8192:0));
    for(unsigned i=0;i<2048;++i) blocks[2*i]=b.arena()->allocate_aligned(8+8*(i%37),8);
    for(unsigned i=0;i<4096;++i) b.arena()->deallocate(blocks[i],i%2 ? 8+8*(i%61)+(i%97==0?8192:0) : 8+8*((i/2)%37));
    watch=false;fail_after=-1;
    require(attempts==0 && hits==0,"an in-band allocator operation made a host allocation");
    require(b.free_table_complete() && b.free_block_count()!=0,"in-band retirements lost an entry");
    // Free-run emission carries the image itself; the automatic default would
    // send this churned buffer as compact wire, which has no free blocks.
    const XSaveOptions image{.emission=XWireEmission::free_runs};
    const auto frame=b.save_verified<AllocatorFailureRoot>(image);
    auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame);
    require(same(holes_of(loaded),holes_of(b)) && loaded.save_verified<AllocatorFailureRoot>(image)==frame,
            "in-band free blocks did not travel with the payload");
}

int main() {try {
    inband_makes_no_host_allocation();
    std::cout<<"{\"status\":\"passed\",\"in_band\":true}\n";
    return 0;
} catch(const std::exception& e) {watch=false;std::cerr<<e.what()<<'\n';return 1;} }
