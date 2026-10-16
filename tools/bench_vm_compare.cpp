// Compile this identical harness against 07d6499 with XOFFSET_BENCH_VM=0,
// and against the stable VM worktree with XOFFSET_BENCH_VM=1. Reflection,
// schema admission and verified-wire validation remain enabled in both builds.
#include "xoffsetdatastructure.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifndef XOFFSET_BENCH_VM
#error "Set XOFFSET_BENCH_VM to 0 (vector baseline) or 1 (stable VM)."
#endif

using namespace XOffsetDatastructure;
using Clock = std::chrono::steady_clock;

struct VmBenchRoot {
    std::uint64_t marker{};
    XVector<std::uint32_t> values;
};
struct VmBenchScalarRoot { std::uint64_t value{}; };
XOFFSET_REGISTER_SCHEMA_NAME(VmBenchRoot, "bench.stable-vm.Root.v1")

namespace {
constexpr std::size_t KiB = 1024;
constexpr std::size_t MiB = KiB * KiB;
constexpr std::size_t initial_small = 4096;
constexpr std::uint64_t root_marker = 0x195a926a5cc24873ULL;
constexpr std::size_t append_batch = 4096;
constexpr std::size_t allocations_per_buffer = 16;

struct Args {
    std::string workload;
    std::size_t size{};
    std::size_t reserve{};
    std::size_t cycles{8};
    std::string wire_path;
};

struct Memory {
    std::uint64_t rss_bytes{};
    std::uint64_t virtual_bytes{};
    std::uint64_t peak_rss_bytes{};
    bool available{};
    std::string rss_source;
};

Memory memory_now() {
    Memory m;
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (::task_info(::mach_task_self(), TASK_VM_INFO,
                    reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        m.rss_bytes = info.resident_size;
        m.virtual_bytes = info.virtual_size;
        m.peak_rss_bytes = info.resident_size_peak;
        m.available = true;
        m.rss_source = "mach_TASK_VM_INFO";
    }
    else throw std::runtime_error("TASK_VM_INFO memory query failed");
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) == 0)
        m.peak_rss_bytes = std::max(m.peak_rss_bytes,
            static_cast<std::uint64_t>(usage.ru_maxrss)); // Darwin: bytes.
#elif defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    std::uint64_t virtual_pages{}, resident_pages{};
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size > 0 && statm >> virtual_pages >> resident_pages) {
        m.rss_bytes = resident_pages * static_cast<std::uint64_t>(page_size);
        m.virtual_bytes = virtual_pages * static_cast<std::uint64_t>(page_size);
        m.available = true;
        m.rss_source = "proc_statm_page_counts";
    }
    else throw std::runtime_error("/proc/self/statm memory query failed");
    std::ifstream rollup("/proc/self/smaps_rollup");
    std::string line;
    while (std::getline(rollup, line)) {
        if (line.rfind("Rss:", 0) == 0) {
            std::istringstream fields(line.substr(4));
            std::uint64_t rss_kib{}; std::string unit;
            if (!(fields >> rss_kib >> unit) || unit != "kB")
                throw std::runtime_error("malformed smaps_rollup Rss field");
            m.rss_bytes = rss_kib * KiB;
            m.rss_source = "proc_smaps_rollup";
            break;
        }
    }
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) == 0)
        m.peak_rss_bytes = static_cast<std::uint64_t>(usage.ru_maxrss) * KiB;
#endif
    return m;
}

struct Result {
    Args args;
    std::string policy;
    std::string operation_unit;
    std::string timing_scope;
    std::size_t initial_logical_bytes{};
    std::size_t initial_reserved_bytes{};
    std::size_t final_logical_bytes{};
    std::size_t final_reserved_bytes{};
    std::size_t final_accessible_bytes{};
    std::size_t final_reserved_mapping_bytes{};
    std::uint64_t total_reserved_mapping_bytes{};
    std::uint64_t total_logical_bytes{};
    std::size_t vm_enable_calls_est{};
    std::size_t vm_enabled_bytes_est{};
    std::size_t total_requested_payload_bytes{};
    std::size_t setup_pretouched_bytes{};
    std::size_t newly_exposed_bytes{};
    std::size_t relocation_copy_bytes_est{};
    std::size_t allocation_failures{};
    std::size_t address_changes{};
    std::size_t wire_size{};
    std::size_t wire_reserved_bytes{};
    std::uint64_t checksum{};
    bool save_base_changed{};
    double elapsed_ms{};
    double create_ms{};
    double allocate_ms{};
    double free_ms{};
    double destroy_ms{};
    std::vector<double> operation_samples_ms;
    std::vector<double> growth_samples_ms;
    Memory memory_before, memory_after, memory_live, memory_peak, memory_after_validation,
           memory_after_cleanup;
};

double milliseconds(Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::size_t bounded_bytes(std::size_t value) {
    if (value > XBufferCore::MAX_PAYLOAD)
        throw std::runtime_error("requested arena extent exceeds MAX_PAYLOAD");
    return value;
}

std::size_t add_bytes(std::size_t a, std::size_t b) {
    if (a > XBufferCore::MAX_PAYLOAD || b > XBufferCore::MAX_PAYLOAD - a)
        throw std::runtime_error("arena budget arithmetic exceeds MAX_PAYLOAD");
    return a + b;
}

std::size_t multiply_bytes(std::size_t a, std::size_t b) {
    if (b && a > XBufferCore::MAX_PAYLOAD / b)
        throw std::runtime_error("arena budget arithmetic exceeds MAX_PAYLOAD");
    return a * b;
}

std::size_t next_power_of_two(std::size_t value) {
    std::size_t result = 1;
    while (result < value) {
        if (result > XBufferCore::MAX_PAYLOAD / 2)
            return XBufferCore::MAX_PAYLOAD;
        result *= 2;
    }
    return result;
}

std::uint32_t value_at(std::size_t index) {
    return static_cast<std::uint32_t>(index) * 2654435761u ^ 0xa53179bdu;
}

std::uint64_t verify_values(XBuffer& buffer, std::size_t count) {
    const auto& root = buffer.root<VmBenchRoot>();
    require(root.marker == root_marker, "root marker changed");
    require(root.values.size() == count, "vector length changed");
    std::uint64_t checksum = root.marker;
    for (std::size_t i = 0; i < count; ++i) {
        require(root.values[i] == value_at(i), "vector payload changed");
        checksum = (checksum ^ root.values[i]) * 1099511628211ULL;
    }
    return checksum;
}

std::uint64_t hash_wire(const std::string& wire) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : wire) h = (h ^ c) * 1099511628211ULL;
    return h;
}

XBuffer configured_buffer(std::size_t initial, std::size_t cap, bool default_policy) {
#if XOFFSET_BENCH_VM
    if (default_policy) return XBuffer(initial);
    return XBuffer(initial, XBuffer::maximum_capacity(cap));
#else
    if (default_policy) return XBuffer(initial);
    return XBuffer(initial, XBuffer::max_capacity(cap));
#endif
}

std::size_t accessible_extent(const XBuffer& buffer) {
#if XOFFSET_BENCH_VM
    return buffer.accessible_bytes();
#else
    // Not an OS residency measurement: vector owns this allocated capacity.
    return buffer.reserved_capacity();
#endif
}

void capture_buffer(Result& r, const XBuffer& buffer) {
    r.final_logical_bytes = buffer.segment_size();
    r.final_reserved_bytes = buffer.reserved_capacity();
    r.final_accessible_bytes = accessible_extent(buffer);
#if XOFFSET_BENCH_VM
    r.final_reserved_mapping_bytes = buffer.reserved_bytes();
#else
    r.final_reserved_mapping_bytes = buffer.reserved_capacity();
#endif
    r.total_reserved_mapping_bytes = r.final_reserved_mapping_bytes;
    r.total_logical_bytes = r.final_logical_bytes;
}

void capture_memory_after(Result& r) {
    r.memory_after = memory_now();
    r.memory_live = r.memory_after;
    r.memory_peak = r.memory_after;
}

void record_growth(Result& r, XBuffer& buffer, std::size_t extra) {
    const auto old_address = reinterpret_cast<std::uintptr_t>(buffer.get_address());
    const auto old_size = buffer.segment_size();
    const auto old_accessible = accessible_extent(buffer);
    const auto from = Clock::now();
    const bool ok = buffer.grow(extra);
    const auto to = Clock::now();
    require(ok, "grow failed; requested reserve/capacity or OS resources are insufficient");
    r.growth_samples_ms.push_back(milliseconds(from, to));
    r.newly_exposed_bytes += extra;
#if XOFFSET_BENCH_VM
    const auto enabled_extra = accessible_extent(buffer) - old_accessible;
    if (enabled_extra) ++r.vm_enable_calls_est;
    r.vm_enabled_bytes_est += enabled_extra;
#else
    (void)old_accessible;
#endif
    if (reinterpret_cast<std::uintptr_t>(buffer.get_address()) != old_address) {
        ++r.address_changes;
        r.relocation_copy_bytes_est += old_size;
    }
}

Result grow_workload(const Args& args) {
    Result r; r.args = args;
    require(args.size >= initial_small, "grow size must be at least 4096 bytes");
    bounded_bytes(args.size);
    const bool same_reserve = args.workload == "grow_equal_reserve";
    const std::size_t cap = args.reserve ? args.reserve : args.size;
    require(cap >= args.size, "grow reserve is smaller than final logical extent");
    bounded_bytes(cap);
    r.policy = same_reserve ? "equal_initial_reservation" : "baseline_default_hint_vs_vm_explicit_limit";
    r.operation_unit = "one_grow_up_to_4MiB";
    r.timing_scope = "grow_only_including_zeroing_and_optional_baseline_relocation";
    r.initial_logical_bytes = initial_small;
    r.growth_samples_ms.reserve((args.size - initial_small + 4 * MiB - 1) / (4 * MiB));
    r.memory_before = memory_now();
    auto buffer = configured_buffer(initial_small, cap,
        !same_reserve && !XOFFSET_BENCH_VM);
    r.initial_reserved_bytes = buffer.reserved_capacity();
    buffer.make<VmBenchScalarRoot>()->value = root_marker;
    const std::size_t tail_start = static_cast<std::size_t>(buffer.arena()->used_end);
    std::memset(static_cast<char*>(const_cast<void*>(buffer.get_address())) + tail_start,
                0x5a, initial_small - tail_start);
    r.setup_pretouched_bytes = initial_small;
    const auto from = Clock::now();
    while (buffer.segment_size() < args.size) {
        const auto extra = std::min<std::size_t>(4 * MiB, args.size - buffer.segment_size());
        record_growth(r, buffer, extra);
    }
    r.elapsed_ms = milliseconds(from, Clock::now());
    r.operation_samples_ms = r.growth_samples_ms;
    capture_buffer(r, buffer);
    capture_memory_after(r);
    require(buffer.root<VmBenchScalarRoot>().value == root_marker, "grow changed root");
    const auto* bytes = static_cast<const unsigned char*>(buffer.get_address());
    std::uint64_t checksum = root_marker;
    for (std::size_t i = tail_start; i < args.size; ++i) {
        const unsigned char expected = i < initial_small ? 0x5a : 0;
        require(bytes[i] == expected, "grow did not preserve/zero the full payload range");
        checksum = (checksum ^ bytes[i]) * 1099511628211ULL;
    }
    r.checksum = checksum;
    r.memory_after_validation = memory_now();
    return r;
}

Result append_workload(const Args& args) {
    Result r; r.args = args;
    const bool preallocated = args.workload == "append_preallocated";
    const auto payload = multiply_bytes(args.size, sizeof(std::uint32_t));
    const auto final_vector_capacity_bytes = multiply_bytes(next_power_of_two(args.size), sizeof(std::uint32_t));
    const auto allocator_extent = add_bytes(multiply_bytes(final_vector_capacity_bytes, 2), MiB);
    const auto budget = std::max(add_bytes(multiply_bytes(payload, 3), MiB), allocator_extent);
    const auto cap = args.reserve ? bounded_bytes(args.reserve) : next_power_of_two(budget);
    const auto initial = preallocated ? allocator_extent : initial_small;
    require(cap >= initial, "append reserve is smaller than initial extent");
    r.policy = preallocated ? "equal_preallocated_logical_extent_and_reservation"
                           : "baseline_default_hint_vs_vm_explicit_limit_same_retry_algorithm";
    r.operation_unit = "up_to_4096_push_back_elements";
    r.timing_scope = "push_with_cached_root_and_growth_retry";
    r.total_requested_payload_bytes = payload;
    r.initial_logical_bytes = initial;
    r.operation_samples_ms.reserve((args.size + append_batch - 1) / append_batch);
    r.growth_samples_ms.reserve(32);
    r.memory_before = memory_now();
    auto buffer = configured_buffer(initial, cap, !preallocated && !XOFFSET_BENCH_VM);
    r.initial_reserved_bytes = buffer.reserved_capacity();
    auto* root = buffer.make<VmBenchRoot>();
    root->marker = root_marker;
    const auto from = Clock::now();
    for (std::size_t begin = 0; begin < args.size; begin += append_batch) {
        const auto batch_from = Clock::now();
        const auto end = std::min(args.size, begin + append_batch);
        for (auto i = begin; i < end; ++i) {
            for (;;) {
                try {
                    root->values.push_back(value_at(i));
                    break;
                } catch (const XBadAlloc&) {
                    ++r.allocation_failures;
                    require(!preallocated, "preallocated append unexpectedly required arena growth");
                    // Identical explicit retry algorithm. Reacquire root after every grow.
                    const auto logical = buffer.segment_size();
                    require(logical <= XBufferCore::MAX_PAYLOAD / 2,
                            "append doubling would exceed MAX_PAYLOAD");
                    record_growth(r, buffer, logical);
                    root = &buffer.root<VmBenchRoot>();
                }
            }
        }
        r.operation_samples_ms.push_back(milliseconds(batch_from, Clock::now()));
    }
    r.elapsed_ms = milliseconds(from, Clock::now());
    capture_buffer(r, buffer);
    capture_memory_after(r);
    r.checksum = verify_values(buffer, args.size);
    r.memory_after_validation = memory_now();
    return r;
}

Result many_workload(const Args& args) {
    Result r; r.args = args;
    const bool same_reserve = args.workload == "many_equal_reserve";
    const auto cap = args.reserve ? bounded_bytes(args.reserve) : 64 * KiB;
    require(cap >= initial_small, "many reserve is smaller than initial extent");
    require(args.size > 0, "many requires a positive buffer count");
    require(args.size <= std::numeric_limits<std::size_t>::max() /
            (allocations_per_buffer * sizeof(std::uint64_t*)), "many count overflows pointer storage");
    r.policy = same_reserve ? "equal_64KiB_or_requested_reservation"
                           : "each_implementation_default_reservation";
    r.operation_unit = "16_allocations_in_one_buffer_oldest_first_with_all_buffers_live";
    r.timing_scope = "create_plus_allocate_plus_free_plus_destroy; latency_samples_allocate_only";
    r.initial_logical_bytes = initial_small;
    r.operation_samples_ms.reserve(args.size);
    r.total_requested_payload_bytes = args.size * allocations_per_buffer * sizeof(std::uint64_t);
    std::vector<std::unique_ptr<XBuffer>> buffers;
    buffers.reserve(args.size);
    std::vector<std::uint64_t*> pointers(args.size * allocations_per_buffer);
    r.memory_before = memory_now();
    auto from = Clock::now();
    for (std::size_t i = 0; i < args.size; ++i) {
        auto buffer = std::make_unique<XBuffer>(configured_buffer(initial_small, cap, !same_reserve));
        buffer->make<VmBenchScalarRoot>()->value = root_marker ^ i;
        buffers.push_back(std::move(buffer));
    }
    r.create_ms = milliseconds(from, Clock::now());
    capture_buffer(r, *buffers.front());
    r.initial_reserved_bytes = buffers.front()->reserved_capacity();
    r.total_reserved_mapping_bytes *= args.size;
    r.total_logical_bytes *= args.size;
    from = Clock::now();
    for (std::size_t i = 0; i < args.size; ++i) {
        const auto batch_from = Clock::now();
        auto alloc = buffers[i]->allocator<std::uint64_t>();
        for (std::size_t j = 0; j < allocations_per_buffer; ++j) {
            auto* ptr = alloc.allocate(1);
            *ptr = root_marker ^ (i * allocations_per_buffer + j);
            pointers[i * allocations_per_buffer + j] = ptr;
        }
        r.operation_samples_ms.push_back(milliseconds(batch_from, Clock::now()));
    }
    r.allocate_ms = milliseconds(from, Clock::now());
    capture_memory_after(r); // All buffers and allocations are still live.
    std::uint64_t checksum = root_marker;
    for (std::size_t i = 0; i < args.size; ++i) {
        require(buffers[i]->root<VmBenchScalarRoot>().value == (root_marker ^ i),
                "many changed scalar root");
        for (std::size_t j = 0; j < allocations_per_buffer; ++j) {
            const auto k = i * allocations_per_buffer + j;
            require(*pointers[k] == (root_marker ^ k), "many changed allocation");
            checksum = (checksum ^ *pointers[k]) * 1099511628211ULL;
        }
    }
    r.checksum = checksum;
    r.memory_after_validation = memory_now();
    from = Clock::now();
    for (std::size_t i = 0; i < args.size; ++i) {
        auto alloc = buffers[i]->allocator<std::uint64_t>();
        for (std::size_t j = 0; j < allocations_per_buffer; ++j)
            alloc.deallocate(pointers[i * allocations_per_buffer + j], 1);
    }
    r.free_ms = milliseconds(from, Clock::now());
    from = Clock::now();
    buffers.clear();
    r.destroy_ms = milliseconds(from, Clock::now());
    r.elapsed_ms = r.create_ms + r.allocate_ms + r.free_ms + r.destroy_ms;
    r.memory_after_cleanup = memory_now();
    r.memory_after = r.memory_after_cleanup;
    if (r.memory_after_cleanup.peak_rss_bytes > r.memory_peak.peak_rss_bytes)
        r.memory_peak.peak_rss_bytes = r.memory_after_cleanup.peak_rss_bytes;
    return r;
}

XBuffer prepared_transport_buffer(const Args& args, Result& r) {
    require(args.size % sizeof(std::uint32_t) == 0,
            "transport size must be a multiple of four payload bytes");
    const auto initial = add_bytes(multiply_bytes(args.size, 2), MiB);
    const auto cap = args.reserve ? bounded_bytes(args.reserve) : initial;
    require(cap >= initial, "transport reserve is smaller than prepared logical extent");
    auto buffer = configured_buffer(initial, cap, false);
    r.initial_reserved_bytes = buffer.reserved_capacity();
    auto* root = buffer.make<VmBenchRoot>();
    root->marker = root_marker;
    root->values.reserve(args.size / sizeof(std::uint32_t));
    for (std::size_t i = 0; i < args.size / sizeof(std::uint32_t); ++i)
        root->values.push_back(value_at(i));
    // Match vector's eager initialization: both complete logical extents have
    // been written before a save trial. Setup is excluded from measured time.
    const auto used = static_cast<std::size_t>(buffer.arena()->used_end);
    std::memset(static_cast<char*>(const_cast<void*>(buffer.get_address())) + used,
                0, initial - used);
    r.initial_logical_bytes = initial;
    r.setup_pretouched_bytes = initial;
    r.total_requested_payload_bytes = args.size;
    (void)verify_values(buffer, args.size / sizeof(std::uint32_t));
    return buffer;
}

Result save_workload(const Args& args) {
    Result r; r.args = args;
    r.policy = "equal_prepared_extent_and_reservation_single_first_save";
    r.operation_unit = "one_save_verified";
    r.timing_scope = "save_verified_including_normalization_and_wire_copies";
    auto buffer = prepared_transport_buffer(args, r);
    r.memory_before = memory_now();
    const auto old_address = reinterpret_cast<std::uintptr_t>(buffer.get_address());
    const auto from = Clock::now();
    auto wire = buffer.save_verified<VmBenchRoot>();
    r.elapsed_ms = milliseconds(from, Clock::now());
    r.operation_samples_ms.push_back(r.elapsed_ms);
    r.save_base_changed = old_address != reinterpret_cast<std::uintptr_t>(buffer.get_address());
    r.address_changes = r.save_base_changed ? 1 : 0;
    if (r.save_base_changed) r.relocation_copy_bytes_est = buffer.segment_size();
    capture_buffer(r, buffer);
    capture_memory_after(r);
    r.wire_size = wire.size();
    XWireHeaderV1 h{}; std::memcpy(&h, wire.data(), sizeof(h));
    r.wire_reserved_bytes = static_cast<std::size_t>(h.reserved_bytes);
    r.checksum = hash_wire(wire);
    (void)verify_values(buffer, args.size / sizeof(std::uint32_t));
    auto loaded = XBuffer::load_verified<VmBenchRoot>(wire);
    (void)verify_values(loaded, args.size / sizeof(std::uint32_t));
    r.memory_after_validation = memory_now();
    return r;
}

Result load_workload(const Args& args) {
    Result r; r.args = args;
    r.policy = "same_wire_bytes_and_requested_loader_reservation";
    r.operation_unit = "one_load_verified";
    r.timing_scope = "load_verified_including_allocation_copy_and_full_schema_graph_validation";
    std::string wire;
    if (!args.wire_path.empty()) {
        std::ifstream fixture(args.wire_path, std::ios::binary);
        require(bool(fixture), "cannot open load wire fixture");
        fixture.seekg(0, std::ios::end);
        require(bool(fixture), "cannot seek to load wire fixture end");
        const auto length = fixture.tellg();
        require(length != std::ifstream::pos_type(-1), "cannot determine load wire fixture length");
        const auto file_bytes = static_cast<std::streamoff>(length);
        require(file_bytes >= 0 &&
                static_cast<std::uint64_t>(file_bytes) <=
                    XBufferCore::MAX_PAYLOAD + sizeof(XWireHeaderV1),
                "load wire fixture length exceeds protocol limits");
        require(static_cast<std::uint64_t>(file_bytes) <=
                static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()),
                "load wire fixture length exceeds stream read limits");
        fixture.seekg(0, std::ios::beg);
        require(bool(fixture), "cannot seek to load wire fixture beginning");
        // One allocation stays live through load. Iterator-driven string
        // growth would free arena-sized intermediate buffers for the old
        // vector loader to reuse, contaminating the allocation comparison.
        wire.resize(static_cast<std::size_t>(file_bytes));
        fixture.read(wire.data(), static_cast<std::streamsize>(file_bytes));
        require(bool(fixture) && fixture.gcount() == file_bytes,
                "cannot read complete load wire fixture");
        require(fixture.peek() == std::char_traits<char>::eof() && !fixture.bad(),
                "load wire fixture length changed while reading");
        r.total_requested_payload_bytes = args.size;
    } else {
        // Loader reserve has its own meaning; preparation requires enough
        // logical space for the vector and is not part of the measured load.
        Args prepare = args; prepare.reserve = 0;
        auto prepared = prepared_transport_buffer(prepare, r);
        wire = prepared.save_verified<VmBenchRoot>();
    }
    require(wire.size() >= sizeof(XWireHeaderV1), "load fixture is smaller than wire header");
    XWireHeaderV1 h{}; std::memcpy(&h, wire.data(), sizeof(h));
    const auto cap = args.reserve ? bounded_bytes(args.reserve) : static_cast<std::size_t>(h.used_bytes);
    require(cap >= h.used_bytes, "load reserve is smaller than wire payload extent");
    // The old reader takes its allocation hint from this field; VM readers
    // take an explicit process-local cap. Send identical input bytes to both.
    h.reserved_bytes = cap;
    std::memcpy(wire.data(), &h, sizeof(h));
    r.initial_logical_bytes = static_cast<std::size_t>(h.used_bytes);
    r.wire_size = wire.size();
    r.wire_reserved_bytes = cap;
    r.checksum = hash_wire(wire);
    r.memory_before = memory_now();
    const auto from = Clock::now();
#if XOFFSET_BENCH_VM
    auto loaded = XBuffer::load_verified<VmBenchRoot>(wire, XBuffer::maximum_capacity(cap));
#else
    auto loaded = XBuffer::load_verified<VmBenchRoot>(wire);
#endif
    r.elapsed_ms = milliseconds(from, Clock::now());
    r.initial_reserved_bytes = loaded.reserved_capacity();
    r.operation_samples_ms.push_back(r.elapsed_ms);
    capture_buffer(r, loaded);
    capture_memory_after(r);
    (void)verify_values(loaded, args.size / sizeof(std::uint32_t));
    r.memory_after_validation = memory_now();
    return r;
}

Result prepare_wire_workload(const Args& args) {
    require(!args.wire_path.empty(), "prepare_wire requires --wire-path");
    Result r; r.args = args;
    r.policy = "fixture_preparation_outside_measured_process";
    r.operation_unit = "fixture_file";
    r.timing_scope = "not_a_measured_workload";
    r.memory_before = memory_now();
    Args prepare = args; prepare.reserve = 0;
    auto buffer = prepared_transport_buffer(prepare, r);
    auto wire = buffer.save_verified<VmBenchRoot>();
    XWireHeaderV1 h{}; std::memcpy(&h, wire.data(), sizeof(h));
    const auto cap = args.reserve ? bounded_bytes(args.reserve) : static_cast<std::size_t>(h.used_bytes);
    require(cap >= h.used_bytes, "fixture reserve is smaller than normalized extent");
    h.reserved_bytes = cap;
    std::memcpy(wire.data(), &h, sizeof(h));
    std::ofstream file(args.wire_path, std::ios::binary | std::ios::trunc);
    require(bool(file), "cannot open wire fixture output");
    file.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    file.close();
    require(bool(file), "cannot write wire fixture output");
    r.wire_size = wire.size();
    r.wire_reserved_bytes = cap;
    r.checksum = hash_wire(wire);
    capture_buffer(r, buffer);
    capture_memory_after(r);
    r.memory_after_validation = r.memory_after;
    return r;
}

Result cycle_workload(const Args& args) {
    Result r; r.args = args;
    require(args.cycles > 0, "cycles must be positive");
    r.policy = "equal_prepared_extent_and_reservation_repeated_save_then_regrow";
    r.operation_unit = "one_save_verified_then_grow_to_initial_extent";
    r.timing_scope = "sum_save_and_grow_intervals; excludes_per_cycle_wire_checksum";
    auto buffer = prepared_transport_buffer(args, r);
    r.memory_before = memory_now();
    std::string last_wire;
    std::optional<std::uint64_t> first_checksum;
    double elapsed = 0;
    for (std::size_t i = 0; i < args.cycles; ++i) {
        const auto old_address = reinterpret_cast<std::uintptr_t>(buffer.get_address());
        const auto from = Clock::now();
        auto wire = buffer.save_verified<VmBenchRoot>();
        const auto save_ms = milliseconds(from, Clock::now());
        if (reinterpret_cast<std::uintptr_t>(buffer.get_address()) != old_address) {
            ++r.address_changes;
            r.save_base_changed = true;
            r.relocation_copy_bytes_est += buffer.segment_size();
        }
        record_growth(r, buffer, r.initial_logical_bytes - buffer.segment_size());
        const double cycle_ms = save_ms + r.growth_samples_ms.back();
        r.operation_samples_ms.push_back(cycle_ms);
        elapsed += cycle_ms;
        const auto checksum = hash_wire(wire); // Outside both timing intervals.
        if (first_checksum) require(checksum == *first_checksum, "wire changed across save/grow cycles");
        else first_checksum = checksum;
        last_wire = std::move(wire);
    }
    r.elapsed_ms = elapsed;
    capture_buffer(r, buffer);
    capture_memory_after(r);
    r.wire_size = last_wire.size();
    XWireHeaderV1 h{}; std::memcpy(&h, last_wire.data(), sizeof(h));
    r.wire_reserved_bytes = h.reserved_bytes;
    r.checksum = *first_checksum;
    (void)verify_values(buffer, args.size / sizeof(std::uint32_t));
    auto loaded = XBuffer::load_verified<VmBenchRoot>(last_wire);
    (void)verify_values(loaded, args.size / sizeof(std::uint32_t));
    r.memory_after_validation = memory_now();
    return r;
}

std::string json_string(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    // Nearest-rank percentile, including the last observation for p100.
    const auto rank = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(fraction * values.size())));
    return values[std::min(rank - 1, values.size() - 1)];
}

void print_memory(std::ostream& out, const Memory& memory) {
    out << "{\"available\":" << (memory.available ? "true" : "false")
        << ",\"rss_bytes\":" << memory.rss_bytes
        << ",\"virtual_bytes\":" << memory.virtual_bytes
        << ",\"peak_rss_bytes\":" << memory.peak_rss_bytes
        << ",\"rss_source\":" << json_string(memory.rss_source) << '}';
}

void print_samples(std::ostream& out, const std::vector<double>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << values[i];
    }
    out << ']';
}

void print_result(const Result& r) {
    auto& out = std::cout;
    out << std::setprecision(12) << '{';
    out << "\"schema_version\":1,\"variant\":" << json_string(XOFFSET_BENCH_VM ? "stable_vm" : "vector_07d6499")
        << ",\"workload\":" << json_string(r.args.workload)
        << ",\"size\":" << r.args.size
        << ",\"requested_reserve_bytes\":" << r.args.reserve
        << ",\"requested_cycles\":" << r.args.cycles
        << ",\"policy\":" << json_string(r.policy)
        << ",\"operation_unit\":" << json_string(r.operation_unit)
        << ",\"timing_scope\":" << json_string(r.timing_scope)
        << ",\"initial_logical_bytes\":" << r.initial_logical_bytes
        << ",\"initial_reserved_bytes\":" << r.initial_reserved_bytes
        << ",\"final_logical_bytes\":" << r.final_logical_bytes
        << ",\"final_reserved_bytes\":" << r.final_reserved_bytes
        << ",\"final_accessible_bytes\":" << r.final_accessible_bytes
        << ",\"final_reserved_mapping_bytes\":" << r.final_reserved_mapping_bytes
        << ",\"total_reserved_mapping_bytes\":" << r.total_reserved_mapping_bytes
        << ",\"total_logical_bytes\":" << r.total_logical_bytes
        << ",\"vm_enable_calls_est\":" << r.vm_enable_calls_est
        << ",\"vm_enabled_bytes_est\":" << r.vm_enabled_bytes_est
        << ",\"total_requested_payload_bytes\":" << r.total_requested_payload_bytes
        << ",\"setup_pretouched_bytes\":" << r.setup_pretouched_bytes
        << ",\"elapsed_ms\":" << r.elapsed_ms
        << ",\"operation_count\":" << r.operation_samples_ms.size()
        << ",\"operation_p50_ms\":" << percentile(r.operation_samples_ms, .50)
        << ",\"operation_p95_ms\":" << percentile(r.operation_samples_ms, .95)
        << ",\"operation_p99_ms\":" << percentile(r.operation_samples_ms, .99)
        << ",\"operation_max_ms\":" << percentile(r.operation_samples_ms, 1)
        << ",\"growth_count\":" << r.growth_samples_ms.size()
        << ",\"growth_max_ms\":" << percentile(r.growth_samples_ms, 1)
        << ",\"address_changes\":" << r.address_changes
        << ",\"relocation_copy_bytes_est\":" << r.relocation_copy_bytes_est
        << ",\"newly_exposed_bytes\":" << r.newly_exposed_bytes
        << ",\"zeroed_new_bytes\":" << r.newly_exposed_bytes
        << ",\"allocation_failures\":" << r.allocation_failures
        << ",\"checksum\":" << r.checksum
        << ",\"wire_size\":" << r.wire_size
        << ",\"wire_reserved_bytes\":" << r.wire_reserved_bytes
        << ",\"save_base_changed\":" << (r.save_base_changed ? "true" : "false")
        << ",\"create_ms\":" << r.create_ms
        << ",\"allocate_ms\":" << r.allocate_ms
        << ",\"free_ms\":" << r.free_ms
        << ",\"destroy_ms\":" << r.destroy_ms;
    out << ",\"operation_samples_ms\":"; print_samples(out, r.operation_samples_ms);
    out << ",\"growth_samples_ms\":"; print_samples(out, r.growth_samples_ms);
    out << ",\"memory_before\":"; print_memory(out, r.memory_before);
    out << ",\"memory_after\":"; print_memory(out, r.memory_after);
    out << ",\"memory_live\":"; print_memory(out, r.memory_live);
    out << ",\"memory_peak\":"; print_memory(out, r.memory_peak);
    out << ",\"memory_after_validation\":"; print_memory(out, r.memory_after_validation);
    out << ",\"memory_after_cleanup\":"; print_memory(out, r.memory_after_cleanup);
    out << "}\n";
}

std::size_t parse_size(const std::string& value) {
    std::size_t pos{};
    const auto result = std::stoull(value, &pos);
    require(pos == value.size() && !value.empty() && value.front() != '-',
            "size/reserve must be unsigned decimal integers");
    require(result <= std::numeric_limits<std::size_t>::max(), "numeric argument overflows size_t");
    return static_cast<std::size_t>(result);
}

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        require(i + 1 < argc, "every CLI flag needs one value");
        const std::string value = argv[++i];
        if (flag == "--workload") args.workload = value;
        else if (flag == "--size") args.size = parse_size(value);
        else if (flag == "--reserve") args.reserve = parse_size(value);
        else if (flag == "--cycles") args.cycles = parse_size(value);
        else if (flag == "--wire-path") args.wire_path = value;
        else throw std::runtime_error("unknown argument: " + flag);
    }
    require(!args.workload.empty(), "--workload is required");
    require(args.size > 0, "--size must be positive");
    return args;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto args = parse_args(argc, argv);
        Result result;
        if (args.workload == "grow_default" || args.workload == "grow_equal_reserve")
            result = grow_workload(args);
        else if (args.workload == "append_growth" || args.workload == "append_preallocated")
            result = append_workload(args);
        else if (args.workload == "many_default" || args.workload == "many_equal_reserve")
            result = many_workload(args);
        else if (args.workload == "save_verified") result = save_workload(args);
        else if (args.workload == "load_verified") result = load_workload(args);
        else if (args.workload == "prepare_wire") result = prepare_wire_workload(args);
        else if (args.workload == "save_grow_cycles") result = cycle_workload(args);
        else throw std::runtime_error("unknown workload: " + args.workload);
        print_result(result);
    } catch (const std::exception& e) {
        std::cerr << "VM benchmark failed: " << e.what() << '\n';
        return 1;
    }
}
