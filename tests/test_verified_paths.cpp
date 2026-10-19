#include "xoffsetdatastructure.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>

namespace host {
inline thread_local long remaining = -1;
inline thread_local void (*callback)() = nullptr;
inline thread_local unsigned calls = 0;
inline thread_local void* arena_allocation = nullptr;
inline thread_local std::size_t arena_bytes = 0;
inline thread_local bool route_arena = false;
}
void* operator new(std::size_t n) {
    if (std::exchange(host::route_arena, false)) {
        if (n > host::arena_bytes) throw std::bad_alloc();
        return host::arena_allocation;
    }
    if (host::remaining == 0) {
        host::remaining = -1;
        ++host::calls;
        if (auto f = std::exchange(host::callback, nullptr)) f();
        else throw std::bad_alloc();
    } else if (host::remaining > 0) --host::remaining;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
    if (p && p==host::arena_allocation) {host::arena_allocation=nullptr;return;}
    std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace xo = XOffsetDatastructure;
struct PathsRoot {
    std::uint64_t value{};
    bool enabled{};
    xo::XString name;
    xo::XVector<std::uint8_t> blob;
    xo::XVector<xo::XString> labels;
};
XOFFSET_REGISTER_SCHEMA_NAME(PathsRoot, "test.VerifiedPathsRoot")

namespace {
void need(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
auto make() {
    auto result = xo::TypedXBuffer<PathsRoot>::create(32768,
        xo::XBuffer::maximum_capacity(1024 * 1024).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = result.root();r.value = 0x1234567812345678ULL;r.enabled = true;
    r.name = "verified paths";r.blob.resize(80);
    for (std::size_t i=0;i<r.blob.size();++i) r.blob[i]=std::uint8_t(i+1);
    r.labels.push_back("nested");r.labels.push_back("values");return result;
}
void check(const PathsRoot& r) {
    need(r.value==0x1234567812345678ULL && r.enabled && r.name=="verified paths", "root scalars/string differ");
    need(r.blob.size()>=80 && r.labels.size()==2 && r.labels[0]=="nested" && r.labels[1]=="values", "nested fields differ");
    for(std::size_t i=0;i<80;++i) need(r.blob[i]==std::uint8_t(i+1),"blob differs");
}
template<class F> void rejects(F&& operation) {
    bool caught=false;try {operation();}catch(const xo::XException&){caught=true;}
    need(caught,"expected rejection missing");
}

#if defined(XOFFSET_VERIFIED_PATHS_API)
xo::XBufferCore *from{}, *to{};
xo::TypedXBuffer<PathsRoot>* typed{};
void move_owner() { *to=std::move(*from); }
void swap_owner() { from->swap(*to); }
void release_owner() { *from=xo::XBufferCore{}; }
void grow_payload() { typed->root().blob.resize(131072); }
void grow_extent() { need(from->grow(4096),"callback extent growth failed"); }
struct Arm {
    explicit Arm(void(*f)()=nullptr) {host::calls=0;host::callback=f;host::remaining=0;}
    ~Arm() {host::remaining=-1;host::callback=nullptr;}
};

void into_exact_hot_and_oom() {
    auto source=make();auto* base=source.get_address();auto epoch=source.epoch();auto handle=source.handle();
    auto* root=&source.root();const auto raw=source.copy_bytes();const auto logical=source.get_size();
    std::string out="original destination value";
    bool oom=false;
    {Arm arm;try{source.save_verified_into(out);}catch(const std::bad_alloc&){oom=true;}}
    need(oom && out=="original destination value" && source.copy_bytes()==raw && source.get_size()==logical,
         "new API output OOM changed value/source before normalization");
    source.save_verified_into(out);
    need(out==source.save_verified() && source.get_address()==base && source.epoch()==epoch && handle.get()==root,
         "into differs from exact old V1 or moved owner");
    const auto capacity=out.capacity();
    {Arm arm;source.save_verified_into(out);need(host::remaining==0,"hot output capacity allocated");}
    need(out.capacity()==capacity,"hot capacity changed");
    auto loaded=xo::XBuffer::load_verified<PathsRoot>(out);check(loaded.root<PathsRoot>());
    // Old contract deliberately differs: old save normalizes before output OOM.
    auto old=make();const auto used=old.arena()->used_end;need(old.get_size()>used,"old OOM fixture lacks spare extent");
    {Arm arm;bool failed=false;try{(void)old.save_verified();}catch(const std::bad_alloc&){failed=true;}
        need(failed && old.get_size()==used,"old save OOM normalization contract changed");}
}

void into_owner_callbacks() {
    for(unsigned action=0;action<5;++action) {
        auto source=make();xo::XBufferCore destination;auto other=make();
        from=&source;typed=&source;to=action==1?static_cast<xo::XBufferCore*>(&other):&destination;
        const auto base=source.get_address();const auto epoch=source.epoch();
        std::string out="unchanged";
        const auto expected=source.save_verified(); // compare valid old normalized V1
        // Force a real empty output allocation, not a capacity hit.
        out=std::string("unchanged");
        auto callback=action==0?move_owner:action==1?swap_owner:action==2?release_owner:action==3?grow_extent:grow_payload;
        bool failed=false;
        {Arm arm(callback);try{source.save_verified_into(out);}catch(const xo::XException&){failed=true;}
            need(host::calls==1,"output allocation callback not exercised exactly once");}
        if(action==2 || action==4) need(failed && out=="unchanged","released/oversized source not rejected atomically");
        else {
            need(!failed && out==expected,"moved/swapped/grown source identity was not re-resolved");
            auto loaded=xo::XBuffer::load_verified<PathsRoot>(out);check(loaded.root<PathsRoot>());
        }
        if(action==0 || action==1) need(to->get_address()==base,"record did not follow actual moved payload");
        if(action==3) need(source.epoch()==epoch && source.get_address()==base,"in-place callback changed identity unexpectedly");
    }
}

void into_alias_and_unverified() {
    auto source=make();
    auto* raw=source.arena()->allocate_aligned(sizeof(std::string),alignof(std::string));
    auto* alias=std::construct_at(static_cast<std::string*>(raw),"alias");
    rejects([&]{source.save_verified_into(*alias);});need(*alias=="alias","alias rejected after output mutation");
    std::destroy_at(alias);
    {
        // A legal replaceable operator-new route gives string an allocation
        // inside this arena while the string object itself remains external.
        // clear() retains capacity, so testing only size() would miss it.
        std::string external;
        host::arena_allocation=source.arena()->allocate_aligned(2048,alignof(std::max_align_t));
        host::arena_bytes=2048;host::route_arena=true;external.reserve(1024);
        need(external.data()==host::arena_allocation,"custom arena string allocation not used");
        external.clear();need(external.empty() && external.capacity()>=1024,"empty retained-capacity fixture differs");
        const auto before=source.copy_bytes();
        rejects([&]{source.save_verified_into(external);});
        need(external.empty() && source.copy_bytes()==before,"capacity-only alias changed source/output");
    }
    auto unverified=xo::XBuffer::load_unverified(source.bytes());std::string out="keep";
    rejects([&]{unverified.save_verified_into<PathsRoot>(out);});need(out=="keep","unverified changed output");
}

struct FailingResource final : std::pmr::memory_resource {
    unsigned attempts{},fail_at{};std::size_t live{};
    explicit FailingResource(unsigned point):fail_at(point){}
    void* do_allocate(std::size_t n,std::size_t a) override {
        if(attempts++==fail_at) throw std::bad_alloc();
        auto* p=std::pmr::new_delete_resource()->allocate(n,a);live+=n;return p;
    }
    void do_deallocate(void* p,std::size_t n,std::size_t a) override {
        std::pmr::new_delete_resource()->deallocate(p,n,a);live-=n;
    }
    bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override{return this==&other;}
};
std::string join(const xo::VerifiedSnapshot<PathsRoot>& snapshot) {
    const auto spans=snapshot.segments();std::string wire;
    for(auto s:spans) wire.append(reinterpret_cast<const char*>(s.data()),s.size());return wire;
}
void freeze_transaction_and_lease() {
    auto source=make();const auto raw=source.copy_bytes();auto* base=source.get_address();const auto logical=source.get_size();
    bool oom=false;
    {Arm arm;try{(void)xo::freeze_verified(std::move(source));}catch(const std::bad_alloc&){oom=true;}}
    need(oom && source.get_address()==base && source.copy_bytes()==raw && source.get_size()==logical,
         "shared control OOM consumed/normalized source");
    // Force both independent monotonic upstream allocations by retaining a
    // retired allocation, alongside owned graph ranges.
    for(unsigned point=0;point<2;++point) {
        auto candidate=make();candidate.root().blob.reserve(400);
        const auto before=candidate.copy_bytes();const auto extent=candidate.get_size();const auto* address=candidate.get_address();
        FailingResource resource(point);bool failed=false;
        try{(void)xo::freeze_verified(std::move(candidate),&resource);}catch(const std::bad_alloc&){failed=true;}
        need(failed && resource.attempts==point+1 && resource.live==0 && candidate.get_address()==address &&
             candidate.copy_bytes()==before && candidate.get_size()==extent,"scratch OOM consumed or changed source");
        check(candidate.root());
    }
    auto frozen=xo::freeze_verified(std::move(source));
    need(!source.get_address() && frozen.segments()[1].data()==base,"freeze copied payload or failed to consume at commit");
    check(frozen.root());const auto segments=frozen.segments();const auto wire=join(frozen);
    auto moved=std::move(frozen);need(moved.segments()[0].data()==segments[0].data() &&
        moved.segments()[1].data()==segments[1].data(),"snapshot handle move invalidated segments");
    auto io=moved.acquire_io_lease();moved={};io.cancel();
    need(io.cancellation_requested() && io.segments()[1].data()==base,"cancellation released payload before completion");
    auto loaded=xo::XBuffer::load_verified<PathsRoot>(wire);check(loaded.root<PathsRoot>());
    io.complete();need(io.segments()[0].empty() && io.segments()[1].empty(),"completion did not release lease");
}

void freeze_complete_header_and_graph() {
    for(unsigned action=0;action<4;++action) {
        auto source=make();const auto before=source.copy_bytes();const auto* base=source.get_address();
        if(action==0) source.arena()->magic[0]^=1;
        if(action==1) source.arena()->header_size=0;
        if(action==2) source.arena()->used_end=source.get_size()+1;
        if(action==3) {auto* p=reinterpret_cast<unsigned char*>(&source.root().enabled);*p=2;}
        const auto invalid=source.copy_bytes();
        rejects([&]{(void)xo::freeze_verified(std::move(source));});
        need(source.get_address()==base && source.copy_bytes()==invalid,"failed freeze consumed or rewrote invalid source");
        // Restore via char storage; no bool lvalue is evaluated while invalid.
        std::memcpy(source.data(),before.data(),before.size());check(source.root());
    }
}
#endif
}

int main(int argc,char** argv) {try {
    if(argc==3 && std::string_view(argv[1])=="--write-wire") {
        auto source=make();
#if defined(XOFFSET_VERIFIED_PATHS_API)
        auto frozen=xo::freeze_verified(std::move(source));const auto wire=join(frozen);
#else
        const auto wire=source.save_verified();
#endif
        std::ofstream f(argv[2],std::ios::binary);f.write(wire.data(),wire.size());need(bool(f),"wire write failed");return 0;
    }
    if(argc==3 && std::string_view(argv[1])=="--read-wire") {
        std::ifstream f(argv[2],std::ios::binary);std::string wire((std::istreambuf_iterator<char>(f)),{});
        need(bool(f)||f.eof(),"wire read failed");auto source=xo::XBuffer::load_verified<PathsRoot>(wire);
        check(source.root<PathsRoot>());return 0;
    }
#if defined(XOFFSET_VERIFIED_PATHS_API)
    into_exact_hot_and_oom();into_owner_callbacks();into_alias_and_unverified();
    freeze_transaction_and_lease();freeze_complete_header_and_graph();
    std::cout<<"verified paths: five groups, exact/hot/OOM/owner callbacks/alias/freeze/lifetime/V1 passed\n";
#else
    throw std::runtime_error("full tests require the new APIs; baseline supports compatibility CLI only");
#endif
    return 0;
}catch(const std::exception& e){host::remaining=-1;host::callback=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
