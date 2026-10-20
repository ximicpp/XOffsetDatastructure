#include "validation_scratch.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using xoffset_scratch::Limits;
using xoffset_scratch::ValidationScratch;

namespace {
void need(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Tracking final : std::pmr::memory_resource {
    struct Allocation { std::size_t bytes, alignment; };
    std::map<void*, Allocation> live;
    std::size_t allocations = 0, deallocations = 0;
    bool fail_next = false;
    std::function<void()> on_allocate, on_deallocate;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (std::exchange(fail_next, false)) throw std::bad_alloc();
        auto callback = std::exchange(on_allocate, {});
        if (callback) callback();
        auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        try {
            live.emplace(result, Allocation{bytes, alignment});
        } catch (...) {
            std::pmr::new_delete_resource()->deallocate(result, bytes, alignment);
            throw;
        }
        ++allocations;
        return result;
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        const auto found = live.find(pointer);
        if (found == live.end() || found->second.bytes != bytes || found->second.alignment != alignment)
            std::abort(); // Instrument the ACTUAL original upstream arguments.
        live.erase(found);
        ++deallocations;
        auto callback = std::exchange(on_deallocate, {});
        if (callback) callback();
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
    ~Tracking() override { if (!live.empty()) std::abort(); }
};

void counters_consistent() {
    const auto s = ValidationScratch::statistics();
    need(s.retained_bytes == s.active_bytes + s.cached_bytes + s.pending_release_bytes, "retained byte accounting");
    need(s.retained_blocks == s.active_blocks + s.cached_blocks + s.pending_release_blocks, "retained block accounting");
    need(s.cached_bytes <= Limits::max_idle_bytes, "global idle budget exceeded");
    need(s.cached_blocks <= Limits::idle_slots, "metadata slots exceeded");
}

void fresh_membership_every_lease() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    {
        auto lease = context.acquire();
        need(lease.is_primary(), "initial lease not primary");
        std::pmr::unordered_set<std::size_t> owned(lease.owned_resource()), free(lease.free_resource());
        for (std::size_t i = 0; i < 100; ++i) owned.insert(i);
        free.insert(700);
        need(owned.size() == 100 && free.count(700), "initial membership");
    }
    const auto previous = upstream.allocations;
    {
        auto lease = context.acquire();
        std::pmr::unordered_set<std::size_t> owned(lease.owned_resource()), free(lease.free_resource());
        need(owned.empty() && free.empty(), "cached validation conclusion leaked");
        for (std::size_t i = 0; i < 100; ++i) owned.insert(i + 1000);
        free.insert(800);
        need(!owned.count(1) && !free.count(700), "stale membership reused");
    }
    need(upstream.allocations == previous, "raw blocks not reused");
    context.trim();
    need(upstream.live.empty(), "trim left idle blocks");
}

void larger_reuse_preserves_original_size_and_alignment() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    void* original;
    {
        auto lease = context.acquire();
        original = lease.upstream_resource()->allocate(4096, 256);
        need(reinterpret_cast<std::uintptr_t>(original) % 256 == 0, "overaligned result");
        lease.upstream_resource()->deallocate(original, 4096, 256);
    }
    {
        auto lease = context.acquire();
        auto* smaller = lease.upstream_resource()->allocate(128, 16);
        need(smaller == original && upstream.allocations == 1, "larger block not reused");
        lease.upstream_resource()->deallocate(smaller, 128, 16);
    }
    context.trim(); // Tracking aborts if 128/16 replaces the original raw parameters.
    need(upstream.allocations == 1 && upstream.deallocations == 1, "asymmetric return");
}

void oversized_input_is_not_rejected() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    auto lease = context.acquire();
    const auto payload = Limits::max_cached_block_bytes + 1;
    auto* p = lease.upstream_resource()->allocate(payload, 64);
    std::memset(p, 0x5a, payload);
    lease.upstream_resource()->deallocate(p, payload, 64);
    need(upstream.live.empty(), "oversized block was cached or leaked");
}

void exact_eight_MiB_raw_boundary() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    const auto payload = Limits::max_cached_block_bytes - ValidationScratch::overhead_for(64);
    need(ValidationScratch::raw_bytes_for(payload, 64) == Limits::max_cached_block_bytes, "bad boundary fixture");
    {
        auto lease = context.acquire();
        auto* p = lease.upstream_resource()->allocate(payload, 64);
        lease.upstream_resource()->deallocate(p, payload, 64);
    }
    need(upstream.deallocations == 0, "exact cacheable boundary rejected");
    context.trim();
    {
        auto lease = context.acquire();
        auto* p = lease.upstream_resource()->allocate(payload + 1, 64);
        lease.upstream_resource()->deallocate(p, payload + 1, 64);
    }
    need(upstream.live.empty(), "over-boundary raw block cached");
}

void global_budget_shared_across_contexts() {
    Tracking upstream;
    std::array<std::unique_ptr<ValidationScratch>, 5> contexts;
    const auto payload = Limits::max_cached_block_bytes - ValidationScratch::overhead_for(16);
    for (auto& context : contexts) {
        context = std::make_unique<ValidationScratch>(&upstream);
        auto lease = context->acquire();
        auto* p = lease.upstream_resource()->allocate(payload, 16);
        lease.upstream_resource()->deallocate(p, payload, 16);
        counters_consistent();
    }
    need(ValidationScratch::statistics().cached_bytes == Limits::max_idle_bytes, "global budget not shared");
    need(upstream.deallocations == 1, "fifth large idle block not released");
    for (auto& context : contexts) context.reset();
    need(upstream.live.empty(), "context destructor left dead upstream entries");
}

void idle_metadata_slot_exhaustion_is_only_a_cache_miss() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    auto lease = context.acquire();
    std::vector<void*> active;
    active.reserve(Limits::idle_slots + 1);
    for (std::size_t i = 0; i <= Limits::idle_slots; ++i)
        active.push_back(lease.upstream_resource()->allocate(1, 1));
    for (auto* p : active) lease.upstream_resource()->deallocate(p, 1, 1);
    need(upstream.allocations == Limits::idle_slots + 1, "metadata cap limited legal allocation");
    need(upstream.deallocations == 1, "slot overflow not immediately released");
    need(ValidationScratch::statistics().cached_blocks == Limits::idle_slots, "unexpected idle slot accounting");
    context.trim();
}

void busy_same_thread_uses_independent_resource() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    auto primary = context.acquire();
    auto nested = context.acquire();
    need(primary.is_primary() && !nested.is_primary(), "same-thread exclusivity");
    auto* p = nested.upstream_resource()->allocate(64, 16);
    need(upstream.allocations == 0, "fallback used custom upstream");
    nested.upstream_resource()->deallocate(p, 64, 16);
    counters_consistent();
}

void concurrent_context_lease_is_exclusive() {
    ValidationScratch context;
    auto primary = context.acquire();
    bool fallback = false;
    std::thread other([&] {
        auto lease = context.acquire();
        fallback = !lease.is_primary();
        std::pmr::vector<int> values(lease.owned_resource());
        values.resize(100, 42);
        need(values.back() == 42, "independent concurrent scratch");
    });
    other.join();
    need(fallback && primary.is_primary(), "concurrent lease stole primary");
}

void allocate_callback_can_reenter_without_pool_lock() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    bool observed = false;
    upstream.on_allocate = [&] {
        counters_consistent();
        auto nested = context.acquire();
        need(!nested.is_primary(), "allocate callback did not use independent fallback");
        std::pmr::vector<int> values(nested.free_resource());
        values.resize(32, 7);
        context.trim();
        observed = true;
    };
    {
        auto lease = context.acquire();
        std::pmr::vector<int> values(lease.owned_resource());
        values.resize(100, 5);
    }
    need(observed && upstream.allocations == 1, "allocate callback not exercised");
}

void ordinary_trim_publishes_state_before_reentrant_return() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    {
        auto lease = context.acquire();
        auto* p = lease.upstream_resource()->allocate(64, 16);
        lease.upstream_resource()->deallocate(p, 64, 16);
    }
    bool observed = false;
    upstream.on_deallocate = [&] {
        auto state = ValidationScratch::statistics();
        need(state.cached_bytes == 0 && state.pending_release_bytes != 0, "trim unpublished state");
        auto nested = context.acquire();
        need(nested.is_primary(), "idle trim could not acquire primary");
        auto* p = nested.upstream_resource()->allocate(128, 16);
        nested.upstream_resource()->deallocate(p, 128, 16);
        observed = true;
    };
    context.trim();
    need(observed && ValidationScratch::statistics().cached_blocks == 1, "old trim erased reentrant return");
    context.trim();
    need(upstream.live.empty(), "second explicit trim failed");
}

void destructor_closing_callback_has_no_context_resource_lease() {
    Tracking upstream;
    auto context = std::make_unique<ValidationScratch>(&upstream);
    {
        auto lease = context->acquire();
        auto* p = lease.upstream_resource()->allocate(64, 16);
        lease.upstream_resource()->deallocate(p, 64, 16);
    }
    auto* address = context.get();
    bool observed = false;
    upstream.on_deallocate = [&] {
        auto nested = address->acquire();
        need(!nested.is_primary(), "closing context granted primary");
        std::pmr::vector<int> values(nested.owned_resource());
        values.resize(100, 3);
        counters_consistent();
        observed = true;
    };
    context.reset();
    need(observed && upstream.allocations == 1 && upstream.live.empty(), "closing fallback retained context upstream");
}

void full_trim_pending_bytes_prevent_reentrant_pool_replacement() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    const auto payload = Limits::max_cached_block_bytes - ValidationScratch::overhead_for(16);
    {
        auto lease = context.acquire();
        std::array<void*, 4> pointers{};
        for (auto& pointer : pointers)
            pointer = lease.upstream_resource()->allocate(payload, 16);
        for (auto* pointer : pointers)
            lease.upstream_resource()->deallocate(pointer, payload, 16);
    }
    need(ValidationScratch::statistics().cached_bytes == Limits::max_idle_bytes, "full trim fixture");
    bool observed = false;
    upstream.on_deallocate = [&] {
        const auto during = ValidationScratch::statistics();
        need(during.cached_bytes == 0 && during.pending_release_bytes == Limits::max_idle_bytes,
             "full trim did not publish detached pending bytes");
        auto nested = context.acquire();
        need(nested.is_primary(), "full trim callback primary");
        auto* p = nested.upstream_resource()->allocate(128, 16);
        nested.upstream_resource()->deallocate(p, 128, 16);
        const auto after_small_return = ValidationScratch::statistics();
        need(after_small_return.cached_bytes == 0 &&
             after_small_return.pending_release_bytes == Limits::max_idle_bytes,
             "new block cached while full old pool still pending upstream release");
        observed = true;
    };
    context.trim();
    need(observed && upstream.allocations == 5 && upstream.deallocations == 5 && upstream.live.empty(),
         "full trim callback block was not normally released");
    need(ValidationScratch::statistics().retained_bytes == 0, "full pending trim leaked raw blocks");
}

void actual_primary_upstream_OOM_propagates_and_preserves_counters() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    auto lease = context.acquire();
    need(lease.is_primary(), "OOM fixture accidentally uses fallback");
    const auto before = ValidationScratch::statistics();
    upstream.fail_next = true;
    bool failed = false;
    try { (void)lease.upstream_resource()->allocate(64, 16); }
    catch (const std::bad_alloc&) { failed = true; }
    const auto after = ValidationScratch::statistics();
    need(failed && !upstream.fail_next && upstream.allocations == 0, "OOM injection bypassed");
    need(before.active_bytes == after.active_bytes && before.cached_bytes == after.cached_bytes &&
         before.pending_release_bytes == after.pending_release_bytes, "OOM changed ownership counters");
    auto* retry = lease.upstream_resource()->allocate(64, 16);
    lease.upstream_resource()->deallocate(retry, 64, 16);
}

void independent_contexts_concurrent_returns_share_idle_budget() {
    constexpr std::size_t count = 8;
    std::array<std::unique_ptr<ValidationScratch>, count> contexts;
    for (auto& context : contexts) context = std::make_unique<ValidationScratch>();
    const auto payload = Limits::max_cached_block_bytes - ValidationScratch::overhead_for(16);
    std::atomic<std::size_t> ready{0};
    std::atomic<bool> return_now{false};
    std::mutex failure_mutex;
    std::exception_ptr failure;
    std::vector<std::thread> threads;
    threads.reserve(count);
    try {
        for (std::size_t i = 0; i < count; ++i) {
            threads.emplace_back([&, i] {
                bool announced = false;
                try {
                    auto lease = contexts[i]->acquire();
                    need(lease.is_primary(), "independent context did not grant primary");
                    auto* pointer = lease.upstream_resource()->allocate(payload, 16);
                    announced = true;
                    ready.fetch_add(1, std::memory_order_release);
                    while (!return_now.load(std::memory_order_acquire)) std::this_thread::yield();
                    lease.upstream_resource()->deallocate(pointer, payload, 16);
                    counters_consistent();
                } catch (...) {
                    if (!announced) ready.fetch_add(1, std::memory_order_release);
                    std::lock_guard<std::mutex> guard(failure_mutex);
                    if (!failure) failure = std::current_exception();
                }
            });
        }
    } catch (...) {
        return_now.store(true, std::memory_order_release);
        for (auto& thread : threads) thread.join();
        throw;
    }
    while (ready.load(std::memory_order_acquire) != count) std::this_thread::yield();
    const auto before_returns = ValidationScratch::statistics();
    return_now.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    if (failure) std::rethrow_exception(failure);
    need(before_returns.active_bytes == count * Limits::max_cached_block_bytes,
         "fixture did not hold all independent active allocations simultaneously");
    const auto after_returns = ValidationScratch::statistics();
    need(after_returns.active_blocks == 0 && after_returns.pending_release_blocks == 0,
         "concurrent returns left active or in-progress frees");
    need(after_returns.cached_bytes <= Limits::max_idle_bytes, "concurrent idle budget exceeded");
    // Keep EVERY context alive through join, then trim each separately.
    for (auto& context : contexts) context->trim();
    need(ValidationScratch::statistics().retained_bytes == 0, "concurrent contexts did not fully trim");
}

void wrong_resource_or_request_is_rejected_before_upstream() {
    Tracking first_upstream, second_upstream;
    ValidationScratch first(&first_upstream), second(&second_upstream);
    auto a = first.acquire();
    auto b = second.acquire();
    auto* p = a.upstream_resource()->allocate(100, 64);
    need(!b.deallocate_raw_checked(p, 100, 64), "wrong resource accepted");
    need(!a.deallocate_raw_checked(p, 99, 64), "wrong byte count accepted");
    need(!a.deallocate_raw_checked(p, 100, 32), "wrong alignment accepted");
    need(first_upstream.deallocations == 0 && second_upstream.deallocations == 0, "wrong-state callback reached upstream");
    need(a.deallocate_raw_checked(p, 100, 64), "correct owner could not release after rejection");
    first.trim();
}

void exception_unwinds_both_monotonic_resources_and_unlocks_context() {
    Tracking upstream;
    ValidationScratch context(&upstream);
    try {
        auto lease = context.acquire();
        std::pmr::vector<int> owned(lease.owned_resource()), free(lease.free_resource());
        owned.resize(10, 1); free.resize(20, 2);
        throw std::runtime_error("synthetic validation rejection");
    } catch (const std::runtime_error&) {}
    need(ValidationScratch::statistics().active_blocks == 0, "exception retained active scratch");
    auto retry = context.acquire();
    need(retry.is_primary(), "unwind stranded busy state");
    context.trim();
}

void explicit_external_monotonic_integration_and_overflow() {
    ValidationScratch context;
    auto lease = context.acquire();
    {
        std::pmr::monotonic_buffer_resource owned(4096, lease.upstream_resource());
        std::pmr::monotonic_buffer_resource free(4096, lease.upstream_resource());
        std::pmr::vector<int> a(&owned), b(&free);
        a.resize(100, 1); b.resize(100, 2);
        need(a.front() == 1 && b.back() == 2, "external PMR integration");
    }
    bool rejected = false;
    try { (void)lease.upstream_resource()->allocate((std::numeric_limits<std::size_t>::max)(), 16); }
    catch (const std::bad_alloc&) { rejected = true; }
    need(rejected, "raw byte overflow not rejected");
}
} // namespace

int main() {
    fresh_membership_every_lease();
    larger_reuse_preserves_original_size_and_alignment();
    oversized_input_is_not_rejected();
    exact_eight_MiB_raw_boundary();
    global_budget_shared_across_contexts();
    idle_metadata_slot_exhaustion_is_only_a_cache_miss();
    busy_same_thread_uses_independent_resource();
    concurrent_context_lease_is_exclusive();
    allocate_callback_can_reenter_without_pool_lock();
    ordinary_trim_publishes_state_before_reentrant_return();
    destructor_closing_callback_has_no_context_resource_lease();
    full_trim_pending_bytes_prevent_reentrant_pool_replacement();
    actual_primary_upstream_OOM_propagates_and_preserves_counters();
    independent_contexts_concurrent_returns_share_idle_budget();
    wrong_resource_or_request_is_rejected_before_upstream();
    exception_unwinds_both_monotonic_resources_and_unlocks_context();
    explicit_external_monotonic_integration_and_overflow();
    counters_consistent();
    const auto final = ValidationScratch::statistics();
    need(final.retained_bytes == 0 && final.retained_blocks == 0, "final scratch not fully released");
    std::cout << "17 synthetic validation scratch scenarios completed\n";
}
