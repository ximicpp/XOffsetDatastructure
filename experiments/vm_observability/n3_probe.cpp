// Predictable diagnostic semantics. No formal timing or throughput measurement.
#include "n3_runtime.hpp"
#include "xoffsetdatastructure.hpp"
#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>
#ifndef N3_DIAGNOSTIC
#define N3_DIAGNOSTIC 1
#endif
using namespace XOffsetDatastructure;
struct N3Root { std::int32_t id{}; bool flag{}; XVector<std::int32_t> values, blocker; XString text; };
XOFFSET_REGISTER_SCHEMA_NAME(N3Root, "experiment.N3Root.v1")
using Sem=std::map<std::string,std::uint64_t>;
void require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
XBuffer make(std::int32_t value) {auto x=XBuffer::create<N3Root>(8192,XBuffer::maximum_capacity(65536));x.root<N3Root>().id=value;return x;}
Sem basic() {
    n3::Phase phase("owner_basic");
    {XBufferCore c(4096,XBufferCore::maximum_capacity(16384));auto* p=c.arena()->allocate_aligned(32,16);*static_cast<std::uint64_t*>(p)=77;
     require(*static_cast<std::uint64_t*>(p)==77,"raw bytes");require(c.copy_bytes().size()==4096,"owning copy extent");c.arena()->deallocate(p);}
    return {{"owners",1},{"lookup_calls",2},{"checksum",77}};
}
Sem transfer() {
    n3::Phase phase("owner_transfer");
    auto a=make(11);auto b=make(22);auto handle=a.handle<N3Root>();auto* base=a.get_address();auto alloc=a.allocator<std::int32_t>();
    n3::Scope outer_scope(a.get_address());
    XBufferCore::arena_header orphan{};bool missing=false;
    try{(void)orphan.allocate_aligned(1,16);}catch(const XException& e){missing=std::string(e.what())=="arena owner lookup failed";}
    require(missing,"unregistered arena must report exact owner miss");
    XBuffer moved(std::move(a));require(!handle,"moved handle must invalidate");require(a.copy_bytes().empty(),"moved-from copy is empty");
    a.swap(b);static_cast<XBufferCore&>(b)=std::move(static_cast<XBufferCore&>(moved));
    a=std::move(b);require(handle->id==11 && a.get_address()==base,"handle and address follow object contract");
    auto* p=alloc.allocate(1);*p=27;require(*p==27,"allocator follows arena across moves");alloc.deallocate(p,1);
    return {{"checksum",38},{"owners",2}};
}
Sem metadata() {
    n3::Phase phase("core_metadata");
    XBufferCore a(8192,XBufferCore::maximum_capacity(16384)),b(8192,XBufferCore::maximum_capacity(16384));
    void* pa[8];void* pb[3];
    for(auto& p:pa)p=a.arena()->allocate_aligned(32,16);
    for(auto& p:pb)p=b.arena()->allocate_aligned(64,16);
    for(auto p:pa)a.arena()->deallocate(p);
    for(auto p:pb)b.arena()->deallocate(p);
    auto* reused=a.arena()->allocate_aligned(16,16);require(reused==pa[0],"F donor must remain first-fit whole block");
    XBufferCore::AllocationHeader h;std::memcpy(&h,static_cast<char*>(reused)-sizeof(h),sizeof(h));
    require(h.user_bytes==32,"donor must remain whole");a.arena()->deallocate(reused);
    a.swap(b);require(a.get_size()==8192 && b.get_size()==8192,"swap extents");
    return {{"owners",2},{"core_new",11},{"core_returns",12},{"donor_user_bytes",32},{"checksum",11}};
}
Sem wire() {
    n3::Phase phase("copy_wire");
    auto a=make(42);auto& r=a.root<N3Root>();r.flag=true;
    r.values.reserve(4);for(int i=0;i<4;++i)r.values.push_back(i+10);
    r.blocker.reserve(4);r.values.reserve(16); // force four completed relocations
    r.text="abc";r.blocker.reserve(16);r.text.reserve(128); // force three copied characters
    auto bytes=a.save_verified<N3Root>();XWireHeaderV1 h;std::memcpy(&h,bytes.data(),sizeof(h));
    auto loaded=XBuffer::load_verified<N3Root>(bytes,XBuffer::maximum_capacity(65536));
    require(loaded.root<N3Root>().id==42 && loaded.root<N3Root>().text=="abc","wire round trip");
    for(int i=0;i<4;++i)require(loaded.root<N3Root>().values[i]==i+10,"wire element");
    auto bad=bytes;bad[0]='?';bool failed=false;
    try{(void)XBuffer::load_verified<N3Root>(bad);}catch(const XException& e){failed=std::string(e.what()).find("bad magic")!=std::string::npos;}
    require(failed,"bad magic must fail for the exact reason");
    bad=bytes;bad[sizeof(h)+h.root_offset+offsetof(N3Root,flag)]=2;failed=false;
    try{(void)XBuffer::load_verified<N3Root>(bad);}catch(const XException& e){failed=std::string(e.what()).find("wire validation: invalid bool representation")!=std::string::npos;}
    require(failed,"invalid bool graph must fail verified validation");
    return {{"checksum",88},{"wire_bytes",bytes.size()},{"payload_bytes",h.used_bytes},{"relocated_bytes",16},{"saved_frames",1},{"payload_loads",2}};
}
Sem pool() {
    n3::Phase phase("pool_reclaim");using S=detail::VirtualMemoryStorage;using P=detail::VmStoragePool;
    std::size_t page=0;
    {S s(65536,131072);page=s.page_size();s.data()[0]=19;s.resize_logical(64);auto r=s.reclaim_tail();require(r.complete(),"reclaim");
     require(s.data()[0]==19 && s.grow_to(65536),"regrow");for(std::size_t i=64;i<65536;++i)require(s.data()[i]==0,"regrow zero");}
    P p({2,131072,131072});
    {S s(512,65536,p);s.data()[0]=7;}
    {S s(512,65536,p);require(s.data()[0]==0,"pool reexposure zero");}
    require(p.stats().hits==1 && p.stats().misses==1,"pool hit/miss");p.clear();require(p.stats().cached_blocks==0,"clear");
    return {{"checksum",19},{"hits",1},{"misses",1},{"page_bytes",page}};
}
Sem threaded() {
    std::atomic<int> ready{0};std::atomic<bool> go{false};std::atomic<std::uint64_t> sum{0};std::vector<std::thread> workers;
    for(int id=0;id<2;++id)workers.emplace_back([&,id]{n3::Phase phase("threads");
       XBufferCore c(4096,XBufferCore::maximum_capacity(16384));auto* p=c.arena()->allocate_aligned(32,16);c.arena()->deallocate(p);
       ++ready;while(!go.load())std::this_thread::yield();
       auto* q=c.arena()->allocate_aligned(16,16);require(q==p,"thread donor");c.arena()->deallocate(q);sum.fetch_add(id+5);});
    while(ready.load()!=2)std::this_thread::yield();go=true;for(auto& t:workers)t.join();
    return {{"checksum",sum.load()},{"worker_threads",2},{"owners",2}};
}
Sem registry_failure() {
    n3::Phase phase("failure_registry");auto existing=make(73);auto* base=existing.get_address();bool failed=false;
    detail::fail_next_registry_registration();try{(void)make(1);}catch(const XBadAlloc&){failed=true;}detail::clear_registry_failures();
    require(failed && existing.get_address()==base && existing.root<N3Root>().id==73,"registration rollback");
#if N3_DIAGNOSTIC
    n3::fail_after(2,0);failed=false;try{(void)make(2);}catch(const XBadAlloc&){failed=true;}n3::clear_failure();require(failed,"registry allocator failure");
#endif
    auto retry=make(9);require(retry.root<N3Root>().id==9,"registry retry");return {{"checksum",82}};
}
Sem metadata_failure() {
    n3::Phase phase("failure_metadata");XBufferCore c(4096,XBufferCore::maximum_capacity(16384));auto* p=c.arena()->allocate_aligned(32,16);
#if N3_DIAGNOSTIC
    auto free_before=c.get_free_memory();n3::fail_after(1,0);c.arena()->deallocate(p);n3::clear_failure();
    require(c.get_free_memory()==free_before,"metadata OOM loses only reclamation, preserving noexcept contract");
    c.arena()->deallocate(p);auto* q=c.arena()->allocate_aligned(16,16);require(q==p,"retry return donor");
#endif
    return {{"checksum",32}};
}
Sem vm_failure() {
    n3::Phase phase("failure_vm");using S=detail::VirtualMemoryStorage;S s(64,131072);s.data()[0]=31;auto* base=s.data();
    detail::fail_next_vm_operation(detail::VmOperation::Prepare);require(!s.grow_to(65536),"prepare must fail");
    require(s.data()==base && s.size()==64 && s.data()[0]==31,"grow failure atomicity");
    require(s.grow_to(65536),"grow retry");s.resize_logical(64);detail::fail_next_vm_operation(detail::VmOperation::Reclaim);
    require(s.reclaim_tail().status==detail::VmReclaimStatus::Failed,"reclaim failure");detail::clear_vm_failures();
    require(s.reclaim_tail().complete(),"reclaim retry");return {{"checksum",31}};
}
int main(int argc,char** argv) {
    try {
        if(argc!=3 || std::string(argv[1])!="--case")return 2;
        const std::string name=argv[2];Sem sem;bool control=false;
        if(name=="owner_basic")sem=basic();else if(name=="owner_transfer")sem=transfer();else if(name=="core_metadata")sem=metadata();
        else if(name=="copy_wire")sem=wire();else if(name=="pool_reclaim")sem=pool();else if(name=="threads")sem=threaded();
        else if(name=="failure_registry")sem=registry_failure();else if(name=="failure_metadata")sem=metadata_failure();else if(name=="failure_vm")sem=vm_failure();
        else if(name=="recorder_overflow") {control=true;for(int i=0;i<17000;++i)n3::emit("overflow_control");sem={{"checksum",17000}};}
        else return 2;
        std::cout<<"{\"schema\":\"n3-probe-v1\",\"case\":\""<<name<<"\",\"status\":\""<<(control?"passed_control":"passed")<<"\",\"semantic\":{";
        bool comma=false;for(auto& [key,value]:sem){if(comma)std::cout<<',';comma=true;std::cout<<'"'<<key<<"\":"<<value;}
        std::cout<<"},\"trace\":";
#if N3_DIAGNOSTIC
        n3::dump(std::cout);
#else
        std::cout<<"null";
#endif
        std::cout<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
