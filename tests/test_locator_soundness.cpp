// Concrete counterexamples to incomplete mutable locator witnesses. No timer.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace xo=XOffsetDatastructure;
namespace fd=xo::fixed_detail;
struct Node { std::uint32_t id{};xo::XVector<Node> children; };
namespace boost::typelayout::v1 {
template<>struct is_byte_copy_safe<Node>:std::true_type{};
}
struct Forest {xo::XVector<Node> left,right;xo::XVector<std::byte> opaque;};
namespace {
void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class H> auto descriptor(const H& head){
    static_assert(sizeof(H)==16);std::array<std::byte,16> bytes{};
    std::memcpy(bytes.data(),&head,16);return bytes;
}
void ancestor_change_without_local_witness_change(){
    auto buffer=xo::XBuffer::create<Forest>(65536,
        xo::XBuffer::maximum_capacity(1u<<20).with_growth(xo::XGrowPolicy::Automatic));
    auto& r=buffer.root<Forest>();r.left.resize(1);r.right.resize(1);r.right[0].children.resize(1);
    auto& a=r.left;
    auto& immediate_parent=r.right[0].children;
    auto* b=&immediate_parent[0].children;
    const auto ad=descriptor(a),pd=descriptor(immediate_parent),bd=descriptor(*b);
    const auto right_before=descriptor(r.right);
    // V2 has no in-arena allocator state (and no state epoch). Its allocator
    // generation is the arena tail plus the host free table: a steal must
    // neither allocate (used_end) nor retire (free blocks).
    const auto core_epoch=buffer.epoch();const auto used_end=buffer.arena()->used_end;
    const auto free_blocks=buffer.free_block_count();const auto free_bytes=buffer.free_block_bytes();
    need(a.arena()==buffer.arena(),"fixture head is not bound to the payload base");
    need(!fd::move_assignment_owns_address(a,b),"initial source predicate should be negative");
    // Public move to an empty same-arena destination steals without allocating
    // or retiring. B's complete immediate parent allocation stays at the same
    // address and retains its descriptor.
    r.left[0].children=std::move(r.right);
    need(descriptor(a)==ad && descriptor(immediate_parent)==pd && descriptor(*b)==bd,
         "local witness fixture changed");
    need(buffer.epoch()==core_epoch && buffer.arena()->used_end==used_end &&
         buffer.free_block_count()==free_blocks && buffer.free_block_bytes()==free_bytes,
         "unexpected generation change (allocation or retirement)");
    need(descriptor(r.right)!=right_before,"full ancestor witness must detect moved root.right");
    need(fd::move_assignment_owns_address(a,b),"ordinary API changed negative to positive ownership");
    // This is a local finite tree test, not a recursive verified-wire schema.
}
void byte_placement_is_not_a_declared_subobject(){
    auto buffer=xo::XBuffer::create<Forest>(65536,
        xo::XBuffer::maximum_capacity(1u<<20).with_growth(xo::XGrowPolicy::Automatic));
    auto& bytes=buffer.root<Forest>().opaque;bytes.resize(64);
    using Head=xo::XVector<std::uint8_t>;
    need(reinterpret_cast<std::uintptr_t>(bytes.data())%alignof(Head)==0,"byte payload alignment fixture");
    auto* placed=std::construct_at(reinterpret_cast<Head*>(bytes.data()),bytes.binding());
    need(fd::move_assignment_head_in_arena(placed,sizeof(Head),bytes.arena()),"placed head is in the same arena");
    need(fd::move_assignment_owns_address(bytes,placed),"byte owner must detect the embedded head");
    // Same allocator, in-arena address and nonrecursive byte element type are
    // insufficient negative evidence. An index must classify this domain as
    // unknown rather than claim the placed head is a reflected owning member.
    std::destroy_at(placed);
}
}
int main(){try{
    ancestor_change_without_local_witness_change();byte_placement_is_not_a_declared_subobject();
    std::cout<<"locator soundness: unchanged local parents/epoch and byte-placement counterexamples passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
