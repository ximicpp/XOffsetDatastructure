// Isolated current-F owner-index workload. Formal timing is dispatched by W3.
#ifndef N3_DIAGNOSTIC
#define N3_DIAGNOSTIC 0
#endif
#if N3_DIAGNOSTIC != 0 && N3_DIAGNOSTIC != 1
#error "N3_DIAGNOSTIC must be 0 or 1"
#endif
#if N3_DIAGNOSTIC
#include "n3_runtime.hpp"
#endif
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using Core = XOffsetDatastructure::XBufferCore;
using Allocator = XOffsetDatastructure::XArenaAllocator<std::uint64_t>;
constexpr std::size_t logical_bytes = 65536;
constexpr std::uint64_t value_marker = 0x195a926a5cc24873ULL;
constexpr std::uint64_t hash_seed = 14695981039346656037ULL;
constexpr std::uint64_t hash_prime = 1099511628211ULL;

enum class Control { none, partial_startup, fail_create, fail_allocate, fail_verify };
enum class Failure {
    none, partial_startup, fail_create, fail_allocate, fail_verify,
    startup_exception, worker_exception, main_exception, invalid_geometry,
    value_mismatch, conservation_mismatch, progress_mismatch, invalid_cli, diagnostic_trace_invalid
};
enum class Stage { create, inspect_create, allocate, fill, free, inspect_free, destroy };

const char* control_name(Control control) noexcept {
    switch (control) {
    case Control::none: return "none";
    case Control::partial_startup: return "partial_startup";
    case Control::fail_create: return "fail_create";
    case Control::fail_allocate: return "fail_allocate";
    case Control::fail_verify: return "fail_verify";
    }
    return "none";
}
const char* failure_reason(Failure failure) noexcept {
    switch (failure) {
    case Failure::none: return "";
    case Failure::partial_startup: return "control:partial_startup";
    case Failure::fail_create: return "control:fail_create";
    case Failure::fail_allocate: return "control:fail_allocate";
    case Failure::fail_verify: return "control:fail_verify";
    case Failure::startup_exception: return "startup_exception";
    case Failure::worker_exception: return "worker_exception";
    case Failure::main_exception: return "main_exception";
    case Failure::invalid_geometry: return "invalid_geometry";
    case Failure::value_mismatch: return "value_mismatch";
    case Failure::conservation_mismatch: return "conservation_mismatch";
    case Failure::progress_mismatch: return "progress_mismatch";
    case Failure::invalid_cli: return "invalid_cli";
    case Failure::diagnostic_trace_invalid: return "diagnostic_trace_invalid";
    }
    return "unknown_failure";
}
const char* stage_name(Stage stage) noexcept {
    switch (stage) {
    case Stage::create: return "create";
    case Stage::allocate: return "allocate";
    case Stage::free: return "free";
    case Stage::destroy: return "destroy";
    case Stage::inspect_create:
    case Stage::fill:
    case Stage::inspect_free: return "fill";
    }
    return "fill";
}

struct Options {
    std::size_t owners = 0, threads = 0, requests = 0;
    Control control = Control::none;
};
std::size_t number(std::string_view value) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("expected a decimal integer");
    return result;
}
void parse_options(Options& options, int argc, char** argv) {
    unsigned seen = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        if (++i == argc) throw std::invalid_argument("missing argument value");
        const std::string_view value(argv[i]);
        unsigned bit = 0;
        if (flag == "--owners") { bit = 1; options.owners = number(value); }
        else if (flag == "--threads") { bit = 2; options.threads = number(value); }
        else if (flag == "--requests") { bit = 4; options.requests = number(value); }
        else if (flag == "--control") {
            bit = 8;
            if (value == "none") options.control = Control::none;
            else if (value == "partial_startup") options.control = Control::partial_startup;
            else if (value == "fail_create") options.control = Control::fail_create;
            else if (value == "fail_allocate") options.control = Control::fail_allocate;
            else if (value == "fail_verify") options.control = Control::fail_verify;
            else throw std::invalid_argument("unknown control");
        } else throw std::invalid_argument("unknown argument");
        if (seen & bit) throw std::invalid_argument("duplicate argument");
        seen |= bit;
    }
    if ((seen & 7) != 7) throw std::invalid_argument("owners, threads and requests are required");
    const bool condition =
        (options.threads == 1 && (options.owners == 8 || options.owners == 64 || options.owners == 256)) ||
        (options.owners == 256 && (options.threads == 2 || options.threads == 4 || options.threads == 8));
    if (!condition) throw std::invalid_argument("condition is outside the six frozen cases");
    if (options.requests != 256 && options.requests != 8192)
        throw std::invalid_argument("requests must be 256 or 8192");
#if N3_DIAGNOSTIC
    if (options.requests != 256) throw std::invalid_argument("diagnostic requests must be 256");
#endif
    if (options.control != Control::none) {
        const bool startup = options.control == Control::partial_startup;
        if (options.requests != 256 || options.owners != (startup ? 256u : 8u) ||
            options.threads != (startup ? 4u : 1u))
            throw std::invalid_argument("control condition differs from frozen case");
    }
}

struct Owner {
    std::unique_ptr<Core> object;
    std::size_t index = 0, worker = 0;
    std::uintptr_t core = 0, base = 0;
    std::size_t logical = 0, used_end = 0, free_before = 0, free_after = 0;
    std::size_t allocated = 0, freed = 0;
    bool created = false, destroyed = false, conserved = true;
};
struct Range { std::uintptr_t begin = 0, end = 0; };
struct Counts {
    std::size_t threads_started = 0, threads_joined = 0;
    std::size_t created = 0, allocated = 0, freed = 0, destroyed = 0, validated = 0;
};
struct Validation {
    bool values = false, alignment = true, ranges = true, nonoverlap = true, conservation = true;
};
struct Report {
    Options options;
    Counts counts;
    Validation validation;
    std::vector<Owner> owners;
    std::vector<std::uint64_t*> pointers;
    std::vector<Range> ranges;
    std::vector<std::exception_ptr> worker_errors;
    std::exception_ptr main_error;
    std::atomic<Failure> failure{Failure::none};
    std::optional<std::uint64_t> wall[4];
    std::uint64_t checksum = hash_seed;
    explicit Report(Options value, bool allocate_metadata = true) : options(value) {
        if (!allocate_metadata) return;
        owners.resize(value.owners);
        pointers.resize(value.requests, nullptr);
        ranges.resize(value.requests);
        worker_errors.resize(value.threads);
        for (std::size_t i = 0; i < owners.size(); ++i) {
            owners[i].index = i;
            owners[i].worker = i % value.threads;
        }
    }
    void fail(Failure cause) noexcept {
        auto expected = Failure::none;
        failure.compare_exchange_strong(expected, cause);
    }
};
struct ControlSignal { Failure cause; };

// Every library operation, including failure cleanup, belongs to index % T.
void worker_stage(Report& report, std::size_t lane, Stage stage) {
#if N3_DIAGNOSTIC
    n3::Phase phase(stage_name(stage));
#endif
    const auto& options = report.options;
    const auto per_owner = options.requests / options.owners;
    for (std::size_t i = lane; i < options.owners; i += options.threads) {
        auto& owner = report.owners[i];
        if (stage == Stage::create) {
            if (report.failure.load() != Failure::none) return;
            if (options.control == Control::fail_create && i == 3)
                throw ControlSignal{Failure::fail_create};
            owner.object = std::make_unique<Core>(logical_bytes, Core::maximum_capacity(logical_bytes));
            owner.created = true;
        } else if (stage == Stage::inspect_create) {
            if (!owner.object) continue;
            owner.core = reinterpret_cast<std::uintptr_t>(owner.object.get());
            owner.base = reinterpret_cast<std::uintptr_t>(owner.object->data());
            owner.logical = owner.object->get_size();
            owner.used_end = owner.object->arena()->used_end;
            if (owner.object->maximum_capacity() != logical_bytes)
                report.fail(Failure::invalid_geometry);
        } else if (stage == Stage::allocate) {
            if (report.failure.load() != Failure::none) return;
#if N3_DIAGNOSTIC
            n3::Scope scope(reinterpret_cast<const void*>(owner.base));
#endif
            Allocator allocator(owner.object->arena());
            for (std::size_t j = 0; j < per_owner; ++j) {
                if (report.failure.load() != Failure::none) return;
                const auto k = i * per_owner + j;
                if (options.control == Control::fail_allocate && k == 7)
                    throw ControlSignal{Failure::fail_allocate};
                report.pointers[k] = allocator.allocate(1);
                ++owner.allocated;
            }
        } else if (stage == Stage::fill) {
            if (!owner.object) continue;
            owner.used_end = owner.object->arena()->used_end;
            owner.free_before = owner.object->get_free_memory();
            for (std::size_t j = 0; j < owner.allocated; ++j) {
                const auto k = i * per_owner + j;
                const auto value = value_marker ^ static_cast<std::uint64_t>(k);
                *report.pointers[k] = value ^
                    (options.control == Control::fail_verify && k == 7 ? 1ULL : 0ULL);
            }
        } else if (stage == Stage::free) {
            if (!owner.object) continue;
#if N3_DIAGNOSTIC
            n3::Scope scope(reinterpret_cast<const void*>(owner.base));
#endif
            Allocator allocator(owner.object->arena());
            for (std::size_t j = owner.freed; j < owner.allocated; ++j) {
                allocator.deallocate(report.pointers[i * per_owner + j], 1);
                ++owner.freed;
            }
        } else if (stage == Stage::inspect_free) {
            if (!owner.object) continue;
            owner.free_after = owner.object->get_free_memory();
            const auto reclaimed = owner.allocated *
                (sizeof(Core::AllocationHeader) + sizeof(std::uint64_t));
            owner.conserved = owner.freed == owner.allocated &&
                reinterpret_cast<std::uintptr_t>(owner.object->data()) == owner.base &&
                owner.object->get_size() == owner.logical && owner.logical == logical_bytes &&
                owner.object->arena()->used_end == owner.used_end &&
                owner.free_after == owner.free_before + reclaimed;
            if (!owner.conserved) report.fail(Failure::conservation_mismatch);
        } else if (stage == Stage::destroy) {
            if (!owner.object) continue;
#if N3_DIAGNOSTIC
            n3::Scope scope(reinterpret_cast<const void*>(owner.base));
#endif
            owner.object.reset();
            owner.destroyed = true;
        }
    }
}

// The workers are stopped at the next barrier. The coordinator uses saved
// metadata and payload only; it never calls the owner/allocator/registry APIs.
void verify(Report& report) {
    const auto per_owner = report.options.requests / report.options.owners;
    std::size_t range_count = 0;
    for (const auto& owner : report.owners) {
        if (!owner.created) continue;
        report.validation.ranges = report.validation.ranges && owner.core != 0 && owner.base != 0 &&
            owner.logical == logical_bytes && owner.used_end <= owner.logical;
        for (std::size_t j = 0; j < owner.allocated; ++j) {
            const auto pointer = reinterpret_cast<std::uintptr_t>(report.pointers[owner.index * per_owner + j]);
            const bool inside = pointer >= owner.base && pointer - owner.base <= owner.logical &&
                sizeof(std::uint64_t) <= owner.logical - (pointer - owner.base);
            report.validation.ranges = report.validation.ranges && inside;
            const auto alignment = std::max(alignof(std::uint64_t), alignof(std::max_align_t));
            report.validation.alignment = report.validation.alignment && pointer != 0 && pointer % alignment == 0;
            if (inside) report.ranges[range_count++] = {pointer, pointer + sizeof(std::uint64_t)};
        }
    }
    std::sort(report.ranges.begin(), report.ranges.begin() + static_cast<std::ptrdiff_t>(range_count),
              [](const Range& left, const Range& right) { return left.begin < right.begin; });
    for (std::size_t i = 1; i < range_count; ++i)
        report.validation.nonoverlap = report.validation.nonoverlap &&
            report.ranges[i - 1].end <= report.ranges[i].begin;
    if (!report.validation.ranges || !report.validation.alignment || !report.validation.nonoverlap) {
        report.fail(Failure::invalid_geometry);
        return;
    }
    // Preserve the exact progress of earlier create/allocate controls.
    if (report.failure.load() != Failure::none) return;
    for (std::size_t k = 0; k < report.pointers.size(); ++k) {
        const auto expected = value_marker ^ static_cast<std::uint64_t>(k);
        if (*report.pointers[k] != expected) {
            const bool injected = report.options.control == Control::fail_verify && k == 7 &&
                *report.pointers[k] == (expected ^ 1ULL);
            report.fail(injected ? Failure::fail_verify : Failure::value_mismatch);
            return;
        }
        ++report.counts.validated;
        report.checksum = (report.checksum ^ *report.pointers[k]) * hash_prime;
    }
    report.validation.values = true;
}

struct Team {
    Report& report;
    std::barrier<> barrier;
    std::mutex mutex;
    std::condition_variable changed;
    std::size_t ready = 0;
    bool start = false, abort = false;
    std::vector<std::thread> threads;
    explicit Team(Report& value)
        : report(value), barrier(static_cast<std::ptrdiff_t>(value.options.threads + 1)) {
        threads.reserve(value.options.threads);
    }
    void worker(std::size_t lane) {
        {
            std::unique_lock lock(mutex);
            ++ready;
            changed.notify_all();
            changed.wait(lock, [&] { return start || abort; });
            if (abort) return;
        }
        // Untimed rendezvous removes team construction from the first phase.
        barrier.arrive_and_wait();
        constexpr Stage stages[] = {Stage::create, Stage::inspect_create, Stage::allocate,
            Stage::fill, Stage::free, Stage::inspect_free, Stage::destroy};
        for (const auto stage : stages) {
            barrier.arrive_and_wait();
            try {
                worker_stage(report, lane, stage);
            } catch (const ControlSignal& signal) {
                report.fail(signal.cause);
            } catch (...) {
                if (!report.worker_errors[lane]) report.worker_errors[lane] = std::current_exception();
                report.fail(Failure::worker_exception);
            }
            barrier.arrive_and_wait();
        }
    }
    void join() {
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
                ++report.counts.threads_joined;
            }
        }
    }
};

void collect_counts(Report& report) noexcept {
    for (const auto& owner : report.owners) {
        report.counts.created += owner.created;
        report.counts.allocated += owner.allocated;
        report.counts.freed += owner.freed;
        report.counts.destroyed += owner.destroyed;
        report.validation.conservation = report.validation.conservation && owner.conserved &&
            !owner.object && owner.created == owner.destroyed && owner.allocated == owner.freed;
    }
}
void run(Report& report) {
    Team team(report);
    try {
        for (std::size_t lane = 0; lane < report.options.threads; ++lane) {
            if (report.options.control == Control::partial_startup && lane == 2)
                throw ControlSignal{Failure::partial_startup};
            team.threads.emplace_back([&team, lane] { team.worker(lane); });
            ++report.counts.threads_started;
        }
    } catch (const ControlSignal& signal) {
        report.fail(signal.cause);
    } catch (...) {
        report.main_error = std::current_exception();
        report.fail(Failure::startup_exception);
    }
    if (report.failure.load() != Failure::none) {
        {
            std::lock_guard lock(team.mutex);
            team.abort = true;
        }
        team.changed.notify_all();
        team.join();
        collect_counts(report);
        return;
    }
    try {
        std::unique_lock lock(team.mutex);
        team.changed.wait(lock, [&] { return team.ready == report.options.threads; });
        team.start = true;
    } catch (...) {
        report.main_error = std::current_exception();
        report.fail(Failure::startup_exception);
        {
            std::lock_guard lock(team.mutex);
            team.abort = true;
        }
        team.changed.notify_all();
        team.join();
        collect_counts(report);
        return;
    }
    team.changed.notify_all();
    team.barrier.arrive_and_wait();
    constexpr Stage stages[] = {Stage::create, Stage::inspect_create, Stage::allocate,
        Stage::fill, Stage::free, Stage::inspect_free, Stage::destroy};
    for (const auto stage : stages) {
        int clock_index = -1;
        if (stage == Stage::create) clock_index = 0;
        else if (stage == Stage::allocate) clock_index = 1;
        else if (stage == Stage::free) clock_index = 2;
        else if (stage == Stage::destroy) clock_index = 3;
#if !N3_DIAGNOSTIC
        std::chrono::steady_clock::time_point from{};
        if (clock_index >= 0) from = std::chrono::steady_clock::now();
#endif
        team.barrier.arrive_and_wait();
        team.barrier.arrive_and_wait();
#if !N3_DIAGNOSTIC
        if (clock_index >= 0)
            report.wall[clock_index] = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - from).count());
#endif
        if (stage == Stage::fill) {
            try { verify(report); }
            catch (...) {
                report.main_error = std::current_exception();
                report.fail(Failure::main_exception);
            }
        }
    }
    team.join();
    collect_counts(report);
}

bool expected_control(const Report& report) noexcept {
    const auto& c = report.counts;
    if (!report.validation.alignment || !report.validation.ranges ||
        !report.validation.nonoverlap || !report.validation.conservation) return false;
    switch (report.options.control) {
    case Control::none: return false;
    case Control::partial_startup:
        return report.failure.load() == Failure::partial_startup && c.threads_started == 2 &&
            c.threads_joined == 2 && c.created == 0 && c.allocated == 0 && c.freed == 0 &&
            c.destroyed == 0 && c.validated == 0;
    case Control::fail_create:
        return report.failure.load() == Failure::fail_create && c.threads_started == 1 &&
            c.threads_joined == 1 && c.created == 3 && c.allocated == 0 && c.freed == 0 &&
            c.destroyed == 3 && c.validated == 0;
    case Control::fail_allocate:
        return report.failure.load() == Failure::fail_allocate && c.threads_started == 1 &&
            c.threads_joined == 1 && c.created == 8 && c.allocated == 7 && c.freed == 7 &&
            c.destroyed == 8 && c.validated == 0;
    case Control::fail_verify:
        return report.failure.load() == Failure::fail_verify && c.threads_started == 1 &&
            c.threads_joined == 1 && c.created == 8 && c.allocated == 256 && c.freed == 256 &&
            c.destroyed == 8 && c.validated == 7 && !report.validation.values;
    }
    return false;
}
const char* status(Report& report) noexcept {
    if (expected_control(report)) return "expected_failure";
    const auto& c = report.counts;
    const auto& v = report.validation;
    const bool complete = report.options.control == Control::none &&
        c.threads_started == report.options.threads && c.threads_joined == report.options.threads &&
        c.created == report.options.owners && c.destroyed == c.created &&
        c.allocated == report.options.requests && c.freed == c.allocated && c.validated == c.allocated &&
        v.values && v.alignment && v.ranges && v.nonoverlap && v.conservation;
    if (report.failure.load() == Failure::none && complete) return "ok";
    if (!v.conservation) report.failure.store(Failure::conservation_mismatch);
    else if (!v.ranges || !v.alignment || !v.nonoverlap) report.failure.store(Failure::invalid_geometry);
    else if (report.failure.load() == Failure::none ||
             report.failure.load() == Failure::partial_startup ||
             report.failure.load() == Failure::fail_create ||
             report.failure.load() == Failure::fail_allocate ||
             report.failure.load() == Failure::fail_verify)
        report.failure.store(Failure::progress_mismatch);
    return "error";
}

void print_bool(std::ostream& out, bool value) { out << (value ? "true" : "false"); }
std::string json(Report& report, const char* result_status) {
    std::ostringstream out;
    const auto& o = report.options;
    const auto& c = report.counts;
    const auto& v = report.validation;
    out << "{\"schema_version\":1,\"profile\":\"" << (N3_DIAGNOSTIC ? "diagnostic" : "timing")
        << "\",\"timing_evidence\":false,\"condition\":{\"owners\":" << o.owners
        << ",\"threads\":" << o.threads << ",\"requests\":" << o.requests
        << ",\"per_owner\":" << (o.owners ? o.requests / o.owners : 0)
        << "},\"control\":\"" << control_name(o.control) << "\",\"status\":\"" << result_status << "\",\"reason\":";
    const auto failure = report.failure.load();
    if (failure == Failure::none) out << "null";
    else out << '"' << failure_reason(failure) << '"';
    out << ",\"counts\":{\"threads_started\":" << c.threads_started << ",\"threads_joined\":" << c.threads_joined
        << ",\"created\":" << c.created << ",\"allocated\":" << c.allocated << ",\"freed\":" << c.freed
        << ",\"destroyed\":" << c.destroyed << ",\"validated\":" << c.validated
        << "},\"checksum\":\"" << report.checksum << "\",\"phases\":{";
    constexpr const char* names[] = {"create", "allocate", "free", "destroy"};
    for (std::size_t i = 0; i < 4; ++i) {
        if (i) out << ',';
        out << '"' << names[i] << "\":{\"wall_ns\":";
        if (report.wall[i]) out << *report.wall[i]; else out << "null";
        out << '}';
    }
    out << "},\"owners\":[";
    for (std::size_t i = 0; i < report.owners.size(); ++i) {
        if (i) out << ',';
        const auto& owner = report.owners[i];
        out << "{\"index\":" << owner.index << ",\"worker\":" << owner.worker
            << ",\"core\":\"" << owner.core << "\",\"base\":\"" << owner.base
            << "\",\"logical\":" << owner.logical << ",\"used_end\":" << owner.used_end
            << ",\"free_before\":" << owner.free_before << ",\"free_after\":" << owner.free_after
            << ",\"created\":";
        print_bool(out, owner.created);
        out << ",\"allocated\":" << owner.allocated << ",\"freed\":" << owner.freed << ",\"destroyed\":";
        print_bool(out, owner.destroyed);
        out << '}';
    }
    out << "],\"validation\":{\"values\":"; print_bool(out, v.values);
    out << ",\"alignment\":"; print_bool(out, v.alignment);
    out << ",\"ranges\":"; print_bool(out, v.ranges);
    out << ",\"nonoverlap\":"; print_bool(out, v.nonoverlap);
    out << ",\"conservation\":"; print_bool(out, v.conservation);
    out << "},\"trace\":";
#if N3_DIAGNOSTIC
    n3::dump(out);
#else
    out << "null";
#endif
    out << "}\n";
    return out.str();
}
void print_exception(const std::exception_ptr& error) {
    if (!error) return;
    try { std::rethrow_exception(error); }
    catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; }
    catch (...) { std::cerr << "non-standard exception\n"; }
}
} // namespace

int main(int argc, char** argv) {
    Options options;
    try {
        parse_options(options, argc, argv);
    } catch (...) {
        print_exception(std::current_exception());
        Report error(options, false);
        error.fail(Failure::invalid_cli);
        std::cout << json(error, "error");
        return 2;
    }
    try {
        Report report(options);
        run(report);
#if N3_DIAGNOSTIC
        if (n3::dropped_count() != 0 || n3::event_count() > 5631)
            report.failure.store(Failure::diagnostic_trace_invalid);
#endif
        const auto* result_status = status(report);
        for (const auto& error : report.worker_errors) print_exception(error);
        print_exception(report.main_error);
        std::cout << json(report, result_status);
        return std::string_view(result_status) == "ok" ? 0 : 2;
    } catch (...) {
        print_exception(std::current_exception());
        Report error(options, false);
        error.fail(Failure::main_exception);
        std::cout << json(error, "error");
        return 2;
    }
}
