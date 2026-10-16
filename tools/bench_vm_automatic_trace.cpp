// Generated-header diagnostics are deliberately separate from the timed binary.
#include <cstdint>
#include <vector>
#include <stdexcept>
namespace vm_trace {
struct Event { const char* name; std::uint64_t op, lane, a, b, c, d; };
inline std::vector<Event> events;
inline std::uint64_t operation{}, lane{};
inline bool active{};
inline void emit(const char* name, std::uint64_t a, std::uint64_t b,
                 std::uint64_t c, std::uint64_t d) {
    if (!active) return;
    if (events.size() == events.capacity()) throw std::runtime_error("diagnostic log exhausted");
    events.push_back({name, operation, lane, a, b, c, d});
}
}
#define XOFFSET_BENCH_VM 1
#define main historical_benchmark_main
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#include "bench_vm_compare.cpp"
#pragma clang diagnostic pop
#undef main

#ifndef XOFFSET_TRACE_DIAGNOSTIC
#define XOFFSET_TRACE_DIAGNOSTIC 0
#endif

// Identical member layout to the historical FollowupRoot, including unused rows.
struct AutomaticTraceRoot {
    std::uint64_t marker{};
    XVector<std::uint32_t> values;
    XVector<std::uint32_t> other;
    XVector<XVector<std::uint32_t>> rows;
};
XOFFSET_REGISTER_SCHEMA_NAME(AutomaticTraceRoot, "bench.vm-automatic-trace.Root.v1")

namespace automatic_trace {
struct Config {
    std::string mode{"automatic"};
    std::size_t count{1000000}, initial{4096}, maximum{32 * MiB}, cadence{256}, burst{1}, lifetime{};
};
struct Op { std::uint32_t kind, value; }; // 0 primary append, 1 secondary append, 2 destroy/recreate secondary.
void hash(std::uint64_t& out, std::uint64_t x) { out = (out ^ x) * 1099511628211ULL; }
std::vector<Op> make_trace(const Config& c, std::uint64_t& checksum,
                         std::vector<std::uint32_t>& expected_other) {
    std::vector<Op> ops;
    const auto waves = c.cadence ? (c.count + c.cadence - 1) / c.cadence : 0;
    require(c.burst <= 4096 && waves <= 16000000 / std::max<std::size_t>(1, c.burst), "trace too large");
    ops.reserve(c.count + waves * (c.burst + 1));
    std::size_t wave = 0, secondary = 0;
    for (std::size_t i = 0; i < c.count; ++i) {
        ops.push_back({0, value_at(i)});
        if (c.cadence && i % c.cadence == 0) {
            if (c.lifetime && wave && wave % c.lifetime == 0) {
                ops.push_back({2, 0}); expected_other.clear();
            }
            for (std::size_t j = 0; j < c.burst; ++j) {
                auto value = value_at(secondary++);
                ops.push_back({1, value}); expected_other.push_back(value);
            }
            ++wave;
        }
    }
    for (const auto& op : ops) { hash(checksum, op.kind); hash(checksum, op.value); }
    return ops;
}

std::uint64_t offset(const XBuffer& b, const void* p) {
    return p ? static_cast<const char*>(p) - b.data() : 0;
}
std::uint64_t physical_bytes(const void* p) {
    if (!p) return 0;
    XBufferCore::AllocationHeader header;
    std::memcpy(&header, static_cast<const char*>(p) - sizeof(header), sizeof(header));
    return header.user_bytes;
}
void run(const Config& c) {
    std::uint64_t trace_checksum = root_marker, semantic_checksum = root_marker;
    std::vector<std::uint32_t> expected_other;
    const auto ops = make_trace(c, trace_checksum, expected_other);
    if constexpr (XOFFSET_TRACE_DIAGNOSTIC) vm_trace::events.reserve(1000000);
    auto config = XBuffer::maximum_capacity(c.maximum);
    if (c.mode == "automatic") config = config.with_growth(XGrowPolicy::Automatic);
    auto setup_start = Clock::now();
    XBuffer b(c.mode == "preallocated" ? c.maximum : c.initial, config);
    auto* root = b.make<AutomaticTraceRoot>(); root->marker = root_marker;
    const auto setup_ms = milliseconds(setup_start, Clock::now());
    const auto initial_logical = b.get_size();
    const auto initial_used_end = b.arena()->used_end;
    const auto address = b.get_address();
    std::uint64_t retries = 0, observed_growth = 0, seen_secondary = 0, retired_values_checked = 0;
    auto start = Clock::now();
    if constexpr (XOFFSET_TRACE_DIAGNOSTIC) vm_trace::active = true;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        if constexpr (XOFFSET_TRACE_DIAGNOSTIC) { vm_trace::operation = i; vm_trace::lane = op.kind; }
        auto& vector = op.kind == 0 ? root->values : root->other;
        if (op.kind == 2) {
            for (std::size_t j = 0; j < vector.size(); ++j) {
                require(vector[j] == value_at(seen_secondary - vector.size() + j), "retiring secondary value mismatch");
                ++retired_values_checked;
            }
            auto binding = vector.binding();
            std::destroy_at(&vector); std::construct_at(&vector, binding);
            continue;
        }
        const auto previous = b.get_size();
        for (;;) {
            try { vector.push_back(op.value); break; }
            catch (const XBadAlloc&) {
                require(c.mode == "explicit", "non-explicit execution required retry");
                ++retries;
                if constexpr (XOFFSET_TRACE_DIAGNOSTIC)
                    vm_trace::emit("caller_retry", b.get_size(), b.arena()->used_end, vector.size(), vector.capacity());
                require(b.get_size() < b.maximum_capacity(), "explicit hard capacity exhausted");
                require(b.grow(std::min(b.get_size(), b.maximum_capacity() - b.get_size())), "explicit hard capacity exhausted");
            }
        }
        if (op.kind == 1) ++seen_secondary;
        if (b.get_size() != previous) ++observed_growth;
    }
    if constexpr (XOFFSET_TRACE_DIAGNOSTIC) vm_trace::active = false;
    auto elapsed_ms = milliseconds(start, Clock::now());
    require(b.get_address() == address, "base address changed");
    auto verify = [&](const AutomaticTraceRoot& r, bool mix_values) {
        require(r.marker == root_marker && r.values.size() == c.count && r.other.size() == expected_other.size(), "shape mismatch");
        for (std::size_t i = 0; i < c.count; ++i) {
            require(r.values[i] == value_at(i), "primary value mismatch");
            if (mix_values) hash(semantic_checksum, r.values[i]);
        }
        for (std::size_t i = 0; i < expected_other.size(); ++i) {
            require(r.other[i] == expected_other[i], "secondary value mismatch");
            if (mix_values) hash(semantic_checksum, r.other[i]);
        }
    };
    verify(*root, true);
    const auto logical = b.get_size();
    const auto used_end = b.arena()->used_end;
    const auto core_free = b.get_free_memory() - (logical - used_end);
    const auto cached_free = root->values.allocator_state()->free_bytes;
    std::uint64_t wire_physical_bytes = 0, wire_blocks = 0;
    auto* state = root->values.allocator_state();
    for (std::uint32_t cls = 0; cls < fixed_detail::freelist_class_count; ++cls) {
        auto next = state->freelist_head[cls];
        while (next) {
            auto* block = fixed_detail::freelist_block_from_offset(state, next);
            wire_physical_bytes += physical_bytes(block); ++wire_blocks;
            next = block->next_offset;
        }
    }
    const auto primary_offset = offset(b, root->values.data()), secondary_offset = offset(b, root->other.data());
    const auto primary_physical = physical_bytes(root->values.data()), secondary_physical = physical_bytes(root->other.data());
    const auto primary_capacity = root->values.capacity(), secondary_capacity = root->other.capacity();
    const auto accessible = b.accessible_bytes();
    const auto memory = memory_now();
    auto wire = b.save_verified<AutomaticTraceRoot>();
    auto loaded = XBuffer::load_verified<AutomaticTraceRoot>(wire);
    verify(loaded.root<AutomaticTraceRoot>(), false);
    std::cout << std::setprecision(17) << "{\"mode\":" << json_string(c.mode)
        << ",\"diagnostic\":" << (XOFFSET_TRACE_DIAGNOSTIC ? "true" : "false")
        << ",\"count\":" << c.count << ",\"initial\":" << c.initial << ",\"maximum\":" << c.maximum
        << ",\"cadence\":" << c.cadence << ",\"burst\":" << c.burst << ",\"lifetime\":" << c.lifetime
        << ",\"operations\":" << ops.size() << ",\"trace_checksum\":" << trace_checksum
        << ",\"semantic_checksum\":" << semantic_checksum << ",\"elapsed_ms\":" << elapsed_ms
        << ",\"setup_ms\":" << setup_ms << ",\"allocation_retries\":" << retries
        << ",\"observed_growth_operations\":" << observed_growth << ",\"initial_logical\":" << initial_logical
        << ",\"retired_values_checked\":" << retired_values_checked
        << ",\"initial_used_end\":" << initial_used_end
        << ",\"logical_bytes\":" << logical << ",\"used_end\":" << used_end
        << ",\"core_free_bytes\":" << core_free << ",\"wire_free_bytes\":" << cached_free
        << ",\"wire_physical_bytes\":" << wire_physical_bytes << ",\"wire_blocks\":" << wire_blocks
        << ",\"accessible_bytes\":" << accessible << ",\"reserved_bytes\":" << b.reserved_bytes()
        << ",\"primary_offset\":" << primary_offset << ",\"secondary_offset\":" << secondary_offset
        << ",\"primary_physical_bytes\":" << primary_physical << ",\"secondary_physical_bytes\":" << secondary_physical
        << ",\"primary_capacity\":" << primary_capacity << ",\"secondary_capacity\":" << secondary_capacity
        << ",\"memory\":";
    print_memory(std::cout, memory);
    std::cout << ",\"events\":[";
    bool comma = false;
    for (const auto& e : vm_trace::events) {
        if (comma) std::cout << ','; comma = true;
        std::cout << '[' << json_string(e.name) << ',' << e.op << ',' << e.lane << ','
                  << e.a << ',' << e.b << ',' << e.c << ',' << e.d << ']';
    }
    std::cout << "]}\n";
}
}

int main(int argc, char** argv) {
    try {
        automatic_trace::Config c;
        for (int i = 1; i < argc; ++i) {
            require(i + 1 < argc, "flag needs value");
            const std::string key = argv[i], value = argv[++i];
            if (key == "--mode") c.mode = value;
            else if (key == "--count") c.count = parse_size(value);
            else if (key == "--initial") c.initial = parse_size(value);
            else if (key == "--maximum") c.maximum = parse_size(value);
            else if (key == "--cadence") c.cadence = parse_size(value);
            else if (key == "--burst") c.burst = parse_size(value);
            else if (key == "--lifetime") c.lifetime = parse_size(value);
            else throw std::runtime_error("unknown argument");
        }
        require(c.mode == "explicit" || c.mode == "automatic" || c.mode == "preallocated", "bad mode");
        require(c.count && c.count <= 16000000 && c.initial >= 512 && c.initial <= c.maximum && c.maximum <= XBufferCore::MAX_PAYLOAD, "bad bounds");
        automatic_trace::run(c);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
