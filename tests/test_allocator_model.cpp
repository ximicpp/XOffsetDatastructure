// Independent physical interval oracle; the implementation's search is never the oracle.
#include "allocator_model_support.hpp"
#include <iostream>
#include <string>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;
using allocator_model::require;
struct AllocatorModelRoot { std::uint64_t marker{}; XVector<std::uint32_t> values; XString label; };
XOFFSET_REGISTER_SCHEMA_NAME(AllocatorModelRoot,"test.AllocatorModelRoot")
namespace {
using H=XBufferCore::AllocationHeader;
H header(const void* p) { H h; std::memcpy(&h,static_cast<const char*>(p)-16,16); return h; }
struct Live { void* ptr{}; std::uint32_t bytes{}; unsigned char value{}; std::size_t alignment{}; };
void intervals(XBufferCore& b, const std::array<Live,64>& live, allocator_model::Coverage& coverage,
               allocator_model::OrderedRanges& ordered) {
    static thread_local std::vector<allocator_model::Interval> ranges; ranges.clear(); ranges.reserve(2048);
    for(std::size_t i=0;i<live.size();++i) if(auto p=live[i].ptr) {
        auto h=header(p); auto off=static_cast<char*>(p)-b.data();
        require(h.user_offset==off && h.total_bytes==16ull+h.user_bytes && h.user_bytes>=live[i].bytes,"bad live header");
        require(reinterpret_cast<std::uintptr_t>(p)%live[i].alignment==0,"actual address misaligned");
        require(h.alignment && !(h.alignment&(h.alignment-1)) && reinterpret_cast<std::uintptr_t>(p)%h.alignment==0,"bad stored alignment");
        ranges.push_back({h.user_offset-16ull,h.user_offset+std::uint64_t(h.user_bytes),i,0});
        auto* bytes=static_cast<unsigned char*>(p);
        for(std::size_t j=0;j<live[i].bytes;++j) require(bytes[j]==live[i].value,"live byte changed");
    }
#ifdef S3_PROBE
    require(b.s3_validate_index(),"invalid Core slot/index reachability");
    std::uint64_t total=0, count=0;
    b.s3_visit_free([&](auto off,auto size,auto alignment) {
        auto h=header(b.data()+off);
        require(h.user_offset==off && h.user_bytes==size && h.total_bytes==16ull+size && h.alignment==alignment,"bad Core free/index reference");
        ranges.push_back({off-16ull,off+std::uint64_t(size),count++,1}); total+=size+16;
    });
    require(total+b.get_size()-b.arena()->used_end==b.get_free_memory(),"Core free accounting mismatch");
#endif
    ordered.validate(ranges,b.arena()->used_end);
#ifdef S3_PROBE
    // Core live/free ownership must conserve every previously observed byte;
    // a missing residual cannot hide behind a matching broken free-byte count.
    coverage.observe(ranges);
#else
    // Public smoke lacks visibility into Core free records. Retain live-only
    // safety checks there; conservation runs in the diagnostic candidate model.
    (void)coverage;
#endif
}
void trace(std::uint64_t seed,std::size_t operations,bool automatic) {
    constexpr std::size_t maximum=16*1024*1024;
    XBufferCore b(automatic?4096:maximum,XBufferCore::maximum_capacity(maximum).with_growth(automatic?XGrowPolicy::Automatic:XGrowPolicy::Explicit));
    const auto* base=b.get_address(); std::array<Live,64> live{};
    allocator_model::Coverage coverage;
    allocator_model::OrderedRanges ordered;
    struct Donor {std::uint32_t off,size,align;}; std::vector<Donor> fifo; fifo.reserve(operations);
    #ifdef _WIN32
    SYSTEM_INFO info; GetSystemInfo(&info); const auto page=std::size_t(info.dwPageSize);
#else
    const auto page=std::size_t(sysconf(_SC_PAGESIZE));
#endif
    const std::array<std::size_t,8> aligns={1,2,8,16,64,256,page,page*2};
    const auto initial_seed=seed; std::vector<std::uint64_t> journal; journal.reserve(operations);
    std::size_t current_step=0;
    try {
    for(std::size_t step=0;step<operations;++step) {
        current_step=step;
        auto r=allocator_model::random(seed); journal.push_back(r); auto slot=(r>>8)%live.size(); auto& item=live[slot];
        try {
            if(!item.ptr) {
                const auto bytes=std::uint32_t(1+(r%4==0?(r>>16)%8192:(r>>16)%256));
                const auto alignment=aligns[(r>>32)%aligns.size()];
                std::uint32_t expected=0;
#ifdef S3_WHOLE
                for(auto d:fifo) if(d.size>=bytes && d.align>=std::max<std::size_t>(alignment,alignof(std::max_align_t)) && (reinterpret_cast<std::uintptr_t>(b.data()+d.off)%alignment)==0) {expected=d.off;break;}
#endif
                auto* p=b.arena()->allocate_aligned(bytes,alignment); auto h=header(p);
#ifdef S3_WHOLE
                if(expected) require(h.user_offset==expected,"FIFO first-fit donor differs");
                for(auto it=fifo.begin();it!=fifo.end();++it) if(it->off==h.user_offset) {fifo.erase(it);break;}
#endif
                item={p,bytes,static_cast<unsigned char>((slot+step)%251+1),alignment}; std::memset(p,item.value,bytes);
            } else if((r%5)<3) {
                auto h=header(item.ptr); b.arena()->deallocate(item.ptr);
#ifdef S3_WHOLE
                fifo.push_back({h.user_offset,h.user_bytes,h.alignment});
#endif
                item={};
            } else {
                auto new_size=item.bytes+std::uint32_t(1+(r>>24)%2048);
                if(b.arena()->try_expand(item.ptr,new_size,item.alignment)) {
                    require(header(item.ptr).user_bytes>=new_size,"expand too short");
                    std::memset(static_cast<char*>(item.ptr)+item.bytes,item.value,new_size-item.bytes); item.bytes=new_size;
                }
            }
        } catch(const XBadAlloc&) { require(b.get_size()<=maximum,"hard limit escaped"); }
        require(b.get_address()==base,"owner moved");
        // Full ownership and every live byte after each operation, not just at the end.
        intervals(b,live,coverage,ordered);
        if(step%997==996) { XBufferCore moved(std::move(b)); b=std::move(moved); intervals(b,live,coverage,ordered); }
        if(step%4999==4998) { b.shrink_to_fit(); require(b.grow(maximum-b.get_size()),"regrow failed"); intervals(b,live,coverage,ordered); }
    }
    current_step=operations;
    for(auto& item:live) if(item.ptr) { b.arena()->deallocate(item.ptr); item={}; }
    intervals(b,live,coverage,ordered);
    } catch(const std::exception&) {
        std::cerr<<"FAIL seed="<<initial_seed<<" step="<<current_step<<" automatic="<<automatic<<" replay=xorshift64 slot=(r>>8)%64\n";
        for(std::size_t j=0;j<journal.size();++j)std::cerr<<j<<':'<<journal[j]<<'\n';
        throw;
    }
}
void wire_lifecycle() {
    for(auto policy:{XGrowPolicy::Explicit,XGrowPolicy::Automatic}) {
        auto b=XBuffer::create<AllocatorModelRoot>(65536,XBuffer::maximum_capacity(1024*1024).with_growth(policy));
        b.root<AllocatorModelRoot>().marker=713;
        for(unsigned cycle=0;cycle<16;++cycle) {
            auto& r=b.root<AllocatorModelRoot>();
            r.values.clear(); r.values.reserve(100+cycle*17); r.label=std::string(37+cycle,'a'+cycle%20);
            for(unsigned j=0;j<100+cycle*17;++j) r.values.push_back(j*19+cycle);
            auto frame=b.save_verified<AllocatorModelRoot>(); auto loaded=XBuffer::load_verified<AllocatorModelRoot>(frame,XBuffer::maximum_capacity(1024*1024).with_growth(policy));
            require(loaded.grow(65536-loaded.get_size()),"loaded grow failed");
            require(loaded.root<AllocatorModelRoot>().marker==713,"root marker changed");
            require(loaded.root<AllocatorModelRoot>().label==std::string(37+cycle,'a'+cycle%20),"wire string changed");
            for(unsigned j=0;j<r.values.size();++j) require(loaded.root<AllocatorModelRoot>().values[j]==j*19+cycle,"wire value changed");
            // Raw Core free tables are intentionally not persisted. Reacquire all bindings.
            b=std::move(loaded); auto& again=b.root<AllocatorModelRoot>(); again.values.push_back(991);
            require(again.values.back()==991,"post-load mutation failed");
            fixed_detail::release_cached_blocks(again.values.allocator_state());

        }
    }
}
}
int main(int argc,char** argv) {
    try {
        allocator_model::negative_controls();
        std::size_t ops=1000,seeds=2;
        if(argc>1) {ops=std::stoull(argv[1]);seeds=argc>2?std::stoull(argv[2]):8;}
        for(std::size_t i=0;i<seeds;++i) {
            const auto seed=i<8?allocator_model::seeds[i]:allocator_model::seeds[i%8]+(i+1)*0x9e3779b97f4a7c15ull;
            std::cerr<<"seed="<<seed<<" ops="<<ops<<"\n"; trace(seed,ops,i%2);
        }
        wire_lifecycle(); std::cout<<"{\"status\":\"passed\",\"seeds\":"<<seeds<<",\"operations\":"<<ops*seeds<<",\"negative_controls\":"<<allocator_model::negative_control_count<<"}\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
