#pragma once
#include <array>
#include <cstddef>
#include <exception>
#include <mutex>

#if defined(_WIN32)
# if defined(XOFFSET_SNAPSHOT_INDEX_RUNTIME_BUILD)
#  define XOFFSET_SNAPSHOT_INDEX_EXPORT __declspec(dllexport)
# else
#  define XOFFSET_SNAPSHOT_INDEX_EXPORT __declspec(dllimport)
# endif
#else
# define XOFFSET_SNAPSHOT_INDEX_EXPORT __attribute__((visibility("default")))
#endif

namespace XOffsetDatastructure::snapshot_index_detail {
    inline constexpr std::size_t owner_limit = 2u * 1024u * 1024u;
    inline constexpr std::size_t process_limit = 32u * 1024u * 1024u;
    // Exact requested host INDEX bytes; no claim about malloc/OS overhead,
    // pre-existing snapshot allocation, caller handles or whole-process RSS.
    struct BudgetRegistry {
        struct Owner { const void* key{}; std::size_t bytes{}; };
        std::mutex mutex;
        std::array<Owner, 256> owners{};
        std::size_t bytes = sizeof(BudgetRegistry);
    };
    // Exactly one optional SHARED runtime definition, exported even when
    // consumers use hidden visibility. Do not statically link one copy per DSO.
    extern "C" XOFFSET_SNAPSHOT_INDEX_EXPORT BudgetRegistry*
        xoffset_snapshot_index_shared_budget_v1() noexcept;

    inline BudgetRegistry* registry() noexcept {
#if defined(XOFFSET_SNAPSHOT_INDEX_SHARED_RUNTIME)
        return xoffset_snapshot_index_shared_budget_v1();
#else
        // Header-only consumers keep their old no-link contract. The optional
        // index fails closed instead of pretending a per-DSO inline variable
        // enforces a process-wide bound.
        return nullptr;
#endif
    }
    inline bool reserve(const void* key, std::size_t bytes) {
        auto* state = registry();
        if (!state || !key || bytes > owner_limit) return false;
        std::lock_guard lock(state->mutex);
        if (bytes > process_limit - state->bytes) return false;
        BudgetRegistry::Owner* slot = nullptr;
        BudgetRegistry::Owner* empty = nullptr;
        for (auto& owner : state->owners) {
            if (owner.key == key) { slot = &owner; break; }
            if (!owner.key && !empty) empty = &owner;
        }
        if (!slot) slot = empty;
        if (!slot || bytes > owner_limit - slot->bytes) return false;
        slot->key = key; slot->bytes += bytes; state->bytes += bytes;
        return true;
    }
    inline void release(const void* key, std::size_t bytes) noexcept {
        auto* state = registry();
        if (!state) std::terminate();
        std::lock_guard lock(state->mutex);
        for (auto& owner : state->owners) if (owner.key == key) {
            owner.bytes -= bytes; state->bytes -= bytes;
            if (!owner.bytes) owner.key = nullptr;
            return;
        }
        std::terminate();
    }
    inline std::size_t process_requested_bytes() {
        auto* state = registry();
        if (!state) return 0;
        std::lock_guard lock(state->mutex);
        return state->bytes;
    }
} // namespace XOffsetDatastructure::snapshot_index_detail
