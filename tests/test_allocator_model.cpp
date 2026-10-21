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
// V2 blocks carry no header: a block's extent is derived from its owner as
// round8(requested bytes). The oracle computes this itself.
std::uint64_t extent(std::uint32_t bytes) { return (std::uint64_t(bytes)+7u)&~std::uint64_t(7); }
constexpr std::uint64_t payload_header_bytes=48;
struct Live { void* ptr{}; std::uint32_t bytes{}; unsigned char value{}; std::size_t alignment{}; };
using Holes=std::vector<XBufferCore::WireHole>;
// Full ownership after every operation: header + live blocks + the host-side
// free table must tile [0, used_end) exactly, every live byte is unchanged,
// and the published free accounting matches the enumerated holes.
void intervals(XBufferCore& b, const std::array<Live,64>& live, allocator_model::Coverage& coverage,
               allocator_model::OrderedRanges& ordered, Holes& holes) {
    static thread_local std::vector<allocator_model::Interval> ranges; ranges.clear(); ranges.reserve(2048);
    require(&XBufferCore::owner_from_arena(b.arena())==&b,"owner prefix does not name the current Core");
    require(b.free_table_complete(),"free table lost an entry");
    ranges.push_back({0,payload_header_bytes,0,2});
    for(std::size_t i=0;i<live.size();++i) if(auto p=live[i].ptr) {
        const auto off=std::uint64_t(static_cast<char*>(p)-b.data());
        require(off>=payload_header_bytes && off%8==0,"live block is not 8-byte aligned inside the payload");
        require(reinterpret_cast<std::uintptr_t>(p)%live[i].alignment==0,"actual address misaligned");
        ranges.push_back({off,off+extent(live[i].bytes),i,0});
        auto* bytes=static_cast<unsigned char*>(p);
        for(std::size_t j=0;j<live[i].bytes;++j) require(bytes[j]==live[i].value,"live byte changed");
    }
    holes.clear(); std::uint64_t total=0;
    b.for_each_free_block([&](XBufferCore::WireHole h) {
        require(h.bytes!=0 && h.offset%8==0 && h.bytes%8==0,"free block is not an 8-byte multiple");
        ranges.push_back({h.offset,std::uint64_t(h.offset)+h.bytes,holes.size(),1});
        holes.push_back(h); total+=h.bytes;
    });
    require(holes.size()==b.free_block_count() && total==b.free_block_bytes(),"free table count/bytes mismatch");
    require(total+b.get_size()-b.arena()->used_end==b.get_free_memory(),"Core free accounting mismatch");
    ordered.validate(ranges,b.arena()->used_end);
    allocator_model::require_exact_cover(ranges,b.arena()->used_end);
    // Every previously observed byte stays owned (live, free or header); a
    // lost split residual cannot hide behind a matching broken free-byte count.
    coverage.observe(ranges);
}
void trace(std::uint64_t seed,std::size_t operations,bool automatic) {
    constexpr std::size_t maximum=16*1024*1024;
    XBufferCore b(automatic?4096:maximum,XBufferCore::maximum_capacity(maximum).with_growth(automatic?XGrowPolicy::Automatic:XGrowPolicy::Explicit));
    const auto* base=b.get_address(); std::array<Live,64> live{};
    allocator_model::Coverage coverage;
    allocator_model::OrderedRanges ordered;
    Holes holes; holes.reserve(4096);
#ifdef _WIN32
    SYSTEM_INFO info; GetSystemInfo(&info); const auto page=std::size_t(info.dwPageSize);
#else
    const auto page=std::size_t(sysconf(_SC_PAGESIZE));
#endif
    const std::array<std::size_t,8> aligns={1,2,8,16,64,256,page,page*2};
    const auto initial_seed=seed; std::vector<std::uint64_t> journal; journal.reserve(operations);
    std::size_t current_step=0;
    try {
    intervals(b,live,coverage,ordered,holes);
    for(std::size_t step=0;step<operations;++step) {
        current_step=step;
        auto r=allocator_model::random(seed); journal.push_back(r); auto slot=(r>>8)%live.size(); auto& item=live[slot];
        const auto used_before=b.arena()->used_end;
        try {
            if(!item.ptr) {
                const auto bytes=std::uint32_t(1+(r%4==0?(r>>16)%8192:(r>>16)%256));
                const auto alignment=aligns[(r>>32)%aligns.size()];
                const auto need=extent(bytes);
                // Documented V2 policy, evaluated from the previous free-table
                // snapshot: an 8-aligned request is served by the best-fit free
                // block (exact size first, else the smallest larger one, split
                // at its front) before the tail grows; stricter alignment bumps.
                std::uint32_t best=0;
                if(alignment<=8) for(auto h:holes) if(h.bytes>=need && (!best || h.bytes<best)) best=h.bytes;
                auto* p=b.arena()->allocate_aligned(bytes,alignment);
                const auto off=std::uint64_t(static_cast<char*>(p)-b.data());
                if(best) {
                    bool donor=false; for(auto h:holes) donor|=(h.offset==off && h.bytes==best);
                    require(donor,"allocation did not take the best-fit free block");
                    require(b.arena()->used_end==used_before,"free-table reuse moved used_end");
                } else {
                    // The ADDRESS is aligned (the payload base is only page aligned).
                    const auto step_align=std::max<std::uint64_t>(alignment,8);
                    const auto base=std::uint64_t(reinterpret_cast<std::uintptr_t>(b.data()));
                    const auto start=((base+used_before+step_align-1)&~(step_align-1))-base;
                    require(off==start && b.arena()->used_end==start+need,"tail allocation not placed at used_end");
                }
                item={p,bytes,static_cast<unsigned char>((slot+step)%251+1),alignment}; std::memset(p,item.value,bytes);
            } else if((r%5)<3) {
                b.arena()->deallocate(item.ptr,item.bytes);
                require(b.arena()->used_end==used_before,"retire returned bytes to used_end");
                item={};
            } else {
                auto new_size=item.bytes+std::uint32_t(1+(r>>24)%2048);
                const auto off=std::uint64_t(static_cast<char*>(item.ptr)-b.data());
                const bool tail=off+extent(item.bytes)==used_before;
                const bool fits=off+extent(new_size)<=b.get_size();
                const bool expanded=b.arena()->try_expand(item.ptr,item.bytes,new_size);
                // V2: only the tail block grows in place; there is no header slack.
                if(!tail) require(!expanded,"non-tail block expanded in place");
                else if(fits) require(expanded,"tail block within capacity failed to expand");
                if(expanded) {
                    require(b.arena()->used_end==std::max(used_before,off+extent(new_size)),"expand did not move used_end to the new extent");
                    std::memset(static_cast<char*>(item.ptr)+item.bytes,item.value,new_size-item.bytes); item.bytes=new_size;
                } else require(b.arena()->used_end==used_before,"failed expand changed used_end");
            }
        } catch(const XBadAlloc&) {
            require(b.get_size()<=maximum,"hard limit escaped");
            require(b.arena()->used_end==used_before,"failed allocation changed used_end");
        }
        require(b.get_address()==base,"owner moved");
        // Full ownership and every live byte after each operation, not just at the end.
        intervals(b,live,coverage,ordered,holes);
        if(step%997==996) { XBufferCore moved(std::move(b)); b=std::move(moved); intervals(b,live,coverage,ordered,holes); }
        if(step%4999==4998) { b.shrink_to_fit(); require(b.grow(maximum-b.get_size()),"regrow failed"); intervals(b,live,coverage,ordered,holes); }
    }
    current_step=operations;
    for(auto& item:live) if(item.ptr) { b.arena()->deallocate(item.ptr,item.bytes); item={}; }
    intervals(b,live,coverage,ordered,holes);
    } catch(const std::exception&) {
        std::cerr<<"FAIL seed="<<initial_seed<<" step="<<current_step<<" automatic="<<automatic<<" replay=xorshift64 slot=(r>>8)%64\n";
        for(std::size_t j=0;j<journal.size();++j)std::cerr<<j<<':'<<journal[j]<<'\n';
        throw;
    }
}
Holes holes_of(const XBufferCore& b) { Holes out; b.for_each_free_block([&](XBufferCore::WireHole h){out.push_back(h);}); return out; }
bool same(const Holes& a,const Holes& c) {
    if(a.size()!=c.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(a[i].offset!=c[i].offset||a[i].bytes!=c[i].bytes) return false;
    return true;
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
            // V2 persists free space as the wire hole table: the loaded free
            // table equals the saved one, and write-read-write is byte stable.
            const auto saved_holes=holes_of(b);
            if(cycle>0) require(!saved_holes.empty(),"retired blocks missing from the free table");
            require(same(holes_of(loaded),saved_holes) && loaded.free_block_bytes()==b.free_block_bytes(),"hole table not reloaded");
            require(loaded.save_verified<AllocatorModelRoot>()==frame,"write-read-write is not byte stable");
            require(loaded.grow(65536-loaded.get_size()),"loaded grow failed");
            require(loaded.root<AllocatorModelRoot>().marker==713,"root marker changed");
            require(loaded.root<AllocatorModelRoot>().label==std::string(37+cycle,'a'+cycle%20),"wire string changed");
            for(unsigned j=0;j<r.values.size();++j) require(loaded.root<AllocatorModelRoot>().values[j]==j*19+cycle,"wire value changed");
            // Reacquire all bindings after the transfer.
            b=std::move(loaded); auto& again=b.root<AllocatorModelRoot>(); again.values.push_back(991);
            require(again.values.back()==991,"post-load mutation failed");
            // Free space saved before the reload is reusable after it (V1 left
            // pre-save Core free blocks as permanent dead space).
            if(const auto now=holes_of(b); !now.empty()) {
                const auto used=b.arena()->used_end; const auto size=now.front().bytes;
                auto* p=b.arena()->allocate_aligned(size,8); bool reused=false;
                for(auto h:now) reused|=(h.bytes==size && p==b.data()+h.offset);
                require(reused && b.arena()->used_end==used,"reloaded hole was not reused");
                b.arena()->deallocate(p,size);
            }
        }
    }
}
}
int main(int argc,char** argv) {
    try {
        allocator_model::negative_controls();
        wire_lifecycle();
        std::size_t ops=1000,seeds=2;
        if(argc>1) {ops=std::stoull(argv[1]);seeds=argc>2?std::stoull(argv[2]):8;}
        for(std::size_t i=0;i<seeds;++i) {
            const auto seed=i<8?allocator_model::seeds[i]:allocator_model::seeds[i%8]+(i+1)*0x9e3779b97f4a7c15ull;
            std::cerr<<"seed="<<seed<<" ops="<<ops<<"\n"; trace(seed,ops,i%2);
        }
        std::cout<<"{\"status\":\"passed\",\"seeds\":"<<seeds<<",\"operations\":"<<ops*seeds<<",\"negative_controls\":"<<allocator_model::negative_control_count<<"}\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
