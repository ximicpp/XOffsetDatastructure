// Uninstrumented timing and a separately compiled diagnostic executable share this trace.
#ifndef S3_DIAGNOSTIC
#define S3_DIAGNOSTIC 0
#endif
#include <cstdlib>
#include <new>
namespace s3_heap { inline bool active=false; inline std::size_t allocations=0,bytes=0; }
#if S3_DIAGNOSTIC
void* operator new(std::size_t n) { if(s3_heap::active){++s3_heap::allocations;s3_heap::bytes+=n;} if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);} void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);} void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#endif
#define XOFFSET_BENCH_VM 1
#define main historical_benchmark_main
#include "bench_vm_compare.cpp"
#undef main
#include <algorithm>
#include <random>
namespace s3 {
void lookup(std::size_t n,const std::string& shape,const std::string& order,std::size_t rounds) {
    XBufferCore b(32*MiB,XBufferCore::maximum_capacity(32*MiB));
    std::vector<void*> donors(n),chosen(std::max(n,rounds));std::vector<std::size_t> permutation(n);
    const auto request=shape=="tail"||shape=="miss"?1024:64u;
    const auto align=shape=="alignment"?64u:16u;
    for(std::size_t i=0;i<n;++i) {
        auto bytes=shape=="dense"?512u:64u;
        if(shape=="tail"&&i+1==n)bytes=4096;
        if(shape=="alignment")bytes=512;
        donors[i]=b.arena()->allocate_aligned(bytes,16);permutation[i]=i;
    }
    // A live guard protects the last donor from being a physical tail in A/B.
    auto* guard=b.arena()->allocate_aligned(64,16);std::memset(guard,99,64);
    if(order=="reverse")std::reverse(permutation.begin(),permutation.end());
    else if(order=="random") {std::mt19937 rng(0x31415926);std::shuffle(permutation.begin(),permutation.end(),rng);}
    s3_heap::active=true;
    for(auto i:permutation)b.arena()->deallocate(donors[i]);
    s3_heap::active=false;
#ifdef S3_PROBE
    auto host_peak=b.s3_host_bytes();
#else
    std::size_t host_peak=0;
#endif
    double elapsed=0;std::uint64_t checksum=0;std::size_t operations=0;
    for(std::size_t cycle=0;cycle<rounds;++cycle) {
        const auto count=shape=="head"||shape=="dense"?n:std::size_t(1);
        auto start=Clock::now();s3_heap::active=true;
        for(std::size_t j=0;j<count;++j) {chosen[j]=b.arena()->allocate_aligned(request,align);std::memset(chosen[j],int((j+cycle)%251),request);}
        if(shape!="miss")for(std::size_t j=0;j<count;++j)b.arena()->deallocate(chosen[j]);
        s3_heap::active=false;elapsed+=milliseconds(start,Clock::now());operations+=count;
#ifdef S3_PROBE
        host_peak=std::max(host_peak,b.s3_host_bytes());
#endif
        // Freed bytes may contain cache metadata only at wire level; Core retains bytes.
        for(std::size_t j=0;j<count;++j)for(std::size_t k=0;k<request;++k) {require(static_cast<unsigned char*>(chosen[j])[k]==(j+cycle)%251,"lookup data changed");checksum=(checksum^static_cast<unsigned char*>(chosen[j])[k])*1099511628211ull;}
    }
    for(unsigned j=0;j<64;++j)require(static_cast<unsigned char*>(guard)[j]==99,"lookup guard changed");
    std::cout<<std::setprecision(17)<<"{\"workload\":\"lookup\",\"shape\":"<<json_string(shape)<<",\"order\":"<<json_string(order)
        <<",\"size\":"<<n<<",\"rounds\":"<<rounds<<",\"operations\":"<<operations<<",\"semantic_checksum\":"<<checksum
        <<",\"elapsed_ms\":"<<elapsed<<",\"used_end\":"<<b.arena()->used_end<<",\"logical_bytes\":"<<b.get_size()
        <<",\"diagnostic\":"<<(S3_DIAGNOSTIC?"true":"false")<<",\"host_allocation_calls\":"<<s3_heap::allocations
        <<",\"host_requested_bytes\":"<<s3_heap::bytes<<",\"host_metadata_peak_bytes\":"<<host_peak
#if S3_DIAGNOSTIC
        <<",\"core_scans\":"<<s3_diagnostics::core_scans<<",\"core_moved_bytes\":"<<s3_diagnostics::core_moved_bytes
        <<",\"core_new\":"<<s3_diagnostics::core_new<<",\"core_whole\":"<<s3_diagnostics::core_whole
        <<",\"core_split\":"<<s3_diagnostics::core_split<<",\"wire_scans\":"<<s3_diagnostics::wire_scans
#endif
        <<"}\n";
}
}
int main(int argc,char** argv) {try {
    std::string workload="lookup",shape="dense",order="forward";std::size_t n=512,rounds=16;
    for(int i=1;i<argc;++i) {require(i+1<argc,"missing flag value");std::string key=argv[i],val=argv[++i];
        if(key=="--workload")workload=val;else if(key=="--shape")shape=val;else if(key=="--order")order=val;
        else if(key=="--size")n=parse_size(val);else if(key=="--rounds")rounds=parse_size(val);else if(key=="--cycles"||key=="--reserve"||key=="--wire"){}else throw std::runtime_error("unknown option");}
    if(workload!="lookup") {
        const auto args=parse_args(argc,argv);Result result;
        if(workload=="save_verified")result=save_workload(args);
        else if(workload=="load_verified")result=load_workload(args);
        else if(workload=="append_preallocated")result=append_workload(args);
        else if(workload=="grow_equal_reserve")result=grow_workload(args);
        else if(workload=="save_repeat") {
            Result setup;auto buffer=prepared_transport_buffer(args,setup);auto first=buffer.save_verified<VmBenchRoot>();
            std::uint64_t checksum=0;auto start=Clock::now();
            for(std::size_t j=0;j<args.cycles;++j) {auto wire=buffer.save_verified<VmBenchRoot>();checksum^=wire.size()+j;}
            const auto elapsed=milliseconds(start,Clock::now());
            (void)verify_values(buffer,args.size/sizeof(std::uint32_t));
            std::cout<<std::setprecision(17)<<"{\"workload\":\"save_repeat\",\"size\":"<<args.size<<",\"requested_cycles\":"<<args.cycles
                <<",\"requested_reserve_bytes\":"<<args.reserve<<",\"operation_count\":"<<args.cycles<<",\"checksum\":"<<checksum<<",\"elapsed_ms\":"<<elapsed<<"}\n";
            return 0;
        } else throw std::runtime_error("unknown legacy workload");
        print_result(result);return 0;
    }
    require(n&&n<=4096&&rounds&&rounds<=512,"work budget exceeded");
    s3::lookup(n,shape,order,rounds);
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;} }
