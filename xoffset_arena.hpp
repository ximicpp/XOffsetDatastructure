#ifndef X_OFFSET_ARENA_HPP
#define X_OFFSET_ARENA_HPP

// The arena: XBufferCore (payload, allocator, free blocks, growth and
// relocation) and the allocator handle XArenaAllocatorBase. Part of
// xoffsetdatastructure.hpp; include that header. Memory model and reuse:
// docs/USER_GUIDE.md and docs/BUFFER_REUSE.md.

#include "xoffset_config.hpp"
#include "xoffset_schema.hpp"
#include "xoffset_admission.hpp"
#include "xoffset_fixed_string.hpp"
#include <cstddef>
#include <cstdlib>
#include <compare>
#include <iterator>
#include <concepts>
#include <functional>
#include <cstdint>
#include <span>
#include <type_traits>
#include <string>
#include <string_view>
#include <ostream>
#include <utility>
#include <array>
#include <bit>
#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <exception>
#include <optional>
#include <mutex>
#include <atomic>
#include <map>
#include <unordered_map>
#if defined(__linux__)
#include <cerrno>
#endif
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
#include <cstdio>
#include <typeinfo>
#endif
#include "xoffset_virtual_memory.hpp"

// Platform: 64-bit little-endian only.
// Type safety: K0 admission is XOffset's own (xoffset_admission.hpp, is_byte_copy_safe_v<T>).
#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
static_assert(sizeof(void*) == 8,
    "XOffsetDatastructure requires 64-bit platform (sizeof(void*) must be 8)");
static_assert(XOFFSET_LITTLE_ENDIAN,
    "XOffsetDatastructure requires little-endian platform");
#endif // XOFFSET_DISABLE_PLATFORM_CHECKS

// Apple: load_verified uses std::pmr from the
// system libc++, which Apple ships from macOS 14 / iOS 17 / tvOS 17 / watchOS 10.
// The SDK does not mark those symbols with availability, so an older
// deployment target compiles and links, then fails at launch on the old
// system. Fail the build instead. Only the system libc++ is checked: its
// headers are configured with vendor availability annotations, while a
// toolchain's own libc++ (P2996 Clang, linked with its rpath) and libstdc++
// carry std::pmr themselves.
#if defined(__APPLE__)
#include <version>
#if defined(_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS)
#define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX _LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS
#elif defined(_LIBCPP_VERSION) && !defined(_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS)
#define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX 1
#else
#define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX 0
#endif
#if !XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 140000
#error "XOffset needs macOS 14 or newer (std::pmr in the system libc++): build with -mmacosx-version-min=14.0 (CMake: CMAKE_OSX_DEPLOYMENT_TARGET=14.0)"
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 170000
#error "XOffset needs iOS 17 or newer (std::pmr in the system libc++): build with -miphoneos-version-min=17.0 (CMake: CMAKE_OSX_DEPLOYMENT_TARGET=17.0)"
#elif defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 170000
#error "XOffset needs tvOS 17 or newer (std::pmr in the system libc++): raise the deployment target to 17.0"
#elif defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 100000
#error "XOffset needs watchOS 10 or newer (std::pmr in the system libc++): raise the deployment target to 10.0"
#endif
#endif

namespace XOffsetDatastructure {
    template <typename T> struct XVectorCore;

    namespace detail {
        template<class T> T* begin_dynamic_array(void*, std::size_t) noexcept;
        template<class T> T* rebuild_dynamic_array(void*, std::size_t, std::uint32_t) noexcept;
        template<class T> T* begin_fresh_ordinary_outer(void*) noexcept;
        template<class T> T* begin_relocated_ordinary_outer(T*, const T&) noexcept;
        template<class T> consteval bool shift_relocate_supported();
        template<class T> void shift_rebase_moved(T*, std::size_t, std::int32_t) noexcept;
        template<class T> void empty_vacated_slot(T*) noexcept;
        template<class T> consteval bool settles_vacated_heads();
        template<class T> void zero_implicit_padding(T*) noexcept;
        template<class T> void scrub_implicit_padding(T*) noexcept;
        template<class T> consteval bool subtree_copy_supported();
        template<class T> bool subtree_copy_assign(XVectorCore<T>&, const XVectorCore<T>&);
#if defined(XOFFSET_TEST_HOOKS)
        // Test hooks: a per-thread switch that sends container copy
        // assignment down the element-wise reference path, and counters.
        struct SubtreeCopyStats {
            std::uint64_t whole_hits = 0;        // contiguous source: one memcpy
            std::uint64_t compact_hits = 0;      // any other source: compacted
            std::uint64_t pod_hits = 0;          // trivially copyable element copies
            std::uint64_t whole_skipped_layout = 0; // source not one contiguous range
            std::uint64_t whole_skipped_slack = 0;  // contiguous, spare bytes over the bound
            std::uint64_t fallback_type = 0;     // element type not admitted: element-wise
            std::uint64_t fallback_alloc = 0;    // segment did not fit (XBadAlloc): element-wise
            std::uint64_t whole_retires = 0;     // old content retired as one range
            std::uint64_t piecewise_retires = 0; // old content retired block by block
            std::uint64_t bytes = 0;             // segment bytes of whole and compact copies
        };
        inline thread_local bool subtree_copy_hook_enabled = true;
        inline thread_local SubtreeCopyStats subtree_copy_stats{};
        inline bool subtree_copy_enabled() noexcept { return subtree_copy_hook_enabled; }
#define XOFFSET_DETAIL_SUBTREE_COUNT(field, n) \
        (::XOffsetDatastructure::detail::subtree_copy_stats.field += (n))
#else
        constexpr bool subtree_copy_enabled() noexcept { return true; }
#define XOFFSET_DETAIL_SUBTREE_COUNT(field, n) ((void)0)
#endif
        struct VerifiedPathsAccess;
        struct DeltaAccess;
        struct ReceiverAccess;
        // Registration is the only registry operation allowed to allocate.
        // One-shot thread-local injection leaves existing owners untouched.
        inline thread_local bool pending_registry_failure = false;
        inline void fail_next_registry_registration() noexcept { pending_registry_failure = true; }
        inline void clear_registry_failures() noexcept { pending_registry_failure = false; }

        // std::malloc as an allocator (the owner record, see owner_identity):
        // it never runs a new_handler.
        template <typename T>
        struct MallocAllocator {
            using value_type = T;
            MallocAllocator() = default;
            template <typename U>
            MallocAllocator(const MallocAllocator<U>&) noexcept {}
            T* allocate(std::size_t n) {
                if (n > std::size_t(-1) / sizeof(T)) XOFFSET_THROW(std::bad_alloc());
                if (void* p = std::malloc(n * sizeof(T))) return static_cast<T*>(p);
                XOFFSET_THROW(std::bad_alloc());
            }
            void deallocate(T* p, std::size_t) noexcept { std::free(p); }
            template <typename U>
            bool operator==(const MallocAllocator<U>&) const noexcept { return true; }
        };

        // Logical host-identity diagnostics; disabled in ordinary production.
        struct OwnerLookupStats {
            std::uint64_t entries = 0, cache_hits = 0, cold_lookups = 0;
            std::uint64_t dead_records = 0, registrations = 0, owner_updates = 0;
            std::uint64_t slow_revalidations = 0, record_object_bytes = 0;
        };
#if defined(XOFFSET_OWNER_LOOKUP_DIAGNOSTICS) && XOFFSET_OWNER_LOOKUP_DIAGNOSTICS
        inline thread_local OwnerLookupStats owner_lookup_stats;
        inline void reset_owner_lookup_stats() noexcept { owner_lookup_stats = {}; }
        inline void owner_lookup_count(std::uint64_t OwnerLookupStats::* field,
                                       std::uint64_t count = 1) noexcept {
            owner_lookup_stats.*field += count;
        }
#else
        inline void owner_lookup_count(std::uint64_t OwnerLookupStats::*,
                                       std::uint64_t = 1) noexcept {}
#endif
    }

    template <typename T, typename Arena>
    class XArenaAllocatorBase {
    public:
        using value_type = T;

        XArenaAllocatorBase() = default;
        explicit XArenaAllocatorBase(Arena* arena) noexcept : arena_(arena) {}

        template <typename U>
        XArenaAllocatorBase(const XArenaAllocatorBase<U, Arena>& other) noexcept
            : arena_(other.arena()) {}

        template <typename U>
        struct rebind {
            using other = XArenaAllocatorBase<U, Arena>;
        };

        T* allocate(std::size_t n) {
            if (!arena_) {
                XOFFSET_THROW(XException(XErrc::invalid_state, "allocator has no arena"));
            }
            if (n > 0x7ffffff8u / sizeof(T)) XOFFSET_THROW(XBadAlloc());
            if (n == 0) return nullptr;
            return detail::begin_dynamic_array<T>(
                arena_->allocate_aligned(
                    static_cast<std::uint32_t>(n * sizeof(T)), alignof(T)), n);
        }

        // V2: the block extent is derived from n (as allocated).
        void deallocate(T* ptr, std::size_t n) noexcept {
            if (arena_ && ptr) {
                arena_->deallocate(ptr, static_cast<std::uint32_t>(n * sizeof(T)));
            }
        }

        Arena* arena() const noexcept { return arena_; }

        template <typename U>
        bool operator==(const XArenaAllocatorBase<U, Arena>& other) const noexcept {
            return arena_ == other.arena();
        }

        template <typename U>
        bool operator!=(const XArenaAllocatorBase<U, Arena>& other) const noexcept {
            return !(*this == other);
        }

    private:
        Arena* arena_{};
    };

    template <typename T>
    struct schema_name {
        static constexpr std::string_view value = "";
    };

    template <typename T>
    inline constexpr bool has_schema_name_v = !schema_name<T>::value.empty();

    class XString;
    template <typename T> struct XVectorCore;
    template <typename T> class XSet;
    template <typename K, typename V> class XMap;
    template <typename K, typename V> struct XKeyValue;

    // Explicit: the payload grows only at grow(). Automatic: container
    // operations grow it in place up to the maximum (a reservation of the
    // whole maximum, so the address never changes). Relocating: it starts in
    // a reservation sized from its first content (twice it, a power of two
    // from 64 KiB to 1 MiB: XBufferCore::relocating_first_reserve; not
    // zeroed), container operations grow it in place inside that reservation, and
    // make_room() / grow() move it to a larger one when it is full (servers:
    // one move, to a reservation of the whole maximum; phones: 16 MiB on iOS,
    // 64 MiB on Android, then doubling). A move invalidates every pointer and
    // reference into the payload (epoch); see make_room().
    enum class XGrowPolicy : std::uint8_t { Explicit, Automatic, Relocating };
    using XBufferPool = detail::VmStoragePool;
    // Storage request of a MaximumCapacity. Auto picks by the rule in
    // XBufferCore::plan_storage; Pinned is always a VM reservation (address
    // stable for life); Movable requires a slot and throws where the rule
    // cannot give one.
    enum class XStorage { Auto, Pinned, Movable };
    // Storage an owner is on. Only MovableSlot changes address, and only at
    // grow(), reload_verified, begin_verified_reload or reset.
    enum class XStorageKind { Vm, PinnedSlot, MovableSlot };
    using XSlotStorageStats = detail::SlotStorageStats;




    namespace fixed_detail {
        struct StableEraseOwnerWitness;
        struct allocator_ref;
    }

    class XBufferCore;
#if defined(XOFFSET_STRESS_RELOCATE)
    namespace detail {
        // Stress mode, test builds only: a pattern no
        // live value is expected to hold, written over storage the library
        // stopped using, so a stale plain reference reads garbage at once.
        inline constexpr unsigned char stress_poison_byte = 0xDB;

        // Storage a payload moved out of is not released at once: its whole
        // pages become inaccessible (a stale plain reference faults), the rest
        // is overwritten with the poison pattern, and it stays reserved until
        // 32 later moves have happened (so the addresses are not reused by
        // the next buffer meanwhile).
        class StressGraveyard {
        public:
            static void bury(VirtualMemoryStorage&& old) noexcept {
                if (!old.data()) return;
                Grave grave;
                grave.storage = std::move(old);
                seal(grave);
                std::lock_guard<std::mutex> lock(state().mutex);
                auto& graves = state().graves;
                if (graves.size() >= limit) {
                    unseal(graves.front());
                    graves.erase(graves.begin());   // released here
                }
                XOFFSET_TRY {
                    graves.push_back(std::move(grave));
                } XOFFSET_CATCH(...) {
                    unseal(grave);
                }
            }

        private:
            static constexpr std::size_t limit = 32;
            struct Grave {
                VirtualMemoryStorage storage;
                char* lo = nullptr;
                std::size_t bytes = 0;
            };
            struct State {
                std::mutex mutex;
                std::vector<Grave> graves;
            };
            // Never destroyed: graves may outlive every other static.
            static State& state() noexcept {
                static State* s = new State;
                return *s;
            }
            static void seal(Grave& grave) noexcept {
                auto& st = grave.storage;
                char* const data = st.data();
                const std::size_t used = (std::max)(st.size(), st.accessible_bytes());
                const std::size_t page = st.page_size() ? st.page_size() : 4096u;
                const auto region = reinterpret_cast<std::uintptr_t>(data) - VirtualMemoryStorage::host_header;
                const auto end = reinterpret_cast<std::uintptr_t>(data) + st.accessible_bytes();
                const auto lo = (region + page - 1) & ~std::uintptr_t(page - 1);
                const auto hi = end & ~std::uintptr_t(page - 1);
                if (st.size() != 0) std::memset(data, stress_poison_byte, (std::min)(st.size(), used));
#if !defined(_WIN32)
                if (hi > lo && ::mprotect(reinterpret_cast<void*>(lo), hi - lo, PROT_NONE) == 0) {
                    grave.lo = reinterpret_cast<char*>(lo);
                    grave.bytes = hi - lo;
                }
#endif
            }
            static void unseal(Grave& grave) noexcept {
#if !defined(_WIN32)
                if (grave.bytes) (void)::mprotect(grave.lo, grave.bytes, PROT_READ | PROT_WRITE);
#endif
                grave.bytes = 0;
            }
        };
    }
#endif
    namespace detail {
        // A payload lineage's control block. Proxies (XRef) hold a pointer
        // to it and a copy of `base` and `version`:
        // one compare of the version tells them the payload is where they
        // last saw it. The version is even while the lineage lives and grows
        // by 2 when the payload moves; it is odd once the buffer is gone or
        // its content was replaced (reload, reset, clone_into), and a proxy
        // whose version is below `birth` belongs to an earlier lineage of the
        // same block. Blocks are never freed (a type-stable pool): a stale
        // proxy reads a valid block and a mismatching version.
        // The control block follows the content: an XBuffer move or swap
        // takes it along, so proxies stay valid across them.
        struct XCtl {
            std::uint64_t version = 1;
            char* base = nullptr;
            std::uint64_t birth = 0;
            XBufferCore* core = nullptr;
            std::uint32_t borrow = 0;      // open pin scopes (read/edit)
            XErrc sticky = XErrc::ok;      // the first failure (fail-soft)
            XCtl* next_free = nullptr;
        };

        class XCtlPool {
        public:
            // A control block for a new lineage: even version, birth = version.
            static XCtl* acquire() {
                auto& state = global();
                XCtl* ctl = nullptr;
                {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    if (!state.free) refill(state);
                    ctl = state.free;
                    if (ctl) state.free = ctl->next_free;
                }
                if (!ctl) return nullptr;
                ctl->next_free = nullptr;
                ctl->version = (ctl->version | 1u) + 1u;
                ctl->birth = ctl->version;
                ctl->borrow = 0;
                ctl->sticky = XErrc::ok;
                return ctl;
            }
            // The lineage ends: every proxy of it becomes stale.
            static void release(XCtl* ctl) noexcept {
                ctl->version |= 1u;
                ctl->base = nullptr;
                ctl->core = nullptr;
                auto& state = global();
                std::lock_guard<std::mutex> lock(state.mutex);
                ctl->next_free = state.free;
                state.free = ctl;
            }

        private:
            struct State {
                std::mutex mutex;
                XCtl* free = nullptr;
            };
            static State& global() noexcept {
                alignas(State) static unsigned char storage[sizeof(State)];
                static State* state = ::new (static_cast<void*>(storage)) State;
                return *state;
            }
            // 4 KiB of blocks at a time, never returned.
            static void refill(State& state) noexcept {
                constexpr std::size_t count = 4096 / sizeof(XCtl);
                auto* blocks = static_cast<XCtl*>(::operator new(count * sizeof(XCtl), std::nothrow));
                if (!blocks) return;
                for (std::size_t i = 0; i < count; ++i) {
                    ::new (static_cast<void*>(blocks + i)) XCtl;
                    blocks[i].next_free = state.free;
                    state.free = blocks + i;
                }
            }
        };

        // A delta sender's touch marks: XBuffer::touch and touch_set, and the
        // in-band allocator's own writes (xoffset_delta.hpp reads them). A
        // buffer keeps one (XBufferCore::touch_log_) once a delta from it
        // paired with a baseline. Touches are counted in periods: a delta
        // from the buffer ends one. The image ranges touched in the current
        // period are listed as runs, in touch order (a touch that starts
        // inside the last run or at its end extends it), up to run_capacity
        // of them. Per-block stamps (the period of a 64-byte block's last
        // touch) are kept only where a delta needs more than the list: once
        // a second baseline pairs (each finds what was touched since its own
        // last delta), and in a period whose touches overflow the list.
        struct DeltaTouchRun {
            std::uint32_t at;     // image offset
            std::uint32_t bytes;  // nonzero
        };
        struct alignas(64) DeltaTouchLog {
            static constexpr std::uint32_t run_capacity = 25;  // the fields below and the runs: four lines
            static constexpr std::size_t block = 64;            // the bytes one stamp stands for
            static constexpr std::size_t group = 16;            // blocks per entry of `groups`
            static constexpr std::uint32_t no_dir = 0xFFFFFFFFu;
            // Nonzero while the list holds every touch since the last delta
            // of the only baseline that pairs, an XDeltaRecord::touches one,
            // which keeps it (mixed with the buffer's epoch and its root
            // type): its next delta is the listed runs, found with one compare.
            std::uint64_t token = 0;
            std::uint32_t listed = 0;     // runs in `runs`
            std::uint32_t now = 1;        // the current period
            bool overflow = false;        // more runs in this period than the list holds: stamped
            bool shared = false;          // several baselines pair: every touch is stamped
            bool lost = false;            // a touch could not be recorded
            bool slow = false;            // overflow, shared or a copy to fetch: add takes add_slow
            // The free directory whose bytes (and the header's free_dir and
            // free_bytes) the allocator recorded in this period (no_dir: none).
            std::uint32_t dir_at = no_dir;
            // Where the copy of the only baseline that pairs is (0: none, or
            // several): a touched range's first line of it is fetched ahead
            // of the next delta (a hint: the address is never dereferenced).
            std::uintptr_t copy = 0;
            std::uint64_t epoch = 0;      // the buffer content (epoch) the marks describe
            std::uint64_t id = 0;         // changes when the log starts over
            const void* owner = nullptr;  // the baseline that paired last
            DeltaTouchRun runs[run_capacity];
            std::uint32_t stamps_from = 0xFFFFFFFFu;  // stamps hold every touch of the periods from this one
            std::vector<std::uint32_t> stamps;
            std::vector<std::uint32_t> groups;  // per `group` blocks: the latest of their stamps

            // Records a touch of image bytes [at, at + bytes) (bytes > 0).
            XOFFSET_DETAIL_INLINE void add(std::uint32_t at, std::uint32_t bytes) noexcept {
                const std::uint32_t n = listed;
                if (!slow && n < run_capacity) [[likely]] {
                    list(n, at, bytes);
                    return;
                }
                add_slow(at, bytes);
            }
            // Into the list of n < run_capacity runs: a touch that starts
            // inside the last run or at its end extends it.
            XOFFSET_DETAIL_INLINE void list(std::uint32_t n, std::uint32_t at, std::uint32_t bytes) noexcept {
                if (n != 0 && at - runs[n - 1].at <= runs[n - 1].bytes) {  // (at >= runs[n - 1].at)
                    DeltaTouchRun& last = runs[n - 1];
                    if (at + bytes - last.at > last.bytes) last.bytes = at + bytes - last.at;
                } else {
                    runs[n] = DeltaTouchRun{at, bytes};
                    listed = n + 1;
                }
            }
            // add when the list is full or overflowed, every touch is
            // stamped, or the copy of the only baseline that pairs is
            // fetched ahead.
            XOFFSET_DETAIL_NOINLINE void add_slow(std::uint32_t at, std::uint32_t bytes) noexcept {
                if (!overflow) {
                    if (copy) XOFFSET_DETAIL_PREFETCH_WITH_MSVC(reinterpret_cast<const char*>(copy + at), 1);
                    const std::uint32_t n = listed;
                    if (n < run_capacity || at - runs[n - 1].at <= runs[n - 1].bytes) {
                        list(n, at, bytes);
                    } else if (join()) {
                        list(listed, at, bytes);
                    } else {
                        overflow_list();  // stamps the list; this touch below
                    }
                }
                if (overflow || shared) stamp(at, bytes);
            }
            // The full list with each run that starts less than a block past
            // the end of the run kept before it (in touch order) joined to
            // it: the bytes between go with them (untouched, they are what
            // the receiver holds, and the stamps would send their blocks
            // whole). False when that leaves fewer than a quarter of the list
            // free (the list then overflows, so that joins stay rare).
            XOFFSET_DETAIL_NOINLINE bool join() noexcept {
                std::uint32_t w = 1;
                for (std::uint32_t i = 1; i < run_capacity; ++i) {
                    DeltaTouchRun& last = runs[w - 1];
                    const DeltaTouchRun r = runs[i];
                    if (r.at - last.at < std::uint64_t(last.bytes) + block) {  // (r.at >= last.at)
                        if (r.at + r.bytes - last.at > last.bytes) last.bytes = r.at + r.bytes - last.at;
                    } else {
                        runs[w++] = r;
                    }
                }
                listed = w;
                return w <= run_capacity - run_capacity / 4;
            }
            // slow after overflow, shared or copy changed.
            void settle() noexcept { slow = overflow || shared || copy != 0; }
            // The list holds no more: its runs go into the stamps, and so
            // does every later touch of the period.
            XOFFSET_DETAIL_NOINLINE void overflow_list() noexcept {
                overflow = slow = true;
                token = 0;
                stamp_list();
            }
            // The listed runs into the stamps: from now on this period's
            // touches are all stamped.
            void stamp_list() noexcept {
                for (std::uint32_t k = 0; k < listed; ++k) stamp(runs[k].at, runs[k].bytes);
                stamps_from = (std::min)(stamps_from, now);
            }
            XOFFSET_DETAIL_NOINLINE void stamp(std::uint32_t at, std::uint32_t bytes) noexcept {
                const std::size_t first = at / block, last = (std::size_t(at) + bytes - 1u) / block;
                if (!reserve(last)) return;
                for (std::size_t i = first; i <= last; ++i) stamps[i] = now;
                for (std::size_t g = first / group; g <= last / group; ++g) groups[g] = now;
            }
            // Room in the stamps for block `last` (and every block below
            // `used`); false (the log lost a touch) when there is none.
            bool reserve(std::size_t last, std::size_t used = 0) noexcept {
                if (stamps.size() > last && stamps.size() * block >= used) return true;
                XOFFSET_TRY {
                    const std::size_t blocks = (std::max)(last + 1u, (used + block - 1u) / block);
                    if (stamps.size() < blocks) {
                        stamps.resize(blocks, 0u);
                        groups.resize((blocks + group - 1u) / group, 0u);
                    }
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    lose();
                    return false;
                }
                return true;
            }
            // A touch could not be recorded: the next delta compares
            // everything (from a touches baseline: a key delta).
            void lose() noexcept {
                lost = true;
                token = 0;
            }
            // Starts over for buffer content `content_epoch`, as log `new_id`:
            // the baselines that paired before compare everything next.
            void restart(std::uint64_t content_epoch, std::uint64_t new_id) noexcept {
                token = 0;
                listed = 0;
                now = 1;
                overflow = shared = lost = slow = false;
                dir_at = no_dir;
                copy = 0;
                epoch = content_epoch;
                id = new_id;
                owner = nullptr;
                stamps_from = 0xFFFFFFFFu;
                stamps.clear();  // regrown with zeros when stamps are needed
                groups.clear();
            }
            // A delta from the buffer ends the period: the next one lists
            // what is touched from now on.
            void next_period() noexcept {
                listed = 0;
                overflow = false;
                dir_at = no_dir;
                settle();
            }
            // next_period while the log's token is set: the list held every
            // touch, alone (no overflow, not shared, no copy: slow is off).
            void next_listed_period() noexcept {
                listed = 0;
                dir_at = no_dir;
            }
        };
    }

    class XBufferCore {
    public:
        using size_type = std::size_t;

        // The image header (wire 3.0): the first 40 bytes of the payload,
        // the same in memory and on the wire (docs/WIRE_FORMAT.md). The root
        // object follows at offset 40. Blocks carry no headers: every block
        // is 8-byte aligned with a length that is a multiple of 8, and its
        // extent is derived from the owning head (see
        // XBufferCore::allocate_block).
        struct alignas(8) arena_header {
            std::uint64_t magic = 0;          // image_magic: 'X' 'O' major minor, then the header size (40)
            std::uint64_t schema_hash = 0;    // fingerprint of the root type (0 while none is bound)
            std::uint64_t root_type_id = 0;
            std::uint32_t used_end = 0;       // bytes of the image, this header included
            std::uint32_t extent_count = 0;   // 0: an image is dense (a sparse wire's extent count)
            std::uint32_t free_dir = 0;       // in-band allocator: the directory's offset (0: no free block)
            std::uint32_t free_bytes = 0;     // bytes of the free blocks, the directory not counted

            void* allocate_aligned(std::uint32_t bytes, std::size_t alignment) {
                return owner().allocate_block(block_size(bytes), alignment);
            }

            // Internal allocator operation: ptr must identify a live block of
            // old_bytes from this arena. Only the tail block can grow, and only
            // within the configured capacity. Failure leaves it unchanged.
            bool try_expand(void* ptr, std::uint32_t old_bytes, std::uint32_t new_bytes) {
                return owner().try_expand_block(ptr, block_size(old_bytes), block_size(new_bytes));
            }

            // ptr must identify a live block of exactly `bytes` (as allocated).
            void deallocate(void* ptr, std::uint32_t bytes) noexcept {
                if (auto* core = XBufferCore::owner_from_arena_noexcept(this))
                    core->retire_block(ptr, block_size(bytes));
            }

            static std::uint32_t block_size(std::uint32_t bytes) noexcept {
                return bytes == 0 ? 8u : (bytes + 7u) & ~7u;
            }

        private:
            XBufferCore& owner() {
                return XBufferCore::owner_from_arena(this);
            }
        };

        XBufferCore() = default;
        ~XBufferCore() {
            retire_ctl();
            unregister_registry();
            // The storage is released next (back to its slot or pool, or
            // unmapped). On a settled owner only [0, tail_clean_from()) was
            // ever dirtied (N2), so the next owner of these bytes clears only
            // that much instead of the whole logical length.
            if (const ZeroTail tail = settled_zero_tail(); tail.base)
                buffer_.note_clean_tail_for_release(tail.used_end);
        }

        XBufferCore(const XBufferCore&) = delete;
        XBufferCore& operator=(const XBufferCore&) = delete;

        XBufferCore(XBufferCore&& other) noexcept {
            move_from(std::move(other));
        }

        XBufferCore& operator=(XBufferCore&& other) noexcept {
            if (this != &other) move_from(std::move(other));
            return *this;
        }

        static constexpr std::size_t GROWTH_HEADROOM = 16;
        static constexpr std::size_t MIN_RESERVE = 64ULL * 1024;
        static constexpr std::size_t MAX_RESERVE = 256ULL * 1024 * 1024;
        // The largest payload and maximum capacity: offsets and relative
        // heads inside a payload are 32-bit. Creating or loading a buffer
        // with a larger maximum fails at once (capacity_exceeded).
        static constexpr std::size_t MAX_PAYLOAD = 0x7ffffff8u;

#ifdef XOFFSET_DEFAULT_MAXIMUM_CAPACITY
        static constexpr std::size_t DEFAULT_MAXIMUM_CAPACITY = XOFFSET_DEFAULT_MAXIMUM_CAPACITY;
#else
        static constexpr std::size_t DEFAULT_MAXIMUM_CAPACITY = 64ULL * 1024 * 1024;
#endif
        static_assert(DEFAULT_MAXIMUM_CAPACITY >= sizeof(arena_header) &&
                      DEFAULT_MAXIMUM_CAPACITY <= MAX_PAYLOAD);

        struct MaximumCapacity {
            size_type value;
            XGrowPolicy growth = XGrowPolicy::Explicit;
            XBufferPool pool;
            XStorage storage = XStorage::Auto;
            // Relocating only: the first reservation in bytes, the 64-byte
            // host header included (0: sized from the first content, see
            // relocating_first_reserve). A type that lives in tens of
            // thousands of small buffers can start at a few KiB (a power of
            // two up to 1 MiB is a slot of that size); it moves to larger
            // slots as it grows.
            size_type reserve = 0;

            MaximumCapacity with_growth(XGrowPolicy policy) const noexcept {
                auto result = *this;
                result.growth = policy;
                return result;
            }
            MaximumCapacity with_pool(const XBufferPool& shared_pool) const noexcept {
                auto result = *this;
                result.pool = shared_pool;
                return result;
            }
            MaximumCapacity with_storage(XStorage request) const noexcept {
                auto result = *this;
                result.storage = request;
                return result;
            }
            MaximumCapacity with_reserve(size_type bytes) const noexcept {
                auto result = *this;
                result.reserve = bytes;
                return result;
            }
        };
        // Largest payload a slot holds (0: slots disabled at build time).
        static constexpr size_type SLOT_MAX = detail::SlotGeometry::max_payload;
        static MaximumCapacity maximum_capacity(size_type bytes) noexcept { return {bytes}; }
        size_type maximum_capacity() const noexcept { return buffer_.maximum_capacity(); }
        // The largest default first reservation of a relocating buffer: a
        // 1 MiB slot's payload (XOFFSET_RELOCATING_RESERVE_BYTES).
        static constexpr size_type RELOCATING_RESERVE = detail::SlotGeometry::relocating_payload;
        // The smallest: a 64 KiB slot's payload (or RELOCATING_RESERVE when
        // that is smaller).
        static constexpr size_type RELOCATING_RESERVE_MIN =
            detail::SlotGeometry::relocating_slot_bytes < (size_type(64) << 10)
                ? RELOCATING_RESERVE
                : (size_type(64) << 10) - detail::SlotGeometry::header_bytes;
        // A relocating buffer's first reservation (payload bytes) when it
        // starts with `bytes` of content (create's initial size, or the
        // payload it loads) and has no with_reserve: a slot of twice the
        // content rounded up to a power of two, from 64 KiB to 1 MiB, less
        // its 64-byte header. Small buffers take few page-table pages;
        // moving a small payload is cheap, and doubling keeps moves rare.
        static constexpr size_type relocating_first_reserve(size_type bytes) noexcept {
            constexpr size_type largest = detail::SlotGeometry::relocating_slot_bytes;
            if (bytes >= largest / 2) return RELOCATING_RESERVE;
            const size_type slot = std::bit_ceil(std::max<size_type>(2 * bytes, size_type(64) << 10));
            return slot >= largest ? RELOCATING_RESERVE : slot - detail::SlotGeometry::header_bytes;
        }
        // Where a relocating buffer moves once its first reservation is full:
        // 0 means a reservation of the whole maximum (one move); otherwise
        // that many bytes, then doubling (XOFFSET_RELOCATE_STEP_BYTES).
        static constexpr size_type RELOCATE_STEP = XOFFSET_RELOCATE_STEP_BYTES;
        // A relocating buffer that may grow to `maximum` (default: the
        // format limit, 2 GiB - 8).
        static MaximumCapacity relocating(size_type maximum = MAX_PAYLOAD) noexcept {
            return MaximumCapacity{maximum, XGrowPolicy::Relocating, {}, XStorage::Auto, 0};
        }
        XGrowPolicy growth_policy() const noexcept { return packed_.growth_policy; }
        // Automatic growth runs inside container operations, where the payload
        // must not move: a movable slot switched to Automatic first moves to a
        // VM reservation (pinned from then on). That move invalidates pointers
        // like grow(); it throws XBadAlloc on failure and changes nothing.
        //
        // Relocating needs storage without a pool. Leaving Relocating for
        // Automatic moves a buffer whose reservation is smaller than its
        // maximum to a reservation of the whole maximum, as above.
        void set_growth_policy(XGrowPolicy policy) {
            if (policy == XGrowPolicy::Relocating && buffer_.pool().enabled())
                XOFFSET_THROW(XException(XErrc::invalid_argument,
                    "XGrowPolicy::Relocating needs storage without a pool"));
            if (policy == XGrowPolicy::Automatic &&
                (buffer_.movable() || buffer_.reservation_limit() < buffer_.maximum_capacity())) {
                const auto previous = packed_.growth_policy;
                packed_.growth_policy = XGrowPolicy::Automatic;
                if (!replace_storage(buffer_.size(), true, true)) {
                    packed_.growth_policy = previous;
                    XOFFSET_THROW(XBadAlloc());
                }
            }
            packed_.growth_policy = policy;
        }
        XStorageKind storage_kind() const noexcept {
            if (!buffer_.is_slot()) return XStorageKind::Vm;
            return buffer_.movable() ? XStorageKind::MovableSlot : XStorageKind::PinnedSlot;
        }
        // False only on a movable slot, whose payload address can change.
        bool address_stable() const noexcept {
            return !buffer_.movable() && packed_.growth_policy != XGrowPolicy::Relocating;
        }
        // The largest payload this buffer reaches without moving (a
        // relocating buffer's current reservation).
        size_type reservation_limit() const noexcept { return buffer_.reservation_limit(); }
        // Discards the pages of free slots (see xoffset_slot_storage.hpp).
        static void trim_slot_storage() noexcept { detail::SlotSlab::trim(); }
        static XSlotStorageStats slot_storage_stats() noexcept { return detail::SlotSlab::stats(); }
        detail::VmReclaimResult reclaim_tail_pages() noexcept { return buffer_.reclaim_tail(); }

        // Numeric reservation parameters are legacy minimum hints, not hard limits.
        static size_type capacity_from_hint(size_type size, size_type hint = 0) {
            if (size > MAX_PAYLOAD || hint > MAX_PAYLOAD)
                XOFFSET_THROW(XBadAlloc(XErrc::capacity_exceeded, detail::payload_limit_message));
            return std::max({size, hint, DEFAULT_MAXIMUM_CAPACITY});
        }

        explicit XBufferCore(size_type size) {
            XOFFSET_VREQUIRE(init_fresh(size, capacity_from_hint(size)));
        }

        XBufferCore(size_type size, size_type max_reserved) {
            XOFFSET_VREQUIRE(init_fresh(size, capacity_from_hint(size, max_reserved)));
        }

        XBufferCore(size_type size, MaximumCapacity cap) : packed_{.growth_policy = cap.growth} {
            XOFFSET_VREQUIRE(init_fresh(size, cap.value, cap.pool, cap.storage, cap.reserve));
        }

        XBufferCore(const char* data, size_type size) {
            XOFFSET_VREQUIRE(init_from_bytes(data, size, capacity_from_hint(size)));
        }

        XBufferCore(const char* data, size_type size, size_type max_reserved) {
            XOFFSET_VREQUIRE(init_from_bytes(data, size, capacity_from_hint(size, max_reserved)));
        }

        XBufferCore(const char* data, size_type size, MaximumCapacity cap) : packed_{.growth_policy = cap.growth} {
            XOFFSET_VREQUIRE(init_from_bytes(data, size, cap.value, cap.pool, cap.storage, cap.reserve));
        }

        // The image of an admitted wire: dense (extent_count 0), `stored`
        // is the image itself, header included; sparse, `stored` is the
        // 40-byte header, `extents` the extent table, and the extents' bytes
        // follow the table (the gaps load as zero). The copied header's
        // extent count is cleared (an image is dense).
        struct WirePayload {
            const char* stored;
            const void* extents;
            std::size_t extent_count;
        };
        XBufferCore(WirePayload wire, size_type size, MaximumCapacity cap) : packed_{.growth_policy = cap.growth} {
            XOFFSET_VREQUIRE(init_from_wire(wire, size, cap.value, cap.pool, cap.storage, cap.reserve));
        }

        uint64_t epoch() const noexcept { return m_epoch; }

        bool grow(size_type extra_bytes) {
            if (extra_bytes == 0) return true;
            if (buffer_.empty() || extra_bytes > buffer_.maximum_capacity() - buffer_.size()) return false;
            // The allocator reads the storage's logical extent directly. No segment
            // reopen, offset rewrite, or potentially throwing allocator update follows.
            const auto target = buffer_.size() + extra_bytes;
            if (buffer_.grow_to(target)) return true;
            // A movable slot or a relocating buffer that is too small moves to
            // a larger owner: every pointer into the payload is invalidated
            // (epoch), XHandles re-resolve.
            return (buffer_.movable() || packed_.growth_policy == XGrowPolicy::Relocating) &&
                target > buffer_.reservation_limit() && replace_storage(target, true);
        }

        // Makes room for allocations of `bytes` in total (blocks of a
        // multiple of 8 bytes, alignment at most 8; add alignment - 8 per
        // block above that): when it returns XErrc::ok, they succeed without
        // the payload growing or moving. Grows the payload in place where
        // the reservation allows (Automatic, Relocating; Explicit up to its
        // storage), otherwise moves a relocating buffer or a movable slot to
        // a larger reservation, which invalidates every pointer and reference
        // into the payload (epoch). Never throws and never aborts: the
        // result is capacity_exceeded past the maximum, out_of_storage when
        // the OS gives no memory or address space, invalid_state without a
        // payload. On failure nothing changed.
        // True when `bytes` more fit in the committed payload: an allocation
        // of at most that much needs no growth and no move (proxies run such
        // operations inline).
        XOFFSET_DETAIL_INLINE bool has_room(size_type bytes) const noexcept {
            const size_type used = used_end_clamped();
            return used <= buffer_.size() && bytes <= buffer_.size() - used;
        }

        // Room for `bytes` more without moving the payload (growing it in
        // place inside the reservation when the policy allows): ok, or why
        // not. A recorded failure (error()) refuses at once. The fail-soft
        // preflight of builds without exceptions uses it.
        XErrc reserve_in_place(size_type bytes) noexcept {
            if (ctl_ && ctl_->sticky != XErrc::ok) return ctl_->sticky;
            if (buffer_.empty()) return XErrc::invalid_state;
            const size_type used = used_end_clamped();
            if (bytes > MAX_PAYLOAD - used) return XErrc::capacity_exceeded;
            const size_type need = used + bytes;
            if (need <= buffer_.size()) return XErrc::ok;
            if (need > buffer_.maximum_capacity()) return XErrc::capacity_exceeded;
            if (packed_.growth_policy == XGrowPolicy::Explicit) return XErrc::out_of_storage;
            if (need > buffer_.reservation_limit()) return packed_.pinned ? XErrc::borrowed : XErrc::out_of_storage;
            // ensure_allocation_extent's growth, without its throw.
            const size_type maximum = packed_.growth_policy == XGrowPolicy::Automatic
                ? buffer_.maximum_capacity() : buffer_.reservation_limit();
            if (buffer_.grow_to(extent_target(need, maximum)) || buffer_.grow_to(need)) return XErrc::ok;
            return XErrc::out_of_storage;
        }

        XErrc make_room(size_type bytes) noexcept {
            if (buffer_.empty()) [[unlikely]] return XErrc::invalid_state;
            const size_type used = used_end_clamped();
            if (bytes > MAX_PAYLOAD - used) return XErrc::capacity_exceeded;
            const size_type need = used + bytes;
            if (need <= buffer_.size()) [[likely]] return XErrc::ok;
            return make_room_slow(need);
        }

        // ---- control block, sticky error, pin scopes.

        // This lineage's control block (made on first use; null when no
        // block can be allocated).
        detail::XCtl* control() noexcept {
            if (!ctl_) [[unlikely]] make_ctl();
            return ctl_;
        }
        // The first failure a proxy or a pin scope recorded (fail-soft:
        // nothing throws or aborts). A failed buffer refuses save_verified
        // and the other emission entries until clear_error().
        XErrc error() const noexcept { return ctl_ ? ctl_->sticky : XErrc::ok; }
        bool ok() const noexcept { return error() == XErrc::ok; }
        void clear_error() noexcept { if (ctl_) ctl_->sticky = XErrc::ok; }
        // Records `code` unless an earlier failure is recorded.
        void record_error(XErrc code) noexcept {
            if (code == XErrc::ok) return;
            if (auto* ctl = control(); ctl && ctl->sticky == XErrc::ok) {
                ctl->sticky = code;
                failure_recorded();
            }
        }
        // A failure was just recorded: the touch log's token stands for a
        // buffer that emits (xoffset_delta.hpp's short path compares only
        // it), so it is dropped; the general path refuses the delta, and
        // after clear_error() pairs again.
        void failure_recorded() noexcept {
            if (touch_log_) [[unlikely]] touch_log_->token = 0;
        }
        // An open pin scope: the payload does not move, retired blocks wait.
        bool pinned() const noexcept { return packed_.pinned; }

        // A buffer with a recorded failure emits nothing: `out` is cleared
        // and, with exceptions, XException(error()) is thrown; without them
        // the entry returns (true here) and the caller sees an empty wire.
        XOFFSET_DETAIL_INLINE bool refuse_failed_emit(std::string& out) {
            if (!emit_failed()) [[likely]] return false;
            return refuse_failed_emit_slow(out);
        }
        XOFFSET_DETAIL_INLINE bool emit_failed() const noexcept { return ctl_ && ctl_->sticky != XErrc::ok; }
        XOFFSET_DETAIL_NOINLINE bool refuse_failed_emit_slow(std::string& out) {
            out.clear();
#if XOFFSET_NO_EXCEPTIONS
            return true;
#else
            throw XException(ctl_->sticky,
                std::string("the buffer has a recorded failure (") + to_string(ctl_->sticky) +
                "); it emits no wire until clear_error()");
#endif
        }

        // Pin scope bracket (XBuffer::read / edit use it; nests). Entering
        // fails only when no control block can be made.
        bool pin_enter() noexcept {
            auto* ctl = control();
            if (!ctl) return false;
            ++ctl->borrow;
            packed_.pinned = true;
            return true;
        }
        // Bare pin scopes (XBuffer::edit, build and read while the lineage
        // has no block): the pin flag is the whole scope. A block made while
        // one is open (a recorded failure, a nested scope, a move of the
        // content) counts it and is noted in handoff_; the close then exits
        // that block (pin_close_bare_slow).
        //
        // A bare scope may open: no scope is open, the lineage has no block
        // (so no recorded failure) and no handoff, and no block waits in the
        // quarantine. Packed's bytes 1 to 3 are zero.
        XOFFSET_DETAIL_INLINE bool pin_bare_free() const noexcept { return packed_low_word() < 0x100u; }
        // edit's fast entry: a bare scope may open, the growth policy is not
        // Relocating (whose reservation edit may move first), and the
        // payload stays put while the scope is open: address-stable storage,
        // or a movable slot at most half used (buffer_.stay_end()). One
        // compare on Packed's low word (policy byte 0 or 1, bytes 1 to 3
        // zero) and one on used_end. Only for an owner with an arena (one
        // whose root key is set).
        XOFFSET_DETAIL_INLINE bool edit_quiet() const noexcept {
            static_assert(static_cast<int>(XGrowPolicy::Explicit) < 2 &&
                          static_cast<int>(XGrowPolicy::Automatic) < 2 &&
                          static_cast<int>(XGrowPolicy::Relocating) == 2);
            return packed_low_word() < 2u && arena_unchecked()->used_end <= buffer_.stay_end();
        }
        void pin_open_bare() noexcept { packed_.pinned = true; }
        // Closes a bare scope. False: a failure was recorded inside it.
        XOFFSET_DETAIL_INLINE bool pin_close_bare() noexcept {
            if (scope_marks() != 0) [[unlikely]] return pin_close_bare_slow();
            packed_.pinned = false;
#if defined(XOFFSET_STRESS_RELOCATE)
            stress_relocate();
#endif
            return true;
        }
        // Closes a scope opened by pin_enter on this lineage. `ctl` is the
        // block pin_enter used (the XBuffer may have moved meanwhile: the
        // block follows the content to its current owner).
        static void pin_exit(detail::XCtl* ctl) noexcept {
            if (!ctl || ctl->borrow == 0) return;
            if (--ctl->borrow != 0) return;
            if (auto* core = ctl->core) {
                core->packed_.pinned = false;
                core->release_quarantine();
#if defined(XOFFSET_STRESS_RELOCATE)
                core->stress_relocate();
#endif
            }
        }

    private:
        // A bare scope's close with something to do: a block took the scope
        // over (handoff_: it counts the scope, wherever the content went),
        // or blocks wait in the quarantine.
        XOFFSET_DETAIL_NOINLINE bool pin_close_bare_slow() noexcept {
            if (detail::XCtl* const taken = handoff_) {
                handoff_ = nullptr;
                sync_blocked();
                const bool ok = taken->sticky == XErrc::ok;
                pin_exit(taken);
                return ok;
            }
            packed_.pinned = false;
            release_quarantine();
#if defined(XOFFSET_STRESS_RELOCATE)
            stress_relocate();
#endif
            return true;
        }
        void make_ctl() noexcept {
            XOFFSET_TRY {
                ctl_ = detail::XCtlPool::acquire();
            } XOFFSET_CATCH(...) {
                ctl_ = nullptr;
            }
            if (ctl_) {
                ctl_->core = this;
                ctl_->base = buffer_.data();
                if (packed_.pinned) [[unlikely]] {
                    // Pinned without a block: a bare scope is open on this
                    // owner. The block counts it, and the scope closes on it.
                    ctl_->borrow = 1;
                    handoff_ = ctl_;
                }
            }
            sync_blocked();
        }
        void sync_blocked() noexcept { packed_.blocked = ctl_ != nullptr || handoff_ != nullptr; }
        // packed_ as one integer (see Packed). 0 is the default word.
        std::uint64_t packed_bits() const noexcept {
            std::uint64_t bits;
            std::memcpy(&bits, &packed_, sizeof(bits));
            return bits;
        }
        void set_packed_bits(std::uint64_t bits) noexcept { std::memcpy(&packed_, &bits, sizeof(bits)); }
        // Packed's first four bytes (policy, pinned, blocked, quarantined)
        // as one little-endian word, and its blocked and quarantined bytes.
        std::uint32_t packed_low_word() const noexcept {
            std::uint32_t word;
            std::memcpy(&word, &packed_, sizeof(word));
            return word;
        }
        std::uint16_t scope_marks() const noexcept {
            std::uint16_t marks;
            std::memcpy(&marks, reinterpret_cast<const unsigned char*>(&packed_) + offsetof(Packed, blocked), sizeof(marks));
            return marks;
        }
        // A bare scope that the content may leave (move, swap) gets its
        // block first, so the scope follows the content as a counted one.
        void promote_bare_pin() noexcept {
            if (packed_.pinned && !ctl_) [[unlikely]] make_ctl();
        }
        // After ctl_ changed hands: point it at this owner, mirror its pins.
        void adopt_ctl() noexcept {
            if (ctl_) {
                ctl_->core = this;
                ctl_->base = buffer_.data();
            }
            packed_.pinned = ctl_ && ctl_->borrow != 0;
            sync_blocked();
        }
        void retire_ctl() noexcept {
            if (!ctl_) return;
            detail::XCtlPool::release(ctl_);
            ctl_ = nullptr;
            packed_.pinned = false;
            quarantine_.clear();
            packed_.quarantined = 0;
            sync_blocked();
        }
        // The content is replaced (reload, reset, clone_into): proxies of the
        // old content become stale. An open pin scope keeps its block (the
        // scope closes on it), which starts a new lineage in place.
        XOFFSET_DETAIL_NOINLINE void end_lineage() noexcept {
            quarantine_.clear();
            packed_.quarantined = 0;
            ctl_->version += 2;
            ctl_->birth = ctl_->version;
            ctl_->sticky = XErrc::ok;
        }
        // retire_block inside a pin scope: remember the block (host memory;
        // when that fails the block is simply never reused).
        XOFFSET_DETAIL_NOINLINE void quarantine_block(void* ptr, std::uint32_t bytes) noexcept {
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (!ptr || bytes == 0 || address < base || address - base >= buffer_.size()) return;
            XOFFSET_TRY {
                quarantine_.push_back((std::uint64_t(address - base) << 32) | bytes);
                packed_.quarantined = 1;
            } XOFFSET_CATCH(...) {
            }
        }
        void release_quarantine() noexcept {
            if (!quarantine_.empty()) [[unlikely]] release_quarantine_slow();
        }
        XOFFSET_DETAIL_NOINLINE void release_quarantine_slow() noexcept {
            auto blocks = std::move(quarantine_);
            quarantine_.clear();
            packed_.quarantined = 0;
            for (const auto entry : blocks) {
                const auto offset = static_cast<std::uint32_t>(entry >> 32);
                const auto bytes = static_cast<std::uint32_t>(entry);
                if (offset < buffer_.size() && bytes <= buffer_.size() - offset)
                    retire_block(buffer_.data() + offset, bytes);
            }
            // Hand the list's storage back for the next scope: a loop of
            // scopes that each retire a few blocks then allocates no host
            // memory per scope. A list a large scope grew is released.
            if (blocks.capacity() <= quarantine_keep_max && quarantine_.capacity() == 0) {
                blocks.clear();
                quarantine_.swap(blocks);
            }
        }
#if defined(XOFFSET_STRESS_RELOCATE)
        void stress_poison_block(void* ptr, std::uint32_t bytes) noexcept {
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (!ptr || bytes == 0 || address < base || address - base >= buffer_.size() ||
                bytes > buffer_.size() - (address - base)) return;
            std::memset(ptr, detail::stress_poison_byte, bytes);
        }
        // Stress mode: every pin scope's end moves the payload, so a plain
        // reference kept past the scope points at released storage.
        void stress_relocate() noexcept {
            if (packed_.growth_policy != XGrowPolicy::Relocating || buffer_.empty()) return;
            (void)replace_storage(buffer_.size(), true);
        }
#endif

        XOFFSET_DETAIL_NOINLINE XErrc make_room_slow(size_type need) noexcept {
            if (need > buffer_.maximum_capacity()) return XErrc::capacity_exceeded;
            const size_type limit = buffer_.reservation_limit();
            if (need <= limit) {
                const size_type target = packed_.growth_policy == XGrowPolicy::Explicit
                    ? need : extent_target(need, limit);
                return buffer_.grow_to(target) || buffer_.grow_to(need) ? XErrc::ok : XErrc::out_of_storage;
            }
            if (!buffer_.movable() && packed_.growth_policy != XGrowPolicy::Relocating) return XErrc::out_of_storage;
            if (packed_.pinned) return XErrc::borrowed;
            return replace_storage(need, true) ? XErrc::ok : XErrc::out_of_storage;
        }

    public:
        void shrink_to_fit() {
            if (buffer_.empty()) return;
            auto used = used_end_clamped();
            if (used < buffer_.size()) {
                buffer_.resize_logical(used);
            }
        }

        const void* get_address() const { return buffer_.data(); }
        char* data() noexcept { return buffer_.data(); }
        const char* data() const noexcept { return buffer_.data(); }
        std::span<char> get_buffer() noexcept { return {buffer_.data(), buffer_.size()}; }
        std::span<const char> get_buffer() const noexcept { return {buffer_.data(), buffer_.size()}; }
        std::string copy_bytes() const {
            return buffer_.empty() ? std::string{} : std::string(buffer_.data(), buffer_.size());
        }
        size_type get_size() const { return buffer_.size(); }
        size_type reserved_capacity() const { return buffer_.capacity(); }
        size_type reserved_bytes() const noexcept { return buffer_.reserved_bytes(); }
        size_type accessible_bytes() const noexcept { return buffer_.accessible_bytes(); }

        void swap(XBufferCore& other) noexcept {
            if (this == &other) return;
            promote_bare_pin();
            other.promote_bare_pin();
            std::swap(ctl_, other.ctl_);
            // Policy and reset mark follow the content, as one word; the pin
            // flag in it is set again by adopt_ctl below.
            {
                const auto mine = packed_bits();
                set_packed_bits(other.packed_bits());
                other.set_packed_bits(mine);
            }
            quarantine_.swap(other.quarantine_);
            buffer_.swap(other.buffer_);
            // A touch log follows the content (its epoch no longer matches,
            // so it starts over before its next use).
            touch_log_.swap(other.touch_log_);
            // After the payloads swapped: each block names its content's base.
            adopt_ctl();
            other.adopt_ctl();
            owner_record_.swap(other.owner_record_);
            std::swap(free_table_lost_, other.free_table_lost_);
            publish_host_owner();
            other.publish_host_owner();
            std::swap(root_key_, other.root_key_);
            std::swap(typed_root_access_allowed_, other.typed_root_access_allowed_);
            advance_epoch();
            other.advance_epoch();
            // No record is written: a record follows its payload, so after the
            // two swaps above each record is again its payload's, and the
            // owner it names is read from the host header (record_owner). A
            // record lives on the heap and is touched by nothing else on the
            // hot paths, so under memory pressure it is cold; writing it here
            // would cost a page fault per buffer.
        }

        friend void swap(XBufferCore& a, XBufferCore& b) noexcept { a.swap(b); }

        size_type get_free_memory() const {
            if (buffer_.empty()) return 0;
            size_type used = used_end_clamped();
            size_type tail = buffer_.size() > used ? buffer_.size() - used : 0;
            return tail + static_cast<size_type>(inband_free_bytes());
        }

        arena_header* arena() noexcept {
            if (buffer_.size() < sizeof(arena_header)) return nullptr;
            return std::launder(reinterpret_cast<arena_header*>(buffer_.data()));
        }

        const arena_header* arena() const noexcept {
            if (buffer_.size() < sizeof(arena_header)) return nullptr;
            return std::launder(reinterpret_cast<const arena_header*>(buffer_.data()));
        }

        // The header of an owner known to hold an arena (its root key is set).
        const arena_header* arena_unchecked() const noexcept {
            return std::launder(reinterpret_cast<const arena_header*>(buffer_.data()));
        }
        arena_header* arena_unchecked() noexcept {
            return std::launder(reinterpret_cast<arena_header*>(buffer_.data()));
        }

        // The root object is the image's first block, at root_at: there is
        // one when the image holds a block of T's size past the header.
        template <typename T>
        T* find_root_storage() noexcept {
            const auto* sm = arena();
            return sm && sm->used_end >= root_at + sizeof(T) ? object_at_offset<T>(root_at) : nullptr;
        }

        // The root object of an image known to hold one (used_end >= root_at
        // + sizeof(T), checked by the caller): at root_at.
        template <typename T>
        XOFFSET_DETAIL_INLINE T* root_storage_unchecked() noexcept {
            static_assert(alignof(T) <= 8);
            return std::launder(reinterpret_cast<T*>(buffer_.data() + root_at));
        }

        template <typename T>
        const T* find_root_storage() const noexcept {
            const auto* sm = arena();
            return sm && sm->used_end >= root_at + sizeof(T) ? object_at_offset_const<T>(root_at) : nullptr;
        }

        // BindType false: the caller keeps the root key, which already
        // names the root (XBuffer::reset<T>()'s same-type path).
        template <typename T, bool BindType = true, typename... Args>
        XOFFSET_DETAIL_INLINE T* construct_root_storage(Args&&... args) {
            T* result = construct_root_object<T>(std::forward<Args>(args)...);
            if constexpr (BindType) bind_root<T>();
            return result;
        }

        // A root type's key: one static per type, so its address names the
        // type (writable, so that no linker folds two of them into one).
        // `named`: the type has a schema name, so the image header holds its
        // fingerprint.
        struct RootKey {
            bool named;
        };
        template <typename T>
        static const RootKey* root_key_of() noexcept {
            return root_key_for<std::remove_cv_t<T>>();
        }
        template <typename T>
        static const RootKey* root_key_for() noexcept {
            static RootKey key{has_schema_name_v<T>};
            return &key;
        }

        // root_key_ (see there) is T's.
        template <typename T>
        bool root_is() const noexcept { return root_key_ == root_key_of<T>(); }
        bool root_bound() const noexcept { return root_key_ != nullptr; }
        // The bound root type has a schema name.
        bool root_named() const noexcept { return root_key_ && root_key_->named; }
        template <typename T>
        void bind_root() noexcept { root_key_ = root_key_of<T>(); }

    public:
        // A free range of the image: a free block or the free directory.
        struct WireHole {
            std::uint32_t offset = 0;
            std::uint32_t bytes = 0;
        };
        static_assert(sizeof(WireHole) == 8);

        // The image header's layout (docs/WIRE_FORMAT.md). image_magic is
        // 'X' 'O' 3 0 (the format version) followed by the header size.
        static_assert(sizeof(arena_header) == 40 && offsetof(arena_header, schema_hash) == 8 &&
                      offsetof(arena_header, root_type_id) == 16 && offsetof(arena_header, used_end) == 24 &&
                      offsetof(arena_header, extent_count) == 28 && offsetof(arena_header, free_dir) == 32 &&
                      offsetof(arena_header, free_bytes) == 36);
        static constexpr std::uint8_t image_format_major = 3;
        static constexpr std::uint8_t image_format_minor = 0;
        static constexpr std::uint64_t image_magic = 0x4F58u | (std::uint64_t(image_format_major) << 16) |
                                                     (std::uint64_t(image_format_minor) << 24) |
                                                     (std::uint64_t(sizeof(arena_header)) << 32);
        // The root object's offset: right past the header.
        static constexpr std::uint32_t root_at = sizeof(arena_header);

        // The header of an image of `used` bytes whose payload is written in
        // place (a key delta): no free block until the caller records them.
        static void write_image_header(char* base, std::uint64_t used, std::uint64_t schema_hash,
                                       std::uint64_t root_type_id) noexcept {
            arena_header header{};
            header.magic = image_magic;
            header.schema_hash = schema_hash;
            header.root_type_id = root_type_id;
            header.used_end = static_cast<std::uint32_t>(used);
            std::memcpy(base, &header, sizeof(header));
        }

        // Host owner of a payload base (the arena header address), kept
        // in the host header just before the payload (a slot's, or a VM
        // region's first 64 bytes, on the payload's first page); no registry
        // or TLS lookup.
        static XBufferCore& owner_from_arena(const void* arena) {
            auto* owner = owner_from_arena_noexcept(arena);
            if (!owner) XOFFSET_THROW(XException(XErrc::invalid_state, "arena owner lookup failed"));
            return *owner;
        }
        static XBufferCore* owner_from_arena_noexcept(const void* arena) noexcept {
            if (!arena) return nullptr;
            return *reinterpret_cast<XBufferCore* const*>(
                static_cast<const char*>(arena) - sizeof(void*));
        }

        // ---- Allocator --------------------------------------------------
        // `bytes` is a block size: a nonzero multiple of 8. The state is in
        // the payload (the in-band allocator below). A block is never handed
        // out larger than requested, so every live extent equals the size its
        // owner derives from its head and passes back to retire/try_expand.
        //
        // A small request at an alignment of at most 8 is decided inline
        // (try_allocate_small: a free block of the exact size, or a bump
        // inside the current extent when no free block is large enough);
        // every other request takes the general path, whose placement is the
        // same (exact size, smallest larger block split, bump).
        XOFFSET_DETAIL_INLINE void* allocate_block(std::uint32_t bytes, std::size_t alignment,
                                                        bool allow_growth = true) {
            if (bytes != 0 && (bytes & 7u) == 0 && bytes <= small_block_max &&
                alignment - 1u < 8u && (alignment & (alignment - 1u)) == 0) [[likely]] {
                if (void* block = try_allocate_small(bytes)) [[likely]] return block;
            }
            return allocate_block_general(bytes, alignment, allow_growth);
        }

        // The largest block size allocate_block and retire_block decide inline.
        static constexpr std::uint32_t small_block_max = 4096;

        // allocate_block's two inline outcomes for `bytes`, a nonzero multiple
        // of 8 up to small_block_max, at an alignment of at most 8: the block,
        // or null where allocate_block takes the general path.
        XOFFSET_DETAIL_INLINE void* try_allocate_small(std::uint32_t bytes) noexcept {
            return inband_allocate_small(bytes);
        }

        // allocate_block for a string's first block (fixed_detail::
        // allocate_first_chars): `bytes` is block_bytes_of(n) of a nonzero n,
        // so a nonzero multiple of 8, and the alignment is 1. With no hole
        // recorded (each message built anew after a reset) this is
        // allocate_block's bump without the request checks the contract
        // settles; anything else returns null and the caller takes
        // allocate_block unchanged. Same placement either way. The caller
        // found the payload (the string's own), so the buffer is not empty,
        // and used_end lies in [min_payload_size(), size()] (as in
        // allocate_block's bump): one compare. SizeChecked false: the caller
        // already knows bytes <= small_block_max.
        template <bool SizeChecked = true>
        XOFFSET_DETAIL_INLINE void* try_bump_first(std::uint32_t bytes) noexcept {
            if constexpr (SizeChecked)
                if (bytes > small_block_max) return nullptr;
            char* const data = buffer_.data();
            auto* const sm = std::launder(reinterpret_cast<arena_header*>(data));
            // No free block (the free bytes are zero).
            if (inband_word(data + inband_bytes_at) != 0) return nullptr;
            // used_end is 8-aligned (every image's is: see
            // check_reinit_payload), and so is the payload base.
            const std::uint64_t start = sm->used_end;
            if (start + bytes > buffer_.size()) [[unlikely]] return nullptr;
            sm->used_end = start + bytes;
            return payload_byte_address(static_cast<size_type>(start));
        }
        // publish_first_block's bump: try_bump_first<false> from the payload
        // base the caller's owner lookup returned (`data`, the arena header),
        // so the owner's buffer pointer is not read again.
        XOFFSET_DETAIL_INLINE void* try_bump_first_at(char* data, std::uint32_t bytes) noexcept {
            auto* const sm = std::launder(reinterpret_cast<arena_header*>(data));
            if (inband_word(data + inband_bytes_at) != 0) return nullptr;
            const std::uint64_t start = sm->used_end;
            if (start + bytes > buffer_.size()) [[unlikely]] return nullptr;
            sm->used_end = static_cast<std::uint32_t>(start + bytes);
            return data + start;
        }

        bool try_expand_block(void* ptr, std::uint32_t old_bytes, std::uint32_t new_bytes) {
            if (!ptr || buffer_.empty() || old_bytes == 0 || new_bytes == 0 ||
                ((old_bytes | new_bytes) & 7u) != 0 || new_bytes > MAX_PAYLOAD) return false;
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (address < base || address - base >= buffer_.size()) return false;
            const auto offset = static_cast<size_type>(address - base);
            if (std::uint64_t(offset) + old_bytes != arena()->used_end) return false;
            if (new_bytes <= old_bytes) return true;
#if defined(XOFFSET_STRESS_RELOCATE)
            // Stress mode: in a Relocating buffer a container always regrows
            // into a fresh block (and the old one is poisoned when retired),
            // as std::vector would. Other policies keep their layout.
            if (packed_.growth_policy == XGrowPolicy::Relocating) return false;
#endif
            if (new_bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + new_bytes)) return false;
            arena()->used_end = offset + new_bytes;
            return true;
        }

        // ptr/bytes must name exactly one live block of this arena, or a run
        // of adjacent live blocks released together (subtree retirement):
        // blocks have no headers and the allocator records only free blocks.
        // A small block in range is recorded inline (inband_retire_small);
        // everything else takes retire_block_general.
        XOFFSET_DETAIL_INLINE void retire_block(void* ptr, std::uint32_t bytes) noexcept {
#if defined(XOFFSET_STRESS_RELOCATE)
            // Stress mode: a retired block is poisoned, in a pin scope too (a
            // reference the container's own rules invalidated reads garbage).
            if (packed_.growth_policy == XGrowPolicy::Relocating) stress_poison_block(ptr, bytes);
#endif
            // Inside a pin scope a retired block is not reused until the
            // scope closes: plain references into it stay harmless.
            if constexpr (detail::pin_quarantine)
                if (packed_.pinned) [[unlikely]] return quarantine_block(ptr, bytes);
            // One compare is the block's range test: a ptr below the base
            // gives an offset that wraps past size() unless the block would
            // cross the payload's start, which no block of any payload does
            // (the 64-byte host header lies before each payload); a null ptr
            // is a page or more below it.
            const std::uint64_t at = reinterpret_cast<std::uintptr_t>(ptr) -
                                     reinterpret_cast<std::uintptr_t>(buffer_.data());
            if (bytes != 0 && (bytes & 7u) == 0 && bytes <= small_block_max &&
                at + bytes <= buffer_.size()) [[likely]]
                return inband_retire_small(ptr, bytes, static_cast<std::uint32_t>(at));
            retire_block_general(ptr, bytes);
        }

        // ---- In-band allocator --------------------------------------------
        // The allocator's state is payload bytes, so a copy of the payload
        // carries it. Free blocks of one size form a stack linked through
        // their first 4 bytes (the payload offset of the next block of that
        // size, 0 for the last); the rest of a free block is zero. A
        // directory, itself a block of the payload, holds the stacks' heads
        // sorted by size:
        //   u32 n, u32 cap, cap entries {u32 bytes, u32 head}
        // Entries [0, n) are in use; every other one is {inband_end, 0}, and
        // n < cap, so a scan for the first entry of at least a size needs no
        // bound. A size whose stack emptied keeps its entry (head 0). The
        // image header holds the directory's offset (free_dir, at 32; 0:
        // no directory, so no free block) and the bytes of all free blocks
        // (at 36, the directory not counted). The first retirement makes the
        // directory: inside the retired block when that is large enough (up
        // to 8 entries), else at the end of the payload. A full directory
        // drops its empty entries when at most half of them hold a block;
        // else it moves to a block of twice the entries (the retired block
        // again when large enough) and its old block becomes free. Placement:
        // the exact size, else the smallest larger block (split), else the
        // end of the payload. Nothing is allocated on the host, and the
        // storage only grows in place: a block that cannot be recorded (no
        // room for a directory) is lost, and the free state is marked
        // incomplete (free_table_lost_).
        static constexpr std::size_t inband_dir_at = 32;
        static constexpr std::size_t inband_bytes_at = 36;
        static constexpr std::uint32_t inband_end = 0xFFFFFFFFu;  // an unused entry's size
        static constexpr std::uint32_t inband_first_cap = 8;        // a directory at the end
        static constexpr std::uint32_t inband_hosted_cap_min = 4;   // one inside a retired block
        static constexpr std::uint32_t inband_hosted_cap_max = 8;
        static constexpr std::uint32_t inband_dir_bytes(std::uint32_t cap) noexcept { return 8u + 8u * cap; }
        // A loaded directory's cap matches its block (no 32-bit wrap: the
        // entries the check reads lie inside the block).
        static constexpr bool inband_dir_spans(std::uint32_t cap, std::uint32_t bytes) noexcept {
            return 8u + 8u * std::uint64_t(cap) == bytes;
        }

        static XOFFSET_DETAIL_INLINE std::uint32_t inband_word(const char* at) noexcept {
            std::uint32_t value;
            std::memcpy(&value, at, sizeof(value));
            return value;
        }
        static XOFFSET_DETAIL_INLINE void inband_set(char* at, std::uint32_t value) noexcept {
            std::memcpy(at, &value, sizeof(value));
        }

        // The first entry of directory d whose size is at least `bytes` (an
        // unused entry, when no used one is): entries in order, four to a
        // step (a compare and a branch each; the unused entry after the
        // used ones ends the scan), after a binary search narrows more than
        // 16 used ones.
        static XOFFSET_DETAIL_INLINE std::uint32_t* inband_find(std::uint32_t* d, std::uint32_t bytes) noexcept {
            std::uint32_t* e = d + 2;
            if (std::uint32_t n = d[0]; n > 16u) [[unlikely]] {
                do {
                    const std::uint32_t half = n / 2u;
                    if (e[2u * (half - 1u)] < bytes) e += 2u * half;
                    n -= half;
                } while (n > 16u);
            }
            for (;; e += 8) {
                if (e[0] >= bytes) return e;
                if (e[2] >= bytes) return e + 2;
                if (e[4] >= bytes) return e + 4;
                if (e[6] >= bytes) return e + 6;
            }
        }

        // A directory of `cap` entries at `at`, n of them (already there) in
        // use: its header and the unused entries.
        static void inband_dir_init(char* at, std::uint32_t n, std::uint32_t cap) noexcept {
            inband_set(at, n);
            inband_set(at + 4, cap);
            constexpr std::uint64_t unused = inband_end;  // {inband_end, 0}, little-endian
            for (std::uint32_t k = n; k < cap; ++k) std::memcpy(at + 8u + 8u * k, &unused, sizeof(unused));
        }

        // try_allocate_small, in-band: the exact size's head; else the first
        // larger size with a block, split (out of line); else a bump inside
        // the current extent. Null where allocate_block takes the general path.
        XOFFSET_DETAIL_INLINE void* inband_allocate_small(std::uint32_t bytes) noexcept {
            if (buffer_.size() < sizeof(arena_header)) [[unlikely]] return nullptr;
            char* const base = buffer_.data();
            if (const std::uint32_t dir = inband_word(base + inband_dir_at); dir != 0) {
                std::uint32_t* const e = inband_find(reinterpret_cast<std::uint32_t*>(base + dir), bytes);
                if (const std::uint32_t offset = e[1]; e[0] == bytes && offset != 0) [[likely]] {
                    e[1] = inband_word(base + offset);
                    inband_set(base + offset, 0);
                    inband_set(base + inband_bytes_at, inband_word(base + inband_bytes_at) - bytes);
                    if (touch_log_) [[unlikely]] inband_log(offset, 4u);
                    return base + offset;
                }
                // The first larger size whose stack holds a block: split.
                std::uint32_t* f = e;
                while (f[1] == 0 && f[0] != inband_end) f += 2;
                if (f[1] != 0) return inband_split(f, bytes);
            }
            // No free block of the size or larger: try_allocate_small's bump.
            auto* const sm = std::launder(reinterpret_cast<arena_header*>(base));
            const std::uint64_t start = sm->used_end;
            if (((reinterpret_cast<std::uintptr_t>(base) + start) & 7u) == 0 &&
                start + bytes <= buffer_.size()) [[likely]] {
                sm->used_end = start + bytes;
                return base + start;
            }
            return nullptr;
        }

        // The head of entry e (a size of at least `bytes` whose stack holds
        // a block): taken, its tail beyond `bytes` freed again.
        XOFFSET_DETAIL_NOINLINE void* inband_split(std::uint32_t* e, std::uint32_t bytes) noexcept {
            char* const base = buffer_.data();
            const std::uint32_t offset = e[1], found = e[0];
            e[1] = inband_word(base + offset);
            inband_set(base + offset, 0);
            inband_set(base + inband_bytes_at, inband_word(base + inband_bytes_at) - found);
            if (touch_log_) [[unlikely]] inband_log(offset, 4u);
            // The tail becomes a free block of its own: its header is
            // cleared first (a wire load writes only each block's own
            // header, so an accepted input may leave other bytes there).
            if (found > bytes) {
                std::memset(base + offset + bytes, 0, 8);
                inband_push_zeroed(offset + bytes, found - bytes, 8u);
            }
            return base + offset;
        }

        // A block of the allocate_block_general request from the directory,
        // or null (allocate_block_general then bumps).
        void* inband_take(std::uint32_t bytes) noexcept {
            char* const base = buffer_.data();
            const std::uint32_t dir = inband_word(base + inband_dir_at);
            if (dir == 0) return nullptr;
            std::uint32_t* e = inband_find(reinterpret_cast<std::uint32_t*>(base + dir), bytes);
            while (e[1] == 0 && e[0] != inband_end) e += 2;
            return e[1] == 0 ? nullptr : inband_split(e, bytes);
        }

        // retire_block, in-band, for a block in range of at most small_max
        // bytes: cleared (M3), then pushed on its size's stack; a size the
        // directory does not hold yet takes inband_add_class.
        XOFFSET_DETAIL_INLINE void inband_retire_small(void* ptr, std::uint32_t bytes, std::uint32_t offset) noexcept {
            zero_retired(ptr, bytes);
            inband_push_zeroed(offset, bytes, bytes);
        }

        // Records a free block whose bytes are all zero; its first `changed`
        // bytes were just written (a touch log records them).
        XOFFSET_DETAIL_INLINE void inband_push_zeroed(std::uint32_t offset, std::uint32_t bytes,
                                                      std::uint32_t changed) noexcept {
            char* const base = buffer_.data();
            if (const std::uint32_t dir = inband_word(base + inband_dir_at); dir != 0) [[likely]] {
                std::uint32_t* const e = inband_find(reinterpret_cast<std::uint32_t*>(base + dir), bytes);
                if (e[0] == bytes) [[likely]] {
                    inband_set(base + offset, e[1]);
                    e[1] = offset;
                    inband_set(base + inband_bytes_at, inband_word(base + inband_bytes_at) + bytes);
                    if (touch_log_) [[unlikely]] inband_log(offset, changed);
                    return;
                }
            }
            inband_add_class(offset, bytes, changed);
        }

        // inband_push_zeroed for a size the directory has no entry for (or
        // with no directory yet).
        XOFFSET_DETAIL_NOINLINE void inband_add_class(std::uint32_t offset, std::uint32_t bytes,
                                                      std::uint32_t changed) noexcept {
            // Whatever happens below, the block's changed bytes and the
            // directory are recorded for a touch log at the end.
            struct Log {
                XBufferCore* core;
                std::uint32_t offset, changed;
                ~Log() {
                    if (core->touch_log_) [[unlikely]] core->inband_log(offset, changed);
                }
            } const log{this, offset, changed};
            char* const base = buffer_.data();
            std::uint32_t dir = inband_word(base + inband_dir_at);
            if (dir == 0) {
                std::uint32_t cap;
                if (bytes >= inband_dir_bytes(inband_hosted_cap_min)) {
                    // The block's end holds the directory (its start stays
                    // the free block, where a split takes its head).
                    cap = (std::min)((bytes - 8u) / 8u, inband_hosted_cap_max);
                    bytes -= inband_dir_bytes(cap);
                    dir = offset + bytes;
                } else {
                    cap = inband_first_cap;
                    dir = inband_bump(inband_dir_bytes(cap));
                    if (dir == 0) return inband_lose();
                }
                inband_dir_init(base + dir, 0, cap);
                inband_set(base + inband_dir_at, dir);
                if (bytes == 0) return;
            }
            auto* d = reinterpret_cast<std::uint32_t*>(base + dir);
            if (d[0] + 1u == d[1]) {
                // Full: when at most half of the entries hold a block, the
                // empty ones go; else a directory of twice the entries.
                // Either way about half the entries are free after it.
                const std::uint32_t n = d[0], cap = d[1];
                std::uint32_t live = 0;
                for (std::uint32_t r = 0; r < n; ++r) live += d[3u + 2u * r] != 0;
                if (live <= n / 2u) {
                    std::uint32_t w = 0;
                    for (std::uint32_t r = 0; r < n; ++r) {
                        if (d[3u + 2u * r] == 0) continue;
                        d[2u + 2u * w] = d[2u + 2u * r];
                        d[3u + 2u * w] = d[3u + 2u * r];
                        ++w;
                    }
                    inband_dir_init(reinterpret_cast<char*>(d), w, cap);
                } else {
                    const std::uint32_t grown = cap * 2u, size = inband_dir_bytes(grown);
                    std::uint32_t at;
                    if (bytes >= size) {
                        // The block's end holds the new directory.
                        bytes -= size;
                        at = offset + bytes;
                    } else {
                        at = inband_bump(size);
                        if (at == 0) return inband_lose();
                    }
                    auto* const nd = reinterpret_cast<std::uint32_t*>(base + at);
                    for (std::uint32_t k = 2; k < 2u + 2u * n; ++k) nd[k] = d[k];
                    inband_dir_init(base + at, n, grown);
                    inband_set(base + inband_dir_at, at);
                    // The old directory's block becomes free.
                    const std::uint32_t old_bytes = inband_dir_bytes(cap);
                    for (std::uint32_t k = 0; k < old_bytes / 4u; ++k) d[k] = 0;
                    inband_push_zeroed(dir, old_bytes, old_bytes);
                    d = nd;
                    if (bytes == 0) return;
                }
            }
            std::uint32_t* const e = inband_find(d, bytes);
            if (e[0] == bytes) {
                // The size came in with the old directory's block.
                inband_set(base + offset, e[1]);
                e[1] = offset;
            } else {
                // The entries from e (and the first unused one) move up one.
                for (std::uint32_t* p = d + 2u + 2u * d[0]; p != e; p -= 2) {
                    p[0] = p[-2];
                    p[1] = p[-1];
                }
                e[0] = bytes;
                e[1] = offset;
                ++d[0];
            }
            inband_set(base + inband_bytes_at, inband_word(base + inband_bytes_at) + bytes);
        }

        // The in-band allocator wrote image bytes [at, at + bytes) (none for
        // bytes 0), the header's free_dir and free_bytes, and the directory:
        // into the touch log, which a delta from touch marks sends. The
        // header words and the directory are recorded once per period
        // (again when the directory moved).
        XOFFSET_DETAIL_NOINLINE void inband_log(std::uint32_t at, std::uint32_t bytes) noexcept {
            detail::DeltaTouchLog& log = *touch_log_;
            const char* const base = buffer_.data();
            const std::uint32_t dir = inband_word(base + inband_dir_at);
            if (log.dir_at != dir) {
                log.add(static_cast<std::uint32_t>(inband_dir_at), 8u);
                if (dir != 0) log.add(dir, inband_dir_bytes(inband_word(base + dir + 4u)));
                log.dir_at = dir;
            }
            if (bytes != 0) log.add(at, bytes);
        }
        // The free state was rewritten as a whole: a touch log no longer
        // says what changed.
        void inband_log_lost() noexcept {
            if (touch_log_) [[unlikely]] touch_log_->lose();
        }

        // `bytes` at the end of the payload (a directory: its writer sets
        // every byte). A retirement runs inside container operations that
        // hold pointers into the payload, so the storage never moves here:
        // past the current extent it grows in place only (up to the
        // reservation, as the growth policy allows). 0 when they do not fit.
        std::uint32_t inband_bump(std::uint32_t bytes) noexcept {
            auto* const sm = arena();
            const std::uint64_t start = sm->used_end;
            if ((start & 7u) != 0) return 0;
            if (start + bytes > buffer_.size()) {
                if (start + bytes > MAX_PAYLOAD || packed_.growth_policy == XGrowPolicy::Explicit) return 0;
                const size_type maximum = buffer_.reservation_limit();
                if (start + bytes > maximum ||
                    !buffer_.grow_to(extent_target(static_cast<size_type>(start + bytes), maximum)))
                    return 0;
            }
            sm->used_end = start + bytes;
            return static_cast<std::uint32_t>(start);
        }

        // A free block could not be recorded: save_verified recomputes the
        // free blocks from the live graph (inband_rebuild).
        void inband_lose() noexcept { free_table_lost_ = true; }

        std::uint32_t inband_dir() const noexcept {
            const auto* sm = arena();
            return sm ? inband_word(reinterpret_cast<const char*>(sm) + inband_dir_at) : 0u;
        }
        std::uint32_t inband_free_bytes() const noexcept {
            const auto* sm = arena();
            return sm ? inband_word(reinterpret_cast<const char*>(sm) + inband_bytes_at) : 0u;
        }
        // fn(WireHole) for each free block of at least min_bytes: by size,
        // each size's stack from its head. fn must not change the blocks.
        template <typename Fn>
        void inband_for_each(Fn&& fn, std::uint32_t min_bytes = 0) const {
            const auto* sm = arena();
            if (!sm) return;
            const char* const base = reinterpret_cast<const char*>(sm);
            const std::uint32_t dir = inband_word(base + inband_dir_at);
            if (dir == 0) return;
            const auto* const d = reinterpret_cast<const std::uint32_t*>(base + dir);
            for (std::uint32_t i = 0, n = d[0]; i < n; ++i) {
                const std::uint32_t bytes = d[2u + 2u * i];
                if (bytes < min_bytes) continue;
                for (std::uint32_t at = d[3u + 2u * i]; at != 0; at = inband_word(base + at)) fn(WireHole{at, bytes});
            }
        }
        std::size_t inband_count() const noexcept {
            std::size_t count = 0;
            inband_for_each([&](WireHole) noexcept { ++count; });
            return count;
        }

        // A table of free blocks (`holes` need not be aligned) whose
        // payload bytes (link words included) may already hold a directory
        // of exactly those blocks (replace_free_blocks: a copy of a
        // buffer's bytes given its wire holes). The directory is the entry
        // whose second word is not zero (a free block's is zero; a
        // directory's is its capacity): kept, the blocks recorded anew (a
        // table without one: inband_rebuild), or rejected by `check` (one
        // directory at most, holding exactly the other entries, in stack
        // order), nothing changed.
        enum class inband_adopted { kept, rebuilt, rejected };
        inband_adopted inband_adopt_table(const void* holes, std::size_t count, bool check) noexcept {
            char* const base = buffer_.data();
            const auto* in = static_cast<const char*>(holes);
            const auto entry = [&](std::size_t i) noexcept {
                WireHole hole;
                std::memcpy(&hole, in + i * sizeof(hole), sizeof(hole));
                return hole;
            };
            if (count == 0) {
                inband_set(base + inband_dir_at, 0);
                inband_set(base + inband_bytes_at, 0);
                free_table_lost_ = false;
                return inband_adopted::kept;
            }
            std::size_t dir = count;
            std::uint64_t total = 0;
            bool by_offset = true;
            for (std::size_t i = 0; i < count; ++i) {
                const WireHole hole = entry(i);
                total += hole.bytes;
                if (i != 0 && hole.offset <= entry(i - 1u).offset) by_offset = false;
                if (inband_word(base + hole.offset + 4u) == 0) continue;
                // A free block's second word is 0; only the directory (at
                // least 16 bytes) has another, its capacity. Checked, an
                // 8-byte block with one is rejected, as the full check of a
                // load rejects it.
                if (hole.bytes < 16u) {
                    if (check) dir = count + 1u;
                    continue;
                }
                if (dir == count) dir = i;
                else if (check) dir = count + 1u;
            }
            // A rejected table changes nothing (a delta's rollback restores
            // the payload bytes only).
            const auto fail = [] { return inband_adopted::rejected; };
            if (dir == count) {
                // A table with no directory (a sender without the in-band
                // allocator: its free blocks are zero): recorded anew. With
                // no room for a directory the state is incomplete, as a
                // lost block leaves it.
                (void)inband_rebuild(holes, count);
                return inband_adopted::rebuilt;
            }
            if (dir > count) return fail();
            const WireHole directory = entry(dir);
            total -= directory.bytes;
            if (check) {
                const auto* const d = reinterpret_cast<const std::uint32_t*>(base + directory.offset);
                const std::uint32_t n = d[0], cap = d[1];
                if (n >= cap || !inband_dir_spans(cap, directory.bytes)) return fail();
                std::uint32_t last = 0;
                for (std::uint32_t k = 0; k < n; ++k) {
                    const std::uint32_t size = d[2u + 2u * k];
                    if (size <= last || (size & 7u) != 0) return fail();
                    last = size;
                }
                for (std::uint32_t k = n; k < cap; ++k)
                    if (d[2u + 2u * k] != inband_end || d[3u + 2u * k] != 0) return fail();
                // Each stack's blocks are entries of the table: by offset, a
                // binary search; else (the replica's own regions,
                // for_each_free_region's order) the next entries in order,
                // after the directory. Every other entry is visited once.
                std::size_t next = 1, budget = count - 1u;
                if (!by_offset && dir != 0) return fail();
                for (std::uint32_t k = 0; k < n; ++k) {
                    const std::uint32_t size = d[2u + 2u * k];
                    for (std::uint32_t at = d[3u + 2u * k]; at != 0; at = inband_word(base + at)) {
                        if (budget == 0) return fail();
                        --budget;
                        WireHole found{};
                        if (by_offset) {
                            std::size_t lo = 0, hi = count;
                            while (lo < hi) {
                                const std::size_t mid = lo + (hi - lo) / 2u;
                                if (entry(mid).offset < at) lo = mid + 1u;
                                else hi = mid;
                            }
                            if (lo == count || lo == dir) return fail();
                            found = entry(lo);
                        } else {
                            found = entry(next++);
                        }
                        if (found.offset != at || found.bytes != size) return fail();
                    }
                }
                if (budget != 0) return fail();
            }
            inband_set(base + inband_dir_at, directory.offset);
            inband_set(base + inband_bytes_at, static_cast<std::uint32_t>(total));
            inband_log_lost();
            free_table_lost_ = false;
            return inband_adopted::kept;
        }

        // Free blocks recomputed from the live graph (save_verified after an
        // unverified load or a lost block), or a checked table the directory
        // did not describe (`holes` need not be aligned): every one cleared
        // and recorded anew. The directory goes first, into the end of the
        // largest block (sized for the table's sizes, as far as the block
        // holds them), else at the end of the payload. False (the table stays lost) when a block
        // cannot be recorded.
        bool inband_rebuild(const void* holes, std::size_t count) noexcept {
            auto* const sm = arena();
            if (!sm) return false;
            char* const base = reinterpret_cast<char*>(sm);
            const auto entry = [&](std::size_t i) noexcept {
                WireHole hole;
                std::memcpy(&hole, static_cast<const char*>(holes) + i * sizeof(hole), sizeof(hole));
                return hole;
            };
            inband_set(base + inband_dir_at, 0);
            inband_set(base + inband_bytes_at, 0);
            inband_log_lost();
            free_table_lost_ = false;
            if (count == 0) return true;
            // The distinct sizes, counted up to 32 (more grow the directory).
            std::uint32_t seen[32], sizes = 0;
            std::size_t host = 0;
            std::uint64_t host_bytes = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const WireHole hole = entry(i);
                std::memset(base + hole.offset, 0, hole.bytes);
                if (hole.bytes > host_bytes) host = i, host_bytes = hole.bytes;
                if (sizes < 32u) {
                    std::uint32_t k = 0;
                    while (k < sizes && seen[k] != hole.bytes) ++k;
                    if (k == sizes) seen[sizes++] = static_cast<std::uint32_t>(hole.bytes);
                }
            }
            // Two entries more than the sizes: the host's remainder may be a
            // size of its own, and one entry stays unused.
            const std::uint32_t want = (std::max)(sizes + 2u, inband_first_cap);
            std::uint32_t at = 0, cap = 0, rest = 0;
            if (host_bytes >= inband_dir_bytes(2)) {
                const auto bytes = static_cast<std::uint32_t>(host_bytes);
                cap = (std::min)((bytes - 8u) / 8u, want);
                rest = bytes - inband_dir_bytes(cap);
                at = static_cast<std::uint32_t>(entry(host).offset) + rest;
            } else {
                cap = want;
                at = inband_bump(inband_dir_bytes(cap));
                if (at == 0) return inband_lose(), false;
                host = count;
            }
            inband_dir_init(base + at, 0, cap);
            inband_set(base + inband_dir_at, at);
            if (rest != 0) inband_push_zeroed(at - rest, rest, 0u);
            for (std::size_t i = 0; i < count; ++i) {
                if (i == host) continue;
                const WireHole hole = entry(i);
                inband_push_zeroed(static_cast<std::uint32_t>(hole.offset), static_cast<std::uint32_t>(hole.bytes), 0u);
            }
            return !free_table_lost_;
        }

        // False once a free block could not be recorded, or for the content
        // of an unverified load, until a verified load or save_verified
        // recomputes the free blocks.
        bool free_table_complete() const noexcept { return !free_table_lost_; }
        // The free blocks' count walks their stacks; the directory is not a
        // free block (wire_hole_count counts it).
        std::size_t free_block_count() const noexcept { return inband_count(); }
        std::uint64_t free_block_bytes() const noexcept { return inband_free_bytes(); }

        // The image's free regions: the directory block first, then each
        // size's stack from its head (what the full check tiles with the live
        // blocks).
        std::size_t free_region_count() const noexcept { return inband_dir() != 0 ? 1u + inband_count() : 0u; }
        template <typename Fn>
        void for_each_free_region(Fn&& fn) const {
            if (const WireHole dir = free_directory(); dir.bytes != 0) {
                fn(dir);
                inband_for_each(fn);
            }
        }
        // The free directory, a block of its own and the first wire hole:
        // {0, 0} when there is none.
        WireHole free_directory() const noexcept {
            if (const std::uint32_t dir = inband_dir(); dir != 0)
                return WireHole{dir, inband_dir_bytes(inband_word(static_cast<const char*>(get_address()) + dir + 4u))};
            return WireHole{};
        }
        // The first bytes of a free block that are the allocator's: the link
        // to the next free block of its size, then a zero word, which tells a
        // free block from the directory. The rest reads as zero.
        static constexpr std::uint32_t free_block_header_bytes = 8u;

        // A hint: starts loading, without waiting, the host-side lines the
        // next allocation or release in this buffer reads first: the owner
        // map entry and owner word of the payload, and the arena header. It changes
        // no contents, no placement and no state, and it is a no-op where the
        // compiler has no prefetch builtin. When a buffer that has gone cold
        // is about to be updated from data that is still arriving (a reply
        // being received or decoded), calling this first overlaps those cache
        // misses with that work instead of paying them one after another in
        // the update.
        void prefetch_for_update() const noexcept {
            const char* const base = buffer_.data();
            if (!base) return;
            XOFFSET_DETAIL_PREFETCH(
                detail::OwnerMap::entry_address(reinterpret_cast<std::uintptr_t>(base)), 0);
            XOFFSET_DETAIL_PREFETCH(base - sizeof(void*), 0);
            XOFFSET_DETAIL_PREFETCH(base, 1);
        }

        // Free blocks by size, each size's stack from its head (inband_for_each).
        template <typename Fn>
        void for_each_free_block(Fn&& fn) const { inband_for_each(fn); }

        // Visits only free blocks of at least min_bytes, in no particular
        // offset order.
        template <typename Fn>
        void for_each_free_block_at_least(std::uint32_t min_bytes, Fn&& fn) const { inband_for_each(fn, min_bytes); }

        // Replaces the free state with an exact recomputation (used after an
        // unverified load or when a free block could not be recorded). A
        // directory in the payload that holds exactly these blocks (a copy of
        // a buffer's bytes given its wire holes) is kept, each block cleared
        // past its link word; else every block is recorded anew.
        void replace_free_blocks(const std::vector<WireHole>& holes) {
            const auto adopted = arena() ? inband_adopt_table(holes.data(), holes.size(), true)
                                         : inband_adopted::rejected;
            if (adopted == inband_adopted::kept) {
                char* const base = buffer_.data();
                const std::uint32_t dir = inband_dir();
                for (const WireHole& hole : holes)
                    if (hole.offset != dir) std::memset(base + hole.offset + 4u, 0, hole.bytes - 4u);
            } else if (adopted == inband_adopted::rejected) {
                (void)inband_rebuild(holes.data(), holes.size());
            }
            if (!arena() || free_table_lost_) [[unlikely]]
                XOFFSET_THROW(XException(XErrc::capacity_exceeded, "XBufferCore: no room for the free directory"));
        }

    private:
        // The general allocation and retirement paths (see allocate_block
        // and retire_block, which decide the common outcomes inline).
        XOFFSET_DETAIL_NOINLINE void* allocate_block_general(std::uint32_t bytes, std::size_t alignment,
                                                             bool allow_growth) {
            if (bytes == 0 || (bytes & 7u) != 0 || bytes > MAX_PAYLOAD || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > MAX_PAYLOAD) XOFFSET_THROW(XBadAlloc());
            if (alignment <= 8)
                if (void* reused = inband_take(bytes)) return reused;
            return bump_block(bytes, alignment, allow_growth);
        }

        XOFFSET_DETAIL_NOINLINE void retire_block_general(void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0 || buffer_.empty()) return;
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (address < base || address - base >= buffer_.size()) return;
            const auto offset = static_cast<std::uint32_t>(address - base);
            // M3 (threat model T5/P1): a block becomes free only after its old
            // contents are cleared, so a free block's bytes past its link word
            // are zero: the image is sent as it is, and a delta carries a
            // free block only when it comes or goes.
            // A large block's whole pages go back to the OS instead (Linux,
            // XOFFSET_RETIRE_DISCARD_BYTES): a vector that outgrew its block in
            // a long-lived buffer leaves a hole that later, larger requests do
            // not reuse, and its dirty pages would stay resident.
            const std::size_t cleared = std::min<std::size_t>(bytes, buffer_.size() - offset);
            if (!detail::discard_retired_pages(ptr, cleared)) std::memset(ptr, 0, cleared);
            // A block that ends past the payload is not a block of it.
            if ((bytes & 7u) != 0 || cleared != bytes) {
                if (touch_log_) [[unlikely]] inband_log(offset, static_cast<std::uint32_t>(cleared));
                return;
            }
            inband_push_zeroed(offset, bytes, bytes);
        }

        // M3 zeroing of a retired block: 8..64 bytes (most blocks) use two
        // overlapping fixed-size stores instead of a memset call.
        static XOFFSET_DETAIL_INLINE void zero_retired(void* p, std::size_t n) noexcept {
            auto* bytes = static_cast<unsigned char*>(p);
            if (n >= 8u && n <= 64u) {
                if (n <= 16u) { std::memset(bytes, 0, 8); std::memset(bytes + n - 8u, 0, 8); }
                else if (n <= 32u) { std::memset(bytes, 0, 16); std::memset(bytes + n - 16u, 0, 16); }
                else { std::memset(bytes, 0, 32); std::memset(bytes + n - 32u, 0, 32); }
                return;
            }
            std::memset(bytes, 0, n);
        }


        // A free block whose bytes may hold anything (alignment padding):
        // cleared, then recorded.
        void push_free_block(std::uint32_t offset, std::uint32_t bytes) noexcept {
            std::memset(buffer_.data() + offset, 0, bytes);
            inband_push_zeroed(offset, bytes, bytes);
        }

        void* bump_block(std::uint32_t bytes, std::size_t alignment, bool allow_growth) {
            auto* sm = arena();
            const size_type current_end = used_end_clamped();
            // Align the ADDRESS (the payload base is page aligned on VM and
            // 64-byte aligned on a slot). For alignment <= 8 this equals
            // aligning the offset.
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto limit = (std::numeric_limits<std::uintptr_t>::max)();
            const auto align = std::max<std::size_t>(alignment, 8u);
            if (current_end > limit - base || align - 1u > limit - base - current_end) XOFFSET_THROW(XBadAlloc());
            const size_type start = align_up(base + current_end, align) - base;
            if (start > MAX_PAYLOAD || bytes > MAX_PAYLOAD - start) XOFFSET_THROW(XBadAlloc());
            if (!ensure_allocation_extent(start + bytes, allow_growth)) XOFFSET_THROW(XBadAlloc());
            // Publish the allocation before any host callback can observe it.
            sm->used_end = start + bytes;
            void* result = payload_byte_address(start);
            // Alignment padding (alignment > 8 only) stays an exact free block
            // (recorded in the payload: no host callback runs).
            if (start != current_end)
                push_free_block(static_cast<std::uint32_t>(current_end),
                                static_cast<std::uint32_t>(start - current_end));
            return result;
        }

        void publish_host_owner() noexcept { buffer_.set_host_owner(this); }


        // Host-only identity: the lease that host-callback revalidation reads.
        // A record follows one payload reservation, never a particular Core
        // object's address, and is never revived after release.
        struct OwnerRecord {
            const void* const payload;
            std::atomic<XBufferCore*> owner;
            OwnerRecord(const void* base, XBufferCore* value) noexcept : payload(base), owner(value) {}
        };
        using OwnerRecordPtr = std::shared_ptr<OwnerRecord>;

        static size_type align_up(size_type value, size_type alignment) noexcept {
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        // A record's owner field says whether its payload is still owned
        // (null once released or before a receive publishes it); the owner
        // itself is the payload's host word, which every ownership change
        // (swap, move, replace_storage, publish) republishes. swap() does not
        // write records, so the field may name an earlier owner.
        static XBufferCore* record_owner(const OwnerRecordPtr& record) noexcept {
            if (!record || !record->owner.load(std::memory_order_acquire)) return nullptr;
            return owned_record_owner(*record);
        }
        // The owner of a record whose owner field is set: its payload is
        // live, and the host word names the owner whose buffer starts there.
        static XBufferCore* owned_record_owner(const OwnerRecord& record) noexcept {
            return *reinterpret_cast<XBufferCore* const*>(
                static_cast<const char*>(record.payload) - sizeof(void*));
        }

        static XBufferCore& require_record_owner(const OwnerRecordPtr& record) {
            detail::owner_lookup_count(&detail::OwnerLookupStats::slow_revalidations);
            if (auto* owner = record_owner(record)) return *owner;
            XOFFSET_THROW(XException(XErrc::invalid_state, "arena owner lookup failed"));
        }

        static size_type min_payload_size() noexcept {
            return sizeof(arena_header); // 40: a multiple of 8 on every platform
        }

        // Storage rule. VM: a pool, Pinned, Automatic growth with a
        // maximum above SLOT_MAX, or an initial size above SLOT_MAX. Pinned
        // slot: maximum <= SLOT_MAX (the slot holds the whole maximum).
        // Movable slot: otherwise, sized for the initial size.
        struct StoragePlan {
            std::size_t slot_class; // SlotGeometry::classes: VM
            bool movable;
        };
        static detail::vstatus plan_storage(size_type initial, size_type maximum, XGrowPolicy growth,
                                            bool pooled, XStorage request, StoragePlan& plan) {
            using Geometry = detail::SlotGeometry;
            constexpr StoragePlan vm{Geometry::classes, false};
            plan = vm;
            if (growth == XGrowPolicy::Relocating) {
                // Its storage is chosen by relocating_owner.
                if (pooled || request == XStorage::Pinned)
                    XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                        "XGrowPolicy::Relocating takes no pool and no XStorage::Pinned");
                return detail::vok();
            }
            if (request == XStorage::Pinned) return detail::vok();
            auto refuse = [&]() -> detail::vstatus {
                if (request == XStorage::Movable)
                    XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument,
                        "XStorage::Movable needs slot storage: no pool, initial size <= SLOT_MAX, "
                        "and Explicit growth or maximum <= SLOT_MAX");
                return detail::vok();
            };
            if (!Geometry::enabled || pooled) return refuse();
            if (maximum <= Geometry::max_payload) {
                plan = {Geometry::class_for(maximum), false};
                return detail::vok();
            }
            if (growth == XGrowPolicy::Automatic || initial > Geometry::max_payload) return refuse();
            plan = {Geometry::class_for(initial), true};
            return detail::vok();
        }

        // Storage work in which std::bad_alloc and std::length_error mean
        // that the arena could not get memory: they become XBadAlloc (code
        // out_of_storage) with or without exceptions.
        template <typename Step>
        static detail::vstatus as_arena_failure(Step&& step) {
#if XOFFSET_NO_EXCEPTIONS
            if (step()) return true;
            const auto code = detail::pending_error().code;
            if (code == XErrc::host_allocation_failed || code == XErrc::length_error)
                return detail::record_failure(XErrc::out_of_storage, "XOffset arena allocation failed");
            return false;
#else
            try {
                step();
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            } catch (const std::length_error&) {
                throw XBadAlloc();
            }
#endif
        }

        // as_arena_failure for std::bad_alloc only.
        template <typename Step>
        static detail::vstatus as_arena_alloc_failure(Step&& step) {
#if XOFFSET_NO_EXCEPTIONS
            if (step()) return true;
            if (detail::pending_error().code == XErrc::host_allocation_failed)
                return detail::record_failure(XErrc::out_of_storage, "XOffset arena allocation failed");
            return false;
#else
            try {
                step();
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            }
#endif
        }

        static bool plans_slot(const StoragePlan& plan) noexcept {
            return plan.slot_class != detail::SlotGeometry::classes;
        }

        detail::vstatus init_fresh(size_type size, size_type max_reserved, const XBufferPool& pool = {},
                                   XStorage request = XStorage::Auto, size_type reserve = 0) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD)
                XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::payload_limit_message);
            size = std::max(size, min_payload_size());
            if (size > max_reserved) XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::arena_failure_message);
            StoragePlan plan;
            XOFFSET_VTRY(plan_storage(size, max_reserved, packed_.growth_policy, pool.enabled(), request, plan));
            XOFFSET_VTRY(as_arena_failure([&]() -> detail::vstatus {
                free_table_lost_ = false;
                detail::VirtualMemoryStorage owner;
                if (packed_.growth_policy == XGrowPolicy::Relocating) {
                    if (!relocating_owner(owner, size, max_reserved, std::max(size, reserve_or_default(reserve, size))))
                        XOFFSET_VFAIL_BAD_ALLOC();
                    if (!owner.grow_to(size)) XOFFSET_VFAIL_BAD_ALLOC();
                } else if (plans_slot(plan)) {
                    XOFFSET_VTRY(detail::VirtualMemoryStorage::for_slot_v(owner, plan.slot_class, max_reserved,
                                                                          plan.movable));
                    if (!owner.grow_to(size)) XOFFSET_VFAIL_BAD_ALLOC();
                } else {
                    XOFFSET_VTRY(detail::VirtualMemoryStorage::create_v(owner, size, max_reserved, pool));
                }
                buffer_ = std::move(owner);
                forget_reset_dirt();
                return detail::vok();
            }));
            write_empty_header();
            publish_host_owner();
            return detail::vok();
        }

        detail::vstatus init_from_bytes(const char* data, size_type size, size_type max_reserved,
                                        const XBufferPool& pool = {}, XStorage request = XStorage::Auto,
                                        size_type reserve = 0) {
            StoragePlan plan;
            XOFFSET_VTRY(plan_storage(size, max_reserved, packed_.growth_policy, pool.enabled(), request, plan));
            return init_from_storage(data, size, max_reserved, [&](detail::VirtualMemoryStorage& owner) -> detail::vstatus {
                if (packed_.growth_policy == XGrowPolicy::Relocating) {
                    if (!relocating_owner(owner, size, max_reserved, std::max(size, reserve_or_default(reserve, size))))
                        XOFFSET_VFAIL_BAD_ALLOC();
                    return owner.assign_bytes_v(data, size);
                }
                if (!plans_slot(plan) || size > max_reserved)
                    return detail::VirtualMemoryStorage::from_bytes_v(owner, data, size, max_reserved, pool);
                XOFFSET_VTRY(detail::VirtualMemoryStorage::for_slot_v(owner, plan.slot_class, max_reserved,
                                                                      plan.movable));
                return owner.assign_bytes_v(data, size);
            });
        }

        // Wire payload: extents and sizes validated by the caller.
        detail::vstatus init_from_wire(const WirePayload& wire, size_type size, size_type max_reserved,
                                       const XBufferPool& pool = {}, XStorage request = XStorage::Auto,
                                       size_type reserve = 0) {
            StoragePlan plan;
            XOFFSET_VTRY(plan_storage(size, max_reserved, packed_.growth_policy, pool.enabled(), request, plan));
            return init_from_storage(wire.stored, size, max_reserved,
                                     [&](detail::VirtualMemoryStorage& owner) -> detail::vstatus {
                const char* extents = static_cast<const char*>(wire.extents) + wire.extent_count * 8u;
                if (packed_.growth_policy == XGrowPolicy::Relocating && size <= max_reserved) {
                    if (!relocating_owner(owner, size, max_reserved, std::max(size, reserve_or_default(reserve, size))))
                        XOFFSET_VFAIL_BAD_ALLOC();
                    if (wire.extent_count == 0)
                        XOFFSET_VTRY(owner.assign_bytes_v(wire.stored, size));
                    else
                        XOFFSET_VTRY(owner.assign_extents_v(extents, wire.extents, wire.extent_count, size));
                } else if (!plans_slot(plan) || size > max_reserved) {
                    if (wire.extent_count == 0)
                        XOFFSET_VTRY(detail::VirtualMemoryStorage::from_bytes_v(owner, wire.stored, size, max_reserved,
                                                                                pool));
                    else
                        XOFFSET_VTRY(detail::VirtualMemoryStorage::from_extents_v(owner, extents, wire.extents,
                                                                                  wire.extent_count, size,
                                                                                  max_reserved, pool));
                } else {
                    XOFFSET_VTRY(detail::VirtualMemoryStorage::for_slot_v(owner, plan.slot_class, max_reserved,
                                                                          plan.movable));
                    if (wire.extent_count == 0)
                        XOFFSET_VTRY(owner.assign_bytes_v(wire.stored, size));
                    else
                        XOFFSET_VTRY(owner.assign_extents_v(extents, wire.extents, wire.extent_count, size));
                }
                if (wire.extent_count != 0) copy_wire_header(owner.data(), wire.stored);
                // The image carries its free blocks: the caller's check takes
                // them (or rejects the load).
                free_table_lost_ = false;
                return detail::vok();
            });
        }

        // `make` fills an empty owner (returning detail::vstatus).
        template <typename Make>
        detail::vstatus init_from_storage(const char* data, size_type size, size_type max_reserved, Make&& make) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD)
                XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::payload_limit_message);
            if (!data || size < sizeof(arena_header)) {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload_header, "XBufferCore: payload too small");
            }
            if (size > max_reserved) XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::arena_failure_message);
            XOFFSET_VTRY(as_arena_failure([&]() -> detail::vstatus {
                // Free blocks of an unverified payload are unknown until a verified
                // load checks its free directory (or a typed walk recomputes it).
                free_table_lost_ = true;
                detail::VirtualMemoryStorage owner;
                XOFFSET_VTRY(make(owner));
                buffer_ = std::move(owner);
                forget_reset_dirt();
                return detail::vok();
            }));
            typed_root_access_allowed_ = false;
            XOFFSET_VTRY(check_reinit_payload());
            publish_host_owner();
            return detail::vok();
        }

        // Out of line: inlining the short path into every move site changes
        // register allocation in unrelated callers (GCC 16 reloc map.find).
        XOFFSET_DETAIL_NOINLINE void move_from(XBufferCore&& other) noexcept {
            if (holds_nothing()) [[likely]] {
                // Nothing to retire (a new or moved-from target, as in all
                // three moves of std::swap). Take other's storage, record,
                // free state and schema state; other keeps this empty storage
                // and gets the default state the retired path below leaves
                // it. Each epoch still advances by one.
                buffer_.swap(other.buffer_);
                owner_record_.swap(other.owner_record_);
                free_table_lost_ = std::exchange(other.free_table_lost_, false);
                touch_log_.swap(other.touch_log_);
                publish_host_owner();
                if (ctl_ || other.ctl_ || packed_.pinned || other.packed_.pinned || handoff_ || other.handoff_)
                    [[unlikely]] {
                    promote_bare_pin();
                    other.promote_bare_pin();
                    retire_ctl();
                    ctl_ = std::exchange(other.ctl_, nullptr);
                    quarantine_.swap(other.quarantine_);
                    packed_.quarantined = std::exchange(other.packed_.quarantined, std::uint8_t{0});
                    adopt_ctl();
                    other.adopt_ctl();
                    // adopt_ctl set each pin flag: the rest moves by member.
                    packed_.growth_policy = std::exchange(other.packed_.growth_policy, XGrowPolicy::Explicit);
                    packed_.reset_dirty_end = std::exchange(other.packed_.reset_dirty_end, 0u);
                } else {
                    // Neither side is pinned or has a block or a handoff:
                    // policy and reset mark move as one word and other gets
                    // the default one.
                    const auto theirs = other.packed_bits();
                    other.set_packed_bits(0);
                    set_packed_bits(theirs);
                }
                root_key_ = std::exchange(other.root_key_, nullptr);
                typed_root_access_allowed_ = std::exchange(other.typed_root_access_allowed_, true);
                advance_epoch();
                other.advance_epoch();
                update_registry_owner();
                return;
            }
            const auto target_epoch = m_epoch;
            const auto source_epoch = other.m_epoch;
            XBufferCore retired;
            // Both swaps exchange every storage/schema/free-list member and
            // publish matching record owners without releasing a host object.
            // In particular, no old VM pool or vector can call replaceable
            // operator delete halfway through the incoming owner's transfer.
            retired.swap(*this);
            swap(other);
            // Preserve the existing one-invalidation-per-Core epoch contract.
            m_epoch = target_epoch + 1u;
            other.m_epoch = source_epoch + 1u;
            // Only now destroy retired target resources. Any delete reentry
            // sees the fully published incoming payload, schema and free cache.
        }

        // No storage, slot, pool or registry record: retiring this owner
        // would release nothing.
        bool holds_nothing() const noexcept {
            return !buffer_.data() && !buffer_.is_slot() && !buffer_.pool().enabled() && !owner_record_;
        }

        void update_registry_owner() noexcept {
            if (!owner_record_) return;
            owner_record_->owner.store(this, std::memory_order_release);
            detail::owner_lookup_count(&detail::OwnerLookupStats::owner_updates);
        }

        void unregister_registry() noexcept {
            auto record = std::move(owner_record_);
            if (!record) return;
            record->owner.store(nullptr, std::memory_order_release);
            // A cache may retain the inert record, never its released mapping.
        }

    public:
        // This owner's record (OwnerRecord), made on first use: the identity
        // that the paths running a host callback (a host allocation, a
        // predicate) re-resolve after it: stable_erase_if, save_into_general,
        // save_compact_into. Creation, loads, transfers and destruction make
        // none. The one allocation, including any new_handler reentry,
        // precedes publication of the record. Throws XBadAlloc.
        OwnerRecordPtr owner_identity() {
            if (!owner_record_) [[unlikely]] {
                if (std::exchange(detail::pending_registry_failure, false)) XOFFSET_THROW(XBadAlloc());
                OwnerRecordPtr record;
                XOFFSET_TRY {
                    // From malloc: no new_handler runs, so nothing can move or
                    // release this owner while its record is made.
                    record = std::allocate_shared<OwnerRecord>(detail::MallocAllocator<OwnerRecord>{},
                                                               buffer_.data(), this);
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_THROW(XBadAlloc());
                }
                owner_record_ = std::move(record);
                detail::owner_lookup_count(&detail::OwnerLookupStats::registrations);
                detail::owner_lookup_count(&detail::OwnerLookupStats::record_object_bytes, sizeof(OwnerRecord));
            }
            return owner_record_;
        }

    private:

        // Moves this owner to new storage of at least `logical` bytes: a
        // larger slot, or a VM reservation (pinned from then on) above
        // SLOT_MAX or with `pin`. With `copy` the used bytes come along and the rest reads
        // zero; without it the caller overwrites the content (reload). The
        // free state, schema and typed state stay: this is the same lineage
        // at a new address. Prepares everything first; on failure nothing
        // changed and false is returned.
        XOFFSET_DETAIL_NOINLINE bool replace_storage(size_type logical, bool copy, bool pin = false) noexcept {
            const auto maximum = buffer_.maximum_capacity();
            if (logical > maximum) return false;
            // A pin scope holds plain references into the payload.
            if (packed_.pinned) return false;
            detail::VirtualMemoryStorage next;
            XOFFSET_TRY {
                const auto cls = pin ? detail::SlotGeometry::classes : detail::SlotGeometry::class_for(logical);
                if (packed_.growth_policy == XGrowPolicy::Relocating && !pin) {
                    if (!relocating_owner(next, logical, maximum, relocation_target(logical))) return false;
                    if (copy) XOFFSET_VTRY_OR(false, next.assign_bytes_v(buffer_.data(), used_end_clamped()));
                } else if (cls != detail::SlotGeometry::classes) {
                    XOFFSET_VTRY_OR(false, detail::VirtualMemoryStorage::for_slot_v(next, cls, maximum, true));
                    if (copy) XOFFSET_VTRY_OR(false, next.assign_bytes_v(buffer_.data(), used_end_clamped()));
                } else if (copy) {
                    XOFFSET_VTRY_OR(false, detail::VirtualMemoryStorage::from_bytes_v(
                        next, buffer_.data(), used_end_clamped(), maximum, {}));
                } else {
                    XOFFSET_VTRY_OR(false, detail::VirtualMemoryStorage::create_v(next, 0, maximum));
                }
                if (copy && !next.grow_to(logical)) return false;
            } XOFFSET_CATCH(...) {
                return false;
            }
            // The record names the old storage: it reads as released, and a
            // new one is made on its next use (owner_identity).
            unregister_registry();
            buffer_.swap(next);
            forget_reset_dirt();  // the new storage holds the used bytes and zeros
            publish_host_owner();
            advance_epoch();
            if (ctl_) {
                ctl_->base = buffer_.data();
                ctl_->version += 2;
            }
#if defined(XOFFSET_STRESS_RELOCATE)
            // Stress mode: a Relocating buffer's old storage becomes
            // inaccessible instead.
            if (packed_.growth_policy == XGrowPolicy::Relocating) detail::StressGraveyard::bury(std::move(next));
#endif
            return true;
            // `next` (the old storage) is released here.
        }

        // An owner without storage (default-constructed or moved-from) gets
        // one for a reload of `size` bytes, as load_verified would without a
        // maximum: DEFAULT_MAXIMUM_CAPACITY, this owner's growth policy.
        detail::vstatus ensure_storage(size_type size) {
            if (!buffer_.data()) return create_storage(size);
            return detail::vok();
        }

        // Cold half of ensure_storage, out of line so the inlined reinit
        // admission stays small.
        XOFFSET_DETAIL_NOINLINE detail::vstatus create_storage(size_type size) {
            if (size > DEFAULT_MAXIMUM_CAPACITY)
                XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::arena_failure_message);
            const auto initial = std::max(size, min_payload_size());
            return init_fresh(initial, DEFAULT_MAXIMUM_CAPACITY);
        }

        size_type used_end_clamped() const noexcept {
            const auto* sm = arena();
            if (!sm) return 0;
            size_type used = static_cast<size_type>(sm->used_end);
            if (used < min_payload_size()) used = min_payload_size();
            if (used > buffer_.size()) used = buffer_.size();
            return used;
        }

        char* payload_byte_address(size_type offset) noexcept { return buffer_.data() + offset; }
        const char* payload_byte_address(size_type offset) const noexcept { return buffer_.data() + offset; }

        template <typename T>
        T* object_at_offset(std::uint64_t offset) noexcept {
            if (offset == 0 || offset > buffer_.size() ||
                sizeof(T) > buffer_.size() - offset) {
                return nullptr;
            }
            auto* raw = payload_byte_address(static_cast<size_type>(offset));
            if (reinterpret_cast<std::uintptr_t>(raw) % alignof(T) != 0) return nullptr;
            return std::launder(reinterpret_cast<T*>(raw));
        }

        template <typename T>
        const T* object_at_offset_const(std::uint64_t offset) const noexcept {
            if (offset == 0 || offset > buffer_.size() ||
                sizeof(T) > buffer_.size() - offset) {
                return nullptr;
            }
            auto* raw = payload_byte_address(static_cast<size_type>(offset));
            if (reinterpret_cast<std::uintptr_t>(raw) % alignof(T) != 0) return nullptr;
            return std::launder(reinterpret_cast<const T*>(raw));
        }

        // Inlined into make<T>() (its one caller, through construct_root).
        // The root is the first block of an empty image, so it lands at
        // root_at: an empty image has no free block and bumps there.
        template <typename T, typename... Args>
        XOFFSET_DETAIL_INLINE T* construct_root_object(Args&&... args) {
            static_assert(alignof(T) <= 8, "the root object is at offset 40: its alignment is at most 8");
            if constexpr (sizeof(T) > MAX_PAYLOAD) XOFFSET_THROW(XBadAlloc());
            if (arena()->used_end != root_at) {
                XOFFSET_THROW(XException(XErrc::invalid_state, "XBufferCore: object slot already populated"));
            }
            const auto block = arena_header::block_size(static_cast<std::uint32_t>(sizeof(T)));
            void* raw = allocate_block(block, alignof(T));
            (void)block;
            T* obj;
            XOFFSET_TRY {
                obj = std::construct_at(reinterpret_cast<T*>(raw), std::forward<Args>(args)...);
            } XOFFSET_CATCH(...) {
                // The image is empty again (what the constructor allocated
                // goes with it), so the next root lands at root_at too.
                clear_used_payload();
                XOFFSET_RETHROW;
            }
            return obj;
        }

        static constexpr std::size_t regrow_step = 4096;
        static constexpr std::uint32_t first_dirty_step = 256;

        bool ensure_allocation_extent(size_type required, bool allow_growth = true) {
            if (required <= buffer_.size()) return true;
            if (!allow_growth || packed_.growth_policy == XGrowPolicy::Explicit || buffer_.empty()) return false;
            // Relocating: in place up to the current reservation only.
            const auto maximum = packed_.growth_policy == XGrowPolicy::Automatic
                ? buffer_.maximum_capacity() : buffer_.reservation_limit();
            if (required > maximum) return false;
            // Throw on an OS/preparation failure. In particular, a container's
            // failed tail expansion must not silently retry via a new allocation.
            if (!buffer_.grow_to(extent_target(required, maximum))) XOFFSET_THROW(XBadAlloc());
            return true;
        }

        // The logical length ensure_allocation_extent grows to for
        // `required` bytes, at most `maximum`.
        size_type extent_target(size_type required, size_type maximum) noexcept {
            const auto current = buffer_.size();
            const auto geometric = current > maximum / 2 ? maximum : current * 2;
            auto target = std::max<std::size_t>(required, geometric);
            // Below zero_from() (bytes exposed before an in-place reload
            // shrank the logical length) grow_to must memset every newly
            // exposed byte, so a doubling there clears far more than the tail
            // will use. Grow in steps inside that range; past it fresh OS
            // pages are already zero and growth is geometric again. The step
            // starts at 256 B in each epoch (after each reload or reset) and
            // doubles up to 4 KiB with every step: an owner that grows once
            // (one vector regrow in a resident player) clears a few cache
            // lines, not a cold 4 KiB page, while one that keeps growing
            // reaches the 4 KiB step after five calls. The reset is keyed on
            // the epoch here so the reload path does not pay for it. used_end,
            // and therefore the wire, does not depend on the logical length.
            if (required < buffer_.zero_from()) {
                if (dirty_epoch_ != m_epoch) [[unlikely]] {
                    dirty_epoch_ = m_epoch;
                    dirty_step_ = first_dirty_step;
                }
                target = std::max<std::size_t>(required, std::min<std::size_t>(geometric, std::size_t(current) + dirty_step_));
                if (dirty_step_ < regrow_step) dirty_step_ *= 2;
            }
            return target;
        }

        // Relocating storage: an empty owner whose reservation holds `needed`
        // bytes, aiming at `want`. A slot when one of the slab's classes
        // holds `want` (or, failing that, any class down to `needed`);
        // otherwise a VM reservation of `want`, halved down to `needed` while
        // the OS refuses the address space. Its maximum is `maximum`.
        // false (nothing taken) when nothing can hold `needed`.
        static bool relocating_owner(detail::VirtualMemoryStorage& owner, size_type needed, size_type maximum,
                                     size_type want) noexcept {
            using Geometry = detail::SlotGeometry;
            if (needed > maximum) return false;
            want = std::min(std::max(want, needed), maximum);
            if (needed <= Geometry::slab_max_payload && want <= Geometry::slab_max_payload) {
                // The smallest class holding `bytes`: a slot of 2^(10 + cls).
                constexpr auto class_of = [](size_type bytes) noexcept -> std::size_t {
                    const auto width = static_cast<std::size_t>(std::bit_width(bytes + Geometry::header_bytes - 1));
                    return width > Geometry::min_shift ? width - Geometry::min_shift : 0;
                };
                const auto low = class_of(needed);
                for (auto cls = class_of(want) + 1; cls-- > low;)
                    if (owner.take_slot(cls, maximum, true)) return true;
            }
            for (size_type reserve = want;;) {
                detail::VirtualMemoryStorage next;
                bool reserved = false;
                XOFFSET_TRY {
                    reserved = [&]() -> bool {
                        XOFFSET_VTRY_OR(false, detail::VirtualMemoryStorage::create_v(next, 0, reserve));
                        return true;
                    }();
                } XOFFSET_CATCH(...) {
                    reserved = false;
                }
                if (reserved) {
#if XOFFSET_NO_EXCEPTIONS
                    (void)detail::take_pending_error(); // an earlier, larger attempt's failure
#endif
                    next.raise_maximum(maximum);
                    owner.swap(next);
                    return true;
                }
                if (reserve <= needed) return false;
                reserve = std::max(needed, reserve / 2);
            }
        }

        // Where a relocating buffer whose current reservation is full moves
        // for `needed` bytes: a larger slot while the slot classes below
        // RELOCATING_RESERVE grow (a buffer that started small, by
        // with_reserve), else a reservation of the maximum (RELOCATE_STEP
        // 0, servers) or RELOCATE_STEP, then doubling.
        size_type relocation_target(size_type needed) const noexcept {
            const size_type maximum = buffer_.maximum_capacity();
            const size_type current = buffer_.reservation_limit();
            if (buffer_.is_slot() && current < RELOCATING_RESERVE) {
                size_type next = current > MAX_PAYLOAD / 2 ? MAX_PAYLOAD : current * 2 + 64;
                if (next < needed) next = needed;
                if (next <= RELOCATING_RESERVE) return next;
            }
            if constexpr (RELOCATE_STEP == 0) {
                return maximum;
            } else {
                size_type next = std::max<size_type>(RELOCATE_STEP, current > MAX_PAYLOAD / 2 ? MAX_PAYLOAD : current * 2);
                while (next < needed && next <= MAX_PAYLOAD / 2) next *= 2;
                return std::min(std::max(next, needed), maximum);
            }
        }

        // MaximumCapacity::reserve as payload bytes, for a buffer that starts
        // with `bytes` of content.
        static size_type reserve_or_default(size_type reserve, size_type bytes) noexcept {
            if (reserve == 0) return relocating_first_reserve(bytes);
            return reserve > detail::SlotGeometry::header_bytes ? reserve - detail::SlotGeometry::header_bytes : 1;
        }

    protected:
        // The WirePayload constructor's body, for a default-constructed
        // buffer: the non-throwing load entries report failure instead.
        detail::vstatus init_wire_v(const WirePayload& wire, size_type size, MaximumCapacity cap) {
            packed_.growth_policy = cap.growth;
            return init_from_wire(wire, size, cap.value, cap.pool, cap.storage, cap.reserve);
        }

        // In-place reload/reset (XBuffer::reload_verified / reset). The
        // mapping, the payload address and therefore the owner registration
        // are kept; every typed pointer into the old content is invalidated
        // (epoch), and the owner is left without typed root access until the
        // caller finishes verification.
        XOFFSET_DETAIL_INLINE void begin_reinit() noexcept {
            if (ctl_) [[unlikely]] end_lineage();
            advance_epoch();
            free_table_lost_ = false;
            typed_root_access_allowed_ = false;
            root_key_ = nullptr;
        }

        // XBuffer::reset<T>() of an owner whose root key is T's: the image
        // becomes its header and T's root block at root_at (`root_block`
        // bytes, block-sized), which the caller constructs empty. The old
        // content held that root, so the logical length holds the block
        // (used_end >= root_at + root_block); the root key, typed access and
        // the fingerprint words stay. What the old content left past the
        // root block is remembered as clear_used_payload remembers it.
        // Returns the root block. On a failed construction the caller calls
        // forget_root().
        template <bool MayHaveBlock = true>
        XOFFSET_DETAIL_INLINE char* reset_keeping_root(std::uint32_t root_block) noexcept {
            if constexpr (MayHaveBlock) {
                if (ctl_) [[unlikely]] end_lineage();
            }
            advance_epoch();
            free_table_lost_ = false;
            char* const base = buffer_.data();
            arena_header* const sm = arena_unchecked();
            const std::uint32_t used = sm->used_end;
            if constexpr (detail::scrub_reset) {
                std::memset(base + root_at, 0, used - root_at);
            } else {
                if (used > packed_.reset_dirty_end) packed_.reset_dirty_end = used;
#if defined(XOFFSET_STRESS_RELOCATE)
                std::memset(base + root_at, detail::stress_poison_byte, used - root_at);
#endif
            }
            // The magic word stays: the old content is an image.
            sm->used_end = static_cast<std::uint32_t>(root_at) + root_block;
            sm->extent_count = 0;
            sm->free_dir = 0;
            sm->free_bytes = 0;
            return base + root_at;
        }
        // reset_keeping_root's root did not construct: the image is empty
        // and has no root.
        void forget_root() noexcept {
            root_key_ = nullptr;
            typed_root_access_allowed_ = false;
            clear_used_payload();
        }

        // Same admission as init_from_storage, against this owner's maximum.
        // The two halves are forced inline so that reload_verified compiles
        // as it did before begin_verified_reload shared them.
        template <typename Assign>
        detail::vstatus reinit_from_storage(const char* data, size_type size, Assign&& assign) {
            if (shrinks_by_page(size)) [[unlikely]] return reinit_from_storage_shrinking(data, size, assign);
            XOFFSET_VTRY(admit_reinit(size, data != nullptr));
            XOFFSET_VTRY(as_arena_failure(assign));
            return check_reinit_payload();
        }
        template <typename Assign>
        XOFFSET_DETAIL_NOINLINE detail::vstatus reinit_from_storage_shrinking(const char* data, size_type size,
                                                                             Assign& assign) {
            const ZeroTail tail = settled_zero_tail();
            XOFFSET_VTRY(admit_reinit(size, data != nullptr));
            XOFFSET_VTRY(as_arena_failure(assign));
            note_reinit_zero_tail(tail, size);
            return check_reinit_payload();
        }

        // In-place loads and the zero watermark. On a settled owner (exact
        // free state, nothing dirty past the logical length) the zero tail invariant makes every
        // byte in [tail_clean_from(), logical) zero, so after an in-place load
        // of `size` bytes into the same storage only [size, that offset) can
        // be dirty.
        // Without this, the watermark stays at the old logical length and
        // later growth clears bytes that are already zero (the 4 KiB regrow
        // steps and the geometric slack of the previous content).
        struct ZeroTail {
            const char* base = nullptr;
            size_type used_end = 0;
        };
        // Only a load that shrinks the logical length by at least a page can
        // lower the watermark by enough to matter; small reloads (messages of
        // similar size) leave after one comparison.
        static constexpr std::size_t zero_tail_min_gain = 4096;
        // The common case (a reload of similar size) costs this one comparison;
        // a shrinking load takes an out-of-line copy of the reinit that also
        // lowers the watermark, so nothing of it is if-converted into every
        // reload.
        XOFFSET_DETAIL_INLINE bool shrinks_by_page(size_type size) const noexcept {
            return std::size_t(size) + zero_tail_min_gain <= buffer_.size();
        }
        ZeroTail settled_zero_tail() const noexcept {
            const auto logical = buffer_.size();
            if (buffer_.zero_from() != logical || free_table_lost_ || logical < min_payload_size()) return {};
            const auto used = tail_clean_from();
            return {buffer_.data(), used < logical ? used : logical};
        }
        void note_reinit_zero_tail(const ZeroTail& tail, size_type size) noexcept {
            // A load ends at its logical length: what a reset left dirty is
            // overwritten or lies past it, where the storage's own watermark
            // (zero_from) clears it on growth. Forgotten here, on the two
            // out-of-line shrinking loads, and not on every reload: a mark
            // that outlives a reload only keeps this function from lowering
            // the watermark once.
            forget_reset_dirt();
            if (tail.base && tail.base == buffer_.data())
                buffer_.note_zero_from(std::max<std::size_t>(size, tail.used_end));
        }

        XOFFSET_DETAIL_INLINE detail::vstatus admit_reinit(size_type size, bool has_source = true) {
            if (has_source && size >= sizeof(arena_header) && size <= MAX_PAYLOAD) XOFFSET_VTRY(ensure_storage(size));
            if (buffer_.empty() && buffer_.maximum_capacity() == 0)
                XOFFSET_VFAIL(XException, XErrc::invalid_state, "XBufferCore: buffer has no storage");
            begin_reinit();
            if (size > MAX_PAYLOAD) XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::arena_failure_message);
            if (!has_source || size < sizeof(arena_header)) {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload_header, "XBufferCore: payload too small");
            }
            if (size > buffer_.maximum_capacity())
                XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, detail::arena_failure_message);
            // A movable slot too small for the new content moves first; the
            // old content is discarded anyway, so nothing is copied.
            if ((buffer_.is_slot() ? size > buffer_.accessible_bytes()
                                   : packed_.growth_policy == XGrowPolicy::Relocating && size > buffer_.reservation_limit()) &&
                !replace_storage(size, false))
                XOFFSET_VFAIL(XBadAlloc, XErrc::out_of_storage, detail::arena_failure_message);
            // Free blocks are unknown until verification checks the free directory.
            free_table_lost_ = true;
            return detail::vok();
        }

        XOFFSET_DETAIL_INLINE detail::vstatus check_reinit_payload() {
            auto* sm = arena();
            if (sm->magic != image_magic) {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload_header, "XBufferCore: payload header mismatch");
            }
            if (sm->used_end < min_payload_size() || sm->used_end > buffer_.size()) {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload_header, "XBufferCore: payload used_end out of range");
            }
            // Every image's used_end is 8-aligned (blocks are multiples of 8),
            // so a bump needs no alignment test (try_bump_first).
            if ((sm->used_end & 7u) != 0) {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload_header, "XBufferCore: payload used_end not 8-aligned");
            }
            // The free blocks of loaded bytes are unknown until a verified
            // load (or a clone's source) supplies them: no directory.
            if (free_table_lost_) { sm->free_dir = 0; sm->free_bytes = 0; }
            zero_past_used_end();
            return detail::vok();
        }

        detail::vstatus reinit_from_bytes(const char* data, size_type size) {
            return reinit_from_storage(data, size, [&] { return buffer_.assign_bytes_v(data, size); });
        }

        // A wire written into the storage in place (a direct reload) carries
        // its free blocks: the caller's check takes them.
        void take_loaded_free_state() noexcept { free_table_lost_ = false; }

        // The header of a sparse wire (its extent count names the form; an
        // image is dense).
        static void copy_wire_header(char* base, const char* header) noexcept {
            std::memcpy(base, header, sizeof(arena_header));
            std::launder(reinterpret_cast<arena_header*>(base))->extent_count = 0;
        }

        detail::vstatus reinit_from_wire(const WirePayload& wire, size_type size) {
            return reinit_from_storage(wire.stored, size, [&]() -> detail::vstatus {
                if (wire.extent_count == 0) {
                    XOFFSET_VTRY(buffer_.assign_bytes_v(wire.stored, size));
                } else {
                    XOFFSET_VTRY(buffer_.assign_extents_v(static_cast<const char*>(wire.extents) + wire.extent_count * 8u,
                                                          wire.extents, wire.extent_count, size));
                    copy_wire_header(buffer_.data(), wire.stored);
                }
                // The image carries its free blocks: the caller's check takes
                // them (or rejects the load).
                free_table_lost_ = false;
                return detail::vok();
            });
        }

        // A reload of a dense image of `size` bytes (its header checked by
        // the caller: used_end == size) in place, the case of each message of
        // a stream: the storage's accessible prefix holds it and it shrinks
        // the logical length by less than a page. Anything else takes
        // reinit_from_wire.
        XOFFSET_DETAIL_INLINE bool can_reload_image_in_place(size_type size) const noexcept {
            // size >= sizeof(arena_header) (the caller's header check).
            return buffer_.holds_in_place(size) && !shrinks_by_page(size);
        }
        // The copy of such a reload (can_reload_image_in_place held): every
        // admission check of admit_reinit holds, and check_reinit_payload()
        // by construction (nothing lies past used_end to zero), so only the
        // epoch and the image change. The free state is the image's. Typed
        // access and the root key are left as they are: the
        // caller publishes the new root (at root_at, where a root of the same
        // type already was) or, on any failure, resets the arena, which
        // clears them. False: an injected storage failure, nothing copied.
        XOFFSET_DETAIL_INLINE bool reload_image_in_place(const char* image, size_type size) noexcept {
            if (ctl_) [[unlikely]] end_lineage();
            ++m_epoch;
            free_table_lost_ = false;
            return buffer_.assign_in_place(image, size, 0);
        }

        // In-place direct receive (XBuffer::begin_verified_reload): the
        // admission of reinit, then [0, size) exposed to the writer without
        // clearing. Sets `clean_from` to the offset from which bytes are
        // still zero.
        detail::vstatus reinit_for_overwrite(size_type size, std::size_t& clean_from) {
            XOFFSET_VTRY(admit_reinit(size));
            return as_arena_alloc_failure([&] { return buffer_.expose_for_overwrite_v(size, clean_from); });
        }

        // clone()/clone_into(): the host state that travels with a payload
        // copied from a live buffer of this process. The logical size grows
        // to src's (at most this owner's maximum; the tail reads as zero),
        // the free state is copied (an incomplete one stays incomplete),
        // and so are typed root access and the root/schema binding, because
        // the bytes are exactly src's: nothing needs verifying. Throws
        // XBadAlloc; on a throw this owner keeps the state the payload copy
        // left it in.
        void copy_clone_state(const XBufferCore& src) {
            const auto logical = std::min(src.buffer_.size(), buffer_.maximum_capacity());
            if (logical > buffer_.size() && !buffer_.grow_to(logical) &&
                !((buffer_.movable() || packed_.growth_policy == XGrowPolicy::Relocating) && replace_storage(logical, true)))
                XOFFSET_THROW(XBadAlloc());
            // The payload copy holds src's free blocks; its header words
            // (cleared by the load) are src's again.
            if (!src.free_table_lost_) {
                arena()->free_dir = src.arena()->free_dir;
                arena()->free_bytes = src.arena()->free_bytes;
                inband_log_lost();
                free_table_lost_ = false;
            }
            typed_root_access_allowed_ = src.typed_root_access_allowed_;
            root_key_ = src.root_key_;
        }

        // clone_into(): src's used payload replaces this owner's content in
        // place, like reload_verified (same mapping, address and registration;
        // epoch advances). On any failure this owner is left an empty arena.
        XOFFSET_DETAIL_INLINE void clone_state_from(const XBufferCore& src) {
            if (clone_in_place(src)) [[likely]] return;
            clone_state_general(src);
        }

    private:
        // The common clone_into: this owner's logical length is already src's
        // (two buffers made alike) and src's free state is exact. One copy of
        // src's image, whose header carries the free state; src's typed state
        // and root key come along, and what the old content left past the
        // image is cleared. False: anything else (clone_state_general).
        XOFFSET_DETAIL_INLINE bool clone_in_place(const XBufferCore& src) {
            const size_type logical = buffer_.size();
            if (this == &src || src.buffer_.size() != logical || src.free_table_lost_ || logical < min_payload_size())
                return false;
            // src.size() >= 40: its header is there, and used_end <= size().
            const size_type used = src.arena()->used_end;
            const size_type dirty = tail_clean_from();
            if (ctl_) [[unlikely]] end_lineage();
            advance_epoch();
            if (!buffer_.overwrite_in_place(src.buffer_.data(), used)) [[unlikely]] {
                reset_arena(0);
                XOFFSET_THROW(XBadAlloc());
            }
            // A clone is src byte for byte, past used_end too (its later
            // blocks take zero bytes there, as src's do): what the old
            // content left past the image is cleared.
            if (dirty > used) [[unlikely]] {
                std::memset(buffer_.data() + used, 0, dirty - used);
                forget_reset_dirt();
            }
            // A touch log describes the epoch it was made in: the new epoch
            // starts it over at its next use.
            free_table_lost_ = false;
            typed_root_access_allowed_ = src.typed_root_access_allowed_;
            root_key_ = src.root_key_;
            return true;
        }
        XOFFSET_DETAIL_NOINLINE void clone_state_general(const XBufferCore& src) {
            if (this == &src) XOFFSET_THROW(XException(XErrc::invalid_state, "clone_into(): source and destination are the same buffer"));
            XOFFSET_TRY {
                const auto* sm = src.arena();
                if (!sm) XOFFSET_THROW(XException(XErrc::invalid_state, "clone_into(): source buffer has no arena"));
                XOFFSET_VREQUIRE(reinit_from_bytes(reinterpret_cast<const char*>(sm),
                                                   static_cast<size_type>(sm->used_end)));
                copy_clone_state(src);
            } XOFFSET_CATCH(...) {
                reset_arena(0);
                XOFFSET_RETHROW;
            }
        }

    protected:
        // clone(): a new owner with src's maximum capacity, growth policy,
        // pool and kind of storage: a movable slot onto a movable slot, a VM
        // owner onto VM, a pinned slot onto a pinned slot (Auto picks one,
        // since its maximum fits a slot).
        MaximumCapacity clone_capacity() const noexcept {
            if (packed_.growth_policy == XGrowPolicy::Relocating)
                return MaximumCapacity{buffer_.maximum_capacity(), packed_.growth_policy, {}, XStorage::Auto, 0};
            const auto storage = buffer_.movable() ? XStorage::Movable
                                 : buffer_.is_slot() ? XStorage::Auto
                                                     : XStorage::Pinned;
            return MaximumCapacity{buffer_.maximum_capacity(), packed_.growth_policy, buffer_.pool(), storage};
        }

        // Empty arena: header only, no root, no free block.
        // Zero tail invariant: every logical byte at or past tail_clean_from()
        // is zero (growth zeroes what it exposes, used_end only rises until a
        // reset, and loads zero the bytes past the loaded used_end). With
        // XOFFSET_SCRUB_RESET that offset is used_end: a reset that keeps the
        // logical length clears exactly [header, used_end), live blocks and
        // holes alike. Without it the reset clears nothing and remembers how
        // far the discarded content reached (packed_.reset_dirty_end): the
        // bytes in [used_end, packed_.reset_dirty_end) are the old content's
        // until the new content's blocks take them. When the logical length changes, bytes
        // above the header become dirty-below-zero_from_ and are zeroed by
        // the regrowth to `logical` (or by any later growth). noexcept: if
        // the storage cannot hold a header the owner simply stays without one.
        void reset_arena(size_type logical) noexcept {
            begin_reinit();
            // size() <= maximum_capacity(): the common reset (same logical
            // length) reads nothing past the owner's first line.
            if (logical == buffer_.size() && logical >= min_payload_size()) [[likely]] {
                clear_used_payload();
                return;
            }
            if (buffer_.maximum_capacity() < min_payload_size()) return;
            if (buffer_.size() < min_payload_size() && !buffer_.grow_to(min_payload_size())) return;
            if (buffer_.size() > min_payload_size()) buffer_.resize_logical(min_payload_size());
            forget_reset_dirt();  // all of it now lies past the logical length
            write_empty_header();
            if (logical > min_payload_size()) (void)buffer_.grow_to(std::min(logical, buffer_.maximum_capacity()));
        }

        XOFFSET_DETAIL_INLINE void clear_used_payload() noexcept {
            const auto used = used_end_clamped();
            if constexpr (detail::scrub_reset) {
                std::memset(buffer_.data() + min_payload_size(), 0, used - min_payload_size());
            } else {
                if (used > packed_.reset_dirty_end) packed_.reset_dirty_end = static_cast<std::uint32_t>(used);
#if defined(XOFFSET_STRESS_RELOCATE)
                // Stress mode: nothing may count on a block from the tail being
                // zero, so what the reset leaves is made unmistakable.
                std::memset(buffer_.data() + min_payload_size(), detail::stress_poison_byte, used - min_payload_size());
#endif
            }
            write_empty_header();
        }

        // The offset from which every logical byte is zero: used_end,
        // or the end of what a reset without XOFFSET_SCRUB_RESET left behind.
        size_type tail_clean_from() const noexcept {
            const auto used = static_cast<size_type>(arena()->used_end);
            if constexpr (detail::scrub_reset) return used;
            else return used > packed_.reset_dirty_end ? used : size_type(packed_.reset_dirty_end);
        }
        // The storage was replaced, or everything a reset left lies past the
        // logical length (the storage's zero_from covers it from there).
        void forget_reset_dirt() noexcept {
            if constexpr (!detail::scrub_reset) packed_.reset_dirty_end = 0;
        }

        // An empty image: no root, no free block. The fingerprint words
        // are left as they are (a make<T>() writes them).
        void write_empty_header() noexcept {
            auto* sm = arena();
            sm->magic = image_magic;
            sm->used_end = root_at;
            sm->extent_count = 0;
            sm->free_dir = 0;
            sm->free_bytes = 0;
        }

        // The bytes past a loaded payload's used_end are zeroed, so the
        // invariant above holds after every load. A verified load has none
        // (used_end == used_bytes); an unverified one may carry any bytes there.
        void zero_past_used_end() noexcept {
            const auto used = static_cast<size_type>(arena()->used_end);
            if (used < buffer_.size()) std::memset(buffer_.data() + used, 0, buffer_.size() - used);
        }

        void advance_epoch() noexcept { ++m_epoch; }

    private:
        friend struct detail::VerifiedPathsAccess;
        friend struct detail::DeltaAccess;
        friend struct fixed_detail::StableEraseOwnerWitness;
        // The owner is 64-byte aligned and its first line holds everything
        // the per-message paths touch: the flags, the epoch, the root key
        // (root<T>()'s fast path) and the storage's payload base and size
        // (VirtualMemoryStorage's first two fields, at 24 and 32). root<T>()
        // and the update that follows it, reset<T>()
        // and the allocations of a fresh message find all of it there.
        // Growth (maximum capacity, zero watermark) and the owner record
        // follow. Fields of different access keep this declaration order.
        alignas(64) bool free_table_lost_ = false;

    protected:
        // XBuffer's flag for an open direct reload (VerifiedReloadToken),
        // kept here in padding so that XBuffer adds only its two pointers
        // (see the static_assert after XBuffer).
        bool direct_reload_open_ = false;
        // Storage-associated access/schema state belongs to the owner so even
        // transfers through XBufferCore cannot leave derived flags behind.
        bool typed_root_access_allowed_ = true;

    private:
        // Next growth step below zero_from() (ensure_allocation_extent).
        std::uint32_t dirty_step_ = first_dirty_step;
        std::uint64_t m_epoch = 0;
        // The root type whose typed access the content admits (root<T>()
        // compares it and reads the root at root_at): set by make<T>(), a
        // verified load, reload or delta of T, reset<T>() and a clone (the
        // source's); null for an empty or unverified content. Set only with
        // typed_root_access_allowed_, a root of the type at root_at and, for
        // a type with a schema name, its fingerprint in the image header.
        // Kept across the epochs that keep the type (reload, reset, clone).
        const RootKey* root_key_ = nullptr;
        detail::VirtualMemoryStorage buffer_;
        OwnerRecordPtr owner_record_;

    private:
        uint64_t dirty_epoch_ = 0;  // m_epoch when dirty_step_ was last reset
        // A delta sender's touch marks (detail::DeltaTouchLog), made by the
        // first delta from this buffer that pairs with a baseline; the
        // in-band allocator records its writes in it. It follows the
        // content (move, swap). It fills the padding before packed_.
        std::unique_ptr<detail::DeltaTouchLog> touch_log_;
        // Three small members in one 8-byte word, so that a move or a swap
        // carries all of them with one load and one store per side (what the
        // policy byte alone took). It starts the line of ctl_, which a reset
        // and a reload read anyway. No padding bytes: the word is copied as
        // one integer (take_packed, swap).
        struct Packed {
            XGrowPolicy growth_policy = XGrowPolicy::Explicit;
            // An open pin scope (mirrors ctl_->borrow, or a bare scope).
            bool pinned = false;
            // ctl_ or handoff_ is set: edit's fast entry and a bare scope's
            // close read it with the pin flag (sync_blocked keeps it).
            std::uint8_t blocked = 0;
            // The quarantine holds blocks: a bare scope's close releases them.
            std::uint8_t quarantined = 0;
            // Bytes in [used_end, reset_dirty_end) may be nonzero: the content
            // a reset discarded without clearing (always 0 with
            // XOFFSET_SCRUB_RESET).
            std::uint32_t reset_dirty_end = 0;
        };
        static_assert(sizeof(Packed) == sizeof(std::uint64_t) && std::has_unique_object_representations_v<Packed>);
        alignas(64) Packed packed_;
        // The lineage's control block, and blocks retired inside a pin scope
        // (payload offset << 32 | bytes).
        detail::XCtl* ctl_ = nullptr;
        std::vector<std::uint64_t> quarantine_;
        // Largest quarantine list (entries) kept between pin scopes: 8 KiB.
        static constexpr std::size_t quarantine_keep_max = 1024;
        // The block that took over a bare scope open on this owner (made
        // inside it: a recorded failure, a nested scope, a move of the
        // content). The scope closes on it (pin_close_bare_slow). It stays
        // with this object, not with the content.
        detail::XCtl* handoff_ = nullptr;
    };
} // namespace XOffsetDatastructure

#endif // X_OFFSET_ARENA_HPP
