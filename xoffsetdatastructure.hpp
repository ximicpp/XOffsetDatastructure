#ifndef X_OFFSET_DATA_STRUCTURE_HPP
#define X_OFFSET_DATA_STRUCTURE_HPP

#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    #define XOFFSET_ARCH_64BIT 1
#else
    #define XOFFSET_ARCH_64BIT 0
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define XOFFSET_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    #define XOFFSET_LITTLE_ENDIAN 1
#else
    #define XOFFSET_LITTLE_ENDIAN 0
#endif

#include "xoffset_schema.hpp"
#include "xoffset_admission.hpp"
#include "xoffset_fixed_string.hpp"
#include <cstddef>
#include <cstdlib>
#include <compare>
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

// Apple: load_verified, ValidationScratch and receive_raw use std::pmr from the
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
    class XException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class XBadAlloc : public XException {
    public:
        XBadAlloc() : XException("XOffset arena allocation failed") {}
        explicit XBadAlloc(const std::string& what) : XException(what) {}
    };

    template <typename T> struct XFixedVector;

    namespace detail {
        template<class T> T* begin_dynamic_array(void*, std::size_t) noexcept;
        template<class T> T* rebuild_dynamic_array(void*, std::size_t, std::uint32_t) noexcept;
        template<class T> T* begin_fresh_ordinary_outer(void*) noexcept;
        template<class T> T* begin_relocated_ordinary_outer(T*, const T&) noexcept;
        template<class T> consteval bool future_custom_lifetime_supported();
        template<class T, class Alloc> void future_default_construct(void*, const Alloc&);
        template<class T, class Src, class Alloc> void future_transfer_construct(void*, Src&&, const Alloc&);
        template<class T> void future_relocate_construct(T*, T&) noexcept;
        template<class T> consteval bool shift_relocate_supported();
        template<class T> void shift_rebase_moved(T*, std::size_t, std::int32_t) noexcept;
        template<class T> void empty_vacated_slot(T*) noexcept;
        template<class T> void zero_implicit_padding(T*) noexcept;
        template<class T> consteval bool subtree_copy_supported();
        template<class T> bool subtree_copy_assign(XFixedVector<T>&, const XFixedVector<T>&);
#if defined(XOFFSET_TEST_HOOKS)
        // T4 test hooks: a per-thread switch that sends container copy
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
        struct VerifiedReceiveAccess;
        // Registration is the only registry operation allowed to allocate.
        // One-shot thread-local injection leaves existing owners untouched.
        inline thread_local bool pending_registry_failure = false;
        inline void fail_next_registry_registration() noexcept { pending_registry_failure = true; }
        inline void clear_registry_failures() noexcept { pending_registry_failure = false; }

        // Logical host-identity diagnostics; disabled in ordinary production.
        struct OwnerLookupStats {
            std::uint64_t entries = 0, cache_hits = 0, cold_lookups = 0;
            std::uint64_t dead_records = 0, registrations = 0, owner_updates = 0;
            std::uint64_t slow_revalidations = 0, reclamation_reserves = 0;
            std::uint64_t reclamation_restarts = 0, reclamation_abandoned = 0;
            std::uint64_t record_object_bytes = 0;
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
        using arena_type = Arena;

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
                throw XException("allocator has no arena");
            }
            if (n > 0x7ffffff8u / sizeof(T)) throw XBadAlloc();
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

    struct XFixedString;
    template <typename T> struct XFixedVector;
    template <typename T> class XFixedFlatSet;
    template <typename K, typename V> class XFixedFlatMap;
    template <typename K, typename V> struct XKeyValue;

    enum class XGrowPolicy { Explicit, Automatic };
    using XBufferPool = detail::VmStoragePool;
    // Storage request of a MaximumCapacity (R1). Auto picks by the rule in
    // XBufferCore::plan_storage; Pinned is always a VM reservation (address
    // stable for life); Movable requires a slot and throws where the rule
    // cannot give one.
    enum class XStorage { Auto, Pinned, Movable };
    // Storage an owner is on. Only MovableSlot changes address, and only at
    // grow(), reload_verified, begin_verified_reload or reset.
    enum class XStorageKind { Vm, PinnedSlot, MovableSlot };
    // XStorage, XStorageKind, storage_kind() and slot storage are available.
#define XOFFSET_SLOT_STORAGE_API 1
    using XSlotStorageStats = detail::SlotStorageStats;


#if defined(_MSC_VER)
#define XOFFSET_DETAIL_MOVE_INLINE __forceinline
#define XOFFSET_DETAIL_MOVE_COLD __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define XOFFSET_DETAIL_MOVE_INLINE inline __attribute__((always_inline))
#define XOFFSET_DETAIL_MOVE_COLD __attribute__((noinline, cold))
#else
#define XOFFSET_DETAIL_MOVE_INLINE inline
#define XOFFSET_DETAIL_MOVE_COLD
#endif
// Inlining that GCC 16 does on its own in C++26 mode but not in C++23 (it
// keeps these out of line in push_back: +7% instructions on reloc.hero
// push_back_grow). Forced only for GCC below C++26: C++26 machine code is
// unchanged, and Clang inlines less around them when they are forced.
#if __cplusplus <= 202302L && defined(__GNUC__) && !defined(__clang__)
#define XOFFSET_DETAIL_PRE26_INLINE XOFFSET_DETAIL_MOVE_INLINE
#else
#define XOFFSET_DETAIL_PRE26_INLINE
#endif
#if defined(_MSC_VER)
#define XOFFSET_DETAIL_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define XOFFSET_DETAIL_NOINLINE __attribute__((noinline))
#else
#define XOFFSET_DETAIL_NOINLINE
#endif
// [[indeterminate]] (C++26) where the compiler has it: an automatic variable
// whose bytes are written before they are read does not get the erroneous-
// behaviour fill (GCC 16 zeroes it; Clang without the attribute does not).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(indeterminate)
#define XOFFSET_INDETERMINATE [[indeterminate]]
#endif
#endif
#ifndef XOFFSET_INDETERMINATE
#define XOFFSET_INDETERMINATE
#endif

    namespace fixed_detail {
        struct StableEraseOwnerWitness;
        struct allocator_ref;
    }

    class XBufferCore {
    public:
        using size_type = std::size_t;

        // V2 payload header. Blocks carry no headers: every block is 8-byte
        // aligned with a length that is a multiple of 8, and its extent is
        // derived from the owning head (see XBufferCore::allocate_block).
        struct alignas(8) arena_header {
            char magic[8];
            std::uint32_t header_size = 0;
            std::uint32_t flags = 0;
            std::uint64_t root_offset = 0;
            std::uint64_t used_end = 0;
            std::uint64_t reserved0 = 0;
            std::uint64_t reserved1 = 0;

            void* allocate_aligned(std::uint32_t bytes, std::size_t alignment) {
                return owner().allocate_block(block_size(bytes), alignment);
            }

            void* allocate_without_growth(std::uint32_t bytes, std::size_t alignment) {
                return owner().allocate_block(block_size(bytes), alignment, false);
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
        ~XBufferCore() { unregister_registry(); }

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
        };
        // Largest payload a slot holds (0: slots disabled at build time).
        static constexpr size_type SLOT_MAX = detail::SlotGeometry::max_payload;
        static MaximumCapacity maximum_capacity(size_type bytes) noexcept { return {bytes}; }
        size_type maximum_capacity() const noexcept { return buffer_.maximum_capacity(); }
        // Host-only canonical address recovery (public API kept unchanged from
        // R1+M2 for source compatibility). The exact numeric comparator uses
        // the declared flat-address ABI for search; returned pointers come
        // from the retained real placement byte[MAX] array of a receive owner.
        // W9: the library's OWN resolution (rel32 heads, free lists, headers,
        // wire validation, lifetime helpers) no longer calls this: receive data
        // is resolved by flat origin + delta exactly like ordinary loads, i.e.
        // it relies on the same disclosed implementation-defined
        // pointer-from-address behaviour, not on the receive byte array.
        static std::byte* canonical_bytes_at(const void* anchor,
                std::uintptr_t target, std::size_t bytes) noexcept {
            if (canonical_active_count_.load(std::memory_order_acquire) == 0) return nullptr;
            return canonical_bytes_at_active(anchor, target, bytes);
        }

    private:
        // Keep the inactive acquire-load/return in the small public helper.
        // The active path retains all original owner/TLS/registry semantics.
#if defined(_MSC_VER)
        __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
        __attribute__((noinline))
#endif
        static std::byte* canonical_bytes_at_active(const void* anchor,
                std::uintptr_t target, std::size_t bytes) noexcept {
            const auto address = reinterpret_cast<std::uintptr_t>(anchor);
            auto live = [](const OwnerRecordPtr& r) noexcept {
                return r && r->owner.load(std::memory_order_acquire);
            };
            auto contains_anchor = [&](const OwnerRecordPtr& r) noexcept {
                if (!live(r)) return false;
                const auto base = reinterpret_cast<std::uintptr_t>(r->payload);
                return address >= base && address - base < r->canonical_extent;
            };
            auto fits_target = [&](const OwnerRecordPtr& r) noexcept {
                if (!live(r)) return false;
                const auto base = reinterpret_cast<std::uintptr_t>(r->payload);
                return target >= base && target - base <= r->canonical_extent &&
                    bytes <= r->canonical_extent - (target - base);
            };
            auto recover = [&](const OwnerRecordPtr& r) noexcept -> std::byte* {
                if (!fits_target(r)) std::terminate();
                if (!r->canonical_origin) return nullptr; // disclosed legacy target domain
                const auto offset = target - reinterpret_cast<std::uintptr_t>(r->payload);
                return r->canonical_origin + offset;
            };
            OwnerCache* cache = owner_cache_available() ? &owner_cache() : nullptr;
            if (cache) {
                // Prefer a containing anchor for a zero-length one-past query;
                // an adjacent reservation must not replace that array's origin.
                for (std::size_t attempt = 0; attempt != 4; ++attempt) {
                    const auto slot = (cache->last + attempt) % 4u;
                    auto& r = cache->records[slot];
                    if (contains_anchor(r) && fits_target(r)) {
                        cache->last = slot; return recover(r);
                    }
                }
                // Independently stored heads may bind another C owner or an
                // ordinary owner. Address recovery does not impose root ownership.
                // A zero-length target can equal the next reservation's base.
                // If its anchor was not cached, resolve that anchor cold before
                // allowing a target-only hit to select a different array origin.
                if (bytes != 0) {
                    for (std::size_t attempt = 0; attempt != 4; ++attempt) {
                        const auto slot = (cache->last + attempt) % 4u;
                        auto& r = cache->records[slot];
                        if (fits_target(r)) { cache->last = slot; return recover(r); }
                    }
                }
            }
            OwnerRecordPtr record;
            {
                auto& state = registry_state();
                std::lock_guard lock(state.mutex);
                auto source = state.owners.upper_bound(address);
                if (source != state.owners.begin()) {
                    --source;
                    if (contains_anchor(source->second) && fits_target(source->second)) record = source->second;
                }
                if (!record) {
                    auto candidate = state.owners.upper_bound(target);
                    if (candidate != state.owners.begin()) {
                        --candidate;
                        if (fits_target(candidate->second)) record = candidate->second;
                    }
                }
            }
            if (!record) return nullptr;
            // Canonical recovery is called inside raw-owner/cache-reference
            // critical sections. Never install a cold result in TLS here:
            // releasing an evicted terminal record can invoke replaceable
            // delete and move/destroy the caller's saved Core object.
            // The live registry/owner retain this local record. Without an
            // eviction there is no internal last-owner release or callback.
            return recover(record);
        }

    public:
        XGrowPolicy growth_policy() const noexcept { return growth_policy_; }
        // Automatic growth runs inside container operations, where the payload
        // must not move: a movable slot switched to Automatic first moves to a
        // VM reservation (pinned from then on). That move invalidates pointers
        // like grow(); it throws XBadAlloc on failure and changes nothing.
        void set_growth_policy(XGrowPolicy policy) {
            if (policy == XGrowPolicy::Automatic && buffer_.movable() &&
                !replace_storage(buffer_.size(), true, true)) throw XBadAlloc();
            growth_policy_ = policy;
        }
        XStorageKind storage_kind() const noexcept {
            if (!buffer_.is_slot()) return XStorageKind::Vm;
            return buffer_.movable() ? XStorageKind::MovableSlot : XStorageKind::PinnedSlot;
        }
        // False only on a movable slot, whose payload address can change.
        bool address_stable() const noexcept { return !buffer_.movable(); }
        // Discards the pages of free slots (see xoffset_slot_storage.hpp).
        static void trim_slot_storage() noexcept { detail::SlotSlab::trim(); }
        static XSlotStorageStats slot_storage_stats() noexcept { return detail::SlotSlab::stats(); }
        detail::VmReclaimResult reclaim_tail_pages() noexcept { return buffer_.reclaim_tail(); }

        // Numeric reservation parameters are legacy minimum hints, not hard limits.
        static size_type capacity_from_hint(size_type size, size_type hint = 0) {
            if (size > MAX_PAYLOAD || hint > MAX_PAYLOAD) throw XBadAlloc();
            return std::max({size, hint, DEFAULT_MAXIMUM_CAPACITY});
        }

        static constexpr std::size_t compute_reservation(std::size_t size) {
            std::size_t r;
            if (size > MAX_RESERVE / GROWTH_HEADROOM) {
                r = MAX_RESERVE;
            } else {
                r = size * GROWTH_HEADROOM;
            }
            if (r < MIN_RESERVE) r = MIN_RESERVE;
            if (r > MAX_RESERVE) r = MAX_RESERVE;
            return r;
        }

        explicit XBufferCore(size_type size) {
            init_fresh(size, capacity_from_hint(size));
        }

        XBufferCore(size_type size, size_type max_reserved) {
            init_fresh(size, capacity_from_hint(size, max_reserved));
        }

        XBufferCore(size_type size, MaximumCapacity cap) : growth_policy_(cap.growth) {
            init_fresh(size, cap.value, cap.pool, cap.storage);
        }

        XBufferCore(const char* data, size_type size) {
            init_from_bytes(data, size, capacity_from_hint(size));
        }

        XBufferCore(const char* data, size_type size, size_type max_reserved) {
            init_from_bytes(data, size, capacity_from_hint(size, max_reserved));
        }

        XBufferCore(const char* data, size_type size, MaximumCapacity cap) : growth_policy_(cap.growth) {
            init_from_bytes(data, size, cap.value, cap.pool, cap.storage);
        }

        // The payload of an admitted wire (see XWireHeaderV2): the payload
        // bytes from offset 48, dense (extent_count 0) or as extents. The
        // payload header is written from root_offset and the size.
        struct WirePayload {
            std::uint64_t root_offset;
            const char* stored;
            const void* extents;
            std::size_t extent_count;
        };
        XBufferCore(WirePayload wire, size_type size, MaximumCapacity cap) : growth_policy_(cap.growth) {
            init_from_wire(wire, size, cap.value, cap.pool, cap.storage);
        }

        uint64_t epoch() const noexcept { return m_epoch; }

        bool grow(size_type extra_bytes) {
            if (extra_bytes == 0) return true;
            if (buffer_.empty() || extra_bytes > buffer_.maximum_capacity() - buffer_.size()) return false;
            // The allocator reads the storage's logical extent directly. No segment
            // reopen, offset rewrite, or potentially throwing allocator update follows.
            const auto target = buffer_.size() + extra_bytes;
            if (buffer_.grow_to(target)) return true;
            // A movable slot that is too small moves to a larger owner: every
            // pointer into the payload is invalidated (epoch), XHandles re-resolve.
            return buffer_.movable() && target > buffer_.accessible_bytes() &&
                replace_storage(target, true);
        }

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
        size_type segment_size() const { return buffer_.size(); }
        size_type reserved_capacity() const { return buffer_.capacity(); }
        size_type reserved_bytes() const noexcept { return buffer_.reserved_bytes(); }
        size_type accessible_bytes() const noexcept { return buffer_.accessible_bytes(); }

        void swap(XBufferCore& other) noexcept {
            if (this == &other) return;
            buffer_.swap(other.buffer_);
            owner_record_.swap(other.owner_record_);
            free_table_.swap(other.free_table_);
            std::swap(free_table_lost_, other.free_table_lost_);
            publish_host_owner();
            other.publish_host_owner();
            std::swap(growth_policy_, other.growth_policy_);
            std::swap(root_type_identity_, other.root_type_identity_);
            std::swap(typed_root_access_allowed_, other.typed_root_access_allowed_);
            std::swap(schema_bound_, other.schema_bound_);
            std::swap(bound_root_type_id_, other.bound_root_type_id_);
            std::swap(bound_schema_hash_, other.bound_schema_hash_);
            ++m_epoch;
            ++other.m_epoch;
            update_registry_owner();
            other.update_registry_owner();
        }

        friend void swap(XBufferCore& a, XBufferCore& b) noexcept { a.swap(b); }

        size_type get_free_memory() const {
            if (buffer_.empty()) return 0;
            size_type used = used_end_clamped();
            size_type tail = buffer_.size() > used ? buffer_.size() - used : 0;
            return tail + static_cast<size_type>(free_table_ ? free_table_->bytes : 0);
        }

        arena_header* arena() noexcept {
            if (buffer_.size() < sizeof(arena_header)) return nullptr;
            return std::launder(reinterpret_cast<arena_header*>(buffer_.data()));
        }

        const arena_header* arena() const noexcept {
            if (buffer_.size() < sizeof(arena_header)) return nullptr;
            return std::launder(reinterpret_cast<const arena_header*>(buffer_.data()));
        }

        template <typename T>
        T* find_root_storage() noexcept {
            return arena() ? object_at_offset<T>(root_offset_value()) : nullptr;
        }

        template <typename T>
        const T* find_root_storage() const noexcept {
            return arena() ? object_at_offset_const<T>(root_offset_value()) : nullptr;
        }

        template <typename T, typename... Args>
        T* construct_root_storage(Args&&... args) {
            T* result = construct_object_at_slot<T>(root_offset_slot(), std::forward<Args>(args)...);
            bind_root_type<T>();
            return result;
        }

        template <typename T>
        void bind_root_type() noexcept { root_type_identity_ = type_identity<std::remove_cv_t<T>>(); }

        template <typename T>
        bool root_type_matches() const noexcept { return root_type_identity_ == type_identity<std::remove_cv_t<T>>(); }

        bool root_type_known() const noexcept { return root_type_identity_ != nullptr; }

    public:
        // One entry of the V2 wire hole table: a free block of the payload.
        struct WireHole {
            std::uint32_t offset = 0;
            std::uint32_t bytes = 0;
        };
        static_assert(sizeof(WireHole) == 8);

        // The payload header of a received wire, which does not carry it:
        // six 8-byte stores (magic, header_size with flags 0, root_offset,
        // used_end, the two reserved words).
        static void write_payload_header(char* base, std::uint64_t root_offset, std::uint64_t used_end) noexcept {
            static_assert(offsetof(arena_header, header_size) == 8 && offsetof(arena_header, flags) == 12 &&
                          offsetof(arena_header, root_offset) == 16 && offsetof(arena_header, used_end) == 24 &&
                          offsetof(arena_header, reserved0) == 32 && sizeof(arena_header) == 48);
            const std::uint64_t size_and_flags = sizeof(arena_header); // little-endian: flags (high half) 0
            const std::uint64_t zero = 0;
            std::memcpy(base, payload_magic_, 8);
            std::memcpy(base + 8, &size_and_flags, 8);
            std::memcpy(base + 16, &root_offset, 8);
            std::memcpy(base + 24, &used_end, 8);
            std::memcpy(base + 32, &zero, 8);
            std::memcpy(base + 40, &zero, 8);
        }

        // Host owner of a payload base (the arena header address). V2 keeps it
        // in the host page preceding the payload; no registry or TLS lookup.
        static XBufferCore& owner_from_arena(const void* arena) {
            auto* owner = owner_from_arena_noexcept(arena);
            if (!owner) throw XException("arena owner lookup failed");
            return *owner;
        }
        static XBufferCore* owner_from_arena_noexcept(const void* arena) noexcept {
            if (!arena) return nullptr;
            return *reinterpret_cast<XBufferCore* const*>(
                static_cast<const char*>(arena) - sizeof(void*));
        }

        // ---- V2 allocator (host-side free table) ------------------------
        // `bytes` is a block size: a nonzero multiple of 8. Exact-size bins up
        // to free_small_max serve reuse in O(1); a larger free block is split
        // (best fit) before the tail grows. A block is never handed out larger
        // than requested, so every live extent equals the size its owner
        // derives from its head and passes back to retire/try_expand.
        //
        // D3: the two outcomes that need no host allocation and run no host
        // callback are decided inline: an exact-bin hit (no split remainder,
        // no map node) and a bump without padding inside the current extent.
        // Their owner cannot change, so the identity copy and re-resolution of
        // the general path are skipped. Placement is the general path's
        // (exact bin, larger bin, large block, bump); every other request
        // takes it unchanged.
        XOFFSET_DETAIL_MOVE_INLINE void* allocate_block(std::uint32_t bytes, std::size_t alignment,
                                                        bool allow_growth = true) {
            if (bytes != 0 && (bytes & 7u) == 0 && bytes <= FreeTable::small_max &&
                alignment - 1u < 8u && (alignment & (alignment - 1u)) == 0) [[likely]] {
                FreeTable* table = free_table_.get();
                if (table && table->count != 0) {
                    const auto bin = bytes / 8u - 1u;
                    auto& exact = table->bins[bin];
                    if (!exact.empty()) [[likely]] {
                        check_owner_resolves();
                        const auto offset = exact.back();
                        exact.pop_back();
                        if (exact.empty()) table->clear_nonempty(bin);
                        table->bytes -= bytes;
                        --table->count;
                        return payload_byte_address(offset);
                    }
                } else if (auto* sm = arena()) {
                    // No free block: bump_block, whose start is the current
                    // end when that address is already 8-byte aligned.
                    const size_type start = used_end_clamped();
                    if (((reinterpret_cast<std::uintptr_t>(buffer_.data()) + start) & 7u) == 0 &&
                        start <= MAX_PAYLOAD && bytes <= MAX_PAYLOAD - start &&
                        bytes <= buffer_.size() - start) [[likely]] {
                        check_owner_resolves();
                        sm->used_end = start + bytes;
                        return payload_byte_address(start);
                    }
                }
            }
            return allocate_block_general(bytes, alignment, allow_growth);
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
            if (new_bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + new_bytes)) return false;
            arena()->used_end = offset + new_bytes;
            return true;
        }

        // ptr/bytes must name exactly one live block of this arena, or a run
        // of adjacent live blocks released together (T4 subtree retirement):
        // blocks have no headers and the table records only holes.
        // D3: a small block entering a bin that already has spare capacity
        // needs no host allocation and runs no host callback; it is published
        // inline. Everything else takes retire_block_general unchanged.
        XOFFSET_DETAIL_MOVE_INLINE void retire_block(void* ptr, std::uint32_t bytes) noexcept {
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            FreeTable* table = free_table_.get();
            if (table && ptr && bytes != 0 && (bytes & 7u) == 0 && bytes <= FreeTable::small_max &&
                address >= base && address - base < buffer_.size() &&
                bytes <= buffer_.size() - (address - base)) [[likely]] {
                const auto bin = bytes / 8u - 1u;
                auto& slot = table->bins[bin];
                if (slot.size() < slot.capacity()) [[likely]] {
                    check_owner_resolves();
                    // M3, as in retire_block_general.
                    zero_retired(ptr, bytes);
                    slot.push_back(static_cast<std::uint32_t>(address - base));
                    table->mark_nonempty(bin);
                    table->bytes += bytes;
                    ++table->count;
                    return;
                }
            }
            retire_block_general(ptr, bytes);
        }

        bool free_table_complete() const noexcept { return !free_table_lost_; }
        // Releases this thread's cached empty free tables, so the next table
        // is a fresh host allocation (allocation-failure tests use this).
        static void trim_free_table_cache() noexcept {
            if (!free_table_cache_available()) return;
            auto& cache = free_table_cache();
            while (cache.count) delete cache.tables[--cache.count];
        }
        std::size_t free_block_count() const noexcept { return free_table_ ? free_table_->count : 0; }
        std::uint64_t free_block_bytes() const noexcept { return free_table_ ? free_table_->bytes : 0; }

        // Free blocks in canonical emission order: ascending bin size, LIFO
        // vector order within a bin, then large blocks in (size, insertion)
        // order. load_holes() re-inserts in this order, so write-read-write
        // reproduces identical bytes.
        template <typename Fn>
        void for_each_free_block(Fn&& fn) const {
            if (!free_table_) return;
            const auto& table = *free_table_;
            // Empty bins emit nothing, so visiting only nonempty bins in
            // ascending order keeps the canonical order.
            for (std::uint32_t word = 0; word < FreeTable::small_bins / 64u; ++word) {
                for (auto bits = table.nonempty[word]; bits != 0; bits &= bits - 1u) {
                    const auto bin = word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits));
                    const auto bytes = (bin + 1u) * 8u;
                    for (auto offset : table.bins[bin]) fn(WireHole{offset, bytes});
                }
            }
            for (const auto& [bytes, offset] : table.large) fn(WireHole{offset, bytes});
        }

        // Visits only free blocks of at least min_bytes (bins from min_bytes
        // upward, then large blocks), in no particular offset order.
        template <typename Fn>
        void for_each_free_block_at_least(std::uint32_t min_bytes, Fn&& fn) const {
            if (!free_table_) return;
            const auto& table = *free_table_;
            const std::uint32_t first = min_bytes <= 8u ? 0u : (min_bytes + 7u) / 8u - 1u;
            for (std::uint32_t word = first / 64u; word < FreeTable::small_bins / 64u; ++word) {
                auto bits = table.nonempty[word];
                if (word == first / 64u) bits &= ~std::uint64_t(0) << (first % 64u);
                for (; bits != 0; bits &= bits - 1u) {
                    const auto bin = word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits));
                    const auto bytes = (bin + 1u) * 8u;
                    for (auto offset : table.bins[bin]) fn(WireHole{offset, bytes});
                }
            }
            for (const auto& [bytes, offset] : table.large) fn(WireHole{offset, bytes});
        }

        // Writes free_block_count() little-endian entries.
        void write_free_blocks(char* out) const noexcept {
            for_each_free_block([&](WireHole hole) {
                std::memcpy(out, &hole, sizeof(hole));
                out += sizeof(hole);
            });
        }

        // Replaces the free table with validated holes (wire order).
        // `holes` need not be aligned (it may point into wire bytes). The new
        // table is built off to the side (host allocation may run
        // new_handler); the payload's current owner is re-resolved and the
        // old table is destroyed only after publication.
        XOFFSET_DETAIL_MOVE_INLINE void load_free_blocks(const void* holes, std::size_t count) {
            // No holes, no table to replace, this owner current: the general
            // path would only publish an absent table.
            if (count == 0 && !free_table_ && (!owner_record_ || record_owner(owner_record_) == this)) [[likely]] {
                free_table_lost_ = false;
                return;
            }
            load_free_blocks_general(holes, count);
        }

        XOFFSET_DETAIL_NOINLINE void load_free_blocks_general(const void* holes, std::size_t count) {
            const auto identity = owner_record_;
            FreeTablePtr table; // no holes: an absent table is an exact, empty one
            try {
                if (count != 0) table = make_free_table();
                for (std::size_t i = 0; i < count; ++i) {
                    WireHole hole;
                    std::memcpy(&hole, static_cast<const char*>(holes) + i * sizeof(hole), sizeof(hole));
                    if (hole.bytes <= FreeTable::small_max) {
                        const auto bin = hole.bytes / 8u - 1u;
                        auto& slot = table->bins[bin];
                        if (slot.size() == slot.capacity()) [[unlikely]] table->grown = true;
                        slot.push_back(hole.offset);
                        table->mark_nonempty(bin);
                        table->touched[bin / 64u] |= std::uint64_t(1) << (bin % 64u);
                    } else {
                        table->large.emplace(hole.bytes, hole.offset);
                    }
                    table->bytes += hole.bytes;
                    ++table->count;
                }
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            }
            auto* owner = identity ? record_owner(identity) : this;
            if (!owner) throw XException("arena owner lookup failed");
            owner->free_table_.swap(table);
            owner->free_table_lost_ = false;
        }

        // Replaces the free table with an exact recomputation (used after an
        // unverified load or when a host allocation failure lost an entry).
        void replace_free_blocks(const std::vector<WireHole>& holes) {
            load_free_blocks(holes.data(), holes.size());
        }

    private:
        struct FreeTable {
            static constexpr std::uint32_t small_max = 4096;
            static constexpr std::uint32_t small_bins = small_max / 8;
            std::vector<std::uint32_t> bins[small_bins];
            std::uint64_t nonempty[small_bins / 64] = {};
            // Bit w is set exactly when nonempty[w] != 0, so a search for a
            // larger bin skips empty words without loading them.
            std::uint32_t nonempty_words = 0;
            // Bins that may own storage (set whenever a bin gets storage,
            // never cleared by removal), so recycling visits only those bins.
            std::uint64_t touched[small_bins / 64] = {};
            std::multimap<std::uint32_t, std::uint32_t> large;
            std::uint64_t bytes = 0;
            std::size_t count = 0;
            // RL: false means no bin storage has grown since the last full
            // recycle (or construction), so what is kept is still within the
            // keep limits and a keep-recycle only empties the nonempty bins.
            // Every bin growth (load_free_blocks, insert_free_block,
            // copy_free_table) sets it.
            bool grown = false;

            void mark_nonempty(std::uint32_t bin) noexcept {
                nonempty[bin / 64u] |= std::uint64_t(1) << (bin % 64u);
                nonempty_words |= 1u << (bin / 64u);
            }
            void clear_nonempty(std::uint32_t bin) noexcept {
                auto& word = nonempty[bin / 64u];
                word &= ~(std::uint64_t(1) << (bin % 64u));
                if (word == 0) nonempty_words &= ~(1u << (bin / 64u));
            }
        };

        // A table is ~12 KiB of bin headers. Constructing, zeroing and
        // releasing one per load dominated small-message loads, so released
        // tables are emptied (bin and map storage returned to the host
        // allocator) and kept in a small per-thread cache. D2: a table
        // released by an in-place reload/reset keeps the storage of its small
        // bins (at most keep_bin_entries entries each, keep_bytes per table),
        // so the first retires after the reload, which usually take the same
        // table back, do not allocate it again. A cached table differs from a
        // freshly constructed one only in bin capacity, which no allocation
        // decision, hole table or wire byte depends on.
        static bool& free_table_cache_available() noexcept {
#if XOFFSET_THREAD_CACHES
            static thread_local bool available = true; // trivial: outlives the cache
#else
            static bool available = false; // never set: see XOFFSET_THREAD_CACHES
#endif
            return available;
        }

        struct FreeTableCache {
            static constexpr std::size_t capacity = 4;
            FreeTable* tables[capacity] = {};
            std::size_t count = 0;
            ~FreeTableCache() {
                free_table_cache_available() = false;
                while (count) delete tables[--count];
            }
        };

        static FreeTableCache& free_table_cache() noexcept {
            static thread_local FreeTableCache cache;
            return cache;
        }

        struct FreeTableRecycler {
            // Small-bin storage a table released by reload/reset keeps: at
            // most 64 entries (256 B) per bin and 16 KiB per table, so at most
            // 64 KiB per thread.
            static constexpr std::size_t keep_bin_entries = 64;
            static constexpr std::size_t keep_bytes = 16u * 1024u;
            // Kept out of line: every FreeTablePtr destructor on the mutate
            // path (insert_free_block's spare) inlines only the null check.
            void operator()(FreeTable* table) const noexcept {
                if (table) recycle(table);
            }
#if defined(__clang__) || defined(__GNUC__)
            [[gnu::noinline, gnu::cold]]
#endif
            static void recycle(FreeTable* table, bool keep_bins = false) noexcept {
                if (!free_table_cache_available()) { delete table; return; }
                auto& cache = free_table_cache();
                if (cache.count == FreeTableCache::capacity) { delete table; return; }
                std::size_t kept = 0;
                if (keep_bins && !table->grown) {
                    clear_entries(*table); // also clears the word summary
                    cache.tables[cache.count++] = table;
                    return;
                }
                for (std::uint32_t word = 0; word < FreeTable::small_bins / 64u; ++word) {
                    std::uint64_t still_touched = 0;
                    for (auto bits = table->touched[word]; bits != 0; bits &= bits - 1u) {
                        auto& bin = table->bins[word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits))];
                        const auto bytes = bin.capacity() * sizeof(std::uint32_t);
                        if (keep_bins && bin.capacity() <= keep_bin_entries && bytes <= keep_bytes - kept) {
                            bin.clear(); // storage stays, so the bin stays touched
                            kept += bytes;
                            still_touched |= bits & (~bits + 1u);
                        } else {
                            std::vector<std::uint32_t>().swap(bin);
                        }
                    }
                    table->nonempty[word] = 0;
                    table->touched[word] = still_touched;
                }
                table->nonempty_words = 0;
                table->large.clear();
                table->bytes = 0;
                table->count = 0;
                table->grown = false;
                cache.tables[cache.count++] = table;
            }

            // Removes every entry but keeps the bins' storage (touched stays
            // set), so refilling the table needs no host allocation.
            static void clear_entries(FreeTable& table) noexcept {
                for (std::uint32_t word = 0; word < FreeTable::small_bins / 64u; ++word) {
                    for (auto bits = table.nonempty[word]; bits != 0; bits &= bits - 1u)
                        table.bins[word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits))].clear();
                    table.nonempty[word] = 0;
                }
                table.nonempty_words = 0;
                table.large.clear();
                table.bytes = 0;
                table.count = 0;
            }
        };
        using FreeTablePtr = std::unique_ptr<FreeTable, FreeTableRecycler>;

        // Throws std::bad_alloc only when the cache is empty and allocation fails.
#if defined(__clang__) || defined(__GNUC__)
        [[gnu::noinline, gnu::cold]]
#endif
        static FreeTablePtr make_free_table() {
            if (free_table_cache_available()) {
                auto& cache = free_table_cache();
                if (cache.count) return FreeTablePtr(cache.tables[--cache.count]);
            }
            return FreeTablePtr(new FreeTable());
        }

        // A copy of an exact table for clone(). Entries are payload offsets,
        // so they stay valid at the destination address. Bin order and the
        // large map are kept, so both buffers allocate identically afterwards.
        static FreeTablePtr copy_free_table(const FreeTable& src) {
            auto table = make_free_table();
            for (std::uint32_t word = 0; word < FreeTable::small_bins / 64u; ++word) {
                for (auto bits = src.nonempty[word]; bits != 0; bits &= bits - 1u) {
                    const auto bin = word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits));
                    table->bins[bin] = src.bins[bin];
                }
                table->nonempty[word] = src.nonempty[word];
                table->touched[word] |= src.nonempty[word];
            }
            table->nonempty_words = src.nonempty_words;
            table->large = src.large;
            table->bytes = src.bytes;
            table->count = src.count;
            table->grown = true; // bins were assigned: the next keep-recycle re-applies the limits
            return table;
        }

        // The general allocation and retirement paths (see allocate_block
        // and retire_block, which decide the common outcomes inline).
        XOFFSET_DETAIL_NOINLINE void* allocate_block_general(std::uint32_t bytes, std::size_t alignment,
                                                             bool allow_growth) {
            if (bytes == 0 || (bytes & 7u) != 0 || bytes > MAX_PAYLOAD || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > MAX_PAYLOAD) throw XBadAlloc();
            if (alignment <= 8 && free_table_) {
                FreeSplit split;
                if (void* reused = take_free_block(*free_table_, bytes, split)) {
                    finish_free_split(split);
                    return reused;
                }
            }
            return bump_block(bytes, alignment, allow_growth);
        }

        XOFFSET_DETAIL_NOINLINE void retire_block_general(void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0 || buffer_.empty()) return;
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (address < base || address - base >= buffer_.size()) return;
            const auto offset = static_cast<std::uint32_t>(address - base);
            // M3 (threat model T5/P1): a block becomes a hole only after its old
            // contents are cleared, so emitted holes need no per-save scrub.
            std::memset(ptr, 0, std::min<std::size_t>(bytes, buffer_.size() - offset));
            push_free_block(offset, bytes);
        }

        // M3 zeroing of a retired block: 8..64 bytes (most blocks) use two
        // overlapping fixed-size stores instead of a memset call.
        static XOFFSET_DETAIL_MOVE_INLINE void zero_retired(void* p, std::size_t n) noexcept {
            auto* bytes = static_cast<unsigned char*>(p);
            if (n >= 8u && n <= 64u) {
                if (n <= 16u) { std::memset(bytes, 0, 8); std::memset(bytes + n - 8u, 0, 8); }
                else if (n <= 32u) { std::memset(bytes, 0, 16); std::memset(bytes + n - 16u, 0, 16); }
                else { std::memset(bytes, 0, 32); std::memset(bytes + n - 32u, 0, 32); }
                return;
            }
            std::memset(bytes, 0, n);
        }

        // The inline paths run no host callback, so a registered Core is
        // still its record's owner (the general paths re-resolve instead).
        XOFFSET_DETAIL_MOVE_INLINE void check_owner_resolves() const noexcept {
#if !defined(NDEBUG)
            if (owner_record_ && record_owner(owner_record_) != this) std::abort();
#endif
        }

        // Records one free block. Host allocation (table, bin growth, map
        // node) may run new_handler, which may move, swap or release this
        // Core: every allocation is prepared first, then the payload's owner
        // is re-resolved through its registry record before publication, and
        // replaced host storage is destroyed only after publication. An
        // unregistered Core (receive preparation) is unreachable and is used
        // directly. On host OOM the entry is lost and the table is marked
        // incomplete (save_verified recomputes it from the live graph).
        bool insert_free_block(std::uint32_t offset, std::uint32_t bytes) noexcept {
            return insert_free_block_inline(offset, bytes) || insert_free_block_general(offset, bytes);
        }

        // A small block entering a bin that already has spare capacity needs
        // no host allocation and runs no host callback (as D3's retire_block),
        // so it is published without copying the owner record.
        bool insert_free_block_inline(std::uint32_t offset, std::uint32_t bytes) noexcept {
            FreeTable* table = free_table_.get();
            if (!table || bytes > FreeTable::small_max) return false;
            const auto bin = bytes / 8u - 1u;
            auto& slot = table->bins[bin];
            if (slot.size() == slot.capacity()) return false;
            check_owner_resolves();
            slot.push_back(offset);
            table->mark_nonempty(bin);
            table->bytes += bytes;
            ++table->count;
            return true;
        }

        XOFFSET_DETAIL_NOINLINE bool insert_free_block_general(std::uint32_t offset, std::uint32_t bytes) noexcept {
            const auto identity = owner_record_;
            FreeTablePtr spare_table;
            std::vector<std::uint32_t> spare_bin;
            // Constructed inside the try: the MSVC STL's map allocates its head
            // node in the default constructor.
            std::optional<std::multimap<std::uint32_t, std::uint32_t>> spare_large;
            try {
                if (bytes > FreeTable::small_max) spare_large.emplace().emplace(bytes, offset);
                for (;;) {
                    XBufferCore* owner = identity ? record_owner(identity) : this;
                    if (!owner) return true; // the payload was released
                    if (!owner->free_table_) {
                        if (!spare_table) {
                            spare_table = make_free_table();
                            continue; // re-resolve after the allocation
                        }
                        owner->free_table_.swap(spare_table);
                    }
                    auto& table = *owner->free_table_;
                    if (bytes > FreeTable::small_max) {
                        table.large.insert(spare_large->extract(spare_large->begin()));
                    } else {
                        const auto bin = bytes / 8u - 1u;
                        auto& slot = table.bins[bin];
                        if (slot.size() == slot.capacity()) {
                            const auto want = std::max<std::size_t>(8u, slot.capacity() * 2u);
                            if (spare_bin.capacity() < want) {
                                spare_bin.clear();
                                spare_bin.reserve(want);
                                continue; // re-resolve after the allocation
                            }
                            spare_bin.assign(slot.begin(), slot.end()); // fits: no allocation
                            table.grown = true;
                            slot.swap(spare_bin); // old storage released at scope exit
                            // Storage only arrives here, so only growth marks the bin.
                            table.touched[bin / 64u] |= std::uint64_t(1) << (bin % 64u);
                        }
                        slot.push_back(offset);
                        table.mark_nonempty(bin);
                    }
                    table.bytes += bytes;
                    ++table.count;
                    return true;
                }
            } catch (...) {
                if (auto* owner = identity ? record_owner(identity) : this) owner->free_table_lost_ = true;
                return false;
            }
        }

        void push_free_block(std::uint32_t offset, std::uint32_t bytes) noexcept {
            (void)insert_free_block(offset, bytes);
        }

        struct FreeSplit {
            std::uint32_t offset = 0, bytes = 0;
            std::multimap<std::uint32_t, std::uint32_t>::node_type node;
        };

        // Remainder publication and release of the extracted map node may run
        // host callbacks; the reused block was already removed from the table.
        // The two common outcomes run none and are published directly:
        //  - a large remainder of a large block reuses the extracted map node
        //    (a node insert lands after equal keys, as the emplace would);
        //  - a small remainder of a small block enters a bin with room.
        void finish_free_split(FreeSplit& split) {
            if (split.node) {
                if (split.bytes > FreeTable::small_max) {
                    check_owner_resolves();
                    auto& table = *free_table_;
                    split.node.key() = split.bytes;
                    split.node.mapped() = split.offset;
                    table.large.insert(std::move(split.node));
                    table.bytes += split.bytes;
                    ++table.count;
                    return;
                }
            } else if (split.bytes == 0 || insert_free_block_inline(split.offset, split.bytes)) {
                return;
            }
            const auto identity = owner_record_;
            { auto released = std::move(split.node); }
            if (split.bytes) push_free_block(split.offset, split.bytes);
            if (identity && !record_owner(identity)) throw XException("arena owner lookup failed");
        }

        void* take_free_block(FreeTable& table, std::uint32_t bytes, FreeSplit& split) noexcept {
            std::uint32_t found_offset = 0, found_bytes = 0;
            if (bytes <= FreeTable::small_max) {
                auto bin = bytes / 8u - 1u;
                auto& exact = table.bins[bin];
                if (!exact.empty()) {
                    found_offset = exact.back();
                    exact.pop_back();
                    if (exact.empty()) table.clear_nonempty(bin);
                    table.bytes -= bytes;
                    --table.count;
                    return payload_byte_address(found_offset);
                }
                // Smallest larger nonempty bin: the rest of bin's own word,
                // then the first nonempty word after it.
                const auto first = (bin + 1u) / 64u;
                std::uint64_t bits = 0;
                std::uint32_t word = first;
                if (first < FreeTable::small_bins / 64u)
                    bits = table.nonempty[first] & (~std::uint64_t(0) << ((bin + 1u) % 64u));
                if (bits == 0) {
                    const auto later = table.nonempty_words & (~0u << first << 1u);
                    if (later != 0) {
                        word = static_cast<std::uint32_t>(std::countr_zero(later));
                        bits = table.nonempty[word];
                    }
                }
                if (bits != 0) {
                    const auto larger = word * 64u + static_cast<std::uint32_t>(std::countr_zero(bits));
                    auto& source = table.bins[larger];
                    found_offset = source.back();
                    found_bytes = (larger + 1u) * 8u;
                    source.pop_back();
                    if (source.empty()) table.clear_nonempty(larger);
                }
            }
            if (found_bytes == 0) {
                auto it = table.large.lower_bound(bytes);
                if (it == table.large.end()) return nullptr;
                found_bytes = it->first;
                found_offset = it->second;
                split.node = table.large.extract(it); // released after publication
            }
            table.bytes -= found_bytes;
            --table.count;
            // Split: the remainder stays free (a multiple of 8, at least 8).
            if (found_bytes > bytes) {
                split.offset = found_offset + bytes;
                split.bytes = found_bytes - bytes;
            }
            return payload_byte_address(found_offset);
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
            if (current_end > limit - base || align - 1u > limit - base - current_end) throw XBadAlloc();
            const size_type start = align_up(base + current_end, align) - base;
            if (start > MAX_PAYLOAD || bytes > MAX_PAYLOAD - start) throw XBadAlloc();
            if (!ensure_allocation_extent(start + bytes, allow_growth)) throw XBadAlloc();
            // Publish the allocation before any host callback can observe it.
            sm->used_end = start + bytes;
            void* result = payload_byte_address(start);
            // Alignment padding (alignment > 8 only) stays an exact free block.
            if (start != current_end) {
                const auto identity = owner_record_;
                push_free_block(static_cast<std::uint32_t>(current_end),
                                static_cast<std::uint32_t>(start - current_end));
                if (identity && !record_owner(identity)) throw XException("arena owner lookup failed");
            }
            return result;
        }

        void publish_host_owner() noexcept { buffer_.set_host_owner(this); }

        static constexpr char payload_magic_[8] = {'X', 'B', 'U', 'F', 'V', '2', '\0', '\0'};

        struct AddressHash {
            std::size_t operator()(const void* pointer) const noexcept {
                // Mix page-aligned addresses instead of relying on their low bits.
                auto bits = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
                bits ^= bits >> 30;
                bits *= 0xbf58476d1ce4e5b9ULL;
                bits ^= bits >> 27;
                bits *= 0x94d049bb133111ebULL;
                return static_cast<std::size_t>(bits ^ (bits >> 31));
            }
        };

        // Host-only identity. A record follows one payload reservation, never a
        // particular Core object's address, and is never revived after release.
        // Only direct-receive (canonical) records enter the global index, the
        // sole consumer of which is canonical_bytes_at; an ordinary record is
        // just the identity lease that host-callback revalidation reads.
        inline static std::atomic<std::size_t> canonical_active_count_{0};
        struct OwnerRecord {
            const void* const payload;
            std::atomic<XBufferCore*> owner;
            std::byte* const canonical_origin;
            const std::size_t canonical_extent;
            std::atomic<bool> canonical_registered{false};
            // Written before the record is published, then read only by its owner.
            bool indexed = false;
            OwnerRecord(const void* base, XBufferCore* value,
                    std::byte* origin = nullptr, std::size_t extent = 0) noexcept
                : payload(base), owner(value), canonical_origin(origin), canonical_extent(extent) {}
        };
        // Both functions run under the existing registry mutex. No callbacks,
        // allocations or throws may separate the flag and ledger update.
        static void mark_canonical_registration(OwnerRecord& record) noexcept {
            if (record.canonical_origin && !record.canonical_registered.exchange(true, std::memory_order_relaxed))
                canonical_active_count_.fetch_add(1, std::memory_order_release);
        }
        static void retire_canonical_registration(OwnerRecord& record) noexcept {
            if (record.canonical_registered.exchange(false, std::memory_order_relaxed))
                canonical_active_count_.fetch_sub(1, std::memory_order_acq_rel);
        }
        using OwnerRecordPtr = std::shared_ptr<OwnerRecord>;
        struct NumericPayloadLess {
            using is_transparent = void;
            bool operator()(const void* a, const void* b) const noexcept {
                return reinterpret_cast<std::uintptr_t>(a) < reinterpret_cast<std::uintptr_t>(b);
            }
            bool operator()(const void* a, std::uintptr_t b) const noexcept {
                return reinterpret_cast<std::uintptr_t>(a) < b;
            }
            bool operator()(std::uintptr_t a, const void* b) const noexcept {
                return a < reinterpret_cast<std::uintptr_t>(b);
            }
        };
        using RegistryOwners = std::map<const void*, OwnerRecordPtr, NumericPayloadLess>;

        struct RegistryState {
            std::mutex mutex;
            RegistryOwners owners;
        };

        // This trivial TLS flag outlives the nontrivial cache. Late thread/global
        // destructors use the cold registry rather than a destroyed TLS object.
        static bool& owner_cache_available() noexcept {
#if XOFFSET_THREAD_CACHES
            static thread_local bool available = true;
#else
            static bool available = false; // never set: see XOFFSET_THREAD_CACHES
#endif
            return available;
        }

        struct OwnerCache {
            OwnerRecordPtr records[4];
            std::size_t next = 0;
            std::size_t last = 0;
            ~OwnerCache() { owner_cache_available() = false; }
        };

        static OwnerCache& owner_cache() noexcept {
            static thread_local OwnerCache cache;
            return cache;
        }

        static RegistryState& registry_state() {
            // Globals may first register in main and unregister after ordinary
            // function statics have been destroyed. The index and mutex share
            // process lifetime; its nodes are erased as owners are destroyed.
            // Fork policy: no atfork handlers (VmStoragePool follows the same
            // policy). A child forked while another thread was inside the
            // library may only exec; forking while no other thread is inside
            // the library is supported.
            alignas(RegistryState) static std::byte storage[sizeof(RegistryState)];
            static auto* state = ::new (static_cast<void*>(storage)) RegistryState;
            return *state;
        }

        template <typename T>
        static const void* type_identity() noexcept {
            static char token;
            return &token;
        }

        static size_type align_up(size_type value, size_type alignment) noexcept {
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        static XBufferCore* record_owner(const OwnerRecordPtr& record) noexcept {
            return record ? record->owner.load(std::memory_order_acquire) : nullptr;
        }

        static XBufferCore& require_record_owner(const OwnerRecordPtr& record) {
            detail::owner_lookup_count(&detail::OwnerLookupStats::slow_revalidations);
            if (auto* owner = record_owner(record)) return *owner;
            throw XException("arena owner lookup failed");
        }

        static size_type min_payload_size() noexcept {
            return sizeof(arena_header); // 48: a multiple of 8 on every platform
        }

        // R1 storage rule. VM: a pool, Pinned, Automatic growth with a
        // maximum above SLOT_MAX, or an initial size above SLOT_MAX. Pinned
        // slot: maximum <= SLOT_MAX (the slot holds the whole maximum).
        // Movable slot: otherwise, sized for the initial size.
        struct StoragePlan {
            std::size_t slot_class; // SlotGeometry::classes: VM
            bool movable;
        };
        static StoragePlan plan_storage(size_type initial, size_type maximum, XGrowPolicy growth,
                                        bool pooled, XStorage request) {
            using Geometry = detail::SlotGeometry;
            constexpr StoragePlan vm{Geometry::classes, false};
            if (request == XStorage::Pinned) return vm;
            auto refuse = [&] {
                if (request == XStorage::Movable)
                    throw std::invalid_argument(
                        "XStorage::Movable needs slot storage: no pool, initial size <= SLOT_MAX, "
                        "and Explicit growth or maximum <= SLOT_MAX");
                return vm;
            };
            if (!Geometry::enabled || pooled) return refuse();
            if (maximum <= Geometry::max_payload) return {Geometry::class_for(maximum), false};
            if (growth == XGrowPolicy::Automatic || initial > Geometry::max_payload) return refuse();
            return {Geometry::class_for(initial), true};
        }

        static bool plans_slot(const StoragePlan& plan) noexcept {
            return plan.slot_class != detail::SlotGeometry::classes;
        }

        void init_fresh(size_type size, size_type max_reserved, const XBufferPool& pool = {},
                        XStorage request = XStorage::Auto) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD) throw XBadAlloc();
            size = std::max(size, min_payload_size());
            if (size > max_reserved) throw XBadAlloc();
            const auto plan = plan_storage(size, max_reserved, growth_policy_, pool.enabled(), request);
            try {
                free_table_.reset();
                free_table_lost_ = false;
                if (plans_slot(plan)) {
                    buffer_ = detail::VirtualMemoryStorage::for_slot(plan.slot_class, max_reserved, plan.movable);
                    if (!buffer_.grow_to(size)) throw std::bad_alloc();
                } else {
                    buffer_ = detail::VirtualMemoryStorage(size, max_reserved, pool);
                }
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            } catch (const std::length_error&) {
                throw XBadAlloc();
            }
            auto* sm = arena();
            std::memcpy(sm->magic, payload_magic_, sizeof(payload_magic_));
            sm->header_size = static_cast<std::uint32_t>(sizeof(arena_header));
            sm->flags = 0;
            sm->root_offset = 0;
            sm->used_end = min_payload_size();
            sm->reserved0 = 0;
            sm->reserved1 = 0;
            publish_host_owner();
            register_registry();
        }

        void init_from_bytes(const char* data, size_type size, size_type max_reserved, const XBufferPool& pool = {},
                             XStorage request = XStorage::Auto) {
            const auto plan = plan_storage(size, max_reserved, growth_policy_, pool.enabled(), request);
            init_from_storage(data, size, max_reserved, [&] {
                if (!plans_slot(plan) || size > max_reserved)
                    return detail::VirtualMemoryStorage::from_bytes(data, size, max_reserved, pool);
                auto owner = detail::VirtualMemoryStorage::for_slot(plan.slot_class, max_reserved, plan.movable);
                owner.assign_bytes(data, size);
                return owner;
            });
        }

        // Wire payload: extents and sizes validated by the caller.
        void init_from_wire(const WirePayload& wire, size_type size, size_type max_reserved,
                            const XBufferPool& pool = {}, XStorage request = XStorage::Auto) {
            const auto plan = plan_storage(size, max_reserved, growth_policy_, pool.enabled(), request);
            init_from_storage(wire.stored, size, max_reserved, [&] {
                detail::VirtualMemoryStorage owner;
                if (!plans_slot(plan) || size > max_reserved) {
                    owner = wire.extent_count == 0
                        ? detail::VirtualMemoryStorage::from_bytes(wire.stored, size, max_reserved, pool,
                                                                   sizeof(arena_header))
                        : detail::VirtualMemoryStorage::from_extents(wire.stored, wire.extents, wire.extent_count,
                                                                     size, max_reserved, pool);
                } else {
                    owner = detail::VirtualMemoryStorage::for_slot(plan.slot_class, max_reserved, plan.movable);
                    if (wire.extent_count == 0) owner.assign_bytes(wire.stored, size, sizeof(arena_header));
                    else owner.assign_extents(wire.stored, wire.extents, wire.extent_count, size);
                }
                write_payload_header(owner.data(), wire.root_offset, size);
                return owner;
            });
        }

        template <typename Make>
        void init_from_storage(const char* data, size_type size, size_type max_reserved, Make&& make) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD) throw XBadAlloc();
            if (!data || size < sizeof(arena_header)) {
                throw XException(
                    "XBufferCore: payload too small");
            }
            if (size > max_reserved) throw XBadAlloc();
            try {
                free_table_.reset();
                // Holes of an unverified payload are unknown until a verified
                // load supplies its hole table (or a typed walk recomputes it).
                free_table_lost_ = true;
                buffer_ = make();
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            } catch (const std::length_error&) {
                throw XBadAlloc();
            }
            typed_root_access_allowed_ = false;
            auto* sm = arena();
            if (std::memcmp(sm->magic, payload_magic_, sizeof(payload_magic_)) != 0 ||
                sm->header_size != sizeof(arena_header)) {
                throw XException(
                    "XBufferCore: payload header mismatch");
            }
            if (sm->used_end < min_payload_size() || sm->used_end > buffer_.size()) {
                throw XException(
                    "XBufferCore: payload used_end out of range");
            }
            zero_past_used_end();
            publish_host_owner();
            register_registry();
        }

        // Out of line: inlining the short path into every move site changes
        // register allocation in unrelated callers (GCC 16 reloc map.find).
        XOFFSET_DETAIL_NOINLINE void move_from(XBufferCore&& other) noexcept {
            if (holds_nothing()) [[likely]] {
                // Nothing to retire (a new or moved-from target, as in all
                // three moves of std::swap). Take other's storage, record,
                // free list and schema state; other keeps this empty storage
                // and gets the default state the retired path below leaves
                // it. Each epoch still advances by one.
                buffer_.swap(other.buffer_);
                owner_record_.swap(other.owner_record_);
                free_table_.swap(other.free_table_);
                free_table_lost_ = std::exchange(other.free_table_lost_, false);
                publish_host_owner();
                growth_policy_ = std::exchange(other.growth_policy_, XGrowPolicy::Explicit);
                root_type_identity_ = std::exchange(other.root_type_identity_, nullptr);
                typed_root_access_allowed_ = std::exchange(other.typed_root_access_allowed_, true);
                schema_bound_ = std::exchange(other.schema_bound_, false);
                bound_root_type_id_ = std::exchange(other.bound_root_type_id_, 0);
                bound_schema_hash_ = std::exchange(other.bound_schema_hash_, 0);
                ++m_epoch;
                ++other.m_epoch;
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

        // No storage, slot, pool, registry record or free list: retiring
        // this owner would release nothing.
        bool holds_nothing() const noexcept {
            return !buffer_.data() && !buffer_.is_slot() && !buffer_.pool().enabled() &&
                !owner_record_ && !free_table_;
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
            // An ordinary record never entered the index: no mutex, no node.
            if (!record->indexed) return;
            RegistryOwners::node_type retired_node;
            {
                auto& state = registry_state();
                std::lock_guard lock(state.mutex);
                retire_canonical_registration(*record);
                const auto found = state.owners.find(record->payload);
                if (found != state.owners.end() && found->second == record)
                    retired_node = state.owners.extract(found);
            }
            // A cache may retain the inert record, never its released mapping.
            // The node/control block destructor and operator delete are unlocked.
        }

        // Ordinary owners (init_fresh / init_from_storage, and replace_storage,
        // which moves only movable slots) only; direct-receive owners are
        // indexed by VerifiedReceiveAccess. An ordinary record is not indexed:
        // canonical_bytes_at returns null for it anyway, and reservations
        // never overlap (each payload follows its own host page or slot), so
        // leaving it out cannot change any canonical query result.
        struct PreparedOwner {
            OwnerRecordPtr record;
        };

        // The only allocation, including any new_handler reentry, precedes
        // publication of the record.
        PreparedOwner prepare_owner(const detail::VirtualMemoryStorage& storage) {
            if (std::exchange(detail::pending_registry_failure, false)) throw XBadAlloc();
            try {
                return {std::make_shared<OwnerRecord>(storage.data(), this)};
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            }
        }

        // Publishes a prepared record as this owner's.
        void insert_owner(PreparedOwner& prepared) noexcept {
            owner_record_ = std::move(prepared.record);
            detail::owner_lookup_count(&detail::OwnerLookupStats::registrations);
            detail::owner_lookup_count(&detail::OwnerLookupStats::record_object_bytes, sizeof(OwnerRecord));
        }

        void register_registry() {
            auto prepared = prepare_owner(buffer_);
            insert_owner(prepared);
        }

        // Moves this owner to new storage of at least `logical` bytes: a
        // larger slot, or a VM reservation (pinned from then on) above
        // SLOT_MAX or with `pin`. With `copy` the used bytes come along and the rest reads
        // zero; without it the caller overwrites the content (reload). The
        // free table, schema and typed state stay: this is the same lineage
        // at a new address. Prepares everything first; on failure nothing
        // changed and false is returned.
        XOFFSET_DETAIL_NOINLINE bool replace_storage(size_type logical, bool copy, bool pin = false) noexcept {
            const auto maximum = buffer_.maximum_capacity();
            if (logical > maximum) return false;
            detail::VirtualMemoryStorage next;
            PreparedOwner prepared;
            try {
                const auto cls = pin ? detail::SlotGeometry::classes : detail::SlotGeometry::class_for(logical);
                if (cls != detail::SlotGeometry::classes) {
                    next = detail::VirtualMemoryStorage::for_slot(cls, maximum, true);
                    if (copy) next.assign_bytes(buffer_.data(), used_end_clamped());
                } else if (copy) {
                    next = detail::VirtualMemoryStorage::from_bytes(buffer_.data(), used_end_clamped(), maximum);
                } else {
                    next = detail::VirtualMemoryStorage(0, maximum);
                }
                if (copy && !next.grow_to(logical)) return false;
                prepared = prepare_owner(next);
            } catch (...) {
                return false;
            }
            insert_owner_keeping(prepared);
            buffer_.swap(next);
            publish_host_owner();
            ++m_epoch;
            return true;
            // `next` (the old storage) is released here.
        }

        // insert_owner for an owner that already has a record: the old one
        // is retired after the new one is published.
        void insert_owner_keeping(PreparedOwner& prepared) noexcept {
            auto old = std::move(owner_record_);
            insert_owner(prepared);
            auto fresh = std::move(owner_record_);
            owner_record_ = std::move(old);
            unregister_registry();
            owner_record_ = std::move(fresh);
        }

        // An owner without storage (default-constructed or moved-from) gets
        // one for a reload of `size` bytes, as load_verified would without a
        // maximum: DEFAULT_MAXIMUM_CAPACITY, this owner's growth policy.
        void ensure_storage(size_type size) {
            if (!buffer_.data()) create_storage(size);
        }

        // Cold half of ensure_storage, out of line so the inlined reinit
        // admission stays small.
        XOFFSET_DETAIL_NOINLINE void create_storage(size_type size) {
            if (size > DEFAULT_MAXIMUM_CAPACITY) throw XBadAlloc();
            const auto initial = std::max(size, min_payload_size());
            init_fresh(initial, DEFAULT_MAXIMUM_CAPACITY);
        }

        size_type used_end_clamped() const noexcept {
            const auto* sm = arena();
            if (!sm) return 0;
            size_type used = static_cast<size_type>(sm->used_end);
            if (used < min_payload_size()) used = min_payload_size();
            if (used > buffer_.size()) used = buffer_.size();
            return used;
        }

        std::uint64_t& root_offset_slot() noexcept {
            return arena()->root_offset;
        }

        const std::uint64_t& root_offset_value() const noexcept {
            return arena()->root_offset;
        }

        char* payload_byte_address(size_type offset) noexcept {
            if (auto* bytes = buffer_.canonical_byte_origin())
                return reinterpret_cast<char*>(bytes + offset);
            return buffer_.data() + offset;
        }
        const char* payload_byte_address(size_type offset) const noexcept {
            if (auto* bytes = buffer_.canonical_byte_origin())
                return reinterpret_cast<const char*>(bytes + offset);
            return buffer_.data() + offset;
        }

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

        template <typename T, typename... Args>
        T* construct_object_at_slot(std::uint64_t& slot, Args&&... args) {
            if constexpr (sizeof(T) > MAX_PAYLOAD) throw XBadAlloc();
            if (slot != 0) {
                throw XException(
                    "XBufferCore: object slot already populated");
            }
            const auto block = arena_header::block_size(static_cast<std::uint32_t>(sizeof(T)));
            void* raw = allocate_block(block, alignof(T));
            T* obj;
            try {
                obj = std::construct_at(reinterpret_cast<T*>(raw), std::forward<Args>(args)...);
            } catch (...) {
                retire_block(raw, block);
                throw;
            }
            slot = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(obj) - reinterpret_cast<std::uintptr_t>(buffer_.data()));
            return obj;
        }

        static constexpr std::size_t regrow_step = 4096;

        bool ensure_allocation_extent(size_type required, bool allow_growth = true) {
            if (required <= buffer_.size()) return true;
            if (!allow_growth || growth_policy_ != XGrowPolicy::Automatic ||
                buffer_.empty() || required > buffer_.maximum_capacity()) return false;
            const auto maximum = buffer_.maximum_capacity();
            const auto current = buffer_.size();
            const auto geometric = current > maximum / 2 ? maximum : current * 2;
            auto target = std::max<std::size_t>(required, geometric);
            // D6: below zero_from() (bytes exposed before an in-place reload
            // shrank the logical length) grow_to must memset every newly exposed byte, so a
            // doubling there clears far more than the tail will use. Grow in
            // 4 KiB steps inside that range; past it fresh OS pages are
            // already zero and growth is geometric again. used_end, and
            // therefore the wire, does not depend on the logical length.
            if (required < buffer_.zero_from())
                target = std::max<std::size_t>(required, std::min<std::size_t>(geometric, std::size_t(current) + regrow_step));
            // Throw on an OS/preparation failure. In particular, a container's
            // failed tail expansion must not silently retry via a new allocation.
            if (!buffer_.grow_to(target)) throw XBadAlloc();
            return true;
        }

    protected:
        // In-place reload/reset (XBuffer::reload_verified / reset). The
        // mapping, the payload address and therefore the owner registration
        // are kept; every typed pointer into the old content is invalidated
        // (epoch), and the owner is left without typed root access until the
        // caller finishes verification.
        void begin_reinit(bool keep_table = false) noexcept {
            ++m_epoch;
            if (keep_table && free_table_) {
                // Emptied detached (releasing map nodes may reenter), then
                // kept with its bin storage: the next holes need neither a
                // fresh 12 KiB table nor fresh bins.
                FreeTablePtr kept = std::move(free_table_);
                FreeTableRecycler::clear_entries(*kept);
                if (!free_table_) free_table_ = std::move(kept);
            } else {
                if (auto* table = free_table_.release()) FreeTableRecycler::recycle(table, true);
            }
            free_table_lost_ = false;
            typed_root_access_allowed_ = false;
            schema_bound_ = false;
            bound_root_type_id_ = 0;
            bound_schema_hash_ = 0;
            root_type_identity_ = nullptr;
        }

        // Same admission as init_from_storage, against this owner's maximum.
        // The two halves are forced inline so that reload_verified compiles
        // as it did before begin_verified_reload shared them.
        template <typename Assign>
        void reinit_from_storage(const char* data, size_type size, Assign&& assign) {
            admit_reinit(size, data != nullptr);
            try {
                assign();
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            } catch (const std::length_error&) {
                throw XBadAlloc();
            }
            check_reinit_payload();
        }

        XOFFSET_DETAIL_MOVE_INLINE void admit_reinit(size_type size, bool has_source = true) {
            if (has_source && size >= sizeof(arena_header) && size <= MAX_PAYLOAD) ensure_storage(size);
            if (buffer_.empty() && buffer_.maximum_capacity() == 0)
                throw XException("XBufferCore: buffer has no storage");
            begin_reinit();
            if (size > MAX_PAYLOAD) throw XBadAlloc();
            if (!has_source || size < sizeof(arena_header)) {
                throw XException(
                    "XBufferCore: payload too small");
            }
            if (size > buffer_.maximum_capacity()) throw XBadAlloc();
            // A movable slot too small for the new content moves first; the
            // old content is discarded anyway, so nothing is copied.
            if (buffer_.is_slot() && size > buffer_.accessible_bytes() && !replace_storage(size, false))
                throw XBadAlloc();
            // Holes are unknown until verification supplies the hole table.
            free_table_lost_ = true;
        }

        XOFFSET_DETAIL_MOVE_INLINE void check_reinit_payload() {
            auto* sm = arena();
            if (std::memcmp(sm->magic, payload_magic_, sizeof(payload_magic_)) != 0 ||
                sm->header_size != sizeof(arena_header)) {
                throw XException(
                    "XBufferCore: payload header mismatch");
            }
            if (sm->used_end < min_payload_size() || sm->used_end > buffer_.size()) {
                throw XException(
                    "XBufferCore: payload used_end out of range");
            }
            zero_past_used_end();
        }

        void reinit_from_bytes(const char* data, size_type size) {
            reinit_from_storage(data, size, [&] { buffer_.assign_bytes(data, size); });
        }

        void reinit_from_wire(const WirePayload& wire, size_type size) {
            reinit_from_storage(wire.stored, size, [&] {
                if (wire.extent_count == 0) buffer_.assign_bytes(wire.stored, size, sizeof(arena_header));
                else buffer_.assign_extents(wire.stored, wire.extents, wire.extent_count, size);
                write_payload_header(buffer_.data(), wire.root_offset, size);
            });
        }

        // reinit_from_wire for a dense wire when the storage already holds
        // `size` bytes in place (a reload into a buffer of sufficient size):
        // every admission check of admit_reinit holds, so only its state
        // changes remain.
        XOFFSET_DETAIL_MOVE_INLINE void reinit_from_dense_wire_in_place(const char* stored, std::uint64_t root_offset,
                                                                        size_type size) {
            if (size < sizeof(arena_header) || !buffer_.can_assign_in_place(size)) [[unlikely]] {
                reinit_from_wire({root_offset, stored, nullptr, 0}, size);
                return;
            }
            begin_reinit();
            free_table_lost_ = true;
            if (!buffer_.assign_in_place(stored, size, sizeof(arena_header))) throw XBadAlloc();
            write_payload_header(buffer_.data(), root_offset, size);
            check_reinit_payload();
        }

        // In-place direct receive (XBuffer::begin_verified_reload): the
        // admission of reinit, then [0, size) exposed to the writer without
        // clearing. Returns the offset from which bytes are still zero.
        std::size_t reinit_for_overwrite(size_type size) {
            admit_reinit(size);
            try {
                return buffer_.expose_for_overwrite(size);
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            }
        }

        // clone()/clone_into(): the host state that travels with a payload
        // copied from a live buffer of this process. The logical size grows
        // to src's (at most this owner's maximum; the tail reads as zero),
        // the exact free table is copied (an incomplete one stays incomplete),
        // and so are typed root access and the root/schema binding, because
        // the bytes are exactly src's: nothing needs verifying. Throws
        // XBadAlloc; on a throw this owner keeps the state the payload copy
        // left it in.
        void copy_clone_state(const XBufferCore& src) {
            const auto logical = std::min(src.buffer_.size(), buffer_.maximum_capacity());
            if (logical > buffer_.size() && !buffer_.grow_to(logical) &&
                !(buffer_.movable() && replace_storage(logical, true)))
                throw XBadAlloc();
            if (!src.free_table_lost_) {
                try {
                    free_table_ = src.free_table_ ? copy_free_table(*src.free_table_) : FreeTablePtr{};
                } catch (const std::bad_alloc&) {
                    throw XBadAlloc();
                } catch (const std::length_error&) {
                    throw XBadAlloc();
                }
                free_table_lost_ = false;
            }
            typed_root_access_allowed_ = src.typed_root_access_allowed_;
            schema_bound_ = src.schema_bound_;
            bound_root_type_id_ = src.bound_root_type_id_;
            bound_schema_hash_ = src.bound_schema_hash_;
            root_type_identity_ = src.root_type_identity_;
        }

        // clone_into(): src's used payload replaces this owner's content in
        // place, like reload_verified (same mapping, address and registration;
        // epoch advances). On any failure this owner is left an empty arena.
        void clone_state_from(const XBufferCore& src) {
            if (this == &src) throw XException("clone_into(): source and destination are the same buffer");
            try {
                const auto* sm = src.arena();
                if (!sm) throw XException("clone_into(): source buffer has no arena");
                reinit_from_bytes(reinterpret_cast<const char*>(sm), static_cast<size_type>(sm->used_end));
                copy_clone_state(src);
            } catch (...) {
                reset_arena(0);
                throw;
            }
        }

        // clone(): a new owner with src's maximum capacity, growth policy,
        // pool and kind of storage: a movable slot onto a movable slot, a VM
        // owner onto VM, a pinned slot onto a pinned slot (Auto picks one,
        // since its maximum fits a slot).
        MaximumCapacity clone_capacity() const noexcept {
            const auto storage = buffer_.movable() ? XStorage::Movable
                                 : buffer_.is_slot() ? XStorage::Auto
                                                     : XStorage::Pinned;
            return MaximumCapacity{buffer_.maximum_capacity(), growth_policy_, buffer_.pool(), storage};
        }

        // Empty arena: header only, no root, empty exact free table.
        // N2: every logical byte at or past used_end is zero (growth zeroes
        // what it exposes, used_end only rises until a reset, and loads zero
        // the bytes past the loaded used_end), so a reset that keeps the
        // logical length clears exactly [header, used_end): live blocks and
        // holes alike. Otherwise bytes above the header become
        // dirty-below-zero_from_ and are zeroed by the regrowth to `logical`
        // (or by any later growth). noexcept: if the storage cannot hold a
        // header the owner simply stays without one.
        void reset_arena(size_type logical) noexcept {
            begin_reinit(true);
            if (buffer_.maximum_capacity() < min_payload_size()) return;
            if (logical == buffer_.size() && logical >= min_payload_size()) {
                const auto used = used_end_clamped();
                std::memset(buffer_.data() + min_payload_size(), 0, used - min_payload_size());
                write_empty_header();
                return;
            }
            if (buffer_.size() < min_payload_size() && !buffer_.grow_to(min_payload_size())) return;
            if (buffer_.size() > min_payload_size()) buffer_.resize_logical(min_payload_size());
            write_empty_header();
            if (logical > min_payload_size()) (void)buffer_.grow_to(std::min(logical, buffer_.maximum_capacity()));
        }

        void write_empty_header() noexcept {
            auto* sm = arena();
            std::memcpy(sm->magic, payload_magic_, sizeof(payload_magic_));
            sm->header_size = static_cast<std::uint32_t>(sizeof(arena_header));
            sm->flags = 0;
            sm->root_offset = 0;
            sm->used_end = min_payload_size();
            sm->reserved0 = 0;
            sm->reserved1 = 0;
        }

        // N2: the bytes past a loaded payload's used_end are zeroed, so the
        // invariant above holds after every load. A verified load has none
        // (used_end == used_bytes); an unverified one may carry any bytes there.
        void zero_past_used_end() noexcept {
            const auto used = static_cast<size_type>(arena()->used_end);
            if (used < buffer_.size()) std::memset(buffer_.data() + used, 0, buffer_.size() - used);
        }

        // Storage-associated access/schema state belongs to the owner so even
        // transfers through XBufferCore cannot leave derived flags behind.
        bool typed_root_access_allowed_ = true;
        bool schema_bound_ = false;
        std::uint64_t bound_root_type_id_ = 0;
        std::uint64_t bound_schema_hash_ = 0;

    private:
        friend struct detail::VerifiedPathsAccess;
        friend struct detail::VerifiedReceiveAccess;
        friend struct fixed_detail::StableEraseOwnerWitness;
        OwnerRecordPtr owner_record_;
        detail::VirtualMemoryStorage buffer_;
        XGrowPolicy growth_policy_ = XGrowPolicy::Explicit;
        FreeTablePtr free_table_;
        bool free_table_lost_ = false;
        const void* root_type_identity_ = nullptr;
        std::uint64_t m_epoch = 0;
    };

    // ========================================================================
    // Container implementation details.
    // ========================================================================
    namespace detail {

        // Container concepts — used by XCompactor for migration dispatch.
        template<typename T>
        concept SetLikeContainer = requires(T t) {
            { t.begin() } -> std::input_or_output_iterator;
            { t.end() } -> std::input_or_output_iterator;
            typename T::value_type;
            typename T::key_type;
            { t.emplace(std::move(std::declval<typename T::value_type>())) };
        } && !requires { typename T::mapped_type; };

        template<typename T>
        concept MapLikeContainer = requires(T t) {
            { t.begin() } -> std::input_or_output_iterator;
            { t.end() } -> std::input_or_output_iterator;
            typename T::key_type;
            typename T::mapped_type;
            { t.emplace(std::move(std::declval<typename T::key_type>()),
                         std::move(std::declval<typename T::mapped_type>())) };
        };
        // Forward declarations (resolved at instantiation time).
        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc);

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all(void* dst, Src&& src, Alloc alloc);

        template <std::size_t Count, typename Fn>
        void static_for(Fn&& fn) {
            if constexpr (Count > 0) {
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (fn(std::integral_constant<std::size_t, Is>{}), ...);
                }(std::make_index_sequence<Count>{});
            }
        }

        template <typename T, typename BaseFn, typename MemberFn>
        void for_each_reflected_subobject(BaseFn&& on_base, MemberFn&& on_member) {
            static_for<schema::base_count_v<T>>([&](auto index) {
                on_base(index);
            });
            static_for<schema::member_count_v<T>>([&](auto index) {
                on_member(index);
            });
        }

        template <typename T>
        concept fixed_size_scalar =
            std::is_same_v<T, bool> ||
            std::is_same_v<T, char> ||
            std::is_same_v<T, signed char> ||
            std::is_same_v<T, unsigned char> ||
            std::is_same_v<T, std::byte> ||
            std::is_same_v<T, int8_t> ||
            std::is_same_v<T, uint8_t> ||
            std::is_same_v<T, int16_t> ||
            std::is_same_v<T, uint16_t> ||
            std::is_same_v<T, int32_t> ||
            std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, int64_t> ||
            std::is_same_v<T, uint64_t> ||
            std::is_same_v<T, float> ||
            std::is_same_v<T, double>;

    } // namespace detail

    using XArena = XBufferCore::arena_header;
    template <typename T>
    using XArenaAllocator = XArenaAllocatorBase<T, XArena>;
    using XAllocator = XArenaAllocator<char>;

    // Constraint: Elem has allocator_type AND Arg is not Elem itself.
    template <typename Elem, typename Arg>
    concept alloc_aware_convertible =
        requires { typename Elem::allocator_type; } &&
        !std::is_same_v<std::decay_t<Arg>, Elem>;

    namespace fixed_detail {
        // V2: a head binds directly to its payload base (the arena header).
        struct allocator_ref {
            XArena* segment{};

            XArena* arena() const noexcept { return segment; }
        };

        // A host identity lease detects a dead/replaced reservation after host
        // allocation or a predicate callback. It does not pin the VM mapping or
        // authorize concurrent mutation, target destruction or callback reentry
        // into this container during the destructive commit.
        struct StableEraseOwnerWitness {
            XBufferCore::OwnerRecordPtr record;
            const void* head{};
            std::size_t head_bytes{}, head_offset{};
            bool head_in_payload{};

            StableEraseOwnerWitness(const allocator_ref& alloc,
                                    const void* address, std::size_t bytes)
                : head(address), head_bytes(bytes) {
                auto& owner = XBufferCore::owner_from_arena(alloc.segment);
                record = owner.owner_record_; // shared_ptr copy does not allocate
                const auto base = reinterpret_cast<std::uintptr_t>(record->payload);
                const auto at = reinterpret_cast<std::uintptr_t>(address);
                if (at >= base && at - base < owner.buffer_.size()) {
                    head_in_payload = true;
                    head_offset = at - base;
                }
                validate();
            }

            void validate() const {
                auto* owner = XBufferCore::record_owner(record);
                if (!owner || owner->buffer_.data() != record->payload)
                    throw XException("stable_erase_if owner is no longer live");
                if (head_in_payload &&
                    (head_offset > owner->buffer_.size() ||
                     head_bytes > owner->buffer_.size() - head_offset))
                    throw XException("stable_erase_if target is no longer in storage");
            }
        };

        // O3: an equal rel32 value is not allocator provenance.
        inline bool move_assignment_same_allocator(const allocator_ref& a,
                                                   const allocator_ref& b) noexcept {
            return a.segment && a.segment == b.segment;
        }

        inline bool move_assignment_contains(const void* storage, std::size_t bytes,
                                             const void* address) noexcept {
            if (!storage || bytes == 0) return false;
            const auto first = reinterpret_cast<std::uintptr_t>(storage);
            const auto candidate = reinterpret_cast<std::uintptr_t>(address);
            return candidate >= first && candidate - first < bytes;
        }

        inline bool move_assignment_head_in_arena(const void* head, std::size_t bytes,
                                                  const XArena* arena) noexcept {
            if (!arena) return false;
            const auto first = reinterpret_cast<std::uintptr_t>(arena);
            const auto candidate = reinterpret_cast<std::uintptr_t>(head);
            return candidate >= first && candidate - first <= arena->used_end &&
                bytes <= arena->used_end - (candidate - first);
        }

        template <typename T>
        bool move_assignment_owns_address(const T& value, const void* address) noexcept;
        // MV: result-identical to move_assignment_owns_address; leaf heads are
        // evaluated inline at the move-assignment call site.
        template <typename T>
        XOFFSET_DETAIL_MOVE_INLINE bool move_assignment_owns_address_fast(
            const T& value, const void* address) noexcept;

        // N6: whether a move-assignment steal must first exclude that either
        // head lies inside the other's owned tree. On by default only in debug
        // builds; release builds rely on the caller not moving a container
        // into (or out of) its own subobject.
#if !defined(XOFFSET_MOVE_OWNERSHIP_CHECKS)
#  if defined(NDEBUG)
#    define XOFFSET_MOVE_OWNERSHIP_CHECKS 0
#  else
#    define XOFFSET_MOVE_OWNERSHIP_CHECKS 1
#  endif
#endif
        template <typename T>
        XOFFSET_DETAIL_MOVE_INLINE bool move_assignment_disjoint(const T& a, const T& b) noexcept {
#if XOFFSET_MOVE_OWNERSHIP_CHECKS
            return !move_assignment_owns_address_fast(a, &b) &&
                   !move_assignment_owns_address_fast(b, &a);
#else
            (void)a;
            (void)b;
            return true;
#endif
        }

        template <typename T> consteval bool stable_erase_supported();
        template <typename T> void stable_erase_relocate(T*, T&) noexcept;
        template <typename T> consteval bool can_relocate_without_allocation();
        template <typename T> void relocate_construct(T* dst, T& src) noexcept;

        // W9: flat resolution for every storage kind (ordinary and receive):
        // origin + delta, with no canonical lookup, atomic or window compare.
        // Receive data no longer derives pointers from the receive byte array;
        // it uses the same disclosed flat-address model as ordinary loads.
        template <typename T>
        T* resolve_rel32(const std::int32_t* origin, std::int32_t delta) noexcept {
            if (delta == 0) return nullptr;
            auto* base = reinterpret_cast<const char*>(origin);
            return std::launder(reinterpret_cast<T*>(const_cast<char*>(base + delta)));
        }

        // delta != 0 is known (a head with capacity names a live block).
        template <typename T>
        T* resolve_rel32_live(const std::int32_t* origin, std::int32_t delta) noexcept {
            auto* base = reinterpret_cast<const char*>(origin);
            return std::launder(reinterpret_cast<T*>(const_cast<char*>(base + delta)));
        }

        // memmove of 1..16 bytes without a library call: every source byte is
        // loaded before any store, so src may overlap dst.
        inline void move_short_bytes(char* dst, const char* src, std::size_t count) noexcept {
            if (count >= 8) {
                std::uint64_t head, tail;
                std::memcpy(&head, src, 8);
                std::memcpy(&tail, src + count - 8, 8);
                std::memcpy(dst, &head, 8);
                std::memcpy(dst + count - 8, &tail, 8);
            } else if (count >= 4) {
                std::uint32_t head, tail;
                std::memcpy(&head, src, 4);
                std::memcpy(&tail, src + count - 4, 4);
                std::memcpy(dst, &head, 4);
                std::memcpy(dst + count - 4, &tail, 4);
            } else {
                const char first = src[0], middle = src[count >> 1], last = src[count - 1];
                dst[0] = first;
                dst[count >> 1] = middle;
                dst[count - 1] = last;
            }
        }

        template <typename T>
        const T* resolve_rel32_const(const std::int32_t* origin, std::int32_t delta) noexcept {
            if (delta == 0) return nullptr;
            auto* base = reinterpret_cast<const char*>(origin);
            return std::launder(reinterpret_cast<const T*>(base + delta));
        }

        [[noreturn]] XOFFSET_DETAIL_MOVE_COLD inline void throw_rel32_range() {
            throw XException(
                "relative offset exceeds rel32 range");
        }

        // True when target is within rel32 range of origin.
        XOFFSET_DETAIL_MOVE_INLINE bool rel32_reaches(const void* origin, const void* target) noexcept {
            const auto delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) -
                               static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(origin));
            return delta >= std::numeric_limits<std::int32_t>::min() &&
                   delta <= std::numeric_limits<std::int32_t>::max();
        }

        // MV: identical checks and store; only the throw is out of line, so
        // the range test inlines at every call site.
        XOFFSET_DETAIL_MOVE_INLINE void set_rel32(std::int32_t& field, const void* origin, const void* target) {
            if (!target) {
                field = 0;
                return;
            }
            auto delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) -
                         static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(origin));
            if (delta < static_cast<std::ptrdiff_t>(std::numeric_limits<std::int32_t>::min()) ||
                delta > static_cast<std::ptrdiff_t>(std::numeric_limits<std::int32_t>::max())) [[unlikely]] {
                throw_rel32_range();
            }
            field = static_cast<std::int32_t>(delta);
        }

        inline std::uint32_t round_up_to_alignment(std::uint32_t value,
                                                   std::uint32_t alignment) noexcept {
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        // V2 block size of a logical byte count (a nonzero multiple of 8).
        // Invariant relied on by save_compact_into with assignments: a heap
        // string of capacity c owns block_bytes_of(c + 1) bytes, so its
        // characters and terminator can be read in whole 8-byte words
        // (test_save_composed checks it).
        inline std::uint32_t block_bytes_of(std::uint32_t bytes) noexcept {
            return XArena::block_size(bytes);
        }

        inline void* allocate_dynamic_bytes(const allocator_ref& alloc_ref,
                                            std::uint32_t bytes,
                                            std::size_t alignment) {
            auto* arena = alloc_ref.segment;
            if (!arena) {
                throw XException(
                    "allocator binding has no arena");
            }
            if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
                alignment > XBufferCore::MAX_PAYLOAD ||
                bytes > XBufferCore::MAX_PAYLOAD - std::max<std::size_t>(alignment, 8u)) {
                throw XBadAlloc();
            }
            return XBufferCore::owner_from_arena(arena).allocate_block(block_bytes_of(bytes), alignment);
        }

        // Live block of old_bytes grows in place to new_bytes (tail only).
        // Only a block that ends at used_end can (try_expand_block tests the
        // same sum), so that is tested first: most regrowths after a reload
        // are of other blocks and skip the checks and the owner lookup.
        inline bool try_expand_dynamic_bytes(const allocator_ref& alloc_ref,
                                             void* ptr, std::uint32_t old_bytes,
                                             std::uint32_t new_bytes,
                                             std::size_t alignment) {
            const auto* arena = alloc_ref.segment;
            if (!arena || reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(arena) +
                              block_bytes_of(old_bytes) != arena->used_end) return false;
            if (!ptr || old_bytes == 0 || new_bytes == 0 || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > XBufferCore::MAX_PAYLOAD ||
                reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0 ||
                new_bytes > XBufferCore::MAX_PAYLOAD - 8u) return false;
            return XBufferCore::owner_from_arena(alloc_ref.segment).try_expand_block(
                ptr, block_bytes_of(old_bytes), block_bytes_of(new_bytes));
        }

        inline void retire_dynamic_bytes(const allocator_ref& alloc_ref,
                                         void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0) return;
            if (auto* owner = XBufferCore::owner_from_arena_noexcept(alloc_ref.segment))
                owner->retire_block(ptr, block_bytes_of(bytes));
        }

        // Fused regrowth (A_ZPP_LEVERS 2.2 item 1): a fresh block of
        // new_bytes, `fill` (moves the live run in, publishes the block and
        // returns the old one; must not throw), then the old block retired.
        // The binding and size checks run once; the owner is read again for
        // the retirement, since allocation may run new_handler, which may move
        // the owning Core. Placement equals allocate, then retire.
        template <typename Fill>
        XOFFSET_DETAIL_MOVE_INLINE void regrow_dynamic_bytes(const allocator_ref& alloc_ref,
                                                             std::uint32_t old_bytes, std::uint32_t new_bytes,
                                                             std::size_t alignment, Fill&& fill) {
            auto* arena = alloc_ref.segment;
            auto* owner = XBufferCore::owner_from_arena_noexcept(arena);
            if (!owner || alignment == 0 || (alignment & (alignment - 1)) != 0 ||
                alignment > XBufferCore::MAX_PAYLOAD ||
                new_bytes > XBufferCore::MAX_PAYLOAD - std::max<std::size_t>(alignment, 8u)) [[unlikely]] {
                (void)allocate_dynamic_bytes(alloc_ref, new_bytes, alignment); // throws the usual error
            }
            void* fresh = owner->allocate_block(block_bytes_of(new_bytes), alignment);
            void* old = fill(fresh);
            if (old && old_bytes != 0)
                if (auto* now = XBufferCore::owner_from_arena_noexcept(arena))
                    now->retire_block(old, block_bytes_of(old_bytes));
        }

        template <typename T>
        void destroy_n(T* ptr, std::uint32_t count) noexcept {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(ptr, count);
            }
        }

        // An aggregate element whose destructor is the implicit one only
        // destroys its subobjects, so its heads may be released first.
        template <typename T>
        consteval bool release_walk_supported() {
            if constexpr (!std::is_class_v<T> || !std::is_aggregate_v<T> ||
                          std::is_trivially_destructible_v<T>) {
                return false;
            } else {
                return schema::implicit_lifetime_aggregate<T>();
            }
        }

        // Owner lookup rule 5 (V2_FORMAT): a container that destroys its
        // elements passes its own binding down, so the element heads, and the
        // heads nested in aggregate elements, retire their blocks without a
        // lookup each. The destructors that run afterwards see empty heads.
        template <typename Alloc, typename T>
        void release_in(const Alloc& alloc, T& value) noexcept {
            if constexpr (requires { value.release_in(alloc); }) {
                value.release_in(alloc);
            } else if constexpr (std::is_array_v<T>) {
                for (auto& item : value) release_in(alloc, item);
            } else if constexpr (release_walk_supported<T>()) {
                detail::for_each_reflected_subobject<T>([&](auto index) {
                    using Base = schema::base_t<T, decltype(index)::value>;
                    release_in(alloc, static_cast<Base&>(value));
                }, [&](auto index) {
                    using member = schema::member_t<T, decltype(index)::value>;
                    if constexpr (!std::is_reference_v<typename member::type> &&
                                  !member::is_bit_field) {
                        release_in(alloc, member::get(value));
                    }
                });
            }
        }

        template <typename Alloc, typename T>
        void destroy_n_in(const Alloc& alloc, T* ptr, std::uint32_t count) noexcept {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (std::uint32_t i = 0; i < count; ++i) {
                    release_in(alloc, ptr[i]);
                    std::destroy_at(ptr + i);
                }
            }
        }
#if defined(__linux__)

        // A SHIFT erase from the middle moves the tail of records down by one
        // element: an overlapping memmove with the destination just below the
        // source. On x86-64 glibc, one call over a tail of tens of KiB is
        // about a third slower than the same copy in 4 KiB chunks (SLG
        // S.ticks_middle on a cloud Xeon); tails up to 16 KiB stay one call.
        // Chunks go from low to high addresses, so none overwrites source
        // bytes still to be read. The loop is out of line so that the erase
        // body keeps its inlining.
#if defined(__GLIBC__) && defined(__x86_64__)
        XOFFSET_DETAIL_NOINLINE inline void overlap_move_down_chunked(
                unsigned char* d, const unsigned char* s, std::size_t bytes) noexcept {
            constexpr std::size_t chunk = 4096;
            do {
                std::memmove(d, s, chunk);
                d += chunk;
                s += chunk;
                bytes -= chunk;
            } while (bytes > chunk);
            std::memmove(d, s, bytes);
        }
#endif

        inline void overlap_move_down(void* dst, const void* src, std::size_t bytes) noexcept {
#if defined(__GLIBC__) && defined(__x86_64__)
            if (bytes > 16384) [[unlikely]] {
                overlap_move_down_chunked(static_cast<unsigned char*>(dst),
                                          static_cast<const unsigned char*>(src), bytes);
                return;
            }
#endif
            std::memmove(dst, src, bytes);
        }
#endif

        inline allocator_ref allocator_ref_of(XArena* sm) noexcept {
            return {sm};
        }

        template <typename Alloc>
            requires requires(const Alloc& alloc) { alloc.arena(); }
        inline allocator_ref allocator_ref_of(const Alloc& alloc) noexcept {
            return {alloc.arena()};
        }

        struct stored_allocator_proxy {
            XArena* segment{};

            XArena* arena() const noexcept { return segment; }
        };

        template <typename T, typename Alloc>
        void default_construct(T* dst, const Alloc& alloc) {
            if constexpr (detail::future_custom_lifetime_supported<T>()) {
                detail::future_default_construct<T>(dst, alloc);
            } else if constexpr (std::is_array_v<T>) {
                std::uint32_t built = 0;
                try {
                    for (; built < std::extent_v<T>; ++built) default_construct(&(*dst)[built], alloc);
                } catch (...) {
                    destroy_n(*dst, built);
                    throw;
                }
            } else if constexpr (requires(const Alloc& value) { T(value); }) {
                std::construct_at(dst, alloc);
            } else if constexpr (!std::is_trivially_copyable_v<T> && std::is_class_v<T>) {
                detail::reflect_init_all<T>(static_cast<void*>(dst), alloc);
            } else {
                std::construct_at(dst);
            }
            detail::zero_implicit_padding(dst);
        }

        template <typename T, typename Src, typename Alloc>
        void transfer_construct(T* dst, Src&& src, const Alloc& alloc) {
            using CleanSrc = std::remove_reference_t<Src>;
            if constexpr (detail::future_custom_lifetime_supported<T>()) {
                // Preserve this helper's existing nondestructive copy/fill
                // contract even when its caller passed a temporary.
                detail::future_transfer_construct<T>(dst, static_cast<const CleanSrc&>(src), alloc);
            } else if constexpr (std::is_array_v<T>) {
                std::uint32_t built = 0;
                try {
                    for (; built < std::extent_v<T>; ++built) transfer_construct(&(*dst)[built], src[built], alloc);
                } catch (...) {
                    destroy_n(*dst, built);
                    throw;
                }
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                std::construct_at(dst, static_cast<const CleanSrc&>(src));
            } else if constexpr (requires(const CleanSrc& value, const Alloc& alloc_value) {
                T(value, alloc_value);
            }) {
                std::construct_at(dst, static_cast<const CleanSrc&>(src), alloc);
            } else if constexpr (std::is_class_v<T>) {
                detail::reflect_transfer_init_all<T>(
                    static_cast<void*>(dst),
                    static_cast<const CleanSrc&>(src),
                    alloc);
            } else {
                std::construct_at(dst, static_cast<const CleanSrc&>(src));
            }
            detail::zero_implicit_padding(dst);
        }

        template <typename T, typename Alloc, typename... Args>
        void construct_bound(T* dst, const Alloc& alloc, Args&&... args) {
            if constexpr (sizeof...(Args) == 0) {
                default_construct(dst, alloc);
            } else if constexpr (sizeof...(Args) == 1 &&
                                 (std::is_same_v<T, std::remove_cvref_t<Args>> && ...)) {
                transfer_construct(dst, std::forward<Args>(args)..., alloc);
            } else if constexpr (sizeof...(Args) == 1 &&
                                 (requires { allocator_ref_of(args); } && ...)) {
                std::construct_at(dst, std::forward<Args>(args)...);
            } else if constexpr (requires { T(std::forward<Args>(args)..., alloc); }) {
                std::construct_at(dst, std::forward<Args>(args)..., alloc);
            } else if constexpr (sizeof...(Args) == 1 && requires { T(alloc); } &&
                                 (std::is_assignable_v<T&, Args&&> && ...)) {
                std::construct_at(dst, alloc);
                try {
                    ((*dst = std::forward<Args>(args)), ...);
                } catch (...) {
                    std::destroy_at(dst);
                    throw;
                }
            } else {
                std::construct_at(dst, std::forward<Args>(args)...);
            }
            detail::zero_implicit_padding(dst);
        }

        template <typename T>
        concept string_view_like = std::is_convertible_v<T, std::string_view>;

        template <typename T>
        concept has_ordered_member_view = requires(const T& value) {
            { value.view() } -> std::convertible_to<std::string_view>;
        };

        template <typename T>
        struct is_char_array : std::false_type {};

        template <std::size_t N>
        struct is_char_array<char[N]> : std::true_type {};

        template <std::size_t N>
        struct is_char_array<const char[N]> : std::true_type {};

        template <typename T>
        inline constexpr bool is_char_array_v = is_char_array<std::remove_cvref_t<T>>::value;

        template <typename T>
        concept ordered_string_like =
            has_ordered_member_view<T> ||
            std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
            std::is_convertible_v<T, const char*> ||
            is_char_array_v<T> ||
            std::is_convertible_v<T, std::string_view>;

        template <typename T>
            requires has_ordered_member_view<T>
        inline std::string_view ordered_view(const T& value) noexcept {
            return std::string_view(value.view());
        }

        inline std::string_view ordered_view(std::string_view value) noexcept {
            return value;
        }

        inline std::string_view ordered_view(const char* value) noexcept {
            return value ? std::string_view(value) : std::string_view{};
        }

        template <std::size_t N>
        inline std::string_view ordered_view(const char (&value)[N]) noexcept {
            return std::string_view(value);
        }

        template <string_view_like T>
            requires (!has_ordered_member_view<T> &&
                      !std::is_same_v<std::remove_cvref_t<T>, std::string_view> &&
                      !std::is_convertible_v<T, const char*> &&
                      !is_char_array_v<T>)
        inline std::string_view ordered_view(const T& value) noexcept {
            return std::string_view(value);
        }

        // An enum with an ADL operator<=> but no operator<: Clang resolves
        // `a < b` to the rewritten operator<=>, GCC to the builtin operator<.
        // Calling <=> explicitly gives the same key order on every compiler.
        template <typename L, typename R>
        concept enum_ordered_by_spaceship_only =
            std::is_enum_v<L> && std::is_same_v<L, R> &&
            requires(const L& a, const R& b) { operator<=>(a, b); } &&
            !requires(const L& a, const R& b) { operator<(a, b); };

        template <typename L, typename R>
        bool ordered_less(const L& lhs, const R& rhs) {
            if constexpr (ordered_string_like<L> && ordered_string_like<R>) {
                return ordered_view(lhs) < ordered_view(rhs);
            } else if constexpr (enum_ordered_by_spaceship_only<L, R>) {
                return (lhs <=> rhs) < 0;
            } else {
                return lhs < rhs;
            }
        }

        template <typename L, typename R>
        bool ordered_equal(const L& lhs, const R& rhs) {
            if constexpr (ordered_string_like<L> && ordered_string_like<R>) {
                return ordered_view(lhs) == ordered_view(rhs);
            } else {
                return lhs == rhs;
            }
        }

        template <typename Elem, typename KeyLike, typename KeyFn>
        std::uint32_t lower_bound_index(const Elem* data, std::uint32_t size,
                                        const KeyLike& key, KeyFn&& key_fn) {
            if constexpr (std::is_scalar_v<std::remove_cvref_t<decltype(key_fn(*data))>> &&
                          std::is_scalar_v<KeyLike>) {
                // Scalar keys: branch-free halving (conditional move), same
                // index as the loop below. Random keys made the loop's
                // data-dependent branch mispredict about half the time.
                if (size == 0) return 0;
                const Elem* base = data;
                std::uint32_t n = size;
                while (n > 1) {
                    const std::uint32_t half = n / 2;
                    base = ordered_less(key_fn(base[half - 1]), key) ? base + half : base;
                    n -= half;
                }
                return static_cast<std::uint32_t>(base - data) +
                       static_cast<std::uint32_t>(ordered_less(key_fn(*base), key));
            }
            std::uint32_t first = 0;
            std::uint32_t count = size;
            while (count > 0) {
                std::uint32_t step = count / 2;
                std::uint32_t mid = first + step;
                if (ordered_less(key_fn(data[mid]), key)) {
                    first = mid + 1;
                    count -= step + 1;
                } else {
                    count = step;
                }
            }
            return first;
        }
    } // namespace fixed_detail

    template <typename K, typename V>
    struct XKeyValue {
        using first_type = K;
        using second_type = V;
        K first{};
        V second{};
    };

    // V2.1 (R4): a container head is {data_delta, size_, capacity_}, 12 bytes,
    // 4-aligned. It belongs to the payload it lies in: the owner is found from
    // the head's address (detail::OwnerMap), never stored in the head.
    struct XFixedString {
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;

        // I1: a string of at most inline_max bytes may live in bytes 0..10 of
        // this head. Inline is marked by bit 31 of capacity_ (byte 11, which a
        // heap capacity < 2^31 never sets); bits 24..30 hold the length. The
        // byte after the last character and every unused inline byte are 0.
        static constexpr std::uint32_t inline_flag = 0x80000000u;
        static constexpr std::uint32_t inline_max = 10;

        std::int32_t data_delta = 0;
        std::uint32_t size_ = 0;
        std::uint32_t capacity_ = 0;

        XFixedString() = default;
        // The allocator names the payload this head lies in; V2.1 reads that
        // from the head's address, so the argument is not stored.
        explicit XFixedString(allocator_type) noexcept {}
        ~XFixedString() { release(); }
        XFixedString(const XFixedString& other, allocator_type alloc) : XFixedString(alloc) {
            assign(other.view());
        }

        XFixedString(const XFixedString&) = delete;
        XFixedString(XFixedString&&) = delete;
        XFixedString& operator=(const XFixedString& other) {
            if (this == &other) return *this;
            if (other.is_inline() && is_inline()) [[likely]] {
                // Inline into an inline head: the 12 inline bytes are the
                // whole value (characters, zero padding, length and flag), as
                // store_inline would write them from other.view(). Inline
                // storage needs no binding.
                std::memcpy(inline_bytes(), other.inline_bytes(), 12);
                return *this;
            }
            // assign() looks the binding up only when it needs a block.
            assign(other.view());
            return *this;
        }
        XFixedString& operator=(XFixedString&& other) {
            if (this == &other) return *this;
            if (other.is_inline()) {
                // An inline source owns no storage: copy its bytes, no allocator.
                release();
                std::memcpy(inline_bytes(), other.inline_bytes(), 12);
                other.reset_head();
                return *this;
            }
            if (auto* const shared = reinterpret_cast<XArena*>(detail::OwnerMap::same_payload(this, &other))) {
                // Both heads lie in one payload (one owner-table read).
                if (fixed_detail::move_assignment_disjoint(*this, other)) {
                    std::int32_t incoming_delta = 0;
                    fixed_detail::set_rel32(incoming_delta, &data_delta, other.data());
                    const auto incoming_size = other.size_;
                    const auto incoming_capacity = other.capacity_;
                    // Potentially throwing work precedes the ownership commit.
                    // release() is noexcept; both heads remain in live storage.
                    // MV: release()'s empty-destination case inline (data() is
                    // null exactly when data_delta == 0, and release() then only
                    // clears the descriptor); otherwise release() with the
                    // binding just looked up.
                    if (!is_inline() && data_delta == 0) {
                        size_ = 0;
                        capacity_ = 0;
                    } else {
                        release_in(allocator_type{shared});
                    }
                    data_delta = incoming_delta;
                    size_ = incoming_size;
                    capacity_ = incoming_capacity;
                    other.data_delta = 0;
                    other.size_ = 0;
                    other.capacity_ = 0;
                    return *this;
                }
            }
            return (*this = static_cast<const XFixedString&>(other));
        }

        // Bound = the head lies in a registered payload. Each call is one
        // owner-table read: callers look the binding up only to allocate,
        // grow or retire a block, once per operation (V2_FORMAT, Owner lookup).
        bool is_bound() const noexcept { return arena() != nullptr; }

        allocator_type binding() const noexcept {
            return {arena()};
        }

        // One lookup that both checks and returns the binding.
        XOFFSET_DETAIL_MOVE_INLINE allocator_type bound_binding() const {
            auto* const a = arena();
            if (!a) [[unlikely]] throw_unbound();
            return {a};
        }
        [[noreturn]] XOFFSET_DETAIL_MOVE_COLD static void throw_unbound() {
            throw XException("XFixedString needs a block but lies outside every XOffset payload");
        }

        XOFFSET_DETAIL_MOVE_INLINE XArena* arena() const noexcept {
            return reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(this));
        }

        stored_allocator_type get_stored_allocator() const noexcept {
            return {arena()};
        }

        bool is_inline() const noexcept { return (capacity_ & inline_flag) != 0; }

        // Bytes of out-of-line storage owned by this head (0 when inline/null).
        std::size_t owned_extent() const noexcept {
            return is_inline() || capacity_ == 0 ? 0u : std::size_t(capacity_) + 1u;
        }

        char* data() noexcept {
            if (is_inline()) return inline_bytes();
            return fixed_detail::resolve_rel32<char>(&data_delta, data_delta);
        }

        const char* data() const noexcept {
            if (is_inline()) return inline_bytes();
            return fixed_detail::resolve_rel32_const<char>(&data_delta, data_delta);
        }

        std::uint32_t size() const noexcept { return is_inline() ? inline_size() : size_; }
        std::uint32_t capacity() const noexcept { return is_inline() ? inline_max : capacity_; }
        bool empty() const noexcept { return size() == 0; }

        std::string_view view() const noexcept {
            if (is_inline()) return std::string_view(inline_bytes(), inline_size());
            return data_delta ? std::string_view(
                fixed_detail::resolve_rel32_const<char>(&data_delta, data_delta), size_)
                              : std::string_view{};
        }

        operator std::string_view() const noexcept { return view(); }

        const char* c_str() const noexcept {
            auto* ptr = data();
            return ptr ? ptr : "";
        }

        void clear() noexcept {
            if (is_inline()) {
                store_inline(nullptr, 0);
                return;
            }
            // M4: old characters never stay behind as spare capacity.
            if (auto* ptr = data()) std::memset(ptr, 0, std::size_t(size_) + 1u);
            size_ = 0;
        }

        void reserve(std::size_t requested) {
            if (requested >= XBufferCore::MAX_PAYLOAD) throw XBadAlloc();
            auto new_cap = static_cast<std::uint32_t>(requested);
            if (new_cap <= capacity()) return;

            const auto alloc = bound_binding();
            if (is_inline()) {
                const auto count = inline_size();
                move_to_heap(alloc, inline_bytes(), count, new_cap);
                return;
            }
            auto* old_storage = data();
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, old_storage, static_cast<std::uint32_t>(capacity_ + 1),
                    static_cast<std::uint32_t>(new_cap + 1), alignof(char))) {
                (void)detail::rebuild_dynamic_array<char>(old_storage, std::size_t(new_cap) + 1u, size_ + 1u);
                capacity_ = new_cap;
                return;
            }
            auto* new_data = detail::begin_dynamic_array<char>(fixed_detail::allocate_dynamic_bytes(
                alloc, static_cast<std::uint32_t>(new_cap + 1), alignof(char)), std::size_t(new_cap) + 1u);

            if (auto* old_data = data()) {
                if (size_ != 0) std::memcpy(new_data, old_data, size_);
                new_data[size_] = '\0';
                fixed_detail::retire_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(capacity_ + 1));
            } else {
                new_data[0] = '\0';
            }

            fixed_detail::set_rel32(data_delta, &data_delta, new_data);
            capacity_ = new_cap;
        }

        void assign(std::string_view rhs) {
            if (is_inline()) {
                // A nonempty short value into a bound inline head stays
                // inline: the general path below would do the same after
                // its checks. rhs may alias the inline bytes (store_inline
                // reads them all first).
                if (rhs.size() - 1u < inline_max) [[likely]] {
                    store_inline(rhs.data(), static_cast<std::uint32_t>(rhs.size()));
                    return;
                }
            } else if (rhs.size() - 1u < capacity_) [[likely]] {
                // Nonempty and fits the out-of-line block. capacity_ != 0
                // without the inline flag implies a bound head with a live
                // block, and capacity_ < MAX_PAYLOAD: the checks below are
                // implied. rhs may alias the block (memmove).
                auto* const chars = fixed_detail::resolve_rel32_live<char>(&data_delta, data_delta);
                const auto new_size = static_cast<std::uint32_t>(rhs.size());
                const auto old_size = size_;
                std::memmove(chars, rhs.data(), new_size);
                chars[new_size] = '\0';
                // M4: zero the old characters after the terminator.
                if (old_size > new_size) std::memset(chars + new_size + 1, 0, old_size - new_size);
                size_ = new_size;
                return;
            }
            assign_general(rhs);
        }

    private:
        // assign() off its two fast paths. Clang inlines assign() at the
        // caller only with this part out of line. GCC and MSVC get back the
        // one function they had before the split (inlined or not as they
        // chose then).
#if defined(__clang__)
        XOFFSET_DETAIL_NOINLINE void assign_general(std::string_view rhs) {
#else
        XOFFSET_DETAIL_MOVE_INLINE void assign_general(std::string_view rhs) {
#endif
            if (rhs.size() >= XBufferCore::MAX_PAYLOAD) throw XBadAlloc();
            if (rhs.empty()) { clear(); return; }
            const auto new_size = static_cast<std::uint32_t>(rhs.size());
            if (is_inline() || data_delta == 0) {
                // No out-of-line storage: short values stay inline, longer
                // values get an exact heap block. rhs may alias the inline
                // bytes; it is read completely before the head is rewritten.
                if (new_size <= inline_max) {
                    store_inline(rhs.data(), new_size);
                    return;
                }
                move_to_heap(bound_binding(), rhs.data(), new_size, new_size);
                return;
            }
            auto* old_data = data();
            if (new_size <= capacity_) {
                const auto old_size = size_;
                std::memmove(old_data, rhs.data(), rhs.size());
                // M4: zero the terminator and any old characters after it.
                std::memset(old_data + new_size, 0, std::size_t(old_size > new_size ? old_size - new_size : 0) + 1u);
                size_ = new_size;
                return;
            }

            const auto alloc = binding();
            const auto old_address = reinterpret_cast<std::uintptr_t>(old_data);
            const auto rhs_address = reinterpret_cast<std::uintptr_t>(rhs.data());
            const auto old_extent = std::size_t(capacity_) + 1u;
            const bool aliases_old = old_data && rhs_address >= old_address &&
                rhs_address - old_address <= old_extent &&
                rhs.size() <= old_extent - (rhs_address - old_address);
            const auto alias_offset = aliases_old ? rhs_address - old_address : 0u;
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(capacity_ + 1),
                    static_cast<std::uint32_t>(new_size + 1), alignof(char))) {
                auto* expanded = detail::rebuild_dynamic_array<char>(
                    old_data, std::size_t(new_size) + 1u, size_ + 1u);
                // An old self-view does not automatically retarget through a
                // differently bounded parent array. Reacquire from new array.
                const char* input = aliases_old ? expanded + alias_offset : rhs.data();
                std::memmove(expanded, input, rhs.size());
                expanded[new_size] = '\0';
                size_ = new_size;
                capacity_ = new_size;
                return;
            }
            auto* fresh = detail::begin_dynamic_array<char>(fixed_detail::allocate_dynamic_bytes(
                alloc, static_cast<std::uint32_t>(new_size + 1), alignof(char)), std::size_t(new_size) + 1u);
            std::int32_t incoming_delta = 0;
            try {
                fixed_detail::set_rel32(incoming_delta, &data_delta, fresh);
            } catch (...) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, fresh, static_cast<std::uint32_t>(new_size + 1));
                throw;
            }
            // rhs may alias the old allocation, including its terminator.
            // Read it completely before the old bytes become a free-list node.
            std::memmove(fresh, rhs.data(), rhs.size());
            fresh[new_size] = '\0';
            const auto old_capacity = capacity_;
            data_delta = incoming_delta;
            size_ = new_size;
            capacity_ = new_size;
            fixed_detail::retire_dynamic_bytes(
                alloc, old_data, static_cast<std::uint32_t>(old_capacity + 1));
        }

    public:
        XFixedString& operator=(std::string_view rhs) {
            assign(rhs);
            return *this;
        }

        XFixedString& operator=(const char* rhs) {
            assign(rhs ? std::string_view(rhs) : std::string_view{});
            return *this;
        }

        // Appends rhs, which may view this string's own characters. Growth at
        // least doubles the capacity, so a run of appends is amortized O(1)
        // per character.
        XFixedString& append(std::string_view rhs) {
            const auto count = rhs.size();
            if (!is_inline() && count - 1u < std::size_t(capacity_ - size_)) [[likely]] {
                // Nonempty and fits the out-of-line block (capacity_ != 0
                // without the inline flag: bound, live block, below
                // MAX_PAYLOAD). The spare bytes are zero (M4).
                auto* const chars = fixed_detail::resolve_rel32_live<char>(&data_delta, data_delta);
                if (count <= 16) fixed_detail::move_short_bytes(chars + size_, rhs.data(), count);
                else std::memmove(chars + size_, rhs.data(), count);
                size_ += static_cast<std::uint32_t>(count);
                chars[size_] = '\0';
                return *this;
            }
            append_general(rhs);
            return *this;
        }

        XFixedString& operator+=(std::string_view rhs) { return append(rhs); }

        bool operator==(const XFixedString& rhs) const noexcept { return view() == rhs.view(); }
        bool operator==(std::string_view rhs) const noexcept { return view() == rhs; }
        bool operator==(const char* rhs) const noexcept {
            return view() == (rhs ? std::string_view(rhs) : std::string_view{});
        }
        bool operator<(const XFixedString& rhs) const noexcept { return view() < rhs.view(); }
        bool operator<(std::string_view rhs) const noexcept { return view() < rhs; }
        bool operator<(const char* rhs) const noexcept {
            return view() < (rhs ? std::string_view(rhs) : std::string_view{});
        }
        friend bool operator==(std::string_view lhs, const XFixedString& rhs) noexcept {
            return lhs == rhs.view();
        }
        friend bool operator==(const char* lhs, const XFixedString& rhs) noexcept {
            return (lhs ? std::string_view(lhs) : std::string_view{}) == rhs.view();
        }
        friend bool operator<(std::string_view lhs, const XFixedString& rhs) noexcept {
            return lhs < rhs.view();
        }
        friend bool operator<(const char* lhs, const XFixedString& rhs) noexcept {
            return (lhs ? std::string_view(lhs) : std::string_view{}) < rhs.view();
        }

        friend std::ostream& operator<<(std::ostream& os, const XFixedString& value) {
            return os << value.view();
        }

        // Moves this head's payload into dst (a freshly bound head) and empties
        // this head. Inline bytes are copied; out-of-line storage is rebased.
        void relocate_payload_to(XFixedString& dst) noexcept {
            if (is_inline()) {
                std::memcpy(dst.inline_bytes(), inline_bytes(), 12);
            } else {
                // Both heads are in the same bounded arena; the delta fits.
                fixed_detail::set_rel32(dst.data_delta, &dst.data_delta, data());
                dst.size_ = size_;
                dst.capacity_ = capacity_;
            }
            reset_head();
        }

    private:
        char* inline_bytes() noexcept { return reinterpret_cast<char*>(this); }
        const char* inline_bytes() const noexcept { return reinterpret_cast<const char*>(this); }
        std::uint32_t inline_size() const noexcept { return (capacity_ >> 24) & 0x7fu; }

        void reset_head() noexcept {
            data_delta = 0;
            size_ = 0;
            capacity_ = 0;
        }

        // src may alias these inline bytes: every source byte is loaded before
        // the head is written. The 12 bytes are assembled in two registers
        // (little-endian) and written with two stores; a byte-filled stack
        // buffer reloaded as words stalled store forwarding.
        void store_inline(const char* src, std::uint32_t count) noexcept {
            std::uint64_t lo = 0;
            std::uint32_t hi = 0;
            if (count >= 8) {
                std::memcpy(&lo, src, 8);
                if (count == 10) {
                    std::uint16_t tail;
                    std::memcpy(&tail, src + 8, 2);
                    hi = tail;
                } else if (count == 9) {
                    hi = static_cast<unsigned char>(src[8]);
                }
            } else if (count >= 4) {
                std::uint32_t head, tail;
                std::memcpy(&head, src, 4);
                std::memcpy(&tail, src + count - 4, 4);
                lo = head | (std::uint64_t(tail) << ((count - 4) * 8));
            } else if (count != 0) {
                lo = std::uint64_t(static_cast<unsigned char>(src[0])) |
                     (std::uint64_t(static_cast<unsigned char>(src[count >> 1])) << ((count >> 1) * 8)) |
                     (std::uint64_t(static_cast<unsigned char>(src[count - 1])) << ((count - 1) * 8));
            }
            hi |= (0x80u | count) << 24;
            std::memcpy(inline_bytes(), &lo, 8);
            std::memcpy(inline_bytes() + 8, &hi, 4);
        }

        // append() when the value is inline or null, or does not fit.
        XOFFSET_DETAIL_NOINLINE void append_general(std::string_view rhs) {
            if (rhs.empty()) return;
            const std::size_t old_size = size();
            if (rhs.size() >= XBufferCore::MAX_PAYLOAD - old_size) throw XBadAlloc();
            const auto new_size = static_cast<std::uint32_t>(old_size + rhs.size());
            if (new_size <= inline_max && (is_inline() || data_delta == 0)) {
                char bytes[inline_max];
                if (old_size != 0) std::memcpy(bytes, inline_bytes(), old_size);
                std::memcpy(bytes + old_size, rhs.data(), rhs.size());
                store_inline(bytes, new_size);
                return;
            }
            // rhs may view the current characters, which reserve() may move
            // (and retire, zeroed). The new storage repeats the old extent:
            // the characters, the terminator, then zero bytes (I1 inline
            // padding, M4 spare capacity). Read from there after the move.
            const char* const old_chars = data();
            const std::size_t extent = is_inline() ? 11u : std::size_t(capacity_) + 1u;
            const auto offset = reinterpret_cast<std::uintptr_t>(rhs.data()) -
                reinterpret_cast<std::uintptr_t>(old_chars);
            const bool aliases = old_chars && offset < extent;
            const auto grown = std::max<std::uint64_t>(new_size, std::uint64_t(capacity()) * 2u);
            reserve(static_cast<std::size_t>(std::min<std::uint64_t>(grown, XBufferCore::MAX_PAYLOAD - 1u)));
            auto* const chars = fixed_detail::resolve_rel32_live<char>(&data_delta, data_delta);
            std::memmove(chars + old_size, aliases ? chars + offset : rhs.data(), rhs.size());
            size_ = new_size;
            chars[new_size] = '\0';
        }

        // The head has no out-of-line storage (inline or null). src may alias
        // the inline bytes: it is copied before the head is rewritten.
        void move_to_heap(const allocator_type& alloc, const char* src,
                          std::uint32_t count, std::uint32_t new_cap) {
            auto* fresh = detail::begin_dynamic_array<char>(fixed_detail::allocate_dynamic_bytes(
                alloc, static_cast<std::uint32_t>(new_cap + 1), alignof(char)), std::size_t(new_cap) + 1u);
            std::int32_t incoming_delta = 0;
            try {
                fixed_detail::set_rel32(incoming_delta, &data_delta, fresh);
            } catch (...) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, fresh, static_cast<std::uint32_t>(new_cap + 1));
                throw;
            }
            if (count != 0) std::memmove(fresh, src, count);
            fresh[count] = '\0';
            data_delta = incoming_delta;
            size_ = count;
            capacity_ = new_cap;
        }

        void release() noexcept {
            if (is_inline()) {
                reset_head();
                return;
            }
            // Look up the binding only when there is a block to retire.
            release_in(data() ? binding() : allocator_type{});
        }

        // release() with the binding its container already looked up.
        void release_in(allocator_type alloc) noexcept {
            if (is_inline()) {
                reset_head();
                return;
            }
            auto* old_data = data();
            std::uint32_t old_cap = capacity_;

            data_delta = 0;
            size_ = 0;
            capacity_ = 0;

            if (old_data && alloc.segment) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(old_cap + 1));
            }
        }
    };

    template <typename T>
    struct XFixedVector {
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;
        using value_type = T;
        using size_type = std::uint32_t;
        using iterator = T*;
        using const_iterator = const T*;

        // size_ and capacity_ are char32_t (32-bit unsigned, converts like
        // std::uint32_t) because it is a distinct type for type-based alias
        // analysis: a store to an int or unsigned element cannot alias them,
        // so a push_back loop keeps capacity_ in a register.
        std::int32_t data_delta = 0;
        char32_t size_ = 0;
        char32_t capacity_ = 0;

        XFixedVector() = default;
        // As XFixedString: the head's address names its payload.
        explicit XFixedVector(allocator_type) noexcept {}
        ~XFixedVector() { release(); }
        XFixedVector(const XFixedVector& other, allocator_type alloc) : XFixedVector(alloc) {
            copy_from(other);
        }

        XFixedVector(const XFixedVector&) = delete;
        XFixedVector(XFixedVector&&) = delete;
        XFixedVector& operator=(const XFixedVector& other) {
            if constexpr (detail::subtree_copy_supported<T>() && std::is_trivially_copyable_v<T> &&
                          std::is_trivially_copy_constructible_v<T> &&
                          std::is_trivially_destructible_v<T> &&
                          !std::is_const_v<T> && !std::is_volatile_v<T>) {
                // T4 POD in place: assign_range's in-place case and the M6
                // padding pass of assign_trivial_copy, inline. Regrowth,
                // binding and an empty source take the general path.
                const size_type count = other.size_;
                if (this != &other && count != 0 && count <= capacity_ &&
                    detail::subtree_copy_enabled()) [[likely]] {
                    T* items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                    const size_type old_size = size_;
                    std::memmove(static_cast<void*>(items), other.data(), std::size_t(count) * sizeof(T));
                    size_ = count;
                    if (old_size > count)
                        std::memset(static_cast<void*>(items + count), 0, std::size_t(old_size - count) * sizeof(T));
                    for (size_type i = 0; i < count; ++i) detail::zero_implicit_padding(items + i);
                    XOFFSET_DETAIL_SUBTREE_COUNT(pod_hits, 1);
                    return *this;
                }
                return assign_general_out_of_line(other);
            } else {
                return assign_general(other);
            }
        }

    private:
        // The POD fast path's fallback stays out of line, so that path inlines small.
        XOFFSET_DETAIL_NOINLINE XFixedVector& assign_general_out_of_line(const XFixedVector& other) {
            return assign_general(other);
        }

        XFixedVector& assign_general(const XFixedVector& other) {
            if (this == &other) return *this;
            // An empty source keeps the capacity and needs no binding.
            if (other.size_ == 0) { clear(); return *this; }
            // T4: a trivially copyable element is one range copy; any other
            // admitted subtree is copied into one fresh segment.
            // The range copy looks the binding up only when it regrows;
            // rebuild() checks it for the other paths.
            if constexpr (detail::subtree_copy_supported<T>()) {
                if (detail::subtree_copy_enabled()) {
                    if constexpr (std::is_trivially_copyable_v<T>) {
                        assign_trivial_copy(other);
                        return *this;
                    } else {
                        // Unbound, it falls through to rebuild(), which throws.
                        if (detail::subtree_copy_assign(*this, other)) return *this;
                    }
                }
            } else {
                XOFFSET_DETAIL_SUBTREE_COUNT(fallback_type, 1);
            }
            const auto alloc = element_binding();
            rebuild(other.size_, std::max(capacity_, other.size_), [&](T* dst, size_type i) {
                fixed_detail::transfer_construct(dst, other.data()[i], alloc);
            });
            return *this;
        }

    public:
        XFixedVector& operator=(XFixedVector&& other) {
            if (this == &other) return *this;
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                if (auto* const shared = reinterpret_cast<XArena*>(detail::OwnerMap::same_payload(this, &other))) {
                    // Both heads lie in one payload (one owner-table read).
                    if (fixed_detail::move_assignment_disjoint(*this, other)) {
                        std::int32_t incoming_delta = 0;
                        fixed_detail::set_rel32(incoming_delta, &data_delta, other.data());
                        const auto incoming_size = other.size_;
                        const auto incoming_capacity = other.capacity_;
                        // Out-of-line elements keep their addresses and bindings.
                        // Ancestor/descendant ownership has already been excluded.
                        // MV: release()'s empty-destination case inline (data()
                        // is null exactly when data_delta == 0, and release()
                        // then only clears the descriptor); otherwise release()
                        // with the binding just looked up.
                        if (data_delta == 0) {
                            size_ = 0;
                            capacity_ = 0;
                        } else {
                            release_in(allocator_type{shared});
                        }
                        data_delta = incoming_delta;
                        size_ = incoming_size;
                        capacity_ = incoming_capacity;
                        other.data_delta = 0;
                        other.size_ = 0;
                        other.capacity_ = 0;
                        return *this;
                    }
                }
            }
            return (*this = static_cast<const XFixedVector&>(other));
        }

        // Bound = the head lies in a registered payload. Each call is one
        // owner-table read: callers look the binding up only to allocate,
        // grow or retire a block, once per operation (V2_FORMAT, Owner lookup).
        bool is_bound() const noexcept { return arena() != nullptr; }

        allocator_type binding() const noexcept {
            return {arena()};
        }

        // One lookup that both checks and returns the binding.
        XOFFSET_DETAIL_MOVE_INLINE allocator_type bound_binding() const {
            auto* const a = arena();
            if (!a) [[unlikely]] throw_unbound();
            return {a};
        }
        [[noreturn]] XOFFSET_DETAIL_MOVE_COLD static void throw_unbound() {
            throw XException("XFixedVector needs a block but lies outside every XOffset payload");
        }

        // Elements without heads never use the binding, so building one does
        // not look it up.
        allocator_type element_binding() const noexcept {
            if constexpr (std::is_trivially_copyable_v<T>) return {};
            else return binding();
        }

        XOFFSET_DETAIL_MOVE_INLINE XArena* arena() const noexcept {
            return reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(this));
        }

        stored_allocator_type get_stored_allocator() const noexcept {
            return {arena()};
        }

        T* data() noexcept {
            return fixed_detail::resolve_rel32<T>(&data_delta, data_delta);
        }

        const T* data() const noexcept {
            return fixed_detail::resolve_rel32_const<T>(&data_delta, data_delta);
        }

        size_type size() const noexcept { return size_; }
        size_type capacity() const noexcept { return capacity_; }
        bool empty() const noexcept { return size_ == 0; }

        T& operator[](size_type index) noexcept { return data()[index]; }
        const T& operator[](size_type index) const noexcept { return data()[index]; }

        iterator begin() noexcept { return data(); }
        iterator end() noexcept { return data() ? data() + size_ : nullptr; }
        const_iterator begin() const noexcept { return data(); }
        const_iterator end() const noexcept { return data() ? data() + size_ : nullptr; }
        T& front() noexcept { return data()[0]; }
        const T& front() const noexcept { return data()[0]; }
        T& back() noexcept { return data()[size_ - 1]; }
        const T& back() const noexcept { return data()[size_ - 1]; }

        void clear() noexcept { truncate(0); }

        // Destroys the elements from count on with one binding lookup (rule 5).
        void truncate(size_type count) noexcept {
            const auto old_size = size_;
            if (count >= old_size) return;
            if constexpr (!std::is_trivially_destructible_v<T>)
                fixed_detail::destroy_n_in(binding(), data() + count, old_size - count);
            size_ = count;
            scrub(count, old_size);
        }

        void reserve(std::size_t requested) {
            auto new_cap = checked_count(requested);
            if (new_cap <= capacity_) return;
            const auto alloc = bound_binding();
            if (capacity_ == 0) {
                // First storage: nothing to expand, move or retire.
                auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes(
                    alloc, static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T)), new_cap);
                fixed_detail::set_rel32(data_delta, &data_delta, fresh);
                capacity_ = new_cap;
                return;
            }
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, data(), static_cast<std::uint32_t>(capacity_ * sizeof(T)),
                    static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T))) {
                (void)detail::rebuild_dynamic_array<T>(data(), new_cap, size_);
                capacity_ = new_cap;
                return;
            }
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                reserve_relocate(new_cap, alloc);
            } else {
                rebuild(size_, new_cap, [&](T* dst, size_type i) {
                    fixed_detail::transfer_construct(dst, data()[i], alloc);
                });
            }
        }

    private:
        // reserve into a fresh block for relocatable T; out of line, so the
        // paths above keep their size at the caller.
        XOFFSET_DETAIL_NOINLINE void reserve_relocate(size_type new_cap, allocator_type alloc) {
            if (new_cap > max_size()) throw XBadAlloc();
            bool published = false;
            fixed_detail::regrow_dynamic_bytes(alloc, static_cast<std::uint32_t>(capacity_ * sizeof(T)),
                                               static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T),
                                               [&](void* block) -> void* {
                auto* fresh = detail::begin_dynamic_array<T>(block, new_cap);
                // An unreachable block is never published and is retired as "old".
                if (!fixed_detail::rel32_reaches(&data_delta, fresh)) [[unlikely]] return block;
                auto* old = data();
                relocate_live_run(fresh, old);
                fixed_detail::set_rel32(data_delta, &data_delta, fresh);
                capacity_ = new_cap;
                published = true;
                return old;
            });
            if (!published) [[unlikely]] fixed_detail::throw_rel32_range();
        }

    public:
        // Replaces all elements from stable live T objects. Self/subrange and
        // other live containers (including another arena) may overlap. Private
        // allocator control bytes that allocation itself mutates are not a
        // stable source range. No default construction or old-prefix copy is
        // needed for these implicit-lifetime, trivially destructible objects.
        void assign_range(std::span<const T> values)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            const auto count = checked_count(values.size());
            if (count == 0) { clear(); return; }
            // count <= capacity_ means a live block, so the head is bound; only
            // assign_range_regrow looks the binding up.
            const auto bytes = static_cast<std::uint32_t>(count * sizeof(T));
            if (count <= capacity_) {
                const auto old_size = size_;
                std::memmove(data(), values.data(), bytes);
                size_ = count;
                scrub(count, old_size);
                return;
            }
            assign_range_regrow(values, count, bytes);
        }

        // assign_range into a fresh block; out of line, so the in-capacity
        // path above stays small enough to inline at the caller.
        XOFFSET_DETAIL_NOINLINE void assign_range_regrow(std::span<const T> values, size_type count,
                                                         std::uint32_t bytes) {
            const auto alloc = bound_binding();
            // D8a: a container that already has capacity regrows to
            // max(count, 2 * capacity), like libc++ assign; the first block
            // stays exact. Fresh blocks are zero, so the spare tail is too (M4).
            const size_type new_cap = capacity_ == 0 ? count
                : std::max(count, static_cast<size_type>(
                      std::min<std::uint64_t>(max_size(), std::uint64_t(capacity_) * 2u)));
            const auto cap_bytes = static_cast<std::uint32_t>(new_cap * sizeof(T));
            // Read the complete source before publishing/retiring old storage.
            // Retirement is allowed to overwrite its first bytes as free-list
            // metadata. memmove also covers overlapping live-element ranges.
            bool published = false;
            fixed_detail::regrow_dynamic_bytes(alloc, static_cast<std::uint32_t>(capacity_ * sizeof(T)),
                                               cap_bytes, alignof(T), [&](void* block) -> void* {
                auto* fresh = detail::begin_dynamic_array<T>(block, new_cap);
                // An unreachable block is never published and is retired as "old".
                if (!fixed_detail::rel32_reaches(&data_delta, fresh)) [[unlikely]] return block;
                std::memmove(fresh, values.data(), bytes);
                void* old = data();
                fixed_detail::set_rel32(data_delta, &data_delta, fresh);
                size_ = count;
                capacity_ = new_cap;
                published = true;
                return old;
            });
            if (!published) [[unlikely]] fixed_detail::throw_rel32_range();
        }

        // N7: replaces all elements with [first, last). A contiguous range of
        // trivially copyable T takes the assign_range path (one exact block,
        // aliasing-safe, no default fill). Other forward ranges are built in a
        // fresh exact block before the old storage is released, so the source
        // may alias this container or its children.
        template <std::forward_iterator It, std::sentinel_for<It> S>
            requires std::constructible_from<T, std::iter_reference_t<It>> ||
                     requires(T* dst, It it, allocator_type alloc) {
                         fixed_detail::construct_bound(dst, alloc, *it);
                     }
        void assign(It first, S last) {
            if constexpr (std::contiguous_iterator<It> && std::sized_sentinel_for<S, It> &&
                          std::is_same_v<std::remove_cv_t<std::iter_value_t<It>>, T> &&
                          std::is_trivially_copyable_v<T> &&
                          std::is_trivially_copy_constructible_v<T> &&
                          std::is_trivially_destructible_v<T> &&
                          !std::is_const_v<T> && !std::is_volatile_v<T>) {
                const auto count = static_cast<std::size_t>(last - first);
                assign_range(std::span<const T>(count ? std::to_address(first) : nullptr, count));
            } else {
                const auto count = checked_count(static_cast<std::size_t>(std::ranges::distance(first, last)));
                if (count == 0) { clear(); return; }
                rebuild(count, count, [&](T* dst, size_type) {
                    fixed_detail::construct_bound(dst, element_binding(), *first);
                    ++first;
                });
            }
        }

        void push_back(const T& value) {
            const size_type n = size_;
            if (n < capacity_) [[likely]] {
                // As insert_construct's append fast path, kept here so the
                // caller's loop builds no closure for the general call.
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
                fixed_detail::transfer_construct(slot, value, element_binding());
                size_ = n + 1u;
                return;
            }
            const Grown grown = [&] {
                if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) return push_back_general<T>(value);
                else return push_back_general<const T&>(value);
            }();
            // The general path appended exactly one element. Restating the
            // head here makes it known on both paths, so a caller's push_back
            // loop keeps size_ and capacity_ in registers instead of
            // reloading them through memory.
            data_delta = grown.data_delta;
            capacity_ = grown.capacity;
            size_ = n + 1u;
        }

        void push_back(T&& value) {
            push_back(static_cast<const T&>(value));
        }

        // Stable, decision-first deletion. Predicate receives const T& exactly
        // once in original order. It may throw, but must not structurally mutate
        // this target, end its/its elements' lifetime or release its reservation.
        // This quiescence covers ALL callbacks from the operation's entry,
        // including bitmap new_handler, predicates, destructors and deallocation
        // callbacks. An enclosing vector must not relocate this head meanwhile.
        // Core move retaining the reservation/head address is permitted.
        // This does not enlarge erase_index's destructive reentry contract.
        // User side effects through external aliases are not rolled back.
        template <typename Predicate>
            requires (fixed_detail::stable_erase_supported<T>() &&
                      std::is_invocable_r_v<bool, Predicate&, const T&>)
        size_type stable_erase_if(Predicate&& predicate) {
            const auto original_size = size_;
            if (original_size == 0) return 0;
            const auto alloc = bound_binding();
            const auto original_data_delta = data_delta;
            const auto original_capacity = capacity_;
            auto* const items = data();
            fixed_detail::StableEraseOwnerWitness witness(alloc, this, sizeof(*this));
            const auto decision_bytes = (std::size_t(original_size) + 7u) / 8u;
            auto decisions = std::make_unique<std::uint8_t[]>(decision_bytes);
            // Preparing host scratch can call new_handler. Recheck reservation
            // identity before touching the in-arena descriptor or old items.
            auto validate_target = [&] {
                witness.validate();
                if (data_delta != original_data_delta ||
                    size_ != original_size || capacity_ != original_capacity)
                    throw XException("stable_erase_if target changed during decisions");
            };
            validate_target();
            size_type removed = 0;
            for (size_type read = 0; read < original_size; ++read) {
                const bool erase = std::invoke(predicate, std::as_const(items[read]));
                validate_target();
                if (erase) {
                    decisions[read / 8u] |= std::uint8_t(1u << (read % 8u));
                    ++removed;
                }
            }
            if (removed == 0) return 0;
            // All potentially throwing library preparation and predicate calls
            // completed. Every write slot is dead: it was removed earlier or
            // was the source of an earlier successful relocation.
            size_type write = 0;
            for (size_type read = 0; read < original_size; ++read) {
                if (decisions[read / 8u] & std::uint8_t(1u << (read % 8u))) {
                    std::destroy_at(items + read);
                } else {
                    if (write != read) {
                        fixed_detail::stable_erase_relocate(items + write, items[read]);
                        std::destroy_at(items + read);
                    }
                    ++write;
                }
            }
            size_ = write; // publication follows the complete no-throw commit
            scrub(write, original_size);
            return removed;
        }

        void pop_back() {
            if (size_ == 0) return;
            fixed_detail::destroy_n(data() + (size_ - 1), 1);
            --size_;
            scrub(size_, size_ + 1);
        }

        T& emplace_back() {
            return *append_construct([&](T* dst) {
                fixed_detail::default_construct(dst, element_binding());
            });
        }

        template <typename... Args>
            requires (sizeof...(Args) > 0)
        T& emplace_back(Args&&... args) {
            return *append_construct([&](T* dst) {
                fixed_detail::construct_bound(dst, element_binding(), std::forward<Args>(args)...);
            });
        }

    protected:
        struct Grown {
            std::int32_t data_delta;
            char32_t capacity;
        };

        // push_back beyond capacity or on an unbound head. A small trivially
        // copyable value arrives by value (registers), not by reference. The
        // head fields it changed come back in registers for push_back to
        // restate.
        template <typename V>
        XOFFSET_DETAIL_NOINLINE Grown push_back_general(V value) {
            append_construct([&](T* dst) {
                fixed_detail::transfer_construct(dst, static_cast<const T&>(value), element_binding());
            });
            return {data_delta, capacity_};
        }

        static size_type checked_count(std::size_t count) {
            if (count > max_size()) throw XBadAlloc();
            return static_cast<size_type>(count);
        }

        static constexpr size_type max_size() noexcept {
            return static_cast<size_type>((XBufferCore::MAX_PAYLOAD - 8u) / sizeof(T));
        }

        size_type grown_capacity() const {
            if (size_ == max_size()) throw XBadAlloc();
            return std::max(size_ + 1u, static_cast<size_type>(
                std::min<std::uint64_t>(max_size(), capacity_ ? std::uint64_t(capacity_) * 2u : 4u)));
        }

        template <bool OutOfLine>
        void grow_for_insert() {
            if constexpr (OutOfLine) reserve_for_insert(grown_capacity());
            else reserve(grown_capacity());
        }
        XOFFSET_DETAIL_NOINLINE void reserve_for_insert(size_type new_cap) { reserve(new_cap); }

        // Build in arena storage and publish only after all potentially throwing work.
        template <typename Construct>
        void rebuild(size_type count, size_type cap, Construct&& construct) {
            const auto alloc = bound_binding();
            if (cap > max_size() || count > cap) throw XBadAlloc();
            if (cap == 0) { clear(); return; }
            auto bytes = static_cast<std::uint32_t>(cap * sizeof(T));
            auto* fresh = detail::begin_dynamic_array<T>(
                fixed_detail::allocate_dynamic_bytes(alloc, bytes, alignof(T)), cap);
            size_type constructed = 0;
            std::int32_t delta = 0;
            try {
                fixed_detail::set_rel32(delta, &data_delta, fresh);
                for (; constructed < count; ++constructed) construct(fresh + constructed, constructed);
            } catch (...) {
                fixed_detail::destroy_n(fresh, constructed);
                fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
                throw;
            }
            auto* old = data();
            auto old_size = size_;
            auto old_cap = capacity_;
            data_delta = delta;
            size_ = count;
            capacity_ = cap;
            fixed_detail::destroy_n(old, old_size);
            fixed_detail::retire_dynamic_bytes(alloc, old, static_cast<std::uint32_t>(old_cap * sizeof(T)));
        }

        // Appends (emplace_back, push_back_general) keep the general path in
        // the same function: under Clang a call to it there costs more than
        // it saves (slg.A). GCC and MSVC take insert_construct as before.
        template <typename Construct>
#if defined(__clang__)
        iterator append_construct(Construct&& construct) {
            if (size_ < capacity_) [[likely]] {
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + size_;
                construct(slot);
                ++size_;
                return slot;
            }
            return insert_body<false>(size_, construct);
        }
#else
        XOFFSET_DETAIL_MOVE_INLINE iterator append_construct(Construct&& construct) {
            return insert_construct(size_, std::forward<Construct>(construct));
        }
#endif

        // GrowOutOfLine: an insert into a full vector grows through an
        // out-of-line reserve. Flat set/map inserts use it: with reserve
        // inlined there, GCC stops inlining their insert/try_emplace.
        template <bool GrowOutOfLine = false, typename Construct>
        iterator insert_construct(size_type index, Construct&& construct) {
            if (index == size_ && size_ < capacity_) [[likely]] {
                // Append into spare capacity. capacity_ != 0 implies a bound
                // head with a live block (data_delta != 0), and size_ <
                // capacity_ <= max_size(): the checks below are implied.
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + size_;
                construct(slot);
                ++size_;
                return slot;
            }
#if defined(__clang__)
            if constexpr (std::is_trivially_copyable_v<T>) {
                const size_type n = size_;
                if (index < n && n < capacity_) [[likely]] {
                    // Insert into spare capacity, inline. As on the append
                    // path the checks of insert_general are implied, and the
                    // head stays in registers across the move.
                    alignas(T) std::byte storage[sizeof(T)];
                    auto* staged = reinterpret_cast<T*>(storage);
                    construct(staged);
                    T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                    // Nothing below throws, so size_ is published before the
                    // move: the next operation's head load then does not wait
                    // on a store still in flight behind the move.
                    size_ = n + 1u;
                    std::memmove(items + index + 1, items + index, std::size_t(n - index) * sizeof(T));
                    std::memcpy(items + index, staged, sizeof(T));
                    return items + index;
                }
            }
            return insert_general<GrowOutOfLine>(index, construct);
#else
            // GCC and MSVC keep the one function they had: with the insert
            // fast path, GCC 16 inlined less into push_back_general.
            return insert_body<GrowOutOfLine>(index, construct);
#endif
        }

#if defined(__clang__)
        // insert_construct off its fast paths. Clang inlines the fast paths
        // at the caller only with this part in a separate function.
        template <bool GrowOutOfLine, typename Construct>
        iterator insert_general(size_type index, Construct& construct) {
            return insert_body<GrowOutOfLine>(index, construct);
        }
#endif

        template <bool GrowOutOfLine, typename Construct>
        XOFFSET_DETAIL_MOVE_INLINE iterator insert_body(size_type index, Construct& construct) {
            if (index > size_ || size_ == max_size()) throw XBadAlloc();
            if (index == size_ && append_grow(construct)) {
                // Append into a full vector: built in place in the grown block.
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                // A native temporary is safe only for types without heads.
                alignas(T) std::byte storage[sizeof(T)];
                auto* staged = reinterpret_cast<T*>(storage);
                construct(staged);
                if (size_ == capacity_) grow_for_insert<GrowOutOfLine>();
                std::memmove(data() + index + 1, data() + index, (size_ - index) * sizeof(T));
                std::memcpy(data() + index, staged, sizeof(T));
                ++size_;
            } else if constexpr (detail::shift_relocate_supported<T>()) {
                with_staged(construct, [&](T& staged) {
                    if (size_ == capacity_) grow_for_insert<GrowOutOfLine>();
                    // SHIFT: same bytes and deltas as the loop below, in one
                    // memmove plus an inline-head restart of the moved run.
                    auto* items = data();
                    const auto moved = size_ - index;
                    std::memmove(static_cast<void*>(items + index + 1), static_cast<const void*>(items + index), std::size_t(moved) * sizeof(T));
                    detail::shift_rebase_moved(items + index + 1, moved,
                        -static_cast<std::int32_t>(sizeof(T)));
                    detail::empty_vacated_slot(items + index);
                    std::destroy_at(items + index);
                    fixed_detail::relocate_construct(items + index, staged);
                    ++size_;
                });
            } else if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                with_staged(construct, [&](T& staged) {
                    if (size_ == capacity_) grow_for_insert<GrowOutOfLine>();
                    for (size_type i = size_; i > index; --i) {
                        fixed_detail::relocate_construct(data() + i, data()[i - 1]);
                        std::destroy_at(data() + i - 1);
                    }
                    fixed_detail::relocate_construct(data() + index, staged);
                    ++size_;
                });
            } else {
                const auto alloc = element_binding();
                rebuild(size_ + 1, size_ == capacity_ ? grown_capacity() : capacity_,
                    [&](T* dst, size_type i) {
                        if (i == index) construct(dst);
                        else fixed_detail::transfer_construct(dst, data()[i < index ? i : i - 1], alloc);
                    });
            }
            return data() + index;
        }

        void erase_index(size_type index) {
            if (index >= size_) return;
            if (index + 1 == size_) { pop_back(); return; }
            if constexpr (std::is_trivially_copyable_v<T>) {
#if defined(__clang__)
                // size_ > 1 names a live block. The head stays in registers
                // across the move (Clang reloads it after the memmove
                // otherwise); the vacated slot is zeroed as by scrub(n - 1, n).
                // GCC and MSVC keep the code they had.
                const size_type n = size_;
                T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                std::memmove(items + index, items + index + 1, std::size_t(n - index - 1) * sizeof(T));
                size_ = n - 1u;
                std::memset(static_cast<void*>(items + (n - 1u)), 0, sizeof(T));
#else
                std::memmove(data() + index, data() + index + 1, (size_ - index - 1) * sizeof(T));
                --size_;
                scrub(size_, size_ + 1);
#endif
            } else if constexpr (detail::shift_relocate_supported<T>()) {
                // SHIFT: same bytes and deltas as the loop below, in one
                // memmove plus an inline-head restart of the moved run.
                auto* items = data();
                const auto moved = size_ - index - 1;
                std::destroy_at(items + index);
#if defined(__linux__)
                fixed_detail::overlap_move_down(static_cast<void*>(items + index), static_cast<const void*>(items + index + 1), std::size_t(moved) * sizeof(T));
#else
                std::memmove(static_cast<void*>(items + index), static_cast<const void*>(items + index + 1), std::size_t(moved) * sizeof(T));
#endif
                detail::shift_rebase_moved(items + index, moved, static_cast<std::int32_t>(sizeof(T)));
                detail::empty_vacated_slot(items + size_ - 1);
                std::destroy_at(items + size_ - 1);
                --size_;
                scrub(size_, size_ + 1);
            } else if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                std::destroy_at(data() + index);
                for (size_type i = index; i + 1 < size_; ++i) {
                    fixed_detail::relocate_construct(data() + i, data()[i + 1]);
                    std::destroy_at(data() + i + 1);
                }
                --size_;
                scrub(size_, size_ + 1);
            } else {
                const auto alloc = element_binding();
                rebuild(size_ - 1, capacity_, [&](T* dst, size_type i) {
                    fixed_detail::transfer_construct(dst, data()[i < index ? i : i + 1], alloc);
                });
            }
        }

        template <typename Construct>
        void replace_construct(size_type index, Construct&& construct) {
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                with_staged(construct, [&](T& staged) {
                    std::destroy_at(data() + index);
                    fixed_detail::relocate_construct(data() + index, staged);
                });
            } else {
                const auto alloc = element_binding();
                rebuild(size_, capacity_, [&](T* dst, size_type i) {
                    if (i == index) construct(dst);
                    else fixed_detail::transfer_construct(dst, data()[i], alloc);
                });
            }
        }

        // Append to a full vector without a staged element: grow the block
        // (in place at the arena tail, else a fresh block), build the new
        // element directly at its final slot, then relocate the old run.
        // The new element is built while the old run is still intact, so
        // arguments that alias old elements stay valid. If construct
        // throws, a fresh block is retired and the old run is untouched; a
        // tail expansion stays, as after a successful reserve. Returns false
        // for types that keep the general paths in insert_construct.
        template <typename Construct>
        bool append_grow(Construct& construct) {
            if constexpr (std::is_trivially_copyable_v<T> ||
                          !fixed_detail::can_relocate_without_allocation<T>()) {
                return false;
            } else {
                const auto new_cap = grown_capacity();
                const auto alloc = bound_binding();
                const auto new_bytes = static_cast<std::uint32_t>(new_cap * sizeof(T));
                if (fixed_detail::try_expand_dynamic_bytes(
                        alloc, data(), static_cast<std::uint32_t>(capacity_ * sizeof(T)), new_bytes, alignof(T))) {
                    (void)detail::rebuild_dynamic_array<T>(data(), new_cap, size_);
                    capacity_ = new_cap;
                    construct(data() + size_);
                    ++size_;
                    return true;
                }
                auto* fresh = detail::begin_dynamic_array<T>(
                    fixed_detail::allocate_dynamic_bytes(alloc, new_bytes, alignof(T)), new_cap);
                std::int32_t delta = 0;
                try {
                    fixed_detail::set_rel32(delta, &data_delta, fresh);
                    construct(fresh + size_);
                } catch (...) {
                    fixed_detail::retire_dynamic_bytes(alloc, fresh, new_bytes);
                    throw;
                }
                auto* old = data();
                const auto old_cap = capacity_;
                relocate_live_run(fresh, old);
                data_delta = delta;
                capacity_ = new_cap;
                ++size_;
                fixed_detail::retire_dynamic_bytes(alloc, old, static_cast<std::uint32_t>(old_cap * sizeof(T)));
                return true;
            }
        }

        // Moves the size_ live elements from `old` into the separate fresh
        // block and ends their lifetimes in `old` (left emptied, bindings
        // kept). SHIFT types: one memcpy plus an inline-head rebase by the
        // block distance; same bytes and deltas as the per-element loop.
        void relocate_live_run(T* fresh, T* old) noexcept
            requires (fixed_detail::can_relocate_without_allocation<T>()) {
            if (size_ == 0) return;
            if constexpr (detail::shift_relocate_supported<T>()) {
                std::memcpy(static_cast<void*>(fresh), static_cast<const void*>(old),
                            std::size_t(size_) * sizeof(T));
                detail::shift_rebase_moved(fresh, size_, static_cast<std::int32_t>(
                    reinterpret_cast<const char*>(old) - reinterpret_cast<const char*>(fresh)));
                for (size_type i = 0; i < size_; ++i) detail::empty_vacated_slot(old + i);
            } else {
                for (size_type i = 0; i < size_; ++i) {
                    fixed_detail::relocate_construct(fresh + i, old[i]);
                }
            }
            fixed_detail::destroy_n(old, size_);
        }

        template <typename Construct, typename Commit>
        void with_staged(Construct&& construct, Commit&& commit) {
            const auto alloc = bound_binding();
            auto* staged = detail::begin_dynamic_array<T>(
                fixed_detail::allocate_dynamic_bytes(alloc, sizeof(T), alignof(T)), 1u);
            bool built = false;
            try {
                construct(staged);
                built = true;
                commit(*staged);
            } catch (...) {
                if (built) std::destroy_at(staged);
                fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                throw;
            }
            std::destroy_at(staged);
            fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
        }

        // Only the public default resize of scalar T enters this path.
        // T{} and destruction cannot run user code; fill/custom construction
        // keeps the general transactional path below.
        void resize_default_scalar(size_type count) requires std::is_scalar_v<T> {
            if (count > max_size()) throw XBadAlloc();
            if (count <= size_) {
                const auto old_size = size_;
                size_ = count;
                scrub(count, old_size);
                return;
            }
            const auto old_size = size_;
            if (count <= capacity_) {
                std::uninitialized_value_construct_n(data() + old_size, count - old_size);
                size_ = count;
                return;
            }
            const auto alloc = bound_binding();
            auto* old = data();
            const auto bytes = static_cast<std::uint32_t>(count * sizeof(T));
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, old, static_cast<std::uint32_t>(capacity_ * sizeof(T)), bytes, alignof(T))) {
                // VM preparation completed before publishing the enlarged
                // allocation. Required suffix initialization is nonthrowing.
                auto* expanded = detail::rebuild_dynamic_array<T>(old, count, old_size);
                std::uninitialized_value_construct_n(expanded + old_size, count - old_size);
                size_ = count;
                capacity_ = count;
                return;
            }
            resize_construct(count, [&](T* dst) {
                fixed_detail::default_construct(dst, alloc);
            });
        }

        template <typename Construct>
        void resize_construct(size_type count, Construct&& construct) {
            if (count > max_size()) throw XBadAlloc();
            if (count <= size_) {
                truncate(count);
            } else if (count <= capacity_) {
                auto old_size = size_;
                try {
                    while (size_ < count) { construct(data() + size_); ++size_; }
                } catch (...) {
                    while (size_ > old_size) pop_back();
                    throw;
                }
            } else if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                // Stage every potentially throwing operation before touching the
                // old prefix. In particular, fill arguments may refer to an old
                // element or one of its owned subobjects.
                const auto alloc = bound_binding();
                const auto old_size = size_;
                const auto old_cap = capacity_;
                auto* old = data();
                const auto bytes = static_cast<std::uint32_t>(count * sizeof(T));
                auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes(
                    alloc, bytes, alignof(T)), count);
                size_type tail_built = 0;
                std::int32_t delta = 0;
                try {
                    fixed_detail::set_rel32(delta, &data_delta, fresh);
                    for (; tail_built < count - old_size; ++tail_built)
                        construct(fresh + old_size + tail_built);
                } catch (...) {
                    fixed_detail::destroy_n(fresh + old_size, tail_built);
                    fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
                    throw;
                }
                // Existing bounded-arena relocation rebases inline heads,
                // retains their bindings, and empties source ownership. Its
                // trait also requires nonthrowing destruction. No fill argument
                // is read after this point.
                for (size_type i = 0; i < old_size; ++i)
                    fixed_detail::relocate_construct(fresh + i, old[i]);
                fixed_detail::destroy_n(old, old_size);
                data_delta = delta;
                size_ = count;
                capacity_ = count;
                fixed_detail::retire_dynamic_bytes(
                    alloc, old, static_cast<std::uint32_t>(old_cap * sizeof(T)));
            } else {
                const auto alloc = element_binding();
                rebuild(count, count, [&](T* dst, size_type i) {
                    if (i < size_) fixed_detail::transfer_construct(dst, data()[i], alloc);
                    else construct(dst);
                });
            }
        }

    private:
        // M4 (threat model T5/P2): storage of destroyed elements is zeroed, so
        // a later save never emits the old values as spare capacity.
        void scrub(size_type from, size_type to) noexcept {
            if (to <= from) return;
            if (auto* items = data())
                std::memset(static_cast<void*>(items + from), 0, std::size_t(to - from) * sizeof(T));
        }

        void copy_from(const XFixedVector& other) {
            if (other.size_ == 0) return;
            if constexpr (detail::subtree_copy_supported<T>()) {
                if (detail::subtree_copy_enabled()) {
                    if constexpr (std::is_trivially_copyable_v<T>) {
                        assign_trivial_copy(other);
                        return;
                    } else if (detail::subtree_copy_assign(*this, other)) {
                        return;
                    }
                }
            } else {
                XOFFSET_DETAIL_SUBTREE_COUNT(fallback_type, 1);
            }
            const auto alloc = element_binding();
            reserve(other.size_);
            size_type constructed = 0;
            try {
                for (; constructed < other.size_; ++constructed) {
                    fixed_detail::transfer_construct(
                        data() + constructed, other.data()[constructed], alloc);
                }
            } catch (...) {
                fixed_detail::destroy_n(data(), constructed);
                size_ = 0;
                throw;
            }
            size_ = other.size_;
        }

        // T4 POD: assign_range (in place when the capacity suffices, else one
        // exact block), then the M6 padding zeroing transfer_construct does.
        void assign_trivial_copy(const XFixedVector& other) {
            assign_range(std::span<const T>(other.data(), other.size_));
            auto* items = data();
            for (size_type i = 0; i < size_; ++i) detail::zero_implicit_padding(items + i);
            XOFFSET_DETAIL_SUBTREE_COUNT(pod_hits, 1);
        }

        void release() noexcept {
            // Look up the binding only when there is a block to retire.
            release_in(data() ? binding() : allocator_type{});
        }

        // release() with the binding its container already looked up; the
        // elements reuse it (rule 5).
        void release_in(allocator_type alloc) noexcept {
            T* old_data = data();
            size_type old_size = size_;
            size_type old_cap = capacity_;

            data_delta = 0;
            size_ = 0;
            capacity_ = 0;

            if (old_data && alloc.segment) {
                fixed_detail::destroy_n_in(alloc, old_data, old_size);
                fixed_detail::retire_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(old_cap * sizeof(T)));
            }
        }

    };

    template <typename T>
    class XFixedFlatSet : public XFixedVector<T> {
        using Base = XFixedVector<T>;

    public:
        using value_type = typename Base::value_type;
        using allocator_type = typename Base::allocator_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using Base::Base;

        template <typename KeyLike>
        iterator find(const KeyLike& key) {
            auto* items = Base::data();
            auto index = fixed_detail::lower_bound_index(
                items, Base::size(), key, [](const T& value) -> const T& { return value; });
            if (index < Base::size() && fixed_detail::ordered_equal(items[index], key)) {
                return items + index;
            }
            return Base::end();
        }

        template <typename KeyLike>
        const_iterator find(const KeyLike& key) const {
            auto* items = Base::data();
            auto index = fixed_detail::lower_bound_index(
                items, Base::size(), key, [](const T& value) -> const T& { return value; });
            if (index < Base::size() && fixed_detail::ordered_equal(items[index], key)) {
                return items + index;
            }
            return Base::end();
        }

        template <typename KeyLike>
        bool contains(const KeyLike& key) const {
            return find(key) != Base::end();
        }

        std::pair<iterator, bool> insert(const T& value) {
            auto* items = Base::data();
            auto index = fixed_detail::lower_bound_index(
                items, Base::size(), value, [](const T& item) -> const T& { return item; });
            if (index < Base::size() && fixed_detail::ordered_equal(items[index], value)) {
                return {items + index, false};
            }
            return {Base::template insert_construct<true>(index, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, Base::element_binding());
            }), true};
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        std::pair<iterator, bool> insert(Arg&& arg) {
            auto* items = Base::data();
            auto index = fixed_detail::lower_bound_index(
                items, Base::size(), arg, [](const T& item) -> const T& { return item; });
            if (index < Base::size() && fixed_detail::ordered_equal(items[index], arg)) {
                return {items + index, false};
            }
            return {Base::template insert_construct<true>(index, [&](T* dst) {
                fixed_detail::construct_bound(dst, Base::element_binding(), std::forward<Arg>(arg));
            }), true};
        }

        template <typename... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            if constexpr (sizeof...(Args) == 1) {
                return insert(std::forward<Args>(args)...);
            } else {
                const auto alloc = Base::bound_binding();
                auto* staged = detail::begin_dynamic_array<T>(
                fixed_detail::allocate_dynamic_bytes(alloc, sizeof(T), alignof(T)), 1u);
                bool constructed = false;
                try {
                    fixed_detail::construct_bound(staged, alloc, std::forward<Args>(args)...);
                    constructed = true;
                    auto result = insert(*staged);
                    std::destroy_at(staged);
                    fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                    return result;
                } catch (...) {
                    if (constructed) std::destroy_at(staged);
                    fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                    throw;
                }
            }
        }

        template <typename KeyLike>
        std::uint32_t erase(const KeyLike& key) {
            auto it = find(key);
            if (it == Base::end()) return 0;
            auto index = static_cast<std::uint32_t>(it - Base::begin());
            Base::erase_index(index);
            return 1;
        }
    };

    template <typename K, typename V>
    class XFixedFlatMap : public XFixedVector<XKeyValue<K, V>> {
        using value_base = XFixedVector<XKeyValue<K, V>>;

    public:
        using key_type = K;
        using mapped_type = V;
        using value_type = typename value_base::value_type;
        using allocator_type = typename value_base::allocator_type;
        using iterator = typename value_base::iterator;
        using const_iterator = typename value_base::const_iterator;
        using value_base::value_base;

        template <typename KeyLike>
        iterator find(const KeyLike& key) {
            auto* items = value_base::data();
            auto index = fixed_detail::lower_bound_index(
                items, value_base::size(), key,
                [](const value_type& item) -> const K& { return item.first; });
            if (index < value_base::size() && fixed_detail::ordered_equal(items[index].first, key)) {
                return items + index;
            }
            return value_base::end();
        }

        template <typename KeyLike>
        const_iterator find(const KeyLike& key) const {
            auto* items = value_base::data();
            auto index = fixed_detail::lower_bound_index(
                items, value_base::size(), key,
                [](const value_type& item) -> const K& { return item.first; });
            if (index < value_base::size() && fixed_detail::ordered_equal(items[index].first, key)) {
                return items + index;
            }
            return value_base::end();
        }

        template <typename KeyLike>
        bool contains(const KeyLike& key) const {
            return find(key) != value_base::end();
        }

        template <typename KeyArg, typename... Args>
        std::pair<iterator, bool> try_emplace(KeyArg&& key, Args&&... args) {
            auto* items = value_base::data();
            auto index = fixed_detail::lower_bound_index(
                items, value_base::size(), key,
                [](const value_type& item) -> const K& { return item.first; });
            if (index < value_base::size() && fixed_detail::ordered_equal(items[index].first, key)) {
                return {items + index, false};
            }

            return {value_base::template insert_construct<true>(index, [&](value_type* dst) {
                construct_entry(dst, std::forward<KeyArg>(key), std::forward<Args>(args)...);
            }), true};
        }

        template <typename KeyArg, typename ValueArg>
        std::pair<iterator, bool> emplace(KeyArg&& key, ValueArg&& value) {
            return try_emplace(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
        }

        template <typename KeyArg, typename ValueArg>
        std::pair<iterator, bool> insert_or_assign(KeyArg&& key, ValueArg&& value) {
            auto it = find(key);
            if (it != value_base::end()) {
                if constexpr (std::is_nothrow_assignable_v<V&, ValueArg&&>) {
                    it->second = std::forward<ValueArg>(value);
                    return {it, false};
                } else {
                    auto index = static_cast<std::uint32_t>(it - value_base::begin());
                    value_base::replace_construct(index,
                        [&](value_type* dst) {
                            construct_entry(dst, it->first, std::forward<ValueArg>(value));
                        });
                    return {value_base::data() + index, false};
                }
            }
            return try_emplace(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
        }

        template <typename KeyArg>
        V& operator[](KeyArg&& key) {
            auto [it, inserted] = try_emplace(std::forward<KeyArg>(key));
            (void)inserted;
            return it->second;
        }

        template <typename KeyLike>
        std::uint32_t erase(const KeyLike& key) {
            auto it = find(key);
            if (it == value_base::end()) return 0;
            auto index = static_cast<std::uint32_t>(it - value_base::begin());
            value_base::erase_index(index);
            return 1;
        }

    private:
        template <typename KeyArg, typename... Args>
        void construct_entry(value_type* dst, KeyArg&& key, Args&&... args) {
            auto alloc = value_base::binding();
            fixed_detail::construct_bound(&dst->first, alloc, std::forward<KeyArg>(key));
            try {
                fixed_detail::construct_bound(&dst->second, alloc, std::forward<Args>(args)...);
            } catch (...) {
                std::destroy_at(&dst->first);
                throw;
            }
            detail::zero_implicit_padding(dst);  // between key and value (M6)
        }
    };

    static_assert(sizeof(XFixedString) == 12 && alignof(XFixedString) == 4,
        "XFixedString ABI must stay frozen at 12 bytes, 4-aligned (wire V2.1)");
    static_assert(sizeof(XFixedVector<int>) == 12 && alignof(XFixedVector<int>) == 4,
        "XFixedVector ABI must stay frozen at 12 bytes, 4-aligned (wire V2.1)");
    static_assert(sizeof(XFixedFlatSet<int>) == sizeof(XFixedVector<int>),
        "XFixedFlatSet ABI must stay frozen at 12 bytes");
    static_assert(sizeof(XFixedFlatMap<int, int>) == sizeof(XFixedVector<XKeyValue<int, int>>),
        "XFixedFlatMap ABI must stay frozen at 12 bytes");

    class XString : public XFixedString {
        using Base = XFixedString;

    public:
        using allocator_type = typename Base::allocator_type;
        using Base::operator=;
        using Base::view;
        using Base::size;
        using Base::empty;
        using Base::clear;
        using Base::reserve;
        using Base::append;
        using Base::operator+=;
        using Base::data;
        using Base::c_str;

        XString() = default;

        template <typename Alloc>
        explicit XString(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XString(const XString& other)
            : Base() {
            Base::operator=(other);
        }

        XString(XString&& other)
            : XString(static_cast<const XString&>(other)) {}

        template <fixed_detail::string_view_like ViewLike, typename Alloc>
        XString(const ViewLike& value, const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {
            Base::assign(std::string_view(value));
        }

        template <typename Alloc>
        XString(const XString& other, const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {
            Base::operator=(other);
        }

        XString& operator=(const XString&) = default;
        XString& operator=(XString&&) = default;

    };

    template <typename T>
    class XVector : public XFixedVector<T> {
        using Base = XFixedVector<T>;

        std::uint32_t index_from(typename Base::const_iterator pos) const noexcept {
            auto* begin = Base::data();
            if (!begin || !pos) return 0;
            return static_cast<std::uint32_t>(pos - begin);
        }

        template <typename Arg>
        typename Base::iterator insert_value(typename Base::const_iterator pos, Arg&& arg) {
            auto index = index_from(pos);
            return Base::insert_construct(index, [&](T* dst) {
                fixed_detail::construct_bound(dst, Base::element_binding(), std::forward<Arg>(arg));
            });
        }

    public:
        using allocator_type = typename Base::allocator_type;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using Base::operator[];
        using Base::begin;
        using Base::end;
        using Base::front;
        using Base::back;
        using Base::data;
        using Base::size;
        using Base::capacity;
        using Base::empty;
        using Base::clear;
        using Base::reserve;
        using Base::assign_range;
        using Base::assign;
        using Base::pop_back;
        using Base::emplace_back;

        XVector() = default;

        template <typename Alloc>
        explicit XVector(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XVector(const XVector& other)
            : Base() {
            Base::operator=(other);
        }

        XVector(XVector&& other)
            : XVector(static_cast<const XVector&>(other)) {}

        template <typename Alloc>
        XVector(const XVector& other, const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {
            Base::operator=(other);
        }

        XVector& operator=(const XVector&) = default;
        XVector& operator=(XVector&&) = default;

        using Base::push_back;

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void push_back(Arg&& arg) {
            Base::emplace_back(std::forward<Arg>(arg));
        }

        iterator insert(const_iterator pos, const T& value) {
            return insert_value(pos, value);
        }

        iterator insert(const_iterator pos, T&& value) {
            return insert_value(pos, static_cast<const T&>(value));
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        iterator insert(const_iterator pos, Arg&& arg) {
            return insert_value(pos, std::forward<Arg>(arg));
        }

        iterator erase(const_iterator pos) {
            auto index = index_from(pos);
            if (index >= Base::size()) return Base::end();
            Base::erase_index(index);
            if (index >= Base::size()) return Base::end();
            return Base::data() + index;
        }

        void resize(std::size_t count) {
            auto n = Base::checked_count(count);
            if constexpr (std::is_scalar_v<T>) {
                Base::resize_default_scalar(n);
            } else {
                const auto alloc = Base::element_binding();
                Base::resize_construct(n, [&](T* dst) {
                    fixed_detail::default_construct(dst, alloc);
                });
            }
        }

        void resize(std::size_t count, const T& value) {
            auto n = Base::checked_count(count);
            const auto alloc = Base::element_binding();
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, alloc);
            });
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void resize(std::size_t count, Arg&& arg) {
            auto n = Base::checked_count(count);
            const auto alloc = Base::element_binding();
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::construct_bound(dst, alloc, arg);
            });
        }

        void assign(std::size_t count, const T& value) {
            auto n = Base::checked_count(count);
            if (n == 0) { Base::clear(); return; }
            const auto alloc = Base::element_binding();
            Base::rebuild(n, std::max(n, Base::capacity()), [&](T* dst, size_type) {
                fixed_detail::transfer_construct(dst, value, alloc);
            });
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void assign(std::size_t count, Arg&& arg) {
            auto n = Base::checked_count(count);
            if (n == 0) { Base::clear(); return; }
            const auto alloc = Base::element_binding();
            Base::rebuild(n, std::max(n, Base::capacity()), [&](T* dst, size_type) {
                fixed_detail::construct_bound(dst, alloc, arg);
            });
        }
    };

    static_assert(sizeof(XString) == sizeof(XFixedString),
        "Main XString wrapper must stay zero-overhead over XFixedString");
    static_assert(sizeof(XVector<int>) == sizeof(XFixedVector<int>),
        "Main XVector wrapper must stay zero-overhead over XFixedVector");

    template <typename T>
    class XSet : public XFixedFlatSet<T> {
        using Base = XFixedFlatSet<T>;

    public:
        using allocator_type = typename Base::allocator_type;
        using key_type = T;
        using value_type = typename Base::value_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using Base::begin;
        using Base::end;
        using Base::size;
        using Base::empty;
        using Base::clear;
        using Base::find;
        using Base::contains;
        using Base::erase;
        using Base::emplace;
        using Base::insert;

        XSet() = default;

        template <typename Alloc>
        explicit XSet(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XSet(const XSet& other)
            : Base() {
            Base::operator=(other);
        }

        XSet(XSet&& other)
            : XSet(static_cast<const XSet&>(other)) {}

        template <typename Alloc>
        XSet(const XSet& other, const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {
            Base::operator=(other);
        }

        XSet& operator=(const XSet&) = default;
        XSet& operator=(XSet&&) = default;

        iterator insert(const_iterator, const T& value) {
            return Base::insert(value).first;
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        iterator insert(const_iterator, Arg&& arg) {
            return Base::insert(std::forward<Arg>(arg)).first;
        }
    };

    template <typename K, typename V>
    class XMap : public XFixedFlatMap<K, V> {
        using Base = XFixedFlatMap<K, V>;

    public:
        using allocator_type = typename Base::allocator_type;
        using key_type = typename Base::key_type;
        using mapped_type = typename Base::mapped_type;
        using value_type = typename Base::value_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using Base::begin;
        using Base::end;
        using Base::size;
        using Base::empty;
        using Base::clear;
        using Base::find;
        using Base::contains;
        using Base::erase;
        using Base::operator[];
        using Base::try_emplace;
        using Base::emplace;
        using Base::insert_or_assign;

        XMap() = default;

        template <typename Alloc>
        explicit XMap(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XMap(const XMap& other)
            : Base() {
            Base::operator=(other);
        }

        XMap(XMap&& other)
            : XMap(static_cast<const XMap&>(other)) {}

        template <typename Alloc>
        XMap(const XMap& other, const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {
            Base::operator=(other);
        }

        XMap& operator=(const XMap&) = default;
        XMap& operator=(XMap&&) = default;

    };

    static_assert(sizeof(XSet<int>) == sizeof(XFixedFlatSet<int>),
        "Main XSet wrapper must stay zero-overhead over XFixedFlatSet");
    static_assert(sizeof(XMap<int, int>) == sizeof(XFixedFlatMap<int, int>),
        "Main XMap wrapper must stay zero-overhead over XFixedFlatMap");

    using XFixedBlob = XFixedVector<std::byte>;
    using XBlob = XVector<std::byte>;
    template <typename T> using XFlatSet = XSet<T>;
    template <typename K, typename V> using XFlatMap = XMap<K, V>;
} // namespace XOffsetDatastructure

// Field lists of the library's own types, for the field-list backend (a C++26
// build compares them with reflection in test_schema_describe). The containers
// are not aggregates, so they carry no completeness proof; XKeyValue and
// std::array keep the XOFFSET_RECORD promise, as reflection finds.
namespace XOffsetDatastructure::schema {
    namespace describe_detail {
        template <class H>
        struct head_fields {
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(H, data_delta)
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(H, size_)
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(H, capacity_)
        };
        template <class H, class F = head_fields<H>>
        using head_record = record<H, false, bases<>,
                                   typename F::field_data_delta, typename F::field_size_,
                                   typename F::field_capacity_>;

        template <class K, class V>
        struct key_value_fields {
            using C = XKeyValue<K, V>;
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(C, first)
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(C, second)
        };
    } // namespace describe_detail

    template <> struct description<XFixedString> : describe_detail::head_record<XFixedString> {};
    template <> struct description<XString>
        : describe_detail::record<XString, false, describe_detail::bases<XFixedString>> {};
    template <class T> struct description<XFixedVector<T>> : describe_detail::head_record<XFixedVector<T>> {};
    template <class T> struct description<XVector<T>>
        : describe_detail::record<XVector<T>, false, describe_detail::bases<XFixedVector<T>>> {};
    template <class T> struct description<XFixedFlatSet<T>>
        : describe_detail::record<XFixedFlatSet<T>, false, describe_detail::bases<XFixedVector<T>>> {};
    template <class T> struct description<XSet<T>>
        : describe_detail::record<XSet<T>, false, describe_detail::bases<XFixedFlatSet<T>>> {};
    template <class K, class V> struct description<XFixedFlatMap<K, V>>
        : describe_detail::record<XFixedFlatMap<K, V>, false,
                                  describe_detail::bases<XFixedVector<XKeyValue<K, V>>>> {};
    template <class K, class V> struct description<XMap<K, V>>
        : describe_detail::record<XMap<K, V>, false, describe_detail::bases<XFixedFlatMap<K, V>>> {};
    template <class K, class V> struct description<XKeyValue<K, V>>
        : describe_detail::record<XKeyValue<K, V>, true, describe_detail::bases<>,
              typename describe_detail::key_value_fields<K, V>::field_first,
              typename describe_detail::key_value_fields<K, V>::field_second> {};

    // std::array<E, N>: its one member, under the standard library's name.
#if defined(_LIBCPP_VERSION)
#define XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS __elems_
#elif defined(__GLIBCXX__)
#define XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS _M_elems
#elif defined(_MSC_VER)
#define XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS _Elems
#endif
#ifdef XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS
    namespace describe_detail {
        template <class E, std::size_t N>
        struct array_fields {
            using C = std::array<E, N>;
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(C, XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS)
        };
    } // namespace describe_detail
    template <class E, std::size_t N> struct description<std::array<E, N>>
        : describe_detail::record<std::array<E, N>, true, describe_detail::bases<>,
              typename describe_detail::array_fields<E, N>::field_XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS> {};
#undef XOFFSET_SCHEMA_DETAIL_ARRAY_ELEMS
#endif
} // namespace XOffsetDatastructure::schema

namespace XOffsetDatastructure {

    namespace detail {
        template <typename T>
        inline constexpr bool is_fixed_string_v =
            std::is_base_of_v<XFixedString, std::remove_cv_t<T>>;

        template <typename T, typename = void>
        struct is_fixed_vector : std::false_type {};
        template <typename T>
        struct is_fixed_vector<T, std::void_t<typename std::remove_cv_t<T>::value_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XFixedVector<typename std::remove_cv_t<T>::value_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_vector_v = is_fixed_vector<T>::value;

        template <typename T, typename = void>
        struct is_fixed_flat_set : std::false_type {};
        template <typename T>
        struct is_fixed_flat_set<T, std::void_t<typename std::remove_cv_t<T>::value_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XFixedFlatSet<typename std::remove_cv_t<T>::value_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_flat_set_v = is_fixed_flat_set<T>::value;

        template <typename T, typename = void>
        struct is_fixed_flat_map : std::false_type {};
        template <typename T>
        struct is_fixed_flat_map<T, std::void_t<typename std::remove_cv_t<T>::key_type,
                                                typename std::remove_cv_t<T>::mapped_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XFixedFlatMap<typename std::remove_cv_t<T>::key_type,
                                  typename std::remove_cv_t<T>::mapped_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_flat_map_v = is_fixed_flat_map<T>::value;

        template <typename T>
        struct is_v1_wire_admitted_impl;

        template <typename T, std::size_t N>
        consteval bool reflected_member_wire_admitted() {
            using member = schema::member_t<T, N>;
            using ReflectedMemberType = typename member::type;
            using MemberType = std::remove_cv_t<ReflectedMemberType>;
            return is_v1_wire_admitted_impl<MemberType>::value;
        }

        template <typename T, std::size_t N = 0>
        consteval bool reflected_members_wire_admitted_impl() {
            if constexpr (N >= schema::member_count_v<T>) {
                return true;
            } else {
                return reflected_member_wire_admitted<T, N>() &&
                    reflected_members_wire_admitted_impl<T, N + 1>();
            }
        }

        template <typename T>
        consteval bool reflected_members_wire_admitted() {
            return reflected_members_wire_admitted_impl<T>();
        }

        // A function template rather than a lambda in the static member's
        // initializer: MSVC instantiates the discarded if-constexpr branches of
        // a non-generic lambda (C2825 on CleanT::key_type for CleanT = char).
        template <typename CleanT>
        consteval bool compute_v1_wire_admitted() {
            if constexpr (!is_byte_copy_safe_v<CleanT>) {
                return false;
            } else if constexpr (std::is_reference_v<CleanT> ||
                                 std::is_pointer_v<CleanT> ||
                                 std::is_member_pointer_v<CleanT> ||
                                 std::is_union_v<CleanT>) {
                return false;
            } else if constexpr (std::is_array_v<CleanT>) {
                return is_v1_wire_admitted_impl<std::remove_extent_t<CleanT>>::value;
            } else if constexpr (fixed_size_scalar<CleanT>) {
                return true;
            } else if constexpr (std::is_enum_v<CleanT>) {
                return fixed_size_scalar<std::underlying_type_t<CleanT>>;
            } else if constexpr (is_fixed_string_v<CleanT>) {
                return std::is_same_v<CleanT, XFixedString> || std::is_same_v<CleanT, XString>;
            } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                return (std::is_same_v<CleanT, XFixedFlatMap<typename CleanT::key_type, typename CleanT::mapped_type>> ||
                        std::is_same_v<CleanT, XMap<typename CleanT::key_type, typename CleanT::mapped_type>>) &&
                    is_v1_wire_admitted_impl<typename CleanT::key_type>::value &&
                    is_v1_wire_admitted_impl<typename CleanT::mapped_type>::value;
            } else if constexpr (is_fixed_flat_set_v<CleanT>) {
                return (std::is_same_v<CleanT, XFixedFlatSet<typename CleanT::value_type>> ||
                        std::is_same_v<CleanT, XSet<typename CleanT::value_type>>) &&
                    is_v1_wire_admitted_impl<typename CleanT::value_type>::value;
            } else if constexpr (is_fixed_vector_v<CleanT>) {
                return (std::is_same_v<CleanT, XFixedVector<typename CleanT::value_type>> ||
                        std::is_same_v<CleanT, XVector<typename CleanT::value_type>>) &&
                    is_v1_wire_admitted_impl<typename CleanT::value_type>::value;
            } else if constexpr (std::is_class_v<CleanT>) {
                return alignof(CleanT) <= 8 &&
                    std::is_standard_layout_v<CleanT> &&
                    !std::is_polymorphic_v<CleanT> &&
                    schema::base_count_v<CleanT> == 0 &&
                    reflected_members_wire_admitted<CleanT>();
            } else {
                return false;
            }
        }

        template <typename T>
        struct is_v1_wire_admitted_impl {
            static constexpr bool value = compute_v1_wire_admitted<std::remove_cv_t<T>>();
        };

        template <typename T>
        consteval bool v1_wire_admitted() {
            return is_v1_wire_admitted_impl<std::remove_cvref_t<T>>::value;
        }

        // Validation errors leave the hot path through one out-of-line call.
        [[noreturn]] XOFFSET_DETAIL_MOVE_COLD inline void wire_reject(const char* what) {
            throw XException(what);
        }

        // Retained validation words of one long-lived reload buffer: the
        // bitmaps of payloads above WireGranuleIndex::inline_extent reuse
        // them message after message instead of a fresh host block. They grow
        // to the largest payload validated and are released with the buffer
        // that owns them (or by XBuffer::release_reload_scratch()). The
        // validator clears every word it reads, exactly as on fresh memory,
        // and the words never reach the payload or the wire.
        class WireValidationScratch {
        public:
            WireValidationScratch() = default;
            WireValidationScratch(const WireValidationScratch&) = delete;
            WireValidationScratch& operator=(const WireValidationScratch&) = delete;
            ~WireValidationScratch() { release(); }
            std::size_t capacity() const noexcept { return capacity_; }
            std::uint64_t* words(std::size_t count) {
                if (count > capacity_) {
                    auto* fresh = new std::uint64_t[count];
                    delete[] words_; // the tables may be in use (direct reload)
                    words_ = fresh;
                    capacity_ = count;
                }
                return words_;
            }
            // Header, extent table and hole table of a direct reload.
            std::byte* tables(std::size_t bytes) {
                if (bytes > tables_.size()) tables_.resize(bytes);
                return tables_.data();
            }
            void release(bool tables = true) noexcept {
                delete[] words_;
                words_ = nullptr;
                capacity_ = 0;
                if (tables) std::vector<std::byte>().swap(tables_);
            }
        private:
            std::uint64_t* words_ = nullptr;
            std::size_t capacity_ = 0;
            std::vector<std::byte> tables_;
        };

        // V2 validation index: one bit per 8-byte granule of the payload, one
        // bitmap for live blocks and one for holes. Every range the validator
        // registers is granule aligned: a block is 8-byte aligned with its
        // length rounded up to 8 (register_allocation), and the arena header
        // and every hole are multiples of 8 at multiples of 8. A granule bit
        // therefore records exactly the byte membership that a byte index
        // would, and every overlap, coverage and gap answer is the same.
        //
        // Payloads up to inline_extent use words inside the index (no host
        // memory). Larger ones use a reload buffer's retained words or one
        // block from the upstream resource, returned when the index dies.
        // The size follows the payload extent, which the caller has already
        // bounded by its maximum capacity, never a wire count. Both bitmaps
        // are cleared once, on the first registration.
        class WireGranuleIndex {
        public:
            static constexpr std::size_t inline_words = 64;              // per bitmap
            static constexpr std::size_t inline_extent = inline_words * 64 * 8; // 32 KiB
            WireGranuleIndex() noexcept {} // inline words stay uninitialized until prepare()
            WireGranuleIndex(const WireGranuleIndex&) = delete;
            WireGranuleIndex& operator=(const WireGranuleIndex&) = delete;
            WireGranuleIndex(WireGranuleIndex&&) = delete;
            WireGranuleIndex& operator=(WireGranuleIndex&&) = delete;
            ~WireGranuleIndex() {
                if (block_) block_upstream_->deallocate(block_, block_words_ * sizeof(std::uint64_t),
                                                        alignof(std::uint64_t));
            }

            bool prepared() const noexcept { return owned_ != nullptr; }
            // Granules of the payload; a trailing partial granule (extent not
            // a multiple of 8) is never registered and never free.
            std::size_t granules() const noexcept { return granules_; }
            // Host words this index took from outside itself (0: inline or retained).
            std::size_t upstream_words() const noexcept { return block_words_; }

            void prepare(std::size_t extent, std::pmr::memory_resource* upstream,
                         WireValidationScratch* retained) {
                const auto granules = extent / 8u;
                const auto words = (granules + 63u) / 64u;
                std::uint64_t* storage;
                if (words <= inline_words) {
                    storage = inline_;
                } else if (retained) {
                    storage = retained->words(2 * words);
                } else {
                    if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
                    storage = static_cast<std::uint64_t*>(
                        upstream->allocate(2 * words * sizeof(std::uint64_t), alignof(std::uint64_t)));
                    block_ = storage;
                    block_words_ = 2 * words;
                    block_upstream_ = upstream;
                }
                if (words) std::memset(storage, 0, 2 * words * sizeof(std::uint64_t));
                owned_ = storage;
                free_ = storage + words;
                granules_ = granules;
            }

            // [g0, g1) nonempty, g1 <= granules(). 0 none, 1 live, 2 free. A
            // live hit anywhere in the range wins over a free hit.
            unsigned overlap(std::size_t g0, std::size_t g1) const noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                const auto first = ~std::uint64_t(0) << (g0 % 64u);
                const auto last = ~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u);
                if (w0 == w1) {
                    const auto m = first & last;
                    if (owned_[w0] & m) return 1;
                    return (free_[w0] & m) ? 2 : 0;
                }
                if (owned_[w0] & first) return 1;
                std::uint64_t dead = free_[w0] & first;
                for (auto w = w0 + 1u; w < w1; ++w) {
                    if (owned_[w]) return 1;
                    dead |= free_[w];
                }
                if (owned_[w1] & last) return 1;
                dead |= free_[w1] & last;
                return dead ? 2 : 0;
            }

            void mark(bool free, std::size_t g0, std::size_t g1) noexcept {
                auto* bits = free ? free_ : owned_;
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                const auto first = ~std::uint64_t(0) << (g0 % 64u);
                const auto last = ~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u);
                if (w0 == w1) {
                    bits[w0] |= first & last;
                    return;
                }
                bits[w0] |= first;
                for (auto w = w0 + 1u; w < w1; ++w) bits[w] = ~std::uint64_t(0);
                bits[w1] |= last;
            }

            bool member(bool free, std::size_t granule) const noexcept {
                // The pointer is chosen first: GCC 13 at -O0 with UBSan
                // miscompiles the conditional used directly as the array.
                const std::uint64_t* bits = free ? free_ : owned_;
                return (bits[granule / 64u] >> (granule % 64u)) & 1u;
            }

        private:
            std::uint64_t* owned_ = nullptr;
            std::uint64_t* free_ = nullptr;
            std::size_t granules_ = 0;
            std::uint64_t* block_ = nullptr;
            std::size_t block_words_ = 0;
            std::pmr::memory_resource* block_upstream_ = nullptr;
            std::uint64_t inline_[2 * inline_words];
        };

        // Test switch for the cursor path below (differential on/off runs).
        inline constinit bool wire_validation_cursor = true;
        // Test switch for XBuffer's small dense reload path (differential on/off runs).
        inline constinit bool small_reload_fast_path = true;

#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
        // Diagnostic build only: layout classes, per root type, of the wires
        // that pass verified validation, printed to stderr at exit.
        // canonical: the cursor path validated the whole graph. ascending:
        // live blocks in visiting order at rising offsets, with holes between
        // them. out_of_order: some block starts below the end of the previous
        // one (first_offset_sum adds the payload offset of the first such
        // block, out_of_order_blocks counts them all).
        struct WireLayoutStats {
            const char* type = nullptr;
            WireLayoutStats* next = nullptr;
            std::uint64_t canonical = 0, ascending = 0, out_of_order = 0;
            std::uint64_t out_of_order_blocks = 0, first_offset_sum = 0;
        };
        inline WireLayoutStats* wire_layout_stats_list = nullptr;
        struct WireLayoutStatsReport {
            ~WireLayoutStatsReport() {
                for (auto* s = wire_layout_stats_list; s; s = s->next)
                    std::fprintf(stderr,
                        "xoffset layout stats %s: canonical %llu ascending %llu out_of_order %llu"
                        " out_of_order_blocks %llu first_offset_sum %llu\n", s->type,
                        (unsigned long long)s->canonical, (unsigned long long)s->ascending,
                        (unsigned long long)s->out_of_order, (unsigned long long)s->out_of_order_blocks,
                        (unsigned long long)s->first_offset_sum);
            }
        };
        inline WireLayoutStatsReport wire_layout_stats_report;
        template <typename T> inline WireLayoutStats wire_layout_stats_of{};
        template <typename T> WireLayoutStats& wire_layout_stats() {
            auto& s = wire_layout_stats_of<T>;
            if (!s.type) {
                s.type = typeid(T).name();
                s.next = wire_layout_stats_list;
                wire_layout_stats_list = &s;
            }
            return s;
        }
#endif

        struct WireValidationContext {
            static constexpr std::size_t no_cursor = ~std::size_t(0);
            const char* base{};
            const char* end{};
            XArena* segment{};
            // V2 exact coverage: bytes registered as live and as holes. Ranges
            // never overlap (rejected on insert), so equality with used_end
            // proves the payload is tiled exactly.
            std::uint64_t owned_bytes = 0;
            std::uint64_t free_bytes = 0;
            // Host-only scratch for bitmaps above the inline size: a reload
            // buffer's retained words when set, else one upstream block.
            std::pmr::memory_resource* upstream = std::pmr::new_delete_resource();
            WireValidationScratch* retained = nullptr;
            // Cursor path: while no hole is registered and every live block
            // starts where the previous one ended (canonical preorder), the
            // live set is exactly [0, cursor) and no bitmap is touched. At the
            // first block elsewhere, or the first hole, the live bitmap is set
            // to [0, cursor) (the state the bitmaps would have reached) and
            // validation continues on the bitmaps, so accept/reject and the
            // first error are unchanged. no_cursor: bitmaps only.
            std::size_t cursor = wire_validation_cursor ? 0 : no_cursor;
            WireGranuleIndex granules;
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            std::size_t stats_end = 0, stats_first = no_cursor;
            std::uint64_t stats_out_of_order = 0;
#endif

            std::size_t extent() const noexcept {
                return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(end) -
                                                reinterpret_cast<std::uintptr_t>(base));
            }
            WireGranuleIndex& index() {
                if (!granules.prepared()) granules.prepare(extent(), upstream, retained);
                return granules;
            }
            // The bitmaps with the cursor's live range folded in.
            WireGranuleIndex& general() {
                auto& bits = index();
                if (cursor != no_cursor) {
                    if (cursor) bits.mark(false, 0, cursor / 8u);
                    cursor = no_cursor;
                }
                return bits;
            }
        };

#if defined(_MSC_VER)
#define XOFFSET_DETAIL_V4_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define XOFFSET_DETAIL_V4_INLINE inline __attribute__((always_inline))
#else
#define XOFFSET_DETAIL_V4_INLINE inline
#endif
        XOFFSET_DETAIL_V4_INLINE void validate_storage_range(const void* ptr, std::size_t bytes,
                                           std::size_t alignment,
                                           const WireValidationContext& ctx,
                                           const char* what) {
            if (bytes == 0) return;
            if (!ptr) {
                wire_reject(what);
            }
            auto begin = reinterpret_cast<std::uintptr_t>(ptr);
            auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            auto end = reinterpret_cast<std::uintptr_t>(ctx.end);
            if (begin < base || begin > end || bytes > end - begin) {
                wire_reject(what);
            }
            if ((reinterpret_cast<std::uintptr_t>(ptr) % alignment) != 0) {
                wire_reject(what);
            }
        }

        void register_owned_bitmap(std::size_t begin, std::size_t bytes, WireValidationContext& ctx);

        // Registers granules [begin, begin + bytes) of a range that is already
        // known to lie in the payload on 8-byte granules.
        XOFFSET_DETAIL_V4_INLINE void register_owned_granules(std::size_t begin, std::size_t bytes,
                                                              WireValidationContext& ctx) {
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            if (begin < ctx.stats_end) {
                if (ctx.stats_first == WireValidationContext::no_cursor) ctx.stats_first = begin;
                ++ctx.stats_out_of_order;
            }
            ctx.stats_end = begin + bytes;
#endif
            if (begin == ctx.cursor) [[likely]] {
                ctx.cursor += bytes;
                ctx.owned_bytes += bytes;
                return;
            }
            register_owned_bitmap(begin, bytes, ctx);
        }

        // Bitmap registration, kept out of line so that the validators the
        // cursor path inlines into stay small.
        [[gnu::noinline]] inline void register_owned_bitmap(std::size_t begin, std::size_t bytes,
                                                            WireValidationContext& ctx) {
            auto& index = ctx.general();
            const auto g0 = begin / 8u, g1 = (begin + bytes) / 8u;
            const auto overlap = index.overlap(g0, g1);
            if (overlap == 1) {
                wire_reject(
                    "wire validation detected overlapping dynamic ranges");
            }
            if (overlap == 2) {
                wire_reject("wire validation detected live/free overlap");
            }
            index.mark(false, g0, g1);
            ctx.owned_bytes += bytes;
            /* XBV3: owned-registered */
        }

        // Offset of a range that passed validate_storage_range; a range that
        // is not on 8-byte granules is rejected with `what` (the validator
        // never registers one: see WireGranuleIndex).
        inline std::size_t granule_range_offset(const void* ptr, std::size_t bytes,
                                                const WireValidationContext& ctx,
                                                const char* what) {
            const auto begin = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ptr) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            if (((begin | bytes) & 7u) != 0) wire_reject(what);
            return begin;
        }

        inline void register_owned_range(const void* ptr, std::size_t bytes,
                                         WireValidationContext& ctx,
                                         const char* what) {
            if (bytes == 0 || !ptr) return;
            validate_storage_range(ptr, bytes, 1, ctx, what);
            register_owned_granules(granule_range_offset(ptr, bytes, ctx, what), bytes, ctx);
        }

        inline void register_free_range(const void* ptr, std::size_t bytes,
                                        WireValidationContext& ctx,
                                        const char* what) {
            if (bytes == 0 || !ptr) return;
            validate_storage_range(ptr, bytes, 1, ctx, what);
            const auto begin = granule_range_offset(ptr, bytes, ctx, what);
            auto& index = ctx.general();
            const auto g0 = begin / 8u, g1 = (begin + bytes) / 8u;
            const auto overlap = index.overlap(g0, g1);
            if (overlap == 1) {
                wire_reject(
                    "wire validation detected free-list overlap with live ranges");
            }
            if (overlap == 2) {
                wire_reject(
                    "wire validation detected overlapping free-list ranges");
            }
            index.mark(true, g0, g1);
            ctx.free_bytes += bytes;
            /* XBV3: free-registered */
        }

        XOFFSET_DETAIL_V4_INLINE const char* checked_relative(const std::int32_t& delta,
                                            const WireValidationContext& ctx) {
            auto address = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta;
            auto* ptr = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
            validate_storage_range(ptr, 1, 1, ctx, "wire validation: relative pointer out of bounds");
            return ptr; // W9: flat (validated range), no canonical lookup
        }

        // One check for a head's out-of-line block: data_delta resolves to an
        // 8-byte aligned address with [data, data + block) inside the payload
        // (block >= 1 is the head's 8-rounded extent). When it holds, every
        // check of the full sequence (checked_relative, the payload range,
        // register_allocation's block range) passes, so the block is
        // registered directly; `full` returns nullptr and the caller runs that
        // sequence, which throws its own first error.
        XOFFSET_DETAIL_V4_INLINE const char* checked_block(const std::int32_t& delta, std::size_t block,
                                                           WireValidationContext& ctx) {
            const auto address = static_cast<std::uintptr_t>(
                static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta);
            const auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            const auto end = reinterpret_cast<std::uintptr_t>(ctx.end);
            if (address < base || address > end || block > end - address || (address & 7u) != 0 ||
                ((address - base) & 7u) != 0) [[unlikely]]
                return nullptr;
            register_owned_granules(static_cast<std::size_t>(address - base), block, ctx);
            return reinterpret_cast<const char*>(address);
        }

#undef XOFFSET_DETAIL_V4_INLINE

        // V2: a block has no header. Its extent is derived from its owner:
        // 8-byte aligned, length rounded up to a multiple of 8.
        inline void register_allocation(const void* ptr, std::size_t bytes,
                                         std::size_t alignment, WireValidationContext& ctx,
                                         bool free = false) {
            const auto block = (bytes + 7u) & ~std::size_t(7u);
            validate_storage_range(ptr, block, std::max<std::size_t>(alignment, 8u), ctx,
                                   "wire validation: allocation out of bounds");
            if (free) register_free_range(ptr, block, ctx, "wire validation: invalid free block");
            else register_owned_range(ptr, block, ctx, "wire validation: invalid live block");
        }

        // V2 wire hole table: each entry is a free block of the payload.
        inline void register_wire_holes(const void* holes, std::size_t count,
                                        WireValidationContext& ctx) {
            const auto used = static_cast<std::uint64_t>(ctx.end - ctx.base);
            for (std::size_t i = 0; i < count; ++i) {
                XBufferCore::WireHole hole;
                std::memcpy(&hole, static_cast<const char*>(holes) + i * sizeof(hole), sizeof(hole));
                if (hole.bytes == 0 || (hole.bytes & 7u) != 0 || (hole.offset & 7u) != 0 ||
                    hole.offset < sizeof(XArena) || hole.offset > used ||
                    hole.bytes > used - hole.offset) {
                    wire_reject("wire validation failed: invalid hole table entry");
                }
                register_free_range(ctx.base + hole.offset, hole.bytes, ctx,
                                    "wire validation failed: invalid hole table entry");
            }
        }

        inline void require_exact_coverage(const WireValidationContext& ctx) {
            if (ctx.owned_bytes + ctx.free_bytes != static_cast<std::uint64_t>(ctx.end - ctx.base)) {
                wire_reject(
                    "wire validation failed: payload is not exactly covered by live blocks and holes");
            }
        }

        // Calls fn(begin, end) for each maximal run of payload bytes in no
        // live block, in ascending order (the bytes of a trailing partial
        // granule are in no block).
        template <typename Fn>
        void for_each_unowned_run(WireValidationContext& ctx, Fn&& fn) {
            const auto used = ctx.extent();
            const auto& index = ctx.general();
            std::size_t g = 0;
            const auto count = index.granules();
            while (g < count) {
                if (index.member(false, g)) { ++g; continue; }
                const auto start = g;
                while (g < count && !index.member(false, g)) ++g;
                if (g == count) { fn(start * 8u, used); return; }
                fn(start * 8u, g * 8u);
            }
            if (count * 8u < used) fn(count * 8u, used);
        }

        template <typename T>
        void validate_wire_value(const T& value, WireValidationContext& ctx);
        // True means that visiting a value can reject wire bytes or validate a
        // binding/range. False is permitted only for an admitted representation
        // whose old recursive validator has no observable checks at all.
        template <typename T> struct wire_value_needs_validation;

        template <typename T, std::size_t N = 0>
        consteval bool reflected_member_needs_validation() {
            if constexpr (N >= schema::member_count_v<T>) {
                return false;
            } else {
                using member = schema::member_t<T, N>;
                using Member = typename member::type;
                return wire_value_needs_validation<Member>::value ||
                    reflected_member_needs_validation<T, N + 1>();
            }
        }
        // See compute_v1_wire_admitted.
        template <typename CleanT>
        consteval bool compute_wire_value_needs_validation() {
            if constexpr (!v1_wire_admitted<CleanT>()) {
                return true; // conservative for non-admitted/detail callers
            } else if constexpr (std::is_same_v<CleanT, bool>) {
                return true; // representation bytes must still be checked
            } else if constexpr (std::is_array_v<CleanT>) {
                return wire_value_needs_validation<std::remove_extent_t<CleanT>>::value;
            } else if constexpr (fixed_size_scalar<CleanT> || std::is_enum_v<CleanT>) {
                return false; // enum values were not range-checked by V1
            } else if constexpr (is_fixed_string_v<CleanT> || is_fixed_vector_v<CleanT> ||
                                 is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT>) {
                return true; // also covers sorted-key checks and empty bindings
            } else if constexpr (std::is_class_v<CleanT>) {
                // Admitted classes have no bases. Non-admitted classes took
                // the conservative branch above, retaining their old walk.
                return reflected_member_needs_validation<CleanT>();
            } else {
                return true;
            }
        }
        template <typename T>
        struct wire_value_needs_validation {
            static constexpr bool value = compute_wire_value_needs_validation<std::remove_cv_t<T>>();
        };
        template <typename T>
        inline constexpr bool wire_value_needs_validation_v = wire_value_needs_validation<T>::value;



        // Padding bytes [n, 11) of an inline string of length n, as masks of
        // bytes 0..7 and 8..10 (little endian). A table keeps the check free
        // of length-dependent branches.
        struct InlinePadMasks {
            std::uint64_t low[XFixedString::inline_max + 1];
            std::uint32_t high[XFixedString::inline_max + 1];
        };
        inline constexpr InlinePadMasks inline_pad_masks = [] {
            InlinePadMasks masks{};
            for (unsigned n = 0; n <= XFixedString::inline_max; ++n)
                for (unsigned i = n; i < 11u; ++i) {
                    if (i < 8u) masks.low[n] |= std::uint64_t(0xff) << (8u * i);
                    else masks.high[n] |= std::uint32_t(0xff) << (8u * (i - 8u));
                }
            return masks;
        }();

        // I1: head bytes 0..11 of an inline string (byte 11 = 0x80 | length).
        inline void validate_inline_string_bytes(const char* head) {
            std::uint64_t low;   // head bytes 0..7
            std::uint32_t high;  // head bytes 8..11
            std::memcpy(&low, head, sizeof(low));
            std::memcpy(&high, head + 8, sizeof(high));
            const unsigned length = (high >> 24) & 0x7fu;
            if (length > XFixedString::inline_max)
                wire_reject("wire validation failed: inline XFixedString length exceeds 10");
            // String bytes [length, 11) must be zero.
            if (((low & inline_pad_masks.low[length]) | (high & inline_pad_masks.high[length])) != 0)
                wire_reject("wire validation failed: inline XFixedString has nonzero padding");
        }

        inline void validate_wire_value(const XFixedString& value,
                                        WireValidationContext& ctx) {
            if (value.is_inline()) {
                validate_inline_string_bytes(reinterpret_cast<const char*>(&value));
                return;
            }
            if (value.size_ > value.capacity_ || value.capacity_ >= XBufferCore::MAX_PAYLOAD) {
                wire_reject(
                    "wire validation failed: XFixedString size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    wire_reject(
                        "wire validation failed: empty XFixedString has invalid payload");
                }
                return;
            }

            const auto block = fixed_detail::round_up_to_alignment(value.capacity_ + 1, 8);
            auto* data = checked_block(value.data_delta, block, ctx);
            if (!data) [[unlikely]] {
                data = checked_relative(value.data_delta, ctx);
                validate_storage_range(data, static_cast<std::size_t>(value.capacity_) + 1,
                                       alignof(char), ctx,
                                       "wire validation failed: XFixedString payload out of bounds");
                register_allocation(data, block, 8, ctx);
            }
            if (data[value.size_] != '\0') {
                wire_reject(
                    "wire validation failed: XFixedString missing terminator");
            }
        }

        template <typename T>
        void validate_wire_value(const XFixedVector<T>& value,
                                 WireValidationContext& ctx) {
            if (value.size_ > value.capacity_ ||
                value.capacity_ > (XBufferCore::MAX_PAYLOAD - 8u) / sizeof(T)) {
                wire_reject(
                    "wire validation failed: XFixedVector size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    wire_reject(
                        "wire validation failed: empty XFixedVector has invalid payload");
                }
                return;
            }

            const auto bytes = static_cast<std::size_t>(value.capacity_) * sizeof(T);
            const auto block = (bytes + 7u) & ~std::size_t(7u);
            auto* data = alignof(T) <= 8
                ? reinterpret_cast<const T*>(checked_block(value.data_delta, block, ctx)) : nullptr;
            if (!data) [[unlikely]] {
                data = reinterpret_cast<const T*>(checked_relative(value.data_delta, ctx));
                validate_storage_range(data, bytes, alignof(T), ctx,
                                       "wire validation failed: XFixedVector payload out of bounds");
                register_allocation(data, block, std::max<std::size_t>(alignof(T), 8), ctx);
            }
            if constexpr (wire_value_needs_validation_v<T>) {
                for (std::uint32_t i = 0; i < value.size_; ++i) {
                    /* XV4: checked-element-visited */
                    validate_wire_value(data[i], ctx);
                }
            } else {
                /* XV4: no-op-elements-elided */
            }
        }

        template <typename T, std::size_t N>
        void validate_base_at(const T& value, WireValidationContext& ctx) {
            using BaseType = schema::base_t<T, N>;
            validate_wire_value(static_cast<const BaseType&>(value), ctx);
        }

        template <typename T, std::size_t N>
        void validate_member_at(const T& value, WireValidationContext& ctx) {
            using member = schema::member_t<T, N>;
            using Member = typename member::type;
            if constexpr (wire_value_needs_validation_v<Member>)
                validate_wire_value(member::get(value), ctx);
        }

        template <typename T>
        void validate_wire_value(const T& value, WireValidationContext& ctx) {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<CleanT, bool>) {
                unsigned char bits;
                std::memcpy(&bits, &value, 1);
                if (bits > 1) wire_reject("wire validation: invalid bool representation");
            } else if constexpr (std::is_array_v<CleanT>) {
                if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                    for (const auto& elem : value) validate_wire_value(elem, ctx);
            } else if constexpr (is_fixed_string_v<CleanT>) {
                validate_wire_value(static_cast<const XFixedString&>(value), ctx);
            } else if constexpr (is_fixed_flat_set_v<CleanT>) {
                validate_wire_value(
                    static_cast<const XFixedVector<typename CleanT::value_type>&>(value), ctx);
                // No early exit: one error, and the loop vectorizes.
                bool unordered = false;
                for (std::uint32_t i = 1; i < value.size(); ++i)
                    unordered |= !fixed_detail::ordered_less(value.data()[i - 1], value.data()[i]);
                if (unordered) wire_reject("wire validation: set keys not strictly ordered");
            } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                validate_wire_value(
                    static_cast<const XFixedVector<typename CleanT::value_type>&>(value), ctx);
                // No early exit: one error, and the loop vectorizes.
                bool unordered = false;
                for (std::uint32_t i = 1; i < value.size(); ++i)
                    unordered |= !fixed_detail::ordered_less(value.data()[i - 1].first, value.data()[i].first);
                if (unordered) wire_reject("wire validation: map keys not strictly ordered");
            } else if constexpr (is_fixed_vector_v<CleanT>) {
                validate_wire_value(
                    static_cast<const XFixedVector<typename CleanT::value_type>&>(value), ctx);
            } else if constexpr (std::is_class_v<CleanT> && wire_value_needs_validation_v<CleanT>) {
                for_each_reflected_subobject<CleanT>(
                    [&](auto index) {
                        validate_base_at<CleanT, decltype(index)::value>(value, ctx);
                    },
                    [&](auto index) {
                        validate_member_at<CleanT, decltype(index)::value>(value, ctx);
                });
            }
        }

        // V2: holes == nullptr && check_coverage == false walks the graph
        // without the hole table (used to recompute holes from the live set).
        template <typename T>
        void verify_structural_wire_graph(const T& root, XBufferCore& xbuf,
                                          std::pmr::memory_resource* upstream,
                                          const void* holes,
                                          std::size_t hole_count,
                                          bool check_coverage = true,
                                          std::vector<XBufferCore::WireHole>* recomputed = nullptr,
                                          WireValidationScratch* retained = nullptr) {
            if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
            const auto used = xbuf.arena()->used_end;
            // W9: flat end for every storage kind. The extent check that R1+M2
            // applied only to canonical (receive) storage now applies to all;
            // it cannot fire for a valid ordinary arena (used_end <= size is an
            // allocator/load invariant), so it only turns a corrupted extent
            // into this error instead of an out-of-bounds validation context.
            if (used > xbuf.segment_size())
                wire_reject("wire validation: payload extent out of bounds");
            const char* validated_end = static_cast<const char*>(xbuf.get_address()) + used;
            // Default-initialized, then assigned: the granule index's inline
            // words are written by prepare() before any read. GCC 16 zeroes
            // the whole 1 KiB context for an aggregate initializer, with or
            // without [[indeterminate]], and for a default-initialized one
            // without the attribute.
            XOFFSET_INDETERMINATE WireValidationContext ctx;
            ctx.base = static_cast<const char*>(xbuf.get_address());
            ctx.end = validated_end;
            ctx.segment = xbuf.arena();
            ctx.upstream = upstream;
            ctx.retained = retained;
            if (ctx.segment->flags != 0 || ctx.segment->reserved0 != 0 || ctx.segment->reserved1 != 0) {
                wire_reject("wire validation: unsupported arena header flags");
            }
            register_owned_range(ctx.base, sizeof(XArena), ctx, "wire validation: invalid arena header");
            if (holes) register_wire_holes(holes, hole_count, ctx);
            register_allocation(&root, sizeof(T), alignof(T), ctx);
            validate_wire_value(root, ctx);
            if (check_coverage) require_exact_coverage(ctx);
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            auto& stats = wire_layout_stats<T>();
            if (ctx.cursor != WireValidationContext::no_cursor) {
                ++stats.canonical;
            } else if (ctx.stats_out_of_order == 0) {
                ++stats.ascending;
            } else {
                ++stats.out_of_order;
                stats.out_of_order_blocks += ctx.stats_out_of_order;
                stats.first_offset_sum += ctx.stats_first;
            }
#endif
            if (recomputed) {
                // Every byte not covered by a live block is free.
                recomputed->clear();
                for_each_unowned_run(ctx,
                    [&](std::size_t begin, std::size_t end) {
                        while (begin < end) {
                            const auto chunk = std::min<std::size_t>(end - begin, 0x40000000u);
                            recomputed->push_back({static_cast<std::uint32_t>(begin),
                                                   static_cast<std::uint32_t>(chunk)});
                            begin += chunk;
                        }
                    });
            }
        }

        template <typename T>
        void verify_structural_wire_graph(const T& root, XBufferCore& xbuf,
                                          std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            // A live buffer's own free table is its hole table.
            std::vector<XBufferCore::WireHole> holes;
            const bool complete = xbuf.free_table_complete();
            if (complete) {
                holes.reserve(xbuf.free_block_count());
                xbuf.for_each_free_block([&](XBufferCore::WireHole hole) { holes.push_back(hole); });
            }
            verify_structural_wire_graph(root, xbuf, upstream,
                complete ? holes.data() : nullptr, holes.size(), complete);
        }
    } // namespace detail

    namespace fixed_detail {
        template <typename T> struct is_owning_head : std::false_type {};
        template <> struct is_owning_head<XFixedString> : std::true_type {};
        template <> struct is_owning_head<XString> : std::true_type {};
        template <typename T> struct is_owning_head<XFixedVector<T>> : std::true_type {};
        template <typename T> struct is_owning_head<XVector<T>> : std::true_type {};
        template <typename T> struct is_owning_head<XFixedFlatSet<T>> : std::true_type {};
        template <typename T> struct is_owning_head<XSet<T>> : std::true_type {};
        template <typename K, typename V> struct is_owning_head<XFixedFlatMap<K, V>> : std::true_type {};
        template <typename K, typename V> struct is_owning_head<XMap<K, V>> : std::true_type {};

        // MV: for a string head, or a vector head whose elements are trivially
        // copyable, the generic function evaluates exactly one containment
        // test (no recursion); this is that test, inline. Every other type
        // calls the generic function, so results are identical for all T.
        template <typename T>
        XOFFSET_DETAIL_MOVE_INLINE bool move_assignment_owns_address_fast(
                const T& value, const void* address) noexcept {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_trivially_copyable_v<CleanT> || std::is_array_v<CleanT> ||
                          !is_owning_head<CleanT>::value) {
                return move_assignment_owns_address(value, address);
            } else if constexpr (detail::is_fixed_string_v<CleanT>) {
                return move_assignment_contains(value.data(), value.owned_extent(), address);
            } else if constexpr (std::is_trivially_copyable_v<typename CleanT::value_type>) {
                return move_assignment_contains(value.data(),
                    std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address);
            } else {
                return move_assignment_owns_address(value, address);
            }
        }

        template <typename T, std::size_t I>
        consteval bool member_can_relocate() {
            using member = schema::member_t<T, I>;
            if constexpr (member::is_bit_field) return false;
            else {
                using Member = typename member::type;
                return can_relocate_without_allocation<Member>();
            }
        }

        template <typename T, std::size_t I>
        consteval bool base_can_relocate() {
            using Base = schema::base_t<T, I>;
            return can_relocate_without_allocation<Base>();
        }

        template <typename T>
        consteval bool can_relocate_without_allocation() {
            if constexpr (std::is_const_v<T> || !std::is_nothrow_destructible_v<T>) return false;
            else if constexpr (detail::future_custom_lifetime_supported<T>()) return true;
            else if constexpr (std::is_array_v<T>) return can_relocate_without_allocation<std::remove_extent_t<T>>();
            else if constexpr (std::is_trivially_copyable_v<T> || is_owning_head<T>::value) return true;
            else if constexpr (std::is_class_v<T> && is_byte_copy_safe_v<T>) {
                constexpr auto bases = schema::base_count_v<T>;
                constexpr auto members = schema::member_count_v<T>;
                return []<std::size_t... I>(std::index_sequence<I...>) {
                    return (base_can_relocate<T, I>() && ...);
                }(std::make_index_sequence<bases>{}) && []<std::size_t... I>(std::index_sequence<I...>) {
                    return (member_can_relocate<T, I>() && ...);
                }(std::make_index_sequence<members>{});
            } else return false;
        }

        // O3: detect a head embedded anywhere in the other's owned tree.
        // No allocation, object relocation, or source mutation occurs here.
        template <typename T>
        bool move_assignment_owns_address(const T& value, const void* address) noexcept {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return false;
            } else if constexpr (std::is_array_v<CleanT>) {
                for (const auto& child : value) {
                    if (move_assignment_owns_address(child, address)) return true;
                }
                return false;
            } else if constexpr (is_owning_head<CleanT>::value) {
                if constexpr (detail::is_fixed_string_v<CleanT>) {
                    return move_assignment_contains(value.data(), value.owned_extent(), address);
                } else {
                    const auto* elements = value.data();
                    if (move_assignment_contains(elements,
                        std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address))
                        return true;
                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {
                        const auto count = value.size();
                        const auto* last = elements ? elements + count : nullptr;
                        for (auto* item = elements; item != last; ++item) {
                            const auto& child = *item;
                            if (move_assignment_owns_address(child, address)) return true;
                        }
                    }
                    return false;
                }
            } else if constexpr (std::is_class_v<CleanT> &&
                                 is_byte_copy_safe_v<CleanT>) {
                bool found = false;
                detail::for_each_reflected_subobject<CleanT>([&](auto index) {
                    using Base = schema::base_t<CleanT, decltype(index)::value>;
                    if (!found) found = move_assignment_owns_address(
                        static_cast<const Base&>(value), address);
                }, [&](auto index) {
                    using member = schema::member_t<CleanT, decltype(index)::value>;
                    if constexpr (!member::is_bit_field) {
                        if (!found) found = move_assignment_owns_address(member::get(value), address);
                    }
                });
                return found;
            } else {
                return false;
            }
        }

        // Both slots must be in the same bounded arena. Only inline ownership
        // heads move; out-of-line payloads keep their addresses and ownership.
        template <typename T>
        void relocate_construct(T* dst, T& src) noexcept {
            static_assert(can_relocate_without_allocation<T>());
            if constexpr (detail::future_custom_lifetime_supported<T>()) {
                detail::future_relocate_construct<T>(dst, src);
            } else if constexpr (std::is_array_v<T>) {
                for (std::size_t i = 0; i < std::extent_v<T>; ++i) relocate_construct(&(*dst)[i], src[i]);
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(dst, &src, sizeof(T));
            } else if constexpr (detail::is_fixed_string_v<T>) {
                std::construct_at(dst);
                src.relocate_payload_to(*dst);
            } else if constexpr (is_owning_head<T>::value) {
                std::construct_at(dst);
                set_rel32(dst->data_delta, &dst->data_delta, src.data());
                dst->size_ = src.size_;
                dst->capacity_ = src.capacity_;
                src.data_delta = 0;
                src.size_ = 0;
                src.capacity_ = 0;
            } else {
                dst = detail::begin_relocated_ordinary_outer<T>(dst, src);
                detail::for_each_reflected_subobject<T>([&](auto index) {
                    using Base = schema::base_t<T, decltype(index)::value>;
                    relocate_construct(static_cast<Base*>(dst), static_cast<Base&>(src));
                }, [&](auto index) {
                    using member = schema::member_t<T, decltype(index)::value>;
                    relocate_construct(&(member::get(*dst)), member::get(src));
                });
                detail::zero_implicit_padding(dst);
            }
        }
    } // namespace fixed_detail

    // Customization is opt-in only. It must explicitly begin the COMPLETE T
    // lifetime at a dead destination, transfer/rebase all inline ownership
    // heads without allocating, and empty source ownership before destruction.
    // Both slots belong to the same bounded arena. Nothrow syntax is checked;
    // the semantic lifetime/ownership obligations remain the author's contract.
    template<class T> struct StableEraseRelocator { static constexpr bool enabled = false; };
    // Explicit SOURCE contract, not a reflection guess: T has only ordinary
    // non-overlapping members, no [[no_unique_address]], union or bases. The
    // mechanical gate also requires aggregate/implicit/standard layout, no
    // bases/bit-fields and recursive support. Do not specialize for a type
    // whose source layout has not been audited against these obligations.
    template<class T> struct StableEraseOrdinaryMembers : std::false_type {};

    // Mechanical part of XOFFSET_ORDINARY_RECORDS. Physical disjointness
    // catches a [[no_unique_address]] member that shares storage, but not one
    // that merely could; the declaration stays a source contract.
    namespace detail {
        template <typename T>
        consteval bool ordinary_record_has_no_bases() {
            return schema::base_count_v<T> == 0;
        }
        template <typename T>
        consteval bool ordinary_record_has_no_bit_fields() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (!schema::member_t<T, I>::is_bit_field && ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }
        // Every pair of [offset, offset + sizeof) member ranges is disjoint.
        template <typename T>
        consteval bool ordinary_record_members_disjoint() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                constexpr std::size_t count = sizeof...(I);
                const std::size_t begin[] = {schema::member_t<T, I>::offset..., 0};
                const std::size_t end[] = {
                    (schema::member_t<T, I>::offset + sizeof(schema::member_type_t<T, I>))..., 0};
                for (std::size_t i = 0; i < count; ++i)
                    for (std::size_t j = i + 1; j < count; ++j)
                        if (begin[i] < end[j] && begin[j] < end[i]) return false;
                return true;
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }
    } // namespace detail

    namespace fixed_detail {
        // [class.prop]: an aggregate whose destructor is not user-provided is
        // implicit-lifetime, even when that destructor is nontrivial because a
        // member owns storage. This independent standard branch does not
        // specialize std traits or alter V1/K0 admission. The fixed
        // SDK's std trait is not used as a necessary condition for this branch.
        // Without std::is_implicit_lifetime (C++23) or the compiler's builtin
        // (C++20 on GCC 13, Clang 18): scalars, arrays, and classes with a
        // trivial destructor and a trivial default, copy or move constructor.
        // Callers handle aggregates with an implicit nontrivial destructor.
        template<class T> consteval bool cxx20_implicit_lifetime() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_scalar_v<U> || std::is_array_v<U>) return true;
            else if constexpr (std::is_class_v<U>)
                return std::is_trivially_destructible_v<U> &&
                       (std::is_trivially_default_constructible_v<U> ||
                        std::is_trivially_copy_constructible_v<U> ||
                        std::is_trivially_move_constructible_v<U>);
            else return false;
        }
        template<class T> consteval bool std_implicit_lifetime() {
#if defined(__cpp_lib_is_implicit_lifetime)
            return std::is_implicit_lifetime_v<T>;
#elif defined(__clang__)
#if __has_builtin(__builtin_is_implicit_lifetime)
            return __builtin_is_implicit_lifetime(T);
#elif __has_builtin(__is_implicit_lifetime)
            return __is_implicit_lifetime(T);
#else
            return cxx20_implicit_lifetime<T>();
#endif
#else
            return cxx20_implicit_lifetime<T>();
#endif
        }
        template<class T> consteval bool stable_erase_implicit_lifetime() {
            if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
            else if constexpr (std_implicit_lifetime<T>()) return true;
            else if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> &&
                               StableEraseOrdinaryMembers<T>::value) {
                return schema::implicit_lifetime_aggregate<T>();
            } else return false;
        }
        template<class T> consteval bool stable_erase_supported();
        template<class T, std::size_t I> consteval bool stable_erase_member_supported() {
            using member = schema::member_t<T, I>;
            if constexpr (member::is_bit_field) return false;
            else {
                using Member = typename member::type;
                return stable_erase_supported<Member>();
            }
        }
        template<class T> consteval bool stable_erase_supported() {
            if constexpr (std::is_const_v<T> || std::is_volatile_v<T> ||
                          !can_relocate_without_allocation<T>()) return false;
            else if constexpr (StableEraseRelocator<T>::enabled) {
                return requires(T* destination, T& source) {
                    { StableEraseRelocator<T>::relocate_construct(destination, source) } noexcept -> std::same_as<void>;
                };
            } else if constexpr (is_owning_head<T>::value) return true;
            else if constexpr (!stable_erase_implicit_lifetime<T>()) return false;
            else if constexpr (std::is_array_v<T>) return stable_erase_supported<std::remove_extent_t<T>>();
            else if constexpr (std::is_scalar_v<T>) return true;
            else if constexpr (std::is_class_v<T> &&
                               StableEraseOrdinaryMembers<T>::value &&
                               std::is_aggregate_v<T> && std::is_standard_layout_v<T> &&
                               (std::is_trivially_copyable_v<T> || is_byte_copy_safe_v<T>)) {
                constexpr auto bases = schema::base_count_v<T>;
                constexpr auto members = schema::member_count_v<T>;
                // A placement into an owning base is not by itself a proof of
                // a base-subobject lifetime in a newly begun enclosing T.
                if constexpr (bases != 0) return false;
                else return []<std::size_t... I>(std::index_sequence<I...>) {
                    return (stable_erase_member_supported<T, I>() && ...);
                }(std::make_index_sequence<members>{});
            } else return false;
        }

        template<class T> void stable_erase_relocate(T* destination, T& source) noexcept;
        // The outer memcpy already began every eligible implicit subobject's
        // lifetime. Only nontrivial ownership/custom subobjects need rebuilding.
        template<class T> void stable_erase_rebase_subobjects(T* destination, T& source) noexcept {
            if constexpr (StableEraseRelocator<T>::enabled || is_owning_head<T>::value) {
                stable_erase_relocate(destination, source);
            } else if constexpr (std::is_array_v<T>) {
                for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                    stable_erase_rebase_subobjects(&(*destination)[i], source[i]);
            } else if constexpr (!std::is_trivially_copyable_v<T> && std::is_class_v<T>) {
                static_assert(schema::base_count_v<T> == 0);
                detail::for_each_reflected_subobject<T>([&](auto) {}, [&](auto index) {
                    using member = schema::member_t<T, decltype(index)::value>;
                    stable_erase_rebase_subobjects(&(member::get(*destination)), member::get(source));
                });
            }
        }
        template<class T> void stable_erase_relocate(T* destination, T& source) noexcept {
            static_assert(stable_erase_supported<T>());
            if constexpr (StableEraseRelocator<T>::enabled) {
                StableEraseRelocator<T>::relocate_construct(destination, source);
            } else if constexpr (detail::is_fixed_string_v<T>) {
                std::construct_at(destination);
                source.relocate_payload_to(*destination);
            } else if constexpr (is_owning_head<T>::value) {
                // These explicit constructors begin owning-head lifetimes even
                // when they were not created by an enclosing implicit copy.
                std::construct_at(destination);
                set_rel32(destination->data_delta, &destination->data_delta, source.data());
                destination->size_ = source.size_;
                destination->capacity_ = source.capacity_;
                source.data_delta = 0; source.size_ = 0; source.capacity_ = 0;
            } else {
                static_assert(stable_erase_implicit_lifetime<T>());
                // This copies only sizeof(T) INLINE bytes, never its dynamic
                // payload. memcpy explicitly supplies implicit object creation;
                // member-only construction would not begin a nonimplicit T.
                std::memcpy(destination, &source, sizeof(T));
                stable_erase_rebase_subobjects(destination, source);
            }
        }
    } // namespace fixed_detail

    struct MemoryStats {
        std::size_t total_size;
        std::size_t free_size;
        std::size_t used_size;
        double usage_percent() const { return total_size > 0 ? (used_size * 100.0 / total_size) : 0.0; }
    };

    inline MemoryStats memory_stats(XBufferCore& xbuf) {
        return { xbuf.get_size(), xbuf.get_free_memory(), xbuf.get_size() - xbuf.get_free_memory() };
    }

    // V2.1 wire: [XWireHeaderV2][extent table: extent_count x {u32 offset,
    // u32 bytes}][payload bytes][hole table: hole_count x {u32 offset,
    // u32 bytes}]. The 48-byte payload header is not on the wire: the
    // receiver writes it from used_bytes and root_offset, so the payload
    // bytes start at payload offset XWIRE_PAYLOAD_BEGIN. extent_count 0: the
    // payload bytes are [48, used_bytes), contiguous. Otherwise they are the
    // concatenated extents (ascending, disjoint, 8-byte multiples, the first
    // at offset 48); the gaps between extents load as zero bytes (sparse
    // wire). A gap may cover any payload bytes: the loaded payload is
    // validated exactly like a dense one. Every offset (root, extents,
    // holes) is a payload offset. Blocks have no headers; live blocks plus
    // holes tile the payload exactly (see detail::require_exact_coverage).
    // Little-endian; the length is exactly 40 + 8 * extent_count + payload
    // bytes + 8 * hole_count.
    struct alignas(8) XWireHeaderV2 {
        std::uint32_t magic;        // 'X' 'O' major minor (XWIRE_MAGIC)
        std::uint32_t flags;        // 0
        std::uint32_t used_bytes;   // payload used_end, payload header included
        std::uint32_t root_offset;  // payload offset of the root, >= 48
        std::uint32_t hole_count;
        std::uint32_t extent_count;
        std::uint64_t schema_hash;  // fingerprint: type name and ABI signature
        std::uint64_t root_type_id; // fingerprint: type name
    };
    static_assert(sizeof(XWireHeaderV2) == 40 && alignof(XWireHeaderV2) == 8);

    // Emission forms of save_verified / save_verified_into. Receivers
    // accept every form; the loaded payload is byte-identical.
    enum class XWireEmission : std::uint8_t {
        // Default. Skips each run of at least 4 KiB of adjacent free blocks
        // of at least 256 B (one large deletion, for example); everything
        // else is copied. Skipped bytes are all holes, so every V2 receiver
        // accepts it.
        free_runs,
        // One copy of the whole payload.
        dense,
        // Skips every zero run of at least 64 bytes (capacity slack, holes,
        // zero fields), for cross-machine sends. Costs a scan of the payload.
        // Payloads below 4 KiB, or where it would save less than max(256 B,
        // 1/16 of the payload), stay dense.
        zero_runs,
    };
    struct XSaveOptions {
        XWireEmission emission = XWireEmission::free_runs;
    };

    // Options of save_compact / save_compact_into.
    struct XCompactOptions {
        // Vectors of at least 8 elements (not strings) keep size / slack_div
        // + 1 zeroed spare elements, so a receiver that edits them does not
        // relocate on the first insert. 0: capacity == size, byte-identical
        // to XCompactor::compact followed by save_verified (for stored or
        // read-only receivers).
        std::uint32_t slack_div = 8;
    };

    // One assignment of save_compact_into(out, options, xassign(target,
    // source)...): the wire is written as if `*target = *source` had been
    // done first, without doing it. target: a member subobject (any depth,
    // scalars and array elements included) of the saved buffer's root, not
    // inside a container; source: a valid value of the same type (one with
    // container heads lives in an XOffset buffer).
    template <class M>
    struct XAssign {
        using value_type = M;
        M* target;
        const M* source;
    };
    template <class M>
    XAssign<M> xassign(M& target, const M& source) noexcept { return {&target, &source}; }
#define XOFFSET_SAVE_COMPOSED_API 1

    inline constexpr std::uint8_t XWIRE_FORMAT_MAJOR = 2;
    inline constexpr std::uint8_t XWIRE_FORMAT_MINOR = 1;
    inline constexpr std::uint32_t XWIRE_MAGIC = 0x4F58u | (std::uint32_t(XWIRE_FORMAT_MAJOR) << 16) |
                                                 (std::uint32_t(XWIRE_FORMAT_MINOR) << 24);
    static_assert(XWIRE_MAGIC == 0x01024F58u);
    inline constexpr std::uint32_t XWIRE_FLAGS = 0;
    // Payload offset of the first payload byte on the wire (the payload
    // header's size).
    inline constexpr std::size_t XWIRE_PAYLOAD_BEGIN = 48;
    static_assert(sizeof(XBufferCore::arena_header) == XWIRE_PAYLOAD_BEGIN);

    namespace detail {
        [[noreturn]] inline void wire_reject(const char* who, const char* what) {
            throw XException(std::string(who) + what);
        }

        // Reads the header at the front of a wire of `size` bytes and checks
        // its version and flags. The version is told apart first, so a short
        // or older wire gets the message that names it. V1 and V2.0 wires
        // begin "XOFFV1" / "XOFFV2"; 'F' is no 2.x version byte.
        inline XWireHeaderV2 read_wire_header(const void* data, std::size_t size, const char* who) {
            const auto* b = static_cast<const unsigned char*>(data);
            if (size < 4 || b[0] != 'X' || b[1] != 'O') wire_reject(who, ": not an XOffset wire (bad magic)");
            if (b[2] != XWIRE_FORMAT_MAJOR || b[3] != XWIRE_FORMAT_MINOR) {
                if (size >= 6 && std::memcmp(b, "XOFFV1", 6) == 0)
                    wire_reject(who, ": wire format 1 is not supported; this build reads 2.1");
                if (size >= 6 && std::memcmp(b, "XOFFV2", 6) == 0)
                    wire_reject(who, ": wire format 2.0 is not supported; this build reads 2.1 "
                                     "(sender and receiver must upgrade together)");
                throw XException(std::string(who) + ": wire format " + std::to_string(b[2]) + "." +
                                 std::to_string(b[3]) + " is not supported; this build reads 2.1");
            }
            if (size < sizeof(XWireHeaderV2)) wire_reject(who, ": truncated wire header");
            XWireHeaderV2 header;
            std::memcpy(&header, data, sizeof(header));
            if (header.flags != XWIRE_FLAGS) {
                std::string hex = "0x";
                for (int shift = 28; shift >= 0; shift -= 4) hex += "0123456789abcdef"[(header.flags >> shift) & 15u];
                throw XException(std::string(who) + ": unsupported wire flags " + hex);
            }
            return header;
        }

        // used_bytes and root_offset: the payload holds at least its header,
        // ends on a word, and the root starts past the header inside it.
        inline void check_wire_extent(const XWireHeaderV2& header, const char* who) {
            if (header.used_bytes < XWIRE_PAYLOAD_BEGIN || header.used_bytes > XBufferCore::MAX_PAYLOAD ||
                header.used_bytes % 8u != 0)
                wire_reject(who, ": used size mismatch");
            if (header.root_offset < XWIRE_PAYLOAD_BEGIN || header.root_offset >= header.used_bytes)
                wire_reject(who, ": root offset out of bounds");
        }

        static_assert(sizeof(XBufferCore::arena_header) == XWIRE_PAYLOAD_BEGIN);

        // ---- V2 wire layout (dense or sparse) -----------------------------

        struct WireLayout {
            const char* stored = nullptr;   // payload bytes (dense) or concatenated extents
            const char* extents = nullptr;  // extent table (sparse) or nullptr
            std::size_t extent_count = 0;
            const char* holes = nullptr;    // hole table
            std::size_t hole_count = 0;
            std::vector<XBufferCore::WireHole> gaps; // skipped payload ranges
        };

        // Validates an extent table (count entries at `table`, unaligned) for
        // a payload of `used` bytes; appends the gaps and returns the payload
        // bytes on the wire.
        inline std::uint64_t parse_extent_table(const char* table, std::size_t count,
                                                std::uint64_t used, const char* who,
                                                std::vector<XBufferCore::WireHole>& gaps) {
            if (count == 0) return used - XWIRE_PAYLOAD_BEGIN;
            std::uint64_t stored = 0, end = 0;
            for (std::size_t i = 0; i < count; ++i) {
                std::uint32_t e[2];
                std::memcpy(e, table + i * 8u, sizeof(e));
                if (e[1] == 0 || (e[0] & 7u) != 0 || (e[1] & 7u) != 0 ||
                    (i == 0 ? e[0] != XWIRE_PAYLOAD_BEGIN : e[0] <= end) ||
                    std::uint64_t(e[0]) + e[1] > used)
                    throw XException(std::string(who) + ": invalid extent table");
                if (i != 0) gaps.push_back({static_cast<std::uint32_t>(end),
                                            static_cast<std::uint32_t>(e[0] - end)});
                end = std::uint64_t(e[0]) + e[1];
                stored += e[1];
            }
            if (end < used) gaps.push_back({static_cast<std::uint32_t>(end),
                                            static_cast<std::uint32_t>(used - end)});
            if (gaps.empty()) throw XException(std::string(who) + ": invalid extent table");
            return stored;
        }

        // M2: names the limit a wire exceeds, before any reservation.
        inline void require_wire_within_capacity(std::size_t used, std::size_t limit,
                                                 bool explicit_limit, const char* who) {
            if (used <= limit) return;
            throw XBadAlloc(std::string(who) + ": wire used_bytes " + std::to_string(used) +
                            " exceeds " + (explicit_limit ? "maximum_capacity " :
                                           "the default maximum_capacity ") +
                            std::to_string(limit) +
                            (explicit_limit ? "" : "; pass XBuffer::maximum_capacity() for larger wires"));
        }

        // Validates used_bytes, root_offset, the extent table and the length
        // of a wire whose header passed read_wire_header and the fingerprint
        // check. `body` follows the header. Does not touch the payload.
        inline WireLayout parse_wire_layout(const XWireHeaderV2& header,
                                            const char* body, std::size_t body_size,
                                            const char* who) {
            check_wire_extent(header, who);
            WireLayout layout;
            if (header.extent_count > body_size / 8u) wire_reject(who, ": used size mismatch");
            const auto table = std::size_t(header.extent_count) * 8u;
            layout.extent_count = std::size_t(header.extent_count);
            if (layout.extent_count) layout.extents = body;
            const auto stored = parse_extent_table(body, layout.extent_count, header.used_bytes, who, layout.gaps);
            if (stored > body_size - table) wire_reject(who, ": used size mismatch");
            const auto rest = body_size - table - std::size_t(stored);
            if (header.hole_count > rest / 8u || std::size_t(header.hole_count) * 8u != rest)
                wire_reject(who, ": used size mismatch");
            layout.stored = body + table;
            layout.holes = body + table + stored;
            layout.hole_count = std::size_t(header.hole_count);
            return layout;
        }

        // E1 (N8): on Darwin, outputs of at least 1 MiB are copied in 256 KiB
        // chunks, each preceded by madvise(MADV_WILLNEED) on its pages.
#if defined(__linux__)
        // E-2: on Linux, in an output of at least 1 MiB, each copy of at least
        // 256 KiB whose next-to-last page is not resident (a fresh mapping, not
        // a reused string) goes in 256 KiB chunks, each preceded by
        // madvise(MADV_POPULATE_WRITE), so the kernel faults the chunk's pages
        // in one call instead of one trap per page.
        // A kernel without it (before 5.14) answers EINVAL once; the output
        // is then copied as before for the rest of the process.
#ifndef MADV_POPULATE_WRITE
#define MADV_POPULATE_WRITE 23
#endif
#ifndef XOFFSET_WIRE_MADVISE
#define XOFFSET_WIRE_MADVISE ::madvise
#endif
        inline std::atomic<bool> wire_populate_unsupported{false};
#endif
        struct WireCopier {
            bool prefault = false;
            std::size_t page = 0;
            void copy(char* dst, const char* src, std::size_t n) const noexcept {
#if defined(__APPLE__)
                if (prefault) {
                    constexpr std::size_t chunk = std::size_t(256) << 10;
                    for (std::size_t off = 0; off < n; off += chunk) {
                        const auto len = std::min(chunk, n - off);
                        const auto lo = reinterpret_cast<std::uintptr_t>(dst + off) & ~(page - 1u);
                        const auto hi = (reinterpret_cast<std::uintptr_t>(dst + off) + len + page - 1u) & ~(page - 1u);
                        (void)::madvise(reinterpret_cast<void*>(lo), hi - lo, MADV_WILLNEED);
                        std::memcpy(dst + off, src + off, len);
                    }
                    return;
                }
#elif defined(__linux__)
                constexpr std::size_t chunk = std::size_t(256) << 10;
                unsigned char probe_page = 1;
                // Not the last page: libc++'s resize_and_overwrite writes the
                // terminator before the output is filled, which makes that page
                // resident in a fresh mapping too. n >= 256 KiB, so the page
                // before it is still inside the copy.
                if (prefault && n >= chunk && !wire_populate_unsupported.load(std::memory_order_relaxed) &&
                    ::mincore(reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(dst + n - 1u) & ~(page - 1u)) - page),
                              page, &probe_page) == 0 && (probe_page & 1u) == 0) {
                    bool populate = true;
                    for (std::size_t off = 0; off < n; off += chunk) {
                        const auto len = std::min(chunk, n - off);
                        if (populate) {
                            const auto lo = reinterpret_cast<std::uintptr_t>(dst + off) & ~(page - 1u);
                            const auto hi = (reinterpret_cast<std::uintptr_t>(dst + off) + len + page - 1u) & ~(page - 1u);
                            if (XOFFSET_WIRE_MADVISE(reinterpret_cast<void*>(lo), hi - lo, MADV_POPULATE_WRITE) != 0 &&
                                errno == EINVAL) {
                                wire_populate_unsupported.store(true, std::memory_order_relaxed);
                                populate = false;
                            }
                        }
                        std::memcpy(dst + off, src + off, len);
                    }
                    return;
                }
#endif
                std::memcpy(dst, src, n);
            }
        };
        inline WireCopier wire_copier(std::size_t output_bytes) noexcept {
            WireCopier copier;
#if defined(__APPLE__)
            if (output_bytes >= (std::size_t(1) << 20)) {
                copier.prefault = true;
                copier.page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
                if (copier.page == 0 || (copier.page & (copier.page - 1u)) != 0) copier.prefault = false;
            }
#elif defined(__linux__)
            if (output_bytes >= (std::size_t(1) << 20) && !wire_populate_unsupported.load(std::memory_order_relaxed)) {
                copier.page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
                copier.prefault = copier.page != 0 && (copier.page & (copier.page - 1u)) == 0;
            }
#else
            (void)output_bytes;
#endif
            return copier;
        }

        // Dense wire size: header, payload bytes [48, used_bytes), hole table.
        // Every other emission is smaller (see write_wire).
        inline std::size_t wire_dense_size(const XWireHeaderV2& header, const char* who) {
            const std::uint64_t total = sizeof(XWireHeaderV2) + (header.used_bytes - XWIRE_PAYLOAD_BEGIN) +
                std::uint64_t(header.hole_count) * sizeof(XBufferCore::WireHole);
            if (total > (std::numeric_limits<std::size_t>::max)())
                throw std::length_error(std::string(who) + ": output size exceeds string limit");
            return static_cast<std::size_t>(total);
        }

        // ---- Free-run emission (XWireEmission::free_runs, the default) ------
        // Candidates are free blocks of at least wire_free_run_min_block
        // bytes (only those bins and the large blocks are visited); sorted by
        // offset (LSD radix, 11-bit digits), adjacent candidates merge, and
        // every merged run of at least wire_free_run_min_gap is skipped.
        inline constexpr std::uint32_t wire_free_run_min_block = 256;
        inline constexpr std::uint32_t wire_free_run_min_gap = 4096;

        struct WireGaps {
            std::vector<XBufferCore::WireHole> runs; // ascending, disjoint
            std::uint64_t skipped = 0;
            std::size_t extent_count = 0;
        };

        inline WireGaps wire_free_runs(const XBufferCore& core, std::uint64_t used) {
            WireGaps gaps;
            if (core.free_block_bytes() < wire_free_run_min_gap || used == 0) return gaps;
            std::vector<XBufferCore::WireHole> candidates;
            std::uint64_t total = 0;
            core.for_each_free_block_at_least(wire_free_run_min_block, [&](XBufferCore::WireHole hole) {
                candidates.push_back(hole);
                total += hole.bytes;
            });
            if (total < wire_free_run_min_gap) return gaps;
            std::vector<XBufferCore::WireHole> sorted(candidates.size());
            unsigned bits = 0;
            while (bits < 32u && (std::uint64_t(1) << bits) < used) ++bits;
            for (unsigned shift = 0; shift < bits; shift += 11u) {
                std::uint32_t count[2049] = {};
                for (const auto& hole : candidates) ++count[((hole.offset >> shift) & 2047u) + 1u];
                for (std::size_t i = 0; i < 2048u; ++i) count[i + 1u] += count[i];
                for (const auto& hole : candidates) sorted[count[(hole.offset >> shift) & 2047u]++] = hole;
                candidates.swap(sorted);
            }
            std::uint64_t start = 0, end = 0;
            auto close = [&] {
                // The first extent starts at 48 and is not empty: a run there
                // (free bytes before the root) keeps its first word.
                if (start == XWIRE_PAYLOAD_BEGIN) start += 8u;
                if (end - start >= wire_free_run_min_gap) {
                    gaps.runs.push_back({static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(end - start)});
                    gaps.skipped += end - start;
                }
            };
            for (const auto& hole : candidates) {
                if (hole.offset == end && end != 0) { end += hole.bytes; continue; }
                close();
                start = hole.offset;
                end = std::uint64_t(hole.offset) + hole.bytes;
            }
            close();
            if (!gaps.runs.empty()) {
                // No run starts at or before 48: one extent precedes each run.
                const auto& last = gaps.runs.back();
                gaps.extent_count = gaps.runs.size() + (std::uint64_t(last.offset) + last.bytes < used ? 1u : 0u);
            }
            return gaps;
        }

        // Exact wire size of a free-run emission (dense when `gaps` is empty).
        inline std::size_t wire_free_run_size(const XWireHeaderV2& header, const WireGaps& gaps,
                                              const char* who) {
            return wire_dense_size(header, who) - static_cast<std::size_t>(gaps.skipped) +
                gaps.extent_count * 8u;
        }

        // ---- Zero-run elision (XWireEmission::zero_runs) ---------------
        // Payloads below wire_zero_scan_min_payload are not scanned. A run is
        // found from a 64-byte aligned all-zero block and extended word by
        // word on both sides; every skipped run is at least
        // wire_zero_run_min bytes. The sparse form is used only when it saves
        // at least max(wire_zero_min_saving, used / 16) bytes.
        inline constexpr std::size_t wire_zero_run_min = 64;
        inline constexpr std::size_t wire_zero_scan_min_payload = 4096;
        inline constexpr std::size_t wire_zero_min_saving = 256;

        // Writes the extent table of `payload` (8-aligned, `used` a multiple
        // of 8) at `table` and returns the extent count, or 0 when no run is
        // skipped. The table never exceeds `used` - 48 bytes: every extent
        // but the first holds at least one nonzero word, and every skipped
        // run is at least 64 bytes. The first extent starts at 48 and holds
        // at least the word there. Block 0 holds the payload header (whose
        // magic is never zero), so no run starts in it.
        inline std::size_t wire_zero_run_extents(const char* payload, std::size_t used,
                                                 char* table, std::uint64_t& skipped) noexcept {
            auto word = [&](std::size_t at) noexcept {
                std::uint64_t w;
                std::memcpy(&w, payload + at, sizeof(w));
                return w;
            };
            auto block_zero = [&](std::size_t block) noexcept {
                std::uint64_t w[8];
                std::memcpy(w, payload + block * 64u, sizeof(w));
                return ((w[0] | w[1]) | (w[2] | w[3]) | (w[4] | w[5]) | (w[6] | w[7])) == 0;
            };
            std::size_t count = 0, cursor = XWIRE_PAYLOAD_BEGIN;
            auto emit = [&](std::size_t begin, std::size_t end) noexcept {
                const std::uint32_t entry[2] = {static_cast<std::uint32_t>(begin),
                                                static_cast<std::uint32_t>(end - begin)};
                std::memcpy(table + count * 8u, entry, sizeof(entry));
                ++count;
            };
            skipped = 0;
            const std::size_t blocks = used / 64u;
            for (std::size_t b = 0; b < blocks;) {
                if (!block_zero(b)) { ++b; continue; }
                std::size_t start = b * 64u;
                while (start > cursor && word(start - 8u) == 0) start -= 8u;
                std::size_t e = b + 1u;
                while (e < blocks && block_zero(e)) ++e;
                std::size_t stop = e * 64u;
                while (stop < used && word(stop) == 0) stop += 8u;
                if (start < XWIRE_PAYLOAD_BEGIN + 8u) start = XWIRE_PAYLOAD_BEGIN + 8u;
                if (stop - start >= wire_zero_run_min) {
                    emit(cursor, start);
                    skipped += stop - start;
                    cursor = stop;
                }
                // The block holding `stop` has a nonzero word at `stop`.
                b = stop / 64u + 1u;
            }
            if (count == 0) return 0;
            if (cursor < used) emit(cursor, used);
            return count;
        }

        // Writes the wire for `header` (extent_count is set here) into `out`,
        // which holds at least wire_free_run_size(header, gaps) bytes, or
        // wire_dense_size(header) for zero_runs, and returns the bytes
        // written. `gaps` comes from wire_free_runs (free_runs) or is empty.
        // No allocation and no callback.
        inline std::size_t write_wire(char* out, XWireHeaderV2 header, const char* payload,
                                      const XBufferCore& core, XWireEmission emission,
                                      const WireGaps& gaps) noexcept {
            const auto used = static_cast<std::size_t>(header.used_bytes);
            char* at = out + sizeof(XWireHeaderV2);
            std::size_t extents = 0;
            if (emission == XWireEmission::free_runs && gaps.extent_count) {
                extents = gaps.extent_count;
                std::uint32_t begin = XWIRE_PAYLOAD_BEGIN;
                for (const auto& gap : gaps.runs) {
                    const std::uint32_t entry[2] = {begin, gap.offset - begin};
                    std::memcpy(at, entry, sizeof(entry));
                    at += sizeof(entry);
                    begin = gap.offset + gap.bytes;
                }
                if (begin < used) {
                    const std::uint32_t entry[2] = {begin, static_cast<std::uint32_t>(used - begin)};
                    std::memcpy(at, entry, sizeof(entry));
                }
                at = out + sizeof(XWireHeaderV2);
            } else if (emission == XWireEmission::zero_runs && used >= wire_zero_scan_min_payload) {
                std::uint64_t skipped = 0;
                extents = wire_zero_run_extents(payload, used, at, skipped);
                if (extents && skipped - extents * 8u < (std::max)(std::uint64_t(wire_zero_min_saving),
                                                                    std::uint64_t(used / 16u)))
                    extents = 0;
            }
            header.extent_count = static_cast<std::uint32_t>(extents);
            std::memcpy(out, &header, sizeof(header));
            const auto copier = wire_copier(sizeof(XWireHeaderV2) + used - XWIRE_PAYLOAD_BEGIN);
            if (extents == 0) {
                copier.copy(at, payload + XWIRE_PAYLOAD_BEGIN, used - XWIRE_PAYLOAD_BEGIN);
                at += used - XWIRE_PAYLOAD_BEGIN;
            } else {
                const char* table = at;
                at += extents * 8u;
                for (std::size_t i = 0; i < extents; ++i) {
                    std::uint32_t entry[2];
                    std::memcpy(entry, table + i * 8u, sizeof(entry));
                    copier.copy(at, payload + entry[0], entry[1]);
                    at += entry[1];
                }
            }
            core.write_free_blocks(at);
            return static_cast<std::size_t>(at - out) +
                core.free_block_count() * sizeof(XBufferCore::WireHole);
        }
    }

    // Type admission: is_byte_copy_safe_v (xoffset_admission.hpp); XOffset's
    // frozen containers are specialized at the end of this header.

    template <typename T>
    inline constexpr bool is_v1_wire_admitted_v = detail::v1_wire_admitted<T>();

    // ========================================================================
    // Reflection-based construction and transfer (C++26 P2996).
    // ========================================================================
    namespace detail {
        namespace ft = ::XOffsetDatastructure::fixed_text;

        consteval std::uint64_t fnv1a_append(std::uint64_t seed, std::string_view sv) {
            constexpr std::uint64_t prime = 1099511628211ull;
            for (char ch : sv) {
                seed ^= static_cast<unsigned char>(ch);
                seed *= prime;
            }
            return seed;
        }

        consteval std::uint64_t fnv1a(std::string_view sv) {
            return fnv1a_append(14695981039346656037ull, sv);
        }

        template <typename T>
        consteval std::uint64_t wire_root_type_id() {
            constexpr auto name = schema_name<T>::value;
            static_assert(!name.empty(),
                "wire_root_type_id<T> requires XOFFSET_REGISTER_SCHEMA_NAME(T, \"...\")");
            return fnv1a(name);
        }

        template <typename T>
        struct is_xkeyvalue : std::false_type {};
        template <typename K, typename V>
        struct is_xkeyvalue<XKeyValue<K, V>> : std::true_type {};
        template <typename T>
        inline constexpr bool is_xkeyvalue_v = is_xkeyvalue<std::remove_cv_t<T>>::value;

        template <typename T>
        consteval auto wire_abi_signature();

        template <typename T>
        consteval auto wire_scalar_signature() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, bool>) {
                return ft::FixedString{"bool"};
            } else if constexpr (std::is_same_v<U, char>) {
                return ft::FixedString{"char"};
            } else if constexpr (std::is_same_v<U, signed char>) {
                return ft::FixedString{"i8char"};
            } else if constexpr (std::is_same_v<U, unsigned char>) {
                return ft::FixedString{"u8char"};
            } else if constexpr (std::is_same_v<U, std::byte>) {
                return ft::FixedString{"byte"};
            } else if constexpr (std::is_same_v<U, int8_t>) {
                return ft::FixedString{"i8"};
            } else if constexpr (std::is_same_v<U, uint8_t>) {
                return ft::FixedString{"u8"};
            } else if constexpr (std::is_same_v<U, int16_t>) {
                return ft::FixedString{"i16"};
            } else if constexpr (std::is_same_v<U, uint16_t>) {
                return ft::FixedString{"u16"};
            } else if constexpr (std::is_same_v<U, int32_t>) {
                return ft::FixedString{"i32"};
            } else if constexpr (std::is_same_v<U, uint32_t>) {
                return ft::FixedString{"u32"};
            } else if constexpr (std::is_same_v<U, int64_t>) {
                return ft::FixedString{"i64"};
            } else if constexpr (std::is_same_v<U, uint64_t>) {
                return ft::FixedString{"u64"};
            } else if constexpr (std::is_same_v<U, float>) {
                return ft::FixedString{"f32"};
            } else if constexpr (std::is_same_v<U, double>) {
                return ft::FixedString{"f64"};
            } else {
                static_assert(sizeof(U) == 0, "unsupported v1 scalar in wire_abi_signature()");
            }
        }

        template <typename T, std::size_t N>
        consteval auto fixed_sequence_header_signature(const ft::FixedString<N>& tag) {
            return tag +
                   ft::FixedString{"[s:"} +
                   ft::to_fixed_string<sizeof(std::remove_cv_t<T>)>() +
                   ft::FixedString{",a:"} +
                   ft::to_fixed_string<alignof(std::remove_cv_t<T>)>() +
                   ft::FixedString{"]{data:rel32,size:u32,capacity:u32}"};
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature() {
            using U = std::remove_cv_t<T>;
            using member = schema::member_t<U, Index>;
            static_assert(!member::is_bit_field,
                "wire_abi_signature() does not support bit-fields in v1");
            using FieldType = typename member::type;
            return ft::FixedString{"@"} +
                   ft::to_fixed_string<member::offset>() +
                   ft::FixedString{":"} +
                   wire_abi_signature<FieldType>();
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature_with_sep() {
            if constexpr (Index == 0) {
                return wire_record_field_signature<T, Index>();
            } else {
                return ft::FixedString{","} + wire_record_field_signature<T, Index>();
            }
        }

        template <typename T, std::size_t... Is>
        consteval auto wire_record_fields(std::index_sequence<Is...>) {
            if constexpr (sizeof...(Is) == 0) {
                return ft::FixedString{""};
            } else {
                return (wire_record_field_signature_with_sep<T, Is>() + ...);
            }
        }

        template <typename T>
        consteval auto wire_record_signature() {
            using U = std::remove_cv_t<T>;
            static_assert(schema::base_count_v<U> == 0,
                "wire_abi_signature() does not support base classes in v1");
            constexpr std::size_t field_count = schema::member_count_v<U>;
            return ft::FixedString{"record[s:"} +
                   ft::to_fixed_string<sizeof(U)>() +
                   ft::FixedString{",a:"} +
                   ft::to_fixed_string<alignof(U)>() +
                   ft::FixedString{"]{"} +
                   wire_record_fields<U>(std::make_index_sequence<field_count>{}) +
                   ft::FixedString{"}"};
        }

        template <typename T>
        consteval auto wire_abi_signature() {
            using U = std::remove_cv_t<T>;
            if constexpr (detail::fixed_size_scalar<U>) {
                return wire_scalar_signature<U>();
            } else if constexpr (std::is_enum_v<U>) {
                return ft::FixedString{"enum<"} +
                       wire_abi_signature<std::underlying_type_t<U>>() +
                       ft::FixedString{">"};
            } else if constexpr (std::is_array_v<U>) {
                return ft::FixedString{"array["} +
                       ft::to_fixed_string<std::extent_v<U>>() +
                       ft::FixedString{"]<"} +
                       wire_abi_signature<std::remove_extent_t<U>>() +
                       ft::FixedString{">"};
            } else if constexpr (is_fixed_string_v<U>) {
                return fixed_sequence_header_signature<U>(ft::FixedString{"xstring"}) +
                       ft::FixedString{"{inline:10}"};
            } else if constexpr (is_fixed_flat_map_v<U>) {
                return fixed_sequence_header_signature<U>(ft::FixedString{"xflatmap"}) +
                       ft::FixedString{"<"} +
                       wire_abi_signature<typename U::key_type>() +
                       ft::FixedString{","} +
                       wire_abi_signature<typename U::mapped_type>() +
                       ft::FixedString{">"};
            } else if constexpr (is_fixed_flat_set_v<U>) {
                return fixed_sequence_header_signature<U>(ft::FixedString{"xflatset"}) +
                       ft::FixedString{"<"} +
                       wire_abi_signature<typename U::value_type>() +
                       ft::FixedString{">"};
            } else if constexpr (is_fixed_vector_v<U>) {
                if constexpr (std::is_same_v<typename U::value_type, std::byte>) {
                    return fixed_sequence_header_signature<U>(ft::FixedString{"xblob"});
                } else {
                    return fixed_sequence_header_signature<U>(ft::FixedString{"xvector"}) +
                           ft::FixedString{"<"} +
                           wire_abi_signature<typename U::value_type>() +
                           ft::FixedString{">"};
                }
            } else if constexpr (is_xkeyvalue_v<U>) {
                return ft::FixedString{"xkv[s:"} +
                       ft::to_fixed_string<sizeof(U)>() +
                       ft::FixedString{",a:"} +
                       ft::to_fixed_string<alignof(U)>() +
                       ft::FixedString{"]<"} +
                       wire_abi_signature<typename U::first_type>() +
                       ft::FixedString{","} +
                       wire_abi_signature<typename U::second_type>() +
                       ft::FixedString{">"};
            } else if constexpr (std::is_class_v<U>) {
                return wire_record_signature<U>();
            } else {
                static_assert(sizeof(U) == 0,
                    "wire_abi_signature() encountered a non-admitted v1 type");
            }
        }

        template <typename T>
        consteval std::uint64_t wire_schema_hash() {
            constexpr auto sig = wire_abi_signature<T>();
            std::uint64_t hash = fnv1a(schema_name<T>::value);
            hash = fnv1a_append(hash, std::string_view(sig));
            return hash;
        }

        template <typename T>
        concept has_wire_allocator_ctor = requires(fixed_detail::allocator_ref alloc) {
            T(alloc);
        };

        inline fixed_detail::allocator_ref allocator_binding_for(XBufferCore& xbuf) {
            return {xbuf.arena()};
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc);

        template <typename T>
        void destroy_reflected_prefix(void* raw, std::size_t built) noexcept {
            constexpr auto base_count = schema::base_count_v<T>;
            constexpr auto member_count = schema::member_count_v<T>;
            auto* obj = reinterpret_cast<T*>(raw);
            static_for<member_count>([&](auto index) {
                constexpr auto i = member_count - 1 - decltype(index)::value;
                using member = schema::member_t<T, i>;
                if (base_count + i < built) std::destroy_at(&(member::get(*obj)));
            });
            static_for<base_count>([&](auto index) {
                constexpr auto i = base_count - 1 - decltype(index)::value;
                using Base = schema::base_t<T, i>;
                if (i < built) std::destroy_at(static_cast<Base*>(obj));
            });
        }

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_nth(void* raw, Alloc alloc) {
            using member = schema::member_t<T, N>;
            T& obj = *reinterpret_cast<T*>(raw);
            fixed_detail::default_construct(&(member::get(obj)), alloc);
        }

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_base_nth(void* raw, Alloc alloc) {
            using BaseType = schema::base_t<T, N>;
            T* obj = reinterpret_cast<T*>(raw);
            reflect_init_all_impl<BaseType>(static_cast<void*>(static_cast<BaseType*>(obj)), alloc);
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc) {
            if constexpr (future_custom_lifetime_supported<T>()) {
                future_default_construct<T>(raw, alloc);
            } else {
            std::size_t built = 0;
            try {
                for_each_reflected_subobject<T>(
                [&](auto index) {
                    reflect_init_base_nth<T, decltype(index)::value>(raw, alloc);
                    ++built;
                },
                [&](auto index) {
                    reflect_init_nth<T, decltype(index)::value>(raw, alloc);
                    ++built;
                });
            } catch (...) {
                destroy_reflected_prefix<T>(raw, built);
                throw;
            }
            }
        }

        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc) {
            if constexpr (future_custom_lifetime_supported<T>()) {
                future_default_construct<T>(raw, alloc);
            } else {
            std::memset(raw, 0, sizeof(T));  // zero ONCE at top level
            raw = static_cast<void*>(begin_fresh_ordinary_outer<T>(raw));
            reflect_init_all_impl<T>(raw, alloc);
            }
        }

        // ── Reflection Transfer (move/copy with allocator injection) ──

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all_impl(void* dst, Src&& src, Alloc alloc);

        template <typename T, std::size_t N, typename Src, typename Alloc>
        void reflect_transfer_init_nth(void* dst, Src&& src, Alloc alloc) {
            using member = schema::member_t<T, N>;
            T& d = *reinterpret_cast<T*>(dst);
            fixed_detail::transfer_construct(&(member::get(d)), member::get(src), alloc);
        }

        template <typename T, std::size_t N, typename Src, typename Alloc>
        void reflect_transfer_base_nth(void* dst, Src&& src, Alloc alloc) {
            using BaseType = schema::base_t<T, N>;
            T* dst_obj = reinterpret_cast<T*>(dst);
            BaseType* dst_base = static_cast<BaseType*>(dst_obj);
            if constexpr (std::is_lvalue_reference_v<Src&&>) {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<const BaseType&>(src), alloc);
            } else {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<BaseType&&>(std::move(src)), alloc);
            }
        }

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all_impl(void* dst, Src&& src, Alloc alloc) {
            if constexpr (future_custom_lifetime_supported<T>()) {
                future_transfer_construct<T>(dst, std::forward<Src>(src), alloc);
            } else {
            std::size_t built = 0;
            try {
                for_each_reflected_subobject<T>(
                [&](auto index) {
                    reflect_transfer_base_nth<T, decltype(index)::value>(dst, std::forward<Src>(src), alloc);
                    ++built;
                },
                [&](auto index) {
                    reflect_transfer_init_nth<T, decltype(index)::value>(dst, std::forward<Src>(src), alloc);
                    ++built;
                });
            } catch (...) {
                destroy_reflected_prefix<T>(dst, built);
                throw;
            }
            }
        }

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all(void* dst, Src&& src, Alloc alloc) {
            if constexpr (future_custom_lifetime_supported<T>()) {
                future_transfer_construct<T>(dst, std::forward<Src>(src), alloc);
            } else {
            std::memset(dst, 0, sizeof(T));  // zero ONCE at top level
            dst = static_cast<void*>(begin_fresh_ordinary_outer<T>(dst));
            reflect_transfer_init_all_impl<T>(dst, std::forward<Src>(src), alloc);
            }
        }

        // Internal wrapper: constructs T via reflection on raw storage.
        template <typename T>
        struct ReflectRoot {
            alignas(T) unsigned char storage[sizeof(T)];

            template <typename Alloc>
            ReflectRoot(Alloc alloc) {
                reflect_init_all<T>(storage, alloc);
            }

            T& ref() noexcept {
                return *std::launder(reinterpret_cast<T*>(storage));
            }
            const T& ref() const noexcept {
                return *std::launder(reinterpret_cast<const T*>(storage));
            }
        };

        template <typename T>
        T* construct_root(XBufferCore& xbuf) {
            auto alloc = allocator_binding_for(xbuf);
            if constexpr (has_wire_allocator_ctor<T>) {
                return xbuf.template construct_root_storage<T>(alloc);
            } else {
                auto* wrapper = xbuf.template construct_root_storage<ReflectRoot<T>>(alloc);
                return &wrapper->ref();
            }
        }

        template <typename T>
        T* find_root(XBufferCore& xbuf) {
            if constexpr (has_wire_allocator_ctor<T>) {
                return xbuf.template find_root_storage<T>();
            } else {
                auto* wrapper = xbuf.template find_root_storage<ReflectRoot<T>>();
                return wrapper ? &wrapper->ref() : nullptr;
            }
        }

        // Wire header of a payload of `used` bytes with the root at
        // `root_offset` and `holes` hole table entries; the writer sets
        // extent_count. Sizes fit: used_bytes <= MAX_PAYLOAD < 2 GiB.
        template <typename T>
        XWireHeaderV2 make_wire_header(std::uint64_t used, std::uint64_t root_offset,
                                       std::uint64_t holes) noexcept {
            XWireHeaderV2 header{};
            header.magic = XWIRE_MAGIC;
            header.flags = XWIRE_FLAGS;
            header.used_bytes = static_cast<std::uint32_t>(used);
            header.root_offset = static_cast<std::uint32_t>(root_offset);
            header.hole_count = static_cast<std::uint32_t>(holes);
            header.schema_hash = wire_schema_hash<T>();
            header.root_type_id = wire_root_type_id<T>();
            return header;
        }

    } // namespace detail (reflection)

    template <typename T>
    consteval std::uint64_t wire_root_type_id_v() {
        return detail::wire_root_type_id<T>();
    }

    template <typename T>
    consteval auto wire_abi_signature() {
        return detail::wire_abi_signature<T>();
    }

    // M6 (threat model T5/P3): implicit padding inside a wire type is copied
    // into the arena as whatever bytes the source object held, and from there
    // into every emitted wire. has_implicit_padding_v<T> is true when T, or any
    // record, array element or container element type reachable from it, has
    // bytes not covered by a field.
    namespace detail {
        template <typename T>
        consteval bool has_implicit_padding();

        template <typename U, std::size_t... Is>
        consteval bool record_has_implicit_padding(std::index_sequence<Is...>) {
            std::size_t covered = 0;
            bool padded = false;
            auto field = [&]<std::size_t I>() consteval {
                using member = schema::member_t<U, I>;
                using FieldType = typename member::type;
                if (member::offset != covered) padded = true;
                covered = member::offset + sizeof(FieldType);
                if (has_implicit_padding<FieldType>()) padded = true;
            };
            (field.template operator()<Is>(), ...);
            return padded || covered != sizeof(U);
        }

        template <typename T>
        consteval bool has_implicit_padding() {
            using U = std::remove_cv_t<T>;
            if constexpr (fixed_size_scalar<U> || std::is_enum_v<U> || is_fixed_string_v<U>) {
                return false;
            } else if constexpr (std::is_array_v<U>) {
                return has_implicit_padding<std::remove_extent_t<U>>();
            } else if constexpr (is_fixed_vector_v<U>) {
                // The 12-byte head is three u32 fields; elements live in the arena.
                return has_implicit_padding<typename U::value_type>();
            } else if constexpr (is_xkeyvalue_v<U>) {
                return record_has_implicit_padding<U>(std::make_index_sequence<2>{});
            } else if constexpr (std::is_class_v<U>) {
                static_assert(schema::base_count_v<U> == 0,
                    "has_implicit_padding_v does not support base classes");
                return record_has_implicit_padding<U>(
                    std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                static_assert(sizeof(U) == 0, "has_implicit_padding_v: non-admitted wire type");
            }
        }
    } // namespace detail

    template <typename T>
    inline constexpr bool has_implicit_padding_v = detail::has_implicit_padding<T>();

    // M6: the library zeroes implicit padding of every container element it
    // constructs or relocates, so neither stale arena bytes nor the caller's
    // stack (a copied temporary) reach the wire through padding. Covers
    // records, arrays and XKeyValue (map entries); padding of a record that
    // the caller assigns directly is outside the library's reach.
    namespace detail {
        // `count` runs of `bytes` padding bytes, `stride` apart from `offset`.
        // An array contributes its element's ranges once, repeated by count,
        // so a large array costs no more compile time than one element.
        struct PaddingRange { std::size_t offset; std::size_t bytes; std::size_t stride = 0; std::size_t count = 1; };

        template <typename T>
        consteval std::vector<PaddingRange> merged_padding();

        template <typename T>
        consteval void collect_padding(std::vector<PaddingRange>& out, std::size_t base);

        template <typename U, std::size_t... Is>
        consteval void collect_record_padding(std::vector<PaddingRange>& out, std::size_t base,
                                              std::index_sequence<Is...>) {
            std::size_t covered = 0;
            auto field = [&]<std::size_t I>() consteval {
                using member = schema::member_t<U, I>;
                using FieldType = typename member::type;
                constexpr std::size_t offset = member::offset;
                if (offset > covered) out.push_back({base + covered, offset - covered, 0, 1});
                collect_padding<FieldType>(out, base + offset);
                if (offset + sizeof(FieldType) > covered) covered = offset + sizeof(FieldType);
            };
            (field.template operator()<Is>(), ...);
            if (covered < sizeof(U)) out.push_back({base + covered, sizeof(U) - covered, 0, 1});
        }

        template <typename T>
        consteval void collect_padding(std::vector<PaddingRange>& out, std::size_t base) {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_array_v<U>) {
                using B = std::remove_all_extents_t<U>;
                constexpr std::size_t total = sizeof(U) / sizeof(B);
                if constexpr (std::is_class_v<B>) {
                    for (const auto& r : merged_padding<B>()) {
                        if (r.count == 1) {
                            out.push_back({base + r.offset, r.bytes, sizeof(B), total});
                        } else {
                            for (std::size_t i = 0; i < total; ++i)
                                out.push_back({base + i * sizeof(B) + r.offset, r.bytes, r.stride, r.count});
                        }
                    }
                }
            } else if constexpr (is_xkeyvalue_v<U>) {
                collect_record_padding<U>(out, base, std::make_index_sequence<2>{});
            } else if constexpr (std::is_class_v<U> && !is_fixed_string_v<U> && !is_fixed_vector_v<U>) {
                // Records with bases are left alone (layout of base subobjects
                // is not walked here).
                if constexpr (schema::base_count_v<U> == 0)
                    collect_record_padding<U>(out, base,
                        std::make_index_sequence<schema::member_count_v<U>>{});
            }
        }

        template <typename T>
        consteval std::vector<PaddingRange> merged_padding() {
            std::vector<PaddingRange> raw, merged;
            collect_padding<T>(raw, 0);
            for (const auto& r : raw) {
                if (r.count == 1 && !merged.empty() && merged.back().count == 1 &&
                    merged.back().offset + merged.back().bytes == r.offset)
                    merged.back().bytes += r.bytes;
                else
                    merged.push_back(r);
            }
            return merged;
        }

        template <typename T>
        consteval auto padding_range_array() {
            constexpr std::size_t n = merged_padding<T>().size();
            std::array<PaddingRange, n> ranges{};
            const auto v = merged_padding<T>();
            for (std::size_t i = 0; i < n; ++i) ranges[i] = v[i];
            return ranges;
        }

        template <typename T>
        inline constexpr auto padding_ranges = padding_range_array<T>();

        template <class T>
        XOFFSET_DETAIL_PRE26_INLINE void zero_implicit_padding(T* object) noexcept {
            if constexpr (!std::is_class_v<std::remove_cv_t<T>> && !std::is_array_v<std::remove_cv_t<T>>) {
                (void)object;
            } else if constexpr (padding_ranges<T>.size() == 0) {
                (void)object;
            } else {
                auto* bytes = reinterpret_cast<unsigned char*>(object);
                for (const auto& r : padding_ranges<T>)
                    for (std::size_t i = 0; i < r.count; ++i) std::memset(bytes + r.offset + i * r.stride, 0, r.bytes);
            }
        }
    } // namespace detail

    // Opt-in per type: fails compilation when T carries implicit padding.
#define XOFFSET_REQUIRE_NO_PADDING(Type) \
    static_assert(!::XOffsetDatastructure::has_implicit_padding_v<Type>, \
        "XOffset: " #Type " has implicit padding bytes, which can carry stale memory into a wire " \
        "(threat model T5/P3); add explicit fields")

    template <typename T>
    consteval std::uint64_t wire_schema_hash_v() {
        return detail::wire_schema_hash<T>();
    }

    // K7 schema pins. XOFFSET_PIN_SCHEMA(T, "<signature>") fails compilation on
    // any compiler or target whose wire layout of T differs from the pinned
    // text; the diagnostic shows the layout that compiler computed.
    // schema_pin<T>("T") returns the pin line for the current layout.
    namespace detail {
        template <std::size_t N>
        struct SchemaPinMessage {
            char text[N]{};
            std::size_t length = 0;
            constexpr void append(std::string_view part) noexcept {
                for (char c : part) text[length++] = c;
            }
            constexpr const char* data() const noexcept { return text; }
            constexpr std::size_t size() const noexcept { return length; }
        };

        inline constexpr std::string_view schema_pin_message_head =
            "XOffset schema pin mismatch (K7): this compiler and target lay the type out as \"";
        inline constexpr std::string_view schema_pin_message_tail =
            "\"; update the pin only for an intended schema change";

        // Static storage, so a constexpr string_view may refer to it.
        template <typename T>
        inline constexpr auto schema_pin_signature = wire_abi_signature<T>();

        template <typename T>
        consteval bool schema_pin_matches(std::string_view pinned) {
            return std::string_view(schema_pin_signature<T>) == pinned;
        }

        template <typename T>
        consteval auto schema_pin_message() {
            constexpr std::string_view actual(schema_pin_signature<T>);
            SchemaPinMessage<schema_pin_message_head.size() + actual.size() +
                             schema_pin_message_tail.size()> message;
            message.append(schema_pin_message_head);
            message.append(actual);
            message.append(schema_pin_message_tail);
            return message;
        }
    } // namespace detail

    template <typename T>
    std::string schema_pin(std::string_view type_name) {
        static_assert(is_v1_wire_admitted_v<T>,
            "schema_pin<T>() requires an admitted fixed-schema wire type.");
        constexpr auto signature = wire_abi_signature<T>();
        std::string line = "XOFFSET_PIN_SCHEMA(";
        line.append(type_name);
        line += ", \"";
        for (char c : std::string_view(signature)) {
            if (c == '"' || c == '\\') line += '\\';
            line += c;
        }
        line += "\");";
        return line;
    }

    // XHandle<T> — Epoch-cached handle. O(1) deref when address is stable.
    // ========================================================================
    template <typename T>
    class XHandle {
    public:
        XHandle() noexcept : buffer_(nullptr) {}

        using resolver_type = T* (*)(XBufferCore&);

        explicit XHandle(XBufferCore& buf, resolver_type resolver = nullptr) noexcept
            : buffer_(&buf), cached_ptr_(nullptr), cached_epoch_(0), resolver_(resolver)
        {
            resolve();
        }

        T* operator->() const {
            return resolve();
        }

        T& operator*() const {
            return *resolve();
        }

        T* get() const {
            return resolve();
        }

        explicit operator bool() const {
            return resolve() != nullptr;
        }

    private:
        T* resolve() const {
            if (!buffer_) return nullptr;
            uint64_t current_epoch = buffer_->epoch();
            if (cached_ptr_ && cached_epoch_ == current_epoch) return cached_ptr_;
            cached_ptr_ = resolver_ ? resolver_(*buffer_)
                : (buffer_->root_type_known() && !buffer_->template root_type_matches<T>()
                    ? nullptr : detail::find_root<T>(*buffer_));
            cached_epoch_ = current_epoch;
            return cached_ptr_;
        }

        XBufferCore* buffer_;
        mutable T* cached_ptr_ = nullptr;
        mutable uint64_t cached_epoch_ = 0;
        resolver_type resolver_ = nullptr;
    };

    // XBuffer — Single root object per buffer (make/root/has_root).
    // ========================================================================
    namespace detail {
        // Internal friend bridge; no Core/container/wire field is added.
        struct VerifiedPathsAccess {
            using Identity = XBufferCore::OwnerRecordPtr;
            static Identity identity(XBufferCore& source) noexcept { return source.owner_record_; }
            static XBufferCore& owner(const Identity& value) { return XBufferCore::require_record_owner(value); }
            template<class T> static XWireHeaderV2 header(XBufferCore&, bool);
            template<class T> static void save_into(XBufferCore&, std::string&, XSaveOptions);
            template<class T> static void save_compact_into(XBufferCore&, std::string&, XCompactOptions);
            template<class T, class... M> static void save_composed_into(XBufferCore&, std::string&, XCompactOptions,
                                                                         const XAssign<M>&...);
            static void adopt_storage(XBufferCore&, VirtualMemoryStorage&, XGrowPolicy) noexcept;
            static void detach_host_owner(XBufferCore& core) noexcept { core.buffer_.set_host_owner(nullptr); }
            static void attach_owner(XBufferCore&);
            static void rollback_storage(XBufferCore&, VirtualMemoryStorage&) noexcept;
            template<class T> static void publish_received(XBufferCore&) noexcept;
        };
    }

    // XBuffer::begin_verified_reload / VerifiedReloadToken are available.
#define XOFFSET_VERIFIED_RELOAD_API 1
    template <typename T> class VerifiedReloadToken;

    class XBuffer : public XBufferCore {
    public:
        XBuffer() = default;

        explicit XBuffer(std::size_t size)
            : XBufferCore(size) {}

        XBuffer(const char* data, std::size_t size)
            : XBufferCore(data, size)
        {
            typed_root_access_allowed_ = false;
        }

        XBuffer(const char* data, std::size_t size, std::size_t max_reserved)
            : XBufferCore(data, size, max_reserved)
        {
            typed_root_access_allowed_ = false;
        }

        XBuffer(const char* data, std::size_t size, MaximumCapacity cap)
            : XBufferCore(data, size, cap) {
            typed_root_access_allowed_ = false;
        }

        XBuffer(WirePayload wire, std::size_t size, MaximumCapacity cap)
            : XBufferCore(wire, size, cap) {
            typed_root_access_allowed_ = false;
        }

        /// Legacy minimum reservation hint. Use maximum_capacity() for a hard limit.
        struct MaxCapacity { std::size_t value; };
        static MaxCapacity max_capacity(std::size_t bytes) { return {bytes}; }

        XBuffer(std::size_t size, MaxCapacity cap)
            : XBufferCore(size, cap.value) {}

        XBuffer(std::size_t size, MaximumCapacity cap)
            : XBufferCore(size, cap) {}

        void swap(XBuffer& other) noexcept {
            if (this == &other) return;
            XBufferCore::swap(other);
            reload_scratch_.swap(other.reload_scratch_);
            std::swap(direct_reload_open_, other.direct_reload_open_);
        }

        friend void swap(XBuffer& a, XBuffer& b) noexcept { a.swap(b); }

        template <typename T>
        static XBuffer create(std::size_t reserve_bytes = 4096) {
            XBuffer xbuf(reserve_bytes);
            (void)xbuf.template make<T>();
            return xbuf;
        }

        template <typename T>
        static XBuffer create(std::size_t initial_bytes, MaximumCapacity cap) {
            XBuffer xbuf(initial_bytes, cap);
            (void)xbuf.template make<T>();
            return xbuf;
        }

        /// Create the root object. Growth/shrink/save retain its address;
        /// replacing this buffer's storage or destroying it invalidates the pointer.
        template<typename T>
        T* make() {
            static_assert(is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe. "
                "Allowed: primitives, XString, XVector<T>, XMap<K,V>, XSet<T>, "
                "or structs composed of these.");
            if (!arena()) throw XException("make<T>(): buffer has no arena");
            if (arena()->root_offset != 0) {
                throw XException(
                    "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object.");
            }
            auto* result = detail::construct_root<T>(*this);
            typed_root_access_allowed_ = true;
            this->template bind_root_type<T>();
            bind_schema_if_available<T>();
            return result;
        }
        
        template<typename T>
        T& root() {
            require_typed_root_access(
                "root<T>(): typed root access requires local construction or load_verified<T>(); "
                "use unsafe_root<T>() only for trusted raw payloads");
            require_schema_match<T>("root<T>(): schema mismatch for requested root type");
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                throw XException(
                    "root<T>(): no root object in buffer. "
                    "Call make<T>() first or check with has_root<T>().");
            }
            return *ptr;
        }

        template<typename T>
        bool has_root() {
            if (schema_bound_ && !matches_schema<T>()) return false;
            return detail::find_root<T>(*this) != nullptr;
        }

        template<typename T>
        T& unsafe_root() {
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                throw XException(
                    "unsafe_root<T>(): no root object in buffer");
            }
            return *ptr;
        }

        template<typename T>
        bool unsafe_has_root() {
            return detail::find_root<T>(*this) != nullptr;
        }

        template<typename T>
        XHandle<T> make_handle() {
            make<T>();
            return handle<T>();
        }

        template<typename T>
        XHandle<T> handle() {
            require_typed_root_access(
                "handle<T>(): typed handle access requires local construction or load_verified<T>(); "
                "use unsafe_handle<T>() only for trusted raw payloads");
            require_schema_match<T>("handle<T>(): schema mismatch for requested root type");
            return XHandle<T>(*this, [](XBufferCore& core) -> T* {
                auto& buffer = static_cast<XBuffer&>(core);
                if (!buffer.typed_root_access_allowed_ || !buffer.template root_type_matches<T>())
                    return nullptr;
                if (buffer.schema_bound_ && !buffer.template matches_schema<T>()) return nullptr;
                return detail::find_root<T>(buffer);
            });
        }

        template<typename T>
        XHandle<T> unsafe_handle() {
            return XHandle<T>(*this);
        }

        template<typename T>
        XArenaAllocator<T> allocator() {
            static_assert(is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe.");
            return XArenaAllocator<T>(this->arena());
        }

        template<typename T>
        XArenaAllocator<T> arena_allocator() {
            return allocator<T>();
        }

        template <typename T>
        bool matches_schema() const {
            if (!schema_bound_) return false;
            if constexpr (!has_schema_name_v<T>) {
                return false;
            } else {
                return bound_root_type_id_ == detail::wire_root_type_id<T>() &&
                    bound_schema_hash_ == detail::wire_schema_hash<T>();
            }
        }

        std::span<const std::byte> bytes() const noexcept {
            auto* base = reinterpret_cast<const std::byte*>(this->get_address());
            return std::span<const std::byte>(base, this->segment_size());
        }

        // Serialize with normalized transport semantics: preserve the live object graph,
        // but compact away spare tail capacity before emitting bytes.
        std::string save() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return exact_size == 0 ? std::string{} : std::string(base, exact_size);
        }

        template <typename T>
        void save_verified_into(std::string& out, XSaveOptions options = {}) {
            detail::VerifiedPathsAccess::save_into<T>(*this, out, options);
        }

        // Compacted wire written by direct traversal: preorder blocks, strings
        // of at most 10 bytes inline, no holes, and capacity == size except
        // for the bounded slack of XCompactOptions (slack_div = 0: the
        // canonical wire of XCompactor, the smallest one). Same checks as
        // save_verified; the source is not changed. A valid V2 wire: every
        // receiver accepts it.
        template <typename T>
        void save_compact_into(std::string& out, XCompactOptions options = {}) {
            require_typed_root_access("save_compact<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_compact<T>(): schema mismatch for requested root type");
            if (!detail::find_root<T>(*this))
                throw XException("save_compact<T>(): no root object of requested type");
            detail::VerifiedPathsAccess::save_compact_into<T>(*this, out, options);
        }

        // Composes a wire from parts: writes the wire save_compact_into would
        // write after `*a.target = *a.source` for every assignment (all
        // sources read as they were), without changing this buffer or any
        // source. Targets are distinct, non-overlapping member subobjects of
        // this buffer's root (not inside a container); their implicit
        // padding is written as zero. Throws XException on a bad target or
        // past the wire limit; out is then empty.
        template <typename T, typename A0, typename... A>
        void save_compact_into(std::string& out, XCompactOptions options, const XAssign<A0>& first,
                               const XAssign<A>&... rest) {
            try {
                require_typed_root_access("save_compact<T>(): unverified payload cannot be relabeled as verified");
                require_schema_match<T>("save_compact<T>(): schema mismatch for requested root type");
                if (!detail::find_root<T>(*this))
                    throw XException("save_compact<T>(): no root object of requested type");
            } catch (...) {
                out.clear();
                throw;
            }
            detail::VerifiedPathsAccess::save_composed_into<T>(*this, out, options, first, rest...);
        }

        template <typename T>
        std::string save_compact(XCompactOptions options = {}) {
            std::string out;
            save_compact_into<T>(out, options);
            return out;
        }

        // V2: if a host allocation failure (or an unverified load) left the
        // free table incomplete, recompute it exactly from the live graph.
        template <typename T>
        void ensure_hole_table(const T& root) {
            if (this->free_table_complete()) return;
            std::vector<WireHole> holes;
            detail::verify_structural_wire_graph(root, *this, std::pmr::new_delete_resource(),
                                                 nullptr, 0, false, &holes);
            this->replace_free_blocks(holes);
        }

        template <typename T>
        XWireHeaderV2 wire_header_v2(const T& root) const {
            return detail::make_wire_header<T>(this->segment_size(),
                reinterpret_cast<std::uintptr_t>(&root) - reinterpret_cast<std::uintptr_t>(this->get_address()),
                this->free_block_count());
        }

        template <typename T>
        std::string save_verified(XSaveOptions options = {}) {
            static_assert(is_v1_wire_admitted_v<T>,
                "save_verified<T>() requires an admitted fixed-schema wire type.");
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
            static_assert(!has_implicit_padding_v<T>,
                "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
            require_typed_root_access("save_verified<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_verified<T>(): schema mismatch for requested root type");
            this->shrink_to_fit();
            auto* root_ptr = detail::find_root<T>(*this);
            if (!root_ptr) {
                throw XException(
                    "save_verified<T>(): no root object of requested type");
            }
            ensure_hole_table(*root_ptr);
            const char* base = static_cast<const char*>(this->get_address());
            const auto header = wire_header_v2(*root_ptr);
            detail::WireGaps gaps;
            if (options.emission == XWireEmission::free_runs) gaps = detail::wire_free_runs(*this, header.used_bytes);
            const auto output_size = options.emission == XWireEmission::zero_runs
                ? detail::wire_dense_size(header, "save_verified<T>()")
                : detail::wire_free_run_size(header, gaps, "save_verified<T>()");
            std::string out;
            if (output_size > out.max_size())
                throw std::length_error("save_verified<T>(): output size exceeds string limit");
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {
                return detail::write_wire(output, header, base, *this, options.emission, gaps);
            });
#else
            out.resize(output_size);
            out.resize(detail::write_wire(out.data(), header, base, *this, options.emission, gaps));
#endif
            return out;
        }

        static XBuffer load_unverified(std::span<const std::byte> data,
                                       std::optional<MaximumCapacity> cap = std::nullopt) {
            XBuffer xbuf = cap
                ? XBuffer(reinterpret_cast<const char*>(data.data()), data.size_bytes(), *cap)
                : XBuffer(reinterpret_cast<const char*>(data.data()), data.size_bytes());
            xbuf.typed_root_access_allowed_ = false;
            xbuf.schema_bound_ = false;
            xbuf.bound_root_type_id_ = 0;
            xbuf.bound_schema_hash_ = 0;
            return xbuf;
        }

        static XBuffer load_unverified(const std::string& data,
                                       std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_unverified(std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(data.data()),
                data.size()), cap);
        }

        static XBuffer load(const std::string& data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_unverified(data, cap);
        }

        static XBuffer load(std::span<const std::byte> data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_unverified(data, cap);
        }

        template <typename T>
        static XBuffer load(const std::string& data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_verified<T>(data, cap);
        }

        template <typename T>
        static XBuffer load(std::span<const std::byte> data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_verified<T>(data, cap);
        }

        template <typename T>
        static XBuffer load_verified(std::span<const std::byte> data,
                                     std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_verified_with_resource<T>(data, cap, std::pmr::new_delete_resource());
        }

        // Explicit host-only upstream. A fresh context and fresh membership
        // bitmaps are prepared for every invocation; payloads above 32 KiB take
        // one block from the upstream for them. The upstream must outlive this
        // synchronous call and be non-null.
        template <typename T>
        static XBuffer load_verified_with_resource(std::span<const std::byte> data,
                                                  std::optional<MaximumCapacity> cap,
                                                  std::pmr::memory_resource* upstream) {
            constexpr const char* who = "load_verified<T>()";
            if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
            XWireHeaderV2 header{};
            const auto layout = admit_wire_header<T>(data, header, who);
            const auto used = static_cast<std::size_t>(header.used_bytes);
            // M1: without an explicit limit a wire may claim at most the
            // default maximum; a sparse wire of a few hundred bytes could
            // otherwise make the receiver reserve up to MAX_PAYLOAD.
            const auto capacity = cap ? *cap : MaximumCapacity{DEFAULT_MAXIMUM_CAPACITY};
            detail::require_wire_within_capacity(used, capacity.value, cap.has_value(),
                                                 "load_verified<T>()");
            XBuffer xbuf(WirePayload{header.root_offset, layout.stored, layout.extents, layout.extent_count},
                         used, capacity);
            xbuf.finish_verified_load<T>(header, layout, upstream, who);
            return xbuf;
        }

        /// Verified load INTO this buffer, reusing its mapping, maximum
        /// capacity and owner registration (no new reservation, pool round
        /// trip or registry entry). Admission and verification are exactly
        /// those of load_verified<T>(wire, this buffer's MaximumCapacity).
        /// Every root reference, element pointer and borrowed span into the
        /// previous content is invalidated; XHandles re-resolve. On any
        /// failure the exception propagates and the buffer is left empty and
        /// without typed root access (root<T>() throws) until a later
        /// successful reload_verified or reset<T>(). Committed pages are kept
        /// (see reclaim_tail_pages()).
        /// Validation scratch for payloads above 32 KiB is retained by this
        /// buffer (released with it or by release_reload_scratch()), so a
        /// reload loop does not allocate host memory per message; smaller
        /// payloads need none.
        template <typename T>
        void reload_verified(std::span<const std::byte> data) {
            reload_into<T>(data, nullptr);
        }

        template <typename T>
        void reload_verified(const std::string& data) {
            reload_verified<T>(std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(data.data()), data.size()));
        }

        template <typename T>
        void reload_verified_with_resource(std::span<const std::byte> data,
                                           std::pmr::memory_resource* upstream) {
            if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
            reload_into<T>(data, upstream);
        }

        /// Returns the retained reload scratch words to the host allocator.
        void release_reload_scratch() noexcept {
            // An open direct reload keeps its tables until it finishes.
            if (reload_scratch_) reload_scratch_->release(!direct_reload_open_);
        }

    private:
        // upstream == nullptr: this buffer's own reload scratch.
        template <typename T>
        void reload_into(std::span<const std::byte> data, std::pmr::memory_resource* upstream) {
            constexpr const char* who = "reload_verified<T>()";
            XWireHeaderV2 header;
            if (admit_small_wire<T>(data, header)) [[likely]] {
                // Payload <= small_reload_max: the inline bitmaps suffice, so
                // neither upstream nor the retained scratch is touched.
                try {
                    const char* stored = reinterpret_cast<const char*>(data.data()) + sizeof(XWireHeaderV2);
                    const auto used = static_cast<std::size_t>(header.used_bytes);
                    reinit_from_dense_wire_in_place(stored, header.root_offset, used);
                    finish_verified_load<T>(header, stored + (used - XWIRE_PAYLOAD_BEGIN),
                                            static_cast<std::size_t>(header.hole_count),
                                            upstream ? upstream : std::pmr::new_delete_resource(), who, nullptr);
                } catch (...) {
                    reset_arena(0);
                    throw;
                }
                return;
            }
            try {
                detail::WireValidationScratch* retained = nullptr;
                if (!upstream) {
                    if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
                    retained = reload_scratch_.get();
                    upstream = std::pmr::new_delete_resource();
                }
                header = XWireHeaderV2{};
                const auto layout = admit_wire_header<T>(data, header, who);
                reinit_from_wire({header.root_offset, layout.stored, layout.extents, layout.extent_count},
                                 header.used_bytes);
                finish_verified_load<T>(header, layout, upstream, who, retained);
            } catch (...) {
                reset_arena(0);
                throw;
            }
        }

        detail::WireValidationScratch& retained_reload_scratch() {
            if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
            return *reload_scratch_;
        }

        std::unique_ptr<detail::WireValidationScratch> reload_scratch_;
        bool direct_reload_open_ = false;
        template <typename> friend class VerifiedReloadToken;

    public:
        /// Direct receive into this buffer: reload_verified<T>() without the
        /// library copy. The transport writes the wire's sections into the
        /// token's spans (header; extent table; the payload bytes, densely or
        /// each extent at its payload offset - XWIRE_PAYLOAD_BEGIN with gaps
        /// untouched; hole table), calls complete() with the byte counts it
        /// wrote, then finish().
        /// `payload_bytes`, `holes` and `extents` are the header's used_bytes,
        /// hole_count and extent_count. Admission, verification, rejection
        /// and the result are exactly those of reload_verified<T>() on the
        /// same wire; a length that disagrees with the header, an incomplete
        /// transfer, or a token dropped before finish() rejects. From begin
        /// until finish() succeeds the buffer has no typed root access, and
        /// after any failure it is left empty as after a failed reload.
        /// The buffer must not be used, moved or destroyed while the token is
        /// open; one token per buffer at a time. No host allocation per
        /// message once the retained scratch has grown (sparse wires excepted).
        template <typename T>
        VerifiedReloadToken<T> begin_verified_reload(std::size_t payload_bytes,
                                                     std::size_t holes = 0,
                                                     std::size_t extents = 0) {
            static_assert(is_v1_wire_admitted_v<T>,
                "begin_verified_reload<T>() requires an admitted fixed-schema wire type.");
            if (direct_reload_open_)
                throw XException("begin_verified_reload<T>(): a direct reload is already open");
            try {
                // Holes and extents are disjoint, non-empty 8-byte multiples
                // inside the payload, so larger counts never verify; they
                // are refused before the tables are sized.
                if (holes > payload_bytes / 8u || extents > payload_bytes / 8u)
                    throw XException("reload_verified<T>(): used size mismatch");
                const auto clean_from = reinit_for_overwrite(payload_bytes);
                auto* tables = retained_reload_scratch().tables(
                    sizeof(XWireHeaderV2) + extents * 8u + holes * sizeof(WireHole));
                direct_reload_open_ = true;
                return VerifiedReloadToken<T>(*this, tables, payload_bytes, holes, extents, clean_from);
            } catch (...) {
                reset_arena(0);
                throw;
            }
        }

        /// Empties this buffer and constructs a fresh root of type T in it,
        /// reusing the mapping and owner registration; equivalent to
        /// `*this = create<T>(get_size(), this buffer's MaximumCapacity)`.
        /// Invalidates references into the previous content like
        /// reload_verified.
        template <typename T>
        T& reset() {
            reset_arena(this->get_size());
            if (!this->arena()) throw XException("reset<T>(): buffer has no arena");
            return *this->template make<T>();
        }

        /// An independent copy of this buffer in the same process: one copy of
        /// the used payload into a new owner with this buffer's size, maximum
        /// capacity, growth policy and pool. The exact free table, typed root
        /// access and the root/schema binding come along, so the copy behaves
        /// like this buffer would (same allocations, same save_verified bytes);
        /// nothing goes through a wire and nothing is verified. Throws
        /// XException when this buffer has no arena and XBadAlloc when the new
        /// owner cannot be created; this buffer is never modified.
        XBuffer clone() const {
            const auto* sm = this->arena();
            if (!sm) throw XException("clone(): buffer has no arena");
            XBuffer out(reinterpret_cast<const char*>(sm), static_cast<std::size_t>(sm->used_end),
                        this->clone_capacity());
            out.copy_clone_state(*this);
            return out;
        }

        /// The same copy into an existing buffer, reusing its mapping, address,
        /// owner registration, maximum capacity and growth policy (like
        /// reload_verified); dst's size becomes this buffer's size, at most
        /// dst's maximum capacity. Every reference into dst's previous content is
        /// invalidated and dst.epoch() advances. Throws XException when dst is
        /// this buffer (nothing is changed) and XBadAlloc when this buffer's
        /// used bytes exceed dst's maximum capacity; on any failure other than
        /// the self copy dst is left an empty arena without a root, as after a
        /// failed reload_verified.
        void clone_into(XBuffer& dst) const {
            dst.clone_state_from(*this);
        }

        template <typename T>
        static XBuffer load_verified(const std::string& data,
                                     std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_verified<T>(std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(data.data()),
                data.size()), cap);
        }

        MemoryStats stats() {
            return memory_stats(*this);
        }

    private:
        // Small dense reload (reload_verified): a payload of at most
        // small_reload_max bytes and no extent table. One pass over the
        // header checks exactly what admit_wire_header, parse_wire_layout and
        // the reinit admission check; any other wire, valid or not, takes the
        // general path, which reports the same first error.
        static constexpr std::size_t small_reload_max = detail::WireGranuleIndex::inline_extent;
        static constexpr std::size_t wire_header_words = sizeof(XWireHeaderV2) / 8u;

        // The header fields that do not depend on the payload, as words;
        // used_bytes, root_offset and hole_count are 0, extent_count is 0 (dense).
        template <typename T>
        static constexpr std::array<std::uint64_t, wire_header_words> small_wire_image() noexcept {
            XWireHeaderV2 h{};
            h.magic = XWIRE_MAGIC;
            h.flags = XWIRE_FLAGS;
            h.schema_hash = detail::wire_schema_hash<T>();
            h.root_type_id = detail::wire_root_type_id<T>();
            return std::bit_cast<std::array<std::uint64_t, wire_header_words>>(h);
        }

        template <typename T>
        XOFFSET_DETAIL_MOVE_INLINE static bool admit_small_wire(std::span<const std::byte> data,
                                                                XWireHeaderV2& header) noexcept {
            static_assert(sizeof(XWireHeaderV2) == 40 && offsetof(XWireHeaderV2, used_bytes) == 8 &&
                          offsetof(XWireHeaderV2, hole_count) == 16 && offsetof(XWireHeaderV2, schema_hash) == 24);
            if (!detail::small_reload_fast_path || data.size_bytes() < sizeof(XWireHeaderV2)) return false;
            std::memcpy(&header, data.data(), sizeof(header));
            constexpr auto image = small_wire_image<T>();
            const auto words = std::bit_cast<std::array<std::uint64_t, wire_header_words>>(header);
            // Word 1: used_bytes, root_offset; word 2: hole_count, extent_count (0).
            const std::uint64_t diff = (words[0] ^ image[0]) | (words[2] >> 32) | (words[3] ^ image[3]) |
                                       (words[4] ^ image[4]);
            const std::uint64_t used = header.used_bytes;
            const std::uint64_t body = data.size_bytes() - sizeof(XWireHeaderV2);
            const std::uint64_t stored = used - XWIRE_PAYLOAD_BEGIN;
            return diff == 0 && used >= XWIRE_PAYLOAD_BEGIN && used <= small_reload_max && (used & 7u) == 0 &&
                header.root_offset >= XWIRE_PAYLOAD_BEGIN && header.root_offset < used && stored <= body &&
                body - stored == std::uint64_t(header.hole_count) * 8u;
        }

        // Wire admission shared by load_verified and reload_verified.
        template <typename T>
        static detail::WireLayout admit_wire_header(std::span<const std::byte> data,
                                                    XWireHeaderV2& header, const char* who) {
            static_assert(is_v1_wire_admitted_v<T>,
                "load_verified<T>() requires an admitted fixed-schema wire type.");
            header = detail::read_wire_header(data.data(), data.size_bytes(), who);
            if (header.root_type_id != detail::wire_root_type_id<T>()) {
                throw XException(std::string(who) + ": root type id mismatch");
            }
            if (header.schema_hash != detail::wire_schema_hash<T>()) {
                throw XException(std::string(who) + ": schema hash mismatch");
            }
            return detail::parse_wire_layout(
                header, reinterpret_cast<const char*>(data.data()) + sizeof(XWireHeaderV2),
                data.size_bytes() - sizeof(XWireHeaderV2), who);
        }

        // Post-copy verification shared by load_verified and reload_verified.
        template <typename T>
        void finish_verified_load(const XWireHeaderV2& header, const detail::WireLayout& layout,
                                  std::pmr::memory_resource* upstream, const char* who,
                                  detail::WireValidationScratch* retained = nullptr) {
            finish_verified_load<T>(header, layout.holes, layout.hole_count, upstream, who, retained);
        }

        template <typename T>
        void finish_verified_load(const XWireHeaderV2& header, const char* holes, std::size_t hole_count,
                                  std::pmr::memory_resource* upstream, const char* who,
                                  detail::WireValidationScratch* retained) {
            if (this->arena()->used_end != header.used_bytes) {
                throw XException(std::string(who) + ": non-normalized payload extent");
            }
            auto* root_ptr = detail::find_root<T>(*this);
            if (!root_ptr) {
                throw XException(std::string(who) + ": root not found");
            }
            auto actual_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(root_ptr) -
                reinterpret_cast<std::uintptr_t>(this->get_address()));
            if (actual_offset != header.root_offset) {
                throw XException(std::string(who) + ": root offset mismatch");
            }
            detail::verify_structural_wire_graph(*root_ptr, *this, upstream, holes, hole_count,
                                                 true, nullptr, retained);
            this->load_free_blocks(holes, hole_count);
            typed_root_access_allowed_ = true;
            bind_schema<T>();
        }

        void require_typed_root_access(const char* message) const {
            if (!typed_root_access_allowed_) {
                throw XException(message);
            }
        }

        template <typename T>
        void bind_schema_if_available() noexcept {
            if constexpr (has_schema_name_v<T>) {
                bind_schema<T>();
            }
        }

        template <typename T>
        void bind_schema() noexcept {
            this->template bind_root_type<T>();
            if constexpr (has_schema_name_v<T>) {
                schema_bound_ = true;
                bound_root_type_id_ = detail::wire_root_type_id<T>();
                bound_schema_hash_ = detail::wire_schema_hash<T>();
            }
        }

        template <typename T>
        void require_schema_match(const char* message) const {
            if (schema_bound_ && !matches_schema<T>()) {
                throw XException(message);
            }
        }

    };

    // Open direct reload of one XBuffer (XBuffer::begin_verified_reload).
    template <typename T>
    class VerifiedReloadToken {
    public:
        VerifiedReloadToken(VerifiedReloadToken&& other) noexcept
            : buffer_(std::exchange(other.buffer_, nullptr)), tables_(other.tables_),
              payload_(other.payload_), holes_(other.holes_), extents_(other.extents_),
              clean_from_(other.clean_from_), received_(other.received_), complete_(other.complete_) {}
        VerifiedReloadToken& operator=(VerifiedReloadToken&&) = delete;
        VerifiedReloadToken(const VerifiedReloadToken&) = delete;
        VerifiedReloadToken& operator=(const VerifiedReloadToken&) = delete;
        ~VerifiedReloadToken() { if (buffer_) abort(); }

        explicit operator bool() const noexcept { return buffer_ != nullptr; }
        std::span<std::byte> header_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_, sizeof(XWireHeaderV2)) : std::span<std::byte>{};
        }
        std::span<std::byte> extent_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_ + sizeof(XWireHeaderV2), extents_ * 8u)
                           : std::span<std::byte>{};
        }
        // The payload bytes of the wire: payload [48, used_bytes).
        std::span<std::byte> payload_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_->data()) + XWIRE_PAYLOAD_BEGIN,
                                                  payload_ - XWIRE_PAYLOAD_BEGIN)
                           : std::span<std::byte>{};
        }
        std::span<std::byte> hole_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_ + sizeof(XWireHeaderV2) + extents_ * 8u,
                                                  holes_ * sizeof(XBufferCore::WireHole))
                           : std::span<std::byte>{};
        }
        // payload_bytes counts the payload bytes including sparse gaps
        // (used_bytes - 48).
        void complete(std::size_t header_bytes, std::size_t payload_bytes,
                      std::size_t hole_bytes = 0, std::size_t extent_bytes = 0) noexcept {
            received_ = {header_bytes, payload_bytes, hole_bytes, extent_bytes};
            complete_ = true;
        }

        /// Verifies and publishes the received wire; consumes the token.
        void finish() {
            constexpr const char* who = "reload_verified<T>()";
            auto* buffer = std::exchange(buffer_, nullptr);
            if (!buffer) throw XException("verified reload: token consumed");
            struct Close { XBuffer& b; ~Close() { b.direct_reload_open_ = false; } } close{*buffer};
            try {
                if (!complete_ || received_[0] != sizeof(XWireHeaderV2) ||
                    received_[1] != payload_ - XWIRE_PAYLOAD_BEGIN ||
                    received_[2] != holes_ * sizeof(XBufferCore::WireHole) || received_[3] != extents_ * 8u)
                    throw XException("verified reload: incomplete input");
                // admit_wire_header and parse_wire_layout, with the section
                // lengths fixed by begin instead of read from one wire.
                const auto header = detail::read_wire_header(tables_, sizeof(XWireHeaderV2), who);
                if (header.root_type_id != detail::wire_root_type_id<T>())
                    throw XException(std::string(who) + ": root type id mismatch");
                if (header.schema_hash != detail::wire_schema_hash<T>())
                    throw XException(std::string(who) + ": schema hash mismatch");
                detail::check_wire_extent(header, who);
                if (header.used_bytes != payload_ || header.extent_count != extents_ ||
                    header.hole_count != holes_)
                    throw XException(std::string(who) + ": used size mismatch");
                detail::WireLayout layout;
                layout.extent_count = extents_;
                if (extents_) layout.extents = reinterpret_cast<const char*>(tables_ + sizeof(XWireHeaderV2));
                (void)detail::parse_extent_table(layout.extents, extents_, header.used_bytes, who, layout.gaps);
                layout.holes = reinterpret_cast<const char*>(tables_ + sizeof(XWireHeaderV2) + extents_ * 8u);
                layout.hole_count = holes_;
                // Gaps read as zero, as in assign_extents: clear what an
                // earlier content may have left below clean_from_.
                auto* base = buffer->data();
                for (const auto& gap : layout.gaps) {
                    const auto end = std::min<std::size_t>(std::size_t(gap.offset) + gap.bytes, clean_from_);
                    if (gap.offset < end) std::memset(base + gap.offset, 0, end - gap.offset);
                }
                XBufferCore::write_payload_header(base, header.root_offset, header.used_bytes);
                buffer->check_reinit_payload();
                buffer->template finish_verified_load<T>(header, layout, std::pmr::new_delete_resource(), who,
                                                         &buffer->retained_reload_scratch());
            } catch (...) {
                buffer->reset_arena(0);
                throw;
            }
        }

    private:
        friend class XBuffer;
        VerifiedReloadToken(XBuffer& buffer, std::byte* tables, std::size_t payload, std::size_t holes,
                            std::size_t extents, std::size_t clean_from) noexcept
            : buffer_(&buffer), tables_(tables), payload_(payload), holes_(holes), extents_(extents),
              clean_from_(clean_from) {}
        void abort() noexcept {
            auto* buffer = std::exchange(buffer_, nullptr);
            buffer->reset_arena(0);
            buffer->direct_reload_open_ = false;
        }

        XBuffer* buffer_;
        std::byte* tables_;
        std::size_t payload_, holes_, extents_, clean_from_;
        std::array<std::size_t, 4> received_{};
        bool complete_ = false;
    };

    // ========================================================================
    // TypedXBuffer<T> — Type-bound buffer with compile-time root type
    //
    // Wraps XBuffer and binds it to a specific root type T at compile time.
    // Eliminates the need to repeat <T> on every make/root/handle call.
    //
    // Usage:
    //   TypedXBuffer<GameData> buf(4096);
    //   auto* game = buf.make();         // no <GameData> needed
    //   auto& g = buf.root();            // no <GameData> needed
    //   auto bytes = buf.save();          // standard save
    //   auto loaded = TypedXBuffer<GameData>::load(bytes);
    // ========================================================================
    template<typename T>
    class TypedXBuffer : public XBuffer {
    public:
        using XBuffer::XBuffer;

        static TypedXBuffer create(std::size_t reserve_bytes = 4096) {
            TypedXBuffer xbuf(reserve_bytes);
            (void)xbuf.make();
            return xbuf;
        }

        static TypedXBuffer create(std::size_t initial_bytes, MaximumCapacity cap) {
            TypedXBuffer xbuf(initial_bytes, cap);
            (void)xbuf.make();
            return xbuf;
        }

        /// Creates the single root object of type T.
        T* make() { return XBuffer::make<T>(); }

        /// Returns a reference to the root object.
        T& root() { return XBuffer::root<T>(); }

        /// Returns true if a root object exists.
        bool has_root() { return XBuffer::has_root<T>(); }

        bool matches_schema() const { return XBuffer::matches_schema<T>(); }

        /// Creates root and returns an epoch-cached handle.
        XHandle<T> make_handle() { return XBuffer::make_handle<T>(); }

        /// Returns an epoch-cached handle to the existing root.
        XHandle<T> handle() { return XBuffer::handle<T>(); }

        /// Load from serialized data and return a typed buffer.
        static TypedXBuffer load(const std::string& data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap));
        }

        static TypedXBuffer load(std::span<const std::byte> data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap));
        }

        static TypedXBuffer load_unverified(const std::string& data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_unverified(data, cap));
        }

        static TypedXBuffer load_unverified(std::span<const std::byte> data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_unverified(data, cap));
        }

        std::string save_verified(XSaveOptions options = {}) { return XBuffer::save_verified<T>(options); }
        void reload_verified(const std::string& data) { XBuffer::reload_verified<T>(data); }
        void reload_verified(std::span<const std::byte> data) { XBuffer::reload_verified<T>(data); }
        T& reset() { return XBuffer::reset<T>(); }

        TypedXBuffer clone() const { return TypedXBuffer(XBuffer::clone()); }
        void clone_into(TypedXBuffer& dst) const { XBuffer::clone_into(dst); }

        void save_verified_into(std::string& out, XSaveOptions options = {}) {
            XBuffer::save_verified_into<T>(out, options);
        }
        std::string save_compact(XCompactOptions options = {}) { return XBuffer::save_compact<T>(options); }
        void save_compact_into(std::string& out, XCompactOptions options = {}) { XBuffer::save_compact_into<T>(out, options); }
        template <typename A0, typename... A>
        void save_compact_into(std::string& out, XCompactOptions options, const XAssign<A0>& first,
                               const XAssign<A>&... rest) {
            XBuffer::save_compact_into<T>(out, options, first, rest...);
        }

        static TypedXBuffer load_verified(const std::string& data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap));
        }

        static TypedXBuffer load_verified(std::span<const std::byte> data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap));
        }

        /// Construct from an existing XBuffer (e.g., after compaction).
        TypedXBuffer(XBuffer&& other) noexcept
            : XBuffer(std::move(other)) {}
    };

    // ================================================================
    // XCompactor — Memory compaction via C++26 reflection.
    // ================================================================
    class XCompactor {
    public:
        enum class MigrateStrategy {
            Bitwise,          // direct assignment (primitives, enums, POD)
            AllocatorAware,   // reconstruct with new allocator (XString-like)
            Container,        // iterate elements, recurse (XVector/XSet/XMap)
            Composite,        // reflect members, recurse (user structs)
            NotRegistered     // use built-in auto-detection
        };

        template<typename T>
        struct migrate_as { static constexpr MigrateStrategy value = MigrateStrategy::NotRegistered; };

        // Compact root object into a new tightly-packed buffer.
        // Progressive allocation: tries 2x, 2.5x, 3x, 4x multipliers.
        template<typename T>
        static XBuffer compact(XBufferCore& old_xbuf) {
            static_assert(is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe.");
            auto stats = memory_stats(old_xbuf);
            auto* old_obj = detail::find_root<T>(old_xbuf);

            // Progressive multipliers: try smaller first, fall back to larger
            constexpr double multipliers[] = {2.0, 2.5, 3.0, 4.0};
            for (double mult : multipliers) {
                std::size_t new_size = static_cast<std::size_t>(stats.used_size * mult);
                if (new_size < 4096) new_size = 4096;

                try {
                    XBuffer new_xbuf(new_size);
                    if (!old_obj) {
                        return new_xbuf;
                    }
                    auto* new_obj = new_xbuf.template make<T>();
                    migrate_member(*old_obj, *new_obj, old_xbuf, new_xbuf);
                    new_xbuf.shrink_to_fit();
                    return new_xbuf;
                } catch (const XBadAlloc&) {
                    // Not enough headroom — try next multiplier
                    continue;
                }
            }
            // All multipliers exhausted — should not happen in practice
            throw XException(
                "XCompactor::compact failed: insufficient memory even at 4x multiplier");
        }

    private:

        // Resolve strategy: user-registered > trivial > allocator gate > composite.
        template<typename T>
        static consteval MigrateStrategy resolve_strategy() {
            using CleanT = std::remove_cv_t<T>;
            static_assert(is_byte_copy_safe_v<CleanT>,
                "XCompactor: type is not byte-copy safe and cannot be migrated.");
            if constexpr (migrate_as<CleanT>::value != MigrateStrategy::NotRegistered) {
                return migrate_as<CleanT>::value;
            } else if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return MigrateStrategy::Bitwise;
            } else {
                return MigrateStrategy::Composite;
            }
        }

        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<ElementType>();
            auto new_alloc = detail::allocator_binding_for(new_xbuf);
            if constexpr (strategy == MigrateStrategy::Bitwise) {
                return old_elem;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                return ElementType(old_elem, new_alloc);
            } else if constexpr (detail::has_wire_allocator_ctor<ElementType>) {
                ElementType new_elem(new_alloc);
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            } else {
                alignas(ElementType) unsigned char buf[sizeof(ElementType)];
                detail::reflect_init_all<ElementType>(buf, new_alloc);
                ElementType& new_elem = *std::launder(reinterpret_cast<ElementType*>(buf));
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using ElementType = typename ContainerType::value_type;

            if constexpr (std::is_trivially_copyable_v<ElementType>) {
                new_container = old_container;
                return;
            }

            if constexpr (requires { new_container.reserve(old_container.size()); }) {
                new_container.reserve(old_container.size());
            }

            if constexpr (detail::is_fixed_flat_map_v<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto [it, inserted] = new_container.try_emplace(key);
                    (void)inserted;
                    migrate_member(value, it->second, old_xbuf, new_xbuf);
                }
            } else if constexpr (detail::is_fixed_flat_set_v<ContainerType>) {
                for (const auto& elem : old_container) {
                    new_container.insert(elem);
                }
            } else if constexpr (detail::is_fixed_vector_v<ContainerType>) {
                for (const auto& elem : old_container) {
                    auto& slot = new_container.emplace_back();
                    migrate_member(elem, slot, old_xbuf, new_xbuf);
                }
            } else if constexpr (detail::MapLikeContainer<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto new_key = migrate_element(key, old_xbuf, new_xbuf);
                    auto new_value = migrate_element(value, old_xbuf, new_xbuf);
                    new_container.emplace(std::move(new_key), std::move(new_value));
                }
            } else {
                for (const auto& elem : old_container) {
                    auto migrated_elem = migrate_element(elem, old_xbuf, new_xbuf);
                    if constexpr (detail::SetLikeContainer<ContainerType>) {
                        new_container.emplace(std::move(migrated_elem));
                    } else {
                        new_container.emplace_back(std::move(migrated_elem));
                    }
                }
            }
        }
        
        template<typename MemberType>
        static void migrate_member(const MemberType& old_member, MemberType& new_member, 
                                  XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<MemberType>();
            if constexpr (std::is_array_v<MemberType>) {
                for (std::size_t i = 0; i < std::extent_v<MemberType>; ++i)
                    migrate_member(old_member[i], new_member[i], old_xbuf, new_xbuf);
            } else if constexpr (strategy == MigrateStrategy::Bitwise) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::Container) {
                migrate_container(old_member, new_member, old_xbuf, new_xbuf);
            } else {
                migrate_members(old_member, new_member, old_xbuf, new_xbuf);
            }
        }
        
        template<typename T, std::size_t N>
        static void migrate_base_at(const T& old_obj, T& new_obj,
                                    XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using BaseType = schema::base_t<T, N>;
            migrate_members(static_cast<const BaseType&>(old_obj),
                            static_cast<BaseType&>(new_obj), old_xbuf, new_xbuf);
        }

        template<typename T, std::size_t N>
        static void migrate_member_at(const T& old_obj, T& new_obj,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using member = schema::member_t<T, N>;
            using MemberType = typename member::type;
            migrate_member<MemberType>(member::get(old_obj), member::get(new_obj),
                                       old_xbuf, new_xbuf);
        }

        template<typename T>
        static void migrate_members(const T& old_obj, T& new_obj,
                                   XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            detail::for_each_reflected_subobject<T>(
                [&](auto index) {
                    migrate_base_at<T, decltype(index)::value>(
                        old_obj, new_obj, old_xbuf, new_xbuf);
                },
                [&](auto index) {
                    migrate_member_at<T, decltype(index)::value>(
                        old_obj, new_obj, old_xbuf, new_xbuf);
                });
        }
    };
}

namespace XOffsetDatastructure {

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XFixedVector<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XVector<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XFixedFlatSet<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XSet<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename K, typename V>
struct is_byte_copy_safe<XOffsetDatastructure::XFixedFlatMap<K, V>>
    : std::bool_constant<is_byte_copy_safe_v<K> && is_byte_copy_safe_v<V>> {};

template <typename K, typename V>
struct is_byte_copy_safe<XOffsetDatastructure::XMap<K, V>>
    : std::bool_constant<is_byte_copy_safe_v<K> && is_byte_copy_safe_v<V>> {};

} // namespace XOffsetDatastructure

// ============================================================================
// Registration macros: XCompactor migration strategy registration.
// Must be placed at global namespace scope.
// ============================================================================

// Namespace sentinel — detects accidental use inside a namespace block.
struct _XOffset_NS_Sentinel {};

#define XOFFSET_CHECK_GLOBAL_NAMESPACE_(macro_name)                            \
    static_assert(                                                             \
        ::std::is_same_v<::_XOffset_NS_Sentinel, _XOffset_NS_Sentinel>,       \
        macro_name " must be used at global namespace scope, "                 \
        "not inside any namespace { } block. "                                 \
        "Move the macro call outside all namespace declarations.");

#define XOFFSET_REGISTER_SCHEMA_NAME(Type, literal)                            \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_SCHEMA_NAME")            \
    template<> struct XOffsetDatastructure::schema_name<Type> {                \
        static constexpr std::string_view value = literal;                     \
    };

// K7: compile-time pin of a type's wire layout (see schema_pin<T>()). Valid at
// namespace or block scope. Use an alias for a type name containing commas.
// The diagnostic shows the computed layout where static_assert takes a
// computed message (C++26); otherwise schema_pin<T>() prints it.
#if defined(__cpp_static_assert) && __cpp_static_assert >= 202306L
#define XOFFSET_PIN_SCHEMA(Type, signature)                                    \
    static_assert(                                                             \
        ::XOffsetDatastructure::detail::schema_pin_matches<Type>(signature),   \
        ::XOffsetDatastructure::detail::schema_pin_message<Type>())
#else
#define XOFFSET_PIN_SCHEMA(Type, signature)                                    \
    static_assert(                                                             \
        ::XOffsetDatastructure::detail::schema_pin_matches<Type>(signature),   \
        "XOffset schema pin mismatch (K7): this compiler and target lay " #Type \
        " out differently; schema_pin<T>() prints the layout; update the pin " \
        "only for an intended schema change")
#endif

#define XOFFSET_REGISTER_MIGRATION_TYPE(Type, strategy)                        \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_MIGRATION_TYPE")         \
    template<> struct XOffsetDatastructure::XCompactor::migrate_as<            \
        XOffsetDatastructure::Type> {                                          \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

#define XOFFSET_REGISTER_MIGRATION_CONTAINER(Template, strategy)               \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_MIGRATION_CONTAINER")    \
    template<typename T_> struct XOffsetDatastructure::XCompactor::migrate_as< \
        XOffsetDatastructure::Template<T_>> {                                  \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

#define XOFFSET_REGISTER_MIGRATION_MAP(Template, strategy)                     \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_MIGRATION_MAP")          \
    template<typename K_, typename V_>                                          \
    struct XOffsetDatastructure::XCompactor::migrate_as<                        \
        XOffsetDatastructure::Template<K_, V_>> {                              \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

// XOFFSET_ORDINARY_RECORDS(A, B, ...): declares that each record type has
// only ordinary non-static data members: no bases, unions, bit-fields or
// [[no_unique_address]], members never overlap. It specializes
// wire_record_layout<T>::ordinary_members and StableEraseOrdinaryMembers<T>,
// which turns on the SHIFT path (XVector insert/erase/growth move records with
// one memmove/memcpy plus an inline-head rebase), stable_erase_if and raw
// receive for these types. The wire bytes are the same with or without it.
// Each type is checked at compile time: a class (not a union) without bases
// or bit-fields whose members are physically disjoint. A [[no_unique_address]]
// member that happens not to overlap passes the check, so declaring such a type
// is still undefined behavior; the library never infers the declaration.
// Use at global scope after the type definitions and before any use of the
// types in containers. Use an alias for a type name containing commas.
#define XOFFSET_ORDINARY_RECORDS(...)                                          \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_ORDINARY_RECORDS")                \
    XOFFSET_DETAIL_FOR_EACH(XOFFSET_DETAIL_ORDINARY_RECORD, __VA_ARGS__)

#define XOFFSET_DETAIL_ORDINARY_RECORD(Type)                                   \
    static_assert(::std::is_class_v<Type> && !::std::is_union_v<Type>,       \
        "XOFFSET_ORDINARY_RECORDS: " #Type " must be a class type, not a union"); \
    static_assert(::XOffsetDatastructure::detail::ordinary_record_has_no_bases<Type>(), \
        "XOFFSET_ORDINARY_RECORDS: " #Type " must not have base classes");   \
    static_assert(::XOffsetDatastructure::detail::ordinary_record_has_no_bit_fields<Type>(), \
        "XOFFSET_ORDINARY_RECORDS: " #Type " must not have bit-fields");     \
    static_assert(::XOffsetDatastructure::detail::ordinary_record_members_disjoint<Type>(), \
        "XOFFSET_ORDINARY_RECORDS: " #Type " has overlapping members "       \
        "([[no_unique_address]]?); it is not an ordinary record");           \
    XOFFSET_SCHEMA_DETAIL_ORDINARY(Type)

// The declaration part, shared with XOFFSET_RECORD.
#define XOFFSET_SCHEMA_DETAIL_ORDINARY(Type)                                   \
    template<> struct XOffsetDatastructure::wire_record_layout<Type> {        \
        static constexpr bool ordinary_members = true;                         \
    };                                                                         \
    template<> struct XOffsetDatastructure::StableEraseOrdinaryMembers<Type>  \
        : ::std::true_type {};

// Applies macro(x) to each argument (up to 256).
#define XOFFSET_DETAIL_PARENS ()
#define XOFFSET_DETAIL_EXPAND(...) XOFFSET_DETAIL_EXPAND4(XOFFSET_DETAIL_EXPAND4(XOFFSET_DETAIL_EXPAND4(XOFFSET_DETAIL_EXPAND4(__VA_ARGS__))))
#define XOFFSET_DETAIL_EXPAND4(...) XOFFSET_DETAIL_EXPAND3(XOFFSET_DETAIL_EXPAND3(XOFFSET_DETAIL_EXPAND3(XOFFSET_DETAIL_EXPAND3(__VA_ARGS__))))
#define XOFFSET_DETAIL_EXPAND3(...) XOFFSET_DETAIL_EXPAND2(XOFFSET_DETAIL_EXPAND2(XOFFSET_DETAIL_EXPAND2(XOFFSET_DETAIL_EXPAND2(__VA_ARGS__))))
#define XOFFSET_DETAIL_EXPAND2(...) XOFFSET_DETAIL_EXPAND1(XOFFSET_DETAIL_EXPAND1(XOFFSET_DETAIL_EXPAND1(XOFFSET_DETAIL_EXPAND1(__VA_ARGS__))))
#define XOFFSET_DETAIL_EXPAND1(...) __VA_ARGS__
#define XOFFSET_DETAIL_FOR_EACH(macro, ...)                                    \
    __VA_OPT__(XOFFSET_DETAIL_EXPAND(XOFFSET_DETAIL_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define XOFFSET_DETAIL_FOR_EACH_HELPER(macro, a1, ...)                         \
    macro(a1)                                                                  \
    __VA_OPT__(XOFFSET_DETAIL_FOR_EACH_AGAIN XOFFSET_DETAIL_PARENS (macro, __VA_ARGS__))
#define XOFFSET_DETAIL_FOR_EACH_AGAIN() XOFFSET_DETAIL_FOR_EACH_HELPER

// Compatibility wrappers. `name` is ignored: admission and signatures no longer
// need a registered name.
#define XOFFSET_REGISTER_TYPE(Type, name, strategy)                            \
    XOFFSET_REGISTER_MIGRATION_TYPE(Type, strategy)

#define XOFFSET_REGISTER_CONTAINER(Template, name, strategy)                   \
    XOFFSET_REGISTER_MIGRATION_CONTAINER(Template, strategy)

#define XOFFSET_REGISTER_MAP(Template, name, strategy)                         \
    XOFFSET_REGISTER_MIGRATION_MAP(Template, strategy)

// Built-in registrations
XOFFSET_REGISTER_MIGRATION_TYPE(XString, AllocatorAware)
XOFFSET_REGISTER_MIGRATION_CONTAINER(XVector, Container)
XOFFSET_REGISTER_MIGRATION_CONTAINER(XSet, Container)
XOFFSET_REGISTER_MIGRATION_MAP(XMap, Container)
XOFFSET_REGISTER_MIGRATION_TYPE(XFixedString, AllocatorAware)
XOFFSET_REGISTER_MIGRATION_CONTAINER(XFixedVector, Container)
XOFFSET_REGISTER_MIGRATION_CONTAINER(XFixedFlatSet, Container)
XOFFSET_REGISTER_MIGRATION_MAP(XFixedFlatMap, Container)

#undef XOFFSET_DETAIL_MOVE_INLINE
#undef XOFFSET_DETAIL_MOVE_COLD

#include "xoffset_verified_paths.hpp"
#include "xoffset_verified_receive.hpp"
#include "xoffset_core_lifetime.hpp"

#endif
