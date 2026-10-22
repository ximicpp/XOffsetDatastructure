// Capability probes, not performance. Select exactly one fixed-version library.
#include "model.hpp"
#include <iostream>
#include <memory>
#include <sstream>

#if defined(SC_CAPABILITY_CAPNP) == defined(SC_CAPABILITY_CEREAL)
#error Select exactly one SC_CAPABILITY_CAPNP or SC_CAPABILITY_CEREAL
#endif

namespace {
void need(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F> bool rejected(F&& f){try{f();return false;}catch(...){return true;}}
}

#ifdef SC_CAPABILITY_CAPNP
#include "adapters/capnproto/adapter.hpp"
template<class T> concept HasSetEpoch=requires(T t){t.setEpoch(1);};
static_assert(!HasSetEpoch<sc_cap::Snapshot::Reader>);
namespace {
void probe(){
    sc::Snapshot expected;expected.epoch=9;sc::Record r;r.id=1;r.name="borrowed-name";r.samples={1,2,0xffffffff};r.payload={0,255};r.attrs={{"key",{0,127,255}}};expected.records.push_back(r);
    auto builder=sc::CapnProtoAdapter::build(expected);auto wire=sc::CapnProtoAdapter::emit(builder);builder={};
    auto hash=sc::sha256_hex(sc::wire_view(wire));
    capnp::ReaderOptions limits;limits.traversalLimitInWords=sc::wire_limit/sizeof(capnp::word);limits.nestingLimit=64;
    {
        capnp::FlatArrayMessageReader borrowed(wire.asPtr(),limits);auto root=borrowed.getRoot<sc_cap::Snapshot>();
        auto view=sc::cap_view::SnapshotView{root};need(sc::export_view(view)==expected,"Capnp borrowed complete read");
        const auto start=reinterpret_cast<std::uintptr_t>(wire.asBytes().begin()),end=start+wire.asBytes().size();
        const auto ptr=reinterpret_cast<std::uintptr_t>(root.getRecords()[0].getName().cStr());
        need(ptr>=start&&ptr<end&&root.getRecords()[0].getName().size()<=end-ptr,"Capnp Reader string must borrow wire");
        std::vector<sc::Query> qs={{0,sc::QueryCode::record,1,{}},{1,sc::QueryCode::attr,1,"key"},{2,sc::QueryCode::scan,0,{}}};
        std::vector<sc::QueryResult> results(qs.size());sc::query_view(view,qs,results);
        need(results==sc::query_reference(expected,qs),"Capnp borrowed native query");
    } // Reader destroyed first; its input owner still lives. Never dereference a dangling Reader.
    need(sc::sha256_hex(sc::wire_view(wire))==hash,"borrowed read changed input");
    const bool truncated=rejected([&]{capnp::FlatArrayMessageReader bad(wire.asPtr().slice(0,wire.size()-1),limits);(void)bad.getRoot<sc_cap::Snapshot>().getEpoch();});
    auto malicious=kj::heapArray<capnp::word>(1);std::memset(malicious.begin(),0,sizeof(capnp::word));
    auto* bytes=reinterpret_cast<std::uint8_t*>(malicious.begin());for(unsigned i=4;i<8;++i)bytes[i]=255;
    const bool oversized_segment=rejected([&]{capnp::FlatArrayMessageReader bad(malicious.asPtr(),limits);(void)bad.getRoot<sc_cap::Snapshot>().getEpoch();});
    need(truncated&&oversized_segment,"selected Capnp framing negatives");
    std::cout<<"{\"schema\":\"sc-capability-v1\",\"library\":\"capnproto-v1.5.0\",\"commit\":\"373e61ec89e2359f1c362e9b2eadc552f4779306\",\"borrowed_read\":\"passed_with_live_aligned_input\",\"input_unchanged\":true,\"query_and_full_state\":\"passed\",\"native_shared_identity\":\"unsupported_tree_object_API\",\"native_cycles\":\"unsupported_tree_object_API\",\"selected_negative_cases\":2,\"selected_negative_rejected\":2,\"untrusted_mutable\":\"not_established\",\"timing_samples\":0}\n";
}
}
#endif

#ifdef SC_CAPABILITY_CEREAL
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
namespace {
struct Node {
    static inline int live=0;
    std::int32_t value=0;std::shared_ptr<Node> next;
    Node(){++live;}~Node(){--live;}
    Node(const Node&)=delete;Node& operator=(const Node&)=delete;
    template<class Archive> void serialize(Archive& a){a(value,next);}
};
struct Graph {
    std::shared_ptr<Node> left,right;
    // These probes construct a DAG or a single self-cycle only. Break the
    // explicitly owned strong cycle before RAII owners die, including failures.
    ~Graph(){clear();}
    void clear() noexcept {if(left)left->next.reset();if(right)right->next.reset();left.reset();right.reset();}
    template<class Archive> void serialize(Archive& a){a(left,right);}
};
std::string save(const Graph& graph){std::ostringstream output(std::ios::binary|std::ios::out);{cereal::BinaryOutputArchive archive(output);archive(graph);}return std::move(output).str();}
void load(const std::string& wire,Graph& graph){std::istringstream input(wire,std::ios::binary|std::ios::in);cereal::BinaryInputArchive archive(input);archive(graph);}
struct AllocationObservation {static inline std::size_t requested=0;};
template<class T>struct CountingAllocator {
    using value_type=T;CountingAllocator()=default;template<class U>CountingAllocator(const CountingAllocator<U>&){}
    T* allocate(std::size_t n){AllocationObservation::requested+=n;return std::allocator<T>{}.allocate(n);}
    void deallocate(T* p,std::size_t n){std::allocator<T>{}.deallocate(p,n);}
    template<class U>bool operator==(const CountingAllocator<U>&)const{return true;}
};
void probe(){
    need(Node::live==0,"initial Node ownership");std::string dag_wire,cycle_wire;
    {
        Graph original;original.left=std::make_shared<Node>();original.left->value=7;original.right=original.left;
        dag_wire=save(original);original.clear();need(Node::live==0,"DAG original destroyed before reload");
        Graph restored;load(dag_wire,restored);need(restored.left&&restored.left==restored.right,"cereal shared_ptr identity");
        restored.left->value=77;need(restored.right->value==77,"DAG mutation propagation");
        auto second=save(restored);restored.clear();need(Node::live==0,"DAG reload state cleanup");
        Graph again;load(second,again);need(again.left==again.right&&again.right->value==77,"DAG second checkpoint identity");
    }
    need(Node::live==0,"DAG final cleanup");
    {
        Graph original;original.left=std::make_shared<Node>();original.left->value=8;original.left->next=original.left;original.right=original.left;
        cycle_wire=save(original);original.clear();need(Node::live==0,"cycle original explicitly broken before reload");
        Graph restored;load(cycle_wire,restored);need(restored.left==restored.right&&restored.left->next==restored.left,"self-cycle identity");
        restored.left->next->value=99;need(restored.right->value==99,"self-cycle mutation propagation");
        auto second=save(restored);restored.clear();need(Node::live==0,"cycle reload explicitly broken");
        Graph again;load(second,again);need(again.left==again.left->next&&again.right->value==99,"cycle second checkpoint identity");
    }
    need(Node::live==0,"cycle final cleanup");
    std::ostringstream scalar_output(std::ios::binary|std::ios::out);{cereal::BinaryOutputArchive archive(scalar_output);archive(std::uint64_t{123});}
    auto short_wire=scalar_output.str();short_wire.pop_back();
    const bool truncated=rejected([&]{std::istringstream input(short_wire,std::ios::binary|std::ios::in);cereal::BinaryInputArchive archive(input);std::uint64_t value;archive(value);});
    std::string fake_count(8,'\0');fake_count[0]=64;AllocationObservation::requested=0;
    const bool bad_length=rejected([&]{std::istringstream input(fake_count,std::ios::binary|std::ios::in);cereal::BinaryInputArchive archive(input);std::vector<std::uint8_t,CountingAllocator<std::uint8_t>> values;archive(values);});
    need(truncated&&bad_length,"selected cereal negatives");
    need(Node::live==0,"all tracked graph objects destroyed");
    std::cout<<"{\"schema\":\"sc-capability-v1\",\"library\":\"cereal-v1.3.2\",\"commit\":\"ebef1e929807629befafbb2918ea1a08c7194554\",\"shared_ptr_DAG_identity\":\"passed\",\"self_cycle_identity\":\"passed\",\"mutation_propagation_after_reload\":\"passed\",\"explicit_cycle_cleanup\":true,\"live_nodes_after_cleanup\":"<<Node::live<<",\"shared_ptr_aliasing_constructor\":\"unsupported_not_claimed\",\"raw_pointer_serialization\":\"unsupported_not_claimed\",\"selected_negative_cases\":2,\"selected_negative_rejected\":2,\"observed_vector_elements_allocated_before_short_payload_rejection\":"<<AllocationObservation::requested<<",\"untrusted_mutable\":\"not_established\",\"timing_samples\":0}\n";
}
}
#endif

int main(){try{probe();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}catch(...){std::cerr<<"capability probe error\n";return 1;}}
