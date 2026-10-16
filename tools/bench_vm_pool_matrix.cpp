// OS-layer pool experiments. Diagnostics use a separately generated header.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace pool_diagnostics {
using Clock = std::chrono::steady_clock;
struct Counts {
    std::uint64_t scans{}, locks{}, wait_ns{}, hold_ns{}, mmap_calls{}, mprotect_calls{}, munmap_calls{};
    Counts& operator+=(const Counts& x) {
        scans+=x.scans; locks+=x.locks; wait_ns+=x.wait_ns; hold_ns+=x.hold_ns;
        mmap_calls+=x.mmap_calls; mprotect_calls+=x.mprotect_calls; munmap_calls+=x.munmap_calls;
        return *this;
    }
};
inline thread_local Counts local;
class Lock {
    std::mutex& mutex; Clock::time_point acquired;
public:
    explicit Lock(std::mutex& m):mutex(m) {
        const auto before=Clock::now(); mutex.lock(); acquired=Clock::now();
        ++local.locks; local.wait_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(acquired-before).count();
    }
    ~Lock() { local.hold_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-acquired).count(); mutex.unlock(); }
};
#if defined(__linux__) || defined(__APPLE__)
inline void* mmap_counted(void* p, std::size_t n, int protection, int flags, int fd, off_t offset) {
    ++local.mmap_calls; return ::mmap(p,n,protection,flags,fd,offset);
}
inline int mprotect_counted(void* p,std::size_t n,int protection) {
    ++local.mprotect_calls; return ::mprotect(p,n,protection);
}
inline int munmap_counted(void* p,std::size_t n) {
    ++local.munmap_calls; return ::munmap(p,n);
}
#endif
}
#include "xoffset_virtual_memory.hpp"
using Storage=XOffsetDatastructure::detail::VirtualMemoryStorage;
using Pool=XOffsetDatastructure::detail::VmStoragePool;
using Clock=std::chrono::steady_clock;
constexpr std::size_t KiB=1024, MiB=1024*KiB;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
double ms(Clock::duration d) { return std::chrono::duration<double,std::milli>(d).count(); }
double stamp() { return ms(Clock::now().time_since_epoch()); }
template<class F> double timed(F f) { auto t=Clock::now();f();return ms(Clock::now()-t); }
std::uint64_t rss() {
#if defined(__APPLE__)
    mach_task_basic_info info{}; mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
    require(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS,"RSS failed");
    return info.resident_size;
#elif defined(__linux__)
    FILE* f=std::fopen("/proc/self/statm","r"); require(f!=nullptr,"RSS open failed");
    unsigned long total=0,resident=0; const auto n=std::fscanf(f,"%lu %lu",&total,&resident);std::fclose(f);
    require(n==2,"RSS read failed");return resident*static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
#else
    throw std::runtime_error("RSS unsupported in benchmark");
#endif
}
class Barrier {
    std::mutex mutex; std::condition_variable cv; std::size_t count{}, generation{}, total;
public:
    explicit Barrier(std::size_t n):total(n){}
    void wait() { std::unique_lock<std::mutex> lock(mutex);const auto old=generation;
        if(++count==total){count=0;++generation;cv.notify_all();}else cv.wait(lock,[&]{return old!=generation;}); }
};
struct Config {
    std::string workload="matrix", distribution="fixed", policy="shared", measurement="throughput";
    std::size_t cache=8, threads=1, cycles=16, idle_ms=0;
};
struct Result {
    Config config; std::map<std::string,double> metrics;
    std::map<std::string,std::vector<double>> samples;
    std::uint64_t checksum{}, operations{};
    pool_diagnostics::Counts diagnostic;
};
std::size_t part(std::size_t n,std::size_t i,std::size_t total) { return n/total+(i<n%total); }
std::size_t capacity(const Config& c,std::size_t cycle,std::size_t index) {
    if(c.distribution=="fixed")return 64*KiB;
    if(c.distribution=="mixed")return (16*KiB)<<(index%5);
    if(c.distribution=="bimodal")return index%2?256*KiB:16*KiB;
    if(c.distribution=="phase")return cycle<c.cycles/2?32*KiB:128*KiB;
    throw std::runtime_error("unknown distribution");
}
void validate_idle(const Pool& pool,Pool::Limits limits) {
    const auto p=pool.stats();require(p.cached_blocks<=limits.max_cached_blocks,"count budget");
    require(p.cached_reserved_bytes<=limits.max_reserved_bytes,"reserved budget");
    require(p.cached_accessible_bytes<=limits.max_accessible_bytes,"accessible budget");
}
void stats(Result& result,const std::vector<Pool>& pools,const std::string& prefix) {
    Pool::Stats all{};
    for(const auto& pool:pools){auto p=pool.stats();all.cached_blocks+=p.cached_blocks;all.cached_reserved_bytes+=p.cached_reserved_bytes;
        all.cached_accessible_bytes+=p.cached_accessible_bytes;all.hits+=p.hits;all.misses+=p.misses;}
    result.metrics[prefix+"blocks"]=all.cached_blocks;result.metrics[prefix+"reserved_bytes"]=all.cached_reserved_bytes;
    result.metrics[prefix+"accessible_bytes"]=all.cached_accessible_bytes;result.metrics[prefix+"hits"]=all.hits;result.metrics[prefix+"misses"]=all.misses;
}
void check_zero(const Storage& owner) { for(std::size_t i=0;i<owner.size();++i)require(owner.data()[i]==0,"new exposure not zero"); }
Result matrix(Config c) {
    Result r;r.config=c;
    const auto live_count=std::max(c.cache,c.threads);
    const Pool::Limits limits{c.cache,c.cache*256*KiB,c.cache*64*KiB};
    std::vector<Pool> pools;std::vector<Pool::Limits> shares;
    if(c.policy=="shared"){pools.emplace_back(limits);shares.push_back(limits);}
    else if(c.policy=="local")for(std::size_t t=0;t<c.threads;++t){auto count=part(c.cache,t,c.threads);shares.push_back({count,count*256*KiB,count*64*KiB});pools.emplace_back(shares.back());}
    else require(c.policy=="none","unknown pool policy");
    std::vector<Pool> handles(c.threads);
    for(std::size_t t=0;t<c.threads;++t)if(!pools.empty())handles[t]=pools[c.policy=="shared"?0:t];
    struct ThreadResult { std::vector<double> begin_create,end_create,begin_release,end_release,create_ns,release_ns; std::uint64_t checksum{};pool_diagnostics::Counts diagnostic; };
    std::vector<ThreadResult> results(c.threads);
    std::vector<double> cycle_hits(c.cycles),cycle_misses(c.cycles),cycle_cached(c.cycles);
    for(const auto& share:shares){r.samples["count_budgets"].push_back(share.max_cached_blocks);
        r.samples["reserved_budgets"].push_back(share.max_reserved_bytes);r.samples["accessible_budgets"].push_back(share.max_accessible_bytes);}
    for(auto& x:results){x.begin_create.resize(c.cycles);x.end_create.resize(c.cycles);x.begin_release.resize(c.cycles);x.end_release.resize(c.cycles);
        if(c.measurement=="latency"){x.create_ns.reserve(c.cycles*live_count);x.release_ns.reserve(c.cycles*live_count);}}
    Barrier barrier(c.threads);std::atomic<bool> done{false};std::atomic<std::uint64_t> clears{0};
    pool_diagnostics::Counts clearer_diagnostic;std::vector<std::thread> threads;
    std::thread clearer;
    if(c.workload=="concurrent_clear") {
        require(c.policy=="shared","clear stress requires shared pool");
        clearer=std::thread([&]{pool_diagnostics::local={};while(!done.load()){pools[0].clear();++clears;std::this_thread::yield();}clearer_diagnostic=pool_diagnostics::local;});
    }
    r.metrics["rss_before_bytes"]=rss();
    const auto process_begin=Clock::now();
    for(std::size_t t=0;t<c.threads;++t)threads.emplace_back([&,t]{
        try {
            auto& out=results[t];std::vector<std::optional<Storage>> owners(part(live_count,t,c.threads));
            std::size_t first=0;for(std::size_t j=0;j<t;++j)first+=part(live_count,j,c.threads);
            pool_diagnostics::local={};
            for(std::size_t cycle=0;cycle<c.cycles;++cycle) {
                barrier.wait();out.begin_create[cycle]=stamp();
                for(std::size_t i=0;i<owners.size();++i){auto cap=capacity(c,cycle,first+i);Clock::time_point begin{};if(c.measurement=="latency")begin=Clock::now();owners[i].emplace(std::min(cap,64*KiB),cap,handles[t]);
                    if(c.measurement=="latency")out.create_ns.push_back(std::chrono::duration<double,std::nano>(Clock::now()-begin).count());}
                out.end_create[cycle]=stamp();barrier.wait();
                for(std::size_t i=0;i<owners.size();++i){check_zero(*owners[i]);auto marker=static_cast<unsigned char>((first+i+cycle)%251+1);
                    std::memset(owners[i]->data(),marker,owners[i]->size());out.checksum+=marker*owners[i]->size();}
                barrier.wait();out.begin_release[cycle]=stamp();
                for(auto& owner:owners){Clock::time_point begin{};if(c.measurement=="latency")begin=Clock::now();owner.reset();if(c.measurement=="latency")out.release_ns.push_back(std::chrono::duration<double,std::nano>(Clock::now()-begin).count());}
                out.end_release[cycle]=stamp();barrier.wait();
                if(t==0)for(std::size_t p=0;p<pools.size();++p){validate_idle(pools[p],shares[p]);auto sample=pools[p].stats();
                    cycle_hits[cycle]+=sample.hits;cycle_misses[cycle]+=sample.misses;cycle_cached[cycle]+=sample.cached_blocks;}
                barrier.wait();
            }
            out.diagnostic=pool_diagnostics::local;
        } catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::abort();}
    });
    for(auto& t:threads)t.join();done=true;if(clearer.joinable())clearer.join();
    r.metrics["wall_with_checks_ms"]=ms(Clock::now()-process_begin);r.diagnostic+=clearer_diagnostic;
    for(std::size_t cycle=0;cycle<c.cycles;++cycle){double begin_create=1e100,end_create=0,begin_release=1e100,end_release=0;
        for(const auto& x:results){begin_create=std::min(begin_create,x.begin_create[cycle]);end_create=std::max(end_create,x.end_create[cycle]);
            begin_release=std::min(begin_release,x.begin_release[cycle]);end_release=std::max(end_release,x.end_release[cycle]);}
        const double elapsed=end_create-begin_create+end_release-begin_release;r.samples["cycle_ms"].push_back(elapsed);
        r.metrics["timed_ms"]+=elapsed;if(cycle==0)r.metrics["cold_ms"]=elapsed;else r.metrics["warm_ms"]+=elapsed;
        if(c.distribution=="phase"&&cycle==c.cycles/2)r.metrics["switch_ms"]=elapsed;
    }
    for(auto& x:results){r.checksum+=x.checksum;r.diagnostic+=x.diagnostic;
        r.samples["acquire_ns"].insert(r.samples["acquire_ns"].end(),x.create_ns.begin(),x.create_ns.end());
        r.samples["release_ns"].insert(r.samples["release_ns"].end(),x.release_ns.begin(),x.release_ns.end());}
    r.operations=live_count*c.cycles;r.metrics["operations_per_second"]=r.operations/(r.metrics["timed_ms"]/1000);
    r.samples["cumulative_hits"]=std::move(cycle_hits);r.samples["cumulative_misses"]=std::move(cycle_misses);r.samples["cached_blocks"]=std::move(cycle_cached);
    r.metrics["clear_calls"]=clears;r.metrics["total_count_budget"]=limits.max_cached_blocks;
    r.metrics["total_reserved_budget"]=limits.max_reserved_bytes;r.metrics["total_accessible_budget"]=limits.max_accessible_bytes;
    r.metrics["rss_idle_bytes"]=rss();stats(r,pools,"idle_");
    r.metrics["final_clear_ms"]=timed([&]{for(const auto& p:pools)p.clear();});stats(r,pools,"cleared_");
    require(r.metrics["cleared_blocks"]==0,"clear left idle mappings");r.metrics["rss_cleared_bytes"]=rss();return r;
}
Result idle(Config c) {
    Result r;r.config=c;const bool live=c.workload=="live_reclaim";const std::size_t bytes=live?64*MiB:MiB,count=live?1:16;
    Pool pool({count,count*bytes,count*bytes});std::vector<std::optional<Storage>> owners(count);
    const auto selected=c.policy=="none"?Pool{}:pool;
    r.metrics["rss_before_bytes"]=rss();
    for(std::size_t cycle=0;cycle<c.cycles;++cycle){
        if(!live||cycle==0)r.samples["first_acquire_ms"].push_back(timed([&]{for(auto& o:owners)o.emplace(bytes,bytes,selected);}));
        for(auto& o:owners){check_zero(*o);std::memset(o->data(),0x5a,o->size());}r.samples["rss_live_bytes"].push_back(rss());
        char* base=owners[0]->data();
        if(live){r.samples["shrink_ms"].push_back(timed([&]{owners[0]->resize_logical(16*KiB);}));
            if(c.policy=="reclaim")r.samples["reclaim_ms"].push_back(timed([&]{auto result=owners[0]->reclaim_tail();require(result.complete()&&result.discarded_bytes>0,"reclaim did not discard");r.metrics["discarded_bytes"]+=result.discarded_bytes;}));
            r.samples["idle_accessible_bytes"].push_back(owners[0]->accessible_bytes());
        }else{r.samples["return_ms"].push_back(timed([&]{for(auto& o:owners)o.reset();}));
            if(c.policy=="clear")r.samples["clear_ms"].push_back(timed([&]{pool.clear();}));
            validate_idle(pool,{count,count*bytes,count*bytes});r.samples["idle_accessible_bytes"].push_back(pool.stats().cached_accessible_bytes);}
        r.samples["rss_idle_start_bytes"].push_back(rss());std::this_thread::sleep_for(std::chrono::milliseconds(c.idle_ms));r.samples["rss_idle_end_bytes"].push_back(rss());
        if(live){r.samples["reuse_ms"].push_back(timed([&]{require(owners[0]->grow_to(bytes),"regrow failed");}));require(base==owners[0]->data(),"live base moved");
            for(std::size_t i=0;i<bytes;++i)require(owners[0]->data()[i]==(i<16*KiB?0x5a:0),"regrow bytes changed");std::memset(owners[0]->data(),0,16*KiB);
        }else{r.samples["reuse_ms"].push_back(timed([&]{for(auto& o:owners)o.emplace(bytes,bytes,selected);}));for(auto& o:owners)check_zero(*o);for(auto& o:owners)o.reset();}
        r.samples["rss_reused_bytes"].push_back(rss());r.checksum+=bytes*count;
    }
    r.operations=count*c.cycles;r.metrics["timed_ms"]=0;for(const auto& item:r.samples)if(item.first.find("_ms")!=std::string::npos)for(double value:item.second)r.metrics["timed_ms"]+=value;
    return r;
}
Result lifetime(Config c) {
    Result r;r.config=c;
    for(std::size_t cycle=0;cycle<c.cycles;++cycle){std::optional<Storage> owner;
        {Pool pool({1,256*KiB,256*KiB});owner.emplace(16*KiB,256*KiB,pool);owner->data()[0]=42;pool.clear();require(owner->data()[0]==42,"clear damaged live owner");}
        const auto base=owner->data();require(owner->grow_to(256*KiB),"growth after handle destruction failed");require(base==owner->data()&&owner->data()[0]==42,"owner lifetime changed");
        for(std::size_t i=16*KiB;i<owner->size();++i)require(owner->data()[i]==0,"lifetime growth not zero");owner.reset();++r.operations;r.checksum+=42;}
    r.metrics["timed_ms"]=0;return r;
}
void write(Result& r) {
    std::cout<<std::setprecision(12)<<"{\"workload\":\""<<r.config.workload<<"\",\"distribution\":\""<<r.config.distribution<<"\",\"policy\":\""<<r.config.policy
        <<"\",\"measurement\":\""<<r.config.measurement<<"\",\"cache\":"<<r.config.cache<<",\"threads\":"<<r.config.threads<<",\"cycles\":"<<r.config.cycles
        <<",\"idle_ms\":"<<r.config.idle_ms<<",\"operations\":"<<r.operations<<",\"checksum\":"<<r.checksum<<",\"metrics\":{";
    bool first=true;for(const auto& item:r.metrics){if(!first)std::cout<<',';first=false;std::cout<<'"'<<item.first<<"\":"<<item.second;}std::cout<<"},\"samples\":{";
    first=true;for(const auto& item:r.samples){if(!first)std::cout<<',';first=false;std::cout<<'"'<<item.first<<"\":[";bool first_value=true;for(auto v:item.second){if(!first_value)std::cout<<',';first_value=false;std::cout<<v;}std::cout<<']';}
    std::cout<<"},\"diagnostics\":";
#if defined(XOFFSET_POOL_DIAGNOSTICS)
    auto d=r.diagnostic;std::cout<<"{\"scans\":"<<d.scans<<",\"locks\":"<<d.locks<<",\"wait_ns\":"<<d.wait_ns<<",\"hold_ns\":"<<d.hold_ns
        <<",\"mmap_calls\":"<<d.mmap_calls<<",\"mprotect_calls\":"<<d.mprotect_calls<<",\"munmap_calls\":"<<d.munmap_calls<<'}';
#else
    std::cout<<"null";
#endif
    std::cout<<"}\n";
}
int main(int argc,char** argv) {
    try { Config c;for(int i=1;i<argc;i+=2){require(i+1<argc,"missing argument value");std::string key=argv[i],value=argv[i+1];
        if(key=="--workload")c.workload=value;else if(key=="--distribution")c.distribution=value;else if(key=="--policy")c.policy=value;
        else if(key=="--measurement")c.measurement=value;else if(key=="--cache")c.cache=std::stoull(value);else if(key=="--threads")c.threads=std::stoull(value);
        else if(key=="--cycles")c.cycles=std::stoull(value);else if(key=="--idle-ms")c.idle_ms=std::stoull(value);else throw std::runtime_error("unknown argument");}
        require(c.threads>0&&c.cycles>1&&c.cache>0,"invalid counts");require(c.measurement=="latency"||c.measurement=="throughput","invalid measurement");
        Result r;if(c.workload=="matrix"||c.workload=="concurrent_clear")r=matrix(c);else if(c.workload=="idle_pool"||c.workload=="live_reclaim")r=idle(c);
        else if(c.workload=="handle_lifetime")r=lifetime(c);else throw std::runtime_error("unknown workload");write(r);return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
