#include "xoffsetdatastructure.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <array>
#include <optional>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif
#if defined(XOFFSET_VERIFIED_RECEIVE_API)
#include "receive_custom_future_examples.hpp"
#endif

namespace host {
inline thread_local long remaining=-1;
inline thread_local void(*callback)()=nullptr;
inline thread_local unsigned callbacks=0;
}
void* operator new(std::size_t n) {
    if(host::remaining==0) {
        host::remaining=-1;
        if(auto action=std::exchange(host::callback,nullptr)){++host::callbacks;action();}
        else throw std::bad_alloc();
    }else if(host::remaining>0)--host::remaining;
    if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

namespace xo=XOffsetDatastructure;
struct ReceiveLeaf { xo::XString key;xo::XVector<std::uint8_t> value; };
struct ReceiveRoot {
    std::uint64_t marker{};
    std::uint32_t counter{};
    bool enabled{};
    xo::XString name;
    xo::XVector<ReceiveLeaf> attrs;
    xo::XVector<bool> flags;
    xo::XVector<std::uint8_t> tail;
    xo::XVector<std::uint32_t> numbers;
};
XOFFSET_REGISTER_SCHEMA_NAME(ReceiveRoot,"test.VerifiedReceiveRoot")
#if defined(XOFFSET_VERIFIED_RECEIVE_API)
struct CustomReceiveRoot {
    xo::XVector<receive_custom_v2_examples::Word> numbers;
    xo::XVector<receive_custom_v2_examples::Owned> attrs;
};
XOFFSET_REGISTER_SCHEMA_NAME(CustomReceiveRoot,"test.VerifiedCustomReceiveRoot.v2")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<ReceiveLeaf>{static constexpr bool ordinary_members=true;};
template<> struct wire_record_layout<ReceiveRoot>{static constexpr bool ordinary_members=true;};
template<> struct wire_record_layout<CustomReceiveRoot>{static constexpr bool ordinary_members=true;};
}
#endif
namespace {
const char* stage="entry";
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto make_source(){
    auto source=xo::TypedXBuffer<ReceiveRoot>::create(32768);
    auto& r=source.root();r.marker=0x9988776655443322ULL;r.counter=7;r.enabled=true;r.name="direct receive";
    r.attrs.resize(3);for(unsigned i=0;i<3;++i){r.attrs[i].key="short";r.attrs[i].value.resize(24);r.attrs[i].value[3]=std::uint8_t(10+i);}
    r.flags.reserve(8);r.flags.push_back(true);
    // Live size validates bit0 only; capacity tail need not hold legal bools.
    reinterpret_cast<unsigned char*>(r.flags.data())[7]=2;
    r.tail.resize(80);for(unsigned i=0;i<80;++i)r.tail[i]=std::uint8_t(i+1);
    r.numbers.reserve(4);r.numbers.resize(1);r.numbers[0]=9;
    r.name.reserve(256); // retain genuine free-list metadata for both-domain raw validation
    return source;
}
void check(const ReceiveRoot& r){
    need(r.marker==0x9988776655443322ULL && r.counter==7 && r.enabled && r.name=="direct receive","root scalar/string differs");
    need(r.numbers.size()==1 && r.numbers.capacity()==4 && r.numbers[0]==9,
         "after-fill recovery restarted initialized scalar array/outer");
    need(r.attrs.size()==3 && r.flags.size()==1 && r.flags[0] && r.tail.size()==80,"root sequence sizes differ");
    for(unsigned i=0;i<3;++i)need(r.attrs[i].key=="short" && r.attrs[i].value.size()==24 && r.attrs[i].value[3]==10+i,"nested activated heads differ");
    for(unsigned i=0;i<80;++i)need(r.tail[i]==i+1,"tail differs");
}
template<class F>void rejects(F&& action){bool failed=false;try{action();}catch(const xo::XException&){failed=true;}need(failed,"expected XException missing");}

#if defined(XOFFSET_VERIFIED_RECEIVE_API)
using Token=xo::VerifiedReceiveToken<ReceiveRoot>;
struct Unsound { xo::XString value;~Unsound(){} };
template<class T>concept CanBegin=requires(std::size_t n){xo::begin_verified_receive<T>(n);};
static_assert(CanBegin<ReceiveRoot>);
static_assert(!CanBegin<Unsound>);
static_assert(xo::detail::receive_raw::future_supported_v<ReceiveRoot>);
static_assert(xo::detail::receive_raw::future_supported_v<ReceiveLeaf>);
static_assert(xo::detail::receive_raw::future_supported_v<xo::XString>);
static_assert(!xo::detail::receive_raw::future_supported_v<Unsound>);
static_assert(xo::is_v1_wire_admitted_v<receive_custom_v2_examples::Word>);
static_assert(xo::is_v1_wire_admitted_v<receive_custom_v2_examples::Owned>);
static_assert(xo::detail::receive_raw::future_supported_v<CustomReceiveRoot>);
static_assert(!CanBegin<receive_custom_v2_examples::RawOnly>);
static_assert(!CanBegin<receive_custom_v2_examples::HiddenRawOnly>);
static_assert(!CanBegin<xo::XVector<receive_custom_v2_examples::RawOnly>>);
static_assert(xo::detail::custom_future_contract::surface_complete<xo::XVector<receive_custom_v2_examples::RawOnly>>());
static_assert(!xo::detail::receive_raw::future_supported_v<receive_custom_v2_examples::MissingRvalue>);
static_assert(!xo::detail::receive_raw::future_supported_v<receive_custom_v2_examples::ThrowingDestructor>);
static_assert(!xo::detail::receive_raw::future_supported_v<receive_custom_v2_examples::MissingInline>);
static_assert(xo::detail::custom_future_contract::surface_complete<receive_custom_v2_examples::HiddenByPolicy>());
static_assert(!xo::detail::receive_raw::future_supported_v<receive_custom_v2_examples::HiddenByPolicy>);
static_assert(xo::detail::custom_future_contract::surface_complete<receive_custom_v2_examples::FutureOnlyChild>());
static_assert(!xo::detail::receive_raw::future_supported_v<receive_custom_v2_examples::FutureOnlyChild>);

template<class T>
xo::VerifiedReceiveToken<T> receive_as(const std::string& wire,std::optional<xo::XBufferCore::MaximumCapacity> cap=std::nullopt){
    need(wire.size()>=sizeof(xo::XWireHeaderV1),"bad fixture wire");
    auto token=xo::begin_verified_receive<T>(wire.size()-sizeof(xo::XWireHeaderV1),cap);
    auto operation=token.acquire_operation();
    // Byte stores model recv, not a memcpy implicit-object-creation shortcut.
    auto header=operation.header_bytes(),body=operation.payload_bytes();
    for(std::size_t i=0;i<header.size();++i)header[i]=std::byte(static_cast<unsigned char>(wire[i]));
    for(std::size_t i=0;i<body.size();++i)body[i]=std::byte(static_cast<unsigned char>(wire[header.size()+i]));
    operation.complete(header.size(),body.size());return token;
}
Token receive(const std::string& wire,std::optional<xo::XBufferCore::MaximumCapacity> cap=std::nullopt){
    return receive_as<ReceiveRoot>(wire,cap);
}
std::string body_image(const Token& token){return std::string(static_cast<const char*>(token.payload_address()),token.payload_size());}
void exercise_edit(xo::TypedXBuffer<ReceiveRoot>& buffer){
    check(buffer.root());need(buffer.grow(4096),"post-receive explicit grow failed");
    auto& r=buffer.root();r.name="mutated received";r.attrs[1].value.clear();r.attrs[1].value.resize(40);r.attrs[1].value[7]=99;
    r.flags.push_back(false);need(r.flags.size()==2&&!r.flags[1],"bool spare activation/growth failed");
    r.attrs.erase(r.attrs.begin());r.attrs.resize(4);r.attrs[3].key="new live head";
    r.tail.resize(200); // keep the old Attr allocation away from the arena tail
    r.attrs.reserve(64); // explicit live->dead aggregate prefix relocation
    r.attrs.push_back(r.attrs[0]); // reflective deep-copy into an unused live-array slot
    need(r.attrs.back().key==r.attrs[0].key && r.attrs.back().value.size()==40 &&
         r.attrs.back().value[7]==99,"received aggregate transfer construction failed");
    auto wire=buffer.save_verified();auto again=xo::TypedXBuffer<ReceiveRoot>::load_verified(wire);
    need(again.root().name=="mutated received"&&again.root().attrs[3].key=="new live head","received further mutation/reload failed");
}
void direct_growth_and_lifetime(){
    auto source=make_source();const auto wire=source.save_verified();const auto payload=wire.size()-sizeof(xo::XWireHeaderV1);
    const auto prefix=payload/2;
    auto token=xo::begin_verified_receive<ReceiveRoot>(prefix,xo::XBuffer::maximum_capacity(payload+8192));
    const auto* base=token.payload_address();
    need(reinterpret_cast<std::uintptr_t>(base)%alignof(std::max_align_t)==0,"payload reservation is unaligned");
    need(token.accessible_bytes()>=token.maximum_capacity(),"whole byte-array storage not accessible before input");
    {
        auto operation=token.acquire_operation();auto h=operation.header_bytes(),b=operation.payload_bytes();
        for(std::size_t i=0;i<h.size();++i)h[i]=std::byte(static_cast<unsigned char>(wire[i]));
        for(std::size_t i=0;i<b.size();++i)b[i]=std::byte(static_cast<unsigned char>(wire[h.size()+i]));
        rejects([&]{(void)xo::finish_verified(token);});rejects([&]{(void)token.grow(1);});
        rejects([&]{(void)token.acquire_operation();});
        operation.complete(h.size(),b.size());
    }
    rejects([&]{(void)xo::finish_verified(token);}); // wire declares more body than current logical length
    const auto before=body_image(token);const auto received=token.received_length();
    xo::detail::fail_next_vm_operation(xo::detail::VmOperation::Prepare);
    need(!token.grow(payload-prefix) && token.payload_address()==base && token.payload_size()==prefix &&
         token.received_length()==received && body_image(token)==before,"failed grow changed received prefix/metadata");
    xo::detail::clear_vm_failures();
    need(token.grow(payload-prefix) && token.payload_address()==base,"grow moved VM reservation");
    need(std::memcmp(token.payload_address(),before.data(),before.size())==0,"successful grow lost old received bytes");
    const auto* all=static_cast<const unsigned char*>(token.payload_address());
    for(auto i=prefix;i<payload;++i)need(all[i]==0,"new logical receive bytes not zero");
    {
        auto operation=token.acquire_operation();auto b=operation.payload_bytes();
        for(auto i=prefix;i<payload;++i)b[i]=std::byte(static_cast<unsigned char>(wire[sizeof(xo::XWireHeaderV1)+i]));
        operation.complete(sizeof(xo::XWireHeaderV1),payload);
    }
    auto output=xo::finish_verified(token);
    need(!token && output.get_address()==base,"finish copied payload or did not consume token");
    need(output.maximum_capacity()==payload+8192,"receive max capacity lost");
    check(output.root());need(output.save_verified()==wire,"activation changed V1 bytes");exercise_edit(output);
}
void completion_cancel_and_owner_lifetime(){
    auto source=make_source();const auto wire=source.save_verified();const auto payload=wire.size()-sizeof(xo::XWireHeaderV1);
    auto token=xo::begin_verified_receive<ReceiveRoot>(payload);const auto* base=token.payload_address();
    auto operation=token.acquire_operation();auto moved=std::move(token);need(!token && moved.payload_address()==base,"token move lost reservation");
    operation.cancel();moved.cancel();rejects([&]{(void)xo::finish_verified(moved);});rejects([&]{(void)moved.grow(1);});
    auto h=operation.header_bytes(),b=operation.payload_bytes();
    for(std::size_t i=0;i<h.size();++i)h[i]=std::byte(static_cast<unsigned char>(wire[i]));
    for(std::size_t i=0;i<b.size();++i)b[i]=std::byte(static_cast<unsigned char>(wire[h.size()+i]));
    operation.complete(h.size(),b.size());rejects([&]{(void)xo::finish_verified(moved);}); // cancel != successful completion
    {auto retry=moved.acquire_operation();retry.complete(sizeof(xo::XWireHeaderV1),payload);}
    auto output=xo::finish_verified(moved);need(output.get_address()==base,"cancel/recomplete changed owner");check(output.root());
    auto orphan_operation=[&]{auto temporary=xo::begin_verified_receive<ReceiveRoot>(payload);return temporary.acquire_operation();}();
    need(orphan_operation.payload_bytes().size()==payload,"destroying token released outstanding-operation mapping");
    orphan_operation.cancel();need(orphan_operation.payload_bytes().size()==payload,"cancel released outstanding mapping");
    orphan_operation.complete(0,0);need(orphan_operation.payload_bytes().empty(),"real completion retained operation lease");
    auto abandoned=xo::begin_verified_receive<ReceiveRoot>(payload,xo::XBuffer::maximum_capacity(payload+64));
    {auto lost=abandoned.acquire_operation();lost.cancel();}
    rejects([&]{(void)abandoned.grow(1);});rejects([&]{(void)xo::finish_verified(abandoned);});
    rejects([&]{(void)abandoned.acquire_operation();}); // destructor is not real completion
    auto short_input=xo::begin_verified_receive<ReceiveRoot>(payload);
    {auto part=short_input.acquire_operation();part.complete(sizeof(xo::XWireHeaderV1)-1,payload);}
    rejects([&]{(void)xo::finish_verified(short_input);});
}
struct Resource final:std::pmr::memory_resource{
    unsigned attempt=0,fail;std::size_t live=0;
    explicit Resource(unsigned p):fail(p){}
    void* do_allocate(std::size_t n,std::size_t a)override{if(attempt++==fail)throw std::bad_alloc();auto*p=std::pmr::new_delete_resource()->allocate(n,a);live+=n;return p;}
    void do_deallocate(void*p,std::size_t n,std::size_t a)override{std::pmr::new_delete_resource()->deallocate(p,n,a);live-=n;}
    bool do_is_equal(const std::pmr::memory_resource& p)const noexcept override{return this==&p;}
};
void rollback_and_rejection(){
    auto source=make_source();const auto wire=source.save_verified();const auto n=wire.size()-sizeof(xo::XWireHeaderV1);
    for(unsigned point=0;point<2;++point){
        auto token=receive(wire,xo::XBuffer::maximum_capacity(n+4096));const auto before=body_image(token);const auto*base=token.payload_address();
        host::remaining=point;bool failed=false;try{(void)xo::finish_verified(token);}catch(const xo::XBadAlloc&){failed=true;}
        host::remaining=-1;need(failed&&token.payload_address()==base&&token.received_length()==n&&token.maximum_capacity()==n+4096&&body_image(token)==before,"registry preparation failure lost token");
        auto out=xo::finish_verified(token);check(out.root());
    }
    for(unsigned point=0;point<2;++point){
        auto token=receive(wire);const auto before=body_image(token);const auto*base=token.payload_address();Resource resource(point);bool failed=false;
        try{(void)xo::finish_verified(token,&resource);}catch(const std::bad_alloc&){failed=true;}
        need(failed&&resource.live==0&&token.payload_address()==base&&body_image(token)==before&&token.received_length()==n,"raw scratch OOM lost token/bytes");
        auto out=xo::finish_verified(token);check(out.root());
    }
    auto token=receive(wire);const auto before=body_image(token);xo::detail::fail_next_registry_registration();
    rejects([&]{(void)xo::finish_verified(token);});need(body_image(token)==before&&token.received_length()==n,"registry injection rewrote raw token");
    xo::detail::clear_registry_failures();auto out=xo::finish_verified(token);check(out.root());
    for(unsigned kind=0;kind<4;++kind){
        auto malformed=wire;xo::XWireHeaderV1 h{};std::memcpy(&h,wire.data(),sizeof(h));
        if(kind==0)malformed[0]^=1;
        if(kind==1)malformed[sizeof(h)+h.root_offset+offsetof(ReceiveRoot,enabled)]=2;
        if(kind==2){auto arena=xo::XBufferCore::arena_header{};std::memcpy(&arena,malformed.data()+sizeof(h),sizeof(arena));arena.used_end+=1;std::memcpy(malformed.data()+sizeof(h),&arena,sizeof(arena));}
        if(kind==3){auto offset=sizeof(h)+h.root_offset+offsetof(ReceiveRoot,name)+offsetof(xo::XFixedString,arena_delta);std::int32_t bad=0;std::memcpy(malformed.data()+offset,&bad,sizeof(bad));}
        auto bad=receive(malformed);const auto image=body_image(bad);const auto*base=bad.payload_address();
        rejects([&]{(void)xo::finish_verified(bad);});need(bad&&bad.payload_address()==base&&body_image(bad)==image&&bad.received_length()==n,"malformed rejection consumed or changed token");
    }
    for(unsigned field=0;field<15;++field){
        auto malformed=wire;xo::XWireHeaderV1 h{};std::memcpy(&h,wire.data(),sizeof(h));
        switch(field){
        case 0:h.magic[0]^=1;break;case 1:++h.format_major;break;case 2:++h.format_minor;break;
        case 3:++h.header_size;break;case 4:++h.endian_tag;break;case 5:++h.flags;break;
        case 6:h.root_type_id^=1;break;case 7:h.schema_hash_lo^=1;break;case 8:h.schema_hash_hi=1;break;
        case 9:++h.used_bytes;break;case 10:h.reserved_bytes=n-1;break;
        case 11:h.crc32c=1;break;case 12:h.reserved0=1;break;
        case 13:h.root_offset=n;break;case 14:h.allocator_offset=n;break;
        }
        std::memcpy(malformed.data(),&h,sizeof(h));auto bad=receive(malformed);
        const auto image=body_image(bad);const auto* base=bad.payload_address();
        rejects([&]{(void)xo::finish_verified(bad);});
        need(bad&&bad.payload_address()==base&&body_image(bad)==image&&bad.received_length()==n,
             "wire header rejection mutated/consumed token");
        auto observation=bad.acquire_operation();
        need(std::memcmp(observation.header_bytes().data(),malformed.data(),sizeof(h))==0,
             "wire header rejection changed retained header bytes");
        observation.complete(sizeof(h),n);
    }
}
Token* reentry_token=nullptr;Token* reentry_moved=nullptr;
const std::string* replacement_wire=nullptr;
bool rejected_reentry=false,unpublished=false;
void callback(){
    try{(void)reentry_token->grow(1);}catch(const xo::XException&){rejected_reentry=true;}
    try{(void)static_cast<xo::XArena*>(const_cast<void*>(reentry_token->payload_address()))->freelist_runtime_owner();}
    catch(const xo::XException&){unpublished=true;}
    auto other=xo::XBuffer::create<ReceiveRoot>(4096); // registry is not locked across callback
}
void cancel_callback(){reentry_token->cancel();}
void move_token_callback(){*reentry_moved=std::move(*reentry_token);}
void replace_token_callback(){*reentry_token=receive(*replacement_wire);}
void destroy_token_callback(){delete reentry_token;reentry_token=nullptr;}
void vm_failures_and_new_handler(){
    auto source=make_source();auto wire=source.save_verified();const auto n=wire.size()-sizeof(xo::XWireHeaderV1);
    for(auto op:{xo::detail::VmOperation::Reserve,xo::detail::VmOperation::Enable,xo::detail::VmOperation::Prepare}){
        xo::detail::fail_next_vm_operation(op);bool failed=false;
        try{(void)xo::begin_verified_receive<ReceiveRoot>(n);}catch(const std::bad_alloc&){failed=true;}
        xo::detail::clear_vm_failures();need(failed,"VM begin failure not propagated");
    }
    auto token=receive(wire);const auto*base=token.payload_address();reentry_token=&token;
    host::remaining=0;host::callback=callback;host::callbacks=0;
    auto out=xo::finish_verified(token);host::remaining=-1;host::callback=nullptr;
    need(host::callbacks==1&&rejected_reentry&&unpublished&&out.get_address()==base,"new-handler reentry/owner publication not isolated");check(out.root());
    auto cancelled=receive(wire);const auto cancelled_image=body_image(cancelled);reentry_token=&cancelled;
    host::remaining=0;host::callback=cancel_callback;
    rejects([&]{(void)xo::finish_verified(cancelled);});host::remaining=-1;host::callback=nullptr;
    need(cancelled&&body_image(cancelled)==cancelled_image,"callback cancellation consumed/changed input");
    {auto completion=cancelled.acquire_operation();completion.complete(sizeof(xo::XWireHeaderV1),n);}
    auto uncancelled=xo::finish_verified(cancelled);check(uncancelled.root());
    auto original=receive(wire);Token moved;const auto* original_base=original.payload_address();
    reentry_token=&original;reentry_moved=&moved;
    host::remaining=0;host::callback=move_token_callback;
    auto moved_result=xo::finish_verified(original);host::remaining=-1;host::callback=nullptr;
    need(!original&&!moved&&moved_result.get_address()==original_base,
         "callback moved token did not observe consumed State");check(moved_result.root());
    rejects([&]{(void)moved.acquire_operation();});rejects([&]{(void)moved.grow(0);});
    rejects([&]{(void)xo::finish_verified(moved);});
    auto replaced=receive(wire);const auto* replaced_base=replaced.payload_address();
    reentry_token=&replaced;replacement_wire=&wire;host::remaining=0;host::callback=replace_token_callback;
    auto replaced_result=xo::finish_verified(replaced);host::remaining=-1;host::callback=nullptr;
    need(replaced&&replaced.payload_address()!=replaced_base&&replaced_result.get_address()==replaced_base,
         "finish consumed callback's unrelated replacement token");check(replaced_result.root());
    auto replacement_result=xo::finish_verified(replaced);check(replacement_result.root());
    auto* destroyed=new Token(receive(wire));const auto* destroyed_base=destroyed->payload_address();
    reentry_token=destroyed;host::remaining=0;host::callback=destroy_token_callback;
    auto destroyed_result=xo::finish_verified(*destroyed);host::remaining=-1;host::callback=nullptr;
    need(reentry_token==nullptr&&destroyed_result.get_address()==destroyed_base,
         "finish failed after callback destroyed original Token object");check(destroyed_result.root());
}

void canonical_external_heads_and_many_owners(){
    auto source=make_source();const auto wire=source.save_verified();
    const auto n=wire.size()-sizeof(xo::XWireHeaderV1);
    const auto capacity=xo::XBuffer::maximum_capacity(n+65536).with_growth(xo::XGrowPolicy::Automatic);
    std::array<std::optional<xo::TypedXBuffer<ReceiveRoot>>,7> owners;
    for(auto& slot:owners){auto token=receive(wire,capacity);slot.emplace(xo::finish_verified(token));}
    // Seven domains exceed the four-record TLS fast path. All owners stay live
    // while root/data/binding and allocator free-list addresses alternate.
    for(unsigned round=0;round<4;++round)for(unsigned i=0;i<owners.size();++i){
        auto& owner=*owners[(i*3+round)%owners.size()];
        check(owner.root());const auto a=owner.root().tail.binding();
        auto* p=xo::fixed_detail::allocate_dynamic_bytes(a,24,8);
        xo::fixed_detail::retire_dynamic_bytes(a,p,24);
        auto* q=xo::fixed_detail::allocate_dynamic_bytes(a,24,8);
        need(p==q,"same-address free-list reuse lost canonical allocator provenance");
        xo::fixed_detail::retire_dynamic_bytes(a,q,24);
    }
    auto moved=std::move(*owners[0]);owners[0].reset();
    std::swap(moved,*owners[1]);owners[0].emplace(std::move(moved));
    for(auto& owner:owners)check(owner->root());
    owners[3].reset(); // dead record may still occupy a TLS slot
    {auto token=receive(wire,capacity);owners[3].emplace(xo::finish_verified(token));}
    for(auto& owner:owners)check(owner->root());

    auto normal=xo::TypedXBuffer<ReceiveRoot>::create(16384,capacity);
    auto& target=*owners[2];const auto binding=target.root().name.binding();
    auto* text_slot=normal.arena()->allocate_aligned(sizeof(xo::XString),alignof(xo::XString));
    auto* vector_slot=normal.arena()->allocate_aligned(sizeof(xo::XVector<std::uint32_t>),alignof(xo::XVector<std::uint32_t>));
    auto rel32=[](std::uintptr_t from,std::uintptr_t to){
        const auto positive_limit=std::uint64_t((std::numeric_limits<std::int32_t>::max)());
        return to>=from?std::uint64_t(to-from)<=positive_limit:
                        std::uint64_t(from-to)<=positive_limit+1;
    };
    auto require_head_domain=[&](const void* slot,const auto& owner,const void* state){
        const auto head=reinterpret_cast<std::uintptr_t>(slot);
        const auto base=reinterpret_cast<std::uintptr_t>(owner.get_address());
        const auto last=base+owner.maximum_capacity()-1;
        need(rel32(head,reinterpret_cast<std::uintptr_t>(state))&&
             rel32(head+4,base)&&rel32(head+4,last),
             "external-head fixture mapping does not satisfy checked rel32 domain");
    };
    require_head_domain(text_slot,target,binding.state);
    require_head_domain(vector_slot,target,binding.state);
    // Both constructors check the real rel32 distance. These heads are live in
    // a distinct normal arena; they are not forged within received byte storage.
    auto* text=::new(text_slot) xo::XString(binding);
    auto* values=::new(vector_slot) xo::XVector<std::uint32_t>(binding);
    try{
        text->assign("external normal-arena head");values->resize(4);(*values)[2]=77;
        need(text->view()=="external normal-arena head"&&(*values)[2]==77&&
             text->binding().state==binding.state&&values->binding().state==binding.state,
             "normal-anchor to received-target canonical resolution failed");
        auto destination=std::move(target);target=std::move(destination);
        text->assign("external normal-arena head retained");values->push_back(91);
        need(text->view()=="external normal-arena head retained"&&values->back()==91,
             "external head lost binding after received Core move");
    }catch(...){std::destroy_at(values);std::destroy_at(text);throw;}
    const auto before=binding.state->free_bytes;
    std::destroy_at(values);std::destroy_at(text);
    need(binding.state->free_bytes>before,"external head destruction did not retire received allocations");

    // Distinct C anchor A must not force target B through A's byte origin.
    const auto second_binding=owners[5]->root().name.binding();
    auto* cross_slot=owners[4]->arena()->allocate_aligned(sizeof(xo::XString),alignof(xo::XString));
    require_head_domain(cross_slot,*owners[5],second_binding.state);
    auto* cross=::new(cross_slot) xo::XString(second_binding);
    try{
        cross->assign("cross-C allocator target");
        need(cross->view()=="cross-C allocator target"&&cross->binding().state==second_binding.state,
             "received anchor A incorrectly selected its origin for received target B");
        cross->assign("cross-C allocator target live");
        need(cross->view()=="cross-C allocator target live","cross-C payload access/growth failed");
    }catch(...){std::destroy_at(cross);throw;}
    const auto second_before=second_binding.state->free_bytes;
    std::destroy_at(cross);
    need(second_binding.state->free_bytes>second_before,"cross-C head did not retire target B payload");
}

void custom_future_lifetimes(){
    namespace ex=receive_custom_v2_examples;
    auto source=xo::TypedXBuffer<CustomReceiveRoot>::create(65536,
        xo::XBuffer::maximum_capacity(262144).with_growth(xo::XGrowPolicy::Automatic));
    source.root().numbers.resize(3);
    for(unsigned i=0;i<3;++i)source.root().numbers[i].value=40+i;
    source.root().attrs.resize(2);
    for(unsigned i=0;i<2;++i){source.root().attrs[i].text="owned custom";source.root().attrs[i].value=100+i;}
    const auto wire=source.save_verified();const auto n=wire.size()-sizeof(xo::XWireHeaderV1);
    auto token=receive_as<CustomReceiveRoot>(wire,
        xo::XBuffer::maximum_capacity(n+131072).with_growth(xo::XGrowPolicy::Automatic));
    const auto* base=token.payload_address();
    auto output=xo::finish_verified(token);auto& r=output.root();
    need(output.get_address()==base&&r.numbers[1].value==41&&r.attrs[1].value==101&&
         r.attrs[1].text=="owned custom","custom initial canonical activation differs");
    need(output.save_verified()==wire,"custom initial activation changed complete wire bytes");
    auto* child=r.attrs[0].text.data();
    r.attrs.reserve(16); // parent fresh/relocate; string payload remains live
    need(r.attrs[0].text.data()==child&&r.attrs[0].text=="owned custom",
         "different-address custom relocation reconstructed out-of-line ownership");
    r.numbers.reserve(16);auto* tail=r.numbers.data();
    r.numbers.reserve(32); // successful actual last-allocation physical expansion
    need(r.numbers.data()==tail&&r.numbers[2].value==42,
         "same-address custom prefix restart lost values or tail address");
    for(long point=0;point<3;++point){
        const auto* old=r.numbers.data();const auto oldcap=r.numbers.capacity();
        ex::transfer_failure=point;bool failed=false;
        try{r.numbers.resize(36,r.numbers[0]);}catch(const std::bad_alloc&){failed=true;}
        ex::transfer_failure=-1;
        need(failed&&r.numbers.size()==3&&r.numbers.capacity()==oldcap&&r.numbers.data()==old&&
             r.numbers[0].value==40&&r.numbers[2].value==42,
             "custom throwing tail transfer damaged original prefix/descriptor");
    }
    r.numbers.resize(36,r.numbers[0]);
    need(r.numbers[35].value==40,"custom self fill source was not read before prefix lifetime change");
    for(long point=0;point<3;++point){
        const auto* old=r.attrs.data();const auto oldcap=r.attrs.capacity();
        ex::transfer_failure=point;bool failed=false;
        try{r.attrs.resize(20,r.attrs[0]);}catch(const std::bad_alloc&){failed=true;}
        ex::transfer_failure=-1;
        need(failed&&r.attrs.size()==2&&r.attrs.capacity()==oldcap&&r.attrs.data()==old&&
             r.attrs[0].text.data()==child&&r.attrs[0].text=="owned custom"&&r.attrs[1].value==101,
             "custom nested copy constructor rollback changed old ownership/prefix");
    }
    {const auto* old=r.attrs.data();const auto oldcap=r.attrs.capacity();bool failed=false;
     xo::detail::fail_next_vm_operation(xo::detail::VmOperation::Prepare);
     try{r.attrs.resize(2048,r.attrs[0]);}catch(const xo::XBadAlloc&){failed=true;}
     xo::detail::clear_vm_failures();
     need(failed&&r.attrs.size()==2&&r.attrs.capacity()==oldcap&&r.attrs.data()==old&&
          r.attrs[0].text.data()==child,"custom arena growth failure damaged old ownership");}
    r.attrs.resize(5,r.attrs[0]);r.attrs.erase(r.attrs.begin());
    need(r.attrs.size()==4&&r.attrs.back().text=="owned custom"&&r.attrs.back().value==100,
         "custom fill/erase failed ownership or source lifetime");
    auto final_wire=output.save_verified();
    auto final_token=receive_as<CustomReceiveRoot>(final_wire);
    auto reread=xo::finish_verified(final_token);
    need(reread.root().numbers.back().value==40&&reread.root().attrs.back().text=="owned custom",
         "custom mutated V1 fresh raw-reader semantic check failed");

    auto malformed=wire;xo::XWireHeaderV1 h{};std::memcpy(&h,wire.data(),sizeof(h));
    const auto head=h.root_offset+offsetof(CustomReceiveRoot,attrs);
    xo::detail::RawOwningDescriptor attrs{};
    std::memcpy(&attrs,wire.data()+sizeof(h)+head,sizeof(attrs));
    const auto offset=std::size_t(std::int64_t(head+4)+attrs.data_delta);
    const std::int32_t invalid=0;
    std::memcpy(malformed.data()+sizeof(h)+offset,&invalid,sizeof(invalid));
    auto rejected=receive_as<CustomReceiveRoot>(malformed);
    const auto before=std::string(static_cast<const char*>(rejected.payload_address()),rejected.payload_size());
    rejects([&]{(void)xo::finish_verified(rejected);});
    need(rejected&&std::memcmp(rejected.payload_address(),before.data(),before.size())==0,
         "custom canonical malformed-child failure changed raw bytes before publication");
}
#endif
}
#if !defined(_WIN32)
// Fork policy (registry and VM pool have no atfork handlers): with no other
// thread inside the library at fork time, a child can load ordinary buffers,
// receive, and read buffers received before the fork; the parent's buffer is
// unaffected. A child of a parent with concurrent library activity may only exec.
void fork_after_receive(){
    auto source=make_source();const auto wire=source.save_verified();
    auto token=receive(wire);auto parent=xo::finish_verified(token);check(parent.root());
    const pid_t pid=::fork();need(pid>=0,"fork failed");
    if(pid==0){
        int code=0;
        try{
            auto loaded=xo::TypedXBuffer<ReceiveRoot>::load_verified(wire);check(loaded.root());
            const auto cap=xo::XBuffer::maximum_capacity(wire.size()+65536).with_growth(xo::XGrowPolicy::Automatic);
            auto child_token=receive(wire,cap);auto child=xo::finish_verified(child_token);check(child.root());
            check(parent.root());exercise_edit(child);
            if(child.get_address()==parent.get_address())code=2;
        }catch(...){code=1;}
        std::_Exit(code);
    }
    int status=0;
    need(::waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"forked child could not load or receive");
    check(parent.root());
}
#endif
int main(int argc,char**argv){try{
    if(argc==3&&std::string_view(argv[1])=="--write-wire"){
        auto source=make_source();auto wire=source.save_verified();std::ofstream f(argv[2],std::ios::binary);f.write(wire.data(),wire.size());need(bool(f),"write failed");return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--read-wire"){
        std::ifstream f(argv[2],std::ios::binary);std::string wire((std::istreambuf_iterator<char>(f)),{});
#if defined(XOFFSET_VERIFIED_RECEIVE_API)
        auto token=receive(wire);auto result=xo::finish_verified(token);check(result.root());
#else
        auto result=xo::TypedXBuffer<ReceiveRoot>::load_verified(wire);check(result.root());
#endif
        return 0;
    }
#if defined(XOFFSET_VERIFIED_RECEIVE_API)
    stage="direct-growth-lifetime";direct_growth_and_lifetime();stage="completion-cancel-owner";completion_cancel_and_owner_lifetime();
    stage="rollback-rejection";rollback_and_rejection();stage="VM-newhandler";vm_failures_and_new_handler();
    stage="canonical-external-many";canonical_external_heads_and_many_owners();
    stage="custom-future";custom_future_lifetimes();
#if !defined(_WIN32)
    stage="fork";fork_after_receive();
#endif
    std::cout<<"verified receive: six groups, token/canonical/raw/custom/lifetime/rollback/growth/cancel/newhandler/V1 passed\n";
#else
    throw std::runtime_error("full tests need receive API; baseline supports V1 CLI only");
#endif
    return 0;
}catch(const std::exception&e){host::remaining=-1;host::callback=nullptr;xo::detail::clear_vm_failures();xo::detail::clear_registry_failures();std::cerr<<stage<<": "<<e.what()<<'\n';return 1;}}
