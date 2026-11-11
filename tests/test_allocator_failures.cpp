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
constexpr std::uint64_t payload_header_bytes=48;
constexpr std::uint64_t root_extent=(sizeof(AllocatorFailureRoot)+7u)&~std::uint64_t(7);
Holes holes_of(const XBufferCore& b) { Holes out; b.for_each_free_block([&](XBufferCore::WireHole h){out.push_back(h);}); return out; }
bool same(const Holes& a,const Holes& c) {
    if(a.size()!=c.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(a[i].offset!=c[i].offset||a[i].bytes!=c[i].bytes) return false;
    return true;
}
struct Outcome {std::size_t attempts,hits,free;};
// V2 retirement pushes into the host-side free table. A host allocation
// failure there drops that one entry, records the table as incomplete, and
// save_verified recomputes the hole table from the live graph. Released free
// tables are cached per thread: a cold run (cache trimmed) makes the first
// retire allocate a fresh table, a warm run reuses a cached one; both are
// exercised at every allocation point.
Outcome exercise(long n,bool cold) {
    if(cold) XBufferCore::trim_free_table_cache();
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    auto* guard=b.arena()->allocate_aligned(64,8); std::memset(guard,0x5d,64);
    std::array<void*,128> blocks{};
    for(auto& p:blocks) p=b.arena()->allocate_aligned(256,8);
    attempts=hits=0;fail_after=n;watch=true;
    for(auto p:blocks)b.arena()->deallocate(p,256);
    watch=false;fail_after=-1; auto result=Outcome{attempts,hits,b.get_free_memory()};
    require(b.free_table_complete()==(hits==0),"host OOM was not recorded as an incomplete free table");
    for(unsigned j=0;j<64;++j)require(static_cast<unsigned char*>(guard)[j]==0x5d,"host OOM changed live data");
    std::array<void*,128> reused{}; std::vector<allocator_model::Interval> ranges;ranges.reserve(512);
    for(auto& p:reused) { p=b.arena()->allocate_aligned(128,8); auto off=std::uint64_t(static_cast<char*>(p)-b.data());
        ranges.push_back({off,off+128,0,0}); }
    auto off=std::uint64_t(static_cast<char*>(guard)-b.data());ranges.push_back({off,off+64,0,0});
    ranges.push_back({0,payload_header_bytes,0,2});
    ranges.push_back({b.arena()->root_offset,b.arena()->root_offset+root_extent,0,0});
    // A surviving free entry must never name live bytes or duplicate another.
    b.for_each_free_block([&](XBufferCore::WireHole h) { ranges.push_back({h.offset,std::uint64_t(h.offset)+h.bytes,0,1}); });
    allocator_model::validate(ranges,b.arena()->used_end);
    if(b.free_table_complete()) allocator_model::require_exact_cover(ranges,b.arena()->used_end);
    // V2 exact coverage: unreachable raw blocks must be released before a verified save.
    for(auto p:reused) b.arena()->deallocate(p,128);
    b.arena()->deallocate(guard,64);
    auto frame=b.save_verified<AllocatorFailureRoot>();
    require(b.free_table_complete(),"save_verified did not recompute the lost hole table");
    auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame);
    require(loaded.root<AllocatorFailureRoot>().values.empty(),"post-failure wire corruption");
    require(loaded.free_block_bytes()+payload_header_bytes+root_extent==loaded.arena()->used_end,
            "post-failure hole table does not cover every unreachable byte");
    return result;
}
// D2: an in-place reload keeps the owner's free table, emptied, with its pool
// storage (<= 16 KiB, 4096 words), so the first retires after the reload make
// no host allocation, with or without per-thread caches; a pool that grew past
// that returns its storage, and so does the pool of a table released by
// destroying its buffer (that table goes to the per-thread cache). The hole
// table is the same as with a freshly allocated pool.
void reload_keeps_small_bin_storage() {
    XBufferCore::trim_free_table_cache();
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024).with_growth(XGrowPolicy::Automatic));
    b.root<AllocatorFailureRoot>().values.push_back(77);
    const auto frame=b.save_verified<AllocatorFailureRoot>();
    auto round=[&](std::size_t& small_attempts,std::size_t& many_attempts) {
        b.reload_verified<AllocatorFailureRoot>(frame);
        require(b.free_block_count()==0,"fixture wire has holes");
        std::array<void*,8> small{}; std::vector<void*> many(2100);
        for(auto& p:small) p=b.arena()->allocate_aligned(64,8);
        for(auto& p:many) p=b.arena()->allocate_aligned(32,8);
        attempts=hits=0;fail_after=-1;watch=true;
        for(auto p:small) b.arena()->deallocate(p,64);
        small_attempts=attempts; attempts=0;
        for(auto p:many) b.arena()->deallocate(p,32);
        watch=false; many_attempts=attempts;
        require(b.free_table_complete(),"retires lost an entry");
        return holes_of(b);
    };
    std::size_t s0=0,m0=0,s1=0,m1=0,s2=0,m2=0;
    const auto first=round(s0,m0);
    const auto second=round(s1,m1);
    const auto third=round(s2,m2);
    require(s0>=1 && m0>=1,"first round: table and pool storage were not host allocations");
    // The reload keeps the owner's table (no thread cache involved).
    require(s1==0 && s2==0,"reload did not keep its table");
    require(m1>=1 && m2>=1,"pool storage above the keep limit was kept");
    require(same(first,second) && same(first,third),"kept pool storage changed the hole table");
    // A table holds its first free_table_inline_entries holes of one size in
    // its inline pool; the next one needs pool storage, which a table
    // released by destroying its buffer must not have kept.
    b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::array<void*,XBufferCore::free_table_inline_entries+1> blocks{};
    for(auto& q:blocks) q=b.arena()->allocate_aligned(64,8);
    attempts=hits=0;fail_after=-1;watch=true;
    for(auto q:blocks) b.arena()->deallocate(q,64);
    watch=false;
#if XOFFSET_THREAD_CACHES
    require(attempts==1,"a table released by destroying its buffer kept pool storage");
#else
    require(attempts==2,"without thread caches the retires allocate their table and pool");
#endif
    XBufferCore::trim_free_table_cache();
}
// V2 replacement for the V1 release_cached_blocks transfer (in-arena free lists
// moved into the Core table; both are gone). Free metadata now enters the host
// table from the wire hole table inside load_verified. A real host failure at
// any allocation point must surface as an exception or as an explicitly
// incomplete table, never as a silently truncated one; the source is unaffected.
std::size_t hole_table_load() {
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::array<void*,96> blocks{};
    const auto size=[](std::size_t i){return std::uint32_t(i%3==0?5000:256);}; // small bins and the large table
    for(std::size_t i=0;i<blocks.size();++i) blocks[i]=b.arena()->allocate_aligned(size(i),8);
    b.root<AllocatorFailureRoot>().values.push_back(77);
    for(std::size_t i=0;i<blocks.size();++i) b.arena()->deallocate(blocks[i],size(i));
    auto frame=b.save_verified<AllocatorFailureRoot>(); const auto expected=holes_of(b);
    require(expected.size()==blocks.size(),"fixture hole table");
    XBufferCore::trim_free_table_cache(); // the load allocates its table afresh
    attempts=hits=0;fail_after=-1;watch=true;
    auto normal=XBuffer::load_verified<AllocatorFailureRoot>(frame);
    watch=false; const auto total=attempts;
    require(total>0,"no real load metadata allocations observed");
    require(same(holes_of(normal),expected) && normal.free_table_complete(),"normal load changed the hole table");
    for(std::size_t n=0;n<total;++n) {
        XBufferCore::trim_free_table_cache();
        attempts=hits=0;fail_after=long(n);watch=true;
        try {
            auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame);
            watch=false;fail_after=-1;
            require(!loaded.free_table_complete() || same(holes_of(loaded),expected),"load silently truncated the free table");
            require(loaded.root<AllocatorFailureRoot>().values[0]==77,"absorbed load failure changed the root");
            auto again=XBuffer::load_verified<AllocatorFailureRoot>(loaded.save_verified<AllocatorFailureRoot>());
            require(again.root<AllocatorFailureRoot>().values[0]==77,"absorbed load failure produced invalid wire");
        } catch(const std::bad_alloc&) {
        } catch(const XBadAlloc&) {
        }
        watch=false;fail_after=-1;
        require(hits==1,"requested load fail point not hit");
        require(b.root<AllocatorFailureRoot>().values[0]==77 && same(holes_of(b),expected),"failed load disturbed the source");
    }
    auto retry=XBuffer::load_verified<AllocatorFailureRoot>(frame);
    require(same(holes_of(retry),expected),"retry after load failures changed the hole table");
    return total;
}
void lazy_load() {
    auto b=XBuffer::create<AllocatorFailureRoot>(65536);
    b.root<AllocatorFailureRoot>().values.push_back(77);auto frame=b.save_verified<AllocatorFailureRoot>();
    auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame,XBuffer::maximum_capacity(65536));
    require(loaded.free_block_count()==0,"fixture unexpectedly has holes");
    loaded.grow(4096);auto* p=loaded.arena()->allocate_aligned(64,8);
    XBufferCore::trim_free_table_cache(); // the first retire allocates the table
    attempts=hits=0;fail_after=0;watch=true;loaded.arena()->deallocate(p,64);watch=false;fail_after=-1;
    require(hits==1,"post-load lazy metadata failure not reached");
    require(!loaded.free_table_complete(),"lost free entry was not recorded");
    require(loaded.root<AllocatorFailureRoot>().values[0]==77,"post-load OOM changed root");
    // The lost block is recovered by the save-time recomputation.
    auto again=XBuffer::load_verified<AllocatorFailureRoot>(loaded.save_verified<AllocatorFailureRoot>());
    require(again.root<AllocatorFailureRoot>().values[0]==77 && again.free_block_bytes()>=64,"lost block not recovered as a hole");
}
// G: the failed no-growth attempt inside allocate_dynamic_bytes is a probe,
// not a thrown XBadAlloc: no diagnostic exception string is allocated before
// growth, whose order and errors are kept.
auto growth_policy(bool automatic) {
    auto cap=XBuffer::maximum_capacity(1024*1024);
    return automatic ? cap.with_growth(XGrowPolicy::Automatic) : cap;
}
void growth_probe_without_host_allocation() {
    for(long point:{-1L,0L}) {
        auto b=XBuffer::create<AllocatorFailureRoot>(4096,growth_policy(true));
        require(b.free_block_count()==0,"fixture unexpectedly has cached blocks");
        auto ref=fixed_detail::allocator_ref_of(b.arena()); const auto size=b.get_size();
        attempts=hits=0;fail_after=point;watch=true;
        void* p=nullptr;
        try { p=fixed_detail::allocate_dynamic_bytes(ref,static_cast<std::uint32_t>(size),8); }
        catch(...) { watch=false;fail_after=-1;throw; }
        watch=false;fail_after=-1;
        require(p && b.get_size()>size,"growth allocation did not grow");
        require(attempts==0 && hits==0,"growth signalling attempted a host allocation");
        std::memset(p,0x47,size);
        // V2 exact coverage: the unreachable raw block is released before saving.
        fixed_detail::retire_dynamic_bytes(ref,p,static_cast<std::uint32_t>(size));
        auto frame=b.save_verified<AllocatorFailureRoot>(); (void)XBuffer::load_verified<AllocatorFailureRoot>(frame);
    }
}
// D3: an exact-bin hit, an unpadded bump inside the extent and a retire into
// a bin with spare capacity make no host allocation (a failure armed at the
// first one is never reached). A retire into a full bin is the general path:
// its growth is a real host allocation whose failure is recorded as an
// incomplete table, exactly as before.
void inline_paths_without_host_allocation() {
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::array<void*,16> blocks{};
    for(auto& p:blocks) p=b.arena()->allocate_aligned(64,8);
    for(auto p:blocks) b.arena()->deallocate(p,64); // table + a 16-entry segment in its inline pool
    require(b.free_table_complete() && b.free_block_count()==16,"fixture free table");
    attempts=hits=0;fail_after=0;watch=true;
    for(auto& p:blocks) p=b.arena()->allocate_aligned(64,8);   // exact-bin hits
    for(auto p:blocks) b.arena()->deallocate(p,64);            // bin has capacity 16
    for(auto& p:blocks) p=b.arena()->allocate_aligned(64,8);
    std::array<void*,8> tail{};
    for(auto& p:tail) p=b.arena()->allocate_aligned(72,8);     // bin 72 empty: bump
    watch=false;fail_after=-1;
    require(attempts==0 && hits==0,"an inline allocator path attempted a host allocation");
    require(b.free_table_complete() && b.free_block_count()==0,"inline paths changed the free table");
    for(auto p:tail) b.arena()->deallocate(p,72);              // bin 72 gets an 8-entry segment
    for(std::size_t i=0;i<8;++i) b.arena()->deallocate(blocks[i],64);
    attempts=hits=0;fail_after=0;watch=true;
    b.arena()->deallocate(blocks[8],64);                       // 64 bin: capacity 16, holds 8
    require(attempts==0,"retire into spare capacity attempted a host allocation");
    for(std::size_t i=9;i<16;++i) b.arena()->deallocate(blocks[i],64);
    // The inline pool (32 words) has room for a 2-entry segment of bin 80;
    // the third hole needs a larger pool, which only the general path
    // allocates (the failure loses that hole; later ones get the pool).
    std::array<void*,XBufferCore::free_table_inline_entries+1> extra{};
    for(auto& p:extra) p=b.arena()->allocate_aligned(80,8);
    for(auto p:extra) b.arena()->deallocate(p,80);
    watch=false;fail_after=-1;
    require(hits==1 && !b.free_table_complete(),"general-path bin growth failure was not recorded");
    require(b.free_block_count()==8+16+XBufferCore::free_table_inline_entries,"inline retires lost an entry");
    auto again=XBuffer::load_verified<AllocatorFailureRoot>(b.save_verified<AllocatorFailureRoot>());
    require(again.free_block_bytes()>=8*72+16*64+80*extra.size(),"lost block not recovered as a hole");
}
// V1 released cached in-arena blocks exactly once before growing. V2 keeps
// free blocks in the host table: an allocation no free block can serve grows
// the tail (or fails under Explicit) without consuming or losing them, and the
// cached block is still handed out afterwards (best fit, split at its front).
void growth_probe_keeps_cached_blocks() {
    for(bool automatic:{true,false}) {
        auto b=XBuffer::create<AllocatorFailureRoot>(4096,growth_policy(automatic));
        auto ref=fixed_detail::allocator_ref_of(b.arena());
        auto* small=fixed_detail::allocate_dynamic_bytes(ref,256,8); fixed_detail::retire_dynamic_bytes(ref,small,256);
        require(b.free_block_bytes()==256 && b.free_block_count()==1,"fixture cached block missing");
        const auto size=b.get_size(); const auto used=b.arena()->used_end;
        bool bad=false; void* p=nullptr;
        try { p=fixed_detail::allocate_dynamic_bytes(ref,static_cast<std::uint32_t>(size),8); }
        catch(const XBadAlloc&) { bad=true; }
        require(b.free_block_bytes()==256 && b.free_block_count()==1 && b.free_table_complete(),"growth disturbed the cached free block");
        if(automatic) require(!bad && p && b.get_size()>size && static_cast<char*>(p)==b.data()+used,"automatic growth failed");
        else require(bad && !p && b.get_size()==size && b.arena()->used_end==used,"explicit policy must still end with XBadAlloc");
        require(b.arena()->allocate_aligned(200,8)==small,"cached block was not reused");
        require(b.free_block_bytes()==56,"split residual not kept free");
    }
}
}
// A table keeps its first free_table_inline_entries holes of one size in its
// inline pool (no host storage); the next hole of that size moves the pool to
// host storage. Both forms serve the same LIFO order, emit the same hole table
// and clone it.
void inline_bin_transition() {
    constexpr std::size_t inline_entries=XBufferCore::free_table_inline_entries;
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::array<void*,inline_entries+2> blocks{};
    for(auto& p:blocks) p=b.arena()->allocate_aligned(40,8);
    auto* other=b.arena()->allocate_aligned(48,8); b.arena()->deallocate(other,48); // the table exists
    attempts=hits=0;fail_after=-1;watch=true;
    for(std::size_t i=0;i<inline_entries;++i) b.arena()->deallocate(blocks[i],40);
    require(attempts==0,"an inline bin entry made a host allocation");
    b.arena()->deallocate(blocks[inline_entries],40);
    require(attempts==1,"the hole past the inline entries did not allocate bin storage once");
    b.arena()->deallocate(blocks[inline_entries+1],40);
    watch=false;
    require(b.free_table_complete() && b.free_block_count()==blocks.size()+1,"bin transition lost an entry");
    const auto before=holes_of(b);
    auto copy=b.clone();
    require(same(holes_of(copy),before),"clone of a host bin changed the hole table");
    auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(b.save_verified<AllocatorFailureRoot>());
    require(same(holes_of(loaded),before),"reload of a host bin changed the hole table");
    for(std::size_t i=blocks.size();i-->0;)
        require(b.arena()->allocate_aligned(40,8)==blocks[i],"bin did not hand out its newest hole first");
    require(b.free_block_count()==1,"exact takes left a 40-byte hole");
}
// A table whose holes were all taken again keeps its bins' (empty) storage;
// loading holes into it starts its pool over. The loaded table is exactly the
// holes given, in emission order, and serves them newest first.
void load_into_emptied_table() {
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::array<void*,6> big{}; std::array<void*,3> small{};
    for(auto& p:big) p=b.arena()->allocate_aligned(64,8);
    for(auto& p:small) p=b.arena()->allocate_aligned(40,8);
    for(auto p:big) b.arena()->deallocate(p,64);
    for(auto p:small) b.arena()->deallocate(p,40);
    const auto holes=holes_of(b);
    for(std::size_t i=0;i<big.size();++i) (void)b.arena()->allocate_aligned(64,8);
    for(std::size_t i=0;i<small.size();++i) (void)b.arena()->allocate_aligned(40,8);
    require(b.free_block_count()==0,"fixture: every hole taken again");
    b.replace_free_blocks(holes);
    require(same(holes_of(b),holes) && b.free_block_count()==holes.size(),"load into an emptied table changed the holes");
    for(std::size_t i=big.size();i-->0;) require(b.arena()->allocate_aligned(64,8)==big[i],"64-byte bin order after load");
    for(std::size_t i=small.size();i-->0;) require(b.arena()->allocate_aligned(40,8)==small[i],"40-byte bin order after load");
    require(b.free_block_count()==0,"load into an emptied table left extra holes");
}
// Holes of one size need not arrive in one run: an incremental delta's hole
// list is in offset order, so a size's holes come in many short runs. Each
// run after the first adds to the bin's segment; a segment with room must not
// grow (it doubled per run, and 70 holes asked for 2^30 entries).
void load_interleaved_runs() {
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    std::vector<void*> big, small;
    for(int i=0;i<300;++i) { big.push_back(b.arena()->allocate_aligned(64,8)); small.push_back(b.arena()->allocate_aligned(40,8)); }
    for(auto p:big) b.arena()->deallocate(p,64);
    for(auto p:small) b.arena()->deallocate(p,40);
    auto holes=holes_of(b);
    std::sort(holes.begin(),holes.end(),[](const auto& x,const auto& y){return x.offset<y.offset;});
    for(std::size_t i=0;i<big.size();++i) (void)b.arena()->allocate_aligned(64,8);
    for(std::size_t i=0;i<small.size();++i) (void)b.arena()->allocate_aligned(40,8);
    require(b.free_block_count()==0,"fixture: every hole taken again");
    b.replace_free_blocks(holes);
    require(b.free_block_count()==holes.size(),"interleaved load lost holes");
    // Within a bin the holes keep their load order: the last loaded is taken first.
    for(std::size_t i=holes.size();i-->0;) {
        const auto h=holes[i];
        if(h.bytes!=64) continue;
        require(static_cast<char*>(b.arena()->allocate_aligned(64,8))==b.data()+h.offset,"64-byte bin order after an interleaved load");
    }
    for(std::size_t i=holes.size();i-->0;) {
        const auto h=holes[i];
        if(h.bytes!=40) continue;
        require(static_cast<char*>(b.arena()->allocate_aligned(40,8))==b.data()+h.offset,"40-byte bin order after an interleaved load");
    }
    require(b.free_block_count()==0,"interleaved load left extra holes");
}
// More owners reload than the thread cache holds tables (4): each keeps its
// own emptied table, so the first retire after its reload allocates nothing.
void reloads_keep_their_tables() {
    XBufferCore::trim_free_table_cache();
    std::vector<XBuffer> owners;
    std::vector<std::string> frames;
    for(int i=0;i<8;++i) {
        owners.push_back(XBuffer::create<AllocatorFailureRoot>(65536,XBuffer::maximum_capacity(65536).with_growth(XGrowPolicy::Automatic)));
        auto& b=owners.back();
        b.root<AllocatorFailureRoot>().values.push_back(std::uint32_t(i));
        frames.push_back(b.save_verified<AllocatorFailureRoot>());
        b.arena()->deallocate(b.arena()->allocate_aligned(64,8),64); // b now has a table
        require(b.free_block_count()==1,"fixture retire");
    }
    for(int i=0;i<8;++i) {
        owners[std::size_t(i)].reload_verified<AllocatorFailureRoot>(frames[std::size_t(i)]);
        require(owners[std::size_t(i)].free_block_count()==0,"reload kept a hole of the old content");
    }
    XBufferCore::trim_free_table_cache();
    for(int i=0;i<8;++i) {
        auto& b=owners[std::size_t(i)];
        auto* p=b.arena()->allocate_aligned(64,8);
        attempts=hits=0;fail_after=-1;watch=true;
        b.arena()->deallocate(p,64);
        watch=false;
        require(attempts==0,"the first retire after a reload allocated a table");
        require(b.free_table_complete() && b.free_block_count()==1,"retire after reload lost its entry");
        require(b.root<AllocatorFailureRoot>().values[0]==std::uint32_t(i),"reload changed the root");
    }
}
int main() {try {
    std::size_t points=0,max_lost=0;
    reload_keeps_small_bin_storage();
    reloads_keep_their_tables();
    for(bool cold:{true,false}) {
        auto normal=exercise(-1,cold); require(normal.attempts>0,"no real metadata allocations observed");
        if(!cold) (void)exercise(-1,false); // the previous run's table is now cached
        for(std::size_t n=0;n<normal.attempts;++n) {auto r=exercise(long(n),cold);require(r.hits==1,"requested fail point not hit");
            require(r.free<=normal.free,"failure invented free bytes");max_lost=std::max(max_lost,normal.free-r.free);++points;}
    }
    points+=hole_table_load();
    lazy_load(); inline_paths_without_host_allocation(); inline_bin_transition(); load_into_emptied_table(); load_interleaved_runs(); growth_probe_without_host_allocation(); growth_probe_keeps_cached_blocks(); std::cout<<"{\"status\":\"passed\",\"actual_failure_points\":"<<points+1<<",\"max_unreclaimed_bytes\":"<<max_lost<<"}\n";
} catch(const std::exception& e) {watch=false;std::cerr<<e.what()<<'\n';return 1;} }
