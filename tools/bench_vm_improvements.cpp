// The unchanged first experiment provides memory queries, schema fixtures and
// checked arithmetic. Its entry point is not used by this stage comparison.
#define XOFFSET_BENCH_VM 1
#define main historical_benchmark_main
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#endif
#include "bench_vm_compare.cpp"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#undef main

#include <atomic>
#include <barrier>
#include <exception>
#include <numeric>
#include <random>
#include <thread>

struct ImprovementRoot {
    std::uint64_t marker{};
    XVector<std::uint32_t> values;
    XString text;
    XVector<XVector<std::uint32_t>> rows;
};
XOFFSET_REGISTER_SCHEMA_NAME(ImprovementRoot, "bench.vm-improvement.Root.v1")

namespace {
struct Options {
    Args common;
    std::size_t threads{1};
    std::string order{"sequential"};
};
struct Measurement {
    Result base;
    std::uint64_t semantic_checksum{};
    std::uint64_t wire_checksum{};
    std::size_t requests{};
    std::size_t container_moves{};
    std::size_t container_copy_bytes_est{};
    std::size_t used_end{};
    std::size_t free_bytes{};
    std::size_t cached_free_bytes{};
    std::size_t live_payload_bytes{};
    double first_ms{};
    double repeated_ms{};
};

Options options(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        require(i + 1 < argc, "every CLI flag needs one value");
        std::string value = argv[++i];
        if (flag == "--workload") o.common.workload = value;
        else if (flag == "--size") o.common.size = parse_size(value);
        else if (flag == "--reserve") o.common.reserve = parse_size(value);
        else if (flag == "--cycles") o.common.cycles = parse_size(value);
        else if (flag == "--threads") o.threads = parse_size(value);
        else if (flag == "--order") o.order = value;
        else if (flag == "--wire-path") o.common.wire_path = value;
        else throw std::runtime_error("unknown argument: " + flag);
    }
    require(o.common.size && o.common.cycles, "size and cycles must be positive");
    require(o.threads && o.threads <= 64, "threads must be between 1 and 64");
    require(o.order == "sequential" || o.order == "random", "unknown owner order");
    return o;
}

std::string read_wire(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "cannot open fixture");
    const auto n = file.tellg();
    require(n >= 0 && static_cast<std::uint64_t>(n) <=
        XBufferCore::MAX_PAYLOAD + sizeof(XWireHeaderV1), "fixture length is invalid");
    file.seekg(0);
    std::string wire(static_cast<std::size_t>(n), '\0');
    file.read(wire.data(), static_cast<std::streamsize>(wire.size()));
    require(bool(file) && file.gcount() == n, "fixture read incomplete");
    require(file.peek() == std::char_traits<char>::eof() && !file.bad(), "fixture changed");
    return wire;
}

void write_wire(const std::string& path, const std::string& wire) {
    if (path.empty()) return;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    file.close();
    require(bool(file), "fixture write failed");
}

void capture(Measurement& m, XBuffer& buffer) {
    capture_buffer(m.base, buffer);
    m.used_end = buffer.arena()->used_end;
    m.free_bytes = buffer.get_free_memory();
}

Measurement transport(const Options& o) {
    Measurement m; auto& r = m.base; r.args = o.common;
    const bool load = o.common.workload == "load";
    const bool prepare = o.common.workload == "prepare_transport";
    r.policy = "equal_explicit_capacity; first_and_repeated_operations_in_one_process";
    r.timing_scope = load ? "load_verified_allocation_copy_graph_validation; destruction_excluded"
                          : "save_verified_normalization_and_output; output_destruction_excluded";
    r.operation_unit = load ? "one_load_verified" : "one_save_verified";
    std::optional<XBuffer> source;
    std::string fixture;
    if (load) {
        require(!o.common.wire_path.empty(), "load needs an external fixture");
        fixture = read_wire(o.common.wire_path);
    } else source.emplace(prepared_transport_buffer(o.common, r));
    r.total_requested_payload_bytes = o.common.size;
    r.memory_before = memory_now();
    std::optional<std::uint64_t> expected_wire, expected_values;
    for (std::size_t i = 0; i < (prepare ? 1 : o.common.cycles); ++i) {
        double elapsed{};
        std::string wire;
        std::optional<XBuffer> loaded;
        if (load) {
            XWireHeaderV1 h{};
            require(fixture.size() >= sizeof(h), "fixture header truncated");
            std::memcpy(&h, fixture.data(), sizeof(h));
            const auto cap = o.common.reserve ? o.common.reserve : h.used_bytes;
            const auto from = Clock::now();
            loaded.emplace(XBuffer::load_verified<VmBenchRoot>(fixture,
                XBuffer::maximum_capacity(cap)));
            elapsed = milliseconds(from, Clock::now());
            capture(m, *loaded);
        } else {
            const auto address = source->get_address();
            const auto from = Clock::now();
            wire = source->save_verified<VmBenchRoot>();
            elapsed = milliseconds(from, Clock::now());
            r.address_changes += address != source->get_address();
            capture(m, *source);
        }
        capture_memory_after(r); // Before full value checks or readback allocation.
        if (!i) m.first_ms = elapsed;
        else m.repeated_ms += elapsed;
        r.elapsed_ms += elapsed;
        r.operation_samples_ms.push_back(elapsed);
        ++m.requests;
        auto values_hash = verify_values(load ? *loaded : *source, o.common.size / 4);
        const auto& bytes = load ? fixture : wire;
        auto wire_hash = hash_wire(bytes);
        if (expected_values) require(values_hash == *expected_values, "transport values changed");
        if (expected_wire) require(wire_hash == *expected_wire, "transport wire changed between cycles");
        expected_values = values_hash; expected_wire = wire_hash;
        r.wire_size = bytes.size();
        // Only save trials need an extra reader. Loading already fully validated.
        if (!load && (prepare || i + 1 == o.common.cycles)) {
            auto checked = XBuffer::load_verified<VmBenchRoot>(wire);
            require(verify_values(checked, o.common.size / 4) == values_hash, "save readback changed values");
            if (prepare || i + 1 == o.common.cycles) write_wire(o.common.wire_path, wire);
        }
    }
    m.semantic_checksum = *expected_values;
    m.wire_checksum = *expected_wire;
    m.live_payload_bytes = o.common.size;
    r.memory_after_validation = memory_now();
    return m;
}

Measurement owner(const Options& o) {
    Measurement m; auto& r = m.base; r.args = o.common;
    r.policy = "64KiB_explicit_limit; independent_buffers_per_thread; all_buffers_live_during_lookup";
    r.operation_unit = "whole_allocation_phase_all_buffers; not_per_lookup_latency";
    r.timing_scope = "create_allocate_free_destroy_phase_wall_times_including_barriers_and_scheduling; worker_startup_and_validation_excluded";
    const auto n = o.common.size;
    std::vector<std::unique_ptr<XBuffer>> buffers(n);
    std::vector<std::uint64_t*> pointers(multiply_bytes(n, allocations_per_buffer));
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    if (o.order == "random") {
        std::mt19937 rng(1729);
        std::shuffle(order.begin(), order.end(), rng);
    }
    std::vector<std::exception_ptr> failures(o.threads);
    std::atomic<bool> cancel{false};
    std::barrier gate(static_cast<std::ptrdiff_t>(o.threads + 1));
    std::vector<std::thread> workers;
    for (std::size_t lane = 0; lane < o.threads; ++lane) {
        workers.emplace_back([&, lane] {
            for (int stage = 0; stage != 4; ++stage) {
                gate.arrive_and_wait();
                try {
                    if (!cancel.load()) {
                        for (std::size_t pos = lane; pos < n; pos += o.threads) {
                            // Creation follows index order per lane; multiple lanes
                            // construct concurrently. Lookup order alone is shuffled.
                            // Every phase assigns disjoint buffers to each worker.
                            const auto i = stage == 0 ? pos : order[pos];
                            if (stage == 0) {
                                buffers[i] = std::make_unique<XBuffer>(initial_small,
                                    XBuffer::maximum_capacity(64 * KiB));
                                buffers[i]->make<VmBenchScalarRoot>()->value = root_marker ^ i;
                            } else if (stage == 1) {
                                auto a = buffers[i]->allocator<std::uint64_t>();
                                for (std::size_t j = 0; j < allocations_per_buffer; ++j) {
                                    const auto k = i * allocations_per_buffer + j;
                                    pointers[k] = a.allocate(1);
                                    *pointers[k] = root_marker ^ k;
                                }
                            } else if (stage == 2) {
                                auto a = buffers[i]->allocator<std::uint64_t>();
                                for (std::size_t j = 0; j < allocations_per_buffer; ++j)
                                    a.deallocate(pointers[i * allocations_per_buffer + j], 1);
                            } else buffers[i].reset();
                        }
                    }
                } catch (...) {
                    failures[lane] = std::current_exception();
                    cancel.store(true);
                }
                gate.arrive_and_wait();
            }
        });
    }
    r.memory_before = memory_now();
    double* phases[] = {&r.create_ms, &r.allocate_ms, &r.free_ms, &r.destroy_ms};
    bool failed = false;
    for (int stage = 0; stage != 4; ++stage) {
        const auto from = Clock::now();
        gate.arrive_and_wait();
        gate.arrive_and_wait();
        *phases[stage] = milliseconds(from, Clock::now());
        for (const auto& error : failures) failed |= bool(error);
        if (stage == 1 && !failed) {
            capture(m, *buffers.front());
            r.total_reserved_mapping_bytes *= n;
            r.total_logical_bytes *= n;
            capture_memory_after(r);
            std::uint64_t checksum = root_marker;
            // No throwing verification before joining workers: a bad value is
            // recorded, then all remaining barriers are drained normally.
            for (std::size_t i = 0; i < n; ++i) {
                failed |= buffers[i]->root<VmBenchScalarRoot>().value != (root_marker ^ i);
                for (std::size_t j = 0; j < allocations_per_buffer; ++j) {
                    const auto k = i * allocations_per_buffer + j;
                    failed |= *pointers[k] != (root_marker ^ k);
                    checksum = (checksum ^ *pointers[k]) * 1099511628211ULL;
                }
            }
            m.semantic_checksum = checksum;
            r.memory_after_validation = memory_now();
        }
    }
    for (auto& thread : workers) thread.join();
    for (const auto& error : failures) if (error) std::rethrow_exception(error);
    require(!failed, "owner workload value mismatch");
    r.elapsed_ms = r.create_ms + r.allocate_ms + r.free_ms + r.destroy_ms;
    r.operation_samples_ms = {r.allocate_ms};
    r.memory_after_cleanup = memory_now();
    m.requests = n * allocations_per_buffer * 2;
    m.live_payload_bytes = n * (allocations_per_buffer + 1) * sizeof(std::uint64_t);
    return m;
}

std::uint64_t verify_container(XBuffer& buffer, const Options& o) {
    const auto& root = buffer.root<ImprovementRoot>();
    require(root.marker == root_marker, "container marker changed");
    std::uint64_t checksum = root.marker;
    const bool string = o.common.workload == "string_tail";
    const bool nested = o.common.workload == "nested_interleaved";
    if (string) {
        require(root.text.size() == o.common.size, "string size changed");
        for (std::size_t i = 0; i < o.common.size; ++i) {
            require(root.text.data()[i] == static_cast<char>('a' + i % 23), "string payload changed");
            checksum = (checksum ^ static_cast<unsigned char>(root.text.data()[i])) * 1099511628211ULL;
        }
        require(root.text.c_str()[o.common.size] == '\0', "string terminator changed");
    } else if (nested) {
        require(root.rows.size() == 16, "nested row count changed");
        for (std::size_t row = 0; row < 16; ++row) {
            const auto expected = o.common.size / 16 + (row < o.common.size % 16);
            require(root.rows[row].size() == expected, "nested row size changed");
            for (std::size_t j = 0; j < expected; ++j) {
                require(root.rows[row][j] == value_at(j * 16 + row), "nested value changed");
                checksum = (checksum ^ root.rows[row][j]) * 1099511628211ULL;
            }
        }
    } else {
        require(root.values.size() == o.common.size, "container vector size changed");
        for (std::size_t i = 0; i < o.common.size; ++i) {
            require(root.values[i] == value_at(i), "container vector value changed");
            checksum = (checksum ^ root.values[i]) * 1099511628211ULL;
        }
        if (o.common.workload == "vector_interleaved") {
            require(root.rows.size() == 1, "interleaved companion missing");
            const auto count = (o.common.size + 1023) / 1024;
            require(root.rows[0].size() == count, "interleaved companion size changed");
            for (std::size_t j = 0; j < count; ++j) {
                require(root.rows[0][j] == value_at(j * 1024), "interleaved companion changed");
                checksum = (checksum ^ root.rows[0][j]) * 1099511628211ULL;
            }
        }
    }
    return checksum;
}

template<class F> void retry(Measurement& m, XBuffer& buffer, F&& operation) {
    for (;;) {
        try { operation(); return; }
        catch (const XBadAlloc&) {
            ++m.base.allocation_failures;
            const auto logical = buffer.segment_size();
            require(logical <= buffer.maximum_capacity() / 2, "container workload exhausted budget");
            record_growth(m.base, buffer, logical);
        }
    }
}

Measurement containers(const Options& o) {
    Measurement m; auto& r = m.base; r.args = o.common;
    const bool string = o.common.workload == "string_tail";
    const bool nested = o.common.workload == "nested_interleaved";
    const bool growing = o.common.workload == "vector_growth";
    const auto payload = multiply_bytes(o.common.size, string ? 1 : 4);
    const auto capacity = next_power_of_two(add_bytes(multiply_bytes(payload, 4), 4 * MiB));
    const auto logical = growing ? initial_small : capacity;
    r.policy = growing ? "4096_initial_explicit_geometric_retry" : "equal_preallocated_logical_extent";
    r.operation_unit = "up_to_4096_elements; string_reserve_geometric";
    r.timing_scope = "container_operations_including_relocation_and_explicit_retry; excludes_setup_validation_wire";
    r.memory_before = memory_now();
    XBuffer buffer(logical, XBuffer::maximum_capacity(capacity));
    auto* root = buffer.make<ImprovementRoot>(); root->marker = root_marker;
    if (nested || o.common.workload == "vector_interleaved") {
        root->rows.reserve(nested ? 16 : 1);
        for (int i = 0; i < (nested ? 16 : 1); ++i) root->rows.emplace_back();
    }
    r.initial_logical_bytes = logical;
    r.initial_reserved_bytes = capacity;
    const auto arena_address = buffer.get_address();
    if (string) {
        // Prepare fixed payload outside timers; geometrically increasing
        // assign/reserve mimics string construction without private mutations.
        std::string text(o.common.size, ' ');
        for (std::size_t i = 0; i < text.size(); ++i) text[i] = 'a' + i % 23;
        for (std::size_t count = 1;; count = std::min(o.common.size, count * 2)) {
            const auto* old = root->text.data(); const auto old_size = root->text.size();
            const auto from = Clock::now();
            root->text.assign(std::string_view(text.data(), count));
            const auto elapsed = milliseconds(from, Clock::now());
            r.elapsed_ms += elapsed; r.operation_samples_ms.push_back(elapsed);
            if (old && old != root->text.data()) { ++m.container_moves; m.container_copy_bytes_est += old_size; }
            ++m.requests;
            if (count == o.common.size) break;
        }
    } else {
        for (std::size_t begin = 0; begin < o.common.size; begin += append_batch) {
            const auto from = Clock::now();
            for (std::size_t i = begin; i < std::min(o.common.size, begin + append_batch); ++i) {
                auto& values = nested ? root->rows[i % 16] : root->values;
                auto* old = values.data(); const auto count = values.size();
                retry(m, buffer, [&] { values.push_back(value_at(i)); });
                if (old && old != values.data()) { ++m.container_moves; m.container_copy_bytes_est += count * 4; }
                ++m.requests;
                if (o.common.workload == "vector_interleaved" && i % 1024 == 0) {
                    auto& companion = root->rows[0];
                    auto* previous = companion.data(); const auto previous_count = companion.size();
                    companion.push_back(value_at(i));
                    if (previous && previous != companion.data()) { ++m.container_moves; m.container_copy_bytes_est += previous_count * 4; }
                    ++m.requests;
                }
            }
            const auto elapsed = milliseconds(from, Clock::now());
            r.elapsed_ms += elapsed; r.operation_samples_ms.push_back(elapsed);
        }
    }
    r.address_changes += arena_address != buffer.get_address();
    capture(m, buffer); capture_memory_after(r);
    m.semantic_checksum = verify_container(buffer, o);
    m.live_payload_bytes = payload;
    auto wire = buffer.save_verified<ImprovementRoot>();
    r.wire_size = wire.size(); m.wire_checksum = hash_wire(wire);
    auto loaded = XBuffer::load_verified<ImprovementRoot>(wire);
    require(verify_container(loaded, o) == m.semantic_checksum, "container save/load changed values");
    write_wire(o.common.wire_path, wire);
    r.memory_after_validation = memory_now();
    return m;
}

Measurement diagnostic(const Options& o) {
    Measurement m; auto& r = m.base; r.args = o.common;
    r.memory_before = memory_now();
    if (o.common.workload == "churn") {
        r.policy = "no_pool; repeated_batches; default_or_explicit64KiB_limit";
        r.operation_unit = "one_batch_create_then_destroy";
        r.timing_scope = "create_destroy_wall_time; complete_root_checks_excluded";
        for (std::size_t cycle = 0; cycle < o.common.cycles; ++cycle) {
            std::vector<std::unique_ptr<XBuffer>> buffers; buffers.reserve(o.common.size);
            auto from = Clock::now();
            for (std::size_t i = 0; i < o.common.size; ++i) {
                auto buffer = o.common.reserve ? std::make_unique<XBuffer>(initial_small,
                    XBuffer::maximum_capacity(o.common.reserve)) : std::make_unique<XBuffer>(initial_small);
                buffer->make<VmBenchScalarRoot>()->value = root_marker ^ i;
                buffers.push_back(std::move(buffer));
            }
            const auto created = milliseconds(from, Clock::now());
            capture(m, *buffers.front());
            r.total_reserved_mapping_bytes *= o.common.size;
            r.total_logical_bytes *= o.common.size;
            capture_memory_after(r);
            std::uint64_t checksum = root_marker;
            for (std::size_t i = 0; i < o.common.size; ++i) {
                auto value = buffers[i]->root<VmBenchScalarRoot>().value;
                require(value == (root_marker ^ i), "churn root mismatch");
                checksum = (checksum ^ value) * 1099511628211ULL;
            }
            if (cycle) require(checksum == m.semantic_checksum, "churn checksum changed");
            m.semantic_checksum = checksum;
            from = Clock::now(); buffers.clear();
            const auto destroyed = milliseconds(from, Clock::now());
            r.create_ms += created; r.destroy_ms += destroyed;
            r.operation_samples_ms.push_back(created + destroyed);
            if (!cycle) m.first_ms = created + destroyed;
            else m.repeated_ms += created + destroyed;
        }
        r.elapsed_ms = r.create_ms + r.destroy_ms;
        m.requests = o.common.cycles * o.common.size * 2;
    } else if (o.common.workload == "fragmentation_containers") {
        r.policy = "mixed_nested_vector_capacities; clear_all_then_small_replacements; wire_freelist_reuse";
        r.operation_unit = "one_nested_vector_reserve_fill_or_clear";
        r.timing_scope = "allocation_fill_clear_only; payload_and_wire_checks_excluded";
        const auto logical = add_bytes(multiply_bytes(o.common.size, 4096), MiB);
        XBuffer buffer(logical, XBuffer::maximum_capacity(logical));
        auto* root = buffer.make<ImprovementRoot>(); root->marker = root_marker;
        root->rows.reserve(o.common.size);
        auto from = Clock::now();
        for (std::size_t i = 0; i < o.common.size; ++i) {
            root->rows.emplace_back();
            root->rows.back().reserve(i % 2 ? 16 : 768);
            root->rows.back().push_back(value_at(i));
        }
        r.allocate_ms = milliseconds(from, Clock::now());
        from = Clock::now(); root->rows.clear();
        r.free_ms = milliseconds(from, Clock::now());
        from = Clock::now();
        for (std::size_t i = 0; i < o.common.size; ++i) {
            root->rows.emplace_back();
            root->rows.back().reserve(24);
            for (std::size_t j = 0; j < 24; ++j) root->rows.back().push_back(value_at(i * 24 + j));
        }
        r.allocate_ms += milliseconds(from, Clock::now());
        r.elapsed_ms = r.allocate_ms + r.free_ms;
        capture(m, buffer); capture_memory_after(r);
        m.cached_free_bytes = root->rows.allocator_state()->free_bytes;
        m.semantic_checksum = root_marker;
        for (std::size_t i = 0; i < o.common.size; ++i) {
            require(root->rows[i].size() == 24, "fragmented nested vector length changed");
            for (std::size_t j = 0; j < 24; ++j) {
                require(root->rows[i][j] == value_at(i * 24 + j), "fragmented nested vector changed");
                m.semantic_checksum = (m.semantic_checksum ^ root->rows[i][j]) * 1099511628211ULL;
            }
        }
        auto wire = buffer.save_verified<ImprovementRoot>();
        auto loaded = XBuffer::load_verified<ImprovementRoot>(wire);
        const auto& checked = loaded.root<ImprovementRoot>();
        require(checked.rows.size() == o.common.size, "fragmented nested wire rows changed");
        for (std::size_t i = 0; i < o.common.size; ++i) {
            require(checked.rows[i].size() == 24, "fragmented nested wire length changed");
            for (std::size_t j = 0; j < 24; ++j)
                require(checked.rows[i][j] == value_at(i * 24 + j), "fragmented nested wire value changed");
        }
        r.wire_size = wire.size(); m.wire_checksum = hash_wire(wire);
        m.live_payload_bytes = o.common.size * (24 * 4 + sizeof(XVector<std::uint32_t>));
        m.requests = o.common.size * 2 + 1;
    } else if (o.common.workload == "fragmentation") {
        r.policy = "mixed_core_block_sizes; alternately_freed_large_blocks_reused_by_small_requests";
        r.operation_unit = "one_allocate_or_free";
        r.timing_scope = "allocation_and_free_only; payload_checks_excluded";
        const auto logical = add_bytes(multiply_bytes(o.common.size, 4096), MiB);
        XBuffer buffer(logical, XBuffer::maximum_capacity(logical));
        buffer.make<VmBenchScalarRoot>()->value = root_marker;
        auto a = buffer.allocator<std::uint8_t>();
        std::vector<std::uint8_t*> blocks(o.common.size);
        std::vector<std::size_t> sizes(o.common.size);
        auto from = Clock::now();
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            sizes[i] = i % 2 ? 64 : 3072;
            blocks[i] = a.allocate(sizes[i]);
            std::memset(blocks[i], int(i % 251), sizes[i]);
        }
        r.allocate_ms = milliseconds(from, Clock::now());
        from = Clock::now();
        for (std::size_t i = 0; i < blocks.size(); i += 2) a.deallocate(blocks[i], sizes[i]);
        r.free_ms = milliseconds(from, Clock::now());
        from = Clock::now();
        for (std::size_t i = 0; i < blocks.size(); i += 2) {
            sizes[i] = 96; blocks[i] = a.allocate(sizes[i]);
            std::memset(blocks[i], int(i % 251), sizes[i]);
        }
        r.allocate_ms += milliseconds(from, Clock::now());
        r.elapsed_ms = r.allocate_ms + r.free_ms;
        capture(m, buffer); capture_memory_after(r);
        m.semantic_checksum = root_marker;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            m.live_payload_bytes += sizes[i];
            for (std::size_t j = 0; j < sizes[i]; ++j) {
                require(blocks[i][j] == i % 251, "fragmented block value changed");
                m.semantic_checksum = (m.semantic_checksum ^ blocks[i][j]) * 1099511628211ULL;
            }
        }
        m.requests = o.common.size + 2 * ((o.common.size + 1) / 2);
    } else {
        require(o.common.workload == "shrink_regrow", "unknown diagnostic workload");
        Args args = o.common; args.workload = "save_grow_cycles";
        r = cycle_workload(args); r.args = o.common;
        m.semantic_checksum = root_marker;
        for (std::size_t i = 0; i < o.common.size / 4; ++i)
            m.semantic_checksum = (m.semantic_checksum ^ value_at(i)) * 1099511628211ULL;
        m.wire_checksum = r.checksum;
        m.requests = o.common.cycles * 2; m.live_payload_bytes = o.common.size;
    }
    r.memory_after_validation = memory_now();
    r.memory_after_cleanup = r.memory_after_validation;
    return m;
}

void print_measurement(const Measurement& m, const Options& o) {
    // Reuse the historical serializer without changing its schema or source.
    std::ostringstream legacy;
    auto* previous = std::cout.rdbuf(legacy.rdbuf());
    print_result(m.base);
    std::cout.rdbuf(previous);
    auto json = legacy.str();
    const auto end = json.find_last_of('}');
    require(end != std::string::npos, "invalid result serializer");
    std::cout << json.substr(0, end) << std::setprecision(12)
        << ",\"improvement_schema_version\":1"
        << ",\"threads\":" << o.threads << ",\"order\":" << json_string(o.order)
        << ",\"semantic_checksum\":" << m.semantic_checksum
        << ",\"wire_checksum\":" << m.wire_checksum
        << ",\"request_count\":" << m.requests
        << ",\"container_moves\":" << m.container_moves
        << ",\"container_copy_bytes_est\":" << m.container_copy_bytes_est
        << ",\"used_end\":" << m.used_end << ",\"free_bytes\":" << m.free_bytes
        << ",\"cached_free_bytes\":" << m.cached_free_bytes
        << ",\"live_payload_bytes\":" << m.live_payload_bytes
        << ",\"first_ms\":" << m.first_ms << ",\"repeated_ms\":" << m.repeated_ms
        << ",\"repeated_mean_ms\":" << (o.common.cycles > 1 ? m.repeated_ms / (o.common.cycles - 1) : 0)
        << ",\"owner_probe_count\":null,\"owner_probe_note\":\"not_instrumented; scaling_and_latency_measured\"}\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto o = options(argc, argv);
        Measurement m;
        const auto& work = o.common.workload;
        if (work == "save" || work == "load" || work == "prepare_transport") m = transport(o);
        else if (work == "owner") m = owner(o);
        else if (work.rfind("vector_", 0) == 0 || work == "string_tail" || work == "nested_interleaved") m = containers(o);
        else if (work.rfind("verify_", 0) == 0) {
            auto check = o; check.common.workload = work.substr(7);
            auto wire = read_wire(o.common.wire_path);
            auto loaded = XBuffer::load_verified<ImprovementRoot>(wire);
            m.base.args = o.common;
            m.semantic_checksum = verify_container(loaded, check);
            // Exercise mutation followed by re-save and re-load in the reader.
            loaded.root<ImprovementRoot>().marker ^= 1;
            auto rewritten = loaded.save_verified<ImprovementRoot>();
            auto reloaded = XBuffer::load_verified<ImprovementRoot>(rewritten);
            require(reloaded.root<ImprovementRoot>().marker == (root_marker ^ 1), "reader mutation lost");
            reloaded.root<ImprovementRoot>().marker = root_marker;
            require(verify_container(reloaded, check) == m.semantic_checksum, "reader rewrite lost values");
        } else m = diagnostic(o);
        print_measurement(m, o);
    } catch (const std::exception& e) {
        std::cerr << "VM improvement benchmark failed: " << e.what() << '\n';
        return 1;
    }
}
