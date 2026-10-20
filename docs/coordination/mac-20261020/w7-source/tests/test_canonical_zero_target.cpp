#include "xoffsetdatastructure.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <meta>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fixture {
using Core=XOffsetDatastructure::XBufferCore;
// This test TU alone uses the project's already-required P2996 unchecked
// reflection. No public production hook, friend or private/stdlib macro exists.
consteval std::meta::info member(std::string_view name) {
    auto found=^^void;std::size_t count=0;
    for(const auto item:std::meta::members_of(^^Core,std::meta::access_context::unchecked()))
        if(std::meta::has_identifier(item)&&std::meta::identifier_of(item)==name){found=item;++count;}
    if(count!=1)throw "exact canonical private member missing or ambiguous";
    return found;
}
inline constexpr auto registry_member=member("registry_state");
inline constexpr auto cache_member=member("owner_cache");
inline constexpr auto available_member=member("owner_cache_available");
inline constexpr auto ledger_member=member("canonical_active_count_");
inline constexpr auto mark_member=member("mark_canonical_registration");
inline constexpr auto retire_member=member("retire_canonical_registration");
decltype(auto) registry(){return [:registry_member:]();}
decltype(auto) cache(){return [:cache_member:]();}
decltype(auto) available()noexcept{return [:available_member:]();}
decltype(auto) ledger()noexcept{return ([:ledger_member:]);}
using State=std::remove_reference_t<decltype(registry())>;
using Cache=std::remove_reference_t<decltype(cache())>;
using Owners=std::remove_cvref_t<decltype(std::declval<State&>().owners)>;
using RecordPtr=typename Owners::mapped_type;
using Record=typename RecordPtr::element_type;
void mark(Record& r)noexcept{[:mark_member:](r);}
void retire(Record& r)noexcept{[:retire_member:](r);}
void need(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void cleanup_need(bool v)noexcept{if(!v){std::fputs("private canonical fixture cleanup failed\n",stderr);std::abort();}}

inline constexpr std::size_t N=64;
struct Adjacent {std::byte a[N];std::byte b[N];};
static_assert(std::is_standard_layout_v<Adjacent>);
static_assert(offsetof(Adjacent,a)==0&&offsetof(Adjacent,b)==N&&sizeof(Adjacent)==2*N);

class Scope {
    State& state_;
    Cache& cache_;
    std::array<RecordPtr,4> saved_cache_{};
    std::size_t saved_next_{},saved_last_{},saved_count_{};
    bool saved_available_{};
    std::vector<std::pair<const void*,RecordPtr>> saved_map_;
    bool installed_a_{},installed_b_{};
public:
    RecordPtr a,b;
    static Cache& checked_cache(){need(available(),"test main TLS cache is unavailable");return cache();}
    Scope(Adjacent& bytes,Core& owner_a,Core& owner_b,bool b_canonical)
        :state_(registry()),cache_(checked_cache()),
         a(std::make_shared<Record>(bytes.a,&owner_a,bytes.a,N)),
         b(std::make_shared<Record>(bytes.b,&owner_b,b_canonical?bytes.b:nullptr,N)) {
        saved_available_=available();saved_next_=cache_.next;saved_last_=cache_.last;
        for(std::size_t i=0;i<4;++i)saved_cache_[i]=cache_.records[i];
        std::size_t old_size;
        {std::lock_guard lock(state_.mutex);old_size=state_.owners.size();}
        saved_map_.reserve(old_size); // never allocate under the registry mutex
        {std::lock_guard lock(state_.mutex);
         cleanup_need(state_.owners.size()==old_size); // no exception allocation under mutex
         for(const auto& entry:state_.owners)saved_map_.push_back(entry);
         saved_count_=ledger().load(std::memory_order_acquire);}
    }
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    void install(){
        // Both real allocations/node construction happen before publication.
        Owners prepared;prepared.emplace(a->payload,a);prepared.emplace(b->payload,b);
        auto na=prepared.extract(a->payload),nb=prepared.extract(b->payload);
        {std::lock_guard lock(state_.mutex);
         cleanup_need(!state_.owners.contains(a->payload)&&!state_.owners.contains(b->payload));
         auto first=state_.owners.insert(std::move(na));cleanup_need(first.inserted);installed_a_=true;mark(*a);
         auto second=state_.owners.insert(std::move(nb));cleanup_need(second.inserted);installed_b_=true;mark(*b);}
        seed(b);
        need(ledger().load(std::memory_order_acquire)==saved_count_+1+(b->canonical_origin?1u:0u),
             "fixture active publication ledger differs");
    }
    void seed(const RecordPtr& value)noexcept{
        // saved_cache_ and a/b retain leases, so replacing slots cannot destroy
        // their control blocks or invoke delete/new-handler reentry here.
        for(auto& r:cache_.records)r.reset();
        cache_.records[0]=value;cache_.last=0;cache_.next=1;
    }
    bool selected(const RecordPtr& r)const noexcept{
        return cache_.last<4&&cache_.records[cache_.last]==r;
    }
    bool only_b()const noexcept{
        if(cache_.records[0]!=b)return false;
        for(std::size_t i=1;i<4;++i)if(cache_.records[i])return false;
        return true;
    }
    ~Scope()noexcept{
        // Invalidate before detaching nodes, matching the production lifetime
        // ordering. Default Core objects themselves never own these byte arrays.
        a->owner.store(nullptr,std::memory_order_release);
        b->owner.store(nullptr,std::memory_order_release);
        typename Owners::node_type na,nb;
        {std::lock_guard lock(state_.mutex);
         if(installed_a_){auto i=state_.owners.find(a->payload);cleanup_need(i!=state_.owners.end()&&i->second==a);retire(*a);na=state_.owners.extract(i);}
         if(installed_b_){auto i=state_.owners.find(b->payload);cleanup_need(i!=state_.owners.end()&&i->second==b);retire(*b);nb=state_.owners.extract(i);}
         cleanup_need(state_.owners.size()==saved_map_.size());
         auto actual=state_.owners.begin();
         for(const auto& old:saved_map_){cleanup_need(actual!=state_.owners.end()&&actual->first==old.first&&actual->second==old.second);++actual;}
         cleanup_need(ledger().load(std::memory_order_acquire)==saved_count_);}
        // Node/control-block release and cache restoration occur unlocked.
        for(std::size_t i=0;i<4;++i)cache_.records[i]=saved_cache_[i];
        cache_.next=saved_next_;cache_.last=saved_last_;available()=saved_available_;
        cleanup_need(!a->canonical_registered.load(std::memory_order_relaxed)&&
                     !b->canonical_registered.load(std::memory_order_relaxed));
        for(std::size_t i=0;i<4;++i)cleanup_need(cache_.records[i]==saved_cache_[i]);
        cleanup_need(cache_.next==saved_next_&&cache_.last==saved_last_&&available()==saved_available_);
    }
};

void run_case(bool b_canonical,bool expect_old_bug){
    Adjacent bytes{};
    Core identity_a,identity_b; // DEFAULT live objects: no VM/reservation/load
    const auto target=reinterpret_cast<std::uintptr_t>(bytes.a)+N;
    need(target==reinterpret_cast<std::uintptr_t>(bytes.b),"flat numeric adjacent member boundary differs");
    Scope scope(bytes,identity_a,identity_b,b_canonical);scope.install();
    need(scope.only_b(),"cache accidentally contained cold anchor A");
    auto* result=Core::canonical_bytes_at_active(bytes.a+N/2,target,0);
    if(expect_old_bug){
        need(scope.selected(scope.b),"old source did not reproduce target-only B selection");
        if(b_canonical)need(result!=nullptr&&reinterpret_cast<std::uintptr_t>(result)==target,"old C/C numeric result differs");
        else need(result==nullptr,"old C/ordinary source did not reproduce lost one-past");
    }else{
        need(result==bytes.a+N&&scope.only_b()&&cache().next==1&&cache().last==0,"cold anchor origin differs or cold lookup evicted/installed a cache entry");
    }
    // A positive-width independent target must still select its own domain.
    scope.seed(scope.b);
    auto* positive=Core::canonical_bytes_at_active(bytes.a+N/2,target,1);
    need(scope.selected(scope.b)&&positive==(b_canonical?bytes.b:nullptr),"nonzero cross-domain hot path changed");
    // An actually hot fitting anchor already had correct priority in v2.
    scope.seed(scope.a);
    need(Core::canonical_bytes_at_active(bytes.a+1,target,0)==bytes.a+N&&scope.selected(scope.a),"hot anchor one-past changed");
    scope.seed(scope.b);
    need(Core::canonical_bytes_at_active(bytes.b+1,target,0)==(b_canonical?bytes.b:nullptr)&&scope.selected(scope.b),"B's own zero-length domain changed");
}
} // namespace fixture

int main(int argc,char** argv){try{
    const bool control=argc==2&&std::string_view(argv[1])=="--expect-old-bug";
    fixture::need(argc==1||control,"only optional --expect-old-bug control is supported");
    fixture::run_case(false,control);fixture::run_case(true,control);
    std::cout<<(control?"old helper counterexample reproduced":"canonical zero-target regression passed")
             <<"; two deterministic adjacent-array registry fixtures; no VM/platform result\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
