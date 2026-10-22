// Separate capability probe, not a main-track graph workload or performance run.
#include <cista/serialization.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
struct Node {
    std::uint64_t id{};
    std::uint64_t value{};
    cista::offset::ptr<Node> next;
};
struct Graph {
    // This owner registers its element address range with the serializer.
    // The aliases/cycle below are non-owning pointers to those owned elements.
    cista::offset::indexed_vector<Node> nodes;
    cista::offset::ptr<Node> first;
    cista::offset::ptr<Node> alias;
    cista::offset::ptr<Node> second;
};
constexpr auto mode = cista::mode::NONE;

void need(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T> void inside(const cista::byte_buf& bytes, const T* object) {
    const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
    const auto at = reinterpret_cast<std::uintptr_t>(object);
    need(object != nullptr && bytes.size() >= sizeof(T) && at >= begin &&
         at - begin <= bytes.size() - sizeof(T), "object is outside live wire owner");
}

void shape(const Graph* graph, const cista::byte_buf& owner,
           std::uint64_t first_value, std::uint64_t second_value) {
    inside(owner, graph);
    need(graph->nodes.size() == 2, "owned node count");
    const Node* a = graph->first.get();
    const Node* alias = graph->alias.get();
    const Node* b = graph->second.get();
    inside(owner, graph->nodes.data());
    inside(owner, graph->nodes.data() + 1);
    inside(owner, a);
    inside(owner, b);
    need(a == alias && a == graph->nodes.data() && b == graph->nodes.data() + 1,
         "native shared identity not preserved");
    need(a->id == 1 && b->id == 2 && a->value == first_value && b->value == second_value,
         "node values");
    // Bounded traversal proves a two-node native pointer cycle. It never follows
    // attacker-provided pointers; input is produced by this trusted probe.
    need(a->next.get() == b && b->next.get() == a && a->next->next.get() == a,
         "native two-node cycle not preserved");
}

cista::byte_buf make_wire() {
    Graph original;
    original.nodes.reserve(2);
    original.nodes.emplace_back(Node{1, 10, nullptr});
    original.nodes.emplace_back(Node{2, 20, nullptr});
    original.first = &original.nodes[0];
    original.alias = &original.nodes[0];
    original.second = &original.nodes[1];
    original.nodes[0].next = &original.nodes[1];
    original.nodes[1].next = &original.nodes[0];
    return cista::serialize<mode>(original);
    // original owns its node allocation; it is destroyed on return. No pointer
    // owner/cycle cleanup is delegated to the serialized buffer.
}

void borrowed_read_and_mutable_checkpoint() {
    cista::byte_buf checkpoint;
    {
        auto input = make_wire();
        const auto before = input;
        {
            const auto* from = static_cast<const std::uint8_t*>(input.data());
            const auto* borrowed = cista::offset::deserialize<Graph, mode>(from, from + input.size());
            shape(borrowed, input, 10, 20);
            need(input == before, "borrowed const reading modified source bytes");
            // input remains alive throughout this view's lifetime. This case
            // establishes borrowed read, not independent owning load or mmap.
        }
        auto* mutable_graph = cista::offset::deserialize<Graph, mode>(input);
        mutable_graph->alias->value = 99;
        shape(mutable_graph, input, 99, 20);
        need(mutable_graph->first->value == 99 &&
             mutable_graph->second->next->value == 99,
             "alias mutation is not visible through all paths");
        checkpoint = cista::serialize<mode>(*mutable_graph);
    }
    // Both the native source and the first loaded blob have gone away.
    auto* reloaded = cista::offset::deserialize<Graph, mode>(checkpoint);
    shape(reloaded, checkpoint, 99, 20);
    reloaded->second->value = 77;
    need(reloaded->first->next->value == 77, "post-reload alias mutation");
    auto next_checkpoint = cista::serialize<mode>(*reloaded);
    cista::byte_buf{}.swap(checkpoint);
    // A new independent wire copy relocates all fields and pointer targets.
    auto relocated = next_checkpoint;
    cista::byte_buf{}.swap(next_checkpoint);
    const auto* final = cista::offset::deserialize<Graph, mode>(relocated);
    shape(final, relocated, 99, 77);
    // Loaded nodes contain only scalars/non-owning pointers. No modification
    // here allocated container children, so the blob is the sole loaded owner.
}
} // namespace

int main() {
    try {
        borrowed_read_and_mutable_checkpoint();
        std::cout << "{\"schema_version\":1,\"library\":\"cista-v0.16\","
                     "\"commit\":\"015b94e66c4c0d7c1ef29742383a024c84fbbba1\","
                     "\"mode\":\"NONE\",\"borrowed_read\":\"passed\","
                     "\"native_alias\":\"passed\",\"native_cycle\":\"passed\","
                     "\"alias_mutation_reencode_reload\":\"passed\","
                     "\"ownership\":\"indexed-vector-owned-targets;nonowning-offset-links\","
                     "\"scope\":\"trusted;scalar-update;independent-byte-checkpoints;no-resize-or-mmap-or-durability-claim\","
                     "\"performance_measurement\":false}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cista capability probe failed: " << error.what() << '\n';
        return 1;
    }
}
