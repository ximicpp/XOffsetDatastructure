#pragma once

#include <atomic>
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
// The lean <windows.h>, without min/max: no rpcndr.h (`small`, `hyper`), winsock.h or
// shellapi.h behind a client's back. Both macros are set only for this include; a
// client that needs more includes those headers itself (or <windows.h> first).
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    define XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#    define XOFFSET_DETAIL_UNDEF_NOMINMAX
#  endif
#  include <windows.h>
#  ifdef XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#    undef XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#  endif
#  ifdef XOFFSET_DETAIL_UNDEF_NOMINMAX
#    undef NOMINMAX
#    undef XOFFSET_DETAIL_UNDEF_NOMINMAX
#  endif
#elif defined(__linux__) || defined(__APPLE__)
#  include <sys/mman.h>
#  include <unistd.h>
#else
#  error "XOffset virtual memory storage requires Windows, Linux, or macOS"
#endif

#include "xoffset_error.hpp"
#include "xoffset_slot_storage.hpp"

namespace XOffsetDatastructure {
namespace detail {

// Deterministic, thread-local, one-shot failure injection. Enable is consumed
// only when additional pages are required; Prepare precedes logical publication.
// Reserve also models taking a slot (slot storage has no other OS step).
enum class VmOperation : std::uint8_t { Reserve, Enable, Prepare, Reclaim, Protect };

inline thread_local unsigned vm_pending_failures = 0;
// Set once any thread arms a failure: until then consume_vm_failure reads no
// thread-local storage (a call through the TLV descriptor on Apple targets).
inline std::atomic<bool> vm_failure_injection_used{false};

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
    vm_failure_injection_used.store(true, std::memory_order_relaxed);
    vm_pending_failures |= vm_operation_bit(operation);
}

inline void clear_vm_failures() noexcept { vm_pending_failures = 0; }

inline bool consume_vm_failure(VmOperation operation) noexcept {
    if (!vm_failure_injection_used.load(std::memory_order_relaxed)) return false;
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
                         const VmStoragePool& pool, bool full_copy) {
        XOFFSET_VREQUIRE(reserve_v(logical_bytes, maximum_capacity, pool, full_copy));
    }

    // The constructor's work on an empty owner. Functions named *_v report a
    // failure as detail::vstatus: they throw when exceptions are on and
    // return false (the failure recorded) when they are off. A failed
    // reserve_v leaves an owner that its destructor releases.
    vstatus reserve_v(std::size_t logical_bytes, std::size_t maximum_capacity,
                      const VmStoragePool& pool, [[maybe_unused]] bool full_copy) {
        if (logical_bytes > maximum_capacity) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM initial length exceeds maximum capacity");
        }
        if (maximum_capacity > static_cast<std::size_t>(
                (std::numeric_limits<std::ptrdiff_t>::max)())) {
            XOFFSET_VFAIL_STD(std::length_error, XErrc::length_error,
                              "VM capacity exceeds pointer difference range");
        }
        XOFFSET_VTRY(query_page_geometry());
        std::size_t reserved = 0;
        if (!checked_vm_round_up(maximum_capacity, page_size_, reserved) ||
            reserved > static_cast<std::size_t>(
                (std::numeric_limits<std::ptrdiff_t>::max)())) {
            XOFFSET_VFAIL_STD(std::length_error, XErrc::length_error, "VM reservation page rounding overflows");
        }
        maximum_capacity_ = maximum_capacity;
        if (reserved == 0) return vok();
        // This failure models acquisition, including a cache hit. It is checked
        // before removing an idle owner, so a rejected acquisition keeps the cache.
        if (consume_vm_failure(VmOperation::Reserve)) XOFFSET_VFAIL_BAD_ALLOC();
        pool_ = pool;
        if (!acquire_from_pool()) {
            const std::size_t prefix = page_size_;
            if (reserved > static_cast<std::size_t>(
                    (std::numeric_limits<std::ptrdiff_t>::max)()) - prefix)
                XOFFSET_VFAIL_STD(std::length_error, XErrc::length_error, "VM reservation page rounding overflows");
            // V2.1 owner lookup: the payload starts at a 2^k boundary,
            // k = max(22, ceil(log2 span)), and the mapping covers whole 4 MiB
            // granules, so OwnerMap finds the payload from any address inside
            // and no other mapping shares a granule with it. The host page sits
            // just below the payload, in the granule before it.
            const std::size_t span =
                (reserved + OwnerMap::granule_bytes - 1) & ~(OwnerMap::granule_bytes - 1);
            if (span < reserved)
                XOFFSET_VFAIL_STD(std::length_error, XErrc::length_error, "VM reservation page rounding overflows");
            std::size_t shift = slot_log2(span);
            if (shift < OwnerMap::granule_shift) shift = OwnerMap::granule_shift;
            if (shift >= OwnerMap::address_bits || !OwnerMap::ready()) XOFFSET_VFAIL_BAD_ALLOC();
            const std::size_t align = std::size_t(1) << shift;
#if defined(_WIN32)
            // A reservation starts on an allocation-granularity boundary, so
            // the prefix is one granularity unit; only its last page (the host
            // page) is committed. Find an aligned hole, release it and reserve
            // exactly there; another thread may take the hole in between.
            const std::size_t win_prefix = reservation_granularity_;
            char* mapping = nullptr;
            for (int attempt = 0; attempt < 8 && !mapping; ++attempt) {
                auto* probe = static_cast<char*>(::VirtualAlloc(
                    nullptr, win_prefix + span + align, MEM_RESERVE, PAGE_NOACCESS));
                if (!probe) XOFFSET_VFAIL_BAD_ALLOC();
                const auto payload = (reinterpret_cast<std::uintptr_t>(probe) + win_prefix + align - 1) &
                                     ~std::uintptr_t(align - 1);
                ::VirtualFree(probe, 0, MEM_RELEASE);
                auto* want = reinterpret_cast<char*>(payload - win_prefix);
                mapping = static_cast<char*>(::VirtualAlloc(want, win_prefix + span, MEM_RESERVE, PAGE_NOACCESS));
                if (mapping && mapping != want) {
                    ::VirtualFree(mapping, 0, MEM_RELEASE);
                    mapping = nullptr;
                }
            }
            if (!mapping) XOFFSET_VFAIL_BAD_ALLOC();
            if (!OwnerMap::reachable(mapping, win_prefix + span) ||
                !::VirtualAlloc(mapping + win_prefix - prefix, prefix, MEM_COMMIT, PAGE_READWRITE)) {
                ::VirtualFree(mapping, 0, MEM_RELEASE);
                XOFFSET_VFAIL_BAD_ALLOC();
            }
            base_ = mapping + win_prefix;
#else
            if (full_copy && consume_vm_failure(VmOperation::Enable)) XOFFSET_VFAIL_BAD_ALLOC();
            // Over-map PROT_NONE, then trim both ends.
            const std::size_t total = span + prefix + align;
            void* raw = ::mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (raw == MAP_FAILED) XOFFSET_VFAIL_BAD_ALLOC();
            const auto start = reinterpret_cast<std::uintptr_t>(raw);
            const auto payload = (start + prefix + align - 1) & ~std::uintptr_t(align - 1);
            if (payload - prefix != start) ::munmap(raw, payload - prefix - start);
            if (start + total != payload + span)
                ::munmap(reinterpret_cast<void*>(payload + span), start + total - (payload + span));
            char* mapping = reinterpret_cast<char*>(payload - prefix);
            if (!OwnerMap::reachable(mapping, span + prefix) ||
                ::mprotect(mapping, prefix + (full_copy ? reserved : 0), PROT_READ | PROT_WRITE) != 0) {
                ::munmap(mapping, span + prefix);
                XOFFSET_VFAIL_BAD_ALLOC();
            }
            base_ = mapping + prefix;
            if (full_copy) accessible_bytes_ = reserved;
#endif
            mapped_bytes_ = span;
            OwnerMap::set(base_, span, static_cast<std::uint8_t>(shift));
            reserved_bytes_ = reserved;
        }
        set_host_owner(nullptr);
        // A throwing constructor does not run this object's destructor.
        if (!grow_to(logical_bytes)) {
            release();
            XOFFSET_VFAIL_BAD_ALLOC();
        }
        return vok();
    }

public:
    // The public constructor into an empty owner.
    static vstatus create_v(VirtualMemoryStorage& owner, std::size_t logical_bytes,
                            std::size_t maximum_capacity, const VmStoragePool& pool = {}) {
        return owner.reserve_v(logical_bytes, maximum_capacity, pool, false);
    }

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
        VirtualMemoryStorage owner;
        XOFFSET_VREQUIRE(for_receive_v(owner, logical_bytes, maximum_capacity, pool));
        return owner;
    }
    static vstatus for_receive_v(VirtualMemoryStorage& owner, std::size_t logical_bytes,
                                 std::size_t maximum_capacity, const VmStoragePool& pool = {}) {
        if (logical_bytes > maximum_capacity)
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM receive length exceeds maximum capacity");
        XOFFSET_VTRY(owner.reserve_v(0, maximum_capacity, pool, false));
        if (!maximum_capacity) return vok();
        if (!owner.enable_prefix(maximum_capacity) || consume_vm_failure(VmOperation::Prepare))
            XOFFSET_VFAIL_BAD_ALLOC();
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
        if (!owner.grow_to(logical_bytes)) XOFFSET_VFAIL_BAD_ALLOC();
        return vok();
    }

    // Internal fresh-owner load path. A complete copy initializes every logical
    // byte; normal growth (including reused pages after shrink) still zeroes.
    static VirtualMemoryStorage from_bytes(const char* bytes, std::size_t length,
                                           std::size_t maximum_capacity) {
        return from_bytes(bytes, length, maximum_capacity, VmStoragePool{});
    }

    // With `at`, `bytes` fill [at, length) and the caller writes [0, at).
    static VirtualMemoryStorage from_bytes(const char* bytes, std::size_t length,
                                           std::size_t maximum_capacity,
                                           const VmStoragePool& pool, std::size_t at = 0) {
        VirtualMemoryStorage owner;
        XOFFSET_VREQUIRE(from_bytes_v(owner, bytes, length, maximum_capacity, pool, at));
        return owner;
    }
    // from_bytes into an empty owner.
    static vstatus from_bytes_v(VirtualMemoryStorage& owner, const char* bytes, std::size_t length,
                                std::size_t maximum_capacity,
                                const VmStoragePool& pool, std::size_t at = 0) {
        if (length > maximum_capacity || at > length) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM input length exceeds maximum capacity");
        }
        if (length != at && bytes == nullptr) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument, "VM input is null");
        }
        XOFFSET_VTRY(owner.reserve_v(0, maximum_capacity, pool,
            length != 0 && length == maximum_capacity && !pool.enabled()));
        if (length == 0) return vok();
        if (!owner.enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            XOFFSET_VFAIL_BAD_ALLOC();
        }
        std::memcpy(owner.base_ + at, bytes, length - at);
        owner.logical_bytes_ = length;
        if (owner.zero_from_ < length) owner.zero_from_ = length;
        return vok();
    }

    // V2 sparse load: `stored` holds the concatenated extents; extent i is
    // {u32 offset, u32 bytes} at extents + 8*i (unaligned allowed), ascending
    // and validated by the caller to tile [0, length) with gaps. Gap bytes
    // read as zero (fresh pages, or explicitly cleared if previously exposed),
    // [0, first offset) included.
    static VirtualMemoryStorage from_extents(const char* stored, const void* extents,
                                             std::size_t count, std::size_t length,
                                             std::size_t maximum_capacity,
                                             const VmStoragePool& pool = {}) {
        VirtualMemoryStorage owner;
        XOFFSET_VREQUIRE(from_extents_v(owner, stored, extents, count, length, maximum_capacity, pool));
        return owner;
    }
    // from_extents into an empty owner.
    static vstatus from_extents_v(VirtualMemoryStorage& owner, const char* stored, const void* extents,
                                  std::size_t count, std::size_t length,
                                  std::size_t maximum_capacity,
                                  const VmStoragePool& pool = {}) {
        if (length > maximum_capacity) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM input length exceeds maximum capacity");
        }
        XOFFSET_VTRY(owner.reserve_v(0, maximum_capacity, pool, false));
        if (length == 0) return vok();
        if (!owner.enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            XOFFSET_VFAIL_BAD_ALLOC();
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
        return vok();
    }

    // Slot owner (R1): an empty owner on a slot of class `cls`. The slot's
    // payload capacity is accessible_bytes(); maximum_capacity may exceed it
    // only for a movable owner, whose growth past the slot is the caller's
    // relocation. A slot has no OS step, so the owner never fails to grow
    // inside it except by failure injection.
    static VirtualMemoryStorage for_slot(std::size_t cls, std::size_t maximum_capacity, bool movable) {
        VirtualMemoryStorage owner;
        XOFFSET_VREQUIRE(for_slot_v(owner, cls, maximum_capacity, movable));
        return owner;
    }
    // for_slot into an empty owner.
    static vstatus for_slot_v(VirtualMemoryStorage& owner, std::size_t cls, std::size_t maximum_capacity,
                              bool movable) {
        if (cls >= SlotGeometry::classes)
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument, "slot class out of range");
        if (!movable && maximum_capacity > SlotGeometry::payload_bytes(cls))
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "pinned slot smaller than maximum capacity");
        if (consume_vm_failure(VmOperation::Reserve)) XOFFSET_VFAIL_BAD_ALLOC();
        SlotHeader* slot = SlotSlab::acquire(cls);
        if (!slot) XOFFSET_VFAIL_BAD_ALLOC();
        owner.base_ = slot->payload();
        owner.reserved_bytes_ = SlotGeometry::slot_bytes(cls);
        owner.accessible_bytes_ = SlotGeometry::payload_bytes(cls);
        owner.maximum_capacity_ = maximum_capacity;
        owner.zero_from_ = slot->dirty_end;
        owner.page_size_ = owner.reservation_granularity_ = SlotSlab::page_size();
        owner.slot_class_ = static_cast<std::uint8_t>(cls);
        owner.movable_ = movable;
        owner.set_host_owner(nullptr);
        return vok();
    }

    // In-place reload: replace the logical content of this owner with
    // `length` bytes, keeping the reservation, its accessible pages and the
    // payload address. Bytes beyond `length` that the old content exposed
    // stay below zero_from_, so every later growth zeroes them. A failure
    // leaves the old bytes and length unchanged.
    // `at` as in from_bytes.
    void assign_bytes(const char* bytes, std::size_t length, std::size_t at = 0) {
        XOFFSET_VREQUIRE(assign_bytes_v(bytes, length, at));
    }
    vstatus assign_bytes_v(const char* bytes, std::size_t length, std::size_t at = 0) {
        if (length > maximum_capacity_ || at > length) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM input length exceeds maximum capacity");
        }
        if (length != at && bytes == nullptr) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument, "VM input is null");
        }
        if (length == 0) { logical_bytes_ = 0; return vok(); }
        if (!base_ || !enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            XOFFSET_VFAIL_BAD_ALLOC();
        }
        std::memcpy(base_ + at, bytes, length - at);
        logical_bytes_ = length;
        if (zero_from_ < length) zero_from_ = length;
        return vok();
    }

    // assign_bytes for a length that the accessible prefix already holds
    // (can_assign_in_place): no permission change, so the only failure left
    // is the injected Prepare failure, reported as false with nothing changed.
    bool can_assign_in_place(std::size_t length) const noexcept {
        return base_ != nullptr && length != 0 && length <= accessible_bytes_ && length <= maximum_capacity_;
    }
    bool assign_in_place(const char* bytes, std::size_t length, std::size_t at) noexcept {
        if (consume_vm_failure(VmOperation::Prepare)) return false;
        std::memcpy(base_ + at, bytes, length - at);
        logical_bytes_ = length;
        if (zero_from_ < length) zero_from_ = length;
        return true;
    }

    // In-place form of from_extents (same extent contract). Gap bytes are
    // cleared wherever the old content may have left them dirty.
    void assign_extents(const char* stored, const void* extents, std::size_t count,
                        std::size_t length) {
        XOFFSET_VREQUIRE(assign_extents_v(stored, extents, count, length));
    }
    vstatus assign_extents_v(const char* stored, const void* extents, std::size_t count,
                             std::size_t length) {
        if (length > maximum_capacity_) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM input length exceeds maximum capacity");
        }
        if (length == 0) { logical_bytes_ = 0; return vok(); }
        if (!base_ || !enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            XOFFSET_VFAIL_BAD_ALLOC();
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
        return vok();
    }

    // In-place direct receive: makes [0, length) the logical content for a
    // writer that overwrites it, keeping the reservation and payload address.
    // Nothing is cleared here. Returns the previous zero_from_: bytes at or
    // beyond it are still zero, so a writer that skips ranges (sparse gaps)
    // clears only below it. A failure leaves the old bytes and length.
    std::size_t expose_for_overwrite(std::size_t length) {
        std::size_t clean_from = 0;
        XOFFSET_VREQUIRE(expose_for_overwrite_v(length, clean_from));
        return clean_from;
    }
    vstatus expose_for_overwrite_v(std::size_t length, std::size_t& clean_from) {
        if (length > maximum_capacity_) {
            XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                              "VM input length exceeds maximum capacity");
        }
        if (!base_ || !enable_prefix(length) || consume_vm_failure(VmOperation::Prepare)) {
            XOFFSET_VFAIL_BAD_ALLOC();
        }
        clean_from = zero_from_;
        logical_bytes_ = length;
        if (zero_from_ < length) zero_from_ = length;
        return vok();
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
    // Bytes at or beyond this offset have not been exposed since the OS
    // supplied them; growth below it zeroes what it exposes.
    std::size_t zero_from() const noexcept { return zero_from_; }
    std::size_t accessible_bytes() const noexcept { return accessible_bytes_; }
    std::size_t page_size() const noexcept { return page_size_; }
    bool is_slot() const noexcept { return slot_class_ != no_slot; }
    // A movable slot owner may be replaced by a larger owner at explicit points.
    bool movable() const noexcept { return movable_; }
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
            XOFFSET_THROW(std::length_error("VM length exceeds maximum capacity"));
        }
        if (new_logical_bytes <= logical_bytes_) {
            logical_bytes_ = new_logical_bytes;
        } else if (!grow_to(new_logical_bytes)) {
            XOFFSET_THROW(std::bad_alloc());
        }
    }

    // Explicitly discard only complete pages beyond the current logical end.
    // The partial last page remains accessible; regrowth still zeroes every
    // newly exposed byte, including stale bytes in that retained partial page.
    // No caller may independently map, unmap, or protect any owned address.
    VmReclaimResult reclaim_tail() noexcept {
        if (!base_ || accessible_bytes_ == 0 || is_slot()) return {};
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
        swap(mapped_bytes_, other.mapped_bytes_);
        swap(maximum_capacity_, other.maximum_capacity_);
        swap(accessible_bytes_, other.accessible_bytes_);
        swap(logical_bytes_, other.logical_bytes_);
        swap(zero_from_, other.zero_from_);
        swap(page_size_, other.page_size_);
        swap(reservation_granularity_, other.reservation_granularity_);
        swap(slot_class_, other.slot_class_);
        swap(movable_, other.movable_);
    }

    bool acquire_from_pool() noexcept;
    void release() noexcept;
    // Publish permissions separately from logical bytes. Both initialization
    // paths keep an accurate accessible extent if preparation later fails.
    bool enable_prefix(std::size_t new_logical_bytes) noexcept {
        // A slot is read/write in full from the start.
        if (is_slot()) return new_logical_bytes <= accessible_bytes_;
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
    vstatus query_page_geometry() {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        page_size_ = static_cast<std::size_t>(info.dwPageSize);
        reservation_granularity_ = static_cast<std::size_t>(info.dwAllocationGranularity);
#else
        const long page = ::sysconf(_SC_PAGESIZE);
        if (page <= 0) XOFFSET_VFAIL_BAD_ALLOC();
        page_size_ = static_cast<std::size_t>(page);
        reservation_granularity_ = page_size_;
#endif
        if (page_size_ == 0 || reservation_granularity_ == 0) XOFFSET_VFAIL_BAD_ALLOC();
        return vok();
    }

    void release_slot() noexcept {
        auto* slot = SlotHeader::from_payload(base_);
        slot->owner = nullptr;
        // zero_from_ >= logical_bytes_: everything from it on is still zero.
        slot->dirty_end = zero_from_ < accessible_bytes_ ? zero_from_ : accessible_bytes_;
        SlotSlab::release(slot);
        base_ = nullptr;
        reserved_bytes_ = maximum_capacity_ = accessible_bytes_ = logical_bytes_ = 0;
        zero_from_ = 0;
        page_size_ = reservation_granularity_ = 0;
        slot_class_ = no_slot;
        movable_ = false;
    }

    void release_mapping() noexcept {
        if (base_) {
            // No head of this payload is live; clear its granules before the
            // range can be mapped again.
            OwnerMap::set(base_, mapped_bytes_, 0);
#if defined(_WIN32)
            ::VirtualFree(base_ - reservation_granularity_, 0, MEM_RELEASE);
#else
            ::munmap(base_ - page_size_, mapped_bytes_ + page_size_);
#endif
        }
        base_ = nullptr;
        canonical_byte_origin_ = nullptr;
        reserved_bytes_ = mapped_bytes_ = maximum_capacity_ = accessible_bytes_ = logical_bytes_ = 0;
        zero_from_ = 0;
        page_size_ = reservation_granularity_ = 0;
    }

    char* base_{nullptr};
    std::byte* canonical_byte_origin_{nullptr};
    std::size_t reserved_bytes_{0};
    // Mapped payload span: reserved_bytes_ rounded up to whole owner-map
    // granules (4 MiB). 0 for a slot owner.
    std::size_t mapped_bytes_{0};
    std::size_t maximum_capacity_{0};
    std::size_t accessible_bytes_{0};
    std::size_t logical_bytes_{0};
    // Lowest offset from which every reserved byte is known to be OS-zero.
    // Invariant: zero_from_ >= logical_bytes_ while the mapping is owned.
    std::size_t zero_from_{0};
    std::size_t page_size_{0};
    std::size_t reservation_granularity_{0};
    static constexpr std::uint8_t no_slot = 0xff;
    std::uint8_t slot_class_{no_slot};
    bool movable_{false};
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
    if (is_slot()) {
        release_slot();
        return;
    }
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
