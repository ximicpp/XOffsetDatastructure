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
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#  include <sys/mman.h>
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <TargetConditionals.h>
#  endif
#else
#  error "XOffset virtual memory storage requires Windows, Linux, or macOS"
#endif

// ---------------------------------------------------------------------------
// Process-wide canonical receive window (design "W-fixed").
//
// Direct-receive storage (VirtualMemoryStorage::for_receive) is placed inside
// ONE process-wide reservation at a compile-time constant virtual address.
// One std::byte[XOFFSET_CANONICAL_WINDOW_BYTES] array is started over the
// whole reservation exactly once; every receive payload pointer and every
// canonical rel32 resolution derives from that single array. Because the
// bounds are compile-time constants, the hot-path membership test is two
// compares against immediates and needs no atomic ledger load.
//
// Configuration (all three are plain integer macros, defined identically in
// every translation unit of a program, like any other ABI macro):
//   XOFFSET_CANONICAL_WINDOW_BASE    virtual address (granule aligned)
//   XOFFSET_CANONICAL_WINDOW_BYTES   window size; 0 disables the window, and
//                                    then non-empty receive storage creation
//                                    throws std::bad_alloc (ordinary storage
//                                    is unaffected)
//   XOFFSET_CANONICAL_WINDOW_GRANULE slot granularity (power of two, multiple
//                                    of the runtime page size)
//
// Default rationale (PROT_NONE / MEM_RESERVE address space, which costs no
// memory or commit charge until a slot's pages are enabled exactly as ordinary
// storage enables them). Size: 32 GiB on macOS, 256 GiB elsewhere. On Darwin
// the eager one-time reservation was measured to cost ~0.5 us per GiB (256 GiB
// = 0.1-0.7 ms in the first VM storage creation of every process), so the
// macOS default is 32 GiB (~20 us): 32768 granules, e.g. 512 concurrent
// receives at the default 64 MiB maximum capacity (one receive is at most
// MAX_PAYLOAD < 2 GiB). The Linux (one PROT_NONE VMA) and Windows (one
// MEM_RESERVE VAD) reservations are not known to scale with size and were not
// measured here, so they keep 256 GiB. Define the macro to override.
//  * macOS arm64/x86_64: 0x5800'0000'0000. User VA is 47 bits. Below it are
//    __PAGEZERO, the image, dyld shared cache, stacks and malloc (low GiBs) and
//    ASan's dynamic shadow (first 16 TiB gap above 4 GiB); above it are the
//    nano malloc zone and ASan's allocator (0x6000'0000'0000). It lies inside
//    TSan's Mapping48AddressSpace "mid app" range [0x5500.., 0x5a00..), so the
//    same address works without sanitizers, with ASan and with TSan (probed).
//  * Linux x86_64: 0x4000'0000'0000 (64 TiB). Above ASan's high shadow
//    (ends 0x1000'7fff'7fff), below PIE images (0x5555.. + up to 2^32 pages of
//    randomization) and the top-down mmap area. Under TSan, whose app ranges
//    exclude that address, 0x5800'0000'0000 (TSan "mid app"; TSan re-execs
//    without ASLR when the PIE range would not fit).
//  * Linux aarch64 (48-bit VA): 0x4000'0000'0000, above ASan's high shadow
//    (ends 0x2010'0000'0000) and below PIE (0xaaaa..); under TSan
//    0x0800'0000'0000 (TSan "lo app" range). 39/42-bit VA kernels cannot
//    reserve it: receive then throws std::bad_alloc unless overridden.
//  * Windows x64: 0x5800'0000'0000 (128 TiB user VA since Windows 8.1).
//  * Everything else (32-bit, iOS, other OSes): disabled (bytes 0).
// A reservation that cannot be placed exactly at the base (address already in
// use, sanitizer rejection, VA too small) fails once and permanently: receive
// storage creation then throws std::bad_alloc, ordinary storage is unaffected.
// ---------------------------------------------------------------------------
#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define XOFFSET_DETAIL_WINDOW_TSAN 1
#  endif
#endif
#if !defined(XOFFSET_DETAIL_WINDOW_TSAN) && defined(__SANITIZE_THREAD__)
#  define XOFFSET_DETAIL_WINDOW_TSAN 1
#endif

#if !defined(XOFFSET_CANONICAL_WINDOW_BASE)
#  if defined(__APPLE__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX && \
      (defined(__aarch64__) || defined(__x86_64__))
#    define XOFFSET_CANONICAL_WINDOW_BASE 0x580000000000ull
#  elif defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#    if defined(XOFFSET_DETAIL_WINDOW_TSAN)
#      define XOFFSET_CANONICAL_WINDOW_BASE 0x580000000000ull
#    else
#      define XOFFSET_CANONICAL_WINDOW_BASE 0x400000000000ull
#    endif
#  elif defined(__linux__) && defined(__aarch64__) && !defined(__ILP32__)
#    if defined(XOFFSET_DETAIL_WINDOW_TSAN)
#      define XOFFSET_CANONICAL_WINDOW_BASE 0x080000000000ull
#    else
#      define XOFFSET_CANONICAL_WINDOW_BASE 0x400000000000ull
#    endif
#  elif defined(_WIN64)
#    define XOFFSET_CANONICAL_WINDOW_BASE 0x580000000000ull
#  else
#    define XOFFSET_CANONICAL_WINDOW_BASE 0
#  endif
#endif
#if !defined(XOFFSET_CANONICAL_WINDOW_BYTES)
#  if XOFFSET_CANONICAL_WINDOW_BASE && defined(__APPLE__)
#    define XOFFSET_CANONICAL_WINDOW_BYTES (32ull << 30)
#  elif XOFFSET_CANONICAL_WINDOW_BASE
#    define XOFFSET_CANONICAL_WINDOW_BYTES (256ull << 30)
#  else
#    define XOFFSET_CANONICAL_WINDOW_BYTES 0
#  endif
#endif
#if !defined(XOFFSET_CANONICAL_WINDOW_GRANULE)
#  define XOFFSET_CANONICAL_WINDOW_GRANULE (1ull << 20)
#endif

namespace XOffsetDatastructure {
namespace detail {

// Deterministic, thread-local, one-shot failure injection. Enable is consumed
// only when additional pages are required; Prepare precedes logical publication.
// WindowReserve models failure of the one-time canonical receive window
// reservation (see CanonicalWindow below); it is consumed only by that attempt.
// WindowCollide is a test hook: the next ordinary reservation made while the
// window is unavailable first asks for the window base, so the overlap guard
// (VirtualMemoryStorage::map_ordinary) must relocate it.
enum class VmOperation : std::uint8_t {
    Reserve, Enable, Prepare, Reclaim, Protect, WindowReserve, WindowCollide
};

inline thread_local unsigned vm_pending_failures = 0;

inline unsigned vm_operation_bit(VmOperation operation) noexcept {
    switch (operation) {
        case VmOperation::Reserve: return 1u;
        case VmOperation::Enable: return 2u;
        case VmOperation::Prepare: return 4u;
        case VmOperation::Reclaim: return 8u;
        case VmOperation::Protect: return 16u;
        case VmOperation::WindowReserve: return 32u;
        case VmOperation::WindowCollide: return 64u;
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

// Provenance root of the canonical window: the single std::byte[] array begun
// over the whole reservation. Written once (null -> array) under the window
// mutex and never reset. Readers only form pointers for addresses inside the
// window; any live in-window address was obtained from storage created after
// that store, so a relaxed load observes it (write-read coherence). A null
// value means the window is not (or can never be) reserved: nothing in the
// address range belongs to this library, and resolution returns nullptr.
inline std::atomic<std::byte*> canonical_window_array{nullptr};
// Set (release) once the single reservation attempt has finished, whatever
// its result. Ordinary storage creation waits for this before mapping, so its
// overlap guard always sees the final window state.
inline std::atomic<bool> canonical_window_attempted{false};
// Diagnostics: ordinary reservations moved away from an unavailable window.
inline std::atomic<std::size_t> canonical_window_relocations{0};

class CanonicalWindow {
public:
    static constexpr std::uintptr_t base =
        static_cast<std::uintptr_t>(XOFFSET_CANONICAL_WINDOW_BASE);
    static constexpr std::size_t bytes =
        static_cast<std::size_t>(XOFFSET_CANONICAL_WINDOW_BYTES);
    static constexpr std::size_t granule =
        static_cast<std::size_t>(XOFFSET_CANONICAL_WINDOW_GRANULE);
    static constexpr bool enabled = bytes != 0;
    static constexpr std::size_t slots = enabled ? bytes / granule : 0;
    // The single array object begun over the whole reservation.
    using array_type = std::byte[enabled ? bytes : 1];

    static_assert(granule != 0 && (granule & (granule - 1)) == 0,
                  "XOFFSET_CANONICAL_WINDOW_GRANULE must be a power of two");
    static_assert(!enabled || base != 0, "XOFFSET_CANONICAL_WINDOW_BASE must be nonzero");
    static_assert(!enabled || (base % granule == 0 && bytes % granule == 0),
                  "canonical window base/bytes must be granule multiples");
    static_assert(!enabled || base <= (std::numeric_limits<std::uintptr_t>::max)() - bytes,
                  "canonical window wraps the address space");
    static_assert(bytes <= static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()),
                  "canonical window exceeds pointer difference range");

    // PUBLIC-API resolution (XBufferCore::canonical_bytes_at). Two compares
    // against immediates; for in-window targets the array pointer is loaded
    // and nullptr is returned when the window is not reserved, so it is safe
    // for ANY target. [target, target+n] must lie in the window (target+n may
    // be the window's one-past-end address).
    static std::byte* bytes_at(std::uintptr_t target, std::size_t n) noexcept {
        if constexpr (enabled) {
            const std::uintptr_t offset = target - base;
            if (offset <= bytes && n <= bytes - offset) {
                std::byte* const origin = canonical_window_array.load(std::memory_order_relaxed);
                if (origin) return origin + offset;
            }
        } else {
            (void)target;
            (void)n;
        }
        return nullptr;
    }

    // INTERNAL resolution: no runtime load at all. The in-window branch names
    // the window array object by laundering the constant base address.
    // Precondition (see w2-PROOF.md): target lies within (n == 0: at most one
    // past) the reservation of a live library VirtualMemoryStorage. Every
    // library reservation either is a window slot (then the window array is
    // alive) or does not touch [base, base+bytes] (enforced by map_ordinary
    // while the window is unavailable), so the branch is reached only while
    // the array object exists.
    static std::byte* internal_bytes_at(std::uintptr_t target, std::size_t n) noexcept {
        if constexpr (enabled) {
            const std::uintptr_t offset = target - base;
            if (offset <= bytes && n <= bytes - offset)
                return *std::launder(reinterpret_cast<array_type*>(base)) + offset;
        } else {
            (void)target;
            (void)n;
        }
        return nullptr;
    }

    // Pure compile-time-bounds test: [target, target+n] inside the window
    // address range (says nothing about whether the window is reserved).
    static constexpr bool in_range(std::uintptr_t target, std::size_t n) noexcept {
        return enabled && target - base <= bytes && n <= bytes - (target - base);
    }

    static bool reserved() noexcept {
        return enabled && canonical_window_array.load(std::memory_order_acquire) != nullptr;
    }

    // True when the closed range [p, p+n] meets the closed window range
    // [base, base+bytes] (touching counts: a one-past-end address of an
    // adjacent reservation would otherwise be an in-window target).
    static bool touches(const void* p, std::size_t n) noexcept {
        if constexpr (!enabled) {
            (void)p; (void)n;
            return false;
        } else {
            const auto first = reinterpret_cast<std::uintptr_t>(p);
            const auto last = n > (std::numeric_limits<std::uintptr_t>::max)() - first
                ? (std::numeric_limits<std::uintptr_t>::max)() : first + n;
            return first <= base + bytes && last >= base;
        }
    }

    // Performs the one reservation attempt if it has not happened yet. Called
    // before any ordinary reservation is mapped (eager reservation at first
    // VM storage creation).
    static void ensure_attempted() noexcept {
        if constexpr (enabled) {
            if (canonical_window_attempted.load(std::memory_order_acquire)) return;
            State& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            (void)reserve_locked(s);
        }
    }

    // True when [p, p+n) lies inside the reserved window.
    static bool contains(const void* p, std::size_t n) noexcept {
        if (!enabled || !canonical_window_array.load(std::memory_order_acquire)) return false;
        const std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(p) - base;
        return offset <= bytes && n <= bytes - offset;
    }

    // Reserves the window on first use (once per process, success or failure)
    // and takes a first-fit run of granules covering `reserved` bytes. Returns
    // the slot's first byte as an element of the window array, or nullptr if
    // the window is disabled/unavailable/exhausted. No C++ allocation occurs,
    // locked or not, so new_handler reentry cannot deadlock on this mutex.
    static std::byte* acquire(std::size_t reserved) noexcept {
        if constexpr (!enabled) {
            (void)reserved;
            return nullptr;
        } else {
            if (reserved == 0 || reserved > bytes) return nullptr;
            const std::size_t count = reserved / granule + (reserved % granule != 0);
            State& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            if (!reserve_locked(s)) return nullptr;
            std::size_t first = 0;
            if (!find_locked(s, count, first)) return nullptr;
            for (std::size_t i = first; i != first + count; ++i)
                s.used[i / 64] |= std::uint64_t{1} << (i % 64);
            if (first == s.lowest_free) s.lowest_free = first + count;
            s.used_slots += count;
            return s.array + first * granule;
        }
    }

    // Returns a slot. Never unmaps (that would open a hole another mapping
    // could claim): accessible pages are replaced/decommitted back to an
    // inaccessible, fresh state first. If that fails the slot is quarantined
    // (kept allocated) so stale pages are never handed out again.
    static void release(std::byte* slot, std::size_t reserved, std::size_t accessible) noexcept {
        if constexpr (!enabled) {
            (void)slot; (void)reserved; (void)accessible;
        } else {
            if (!slot || reserved == 0) return;
            if (accessible != 0 && !make_inaccessible(slot, accessible)) return;
            State& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            const std::size_t first = static_cast<std::size_t>(slot - s.array) / granule;
            const std::size_t count = reserved / granule + (reserved % granule != 0);
            for (std::size_t i = first; i != first + count; ++i)
                s.used[i / 64] &= ~(std::uint64_t{1} << (i % 64));
            if (first < s.lowest_free) s.lowest_free = first;
            s.used_slots -= count;
        }
    }

    // Diagnostics for tests: granules currently allocated or quarantined.
    static std::size_t used_granules() noexcept {
        if constexpr (!enabled) return 0;
        else {
            State& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            return s.used_slots;
        }
    }

private:
    static constexpr std::size_t words = slots == 0 ? 1 : (slots + 63) / 64;
    enum : unsigned char { Untried, Reserved, Unavailable };
    struct State {
        std::mutex mutex;
        unsigned char status = Untried;
        std::byte* array = nullptr;
        std::size_t lowest_free = 0; // no free granule below this index
        std::size_t used_slots = 0;
        std::uint64_t used[words] = {};
    };

    // Never destroyed: receive owners may be released during static/thread
    // destruction. Magic-static initialization takes no allocation.
    static State& state() noexcept {
        alignas(State) static unsigned char storage[sizeof(State)];
        static State* const instance = ::new (static_cast<void*>(storage)) State();
        return *instance;
    }

    static bool reserve_locked(State& s) noexcept {
        if (s.status != Untried) return s.status == Reserved;
        s.status = Unavailable; // exactly one attempt per process
        struct Publish {
            ~Publish() { canonical_window_attempted.store(true, std::memory_order_release); }
        } publish;
        if (consume_vm_failure(VmOperation::WindowReserve)) return false;
        void* const address = reinterpret_cast<void*>(base);
#if defined(_WIN32)
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        if (info.dwPageSize == 0 || granule % info.dwPageSize != 0 ||
            base % info.dwAllocationGranularity != 0) return false;
        void* mapping = ::VirtualAlloc(address, bytes, MEM_RESERVE, PAGE_NOACCESS);
        if (!mapping) return false;
        if (mapping != address) { ::VirtualFree(mapping, 0, MEM_RELEASE); return false; }
#else
        const long page = ::sysconf(_SC_PAGESIZE);
        if (page <= 0 || granule % static_cast<std::size_t>(page) != 0) return false;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#  if defined(__linux__) && defined(MAP_FIXED_NOREPLACE)
        // Fails with EEXIST instead of replacing; kernels before 4.17 ignore
        // the flag and treat the address as a hint (verified below).
        flags |= MAP_FIXED_NOREPLACE;
#  endif
        // Darwin: hint without MAP_FIXED. The kernel honours a free hint and
        // otherwise picks another address, which is rejected below.
        void* mapping = ::mmap(address, bytes, PROT_NONE, flags, -1, 0);
        if (mapping == MAP_FAILED) return false;
        if (mapping != address) { ::munmap(mapping, bytes); return false; }
#endif
        // Standard non-allocating placement array new, no initializer: no
        // cookie, no access to the (inaccessible) pages, no user code.
        // The array object of type array_type (std::byte[bytes]) is what
        // internal_bytes_at launders at the constant base.
        // (An array new-expression yields a pointer to the initial element.)
        std::byte* const array = ::new (mapping) array_type;
        if (static_cast<void*>(array) != mapping) std::terminate();
        s.array = array;
        s.status = Reserved;
        canonical_window_array.store(array, std::memory_order_release);
        return true;
    }

    static bool find_locked(const State& s, std::size_t count, std::size_t& first) noexcept {
        std::size_t run = 0, start = 0;
        for (std::size_t i = s.lowest_free; i < slots;) {
            const std::uint64_t word = s.used[i / 64];
            if (i % 64 == 0 && word == ~std::uint64_t{0}) { run = 0; i += 64; continue; }
            if (i % 64 == 0 && word == 0) {
                const std::size_t span = (slots - i < 64) ? slots - i : 64;
                if (run == 0) start = i;
                run += span;
                i += span;
            } else if (word & (std::uint64_t{1} << (i % 64))) {
                run = 0;
                ++i;
                continue;
            } else {
                if (run == 0) start = i;
                ++run;
                ++i;
            }
            if (run >= count) { first = start; return true; }
        }
        return false;
    }

    static bool make_inaccessible(std::byte* slot, std::size_t accessible) noexcept {
        void* const address = static_cast<void*>(slot);
#if defined(_WIN32)
        return ::VirtualFree(address, accessible, MEM_DECOMMIT) != 0;
#elif defined(__APPLE__)
        // Atomic fixed anonymous replacement, as reclaim_tail does.
        return ::mmap(address, accessible, PROT_NONE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED;
#else
        return ::madvise(address, accessible, MADV_DONTNEED) == 0 &&
               ::mprotect(address, accessible, PROT_NONE) == 0;
#endif
    }
};

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

// OS storage only: no allocator metadata or process-local information is stored
// in the reservation. Accessible means page permissions are read/write; it does
// not measure resident physical memory or promise future fault-free allocation.
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
            // Pooled owners were created by this same path, so they already
            // satisfy the window-overlap invariant (see map_ordinary).
#if defined(_WIN32)
            base_ = static_cast<char*>(map_ordinary(reserved, false));
            if (!base_) throw std::bad_alloc();
#else
            if (full_copy && consume_vm_failure(VmOperation::Enable)) throw std::bad_alloc();
            void* mapping = map_ordinary(reserved, full_copy);
            if (!mapping) throw std::bad_alloc();
            base_ = static_cast<char*>(mapping);
            if (full_copy) accessible_bytes_ = reserved;
#endif
            reserved_bytes_ = reserved;
        }
        // A throwing constructor does not run this object's destructor.
        if (!grow_to(logical_bytes)) {
            release();
            throw std::bad_alloc();
        }
    }

public:
    // Internal direct-receive owner. The reservation is a slot of the
    // process-wide canonical window (CanonicalWindow); the payload's
    // canonical byte origin is the window array element at the slot, so every
    // canonical pointer derives from the one window std::byte[] array. All
    // explicit maximum bytes are made accessible up front. This commits
    // virtual backing/accounting, but does not initialize/touch the whole max.
    // Later grow_to zeroes only newly logical bytes. Window slots are never
    // taken from or returned to a VmStoragePool (the pool argument is accepted
    // for source compatibility and ignored), so receive never adopts ordinary
    // storage and ordinary storage never adopts a window slot. A disabled,
    // unreservable or exhausted window throws std::bad_alloc.
    static VirtualMemoryStorage for_receive(std::size_t logical_bytes,
                                            std::size_t maximum_capacity,
                                            [[maybe_unused]] const VmStoragePool& pool = {}) {
        if (logical_bytes > maximum_capacity)
            throw std::invalid_argument("VM receive length exceeds maximum capacity");
        VirtualMemoryStorage owner;
        owner.reserve_window_slot(maximum_capacity);
        if (!maximum_capacity) return owner;
        if (!owner.enable_prefix(maximum_capacity) || consume_vm_failure(VmOperation::Prepare))
            throw std::bad_alloc();
        owner.canonical_byte_origin_ = owner.window_slot_;
        // The whole maximum is read/write before the transport writes: bytes
        // beyond the logical length may be written by a transport, so G1's
        // pristine-page assumption does not apply. Every later growth zeroes.
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
    bool in_canonical_window() const noexcept { return window_slot_ != nullptr; }
    char* data() noexcept { return base_; }
    const char* data() const noexcept { return base_; }
    std::size_t size() const noexcept { return logical_bytes_; }
    std::size_t logical_bytes() const noexcept { return logical_bytes_; }
    std::size_t capacity() const noexcept { return maximum_capacity_; }
    std::size_t maximum_capacity() const noexcept { return maximum_capacity_; }
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
        swap(window_slot_, other.window_slot_);
        swap(reserved_bytes_, other.reserved_bytes_);
        swap(maximum_capacity_, other.maximum_capacity_);
        swap(accessible_bytes_, other.accessible_bytes_);
        swap(logical_bytes_, other.logical_bytes_);
        swap(zero_from_, other.zero_from_);
        swap(page_size_, other.page_size_);
        swap(reservation_granularity_, other.reservation_granularity_);
    }

    // Maps an ordinary (non-window) reservation. Invariant W (w2-PROOF.md):
    // while the canonical window is not reserved, no library reservation
    // touches the closed range [window base, window base + bytes]. The single
    // window attempt is completed first (eager reservation at the first VM
    // storage creation), so the window state seen here is final: if it is
    // reserved the kernel cannot hand out any address in its range; if it is
    // unavailable, a mapping that touches the range is kept mapped (so the
    // kernel cannot return it again) while the reservation is retried at hints
    // just above and just below the window, then released. Returns nullptr on
    // failure (callers throw std::bad_alloc).
    static void* map_ordinary(std::size_t length, [[maybe_unused]] bool accessible) noexcept {
        CanonicalWindow::ensure_attempted();
        const bool guard = CanonicalWindow::enabled && !CanonicalWindow::reserved();
        const bool collide = consume_vm_failure(VmOperation::WindowCollide) && guard;
        void* const window = reinterpret_cast<void*>(CanonicalWindow::base);
#if defined(_WIN32)
        auto map = [&](void* hint) noexcept -> void* {
            void* p = hint ? ::VirtualAlloc(hint, length, MEM_RESERVE, PAGE_NOACCESS) : nullptr;
            return p ? p : (hint ? nullptr : ::VirtualAlloc(nullptr, length, MEM_RESERVE, PAGE_NOACCESS));
        };
        auto unmap = [](void* p) noexcept { ::VirtualFree(p, 0, MEM_RELEASE); };
        void* first = collide ? map(window) : nullptr;
        if (!first) first = map(nullptr);
        if (!first) return nullptr;
#else
        const int prot = accessible ? PROT_READ | PROT_WRITE : PROT_NONE;
        auto map = [&](void* hint) noexcept -> void* {
            void* p = ::mmap(hint, length, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            return p == MAP_FAILED ? nullptr : p;
        };
        auto unmap = [&](void* p) noexcept { ::munmap(p, length); };
        void* first = map(collide ? window : nullptr);
        if (!first) return nullptr;
#endif
        if (!guard || !CanonicalWindow::touches(first, length)) return first;
        canonical_window_relocations.fetch_add(1, std::memory_order_relaxed);
        void* result = nullptr;
        const std::uintptr_t above = CanonicalWindow::base + CanonicalWindow::bytes + CanonicalWindow::granule;
        const std::uintptr_t gap = length + CanonicalWindow::granule;
        const std::uintptr_t hints[2] = {above, CanonicalWindow::base > gap ? CanonicalWindow::base - gap : 0};
        for (const std::uintptr_t hint : hints) {
            if (hint == 0 || hint > (std::numeric_limits<std::uintptr_t>::max)() - length) continue;
            void* candidate = map(reinterpret_cast<void*>(hint));
            if (!candidate) continue;
            if (!CanonicalWindow::touches(candidate, length)) { result = candidate; break; }
            unmap(candidate);
        }
        unmap(first);
        return result;
    }

    // Same validation, rounding and Reserve failure point as the ordinary
    // constructor, but the reservation is a canonical window slot.
    void reserve_window_slot(std::size_t maximum_capacity) {
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
        if (consume_vm_failure(VmOperation::Reserve)) throw std::bad_alloc();
        std::byte* const slot = CanonicalWindow::acquire(reserved);
        if (!slot) throw std::bad_alloc();
        window_slot_ = slot;
        base_ = reinterpret_cast<char*>(slot);
        reserved_bytes_ = reserved;
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
        if (window_slot_) {
            CanonicalWindow::release(window_slot_, reserved_bytes_, accessible_bytes_);
        } else if (base_) {
#if defined(_WIN32)
            ::VirtualFree(base_, 0, MEM_RELEASE);
#else
            ::munmap(base_, reserved_bytes_);
#endif
        }
        base_ = nullptr;
        canonical_byte_origin_ = nullptr;
        window_slot_ = nullptr;
        reserved_bytes_ = maximum_capacity_ = accessible_bytes_ = logical_bytes_ = 0;
        zero_from_ = 0;
        page_size_ = reservation_granularity_ = 0;
    }

    char* base_{nullptr};
    std::byte* canonical_byte_origin_{nullptr};
    // Non-null exactly when this owner's reservation is a canonical window
    // slot (an element of canonical_window_array); such owners are never pooled.
    std::byte* window_slot_{nullptr};
    std::size_t reserved_bytes_{0};
    std::size_t maximum_capacity_{0};
    std::size_t accessible_bytes_{0};
    std::size_t logical_bytes_{0};
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
    if (base_ && state && !window_slot_) {
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto& limits = state->limits;
        auto& statistics = state->statistics;
        if (state->cached.size() < limits.max_cached_blocks &&
            reserved_bytes_ <= limits.max_reserved_bytes - statistics.cached_reserved_bytes &&
            accessible_bytes_ <= limits.max_accessible_bytes - statistics.cached_accessible_bytes) {
            statistics.cached_reserved_bytes += reserved_bytes_;
            statistics.cached_accessible_bytes += accessible_bytes_;
            logical_bytes_ = 0;
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
