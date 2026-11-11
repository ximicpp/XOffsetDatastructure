#pragma once

// Slot storage (R1): the library slab that holds small XBuffer payloads.
//
// A chunk is one 4 MiB mapping aligned to 4 MiB. Each chunk serves a single
// power-of-two class, from 1 KiB up to XOFFSET_SLOT_MAX_BYTES (256 KiB by
// default), and is carved on demand, so only touched pages become resident.
// A slot is aligned to its own size and starts with a 64-byte header whose
// last pointer-sized word is the owner word: it sits at payload - 8, exactly
// where a VM owner keeps it in its region's host header, so owner_from_arena and the
// allocation fast paths do not change. The payload starts at slot + 64.
//
// Each thread keeps a bounded free list per class; a global list per class,
// under one mutex, takes the overflow in batches and serves refills. A slot
// freed by another thread goes to the freeing thread's cache. Slots are never
// returned to the OS; free slots above a retention budget have their whole
// pages after the header page discarded (Linux), and slot_trim() discards all
// of them at once. Fork policy matches VmStoragePool: no atfork handlers.
//
// Zeroing follows the VM rule. The header records dirty_end, the lowest
// payload offset from which every byte is known zero. A new owner takes it as
// zero_from_ and clears only what it exposes below it.
//
// XOFFSET_SLOT_MAX_BYTES=0 disables slots: every owner is VM.
//
// Relocating buffers (XGrowPolicy::Relocating) start in a slot of
// XOFFSET_RELOCATING_RESERVE_BYTES (1 MiB by default): the slab's classes go up
// to the larger of the two sizes. Only XOFFSET_SLOT_MAX_BYTES decides where
// the other policies put a buffer (SLOT_MAX). Chunks of the classes above
// XOFFSET_SLOT_MAX_BYTES are not backed by transparent huge pages (Linux):
// a small buffer touches a page or two of its 1 MiB slot, and a huge page
// would make it 2 MiB. The retention budget counts the bytes a free slot
// has dirtied, not its nominal size.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>

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
#  if defined(XOFFSET_OWNER_MAP_SHARED)
#    include <dlfcn.h>
#  endif
#else
#  error "XOffset slot storage requires Windows, Linux, or macOS"
#endif

#if defined(__SANITIZE_ADDRESS__)
#  define XOFFSET_DETAIL_SLOT_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define XOFFSET_DETAIL_SLOT_ASAN 1
#  endif
#endif
#if defined(XOFFSET_DETAIL_SLOT_ASAN)
#  include <sanitizer/asan_interface.h>
#endif

// Per-thread caches (this slab's free lists, the free-table cache and the
// owner cache in xoffsetdatastructure.hpp). Each keeps a trivial thread_local
// flag that its nontrivial cache clears on thread exit, so late destructors
// take the global path. GCC on MinGW and Cygwin emulates TLS (emutls): a
// thread's TLS blocks can be freed before its thread_local destructors run,
// and a TLS access from such a destructor gets a fresh block, so neither the
// cache nor the flag is reliable there. Those targets get no per-thread
// caches; every path uses the global structures. Clang on MinGW uses native
// TLS and keeps them.
#ifndef XOFFSET_THREAD_CACHES
#  if (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__GNUC__) && !defined(__clang__)
#    define XOFFSET_THREAD_CACHES 0
#  else
#    define XOFFSET_THREAD_CACHES 1
#  endif
#endif

#ifndef XOFFSET_SLOT_MAX_BYTES
#  define XOFFSET_SLOT_MAX_BYTES (256u * 1024u)
#endif
#ifndef XOFFSET_RELOCATING_RESERVE_BYTES
#  define XOFFSET_RELOCATING_RESERVE_BYTES (1024u * 1024u)
#endif
// Where a relocating buffer goes when its first reservation is full: 0 is a
// reservation of its whole maximum (servers: one move), otherwise that many
// bytes and then doubling (phones, whose address space is small: iOS
// without the extended virtual addressing entitlement has about 7 GiB).
#ifndef XOFFSET_RELOCATE_STEP_BYTES
#  if defined(__ANDROID__)
#    define XOFFSET_RELOCATE_STEP_BYTES (64u * 1024u * 1024u)
#  elif defined(__APPLE__)
#    include <TargetConditionals.h>
#    if TARGET_OS_IPHONE
#      define XOFFSET_RELOCATE_STEP_BYTES (16u * 1024u * 1024u)
#    else
#      define XOFFSET_RELOCATE_STEP_BYTES 0u
#    endif
#  else
#    define XOFFSET_RELOCATE_STEP_BYTES 0u
#  endif
#endif
#ifndef XOFFSET_SLOT_RETAIN_BYTES
#  define XOFFSET_SLOT_RETAIN_BYTES (64u * 1024u * 1024u)
#endif

namespace XOffsetDatastructure {
// Owner-table lookups sit on every allocate, grow and retire; GCC stops
// inlining them once the table reads are atomic, so force it.
#if defined(__GNUC__) || defined(__clang__)
#  define XOFFSET_DETAIL_OWNER_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#  define XOFFSET_DETAIL_OWNER_INLINE __forceinline
#else
#  define XOFFSET_DETAIL_OWNER_INLINE inline
#endif

namespace detail {

constexpr std::size_t slot_log2(std::size_t value) noexcept {
    std::size_t shift = 0;
    while ((std::size_t(1) << shift) < value) ++shift;
    return shift;
}

// V2.1 owner lookup: the payload base of an address (a head names no owner).
// One byte per 4 MiB granule of a 48-bit address space. 0: no XOffset region.
// Otherwise bit 7 is set and bits 0..5 are k: the region (a slot, or a VM
// mapping) starts at a 2^k boundary with the 64-byte host header, and its
// payload of at most 2^k bytes follows. For every payload byte a, a - 64 lies
// in [region, region + 2^k), so masking it with ~(2^k - 1) finds the region
// and the payload is 64 bytes past it. A VM payload whose maximum is 2^k (the
// 64 MiB default) ends up to 64 bytes into the granule past 2^k; that granule
// belongs to the mapping and carries the same entry. Slot chunks set their entry once; a VM payload
// is aligned to 2^k and sets the entries of its granules while it is mapped.
// Entries change only while no head lives in the region. One granule's entry
// can still be written by two threads: one clears it and unmaps the range,
// another maps the range again and sets it. The kernel orders the two, C++
// does not, so every access is a relaxed atomic (a plain byte load or store).
struct OwnerMap {
    static constexpr std::size_t granule_shift = 22;
    static constexpr std::size_t granule_bytes = std::size_t(1) << granule_shift;
    static constexpr std::size_t address_bits = 48;
    static constexpr std::size_t entries = std::size_t(1) << (address_bits - granule_shift);
    static constexpr std::uint8_t slot_bit = 0x80;   // a region: payload 64 bytes in
    static constexpr std::uintptr_t header_bytes = 64;
    // The payload base for a nonzero entry `e` read at payload address `a`.
    XOFFSET_DETAIL_OWNER_INLINE static std::uintptr_t decode(std::uintptr_t a, std::uint8_t e) noexcept {
        return ((a - header_bytes) & (~std::uintptr_t(0) << (e & 63u))) + header_bytes;
    }

    static std::size_t index(std::uintptr_t address) noexcept {
        return (address >> granule_shift) & (entries - 1);
    }
    // Makes the table usable before a region registers; false when it cannot
    // be allocated (only on Windows, where it is allocated at run time).
    static bool ready() noexcept;
    static std::uint8_t entry(std::uintptr_t address) noexcept;
    // Where entry(address) is read from, or null while no table exists; for
    // a prefetch hint only (it is not an atomic access).
    static const std::uint8_t* entry_address(std::uintptr_t address) noexcept;
    // Relaxed atomic byte access; this header is C++17, without std::atomic_ref.
    XOFFSET_DETAIL_OWNER_INLINE static std::uint8_t load(const std::uint8_t& e) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return __atomic_load_n(&e, __ATOMIC_RELAXED);
#else
        return *static_cast<const volatile std::uint8_t*>(&e);
#endif
    }
    XOFFSET_DETAIL_OWNER_INLINE static void store(std::uint8_t& e, std::uint8_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __atomic_store_n(&e, value, __ATOMIC_RELAXED);
#else
        *static_cast<volatile std::uint8_t*>(&e) = value;
#endif
    }
    static char* payload_of(const void* p) noexcept;
    // The payload of a if b lies in the same payload, else null. One table
    // read when both are in one granule (granules are never shared).
    static char* same_payload(const void* a, const void* b) noexcept;
    // Sets (or with entry 0 clears) the entries of [begin, begin + bytes).
    static void set(const void* begin, std::size_t bytes, std::uint8_t value) noexcept;
    static bool reachable(const void* begin, std::size_t bytes) noexcept {
        const auto a = reinterpret_cast<std::uintptr_t>(begin);
        return bytes != 0 && (a >> address_bits) == 0 &&
               bytes <= (std::uintptr_t(1) << address_bits) - a;
    }
};

#if defined(_WIN32) || defined(XOFFSET_OWNER_MAP_SHARED)
// A PE image stores zero-initialized COMDAT data in the file (MinGW GCC puts
// this table in .data: a 64 MiB executable), so on Windows the table is
// committed at the first registration. While it is null no region exists and
// every lookup finds no owner. The pointer is set once and never changes; a
// thread that holds a head in a region has synchronized with the region's
// registration, so relaxed loads see it.
//
// XOFFSET_OWNER_MAP_SHARED (Linux, Apple): the same, for a process that loads
// many shared libraries built with this header (the phone test host,
// tools/test_host). Each library would map its own 64 MiB table, and about 50
// fill an iPhone process's address space; with it they take the one table the
// executable exports as xoffset_owner_map_shared(bytes), or else map their own.
inline std::atomic<std::uint8_t*> owner_map_table{nullptr};

inline bool OwnerMap::ready() noexcept {
    if (owner_map_table.load(std::memory_order_acquire)) return true;
#if defined(_WIN32)
    auto* fresh = static_cast<std::uint8_t*>(
        ::VirtualAlloc(nullptr, entries, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!fresh) return false;
    std::uint8_t* expected = nullptr;
    if (!owner_map_table.compare_exchange_strong(expected, fresh, std::memory_order_acq_rel))
        ::VirtualFree(fresh, 0, MEM_RELEASE);
#else
    using Shared = std::uint8_t* (*)(std::size_t);
    std::uint8_t* fresh = nullptr;
    bool mapped = false;
    if (auto shared = reinterpret_cast<Shared>(::dlsym(RTLD_DEFAULT, "xoffset_owner_map_shared"))) {
        fresh = shared(entries);
    } else {
        void* p = ::mmap(nullptr, entries, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        fresh = p == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(p);
        mapped = fresh != nullptr;
    }
    if (!fresh) return false;
    std::uint8_t* expected = nullptr;
    if (!owner_map_table.compare_exchange_strong(expected, fresh, std::memory_order_acq_rel) && mapped)
        ::munmap(fresh, entries);
#endif
    return true;
}

XOFFSET_DETAIL_OWNER_INLINE std::uint8_t OwnerMap::entry(std::uintptr_t address) noexcept {
    const std::uint8_t* table = owner_map_table.load(std::memory_order_relaxed);
    return table ? load(table[index(address)]) : std::uint8_t(0);
}

XOFFSET_DETAIL_OWNER_INLINE const std::uint8_t* OwnerMap::entry_address(std::uintptr_t address) noexcept {
    const std::uint8_t* table = owner_map_table.load(std::memory_order_relaxed);
    return table ? table + index(address) : nullptr;
}
#else
// Zero-initialized (BSS): pages the program never touches cost no memory.
// Mach-O compilers put a weak (inline) zero-initialized variable in
// __DATA,__data, which stores all 64 MiB in the executable; the zero-fill
// section keeps it out of the file (the linker still merges the copies).
#if defined(__APPLE__)
__attribute__((section("__DATA,__bss")))
#endif
inline std::uint8_t owner_map_table[OwnerMap::entries];

inline bool OwnerMap::ready() noexcept { return true; }

XOFFSET_DETAIL_OWNER_INLINE std::uint8_t OwnerMap::entry(std::uintptr_t address) noexcept {
    return load(owner_map_table[index(address)]);
}

XOFFSET_DETAIL_OWNER_INLINE const std::uint8_t* OwnerMap::entry_address(std::uintptr_t address) noexcept {
    return owner_map_table + index(address);
}
#endif

XOFFSET_DETAIL_OWNER_INLINE char* OwnerMap::payload_of(const void* p) noexcept {
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    const std::uint8_t e = entry(a);
    if (e == 0) return nullptr;
    return reinterpret_cast<char*>(decode(a, e));
}

XOFFSET_DETAIL_OWNER_INLINE char* OwnerMap::same_payload(const void* a, const void* b) noexcept {
    const auto ua = reinterpret_cast<std::uintptr_t>(a);
    const auto ub = reinterpret_cast<std::uintptr_t>(b);
    const std::uint8_t e = entry(ua);
    if (e == 0) return nullptr;
    char* const pa = reinterpret_cast<char*>(decode(ua, e));
    // In one granule, two payload bytes share a region exactly when they share
    // a 2^k block: a payload byte is never in its region's first 64 bytes, and
    // a 2^k boundary is a granule boundary when 2^k covers granules.
    if (index(ua) == index(ub)) return ((ua ^ ub) >> (e & 63u)) == 0 ? pa : nullptr;
    return payload_of(b) == pa ? pa : nullptr;
}

inline void OwnerMap::set(const void* begin, std::size_t bytes, std::uint8_t value) noexcept {
    if (bytes == 0) return;
#if defined(_WIN32) || defined(XOFFSET_OWNER_MAP_SHARED)
    std::uint8_t* const table = owner_map_table.load(std::memory_order_acquire);
    if (!table) return;  // entry 0 before any registration: nothing to clear
#else
    std::uint8_t* const table = owner_map_table;
#endif
    const auto a = reinterpret_cast<std::uintptr_t>(begin);
    for (std::size_t i = index(a), last = index(a + bytes - 1); ; ++i) {
        store(table[i], value);
        if (i == last) break;
    }
}

struct SlotGeometry {
    static constexpr std::size_t header_bytes = 64;
    static constexpr std::size_t min_shift = 10;   // 1 KiB
    static constexpr std::size_t chunk_shift = 22; // 4 MiB
    static constexpr std::size_t chunk_bytes = std::size_t(1) << chunk_shift;
    // The Auto storage rule's largest slot (SLOT_MAX + 64).
    static constexpr std::size_t max_slot_bytes = XOFFSET_SLOT_MAX_BYTES;
    static constexpr bool enabled = max_slot_bytes != 0;
    static_assert(max_slot_bytes == 0 ||
                  ((max_slot_bytes & (max_slot_bytes - 1)) == 0 &&
                   max_slot_bytes >= (std::size_t(1) << min_shift) && max_slot_bytes <= chunk_bytes),
                  "XOFFSET_SLOT_MAX_BYTES must be 0 or a power of two from 1 KiB to 4 MiB");
    // A relocating buffer's first reservation (its slot, payload + 64).
    static constexpr std::size_t relocating_slot_bytes = XOFFSET_RELOCATING_RESERVE_BYTES;
    static_assert((relocating_slot_bytes & (relocating_slot_bytes - 1)) == 0 &&
                  relocating_slot_bytes >= (std::size_t(1) << min_shift) && relocating_slot_bytes <= chunk_bytes,
                  "XOFFSET_RELOCATING_RESERVE_BYTES must be a power of two from 1 KiB to 4 MiB");
    // The slab's largest slot: both of the above (none without slots).
    static constexpr std::size_t slab_slot_bytes =
        !enabled ? 0 : max_slot_bytes > relocating_slot_bytes ? max_slot_bytes : relocating_slot_bytes;
    static constexpr std::size_t classes = enabled ? slot_log2(slab_slot_bytes) - min_shift + 1 : 0;
    // The classes the Auto rule uses; the others serve relocating buffers only.
    static constexpr std::size_t auto_classes = enabled ? slot_log2(max_slot_bytes) - min_shift + 1 : 0;
    // Largest payload the Auto rule puts in a slot (SLOT_MAX).
    static constexpr std::size_t max_payload = enabled ? max_slot_bytes - header_bytes : 0;
    // Largest payload any slot holds.
    static constexpr std::size_t slab_max_payload = enabled ? slab_slot_bytes - header_bytes : 0;
    // A relocating buffer's first payload reservation.
    static constexpr std::size_t relocating_payload = relocating_slot_bytes - header_bytes;
    static constexpr std::size_t retain_bytes = XOFFSET_SLOT_RETAIN_BYTES;

    static constexpr std::size_t slot_bytes(std::size_t cls) noexcept {
        return std::size_t(1) << (min_shift + cls);
    }
    static constexpr std::size_t payload_bytes(std::size_t cls) noexcept {
        return slot_bytes(cls) - header_bytes;
    }
    // Smallest Auto class whose payload holds `bytes`; `classes` if none does.
    static constexpr std::size_t class_for(std::size_t bytes) noexcept {
        if (!enabled || bytes > max_payload) return classes;
        std::size_t cls = 0;
        while (payload_bytes(cls) < bytes) ++cls;
        return cls;
    }
    // Smallest slab class whose payload holds `bytes`; `classes` if none does.
    static constexpr std::size_t slab_class_for(std::size_t bytes) noexcept {
        if (!enabled || bytes > slab_max_payload) return classes;
        std::size_t cls = 0;
        while (payload_bytes(cls) < bytes) ++cls;
        return cls;
    }
};
static_assert(SlotGeometry::header_bytes == OwnerMap::header_bytes, "owner lookup assumes the 64-byte host header");

struct SlotHeader {
    SlotHeader* next;       // free-list link, only while free
    std::size_t dirty_end;  // payload offset from which every byte is zero
    std::uint32_t cls;
    std::uint32_t accounted; // bytes it adds to the global free_dirty while listed
    char unused[SlotGeometry::header_bytes - 3 * sizeof(void*) - 2 * sizeof(std::uint32_t)];
    void* owner;            // payload - 8: the XBufferCore owner word

    char* payload() noexcept { return reinterpret_cast<char*>(this) + SlotGeometry::header_bytes; }
    static SlotHeader* from_payload(char* payload) noexcept {
        return reinterpret_cast<SlotHeader*>(payload - SlotGeometry::header_bytes);
    }
};
static_assert(sizeof(SlotHeader) == SlotGeometry::header_bytes);
static_assert(offsetof(SlotHeader, owner) == SlotGeometry::header_bytes - sizeof(void*));

struct SlotStorageStats {
    std::size_t chunks{0};              // mapped 4 MiB chunks (address space)
    std::size_t carved_bytes{0};        // bytes of chunks handed out as slots so far
    std::size_t global_free_slots{0};   // free slots in the global lists
    std::size_t global_free_bytes{0};   // nominal (slot sizes)
    std::size_t global_free_dirty_bytes{0}; // dirtied bytes of those slots (the retention budget)
    std::uint64_t discarded_slots{0};   // slots whose tail pages were discarded
};

class SlotSlab {
public:
    static constexpr std::size_t classes = SlotGeometry::classes;

    // Returns a slot of class `cls` with its payload unpoisoned, or nullptr.
    static SlotHeader* acquire(std::size_t cls) noexcept {
        SlotHeader* slot = nullptr;
        if (cache_available()) {
            auto& cache = thread_cache();
            if (!cache.head[cls]) refill(cache, cls);
            slot = cache.head[cls];
            if (slot) {
                cache.head[cls] = slot->next;
                --cache.count[cls];
            }
        } else {
            slot = take_one(cls);
        }
        if (slot) {
            slot->next = nullptr;
            unpoison(slot);
        }
        return slot;
    }

    // `slot` must be free of any owner; its dirty_end must be current.
    static void release(SlotHeader* slot) noexcept {
        const auto cls = slot->cls;
        poison(slot);
        if (!cache_available()) {
            give_back(slot, slot, 1, cls);
            return;
        }
        auto& cache = thread_cache();
        slot->next = cache.head[cls];
        cache.head[cls] = slot;
        if (++cache.count[cls] > cache_limit(cls)) {
            // Return the older half; the most recently freed stay warm here.
            const auto keep = cache_limit(cls) / 2u;
            SlotHeader* last_kept = cache.head[cls];
            for (std::size_t i = 1; i < keep; ++i) last_kept = last_kept->next;
            SlotHeader* first = last_kept->next;
            SlotHeader* last = first;
            std::size_t moved = 1;
            while (last->next) { last = last->next; ++moved; }
            last_kept->next = nullptr;
            cache.count[cls] = keep;
            give_back(first, last, moved, cls);
        }
    }

    // Discards the pages of every free slot (this thread's cache is flushed
    // first). Live slots and other threads' caches are untouched.
    static void trim() noexcept {
        if (cache_available()) thread_cache().flush();
        auto& state = global();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (std::size_t cls = 0; cls < classes; ++cls)
            for (auto* slot = state.free[cls]; slot; slot = slot->next) discard(state, slot);
    }

    // Bytes a free slot has dirtied: its header and payload below dirty_end
    // (what the retention budget counts).
    static std::size_t dirty_bytes(const SlotHeader* slot) noexcept {
        const auto payload = SlotGeometry::payload_bytes(slot->cls);
        return SlotGeometry::header_bytes + (slot->dirty_end < payload ? slot->dirty_end : payload);
    }

    static SlotStorageStats stats() noexcept {
        auto& state = global();
        std::lock_guard<std::mutex> lock(state.mutex);
        SlotStorageStats result = state.stats;
        result.global_free_dirty_bytes = state.free_dirty;
        for (std::size_t cls = 0; cls < classes; ++cls) {
            result.global_free_slots += state.count[cls];
            result.global_free_bytes += state.count[cls] * SlotGeometry::slot_bytes(cls);
        }
        return result;
    }

    static std::size_t page_size() noexcept {
        static const std::size_t page = [] {
#if defined(_WIN32)
            SYSTEM_INFO info{};
            ::GetSystemInfo(&info);
            return static_cast<std::size_t>(info.dwPageSize);
#else
            const long value = ::sysconf(_SC_PAGESIZE);
            return value > 0 ? static_cast<std::size_t>(value) : std::size_t(4096);
#endif
        }();
        return page;
    }

private:
    struct State {
        std::mutex mutex;
        SlotHeader* free[classes ? classes : 1] = {};
        std::size_t count[classes ? classes : 1] = {};
        char* carve[classes ? classes : 1] = {};
        char* carve_end[classes ? classes : 1] = {};
        std::size_t free_dirty = 0; // sum of `accounted` over the global lists
        SlotStorageStats stats;
    };

    struct ThreadCache {
        SlotHeader* head[classes ? classes : 1] = {};
        std::size_t count[classes ? classes : 1] = {};
        void flush() noexcept {
            for (std::size_t cls = 0; cls < classes; ++cls) {
                if (!head[cls]) continue;
                SlotHeader* last = head[cls];
                while (last->next) last = last->next;
                give_back(head[cls], last, count[cls], cls);
                head[cls] = nullptr;
                count[cls] = 0;
            }
        }
        ~ThreadCache() {
            cache_available() = false;
            flush();
        }
    };

    // Up to 64 slots or 1 MiB per class, and at least 4.
    static constexpr std::size_t cache_limit(std::size_t cls) noexcept {
        const auto by_bytes = (std::size_t(1) << 20) / SlotGeometry::slot_bytes(cls);
        return by_bytes > 64 ? 64 : by_bytes < 4 ? 4 : by_bytes;
    }

    // Same pattern as the free-table cache: this trivial flag outlives the
    // cache, so late thread and static destructors use the global lists.
    static bool& cache_available() noexcept {
#if XOFFSET_THREAD_CACHES
        static thread_local bool available = true;
#else
        static bool available = false; // never set: see XOFFSET_THREAD_CACHES
#endif
        return available;
    }
    static ThreadCache& thread_cache() noexcept {
        static thread_local ThreadCache cache;
        return cache;
    }
    static State& global() noexcept {
        // Process lifetime: owners may be released after static destructors.
        alignas(State) static unsigned char storage[sizeof(State)];
        static State* state = ::new (static_cast<void*>(storage)) State;
        return *state;
    }

    static void refill(ThreadCache& cache, std::size_t cls) noexcept {
        auto& state = global();
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto batch = cache_limit(cls) / 2u;
        for (std::size_t i = 0; i < batch; ++i) {
            SlotHeader* slot = pop_locked(state, cls);
            if (!slot) break;
            slot->next = cache.head[cls];
            cache.head[cls] = slot;
            ++cache.count[cls];
        }
    }

    static SlotHeader* take_one(std::size_t cls) noexcept {
        auto& state = global();
        std::lock_guard<std::mutex> lock(state.mutex);
        return pop_locked(state, cls);
    }

    static SlotHeader* pop_locked(State& state, std::size_t cls) noexcept {
        if (SlotHeader* slot = state.free[cls]) {
            state.free[cls] = slot->next;
            --state.count[cls];
            state.free_dirty -= slot->accounted;
            slot->accounted = 0;
            return slot;
        }
        const auto bytes = SlotGeometry::slot_bytes(cls);
        if (state.carve[cls] == state.carve_end[cls]) {
            if (!OwnerMap::ready()) return nullptr;
            char* chunk = map_chunk(cls);
            if (!chunk) return nullptr;
            if (!OwnerMap::reachable(chunk, SlotGeometry::chunk_bytes)) return nullptr;
            OwnerMap::set(chunk, SlotGeometry::chunk_bytes, static_cast<std::uint8_t>(
                OwnerMap::slot_bit | (SlotGeometry::min_shift + cls)));
            state.carve[cls] = chunk;
            state.carve_end[cls] = chunk + SlotGeometry::chunk_bytes;
            ++state.stats.chunks;
        }
        auto* slot = reinterpret_cast<SlotHeader*>(state.carve[cls]);
        state.carve[cls] += bytes;
        state.stats.carved_bytes += bytes;
        // Fresh anonymous pages: the whole slot reads as zero.
        slot->next = nullptr;
        slot->dirty_end = 0;
        slot->cls = static_cast<std::uint32_t>(cls);
        slot->accounted = 0;
        slot->owner = nullptr;
        return slot;
    }

    static void give_back(SlotHeader* first, SlotHeader* last, std::size_t count,
                          std::size_t cls) noexcept {
        auto& state = global();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (auto* slot = first; ; slot = slot->next) {
            slot->accounted = static_cast<std::uint32_t>(dirty_bytes(slot));
            state.free_dirty += slot->accounted;
            if (slot == last) break;
        }
        last->next = state.free[cls];
        state.free[cls] = first;
        state.count[cls] += count;
        if (state.free_dirty > SlotGeometry::retain_bytes)
            for (auto* slot = first; slot != last->next; slot = slot->next) discard(state, slot);
    }

    // Discards the whole pages after the header page. Linux reads them back
    // as zero; elsewhere their content is unspecified and dirty_end stays.
    static void discard(State& state, SlotHeader* slot) noexcept {
        const auto page = page_size();
        const auto bytes = SlotGeometry::slot_bytes(slot->cls);
        if (bytes < 2 * page) return;
        char* const from = reinterpret_cast<char*>(slot) + page;
        const auto length = bytes - page;
        const auto clean_from = page - SlotGeometry::header_bytes;
        if (slot->dirty_end <= clean_from) return;
#if defined(__linux__)
        if (::madvise(from, length, MADV_DONTNEED) != 0) return;
        slot->dirty_end = clean_from;
#elif defined(__APPLE__)
        (void)::madvise(from, length, MADV_FREE);
#else
        (void)::VirtualAlloc(from, length, MEM_RESET, PAGE_READWRITE);
#endif
        // The pages are gone (elsewhere dirty_end stays, for zeroing, but
        // the memory is returned): only the header page still counts.
        if (slot->accounted > page) {
            state.free_dirty -= slot->accounted - page;
            slot->accounted = static_cast<std::uint32_t>(page);
        }
        ++state.stats.discarded_slots;
    }

    static char* map_chunk([[maybe_unused]] std::size_t cls) noexcept {
        constexpr auto bytes = SlotGeometry::chunk_bytes;
#if defined(_WIN32)
        for (int attempt = 0; attempt < 8; ++attempt) {
            void* probe = ::VirtualAlloc(nullptr, 2 * bytes, MEM_RESERVE, PAGE_NOACCESS);
            if (!probe) return nullptr;
            const auto aligned = (reinterpret_cast<std::uintptr_t>(probe) + bytes - 1) & ~(bytes - 1);
            ::VirtualFree(probe, 0, MEM_RELEASE);
            if (void* chunk = ::VirtualAlloc(reinterpret_cast<void*>(aligned), bytes,
                                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
                return static_cast<char*>(chunk);
        }
        return nullptr;
#else
        void* raw = ::mmap(nullptr, 2 * bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED) return nullptr;
        const auto start = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (start + bytes - 1) & ~(bytes - 1);
        if (aligned != start) ::munmap(raw, aligned - start);
        const auto tail = start + 2 * bytes - (aligned + bytes);
        if (tail) ::munmap(reinterpret_cast<void*>(aligned + bytes), tail);
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
        // Relocating-only classes: a buffer touches a page or two of its slot.
        if (cls >= SlotGeometry::auto_classes)
            (void)::madvise(reinterpret_cast<void*>(aligned), bytes, MADV_NOHUGEPAGE);
#endif
        return reinterpret_cast<char*>(aligned);
#endif
    }

    static void poison([[maybe_unused]] SlotHeader* slot) noexcept {
#if defined(XOFFSET_DETAIL_SLOT_ASAN)
        ASAN_POISON_MEMORY_REGION(slot->payload(), SlotGeometry::payload_bytes(slot->cls));
#endif
    }
    static void unpoison([[maybe_unused]] SlotHeader* slot) noexcept {
#if defined(XOFFSET_DETAIL_SLOT_ASAN)
        ASAN_UNPOISON_MEMORY_REGION(slot->payload(), SlotGeometry::payload_bytes(slot->cls));
#endif
    }
};

} // namespace detail
} // namespace XOffsetDatastructure
