#pragma once
// Diagnostic-only POD state. Never include in bare performance binaries.
#include <cstdint>
#include <ctime>
#include <sys/resource.h>
#include <type_traits>
namespace c14_diag {
struct Counts {
    std::uint64_t canonical{}, inactive{}, active{}, owns{}, vectors{}, capacity_hits{},
        nontrivial_misses{}, nonempty_misses{}, vector_data_reads{};
};
struct Sample {
    std::uint64_t ns{}, utime_us{}, stime_us{}, minor{}, major{}, bytes{};
    int clock_status{}, rusage_status{};
};
struct Emit { Sample at[10]{}; std::uint32_t seen{}; std::uint32_t invocations{}; };
static_assert(std::is_trivially_copyable_v<Counts> && std::is_trivially_copyable_v<Emit>);
inline thread_local bool enabled = false;
inline thread_local Counts counts{};
inline thread_local Counts phases[9]{};
inline thread_local Counts mutations[10]{};
inline thread_local std::uint64_t mutation_seen{};
inline thread_local Emit emit{};
inline thread_local unsigned owns_depth = 0;
inline Sample sample(std::uint64_t bytes = 0) noexcept {
    timespec ts{}; rusage usage{};
    const int clock_status = clock_gettime(CLOCK_MONOTONIC, &ts);
    const int rusage_status = getrusage(RUSAGE_SELF, &usage);
    return {std::uint64_t(ts.tv_sec) * 1000000000ULL + std::uint64_t(ts.tv_nsec),
        std::uint64_t(usage.ru_utime.tv_sec)*1000000ULL + std::uint64_t(usage.ru_utime.tv_usec),
        std::uint64_t(usage.ru_stime.tv_sec)*1000000ULL + std::uint64_t(usage.ru_stime.tv_usec),
        std::uint64_t(usage.ru_minflt), std::uint64_t(usage.ru_majflt), bytes, clock_status, rusage_status};
}
inline void emit_mark(unsigned index, std::uint64_t bytes = 0) noexcept {
    if (enabled && index < 10) { emit.at[index] = sample(bytes); emit.seen |= 1u << index; }
}
inline void phase(unsigned n) noexcept { if (enabled && n < 9) phases[n] = counts; }
struct OwnsScope {
    bool observed;
    OwnsScope() noexcept : observed(enabled) { if (observed) { ++counts.owns; ++owns_depth; } }
    ~OwnsScope() noexcept { if (observed) --owns_depth; }
};
struct MutationScope {
    unsigned stage;
    explicit MutationScope(unsigned s) noexcept : stage(s) {}
    ~MutationScope() noexcept {
        if (enabled && stage < 10) { mutations[stage] = counts; mutation_seen |= 1ULL << stage; }
    }
};
// 0 enter/normalization start; 1 normalization+root/allocator done;
// 2 header ready; 3 string/checks ready immediately before resize_and_overwrite;
// 4 callback entry (string preparation complete); 5 header copy done;
// 6 payload copy done; 7 resize_and_overwrite returned; 8 save_verified return.
// These are nested intervals. They must not be added as independent costs.
}
