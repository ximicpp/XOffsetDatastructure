#define XOFFSET_FREELIST_INDEX_DIAGNOSTICS 1
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;
namespace {
void need(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::uint32_t rounded(std::uint32_t n, std::size_t alignment = 1) {
    const auto a = static_cast<std::uint32_t>(std::max<std::size_t>(alignment, 8));
    return (n + a - 1) & ~(a - 1);
}
std::uint32_t klass(std::uint32_t n) {
    std::uint32_t cap = 8, k = 0;
    while (cap < n && k + 1 < 16) { cap *= 2; ++k; }
    need(cap >= n, "oracle class overflow");
    return k;
}
struct Block { void* pointer; std::uint32_t bytes; };
struct Oracle {
    std::array<std::vector<Block>,16> bins;
    std::uint64_t visits = 0;
    void retire(void* p, std::uint32_t n) {
        n = rounded(n);
        auto& bin = bins[klass(n)];
        bin.insert(bin.begin(), Block{p,n});
    }
    void* take(std::uint32_t n, std::size_t alignment) {
        n = rounded(n,alignment);
        for (auto k = klass(n); k < 16; ++k) {
            auto& bin = bins[k];
            for (auto it = bin.begin(); it != bin.end(); ++it) {
                ++visits;
                if (it->bytes >= n && reinterpret_cast<std::uintptr_t>(it->pointer) % alignment == 0) {
                    auto* result = it->pointer;
                    bin.erase(it);
                    return result;
                }
            }
        }
        return nullptr;
    }
    void remove(void* pointer) {
        for (auto& bin : bins) {
            auto it = std::find_if(bin.begin(),bin.end(),[&](const auto& b){return b.pointer == pointer;});
            if (it != bin.end()) { bin.erase(it); return; }
        }
        throw std::runtime_error("oracle remove absent");
    }
    std::uint64_t bytes() const {
        std::uint64_t total=0;
        for (const auto& bin : bins) for (const auto& b : bin) total+=b.bytes;
        return total;
    }
    void load(fd::allocator_ref ref) {
        for (auto& bin : bins) bin.clear();
        for (std::uint32_t k=0;k<16;++k) {
            for (auto off=ref.state->freelist_head[k];off;) {
                auto* b=fd::freelist_block_from_offset(ref.state,off);
                bins[k].push_back({b,b->block_bytes});off=b->next_offset;
            }
        }
    }
};
struct Arena {
    xo::XBufferCore core;
    xo::XAllocatorStateV1* state;
    Oracle oracle;
    Arena() : core(16u*1024u*1024u,xo::XBufferCore::maximum_capacity(32u*1024u*1024u)) {
        state=core.construct_allocator_storage<xo::XAllocatorStateV1>();
        fd::initialize_allocator_state(*state,core.arena());
    }
    fd::allocator_ref ref() { return {state,core.arena()}; }
    void* alloc(std::uint32_t n,std::size_t alignment=1) {
        auto* expected=oracle.take(n,alignment);
        auto* actual=fd::allocate_dynamic_bytes(ref(),n,alignment);
        need(!expected || actual==expected,"first-fit pointer differs from original oracle");
        need(oracle.bytes()==state->free_bytes,"free_bytes after allocation differs");
        return actual;
    }
    void retire(void* p,std::uint32_t n) {
        oracle.retire(p,n);fd::retire_dynamic_bytes(ref(),p,n);
        need(oracle.bytes()==state->free_bytes,"free_bytes after retirement differs");
    }
};
void emit(const char* name,std::size_t n,std::uint64_t baseline) {
    const auto c=dt::freelist_index_stats;
    std::cout << "{\"case\":\"" << name << "\",\"n\":" << n
      << ",\"baseline_node_visits\":" << baseline
      << ",\"node_visits\":" << c.node_visits << ",\"skip_jumps\":" << c.skip_jumps
      << ",\"lookup_calls\":" << c.lookup_calls << ",\"retire_updates\":" << c.retire_updates
      << ",\"cache_resets\":" << c.cache_resets << ",\"query_resets\":" << c.query_resets
      << ",\"maintenance_calls\":" << c.maintenance_calls
      << ",\"maintenance_comparisons\":" << c.maintenance_comparisons
      << ",\"runtime_owner_acquisitions\":" << c.runtime_owner_acquisitions
      << ",\"sizeof_core\":" << sizeof(xo::XBufferCore)
      << ",\"sizeof_runtime_cache\":" << sizeof(dt::FreelistRuntimeCache) << "}\n";
}
void maintenance_bound() {
    const auto& c=dt::freelist_index_stats;
    need(c.maintenance_comparisons <=
      64*(c.lookup_calls+c.retire_updates+c.cache_resets+1)+4*(c.node_visits+c.skip_jumps),
      "predeclared maintenance comparison bound exceeded");
}
void mixed(std::size_t n,bool same_bucket_retire) {
    Arena a;std::vector<void*> small,fit,extra;
    for (std::size_t i=0;i<n;++i) {small.push_back(a.alloc(24));fit.push_back(a.alloc(32));}
    if(same_bucket_retire) for(std::size_t i=0;i<n;++i) extra.push_back(a.alloc(24));
    for(std::size_t i=0;i<n;++i) {a.retire(small[i],24);a.retire(fit[i],32);}
    a.oracle.visits=0;dt::reset_freelist_index_stats();
    for(std::size_t i=0;i<n;++i) {
        if(same_bucket_retire) a.retire(extra[i],24);
        (void)a.alloc(32);
    }
    const auto c=dt::freelist_index_stats;
    need(c.lookup_calls==n,"lookup gate count");
    need(c.node_visits+c.skip_jumps<=8*n+1024,"predeclared mixed workload gate exceeded");
    maintenance_bound();
    need(a.oracle.visits==(same_bucket_retire?n*(n+1):n*(n+1)/2),"original oracle mixed count");
    need(c.runtime_owner_acquisitions==n*(same_bucket_retire?2:1),"runtime owner acquisition accounting");
    emit(same_bucket_retire?"mixed-same-bucket-retire":"mixed-static",n,a.oracle.visits);
}

#include "workload_prefix.inc"
void actual_M_prefix() {
    Arena a;std::vector<void*> names,samples,payloads,values;
    for(auto len:workload_old_name_lengths) names.push_back(a.alloc(len+1));
    for(std::size_t i=0;i<4096;++i) samples.push_back(a.alloc(128));
    for(std::size_t i=0;i<4096;++i) payloads.push_back(a.alloc(128));
    for(std::size_t i=0;i<4096;++i) values.push_back(a.alloc(16));
    for(std::size_t i=0;i<4096;++i) {(void)a.alloc(129);a.retire(names[i],workload_old_name_lengths[i]+1);}
    for(std::size_t i=0;i<4096;++i) {(void)a.alloc(256);a.retire(samples[i],128);}
    for(std::size_t i=0;i<4096;++i) {(void)a.alloc(384);a.retire(payloads[i],128);}
    a.oracle.visits=0;dt::reset_freelist_index_stats();
    for(std::size_t i=0;i<4096;++i) {(void)a.alloc(32);a.retire(values[i],16);}
    need(a.oracle.visits==2232651,"fixed M prefix baseline scan differs");
    need(dt::freelist_index_stats.node_visits+dt::freelist_index_stats.skip_jumps<=8*4096+1024,"fixed M prefix gate");
    maintenance_bound();emit("fixed-M-stages2-through5-size-replay",4096,a.oracle.visits);
}
void query_changes_and_randomized_oracle() {
    Arena a;std::mt19937_64 rng(0x4f32505245464958ull);
    struct Live {void* p;std::uint32_t n;};std::vector<Live> live;
    const std::uint32_t sizes[]={8,16,24,32,40,48,56,64,72,128};
    const std::size_t alignments[]={1,2,4,8,16,32};
    dt::reset_freelist_index_stats();
    for(unsigned i=0;i<3000;++i) {
        if(!live.empty() && rng()%3==0) {
            auto at=rng()%live.size();auto b=live[at];
            a.retire(b.p,b.n);live.erase(live.begin()+at);
        } else {
            auto n=sizes[rng()%10];auto alignment=alignments[rng()%6];
            live.push_back({a.alloc(n,alignment),rounded(n,alignment)});
        }
    }
    maintenance_bound();emit("seeded-firstfit-size-alignment-switching",3000,a.oracle.visits);
}
void middle_removal_with_unchanged_head() {
    Arena a;
    auto* aligned48=a.alloc(48,32);
    void* unaligned56=nullptr;
    for(unsigned i=0;i<8 && !unaligned56;++i) {
        auto* p=a.alloc(56);
        if(reinterpret_cast<std::uintptr_t>(p)%32) unaligned56=p;
    }
    need(unaligned56,"could not prepare deterministic alignment case");
    auto* fit64=a.alloc(64);
    a.retire(fit64,64);a.retire(aligned48,48);a.retire(unaligned56,56);
    need(a.alloc(64)==fit64,"prepare rejected interval");
    const auto head=a.state->freelist_head[klass(64)];
    need(a.alloc(32,32)==aligned48,"different alignment should remove middle prefix node");
    need(a.state->freelist_head[klass(64)]==head,"middle removal unexpectedly changed head");
    std::memset(aligned48,0xff,32);
    (void)a.alloc(64); // Must not read the now-live poisoned old skip endpoint.
}
void lifecycle_and_epoch() {
    Arena a;
    auto* s0=a.alloc(24);auto* s1=a.alloc(24);auto* f0=a.alloc(32);auto* f1=a.alloc(32);
    a.retire(f1,32);a.retire(f0,32);a.retire(s1,24);a.retire(s0,24);
    (void)a.alloc(32); // known rejected interval ends at s1, head stays s0
    auto* b0=static_cast<fd::FreeBlockHeader*>(s0);
    auto* b1=static_cast<fd::FreeBlockHeader*>(s1);
    b0->next_offset=b1->next_offset; // independent legal list edit with epoch
    a.state->free_bytes-=24;++a.state->epoch;a.oracle.remove(s1);
    std::memset(s1,0xff,24); // would poison a stale cached last-offset read
    (void)a.alloc(32);

    auto* free24=a.alloc(24);auto* free32=a.alloc(32);
    a.retire(free32,32);a.retire(free24,24);
    const auto before=a.core.copy_bytes();auto* old_arena=a.core.arena();auto* state=a.state;
    xo::XBufferCore moved(std::move(a.core));
    need(moved.copy_bytes()==before,"Core move changed wire bytes");
    auto* expected=a.oracle.take(32,1);
    auto* actual=fd::allocate_dynamic_bytes({state,old_arena},32,1);
    need(expected==actual,"move-constructor cache lifetime");
    Arena other;auto* other_state=other.state;
    moved.swap(other.core);
    need(other.core.arena()==old_arena,"swap did not preserve storage address");
    auto* more=fd::allocate_dynamic_bytes({state,old_arena},32,1);
    fd::retire_dynamic_bytes({state,old_arena},more,32);
    auto* borrowed=fd::allocate_dynamic_bytes({other_state,moved.arena()},24,1);
    fd::retire_dynamic_bytes({other_state,moved.arena()},borrowed,24);
    xo::XBufferCore destination;destination=std::move(other.core);
    fd::retire_dynamic_bytes({state,destination.arena()},actual,32);
    const auto wire=destination.copy_bytes();
    xo::XBufferCore reloaded(wire.data(),wire.size(),xo::XBufferCore::maximum_capacity(32u*1024u*1024u));
    auto* rs=reloaded.find_allocator_storage<xo::XAllocatorStateV1>();need(rs,"reload allocator state");
    Oracle ro;ro.load({rs,reloaded.arena()});
    auto* wanted=ro.take(32,1);auto* obtained=fd::allocate_dynamic_bytes({rs,reloaded.arena()},32,1);
    need(!wanted || obtained==wanted,"loaded host cache must start empty");
    fd::release_cached_blocks(rs);need(rs->free_bytes==0,"release free bytes");
    for(auto head:rs->freelist_head) need(head==0,"release retained head");
    fd::retire_dynamic_bytes({rs,reloaded.arena()},obtained,32);
    fd::initialize_allocator_state(*rs,reloaded.arena());
    need(rs->free_bytes==0 && rs->epoch==0,"state reset not empty");
    (void)fd::allocate_dynamic_bytes({rs,reloaded.arena()},32,1);
    // Multiple-state identity is tested separately with both states preceding
    // every managed allocation, as required by positive state-relative offsets.
    need(sizeof(xo::XAllocatorStateV1)==88 && sizeof(xo::XBufferCore::arena_header)==48,"V1 layout changed");
}
void multiple_states_before_managed_blocks() {
    Arena a;
    // Construct BOTH states before any reusable managed payload. A late state
    // after Core free blocks would violate the existing positive-offset rule.
    auto* second=static_cast<xo::XAllocatorStateV1*>(a.core.arena()->allocate_aligned(sizeof(xo::XAllocatorStateV1),alignof(xo::XAllocatorStateV1)));
    std::construct_at(second);fd::initialize_allocator_state(*second,a.core.arena());
    Oracle other;
    auto alloc_second=[&](std::uint32_t n) {
        auto* expected=other.take(n,1);
        auto* actual=fd::allocate_dynamic_bytes({second,a.core.arena()},n,1);
        need(!expected || actual==expected,"second-state firstfit oracle");
        need(other.bytes()==second->free_bytes,"second-state free_bytes");
        return actual;
    };
    auto retire_second=[&](void* p,std::uint32_t n) {
        other.retire(p,n);fd::retire_dynamic_bytes({second,a.core.arena()},p,n);
        need(other.bytes()==second->free_bytes,"second-state retirement free_bytes");
    };
    auto* p24=a.alloc(24);auto* p32=a.alloc(32);
    auto* q24=alloc_second(24);auto* q32=alloc_second(32);
    a.retire(p32,32);a.retire(p24,24);
    retire_second(q32,32);retire_second(q24,24);
    need(a.state->epoch==second->epoch,"prepare equal epochs for state identity");
    need(a.alloc(32)==p32,"primary state cache query");
    need(alloc_second(32)==q32,"secondary state cache query");
    need(a.state->epoch==second->epoch,"state identities must matter when epochs match");
    (void)a.alloc(32);(void)alloc_second(32);
    a.retire(p32,32);retire_second(q32,32);
    auto copy=a.ref();
    auto* expected=a.oracle.take(32,1);
    need(fd::allocate_dynamic_bytes(copy,32,1)==expected,"copied allocator_ref state identity");
    need(alloc_second(32)==q32,"alternating same-owner state identity");
}
void late_state_rejected_by_existing_offset_guard() {
    Arena a;auto* early=a.alloc(24);a.retire(early,24);
    fd::release_cached_blocks(a.state); // Core now owns an early reusable24B block.
    auto* late=static_cast<xo::XAllocatorStateV1*>(a.core.arena()->allocate_aligned(sizeof(xo::XAllocatorStateV1),alignof(xo::XAllocatorStateV1)));
    std::construct_at(late);fd::initialize_allocator_state(*late,a.core.arena());
    bool rejected=false;
    try {(void)fd::allocate_dynamic_bytes({late,a.core.arena()},24,1);}
    catch(const xo::XException& e) {
        rejected=std::string_view(e.what())=="allocator state free-list offset out of range";
    }
    need(rejected,"existing positive-offset guard must reject a late state reusing an earlier block");
}
void existing_owner_and_invalid_requests() {
    Arena a;auto* p=a.alloc(32);a.retire(p,32);
    xo::detail::fail_next_registry_registration();
    try {
        need(a.alloc(32)==p,"cache hit while registration failure is armed");
        a.retire(p,32);
        bool bad=false;try {xo::XBufferCore b(1024);}catch(const xo::XBadAlloc&){bad=true;}
        need(bad,"existing-owner lookup consumed registry injection");
    }catch(...){xo::detail::clear_registry_failures();throw;}
    xo::detail::clear_registry_failures();
    const auto before=a.core.copy_bytes();
    for(auto alignment:{std::size_t{0},std::size_t{3}}) {
        bool bad=false;try{(void)fd::allocate_dynamic_bytes(a.ref(),32,alignment);}catch(const xo::XBadAlloc&){bad=true;}
        need(bad && a.core.copy_bytes()==before,"invalid request changed arena/cache-visible wire");
    }
    static_assert(noexcept(std::declval<dt::FreelistRuntimeCache&>().synchronize(nullptr,0)));
}
}
int main() {
    try {
        for(auto n:{512u,1024u,2048u,4096u}) {mixed(n,false);mixed(n,true);}
        actual_M_prefix();query_changes_and_randomized_oracle();middle_removal_with_unchanged_head();lifecycle_and_epoch();multiple_states_before_managed_blocks();late_state_rejected_by_existing_offset_guard();existing_owner_and_invalid_requests();
        std::cout << "{\"status\":\"passed\",\"performance_measurement\":false}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
