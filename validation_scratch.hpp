#pragma once
// Explicit, opt-in host scratch. This file has no dependency on XOffset/reflection.
// It is an OPTIONAL compiled runtime for a NEW explicit scratch API. Existing
// header-only XOffset load calls must neither include a default context nor link
// this target implicitly. Production integration is a separate caller decision.
//
// Integration: acquire a Lease, construct NEW validation indexes for that call,
// using owned_resource()/free_resource(), and destroy the indexes before Lease.
// Alternatively construct NEW monotonic_buffer_resources with upstream_resource()
// and destroy them before Lease. Never retain an index or validation conclusion.
//
// Context and custom upstream must outlive all primary leases and trim callbacks.
// Context destruction must not race external use, acquire or trim. Destruction
// marks closing BEFORE releasing idle blocks; a synchronous callback acquire then
// obtains an independent new_delete fallback with no retained context pointer.
// Custom upstream must be thread-safe/reentrant for any operations the caller
// permits to overlap, including trim; otherwise the caller must serialize those
// operations. Busy fallback deliberately does not use that custom upstream.
// The 32 MiB bound applies ONLY to cached idle raw blocks. Active allocations
// and detached blocks whose upstream deallocation has not returned are separate;
// retained=active+cached+pending_release can exceed 32 MiB, including in callbacks.
// New cache admission also charges ALL pending releases against available idle
// headroom, preventing trim callbacks from replacing an entire unfreed idle pool.
//
// Every caller must link the ONE shared registry target supplied by CMake,
// including callers in different DSOs. Do not embed independent registry copies
// or unload that target before every context, lease and callback is destroyed.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <new>
#include <stdexcept>

namespace xoffset_scratch {

struct Limits {
    static constexpr std::size_t max_cached_block_bytes = 8u * 1024u * 1024u;
    static constexpr std::size_t max_idle_bytes = 32u * 1024u * 1024u;
    static constexpr std::size_t idle_slots = 128;
};

struct Statistics {
    // All byte counters are ACTUAL upstream allocation bytes, including the
    // in-block provenance header, not RSS or application requested payload.
    std::size_t active_bytes = 0, active_blocks = 0;
    std::size_t cached_bytes = 0, cached_blocks = 0;
    std::size_t pending_release_bytes = 0, pending_release_blocks = 0;
    std::size_t retained_bytes = 0, retained_blocks = 0;
    std::size_t cache_hits = 0, fresh_allocations = 0;
    std::size_t upstream_failures = 0, rejected_deallocations = 0;
    std::size_t idle_budget_misses = 0, idle_slot_misses = 0;
};

namespace detail {
class Resource;
enum class BlockState : unsigned { active, idle, releasing };

struct alignas(std::max_align_t) Header {
    void* raw = nullptr;
    std::size_t raw_bytes = 0, raw_alignment = 0, user_capacity = 0;
    std::size_t requested_bytes = 0, requested_alignment = 0;
    std::pmr::memory_resource* upstream = nullptr;
    Resource* owner = nullptr;
    BlockState state = BlockState::active;
};

struct Plan { std::size_t raw_bytes, raw_alignment, user_offset; };
inline Plan allocation_plan(std::size_t bytes, std::size_t alignment) {
    if (!alignment || (alignment & (alignment - 1u))) XOFFSET_THROW(std::bad_alloc());
    const auto a = alignment < alignof(Header) ? alignof(Header) : alignment;
    constexpr auto maximum = (std::numeric_limits<std::size_t>::max)();
    if (sizeof(Header) > maximum - (a - 1u)) XOFFSET_THROW(std::bad_alloc());
    const auto offset = (sizeof(Header) + a - 1u) & ~(a - 1u);
    const auto payload = bytes ? bytes : std::size_t{1};
    if (payload > maximum - offset) XOFFSET_THROW(std::bad_alloc());
    return {offset + payload, a, offset};
}

struct ReturnBlock {
    void* raw = nullptr;
    std::size_t bytes = 0, alignment = 0;
    std::pmr::memory_resource* upstream = nullptr;
};

struct Pool {
    std::mutex mutex;
    std::array<Header*, Limits::idle_slots> idle{};
    Statistics counts{};
    // Intentionally no upstream cleanup in static destruction. Each context
    // detaches its idle items before its own/custom upstream lifetime ends.
};

#if defined(_WIN32)
#if defined(XOFFSET_SCRATCH_BUILDING_LIBRARY)
#define XOFFSET_SCRATCH_API __declspec(dllexport)
#else
#define XOFFSET_SCRATCH_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define XOFFSET_SCRATCH_API __attribute__((visibility("default")))
#else
#define XOFFSET_SCRATCH_API
#endif
XOFFSET_SCRATCH_API Pool& pool();
#undef XOFFSET_SCRATCH_API

inline Statistics statistics() {
    auto& p = pool();
    std::lock_guard<std::mutex> guard(p.mutex);
    auto result = p.counts;
    result.retained_bytes = result.active_bytes + result.cached_bytes + result.pending_release_bytes;
    result.retained_blocks = result.active_blocks + result.cached_blocks + result.pending_release_blocks;
    return result;
}

inline ReturnBlock detach_for_release(Header* h, Pool& p) noexcept {
    h->state = BlockState::releasing;
    ++p.counts.pending_release_blocks;
    p.counts.pending_release_bytes += h->raw_bytes;
    return {h->raw, h->raw_bytes, h->raw_alignment, h->upstream};
}

inline void return_upstream(ReturnBlock block) noexcept {
    // Deallocation is required not to throw by memory_resource's contract.
    // No pool mutex is held across this call, including user delete callbacks.
    block.upstream->deallocate(block.raw, block.bytes, block.alignment);
    auto& p = pool();
    std::lock_guard<std::mutex> guard(p.mutex);
    p.counts.pending_release_bytes -= block.bytes;
    --p.counts.pending_release_blocks;
}

class Resource final : public std::pmr::memory_resource {
    std::pmr::memory_resource* upstream_;
    bool cache_;
    std::atomic<std::size_t> active_blocks_{0};

    static void* user(Header* h) noexcept { return static_cast<void*>(h + 1); }
    static Header* header(void* pointer) noexcept {
        return std::launder(reinterpret_cast<Header*>(static_cast<std::byte*>(pointer) - sizeof(Header)));
    }

    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        const auto plan = allocation_plan(bytes, alignment);
        auto& p = pool();
        if (cache_) {
            std::lock_guard<std::mutex> guard(p.mutex);
            for (auto& slot : p.idle) {
                auto* h = slot;
                if (!h || h->owner != this || h->user_capacity < bytes ||
                    h->raw_alignment < alignment ||
                    reinterpret_cast<std::uintptr_t>(user(h)) % alignment) continue;
                slot = nullptr;
                p.counts.cached_bytes -= h->raw_bytes;
                --p.counts.cached_blocks;
                p.counts.active_bytes += h->raw_bytes;
                ++p.counts.active_blocks;
                ++p.counts.cache_hits;
                h->requested_bytes = bytes;
                h->requested_alignment = alignment;
                h->state = BlockState::active;
                active_blocks_.fetch_add(1, std::memory_order_relaxed);
                return user(h);
            }
        }
        void* raw;
        XOFFSET_TRY {
            raw = upstream_->allocate(plan.raw_bytes, plan.raw_alignment);
        } XOFFSET_CATCH(...) {
            std::lock_guard<std::mutex> guard(p.mutex);
            ++p.counts.upstream_failures;
            XOFFSET_RETHROW; // An upstream OOM never silently retries via the fallback.
        }
        auto* result = static_cast<std::byte*>(raw) + plan.user_offset;
        auto* h = ::new (static_cast<void*>(result - sizeof(Header))) Header{
            raw, plan.raw_bytes, plan.raw_alignment, plan.raw_bytes - plan.user_offset,
            bytes, alignment, upstream_, this, BlockState::active};
        {
            std::lock_guard<std::mutex> guard(p.mutex);
            p.counts.active_bytes += h->raw_bytes;
            ++p.counts.active_blocks;
            ++p.counts.fresh_allocations;
            active_blocks_.fetch_add(1, std::memory_order_relaxed);
        }
        return result;
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        (void)deallocate_checked(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

public:
    Resource(std::pmr::memory_resource* upstream, bool cache)
        : upstream_(upstream), cache_(cache) {
        (void)pool(); // Before this resource/context can finish construction.
        if (!upstream) XOFFSET_THROW(std::invalid_argument("null scratch upstream"));
    }
    ~Resource() override {
        if (active_blocks() != 0) std::terminate();
        trim();
    }
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    std::size_t active_blocks() const noexcept {
        return active_blocks_.load(std::memory_order_relaxed);
    }

    // Diagnostic checked form: pointer MUST still designate a live block from
    // some live Resource in this component. Arbitrary pointers, use-after-free
    // and racing deallocation violate the underlying PMR caller preconditions.
    // Wrong resource/size/alignment is rejected without calling any upstream;
    // the correct owner may then release it. No exception is thrown in cleanup.
    bool deallocate_checked(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
        if (!pointer) return false;
        auto* h = header(pointer);
        auto& p = pool();
        ReturnBlock retired{};
        {
            std::lock_guard<std::mutex> guard(p.mutex);
            if (h->owner != this || h->state != BlockState::active ||
                h->requested_bytes != bytes || h->requested_alignment != alignment) {
                ++p.counts.rejected_deallocations;
                return false;
            }
            p.counts.active_bytes -= h->raw_bytes;
            --p.counts.active_blocks;
            active_blocks_.fetch_sub(1, std::memory_order_relaxed);
            if (cache_ && h->raw_bytes <= Limits::max_cached_block_bytes) {
                const auto pending = p.counts.pending_release_bytes;
                const bool room = pending < Limits::max_idle_bytes &&
                    p.counts.cached_bytes <= Limits::max_idle_bytes - pending &&
                    h->raw_bytes <= Limits::max_idle_bytes - pending - p.counts.cached_bytes;
                if (room) {
                    for (auto& slot : p.idle) {
                        if (slot) continue;
                        h->state = BlockState::idle;
                        slot = h;
                        p.counts.cached_bytes += h->raw_bytes;
                        ++p.counts.cached_blocks;
                        return true;
                    }
                    ++p.counts.idle_slot_misses;
                } else {
                    ++p.counts.idle_budget_misses;
                }
            }
            retired = detach_for_release(h, p);
        }
        return_upstream(retired);
        return true;
    }

    void trim() noexcept {
        std::array<ReturnBlock, Limits::idle_slots> retired{};
        std::size_t count = 0;
        auto& p = pool();
        {
            std::lock_guard<std::mutex> guard(p.mutex);
            for (auto& slot : p.idle) {
                auto* h = slot;
                if (!h || h->owner != this) continue;
                slot = nullptr; // Publish all old idle items detached first.
                p.counts.cached_bytes -= h->raw_bytes;
                --p.counts.cached_blocks;
                retired[count++] = detach_for_release(h, p);
            }
        }
        for (std::size_t i = 0; i < count; ++i) return_upstream(retired[i]);
        // Reentrant returns belong to a NEW pool state: never clear them here.
    }
};
} // namespace detail

class ValidationScratch {
    std::atomic<bool> closing_{false};
    std::atomic<bool> busy_{false};
    detail::Resource resource_;

    struct Gate {
        std::atomic<bool>* busy = nullptr;
        detail::Resource* resource = nullptr;
        explicit Gate(ValidationScratch& context) noexcept {
            if (context.closing_.load(std::memory_order_acquire)) return;
            bool expected = false;
            if (context.busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                busy = &context.busy_;
                resource = &context.resource_;
            }
        }
        ~Gate() {
            if (!busy) return;
            if (resource->active_blocks() != 0) std::terminate();
            busy->store(false, std::memory_order_release);
        }
        bool primary() const noexcept { return busy != nullptr; }
    };

public:
    explicit ValidationScratch(std::pmr::memory_resource* upstream = std::pmr::new_delete_resource())
        : resource_(upstream, true) {}
    ~ValidationScratch() {
        closing_.store(true, std::memory_order_release);
        // This is a lifetime precondition, enforced even with NDEBUG.
        if (busy_.load(std::memory_order_acquire) || resource_.active_blocks() != 0) std::terminate();
        resource_.trim();
    }
    ValidationScratch(const ValidationScratch&) = delete;
    ValidationScratch& operator=(const ValidationScratch&) = delete;
    ValidationScratch(ValidationScratch&&) = delete;
    ValidationScratch& operator=(ValidationScratch&&) = delete;

    class Lease {
        // Destruction order: both monotonic resources release, then independent
        // fallback dies, finally Gate checks/clears primary busy. Construction
        // failure also destroys Gate and cannot strand busy=true.
        Gate gate_;
        detail::Resource local_{std::pmr::new_delete_resource(), false};
        detail::Resource* selected_;
        std::pmr::monotonic_buffer_resource owned_;
        std::pmr::monotonic_buffer_resource free_;
        friend class ValidationScratch;
        explicit Lease(ValidationScratch& context)
            : gate_(context), selected_(gate_.primary() ? &context.resource_ : &local_),
              owned_(4096, selected_), free_(4096, selected_) {}
    public:
        ~Lease() = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&&) = delete;
        Lease& operator=(Lease&&) = delete;
        bool is_primary() const noexcept { return gate_.primary(); }
        std::pmr::memory_resource* owned_resource() noexcept { return &owned_; }
        std::pmr::memory_resource* free_resource() noexcept { return &free_; }
        std::pmr::memory_resource* upstream_resource() noexcept { return selected_; }
        bool deallocate_raw_checked(void* pointer, std::size_t bytes, std::size_t alignment) noexcept {
            return selected_->deallocate_checked(pointer, bytes, alignment);
        }
    };

    // C++17 guaranteed copy elision; no heap metadata allocation is needed.
    [[nodiscard]] Lease acquire() { return Lease(*this); }
    // May overlap a lease only if custom upstream supports that overlap.
    void trim() noexcept { resource_.trim(); }
    static Statistics statistics() { return detail::statistics(); }
    static std::size_t raw_bytes_for(std::size_t bytes, std::size_t alignment) {
        return detail::allocation_plan(bytes, alignment).raw_bytes;
    }
    static std::size_t overhead_for(std::size_t alignment) {
        return detail::allocation_plan(1, alignment).user_offset;
    }
};

} // namespace xoffset_scratch
