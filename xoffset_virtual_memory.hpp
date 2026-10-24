#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#  include <sys/mman.h>
#  include <unistd.h>
#else
#  error "XOffset virtual memory storage requires Windows, Linux, or macOS"
#endif

namespace XOffsetDatastructure {
namespace detail {

// Deterministic, thread-local, one-shot failure injection. Enable is consumed
// only when additional pages are required; Prepare precedes logical publication.
enum class VmOperation : std::uint8_t { Reserve, Enable, Prepare, Reclaim, Protect };

inline thread_local unsigned vm_pending_failures = 0;

inline unsigned vm_operation_bit(VmOperation operation) noexcept {
    switch (operation) {
        case VmOperation::Reserve: return 1u;
        case VmOperation::Enable: return 2u;
        case VmOperation::Prepare: return 4u;
        case VmOperation::Reclaim: return 8u;
        case VmOperation::Protect: return 16u;
    }
    return 0;
}

inline void fail_next_vm_operation(VmOperation operation) noexcept {
    vm_pending_failures |= vm_operation_bit(operation);
}

inline void clear_vm_failures() noexcept { vm_pending_failures = 0; }

inline bool consume_vm_failure(VmOperation operation) noexcept {
    const unsigned bit = vm_operation_bit(operation);
    const bool fail = (vm_pending_failures & bit) != 0;
    vm_pending_failures &= ~bit;
    return fail;
}

inline bool checked_vm_round_up(std::size_t bytes, std::size_t alignment,
                                std::size_t& rounded) noexcept {
    if (alignment == 0) return false;
    const std::size_t remainder = bytes % alignment;
    const std::size_t extra = remainder == 0 ? 0 : alignment - remainder;
    if (bytes > (std::numeric_limits<std::size_t>::max)() - extra) return false;
    rounded = bytes + extra;
    return true;
}

enum class VmReclaimStatus : std::uint8_t {
    NoOp,
    Reclaimed,
    DiscardedButAccessible,
    Failed
};

// Byte counts describe completed OS operations, not an RSS measurement. A
// failed operation may still have discarded some unused pages. Live bytes,
// logical size, and the reservation are preserved in every result.
struct VmReclaimResult {
    VmReclaimStatus status{VmReclaimStatus::NoOp};
    std::size_t attempted_bytes{0};
    std::size_t discarded_bytes{0};
    std::size_t inaccessible_bytes{0};

    bool complete() const noexcept {
        return status == VmReclaimStatus::NoOp || status == VmReclaimStatus::Reclaimed;
    }
};

class VirtualMemoryStorage;

// An empty handle does not allocate. An explicitly constructed pool owns a
// bounded shared cache; live storage keeps that state alive independently of
// the caller's handle. Budgets apply only to idle mappings, never live owners.
class VmStoragePool {
public:
    struct Limits {
        std::size_t max_cached_blocks;
        std::size_t max_reserved_bytes;
        std::size_t max_accessible_bytes;
    };
    struct Stats {
        std::size_t cached_blocks{0};
        std::size_t cached_reserved_bytes{0};
        std::size_t cached_accessible_bytes{0};
        std::uint64_t hits{0};
        std::uint64_t misses{0};
    };

    VmStoragePool() noexcept = default;
    explicit VmStoragePool(Limits limits);
    bool enabled() const noexcept { return static_cast<bool>(state_); }
    Stats stats() const noexcept;
    void clear() const noexcept;

private:
    struct State;
    std::shared_ptr<State> state_;
    friend class VirtualMemoryStorage;
};

// OS storage. V2: one read/write host page precedes the payload reservation;
// its last pointer-sized slot names the current owner (host_owner). That page
// is never part of the payload, so it is never copied, emitted or exposed by
// data()/size(). Accessible means payload page permissions are read/write; it
// does not measure resident physical memory or promise future fault-free
// allocation.
class VirtualMemoryStorage {
public:
    VirtualMemoryStorage() noexcept = default;

    VirtualMemoryStorage(std::size_t logical_bytes, std::size_t maximum_capacity)
        : VirtualMemoryStorage(logical_bytes, maximum_capacity, VmStoragePool{}) {}

    VirtualMemoryStorage(std::size_t logical_bytes, std::size_t maximum_capacity,
                         const VmStoragePool& pool)
        : VirtualMemoryStorage(logical_bytes, maximum_capacity, pool, false) {}

private:
    // Only a fresh copied load exposing the entire reservation can combine
    // reservation and page enabling. Pooled owners retain their normal path.
    VirtualMemoryStorage(std::size_t logical_bytes, std::size_t maximum_capacity,
                         const VmStoragePool& pool, [[maybe_unused]] bool full_copy) {
        if (logical_bytes > maximum_capacity) {
            throw std::invalid_argument("VM initial length exceeds maximum capacity");
        }
        if (maximum_capacity > static_cast<std::size_t>(
                (std::numeric_limits<std::ptrdiff_t>::max)())) {
            throw std::length_error("VM capacity exceeds pointer difference range");
        }
        query_page_geometry();
        std::size_t reserved = 0;
        if (!checked_vm_round_up(maximum_capacity, page_size_, reserved) ||
            reserved > static_cast<std::size_t>(
                (std::numeric_limits<std::ptrdiff_t>::max)())) {
            throw std::length_error("VM reservation page rounding overflows");
        }
        maximum_capacity_ = maximum_capacity;
        if (reserved == 0) return;
        // This failure models acquisition, including a cache hit. It is checked
        // before removing an idle owner, so a rejected acquisition keeps the cache.
        if (consume_vm_failure(VmOperation::Reserve)) throw std::bad_alloc();
        pool_ = pool;
        if (!acquire_from_pool()) {
            const std::size_t prefix = page_size_;
            if (reserved > static_cast<std::size_t>(
                    (std::numeric_limits<std::ptrdiff_t>::max)()) - prefix)
                throw std::length_error("VM reservation page rounding overflows");
#if defined(_WIN32)
            auto* mapping = static_cast<char*>(::VirtualAlloc(
                nullptr, reserved + prefix, MEM_RESERVE, PAGE_NOACCESS));
            if (!mapping) throw std::bad_alloc();
            if (!::VirtualAlloc(mapping, prefix, MEM_COMMIT, PAGE_READWRITE)) {
                ::VirtualFree(mapping, 0, MEM_RELEASE);
                throw std::bad_alloc();
            }
            base_ = mapping + prefix;
#else
            if (full_copy && consume_vm_failure(VmOperation::Enable)) throw std::bad_alloc();
            void* mapping = ::mmap(nullptr, reserved + prefix,
                                   full_copy ? PROT_READ | PROT_WRITE : PROT_NONE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapping == MAP_FAILED) throw std::bad_alloc();
            if (!full_copy && ::mprotect(mapping, prefix, PROT_READ | PROT_WRITE) != 0) {
                ::munmap(mapping, reserved + prefix);
                throw std::bad_alloc();
            }
            base_ = static_cast<char*>(mapping) + prefix;
            if (full_copy) accessible_bytes_ = reserved;
#endif
            reserved_bytes_ = reserved;
        }
        set_host_owner(nullptr);
        // A throwing constructor does not run this object's destructor.
        if (!grow_to(logical_bytes)) {
            release();
            throw std::bad_alloc();
        }
    }

public:
    // Internal direct-receive owner. All explicit maximum bytes are made
    // accessible before beginning one byte-array lifetime. This commits virtual
    // backing/accounting up front, but does not initialize/touch the whole max.
    // Later grow_to retains that array and zeroes only newly logical bytes.
    // W9: the array (canonical_byte_origin) is still begun and registered for
    // the public canonical_bytes_at API and the raw receive validator, but the
    // library's own resolution of received data is flat origin + delta, the
    // same disclosed flat-address model as ordinary loads.
    static VirtualMemoryStorage for_receive(std::size_t logical_bytes,
                                            std::size_t maximum_capacity,
                                            const VmStoragePool& pool = {}) {
        if (logical_bytes > maximum_capacity)
            throw std::invalid_argument("VM receive length exceeds maximum capacity");
        VirtualMemoryStorage owner(0, maximum_capacity, pool);
        if (!maximum_capacity) return owner;
        if (!owner.enable_prefix(maximum_capacity) || consume_vm_failure(VmOperation::Prepare))
            throw std::bad_alloc();
        // Standard non-allocating placement array new, with no initializer:
        // no cookie, value initialization, payload transfer or user constructor.
        auto* bytes = ::new (static_cast<void*>(owner.base_)) std::byte[maximum_capacity];
        if (static_cast<void*>(bytes) != static_cast<void*>(owner.base_)) std::terminate();
        owner.canonical_byte_origin_ = bytes;
        // G1 does not apply to receive storage: the whole maximum is read/write
        // before the transport writes, and bytes beyond the logical length may
        // be written by a transport. Every growth of this owner zeroes all newly
        // exposed bytes. (A G1 build without this line -- the early G1-only
        // diagnostic variant -- is not shippable.)
        owner.zero_from_ = maximum_capacity;
        if (!owner.grow_to(logical_bytes)) throw std::bad_alloc();
        return owner;
    }

    // Internal fresh-owner load path. A complete copy initializes every logical
    // byte; normal growth (including reused pages after shrink) still zeroes.
    static VirtualMemoryStorage from_bytes(const char* bytes, std::size_t length,
                                           std::size_t maximum_capacity) {
        return from_bytes(bytes, length, maximum_capacity, VmStoragePool{});
    }

    static VirtualMemoryStorage from_bytes(const char* bytes, std::size_t length,
                                           std::size_t maximum_capacity,
                                           const VmStoragePool& pool) {
        if (length > maximum_capacity) {
            throw std::invalid_argument("VM input length exceeds maximum capacity");
        }
        if (length != 0 && bytes == nullptr) {
            throw std::invalid_argument("VM input is null");
        }
        VirtualMemoryStorage owner(0, maximum_capacity, pool,
            length != 0 && length == maximum_capacity && !pool.enabled());
        if (length == 0) return owner;
        if (!owner.enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            throw std::bad_alloc();
        }
        std::memcpy(owner.base_, bytes, length);
        owner.logical_bytes_ = length;
        if (owner.zero_from_ < length) owner.zero_from_ = length;
        return owner;
    }

    // V2 sparse load: `stored` holds the concatenated extents; extent i is
    // {u32 offset, u32 bytes} at extents + 8*i (unaligned allowed), ascending
    // and validated by the caller to tile [0, length) with gaps. Gap bytes
    // read as zero (fresh pages, or explicitly cleared if previously exposed).
    static VirtualMemoryStorage from_extents(const char* stored, const void* extents,
                                             std::size_t count, std::size_t length,
                                             std::size_t maximum_capacity,
                                             const VmStoragePool& pool = {}) {
        if (length > maximum_capacity) {
            throw std::invalid_argument("VM input length exceeds maximum capacity");
        }
        VirtualMemoryStorage owner(0, maximum_capacity, pool, false);
        if (length == 0) return owner;
        if (!owner.enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            throw std::bad_alloc();
        }
        std::size_t cursor = 0;
        auto clear = [&](std::size_t begin, std::size_t end) {
            const auto dirty = (std::min)(end, owner.zero_from_);
            if (begin < dirty) std::memset(owner.base_ + begin, 0, dirty - begin);
        };
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t entry[2];
            std::memcpy(entry, static_cast<const char*>(extents) + i * sizeof(entry), sizeof(entry));
            clear(cursor, entry[0]);
            std::memcpy(owner.base_ + entry[0], stored, entry[1]);
            stored += entry[1];
            cursor = std::size_t(entry[0]) + entry[1];
        }
        clear(cursor, length);
        owner.logical_bytes_ = length;
        if (owner.zero_from_ < length) owner.zero_from_ = length;
        return owner;
    }

    // In-place reload: replace the logical content of this owner with
    // `length` bytes, keeping the reservation, its accessible pages and the
    // payload address. Bytes beyond `length` that the old content exposed
    // stay below zero_from_, so every later growth zeroes them. A failure
    // leaves the old bytes and length unchanged.
    void assign_bytes(const char* bytes, std::size_t length) {
        if (length > maximum_capacity_) {
            throw std::invalid_argument("VM input length exceeds maximum capacity");
        }
        if (length != 0 && bytes == nullptr) {
            throw std::invalid_argument("VM input is null");
        }
        if (length == 0) { logical_bytes_ = 0; return; }
        if (!base_ || !enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            throw std::bad_alloc();
        }
        std::memcpy(base_, bytes, length);
        logical_bytes_ = length;
        if (zero_from_ < length) zero_from_ = length;
    }

    // In-place form of from_extents (same extent contract). Gap bytes are
    // cleared wherever the old content may have left them dirty.
    void assign_extents(const char* stored, const void* extents, std::size_t count,
                        std::size_t length) {
        if (length > maximum_capacity_) {
            throw std::invalid_argument("VM input length exceeds maximum capacity");
        }
        if (length == 0) { logical_bytes_ = 0; return; }
        if (!base_ || !enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            throw std::bad_alloc();
        }
        std::size_t cursor = 0;
        auto clear = [&](std::size_t begin, std::size_t end) {
            const auto dirty = (std::min)(end, zero_from_);
            if (begin < dirty) std::memset(base_ + begin, 0, dirty - begin);
        };
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t entry[2];
            std::memcpy(entry, static_cast<const char*>(extents) + i * sizeof(entry), sizeof(entry));
            clear(cursor, entry[0]);
            std::memcpy(base_ + entry[0], stored, entry[1]);
            stored += entry[1];
            cursor = std::size_t(entry[0]) + entry[1];
        }
        clear(cursor, length);
        logical_bytes_ = length;
        if (zero_from_ < length) zero_from_ = length;
    }

    ~VirtualMemoryStorage() { release(); }

    VirtualMemoryStorage(const VirtualMemoryStorage&) = delete;
    VirtualMemoryStorage& operator=(const VirtualMemoryStorage&) = delete;

    VirtualMemoryStorage(VirtualMemoryStorage&& other) noexcept { swap(other); }

    VirtualMemoryStorage& operator=(VirtualMemoryStorage&& other) noexcept {
        if (this != &other) {
            VirtualMemoryStorage old(std::move(other));
            swap(old);
        }
        return *this;
    }

    std::byte* canonical_byte_origin() const noexcept { return canonical_byte_origin_; }
    // Host-only owner slot in the page preceding the payload (null if none).
    void* host_owner() const noexcept {
        return base_ ? *reinterpret_cast<void* const*>(base_ - sizeof(void*)) : nullptr;
    }
    void set_host_owner(void* owner) noexcept {
        if (base_) *reinterpret_cast<void**>(base_ - sizeof(void*)) = owner;
    }
    char* data() noexcept { return base_; }
    const char* data() const noexcept { return base_; }
    std::size_t size() const noexcept { return logical_bytes_; }
    std::size_t logical_bytes() const noexcept { return logical_bytes_; }
    std::size_t capacity() const noexcept { return maximum_capacity_; }
    std::size_t maximum_capacity() const noexcept { return maximum_capacity_; }
    // The pool this storage was acquired through (empty when none).
    const VmStoragePool& pool() const noexcept { return pool_; }
    std::size_t reserved_bytes() const noexcept { return reserved_bytes_; }
    std::size_t accessible_bytes() const noexcept { return accessible_bytes_; }
    std::size_t page_size() const noexcept { return page_size_; }
    std::size_t reservation_granularity() const noexcept { return reservation_granularity_; }
    bool empty() const noexcept { return logical_bytes_ == 0; }

    // A failed growth retains all old bytes and the old logical length. Pages
    // enabled before a Prepare failure remain accurately recorded, but are not
    // published to the upper allocator. Only newly exposed bytes are zeroed.
    bool grow_to(std::size_t new_logical_bytes) noexcept {
        if (new_logical_bytes < logical_bytes_ ||
            new_logical_bytes > maximum_capacity_) return false;
        if (new_logical_bytes == logical_bytes_) return true;
        if (!enable_prefix(new_logical_bytes)) return false;
        if (consume_vm_failure(VmOperation::Prepare)) return false;
        // Bytes at or beyond zero_from_ have never been exposed since the OS
        // supplied them (fresh anonymous/committed or replaced pages) and are
        // therefore already zero; only previously exposed bytes are cleared.
        const std::size_t dirty_end = (std::min)(new_logical_bytes,
            (std::max)(zero_from_, logical_bytes_));
        if (dirty_end > logical_bytes_)
            std::memset(base_ + logical_bytes_, 0, dirty_end - logical_bytes_);
        logical_bytes_ = new_logical_bytes;
        if (zero_from_ < new_logical_bytes) zero_from_ = new_logical_bytes;
        return true;
    }

    // Logical shrink keeps the reservation and page permissions. Regrowth
    // zeroes the newly exposed range even when it lies in previously used pages.
    void resize_logical(std::size_t new_logical_bytes) {
        if (new_logical_bytes > maximum_capacity_) {
            throw std::length_error("VM length exceeds maximum capacity");
        }
        if (new_logical_bytes <= logical_bytes_) {
            logical_bytes_ = new_logical_bytes;
        } else if (!grow_to(new_logical_bytes)) {
            throw std::bad_alloc();
        }
    }

    // Explicitly discard only complete pages beyond the current logical end.
    // The partial last page remains accessible; regrowth still zeroes every
    // newly exposed byte, including stale bytes in that retained partial page.
    // No caller may independently map, unmap, or protect any owned address.
    VmReclaimResult reclaim_tail() noexcept {
        if (!base_ || accessible_bytes_ == 0) return {};
        std::size_t retained = 0;
        if (!checked_vm_round_up(logical_bytes_, page_size_, retained) ||
            retained > accessible_bytes_) return {VmReclaimStatus::Failed};
        const auto bytes = accessible_bytes_ - retained;
        if (bytes == 0) return {};
        VmReclaimResult result{VmReclaimStatus::Failed, bytes, 0, 0};
        if (consume_vm_failure(VmOperation::Reclaim)) return result;
        char* const tail = base_ + retained;
#if defined(_WIN32)
        if (!::VirtualFree(tail, bytes, MEM_DECOMMIT)) return result;
#elif defined(__APPLE__)
        // Darwin's fixed anonymous replacement is atomic with respect to the
        // old mapping (XNU kern_mman.c, MAP_FIXED/VM_FLAGS_OVERWRITE). It creates
        // fresh backing while keeping ownership of the address range. Never
        // unmap first: another thread could claim the resulting address hole.
        void* replacement = ::mmap(tail, bytes, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (replacement == MAP_FAILED) return result;
#else
        // Discard before protection: if protection fails, accessible_bytes_
        // stays exact and the same range remains eligible for a later retry.
        if (::madvise(tail, bytes, MADV_DONTNEED) != 0) return result;
        result.discarded_bytes = bytes;
        if (consume_vm_failure(VmOperation::Protect) ||
            ::mprotect(tail, bytes, PROT_NONE) != 0) {
            result.status = VmReclaimStatus::DiscardedButAccessible;
            return result;
        }
#endif
        accessible_bytes_ = retained;
        // Decommitted/replaced pages return zero-filled when enabled again.
        if (zero_from_ > retained) zero_from_ = retained;
        result.status = VmReclaimStatus::Reclaimed;
        result.discarded_bytes = bytes;
        result.inaccessible_bytes = bytes;
        return result;
    }

    void swap(VirtualMemoryStorage& other) noexcept {
        pool_.state_.swap(other.pool_.state_);
        swap_storage(other);
    }

    friend void swap(VirtualMemoryStorage& lhs, VirtualMemoryStorage& rhs) noexcept {
        lhs.swap(rhs);
    }

private:
    void swap_storage(VirtualMemoryStorage& other) noexcept {
        using std::swap;
        swap(base_, other.base_);
        swap(canonical_byte_origin_, other.canonical_byte_origin_);
        swap(reserved_bytes_, other.reserved_bytes_);
        swap(maximum_capacity_, other.maximum_capacity_);
        swap(accessible_bytes_, other.accessible_bytes_);
        swap(logical_bytes_, other.logical_bytes_);
        swap(zero_from_, other.zero_from_);
        swap(page_size_, other.page_size_);
        swap(reservation_granularity_, other.reservation_granularity_);
    }

    bool acquire_from_pool() noexcept;
    void release() noexcept;
    // Publish permissions separately from logical bytes. Both initialization
    // paths keep an accurate accessible extent if preparation later fails.
    bool enable_prefix(std::size_t new_logical_bytes) noexcept {
        std::size_t new_accessible_bytes = 0;
        if (!checked_vm_round_up(new_logical_bytes, page_size_, new_accessible_bytes) ||
            new_accessible_bytes > reserved_bytes_) return false;
        if (new_accessible_bytes > accessible_bytes_) {
            if (consume_vm_failure(VmOperation::Enable)) return false;
            const std::size_t extra = new_accessible_bytes - accessible_bytes_;
#if defined(_WIN32)
            void* committed = ::VirtualAlloc(base_ + accessible_bytes_, extra,
                                             MEM_COMMIT, PAGE_READWRITE);
            if (!committed) return false;
#else
            // This owner keeps a read/write prefix and one contiguous PROT_NONE
            // tail. Enabling its prefix does not traverse multiple VMAs with
            // different attributes. External protection or mapping changes to
            // this reservation are unsupported; mprotect over arbitrary VMAs
            // need not roll back earlier VMA changes when a later one fails.
            if (::mprotect(base_ + accessible_bytes_, extra,
                           PROT_READ | PROT_WRITE) != 0) return false;
#endif
            accessible_bytes_ = new_accessible_bytes;
        }
        return true;
    }
    void query_page_geometry() {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        page_size_ = static_cast<std::size_t>(info.dwPageSize);
        reservation_granularity_ = static_cast<std::size_t>(info.dwAllocationGranularity);
#else
        const long page = ::sysconf(_SC_PAGESIZE);
        if (page <= 0) throw std::bad_alloc();
        page_size_ = static_cast<std::size_t>(page);
        reservation_granularity_ = page_size_;
#endif
        if (page_size_ == 0 || reservation_granularity_ == 0) throw std::bad_alloc();
    }

    void release_mapping() noexcept {
        if (base_) {
#if defined(_WIN32)
            ::VirtualFree(base_ - page_size_, 0, MEM_RELEASE);
#else
            ::munmap(base_ - page_size_, reserved_bytes_ + page_size_);
#endif
        }
        base_ = nullptr;
        canonical_byte_origin_ = nullptr;
        reserved_bytes_ = maximum_capacity_ = accessible_bytes_ = logical_bytes_ = 0;
        zero_from_ = 0;
        page_size_ = reservation_granularity_ = 0;
    }

    char* base_{nullptr};
    std::byte* canonical_byte_origin_{nullptr};
    std::size_t reserved_bytes_{0};
    std::size_t maximum_capacity_{0};
    std::size_t accessible_bytes_{0};
    std::size_t logical_bytes_{0};
    // Lowest offset from which every reserved byte is known to be OS-zero.
    // Invariant: zero_from_ >= logical_bytes_ while the mapping is owned.
    std::size_t zero_from_{0};
    std::size_t page_size_{0};
    std::size_t reservation_granularity_{0};
    VmStoragePool pool_;
};

struct VmStoragePool::State {
    explicit State(Limits value) : limits(value) {
        // Returning owners never grow this vector or allocate a control block.
        cached.reserve(limits.max_cached_blocks);
    }
    Limits limits;
    std::mutex mutex;
    std::vector<VirtualMemoryStorage> cached;
    Stats statistics;
};

inline VmStoragePool::VmStoragePool(Limits limits)
    : state_(std::make_shared<State>(limits)) {}

inline VmStoragePool::Stats VmStoragePool::stats() const noexcept {
    if (!state_) return {};
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->statistics;
}

inline void VmStoragePool::clear() const noexcept {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    // Cached owners have no pool handle, so destruction releases their mappings
    // without recursive locking. vector::clear retains the preallocated slots.
    state_->cached.clear();
    state_->statistics.cached_blocks = 0;
    state_->statistics.cached_reserved_bytes = 0;
    state_->statistics.cached_accessible_bytes = 0;
}

inline bool VirtualMemoryStorage::acquire_from_pool() noexcept {
    if (!pool_.state_) return false;
    auto& state = *pool_.state_;
    std::lock_guard<std::mutex> lock(state.mutex);
    for (std::size_t i = 0; i < state.cached.size(); ++i) {
        if (state.cached[i].maximum_capacity_ != maximum_capacity_) continue;
        state.statistics.cached_reserved_bytes -= state.cached[i].reserved_bytes_;
        state.statistics.cached_accessible_bytes -= state.cached[i].accessible_bytes_;
        swap_storage(state.cached[i]);
        if (i + 1 != state.cached.size()) state.cached[i].swap_storage(state.cached.back());
        state.cached.pop_back();
        --state.statistics.cached_blocks;
        ++state.statistics.hits;
        return true;
    }
    ++state.statistics.misses;
    return false;
}

inline void VirtualMemoryStorage::release() noexcept {
    // Keep the state alive until its mutex is released, even when this is the
    // final external reference. Idle owners never retain their pool's state.
    auto state = std::move(pool_.state_);
    if (base_ && state) {
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto& limits = state->limits;
        auto& statistics = state->statistics;
        if (state->cached.size() < limits.max_cached_blocks &&
            reserved_bytes_ <= limits.max_reserved_bytes - statistics.cached_reserved_bytes &&
            accessible_bytes_ <= limits.max_accessible_bytes - statistics.cached_accessible_bytes) {
            statistics.cached_reserved_bytes += reserved_bytes_;
            statistics.cached_accessible_bytes += accessible_bytes_;
            logical_bytes_ = 0;
            set_host_owner(nullptr);
            state->cached.emplace_back();
            swap_storage(state->cached.back());
            ++statistics.cached_blocks;
            return;
        }
    }
    release_mapping();
}

} // namespace detail
} // namespace XOffsetDatastructure
