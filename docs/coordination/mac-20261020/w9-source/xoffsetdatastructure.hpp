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

#include <experimental/meta>
#include <cstddef>
#include <concepts>
#include <functional>
#include <cstdint>
#include <span>
#include <type_traits>
#include <string>
#include <string_view>
#include <ostream>
#include <utility>
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
#include "xoffset_virtual_memory.hpp"

// TypeLayout library — the authoritative byte-copy-safety engine.
// XOffset uses TypeLayout for recursive admission and safety checks.
// Verified-wire schema hashing and frozen-container ABI signatures are owned by XOffset.
//   - is_byte_copy_safe_v<T>    — recursive domain admission predicate
#include <boost/typelayout.hpp>

// Platform: 64-bit little-endian only.
// Type safety: delegated to TypeLayout (see boost::typelayout::is_byte_copy_safe_v<T>).
#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
static_assert(sizeof(void*) == 8,
    "XOffsetDatastructure requires 64-bit platform (sizeof(void*) must be 8)");
static_assert(XOFFSET_LITTLE_ENDIAN,
    "XOffsetDatastructure requires little-endian platform");
#endif // XOFFSET_DISABLE_PLATFORM_CHECKS

namespace XOffsetDatastructure {
    class XException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class XBadAlloc : public XException {
    public:
        XBadAlloc() : XException("XOffset arena allocation failed") {}
    };

    namespace detail {
        template<class T> T* begin_dynamic_array(void*, std::size_t) noexcept;
        template<class T> T* rebuild_dynamic_array(void*, std::size_t, std::uint32_t) noexcept;
        template<class T> T* begin_fresh_ordinary_outer(void*) noexcept;
        template<class T> T* begin_relocated_ordinary_outer(T*, const T&) noexcept;
        template<class T> consteval bool future_custom_lifetime_supported();
        template<class T, class Alloc> void future_default_construct(void*, const Alloc&);
        template<class T, class Src, class Alloc> void future_transfer_construct(void*, Src&&, const Alloc&);
        template<class T> void future_relocate_construct(T*, T&) noexcept;
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

        void deallocate(T* ptr, std::size_t) noexcept {
            if (arena_ && ptr) {
                arena_->deallocate(ptr);
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
    struct XAllocatorStateV1;

    enum class XGrowPolicy { Explicit, Automatic };
    using XBufferPool = detail::VmStoragePool;


    namespace detail {
        // Host-only diagnostics and cache: none of these fields are persisted.
        struct FreelistIndexStats {
            std::uint64_t lookup_calls = 0;
            std::uint64_t node_visits = 0;
            std::uint64_t skip_jumps = 0;
            std::uint64_t cache_resets = 0;
            std::uint64_t query_resets = 0;
            std::uint64_t maintenance_calls = 0;
            std::uint64_t maintenance_comparisons = 0;
            std::uint64_t retire_updates = 0;
            std::uint64_t runtime_owner_acquisitions = 0;
        };
#if defined(XOFFSET_FREELIST_INDEX_DIAGNOSTICS) && XOFFSET_FREELIST_INDEX_DIAGNOSTICS
        inline thread_local FreelistIndexStats freelist_index_stats;
#endif
        inline void reset_freelist_index_stats() noexcept {
#if defined(XOFFSET_FREELIST_INDEX_DIAGNOSTICS) && XOFFSET_FREELIST_INDEX_DIAGNOSTICS
            freelist_index_stats = {};
#endif
        }
        inline FreelistIndexStats snapshot_freelist_index_stats() noexcept {
#if defined(XOFFSET_FREELIST_INDEX_DIAGNOSTICS) && XOFFSET_FREELIST_INDEX_DIAGNOSTICS
            return freelist_index_stats;
#else
            return {};
#endif
        }
        inline void write_freelist_index_stats(std::ostream& out) {
            const auto value = snapshot_freelist_index_stats();
            out << "{\"lookup_calls\":\"" << value.lookup_calls
                << "\",\"node_visits\":\"" << value.node_visits
                << "\",\"skip_jumps\":\"" << value.skip_jumps
                << "\",\"cache_resets\":\"" << value.cache_resets
                << "\",\"query_resets\":\"" << value.query_resets
                << "\",\"maintenance_calls\":\"" << value.maintenance_calls
                << "\",\"maintenance_comparisons\":\"" << value.maintenance_comparisons
                << "\",\"retire_updates\":\"" << value.retire_updates
                << "\",\"runtime_owner_acquisitions\":\"" << value.runtime_owner_acquisitions << "\"}";
        }
        inline void freelist_index_count(std::uint64_t FreelistIndexStats::* member,
                                         std::uint64_t amount = 1) noexcept {
#if defined(XOFFSET_FREELIST_INDEX_DIAGNOSTICS) && XOFFSET_FREELIST_INDEX_DIAGNOSTICS
            freelist_index_stats.*member += amount;
#else
            (void)member;
            (void)amount;
#endif
        }
        template <typename A, typename B>
        inline bool freelist_index_equal(A a, B b) noexcept {
            freelist_index_count(&FreelistIndexStats::maintenance_comparisons);
            return a == b;
        }

        struct FreelistSearchCursor {
            std::uint32_t bytes = 0;
            std::uint32_t head = 0;
            std::uint32_t first = 0;
            std::uint32_t last = 0;
            std::size_t alignment = 0;
        };

        struct FreelistRuntimeCache {
            const void* state = nullptr; // identity only, never dereferenced
            std::uint32_t epoch = 0;
            FreelistSearchCursor cursors[16] = {};

            void reset() noexcept {
                freelist_index_count(&FreelistIndexStats::cache_resets);
                // Count the fixed loop's 16 iterations plus its exit test.
                freelist_index_count(&FreelistIndexStats::maintenance_comparisons, 17);
                state = nullptr;
                epoch = 0;
                for (auto& cursor : cursors) cursor = {};
            }
            void synchronize(const void* identity, std::uint32_t current_epoch) noexcept {
                freelist_index_count(&FreelistIndexStats::maintenance_calls);
                if (!freelist_index_equal(state, identity) ||
                    !freelist_index_equal(epoch, current_epoch)) {
                    reset();
                    state = identity;
                    epoch = current_epoch;
                }
            }
        };
    }

    namespace fixed_detail {
        struct StableEraseOwnerWitness;
        struct allocator_ref;
        inline void* allocate_dynamic_bytes(const allocator_ref&, std::uint32_t, std::size_t);
        inline void retire_dynamic_bytes(const allocator_ref&, void*, std::uint32_t) noexcept;
        inline void release_cached_blocks(XAllocatorStateV1*) noexcept;
    }

    class XBufferCore {
    public:
        using size_type = std::size_t;

        struct alignas(8) arena_header {
            char magic[8];
            std::uint32_t header_size = 0;
            std::uint32_t flags = 0;
            std::uint64_t root_offset = 0;
            std::uint64_t allocator_offset = 0;
            std::uint64_t used_end = 0;
            std::uint64_t reserved0 = 0;

            void* allocate_aligned(std::uint32_t bytes, std::size_t alignment) {
                return owner().allocate_aligned_impl(bytes, alignment);
            }

            void* allocate_without_growth(std::uint32_t bytes, std::size_t alignment) {
                return owner().allocate_aligned_impl(bytes, alignment, false);
            }

            // Internal allocator operation: ptr must identify a live allocation
            // from this arena. Only its physical tail block can grow, and only
            // within the configured capacity. Failure leaves it unchanged.
            bool try_expand(void* ptr, std::uint32_t bytes, std::size_t alignment) {
                return owner().try_expand_impl(ptr, bytes, alignment);
            }

            void deallocate(void* ptr) noexcept {
                owner().deallocate_impl(ptr);
            }

            // Host cache access does not add fields to the V1 header.
            XBufferCore& freelist_runtime_owner() {
                detail::freelist_index_count(&detail::FreelistIndexStats::runtime_owner_acquisitions);
                return owner();
            }
            void reset_freelist_runtime_cache() noexcept {
                freelist_runtime_owner().freelist_runtime_cache_.reset();
            }

        private:
            XBufferCore& owner() {
                return XBufferCore::owner_from_payload_ptr(this);
            }
        };

        XBufferCore() = default;
        ~XBufferCore() { freelist_runtime_cache_.reset(); unregister_registry(); }

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
        };
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
        void set_growth_policy(XGrowPolicy policy) noexcept { growth_policy_ = policy; }
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
            init_fresh(size, cap.value, cap.pool);
        }

        XBufferCore(const char* data, size_type size) {
            init_from_bytes(data, size, capacity_from_hint(size));
        }

        XBufferCore(const char* data, size_type size, size_type max_reserved) {
            init_from_bytes(data, size, capacity_from_hint(size, max_reserved));
        }

        XBufferCore(const char* data, size_type size, MaximumCapacity cap) : growth_policy_(cap.growth) {
            init_from_bytes(data, size, cap.value, cap.pool);
        }

        uint64_t epoch() const noexcept { return m_epoch; }

        bool grow(size_type extra_bytes) {
            if (extra_bytes == 0) return true;
            if (buffer_.empty() || extra_bytes > buffer_.maximum_capacity() - buffer_.size()) return false;
            // The allocator reads the storage's logical extent directly. No segment
            // reopen, offset rewrite, or potentially throwing allocator update follows.
            return buffer_.grow_to(buffer_.size() + extra_bytes);
        }

        void shrink_to_fit() {
            if (buffer_.empty()) return;
            prune_free_blocks();
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
            free_blocks_.swap(other.free_blocks_);
            freelist_runtime_cache_.reset();
            other.freelist_runtime_cache_.reset();
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
            size_type reclaimed = 0;
            for (const auto& block : free_blocks_) {
                reclaimed += block.total_bytes;
            }
            return tail + reclaimed;
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

        template <typename T>
        T* find_allocator_storage() noexcept {
            return arena() ? object_at_offset<T>(allocator_offset_value()) : nullptr;
        }

        template <typename T>
        const T* find_allocator_storage() const noexcept {
            return arena() ? object_at_offset_const<T>(allocator_offset_value()) : nullptr;
        }

        template <typename T, typename... Args>
        T* construct_allocator_storage(Args&&... args) {
            return construct_object_at_slot<T>(allocator_offset_slot(), std::forward<Args>(args)...);
        }

    public:
        struct alignas(8) AllocationHeader {
            std::uint32_t total_bytes = 0;
            std::uint32_t user_bytes = 0;
            std::uint32_t user_offset = 0;
            std::uint32_t alignment = 0;
        };
        static_assert(sizeof(AllocationHeader) == 16 && alignof(AllocationHeader) == 8);

    private:
        struct FreeBlock {
            std::uint32_t user_offset = 0;
            std::uint32_t total_bytes = 0;
            std::uint32_t user_bytes = 0;
            std::uint32_t alignment = 0;
        };

        static constexpr char payload_magic_[8] = {'X', 'B', 'U', 'F', 'V', '1', '\0', '\0'};

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
        inline static std::atomic<std::size_t> canonical_active_count_{0};
        struct OwnerRecord {
            const void* const payload;
            std::atomic<XBufferCore*> owner;
            std::byte* const canonical_origin;
            const std::size_t canonical_extent;
            std::atomic<bool> canonical_registered{false};
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
            static thread_local bool available = true;
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

        static XBufferCore& owner_from_payload_ptr(const void* ptr) {
            detail::owner_lookup_count(&detail::OwnerLookupStats::entries);
            OwnerCache* cache = nullptr;
            if (owner_cache_available()) {
                cache = &owner_cache();
                for (std::size_t attempt = 0; attempt != 4; ++attempt) {
                    const auto slot = (cache->last + attempt) % 4u;
                    auto& record = cache->records[slot];
                    if (record && record->payload == ptr) {
                        if (auto* owner = record->owner.load(std::memory_order_acquire)) {
                            detail::owner_lookup_count(&detail::OwnerLookupStats::cache_hits);
                            cache->last = slot;
                            return *owner;
                        }
                        detail::owner_lookup_count(&detail::OwnerLookupStats::dead_records);
                        record.reset();
                    }
                }
            }
            detail::owner_lookup_count(&detail::OwnerLookupStats::cold_lookups);
            OwnerRecordPtr record;
            {
                auto& state = registry_state();
                std::lock_guard lock(state.mutex);
                const auto found = state.owners.find(ptr);
                if (found != state.owners.end()) record = found->second;
            }
            // Error construction and all shared-owner releases happen unlocked.
            if (!record) throw XException("arena owner lookup failed");
            if (cache) {
                const auto slot = cache->next;
                // Keep the local lease across release of an evicted record.
                cache->records[slot] = record;
                cache->last = slot;
                cache->next = (slot + 1u) % 4u;
            }
            auto* owner = record->owner.load(std::memory_order_acquire);
            if (!owner) throw XException("arena owner lookup failed");
            return *owner;
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
            return align_up(sizeof(arena_header), alignof(std::max_align_t));
        }

        void init_fresh(size_type size, size_type max_reserved, const XBufferPool& pool = {}) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD) throw XBadAlloc();
            size = std::max(size, min_payload_size());
            if (size > max_reserved) throw XBadAlloc();
            try {
                freelist_runtime_cache_.reset();
                buffer_ = detail::VirtualMemoryStorage(size, max_reserved, pool);
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
            sm->allocator_offset = 0;
            sm->used_end = min_payload_size();
            sm->reserved0 = 0;
            register_registry();
        }

        void init_from_bytes(const char* data, size_type size, size_type max_reserved, const XBufferPool& pool = {}) {
            if (size > MAX_PAYLOAD || max_reserved > MAX_PAYLOAD) throw XBadAlloc();
            if (!data || size < sizeof(arena_header)) {
                throw XException(
                    "XBufferCore: payload too small");
            }
            if (size > max_reserved) throw XBadAlloc();
            try {
                freelist_runtime_cache_.reset();
                buffer_ = detail::VirtualMemoryStorage::from_bytes(data, size, max_reserved, pool);
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
            register_registry();
        }

        void move_from(XBufferCore&& other) noexcept {
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

        void update_registry_owner() noexcept {
            if (!owner_record_) return;
            owner_record_->owner.store(this, std::memory_order_release);
            detail::owner_lookup_count(&detail::OwnerLookupStats::owner_updates);
        }

        void unregister_registry() noexcept {
            auto record = std::move(owner_record_);
            if (!record) return;
            record->owner.store(nullptr, std::memory_order_release);
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

        void register_registry() {
            if (std::exchange(detail::pending_registry_failure, false)) throw XBadAlloc();
            try {
                // Both allocations, including any new_handler reentry, precede
                // the mutex. std::map inserts this same-allocator node without
                // allocation, comparison callbacks, rehash or record creation.
                auto record = std::make_shared<OwnerRecord>(buffer_.data(), this,
                    buffer_.canonical_byte_origin(), buffer_.maximum_capacity());
                RegistryOwners prepared;
                prepared.emplace(record->payload, record);
                auto node = prepared.extract(prepared.begin());
                bool inserted = false;
                {
                    auto& state = registry_state();
                    std::lock_guard lock(state.mutex);
                    if (state.owners.find(record->payload) == state.owners.end()) {
                        state.owners.insert(std::move(node));
                        owner_record_ = record;
                        mark_canonical_registration(*record);
                        inserted = true;
                    }
                }
                if (!inserted) throw XException("duplicate arena owner registration");
                detail::owner_lookup_count(&detail::OwnerLookupStats::registrations);
                detail::owner_lookup_count(&detail::OwnerLookupStats::record_object_bytes, sizeof(OwnerRecord));
            } catch (const std::bad_alloc&) {
                throw XBadAlloc();
            }
        }

        void prune_free_blocks() {
            size_type used = used_end_clamped();
            free_blocks_.erase(
                std::remove_if(
                    free_blocks_.begin(), free_blocks_.end(),
                    [&](const FreeBlock& block) {
                        return static_cast<size_type>(block.user_offset + block.user_bytes) > used;
                    }),
                free_blocks_.end());
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

        std::uint64_t& allocator_offset_slot() noexcept {
            return arena()->allocator_offset;
        }

        const std::uint64_t& allocator_offset_value() const noexcept {
            return arena()->allocator_offset;
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
            void* raw = allocate_aligned_impl(
                static_cast<std::uint32_t>(sizeof(T)), alignof(T));
            T* obj;
            try {
                obj = std::construct_at(reinterpret_cast<T*>(raw), std::forward<Args>(args)...);
            } catch (...) {
                deallocate_impl(raw);
                throw;
            }
            slot = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(obj) - reinterpret_cast<std::uintptr_t>(buffer_.data()));
            return obj;
        }

        bool ensure_allocation_extent(size_type required, bool allow_growth = true) {
            if (required <= buffer_.size()) return true;
            if (!allow_growth || growth_policy_ != XGrowPolicy::Automatic ||
                buffer_.empty() || required > buffer_.maximum_capacity()) return false;
            const auto maximum = buffer_.maximum_capacity();
            const auto current = buffer_.size();
            const auto geometric = current > maximum / 2 ? maximum : current * 2;
            // Throw on an OS/preparation failure. In particular, a container's
            // failed tail expansion must not silently retry via a new allocation.
            if (!buffer_.grow_to(std::max(required, geometric))) throw XBadAlloc();
            return true;
        }

        template <bool Probe>
        static void* allocation_failure() {
            if constexpr (Probe) return nullptr;
            else throw XBadAlloc();
        }

        // G: one body for the throwing entry and a non-throwing no-growth
        // probe. Probe=true returns nullptr at EXACTLY the points where
        // allocate_aligned_impl(bytes, alignment, false) throws XBadAlloc, and
        // performs the same state changes otherwise. With allow_growth=false,
        // ensure_allocation_extent cannot grow or throw, and nothing else in
        // the body can throw, so the probe is noexcept and allocates nothing.
        template <bool Probe>
        void* allocate_aligned_body(std::uint32_t bytes, std::size_t alignment, bool allow_growth) {
            if (bytes > MAX_PAYLOAD || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > MAX_PAYLOAD) return allocation_failure<Probe>();
            if (bytes == 0) bytes = 1;
            alignment = std::max<std::size_t>(alignment, alignof(std::max_align_t));

            for (std::size_t i = 0; i < free_blocks_.size(); ++i) {
                const auto& block = free_blocks_[i];
                if (block.user_bytes >= bytes && block.alignment >= alignment) {
                    void* ptr = payload_byte_address(block.user_offset);
                    if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0) continue;
                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    return ptr;
                }
            }

            auto* sm = arena();
            size_type current_end = used_end_clamped();
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto limit = (std::numeric_limits<std::uintptr_t>::max)();
            if (current_end > limit - base || sizeof(AllocationHeader) > limit - base - current_end)
                return allocation_failure<Probe>();
            const auto first_address = base + current_end + sizeof(AllocationHeader);
            if (alignment - 1u > limit - first_address) return allocation_failure<Probe>();
            const auto user_address = align_up(first_address, alignment);
            const size_type user_offset = user_address - base;
            if (user_offset > MAX_PAYLOAD || bytes > MAX_PAYLOAD - user_offset)
                return allocation_failure<Probe>();
            const size_type block_begin = user_offset - sizeof(AllocationHeader);
            const size_type block_end = user_offset + bytes;
            if (!ensure_allocation_extent(block_end, allow_growth)) return allocation_failure<Probe>();

            auto* header = ::new (static_cast<void*>(payload_byte_address(block_begin))) AllocationHeader;
            header->total_bytes = static_cast<std::uint32_t>(block_end - block_begin);
            header->user_bytes = bytes;
            header->user_offset = static_cast<std::uint32_t>(user_offset);
            header->alignment = static_cast<std::uint32_t>(alignment);
            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);
            return payload_byte_address(user_offset);
        }

        void* allocate_aligned_impl(std::uint32_t bytes, std::size_t alignment, bool allow_growth = true) {
            return allocate_aligned_body<false>(bytes, alignment, allow_growth);
        }

        void* try_allocate_aligned_without_growth(std::uint32_t bytes, std::size_t alignment) noexcept {
            return allocate_aligned_body<true>(bytes, alignment, false);
        }

        bool try_expand_impl(void* ptr, std::uint32_t bytes, std::size_t alignment) {
            if (!ptr || buffer_.empty() || bytes == 0 || bytes > MAX_PAYLOAD ||
                alignment == 0 || (alignment & (alignment - 1)) != 0 ||
                alignment > MAX_PAYLOAD) return false;
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (address < base || address - base < sizeof(AllocationHeader) ||
                address - base >= buffer_.size() || address % alignment != 0 ||
                (address - sizeof(AllocationHeader)) % alignof(AllocationHeader) != 0) return false;
            const auto offset = static_cast<size_type>(address - base);
            auto* header = std::launder(reinterpret_cast<AllocationHeader*>(
                payload_byte_address(offset - sizeof(AllocationHeader))));
            if (header->user_offset != offset || header->user_bytes == 0 ||
                header->user_bytes > buffer_.size() - offset ||
                header->total_bytes != std::uint64_t(header->user_bytes) + sizeof(AllocationHeader) ||
                header->alignment < alignment || header->alignment > MAX_PAYLOAD ||
                (header->alignment & (header->alignment - 1)) != 0 ||
                address % header->alignment != 0 ||
                offset + header->user_bytes != arena()->used_end) return false;
            // Reuse can give a container more physical storage than its recorded
            // capacity. Never shorten that storage or lose its tail identity.
            if (bytes <= header->user_bytes) return true;
            if (bytes > MAX_PAYLOAD - offset || !ensure_allocation_extent(offset + bytes)) return false;
            header->user_bytes = bytes;
            header->total_bytes = static_cast<std::uint32_t>(sizeof(AllocationHeader) + bytes);
            arena()->used_end = offset + bytes;
            return true;
        }

        void deallocate_impl(void* ptr) noexcept {
            if (!ptr || buffer_.empty()) return;
            const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            if (address < base || address - base < sizeof(AllocationHeader) ||
                address - base >= buffer_.size()) return;
            auto* header = std::launder(reinterpret_cast<AllocationHeader*>(
                payload_byte_address(address - base - sizeof(AllocationHeader))));
            if (header->total_bytes == 0 || header->user_offset == 0) return;
            const FreeBlock block{header->user_offset, header->total_bytes,
                                  header->user_bytes, header->alignment};
            if (free_blocks_.size() < free_blocks_.capacity()) {
                free_blocks_.push_back(block); // trivial, no allocation/callback
                return;
            }
            const auto identity = owner_record_;
            auto required = std::max(free_blocks_.size() + 1u,
                free_blocks_.capacity() <= free_blocks_.max_size() / 2u
                    ? free_blocks_.capacity() * 2u : free_blocks_.size() + 1u);
            std::vector<FreeBlock> prepared;
            for (;;) {
                try {
                    detail::owner_lookup_count(&detail::OwnerLookupStats::reclamation_reserves);
                    prepared.reserve(required);
                } catch (const std::bad_alloc&) {
                    detail::owner_lookup_count(&detail::OwnerLookupStats::reclamation_abandoned);
                    return; // same best-effort host-OOM reclamation policy
                }
                // reserve/new_handler may move/swap/release/reenter the owner.
                // Never access the original this/vector after that boundary.
                auto* current = record_owner(identity);
                if (!current) {
                    detail::owner_lookup_count(&detail::OwnerLookupStats::reclamation_abandoned);
                    return;
                }
                if (prepared.capacity() <= current->free_blocks_.size()) {
                    required = std::max(current->free_blocks_.size() + 1u,
                        current->free_blocks_.capacity() <= current->free_blocks_.max_size() / 2u
                            ? current->free_blocks_.capacity() * 2u : current->free_blocks_.size() + 1u);
                    detail::owner_lookup_count(&detail::OwnerLookupStats::reclamation_restarts);
                    continue;
                }
                prepared.assign(current->free_blocks_.begin(), current->free_blocks_.end());
                prepared.push_back(block);
                current->free_blocks_.swap(prepared);
                return;
            }
        }

    protected:
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
        friend void* fixed_detail::allocate_dynamic_bytes(const fixed_detail::allocator_ref&, std::uint32_t, std::size_t);
        friend void fixed_detail::retire_dynamic_bytes(const fixed_detail::allocator_ref&, void*, std::uint32_t) noexcept;
        friend void fixed_detail::release_cached_blocks(XAllocatorStateV1*) noexcept;
        detail::FreelistRuntimeCache freelist_runtime_cache_;
        OwnerRecordPtr owner_record_;
        detail::VirtualMemoryStorage buffer_;
        XGrowPolicy growth_policy_ = XGrowPolicy::Explicit;
        std::vector<FreeBlock> free_blocks_;
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

        template <typename T>
        consteval std::size_t reflected_base_count() {
            using namespace std::meta;
            return bases_of(^^T, access_context::unchecked()).size();
        }

        template <typename T>
        consteval std::size_t reflected_member_count() {
            using namespace std::meta;
            return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
        }

        template <typename T, typename BaseFn, typename MemberFn>
        void for_each_reflected_subobject(BaseFn&& on_base, MemberFn&& on_member) {
            static_for<reflected_base_count<T>()>([&](auto index) {
                on_base(index);
            });
            static_for<reflected_member_count<T>()>([&](auto index) {
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

    struct alignas(8) XAllocatorStateV1 {
        std::uint32_t arena_begin = 0;
        std::uint32_t arena_end = 0;
        std::uint32_t free_bytes = 0;
        std::uint32_t epoch = 0;
        std::int32_t segment_delta = 0;
        std::int32_t reserved0 = 0;
        std::uint32_t freelist_head[16] = {};
    };

    static_assert(alignof(XAllocatorStateV1) == 8,
        "XAllocatorStateV1 ABI must stay 8-byte aligned");
    static_assert(sizeof(XAllocatorStateV1) == 88,
        "XAllocatorStateV1 ABI must stay frozen at 88 bytes");

    namespace fixed_detail {
        inline constexpr std::int32_t arena_binding_segment_tag = 0x1;
        inline constexpr std::uint32_t freelist_class_count = 16;
        inline constexpr std::uint32_t freelist_min_block_bytes = 8;
        inline constexpr std::uint32_t freelist_max_block_bytes =
            freelist_min_block_bytes << (freelist_class_count - 1);

        struct allocator_ref {
            XAllocatorStateV1* state{};
            XArena* segment{};

            XAllocatorStateV1* get_allocator_state() const noexcept {
                return state;
            }

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
                auto& owner = XBufferCore::owner_from_payload_ptr(alloc.segment);
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
            return a.segment && a.segment == b.segment && a.state == b.state;
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
        // MV: result-identical to move_assignment_owns_address; leaf heads are
        // evaluated inline at the move-assignment call site.
        template <typename T>
        XOFFSET_DETAIL_MOVE_INLINE bool move_assignment_owns_address_fast(
            const T& value, const void* address) noexcept;

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

        inline bool arena_binding_is_segment(std::int32_t delta) noexcept {
            return delta != 0 && (delta & arena_binding_segment_tag) != 0;
        }

        inline std::int32_t clear_arena_binding_tag(std::int32_t delta) noexcept {
            return delta & ~arena_binding_segment_tag;
        }

        inline void set_tagged_rel32(std::int32_t& field, const void* origin,
                                     const void* target, std::int32_t tag_bits) {
            set_rel32(field, origin, target);
            field |= tag_bits;
        }

        inline XArena* allocator_arena(
            const XAllocatorStateV1* state) noexcept {
            if (!state || state->segment_delta == 0) return nullptr;
            return const_cast<XArena*>(
                resolve_rel32_const<XArena>(
                    &state->segment_delta, state->segment_delta));
        }

        inline allocator_ref allocator_ref_of(XAllocatorStateV1* state) noexcept {
            return {state, allocator_arena(state)};
        }

        struct FreeBlockHeader {
            std::uint32_t next_offset = 0;
            std::uint32_t block_bytes = 0;
        };

        inline std::uint32_t round_up_to_alignment(std::uint32_t value,
                                                   std::uint32_t alignment) noexcept {
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        inline std::uint32_t freelist_size_class(std::uint32_t bytes) noexcept {
            std::uint32_t block = freelist_min_block_bytes;
            std::uint32_t index = 0;
            while (block < bytes && index + 1u < freelist_class_count) {
                block <<= 1u;
                ++index;
            }
            if (block < bytes) return freelist_class_count;
            return index;
        }

        inline std::uint32_t freelist_offset_from_state(
            const XAllocatorStateV1* state, const void* ptr) {
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            const auto origin = reinterpret_cast<std::uintptr_t>(state);
            const auto delta = address - origin;
            if (address <= origin || delta > std::numeric_limits<std::uint32_t>::max()) {
                throw XException(
                    "allocator state free-list offset out of range");
            }
            return static_cast<std::uint32_t>(delta);
        }

        inline FreeBlockHeader* freelist_block_from_offset(
            XAllocatorStateV1* state, std::uint32_t offset) noexcept {
            if (offset == 0) return nullptr;
            auto* raw = reinterpret_cast<char*>(state) + offset; // W9: flat
            return std::launder(reinterpret_cast<FreeBlockHeader*>(raw));
        }

        inline const FreeBlockHeader* freelist_block_from_offset(
            const XAllocatorStateV1* state, std::uint32_t offset) noexcept {
            if (offset == 0) return nullptr;
            auto* raw = reinterpret_cast<const char*>(state) + offset; // W9: flat
            return std::launder(reinterpret_cast<const FreeBlockHeader*>(raw));
        }

        inline void initialize_allocator_state(
            XAllocatorStateV1& state, XArena* segment) {
            state.arena_begin = static_cast<std::uint32_t>(sizeof(XAllocatorStateV1));
            state.arena_end = static_cast<std::uint32_t>(sizeof(XAllocatorStateV1));
            state.free_bytes = 0;
            state.epoch = 0;
            state.segment_delta = 0;
            state.reserved0 = 0;
            std::memset(state.freelist_head, 0, sizeof(state.freelist_head));
            set_rel32(state.segment_delta, &state.segment_delta, segment);
            if (segment) segment->reset_freelist_runtime_cache();
        }

        inline void note_allocator_block(
            XAllocatorStateV1* state, const void* ptr, std::uint32_t bytes) {
            if (!state || !ptr || bytes == 0) return;
            auto offset = freelist_offset_from_state(state, ptr);
            if (state->arena_begin == 0 || offset < state->arena_begin) {
                state->arena_begin = offset;
            }
            std::uint64_t end = static_cast<std::uint64_t>(offset) + bytes;
            if (end > std::numeric_limits<std::uint32_t>::max()) {
                throw XException(
                    "allocator state arena end overflow");
            }
            if (end > state->arena_end) {
                state->arena_end = static_cast<std::uint32_t>(end);
            }
        }

        inline void release_cached_blocks(XAllocatorStateV1* state) noexcept {
            auto* arena = allocator_arena(state);
            if (!state || !arena) return;
            auto& initial_owner = arena->freelist_runtime_owner();
            const auto identity = initial_owner.owner_record_;
            initial_owner.freelist_runtime_cache_.reset();
            std::uint32_t detached[freelist_class_count];
            // Publish a consistent empty cache before any host allocation can
            // invoke new_handler. Reentrant retires populate a new independent
            // cache and must not be erased by a final unconditional reset.
            for (std::uint32_t i = 0; i < freelist_class_count; ++i) {
                detached[i] = std::exchange(state->freelist_head[i], 0u);
            }
            state->free_bytes = 0;
            ++state->epoch;
            for (std::uint32_t i = 0; i < freelist_class_count; ++i) {
                auto offset = detached[i];
                while (offset != 0) {
                    auto* owner = XBufferCore::record_owner(identity);
                    if (!owner) return; // mapping/state were released in callback
                    auto* block = freelist_block_from_offset(state, offset);
                    const auto next = block->next_offset;
                    std::destroy_at(block);
                    owner->deallocate_impl(block);
                    offset = next;
                }
            }
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

            std::uint32_t aligned_bytes = round_up_to_alignment(
                bytes, static_cast<std::uint32_t>(std::max<std::size_t>(alignment, 8u)));

            XBufferCore* owner = nullptr;
            if (auto* state = alloc_ref.state) {
                // One registry lookup also serves a later Core allocation.
                owner = &arena->freelist_runtime_owner();
                auto& cache = owner->freelist_runtime_cache_;
                cache.synchronize(state, state->epoch);
                detail::freelist_index_count(&detail::FreelistIndexStats::lookup_calls);
                auto class_index = freelist_size_class(aligned_bytes);
                if (class_index < freelist_class_count) {
                    for (std::uint32_t i = class_index; i < freelist_class_count; ++i) {
                        auto& cursor = cache.cursors[i];
                        // Different requests can remove a middle node without
                        // changing head. Never reuse their old rejected range.
                        if (!detail::freelist_index_equal(cursor.bytes, aligned_bytes) ||
                            !detail::freelist_index_equal(cursor.alignment, alignment) ||
                            !detail::freelist_index_equal(cursor.head, state->freelist_head[i])) {
                            cursor = {};
                            cursor.bytes = aligned_bytes;
                            cursor.alignment = alignment;
                            cursor.head = state->freelist_head[i];
                            detail::freelist_index_count(&detail::FreelistIndexStats::query_resets);
                        }
                        auto old_first = cursor.first;
                        auto old_last = cursor.last;
                        std::uint32_t prefix_first = 0;
                        std::uint32_t prefix_last = 0;
                        auto* link = &state->freelist_head[i];
                        while (*link) {
                            const auto offset = *link;
                            if (!detail::freelist_index_equal(old_first, std::uint32_t{0}) &&
                                detail::freelist_index_equal(offset, old_first)) {
                                // Identity/epoch/request/head were checked
                                // before reading an offset from the memo.
                                detail::freelist_index_count(&detail::FreelistIndexStats::skip_jumps);
                                if (detail::freelist_index_equal(prefix_first, std::uint32_t{0}))
                                    prefix_first = old_first;
                                prefix_last = old_last;
                                link = &freelist_block_from_offset(state, old_last)->next_offset;
                                old_first = old_last = 0;
                                continue;
                            }
                            auto* block = freelist_block_from_offset(state, offset);
                            detail::freelist_index_count(&detail::FreelistIndexStats::node_visits);
                            if (block->block_bytes >= aligned_bytes &&
                                reinterpret_cast<std::uintptr_t>(block) % alignment == 0) {
                                *link = block->next_offset;
                                state->free_bytes -= block->block_bytes;
                                ++state->epoch;
                                cache.epoch = state->epoch;
                                if (detail::freelist_index_equal(prefix_first, std::uint32_t{0})) {
                                    cursor.first = old_first;
                                    cursor.last = old_last;
                                } else {
                                    cursor.first = prefix_first;
                                    cursor.last = prefix_last;
                                }
                                cursor.head = state->freelist_head[i];
                                std::destroy_at(block); // last header read precedes reuse
                                return static_cast<void*>(block);
                            }
                            if (detail::freelist_index_equal(prefix_first, std::uint32_t{0}))
                                prefix_first = offset;
                            prefix_last = offset;
                            link = &block->next_offset;
                        }
                        cursor.first = prefix_first;
                        cursor.last = prefix_last;
                        cursor.head = state->freelist_head[i];
                    }
                }
            }

            if (!owner) owner = &XBufferCore::owner_from_payload_ptr(arena);
            const auto identity = owner->owner_record_;
            // G: nullptr exactly where allocate_aligned_impl(..., false) threw
            // XBadAlloc. No diagnostic exception object (and so no host
            // allocation, new_handler call or unwinding) separates the failed
            // no-growth attempt from the unchanged recovery below.
            void* raw = owner->try_allocate_aligned_without_growth(aligned_bytes, alignment);
            if (!raw) {
                // Revalidation kept: release_cached_blocks below can still
                // reach host allocation and new_handler.
                owner = &XBufferCore::require_record_owner(identity);
                if (alloc_ref.state && alloc_ref.state->free_bytes != 0)
                    release_cached_blocks(alloc_ref.state);
                owner = &XBufferCore::require_record_owner(identity);
                raw = owner->allocate_aligned_impl(aligned_bytes, alignment);
            }
            note_allocator_block(alloc_ref.state, raw, aligned_bytes);
            return raw;
        }

        inline bool try_expand_dynamic_bytes(const allocator_ref& alloc_ref,
                                             void* ptr, std::uint32_t bytes,
                                             std::size_t alignment) {
            if (!alloc_ref.segment || !ptr || bytes == 0 || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > XBufferCore::MAX_PAYLOAD ||
                bytes > XBufferCore::MAX_PAYLOAD - std::max<std::size_t>(alignment, 8u)) return false;
            const auto aligned_bytes = round_up_to_alignment(
                bytes, static_cast<std::uint32_t>(std::max<std::size_t>(alignment, 8u)));

            // Containers supply only live allocations. Check every state update
            // before the Core publishes the enlarged physical allocation.
            std::uint32_t offset = 0;
            std::uint32_t end = 0;
            if (auto* state = alloc_ref.state) {
                const auto address = reinterpret_cast<std::uintptr_t>(ptr);
                const auto origin = reinterpret_cast<std::uintptr_t>(state);
                if (address <= origin || address - origin > std::numeric_limits<std::uint32_t>::max())
                    return false;
                XBufferCore::AllocationHeader header;
                // W9: flat header address.
                const void* raw_header = static_cast<const void*>(static_cast<const char*>(ptr) - sizeof(header));
                std::memcpy(&header, raw_header, sizeof(header));
                // Valid live allocations cannot expand unless their physical
                // tail is the arena tail. Reject before the registry lookup;
                // Core::try_expand_impl still performs every original check.
                if (std::uint64_t(header.user_offset) + header.user_bytes !=
                    alloc_ref.segment->used_end) return false;
                const auto physical_bytes = std::max(aligned_bytes, header.user_bytes);
                const auto candidate_end = std::uint64_t(address - origin) + physical_bytes;
                if (candidate_end > std::numeric_limits<std::uint32_t>::max()) return false;
                offset = static_cast<std::uint32_t>(address - origin);
                end = static_cast<std::uint32_t>(candidate_end);
            }
            if (!alloc_ref.segment->try_expand(ptr, aligned_bytes, alignment)) return false;
            if (auto* state = alloc_ref.state) {
                if (state->arena_begin == 0 || offset < state->arena_begin) state->arena_begin = offset;
                if (end > state->arena_end) state->arena_end = end;
            }
            return true;
        }

        inline void retire_dynamic_bytes(const allocator_ref& alloc_ref,
                                         void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0) return;
            auto* arena = alloc_ref.segment;
            auto* state = alloc_ref.state;

            std::uint32_t aligned_bytes = round_up_to_alignment(bytes, 8u);
            auto class_index = freelist_size_class(aligned_bytes);
            if (!state || class_index >= freelist_class_count ||
                aligned_bytes < sizeof(FreeBlockHeader)) {
                if (arena) arena->deallocate(ptr);
                return;
            }

            note_allocator_block(state, ptr, aligned_bytes);
            auto offset = freelist_offset_from_state(state, ptr);
            auto& cache = arena->freelist_runtime_owner().freelist_runtime_cache_;
            cache.synchronize(state, state->epoch);
            detail::freelist_index_count(&detail::FreelistIndexStats::retire_updates);
            // Retiring the whole allocation intentionally ends its previous
            // array/object lifetimes. A cast/launder is not header creation.
            auto* header = std::construct_at(reinterpret_cast<FreeBlockHeader*>(ptr),
                FreeBlockHeader{state->freelist_head[class_index], aligned_bytes});
            state->freelist_head[class_index] = offset;
            state->free_bytes += aligned_bytes;
            ++state->epoch;
            cache.epoch = state->epoch;
            // A new head leaves a known rejected interval intact, even
            // when it precedes that interval with a fitting block.
            cache.cursors[class_index].head = offset;
        }

        template <typename T>
        void destroy_n(T* ptr, std::uint32_t count) noexcept {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(ptr, count);
            }
        }

        inline allocator_ref allocator_ref_of(XArena* sm) noexcept {
            return {nullptr, sm};
        }

        template <typename Alloc>
            requires requires(const Alloc& alloc) { alloc.arena(); }
        inline allocator_ref allocator_ref_of(const Alloc& alloc) noexcept {
            if constexpr (requires(const Alloc& value) { value.get_allocator_state(); }) {
                return {alloc.get_allocator_state(), alloc.arena()};
            } else {
                return {nullptr, alloc.arena()};
            }
        }

        struct stored_allocator_proxy {
            XAllocatorStateV1* state{};
            XArena* segment{};

            XArena* arena() const noexcept { return segment; }

            XAllocatorStateV1* get_allocator_state() const noexcept {
                return state;
            }
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

        template <typename L, typename R>
        bool ordered_less(const L& lhs, const R& rhs) {
            if constexpr (ordered_string_like<L> && ordered_string_like<R>) {
                return ordered_view(lhs) < ordered_view(rhs);
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

    struct alignas(8) XFixedString {
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;

        std::int32_t arena_delta = 0;
        std::int32_t data_delta = 0;
        std::uint32_t size_ = 0;
        std::uint32_t capacity_ = 0;

        XFixedString() = default;
        explicit XFixedString(allocator_type alloc) { bind(alloc); }
        explicit XFixedString(XAllocatorStateV1* state) {
            bind(fixed_detail::allocator_ref_of(state));
        }
        ~XFixedString() { release(); }
        XFixedString(const XFixedString& other, allocator_type alloc) : XFixedString(alloc) {
            assign(other.view());
        }

        XFixedString(const XFixedString&) = delete;
        XFixedString(XFixedString&&) = delete;
        XFixedString& operator=(const XFixedString& other) {
            if (this == &other) return *this;
            if (!is_bound() && other.is_bound()) bind_like(other);
            ensure_bound();
            assign(other.view());
            return *this;
        }
        XFixedString& operator=(XFixedString&& other) {
            if (this == &other) return *this;
            if (is_bound() && other.is_bound()) {
                const auto destination = binding();
                // MV: binding() is {allocator_state(), arena()}, and for a state
                // binding arena() == allocator_arena(allocator_state()). Equal
                // non-null states therefore give the identical binding (same field
                // read, no intervening write); otherwise compute it as before.
                const auto source_state = other.allocator_state();
                const auto source = source_state && source_state == destination.state
                    ? destination : other.binding();
                if (fixed_detail::move_assignment_same_allocator(destination, source) &&
                    fixed_detail::move_assignment_head_in_arena(this, sizeof(*this), destination.segment) &&
                    fixed_detail::move_assignment_head_in_arena(&other, sizeof(other), source.segment) &&
                    !fixed_detail::move_assignment_owns_address_fast(*this, &other) &&
                    !fixed_detail::move_assignment_owns_address_fast(other, this)) {
                    std::int32_t incoming_delta = 0;
                    fixed_detail::set_rel32(incoming_delta, &data_delta, other.data());
                    const auto incoming_size = other.size_;
                    const auto incoming_capacity = other.capacity_;
                    // Potentially throwing work precedes the ownership commit.
                    // release() is noexcept; both heads remain in live storage.
                    // MV: release()'s empty-destination case inline (data() is
                    // null exactly when data_delta == 0, and release() then only
                    // clears the descriptor); otherwise the unchanged release().
                    if (data_delta == 0) {
                        size_ = 0;
                        capacity_ = 0;
                    } else {
                        release();
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

        void bind(allocator_type alloc) {
            if (alloc.state) {
                fixed_detail::set_tagged_rel32(arena_delta, &arena_delta, alloc.state, 0);
            } else {
                fixed_detail::set_tagged_rel32(
                    arena_delta, &arena_delta, alloc.segment,
                    fixed_detail::arena_binding_segment_tag);
            }
        }

        void bind_like(const XFixedString& other) {
            if (auto* state = other.allocator_state()) {
                bind(fixed_detail::allocator_ref_of(state));
            } else {
                bind(fixed_detail::allocator_ref_of(other.arena()));
            }
        }

        bool is_bound() const noexcept { return arena_delta != 0; }

        XAllocatorStateV1* allocator_state() const noexcept {
            if (arena_delta == 0 || fixed_detail::arena_binding_is_segment(arena_delta)) {
                return nullptr;
            }
            return const_cast<XAllocatorStateV1*>(
                fixed_detail::resolve_rel32_const<XAllocatorStateV1>(&arena_delta, arena_delta));
        }

        allocator_type binding() const noexcept {
            return {allocator_state(), arena()};
        }

        XArena* arena() const noexcept {
            if (arena_delta == 0) return nullptr;
            if (fixed_detail::arena_binding_is_segment(arena_delta)) {
                return const_cast<XArena*>(
                    fixed_detail::resolve_rel32_const<XArena>(
                        &arena_delta, fixed_detail::clear_arena_binding_tag(arena_delta)));
            }
            return fixed_detail::allocator_arena(allocator_state());
        }

        stored_allocator_type get_stored_allocator() const noexcept {
            auto alloc = binding();
            return {alloc.state, alloc.segment};
        }

        char* data() noexcept {
            return fixed_detail::resolve_rel32<char>(&data_delta, data_delta);
        }

        const char* data() const noexcept {
            return fixed_detail::resolve_rel32_const<char>(&data_delta, data_delta);
        }

        std::uint32_t size() const noexcept { return size_; }
        std::uint32_t capacity() const noexcept { return capacity_; }
        bool empty() const noexcept { return size_ == 0; }

        std::string_view view() const noexcept {
            return data() ? std::string_view(data(), size_) : std::string_view{};
        }

        operator std::string_view() const noexcept { return view(); }

        const char* c_str() const noexcept {
            return data() ? data() : "";
        }

        void clear() noexcept {
            size_ = 0;
            if (auto* ptr = data()) ptr[0] = '\0';
        }

        void reserve(std::size_t requested) {
            ensure_bound();
            if (requested >= XBufferCore::MAX_PAYLOAD) throw XBadAlloc();
            auto new_cap = static_cast<std::uint32_t>(requested);
            if (new_cap <= capacity_) return;

            auto alloc = binding();
            auto* old_storage = data();
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, old_storage, static_cast<std::uint32_t>(new_cap + 1), alignof(char))) {
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
            ensure_bound();
            if (rhs.size() >= XBufferCore::MAX_PAYLOAD) throw XBadAlloc();
            if (rhs.empty()) { clear(); return; }
            const auto new_size = static_cast<std::uint32_t>(rhs.size());
            auto* old_data = data();
            if (new_size <= capacity_) {
                std::memmove(old_data, rhs.data(), rhs.size());
                old_data[new_size] = '\0';
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
                    alloc, old_data, static_cast<std::uint32_t>(new_size + 1), alignof(char))) {
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

        XFixedString& operator=(std::string_view rhs) {
            assign(rhs);
            return *this;
        }

        XFixedString& operator=(const char* rhs) {
            assign(rhs ? std::string_view(rhs) : std::string_view{});
            return *this;
        }

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

    private:
        void release() noexcept {
            auto* old_data = data();
            auto alloc = binding();
            std::uint32_t old_cap = capacity_;

            data_delta = 0;
            size_ = 0;
            capacity_ = 0;

            if (old_data && alloc.segment) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(old_cap + 1));
            }
        }

        void ensure_bound() const {
            if (!is_bound() || !arena()) {
                throw XException(
                    "XFixedString is not bound to an allocator");
            }
        }
    };

    template <typename T>
    struct alignas(8) XFixedVector {
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;
        using value_type = T;
        using size_type = std::uint32_t;
        using iterator = T*;
        using const_iterator = const T*;

        std::int32_t arena_delta = 0;
        std::int32_t data_delta = 0;
        std::uint32_t size_ = 0;
        std::uint32_t capacity_ = 0;

        XFixedVector() = default;
        explicit XFixedVector(allocator_type alloc) { bind(alloc); }
        explicit XFixedVector(XAllocatorStateV1* state) {
            bind(fixed_detail::allocator_ref_of(state));
        }
        ~XFixedVector() { release(); }
        XFixedVector(const XFixedVector& other, allocator_type alloc) : XFixedVector(alloc) {
            copy_from(other);
        }

        XFixedVector(const XFixedVector&) = delete;
        XFixedVector(XFixedVector&&) = delete;
        XFixedVector& operator=(const XFixedVector& other) {
            if (this == &other) return *this;
            if (!is_bound() && other.is_bound()) bind_like(other);
            ensure_bound();
            rebuild(other.size_, std::max(capacity_, other.size_), [&](T* dst, size_type i) {
                fixed_detail::transfer_construct(dst, other.data()[i], binding());
            });
            return *this;
        }
        XFixedVector& operator=(XFixedVector&& other) {
            if (this == &other) return *this;
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                if (is_bound() && other.is_bound()) {
                    const auto destination = binding();
                    // MV: binding() is {allocator_state(), arena()}, and for a state
                    // binding arena() == allocator_arena(allocator_state()). Equal
                    // non-null states therefore give the identical binding (same field
                    // read, no intervening write); otherwise compute it as before.
                    const auto source_state = other.allocator_state();
                    const auto source = source_state && source_state == destination.state
                        ? destination : other.binding();
                    if (fixed_detail::move_assignment_same_allocator(destination, source) &&
                        fixed_detail::move_assignment_head_in_arena(this, sizeof(*this), destination.segment) &&
                        fixed_detail::move_assignment_head_in_arena(&other, sizeof(other), source.segment) &&
                        !fixed_detail::move_assignment_owns_address_fast(*this, &other) &&
                        !fixed_detail::move_assignment_owns_address_fast(other, this)) {
                        std::int32_t incoming_delta = 0;
                        fixed_detail::set_rel32(incoming_delta, &data_delta, other.data());
                        const auto incoming_size = other.size_;
                        const auto incoming_capacity = other.capacity_;
                        // Out-of-line elements keep their addresses and bindings.
                        // Ancestor/descendant ownership has already been excluded.
                        // MV: release()'s empty-destination case inline (data()
                        // is null exactly when data_delta == 0, and release()
                        // then only clears the descriptor); otherwise unchanged.
                        if (data_delta == 0) {
                            size_ = 0;
                            capacity_ = 0;
                        } else {
                            release();
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

        void bind(allocator_type alloc) {
            if (alloc.state) {
                fixed_detail::set_tagged_rel32(arena_delta, &arena_delta, alloc.state, 0);
            } else {
                fixed_detail::set_tagged_rel32(
                    arena_delta, &arena_delta, alloc.segment,
                    fixed_detail::arena_binding_segment_tag);
            }
        }

        void bind_like(const XFixedVector& other) {
            if (auto* state = other.allocator_state()) {
                bind(fixed_detail::allocator_ref_of(state));
            } else {
                bind(fixed_detail::allocator_ref_of(other.arena()));
            }
        }

        bool is_bound() const noexcept { return arena_delta != 0; }

        XAllocatorStateV1* allocator_state() const noexcept {
            if (arena_delta == 0 || fixed_detail::arena_binding_is_segment(arena_delta)) {
                return nullptr;
            }
            return const_cast<XAllocatorStateV1*>(
                fixed_detail::resolve_rel32_const<XAllocatorStateV1>(&arena_delta, arena_delta));
        }

        allocator_type binding() const noexcept {
            return {allocator_state(), arena()};
        }

        XArena* arena() const noexcept {
            if (arena_delta == 0) return nullptr;
            if (fixed_detail::arena_binding_is_segment(arena_delta)) {
                return const_cast<XArena*>(
                    fixed_detail::resolve_rel32_const<XArena>(
                        &arena_delta, fixed_detail::clear_arena_binding_tag(arena_delta)));
            }
            return fixed_detail::allocator_arena(allocator_state());
        }

        stored_allocator_type get_stored_allocator() const noexcept {
            auto alloc = binding();
            return {alloc.state, alloc.segment};
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

        void clear() noexcept {
            fixed_detail::destroy_n(data(), size_);
            size_ = 0;
        }

        void reserve(std::size_t requested) {
            ensure_bound();
            auto new_cap = checked_count(requested);
            if (new_cap <= capacity_) return;
            if (fixed_detail::try_expand_dynamic_bytes(
                    binding(), data(), static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T))) {
                (void)detail::rebuild_dynamic_array<T>(data(), new_cap, size_);
                capacity_ = new_cap;
                return;
            }
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                if (new_cap > max_size()) throw XBadAlloc();
                auto alloc = binding();
                auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes(
                    alloc, static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T)), new_cap);
                auto* old = data();
                auto old_cap = capacity_;
                for (size_type i = 0; i < size_; ++i) {
                    fixed_detail::relocate_construct(fresh + i, old[i]);
                }
                fixed_detail::destroy_n(old, size_);
                fixed_detail::set_rel32(data_delta, &data_delta, fresh);
                capacity_ = new_cap;
                fixed_detail::retire_dynamic_bytes(alloc, old, static_cast<std::uint32_t>(old_cap * sizeof(T)));
            } else {
                rebuild(size_, new_cap, [&](T* dst, size_type i) {
                    fixed_detail::transfer_construct(dst, data()[i], binding());
                });
            }
        }

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
            ensure_bound();
            const auto bytes = static_cast<std::uint32_t>(count * sizeof(T));
            if (count <= capacity_) {
                std::memmove(data(), values.data(), bytes);
                size_ = count;
                return;
            }
            auto alloc = binding();
            auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes(
                alloc, bytes, alignof(T)), count);
            std::int32_t delta = 0;
            try {
                fixed_detail::set_rel32(delta, &data_delta, fresh);
            } catch (...) {
                fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
                throw;
            }
            // Read the complete source before publishing/retiring old storage.
            // Retirement is allowed to overwrite its first bytes as free-list
            // metadata. memmove also covers overlapping live-element ranges.
            std::memmove(fresh, values.data(), bytes);
            auto* old = data();
            const auto old_cap = capacity_;
            data_delta = delta;
            size_ = count;
            capacity_ = count;
            fixed_detail::retire_dynamic_bytes(
                alloc, old, static_cast<std::uint32_t>(old_cap * sizeof(T)));
        }

        void push_back(const T& value) {
            insert_construct(size_, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, binding());
            });
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
            ensure_bound();
            const auto original_arena_delta = arena_delta;
            const auto original_data_delta = data_delta;
            const auto original_capacity = capacity_;
            auto* const items = data();
            fixed_detail::StableEraseOwnerWitness witness(binding(), this, sizeof(*this));
            const auto decision_bytes = (std::size_t(original_size) + 7u) / 8u;
            auto decisions = std::make_unique<std::uint8_t[]>(decision_bytes);
            // Preparing host scratch can call new_handler. Recheck reservation
            // identity before touching the in-arena descriptor or old items.
            auto validate_target = [&] {
                witness.validate();
                if (arena_delta != original_arena_delta || data_delta != original_data_delta ||
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
            return removed;
        }

        void pop_back() {
            if (size_ == 0) return;
            fixed_detail::destroy_n(data() + (size_ - 1), 1);
            --size_;
        }

        T& emplace_back() {
            return *insert_construct(size_, [&](T* dst) {
                fixed_detail::default_construct(dst, binding());
            });
        }

        template <typename... Args>
            requires (sizeof...(Args) > 0)
        T& emplace_back(Args&&... args) {
            return *insert_construct(size_, [&](T* dst) {
                fixed_detail::construct_bound(dst, binding(), std::forward<Args>(args)...);
            });
        }

    protected:
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

        // Build in arena storage and publish only after all potentially throwing work.
        template <typename Construct>
        void rebuild(size_type count, size_type cap, Construct&& construct) {
            ensure_bound();
            if (cap > max_size() || count > cap) throw XBadAlloc();
            if (cap == 0) { clear(); return; }
            auto alloc = binding();
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

        template <typename Construct>
        iterator insert_construct(size_type index, Construct&& construct) {
            ensure_bound();
            if (index > size_ || size_ == max_size()) throw XBadAlloc();
            if (index == size_ && size_ < capacity_) {
                construct(data() + size_);
                ++size_;
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                // A native temporary is safe only for types without arena bindings.
                alignas(T) std::byte storage[sizeof(T)];
                auto* staged = reinterpret_cast<T*>(storage);
                construct(staged);
                if (size_ == capacity_) reserve(grown_capacity());
                std::memmove(data() + index + 1, data() + index, (size_ - index) * sizeof(T));
                std::memcpy(data() + index, staged, sizeof(T));
                ++size_;
            } else if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                with_staged(construct, [&](T& staged) {
                    if (size_ == capacity_) reserve(grown_capacity());
                    for (size_type i = size_; i > index; --i) {
                        fixed_detail::relocate_construct(data() + i, data()[i - 1]);
                        std::destroy_at(data() + i - 1);
                    }
                    fixed_detail::relocate_construct(data() + index, staged);
                    ++size_;
                });
            } else {
                rebuild(size_ + 1, size_ == capacity_ ? grown_capacity() : capacity_,
                    [&](T* dst, size_type i) {
                        if (i == index) construct(dst);
                        else fixed_detail::transfer_construct(dst, data()[i < index ? i : i - 1], binding());
                    });
            }
            return data() + index;
        }

        void erase_index(size_type index) {
            if (index >= size_) return;
            if (index + 1 == size_) { pop_back(); return; }
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(data() + index, data() + index + 1, (size_ - index - 1) * sizeof(T));
                --size_;
            } else if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                std::destroy_at(data() + index);
                for (size_type i = index; i + 1 < size_; ++i) {
                    fixed_detail::relocate_construct(data() + i, data()[i + 1]);
                    std::destroy_at(data() + i + 1);
                }
                --size_;
            } else {
                rebuild(size_ - 1, capacity_, [&](T* dst, size_type i) {
                    fixed_detail::transfer_construct(dst, data()[i < index ? i : i + 1], binding());
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
                rebuild(size_, capacity_, [&](T* dst, size_type i) {
                    if (i == index) construct(dst);
                    else fixed_detail::transfer_construct(dst, data()[i], binding());
                });
            }
        }

        template <typename Construct, typename Commit>
        void with_staged(Construct&& construct, Commit&& commit) {
            auto alloc = binding();
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
                size_ = count;
                return;
            }
            const auto old_size = size_;
            if (count <= capacity_) {
                std::uninitialized_value_construct_n(data() + old_size, count - old_size);
                size_ = count;
                return;
            }
            ensure_bound();
            auto alloc = binding();
            auto* old = data();
            const auto bytes = static_cast<std::uint32_t>(count * sizeof(T));
            if (fixed_detail::try_expand_dynamic_bytes(alloc, old, bytes, alignof(T))) {
                // VM preparation completed before publishing the enlarged
                // allocation. Required suffix initialization is nonthrowing.
                auto* expanded = detail::rebuild_dynamic_array<T>(old, count, old_size);
                std::uninitialized_value_construct_n(expanded + old_size, count - old_size);
                size_ = count;
                capacity_ = count;
                return;
            }
            resize_construct(count, [&](T* dst) {
                fixed_detail::default_construct(dst, binding());
            });
        }

        template <typename Construct>
        void resize_construct(size_type count, Construct&& construct) {
            if (count > max_size()) throw XBadAlloc();
            if (count <= size_) {
                while (size_ > count) pop_back();
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
                ensure_bound();
                auto alloc = binding();
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
                rebuild(count, count, [&](T* dst, size_type i) {
                    if (i < size_) fixed_detail::transfer_construct(dst, data()[i], binding());
                    else construct(dst);
                });
            }
        }

    private:
        void copy_from(const XFixedVector& other) {
            if (other.size_ == 0) return;
            reserve(other.size_);
            size_type constructed = 0;
            try {
                for (; constructed < other.size_; ++constructed) {
                    fixed_detail::transfer_construct(
                        data() + constructed, other.data()[constructed], binding());
                }
            } catch (...) {
                fixed_detail::destroy_n(data(), constructed);
                size_ = 0;
                throw;
            }
            size_ = other.size_;
        }

        void release() noexcept {
            T* old_data = data();
            auto alloc = binding();
            size_type old_size = size_;
            size_type old_cap = capacity_;

            data_delta = 0;
            size_ = 0;
            capacity_ = 0;

            if (old_data && alloc.segment) {
                fixed_detail::destroy_n(old_data, old_size);
                fixed_detail::retire_dynamic_bytes(
                    alloc, old_data, static_cast<std::uint32_t>(old_cap * sizeof(T)));
            }
        }

        void ensure_bound() const {
            if (!is_bound() || !arena()) {
                throw XException(
                    "XFixedVector is not bound to an allocator");
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
            return {Base::insert_construct(index, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, Base::binding());
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
            return {Base::insert_construct(index, [&](T* dst) {
                fixed_detail::construct_bound(dst, Base::binding(), std::forward<Arg>(arg));
            }), true};
        }

        template <typename... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            if constexpr (sizeof...(Args) == 1) {
                return insert(std::forward<Args>(args)...);
            } else {
                auto alloc = Base::binding();
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

            return {value_base::insert_construct(index, [&](value_type* dst) {
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
        }
    };

    static_assert(sizeof(XFixedString) == 16,
        "XFixedString ABI must stay frozen at 16 bytes");
    static_assert(sizeof(XFixedVector<int>) == 16,
        "XFixedVector ABI must stay frozen at 16 bytes");
    static_assert(sizeof(XFixedFlatSet<int>) == sizeof(XFixedVector<int>),
        "XFixedFlatSet ABI must stay frozen at 16 bytes");
    static_assert(sizeof(XFixedFlatMap<int, int>) == sizeof(XFixedVector<XKeyValue<int, int>>),
        "XFixedFlatMap ABI must stay frozen at 16 bytes");

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
        using Base::data;
        using Base::c_str;

        XString() = default;

        template <typename Alloc>
        explicit XString(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XString(const XString& other)
            : Base(other.binding()) {
            if (other.is_bound()) Base::operator=(other);
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
                fixed_detail::construct_bound(dst, Base::binding(), std::forward<Arg>(arg));
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
        using Base::pop_back;
        using Base::emplace_back;

        XVector() = default;

        template <typename Alloc>
        explicit XVector(const Alloc& alloc)
            : Base(fixed_detail::allocator_ref_of(alloc)) {}

        XVector(const XVector& other)
            : Base(other.binding()) {
            if (other.is_bound()) Base::operator=(other);
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
                Base::resize_construct(n, [&](T* dst) {
                    fixed_detail::default_construct(dst, Base::binding());
                });
            }
        }

        void resize(std::size_t count, const T& value) {
            auto n = Base::checked_count(count);
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, Base::binding());
            });
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void resize(std::size_t count, Arg&& arg) {
            auto n = Base::checked_count(count);
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::construct_bound(dst, Base::binding(), arg);
            });
        }

        void assign(std::size_t count, const T& value) {
            auto n = Base::checked_count(count);
            if (n == 0) { Base::clear(); return; }
            Base::rebuild(n, std::max(n, Base::capacity()), [&](T* dst, size_type) {
                fixed_detail::transfer_construct(dst, value, Base::binding());
            });
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void assign(std::size_t count, Arg&& arg) {
            auto n = Base::checked_count(count);
            if (n == 0) { Base::clear(); return; }
            Base::rebuild(n, std::max(n, Base::capacity()), [&](T* dst, size_type) {
                fixed_detail::construct_bound(dst, Base::binding(), arg);
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
            : Base(other.binding()) {
            if (other.is_bound()) Base::operator=(other);
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
            : Base(other.binding()) {
            if (other.is_bound()) Base::operator=(other);
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
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using ReflectedMemberType = [:type_of(member):];
            using MemberType = std::remove_cv_t<ReflectedMemberType>;
            return is_v1_wire_admitted_impl<MemberType>::value;
        }

        template <typename T, std::size_t N = 0>
        consteval bool reflected_members_wire_admitted_impl() {
            if constexpr (N >= reflected_member_count<T>()) {
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

        template <typename T>
        struct is_v1_wire_admitted_impl {
            using CleanT = std::remove_cv_t<T>;

            static constexpr bool value = []() consteval {
                if constexpr (!boost::typelayout::is_byte_copy_safe_v<CleanT>) {
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
                        reflected_base_count<CleanT>() == 0 &&
                        reflected_members_wire_admitted<CleanT>();
                } else {
                    return false;
                }
            }();
        };

        template <typename T>
        consteval bool v1_wire_admitted() {
            return is_v1_wire_admitted_impl<std::remove_cvref_t<T>>::value;
        }

        using WireValidationRangeIndex = std::pmr::map<std::size_t, std::size_t>;

        // Entries are nonempty, pairwise disjoint half-open arena-offset ranges.
        // A validated candidate can overlap only its successor or predecessor.
        // A successful check also retains the unique lower_bound insertion
        // position. Neither range set changes before the eventual insertion.
        inline bool wire_range_overlaps_with_hint(
                                        const WireValidationRangeIndex& ranges,
                                        std::size_t begin_offset,
                                        std::size_t end_offset,
                                        WireValidationRangeIndex::const_iterator& insertion_hint) {
            if (begin_offset == end_offset) {
                insertion_hint = ranges.end();
                return false;
            }
            insertion_hint = ranges.lower_bound(begin_offset);
            const auto next = insertion_hint;
            if (next != ranges.end() && next->first < end_offset) return true;
            if (next != ranges.begin()) {
                auto previous = next;
                --previous;
                if (previous->second > begin_offset) return true;
            }
            return false;
        }

        // Preserve the original three-argument helper and its function type.
        inline bool wire_range_overlaps(const WireValidationRangeIndex& ranges,
                                        std::size_t begin_offset,
                                        std::size_t end_offset) {
            auto insertion_hint = ranges.cend();
            return wire_range_overlaps_with_hint(
                ranges, begin_offset, end_offset, insertion_hint);
        }

        // XV5: byte-exact ownership with per-page word-any/word-full summaries.
        // Data words are read only when their any bit is set and full bit is not.
        class WireValidationByteIndex {
            static constexpr std::size_t page_bytes = 4096;
            static constexpr std::size_t word_bits = 64;
            static constexpr std::size_t words_per_page = 64;
            static constexpr std::size_t dense_words_per_page = 66;
            static constexpr std::uint64_t all_bits = (std::numeric_limits<std::uint64_t>::max)();
            struct alignas(8) Page {
                std::uint64_t words[64];
                std::uint64_t any, full;
                // Scalar data objects have lifetime, but intentionally no value
                // until written. The initialized metadata guards every read.
                Page() noexcept : any{}, full{} {}
            };
            static_assert(alignof(Page) >= 2);
            static_assert(sizeof(Page) == 528);
            static_assert(sizeof(std::uintptr_t) >= sizeof(Page*));
            static_assert(std::is_nothrow_default_constructible_v<Page>);
            std::pmr::memory_resource* resource_;
            bool allow_dense_;
            std::uint64_t* dense_{};
            std::size_t dense_pages_{};
            std::uintptr_t* directory_{};
            std::size_t directory_size_{}, extent_{}, registrations_{};
            std::size_t coverage_begin_{}, coverage_end_{};

            bool disjoint_coverage_bounds(std::size_t begin, std::size_t end) const noexcept {
                return registrations_ == 0 || end <= coverage_begin_ || begin >= coverage_end_;
            }

            static constexpr std::size_t page_count(std::size_t extent) noexcept {
                return extent / page_bytes + (extent % page_bytes != 0);
            }
            static std::uint64_t low_mask(unsigned bits) noexcept {
                return bits == 64 ? all_bits : (std::uint64_t{1} << bits) - 1;
            }
            static Page* page_pointer(std::uintptr_t value) noexcept {
                return reinterpret_cast<Page*>(value); // tags0/1 never reach here
            }
            std::uintptr_t tail_entry(std::size_t page) const noexcept {
                if (page < dense_pages_ || page - dense_pages_ >= directory_size_) return 0;
                return directory_[page - dense_pages_];
            }
            struct View {
                const std::uint64_t* words;
                std::uint64_t any, full;
            };
            View page_view(std::size_t page) const noexcept {
                if (page < dense_pages_) {
                    const auto* control = dense_ + dense_pages_ * words_per_page + page * 2;
                    return {dense_ + page * words_per_page, control[0], control[1]};
                }
                const auto entry = tail_entry(page);
                if (entry == 0) return {nullptr, 0, 0};
                if (entry == 1) return {nullptr, all_bits, all_bits};
                const auto* p = page_pointer(entry);
                return {p->words, p->any, p->full};
            }
            static std::uint64_t read_word(const View& view, std::size_t word) noexcept {
                const auto bit = std::uint64_t{1} << word;
                if (!(view.any & bit)) {
                    /* XV5: empty-word-synthesized */
                    return 0;
                }
                if (view.full & bit) {
                    /* XV5: full-word-synthesized */
                    return all_bits;
                }
                /* XV5: initialized-data-word-read */
                return view.words[word];
            }
            struct Span {
                std::size_t first, last;
                std::uint64_t first_mask, last_mask, whole;
            };
            static Span page_span(std::size_t lo, std::size_t hi) noexcept {
                const auto first = lo / word_bits, last = (hi - 1) / word_bits;
                const auto left = unsigned(lo % word_bits);
                const auto right = unsigned((hi - 1) % word_bits + 1);
                const auto a = first == last ? low_mask(right) & ~low_mask(left) : ~low_mask(left);
                const auto z = first == last ? 0 : low_mask(right);
                auto whole = low_mask(unsigned(last + 1)) & ~low_mask(unsigned(first));
                if (a != all_bits) whole &= ~(std::uint64_t{1} << first);
                if (first != last && z != all_bits) whole &= ~(std::uint64_t{1} << last);
                return {first, last, a, z, whole};
            }
            static bool page_overlaps(const View& view, const Span& span) noexcept {
                /* XV5: page-summary-query */
                if (view.any & span.whole) return true;
                if (span.first_mask != all_bits &&
                    (read_word(view, span.first) & span.first_mask)) return true;
                if (span.first != span.last && span.last_mask != all_bits &&
                    (read_word(view, span.last) & span.last_mask)) return true;
                return false;
            }
            static void mark_boundary(std::uint64_t* words, std::uint64_t& any,
                                      std::uint64_t& full, std::size_t word,
                                      std::uint64_t mask,
                                      const std::uint64_t* cached_word = nullptr) noexcept {
                const auto bit = std::uint64_t{1} << word;
                const auto value = (cached_word ? *cached_word : read_word({words, any, full}, word)) | mask;
                if (value == all_bits) {
                    full |= bit;
                    /* XV5: boundary-promoted-full */
                } else {
                    words[word] = value;
                    /* XV5: partial-data-word-written */
                }
                any |= bit;
            }
            static void mark_page(std::uint64_t* words, std::uint64_t& any,
                                  std::uint64_t& full, const Span& span,
                                  const std::uint64_t* cached_first = nullptr,
                                  const std::uint64_t* cached_last = nullptr) noexcept {
                // All whole words, including a full page, need only summaries.
                any |= span.whole;
                full |= span.whole;
                /* XV5: whole-word-mask-marked */
                if (span.first_mask != all_bits)
                    mark_boundary(words, any, full, span.first, span.first_mask, cached_first);
                if (span.first != span.last && span.last_mask != all_bits)
                    mark_boundary(words, any, full, span.last, span.last_mask, cached_last);
            }
            Page* allocate_page() {
                std::pmr::polymorphic_allocator<Page> allocator(resource_);
                auto* p = allocator.allocate(1);
                std::construct_at(p);
                /* XV5: page-prepared */
                return p;
            }
            void release_page(Page* p) noexcept {
                if (!p) return;
                std::destroy_at(p);
                std::pmr::polymorphic_allocator<Page>(resource_).deallocate(p, 1);
                /* XV5: page-released */
            }
            void release_directory(std::uintptr_t* p, std::size_t n) noexcept {
                if (!p) return;
                std::destroy_n(p, n);
                std::pmr::polymorphic_allocator<std::uintptr_t>(resource_).deallocate(p, n);
                /* XV5: directory-released */
            }
            void release_dense(std::uint64_t* p, std::size_t pages) noexcept {
                if (!p) return;
                const auto n = pages * dense_words_per_page;
                std::destroy_n(p, n);
                std::pmr::polymorphic_allocator<std::uint64_t>(resource_).deallocate(p, n);
                /* XV5: dense-released */
            }
        public:
            // Preserve the old per-index hard logical dense-byte budget rather
            // than silently increasing it for the additional word summaries.
            static constexpr std::size_t dense_byte_budget = 8u * 1024u * 1024u + 2048u;
            static constexpr std::size_t dense_extent_limit =
                (dense_byte_budget / (dense_words_per_page * sizeof(std::uint64_t))) * page_bytes;
            explicit WireValidationByteIndex(std::pmr::memory_resource* resource =
                    std::pmr::new_delete_resource(), bool allow_dense = true) noexcept
                : resource_(resource), allow_dense_(allow_dense) {}
            WireValidationByteIndex(const WireValidationByteIndex&) = delete;
            WireValidationByteIndex& operator=(const WireValidationByteIndex&) = delete;
            WireValidationByteIndex(WireValidationByteIndex&&) = delete;
            WireValidationByteIndex& operator=(WireValidationByteIndex&&) = delete;
            ~WireValidationByteIndex() {
                for (std::size_t i = 0; i < directory_size_; ++i)
                    if (directory_[i] > 1) release_page(page_pointer(directory_[i]));
                release_directory(directory_, directory_size_);
                release_dense(dense_, dense_pages_);
            }
            std::size_t size() const noexcept { return registrations_; }
            bool empty() const noexcept { return registrations_ == 0; }
            std::size_t extent() const noexcept { return extent_; }
            std::pair<std::size_t, std::size_t> coverage_bounds() const noexcept {
                return {coverage_begin_, coverage_end_};
            }
            std::size_t directory_entries() const noexcept { return directory_size_; }
            std::size_t dense_prefix_bytes() const noexcept { return dense_pages_ * page_bytes; }
            std::size_t dense_requested_bytes() const noexcept { return dense_pages_ * dense_words_per_page * 8; }
            std::pmr::memory_resource* resource() const noexcept { return resource_; }
            std::size_t allocated_pages() const noexcept {
                std::size_t result = 0;
                for (std::size_t i = 0; i < directory_size_; ++i) result += directory_[i] > 1;
                return result;
            }
            std::uint64_t word_at(std::size_t word) const noexcept {
                if (word >= extent_ / word_bits + (extent_ % word_bits != 0)) return 0;
                return read_word(page_view(word / words_per_page), word % words_per_page);
            }
            bool overlaps(std::size_t begin, std::size_t end) const noexcept {
                /* XV5: standalone-query */
                if (begin >= end || begin >= extent_) return false;
                end = std::min(end, extent_);
                const auto first = begin / page_bytes, last = (end - 1) / page_bytes;
                for (auto page = first;; ++page) {
                    /* XV5: standalone-page-tested */
                    const auto view = page_view(page);
                    if (view.any) {
                        const auto lo = page == first ? begin % page_bytes : 0;
                        const auto hi = page == last ? end - page * page_bytes : page_bytes;
                        if (page_overlaps(view, page_span(lo, hi))) return true;
                    }
                    if (page == last) break;
                }
                return false;
            }
            //0 none,1 owned,2 free. A free hit never prevents a later owned hit.
            static unsigned overlap_priority(const WireValidationByteIndex& owned,
                                             const WireValidationByteIndex& free,
                                             std::size_t begin, std::size_t end) noexcept {
                /* XV5: paired-query */
                const auto limit = std::max(owned.extent_, free.extent_);
                if (begin >= end || begin >= limit) return 0;
                end = std::min(end, limit);
                /* XV5: coverage-bounds-query */
                if (owned.disjoint_coverage_bounds(begin, end) &&
                    free.disjoint_coverage_bounds(begin, end)) {
                    /* XV5: coverage-bounds-fast-return */
                    return 0;
                }
                const auto first = begin / page_bytes, last = (end - 1) / page_bytes;
                bool free_hit = false;
                for (auto page = first;; ++page) {
                    /* XV5: paired-page-tested */
                    const auto live = owned.page_view(page);
                    const auto dead = free_hit ? View{nullptr, 0, 0} : free.page_view(page);
                    if (live.any || dead.any) {
                        const auto lo = page == first ? begin % page_bytes : 0;
                        const auto hi = page == last ? end - page * page_bytes : page_bytes;
                        const auto span = page_span(lo, hi);
                        if (live.any && page_overlaps(live, span)) return 1;
                        if (dead.any && page_overlaps(dead, span)) free_hit = true;
                    }
                    if (page == last) break;
                }
                return free_hit ? 2 : 0;
            }
            // XV6 draft: prepared dense, one-page registrations need no host
            // allocation. Check both sets before any mark and retain the target
            // boundary values, avoiding the second page walk and data reread.
            // false means the caller must use the unchanged general path.
            static bool try_insert_dense_single_page(
                    WireValidationByteIndex& owned,
                    WireValidationByteIndex& free,
                    bool incoming_free, std::size_t begin, std::size_t end,
                    std::size_t validated_extent, unsigned& overlap) noexcept {
                auto& target = incoming_free ? free : owned;
                if (begin >= end || end > validated_extent ||
                    validated_extent > target.extent_ || !target.dense_)
                    return false;
                const auto page = begin / page_bytes;
                if (page != (end - 1) / page_bytes || page >= target.dense_pages_)
                    return false;
                // XV8: the existing prepared/dense/page/extent guards above
                // already hold. One arena byte is one occupancy bit.
                if (begin / word_bits == (end - 1) / word_bits) {
                    /* XV8: single-word-probe */
                    overlap = 0;
                    const auto word = (begin / word_bits) % words_per_page;
                    const auto bit = std::uint64_t{1} << word;
                    const auto mask = low_mask(unsigned((end - 1) % word_bits + 1)) &
                                      ~low_mask(unsigned(begin % word_bits));
                    const bool bounds_disjoint = owned.disjoint_coverage_bounds(begin, end) &&
                                                 free.disjoint_coverage_bounds(begin, end);
                    std::uint64_t old_target = 0;
                    if (mask == all_bits) {
                        // There are no neighboring bits to preserve. A whole
                        // word uses only initialized summary bits, just as the
                        // existing general path: never read its data storage.
                        /* XV8: single-whole-word */
                        if (!bounds_disjoint) {
                            if (owned.page_view(page).any & bit) {
                                overlap = 1;
                                return true;
                            }
                            if (free.page_view(page).any & bit) {
                                overlap = 2;
                                return true;
                            }
                        }
                    } else {
                        /* XV8: single-partial-word */
                        if (bounds_disjoint) {
                            // Bounds exclude requested bytes, not other bytes
                            // in this word. Preserve those exact neighbors.
                            old_target = read_word(target.page_view(page), word);
                        } else {
                            const auto live = read_word(owned.page_view(page), word);
                            if (live & mask) {
                                overlap = 1;
                                return true;
                            }
                            const auto dead = read_word(free.page_view(page), word);
                            if (dead & mask) {
                                overlap = 2;
                                return true;
                            }
                            old_target = incoming_free ? dead : live;
                        }
                    }
                    // Both complete conflict checks precede this no-throw,
                    // allocation-free commit. read_word guarded every data read.
                    const auto value = old_target | mask;
                    auto* control = target.dense_ + target.dense_pages_ * words_per_page + page * 2;
                    if (value == all_bits) control[1] |= bit;
                    else target.dense_[page * words_per_page + word] = value;
                    control[0] |= bit;
                    target.coverage_begin_ = target.registrations_ ?
                        std::min(target.coverage_begin_, begin) : begin;
                    target.coverage_end_ = std::max(target.coverage_end_, end);
                    ++target.registrations_;
                    /* XV8: single-word-committed */
                    return true;
                }
                /* XV6: dense-single-page-probe */
                overlap = 0;
                const auto span = page_span(begin % page_bytes,
                                            end - page * page_bytes);
                const auto target_view = target.page_view(page);
                std::uint64_t first_value{}, last_value{};
                bool first_loaded = false, last_loaded = false;
                auto first_word = [&]() noexcept {
                    if (!first_loaded) {
                        first_value = read_word(target_view, span.first);
                        first_loaded = true;
                    }
                    return first_value;
                };
                auto last_word = [&]() noexcept {
                    if (!last_loaded) {
                        last_value = read_word(target_view, span.last);
                        last_loaded = true;
                    }
                    return last_value;
                };
                auto target_overlaps = [&]() noexcept {
                    /* XV6: cached-page-summary-query */
                    if (target_view.any & span.whole) return true;
                    if (span.first_mask != all_bits &&
                        (first_word() & span.first_mask)) return true;
                    return span.first != span.last && span.last_mask != all_bits &&
                           (last_word() & span.last_mask);
                };
                /* XV6: dense-coverage-bounds-query */
                if (owned.disjoint_coverage_bounds(begin, end) &&
                    free.disjoint_coverage_bounds(begin, end)) {
                    /* XV6: dense-coverage-bounds-fast-return */
                } else {
                    /* XV6: dense-paired-page-tested */
                    // The complete range is on this one page: checking owned
                    // before free preserves the global first-error priority.
                    const auto live = incoming_free ? owned.page_view(page) : target_view;
                    if (live.any && (incoming_free ? page_overlaps(live, span) : target_overlaps())) {
                        overlap = 1;
                        /* XV6: dense-single-page-rejected */
                        return true;
                    }
                    const auto dead = incoming_free ? target_view : free.page_view(page);
                    if (dead.any && (incoming_free ? target_overlaps() : page_overlaps(dead, span))) {
                        overlap = 2;
                        /* XV6: dense-single-page-free-rejected */
                        return true;
                    }
                }
                // Preserve bits outside the requested masks even when the
                // coverage envelope proved the requested bytes disjoint.
                if (span.first_mask != all_bits) first_word();
                if (span.first != span.last && span.last_mask != all_bits) last_word();
                auto* control = target.dense_ + target.dense_pages_ * words_per_page + page * 2;
                mark_page(target.dense_ + page * words_per_page,
                          control[0], control[1], span,
                          first_loaded ? &first_value : nullptr,
                          last_loaded ? &last_value : nullptr);
                target.coverage_begin_ = target.registrations_ ?
                    std::min(target.coverage_begin_, begin) : begin;
                target.coverage_end_ = std::max(target.coverage_end_, end);
                ++target.registrations_;
                /* XV6: dense-single-page-committed */
                return true;
            }

            // Checks precede all preparation. No mark, count or extent update
            // occurs until every needed allocation and object lifetime exists.
            void insert_disjoint(std::size_t begin, std::size_t end,
                                 std::size_t validated_extent) {
                if (begin >= end || end > validated_extent)
                    throw XException("wire validation bitmap received invalid checked range");
                const auto requested_extent = std::max(extent_, validated_extent);
                const bool make_dense = !dense_ && extent_ == 0 && allow_dense_ &&
                                        requested_extent <= dense_extent_limit;
                const auto prefix_pages = make_dense ? page_count(requested_extent) : dense_pages_;
                const auto total_pages = page_count(requested_extent);
                const auto needed = total_pages - prefix_pages;
                if (needed > (std::numeric_limits<std::size_t>::max)() / sizeof(std::uintptr_t))
                    throw std::bad_alloc();
                const auto first = begin / page_bytes, last = (end - 1) / page_bytes;
                const auto first_lo = begin % page_bytes, last_hi = end - last * page_bytes;
                const auto tail_first = std::max(first, prefix_pages);
                const auto tail_lo = tail_first == first ? first_lo : 0;
                const bool has_tail = last >= prefix_pages;
                const bool first_partial = has_tail && (tail_first == last ?
                    (tail_lo != 0 || last_hi != page_bytes) : tail_lo != 0);
                const bool last_partial = has_tail && tail_first != last && last_hi != page_bytes;
                std::uint64_t* fresh_dense = nullptr;
                std::uintptr_t* fresh_directory = nullptr;
                auto* slots = directory_;
                Page* fresh_first = nullptr;
                Page* fresh_last = nullptr;
                try {
                    if (make_dense) {
                        const auto count = prefix_pages * dense_words_per_page;
                        fresh_dense = std::pmr::polymorphic_allocator<std::uint64_t>(resource_).allocate(count);
                        // Starts every scalar lifetime, without value-initializing
                        // data. Only initialized summary bits authorize a read.
                        std::uninitialized_default_construct_n(fresh_dense, count);
                        std::fill_n(fresh_dense + prefix_pages * words_per_page, prefix_pages * 2, std::uint64_t{0});
                        /* XV5: dense-prepared */
                    }
                    if (needed > directory_size_) {
                        fresh_directory = std::pmr::polymorphic_allocator<std::uintptr_t>(resource_).allocate(needed);
                        std::uninitialized_value_construct_n(fresh_directory, needed);
                        for (std::size_t i = 0; i < directory_size_; ++i) fresh_directory[i] = directory_[i];
                        slots = fresh_directory;
                        /* XV5: directory-prepared */
                    }
                    if (first_partial && slots[tail_first - prefix_pages] == 0) fresh_first = allocate_page();
                    if (last_partial && slots[last - prefix_pages] == 0) fresh_last = allocate_page();
                } catch (...) {
                    release_page(fresh_last);
                    release_page(fresh_first);
                    release_directory(fresh_directory, needed);
                    release_dense(fresh_dense, prefix_pages);
                    throw;
                }
                auto* dense_words = fresh_dense ? fresh_dense : dense_;
                if (fresh_first) slots[tail_first - prefix_pages] = reinterpret_cast<std::uintptr_t>(fresh_first);
                if (fresh_last) slots[last - prefix_pages] = reinterpret_cast<std::uintptr_t>(fresh_last);
                for (auto page = first;; ++page) {
                    const auto lo = page == first ? first_lo : 0;
                    const auto hi = page == last ? last_hi : page_bytes;
                    if (page < prefix_pages) {
                        auto* control = dense_words + prefix_pages * words_per_page + page * 2;
                        mark_page(dense_words + page * words_per_page, control[0], control[1], page_span(lo, hi));
                    } else if (lo == 0 && hi == page_bytes) {
                        auto& entry = slots[page - prefix_pages];
                        if (entry > 1) {
                            // Retain a previously allocated Page until destruction.
                            // Normal disjoint registrations never need this case.
                            auto* p = page_pointer(entry);p->any = p->full = all_bits;
                        } else entry = 1;
                        /* XV5: full-page-tagged */
                    } else {
                        auto* p = page_pointer(slots[page - prefix_pages]);
                        mark_page(p->words, p->any, p->full, page_span(lo, hi));
                    }
                    if (page == last) break;
                }
                if (fresh_dense) { dense_ = fresh_dense; dense_pages_ = prefix_pages; }
                if (fresh_directory) {
                    release_directory(directory_, directory_size_);
                    directory_ = fresh_directory;
                    directory_size_ = needed;
                }
                extent_ = requested_extent;
                // Only successful commit publishes covered bounds; scratch
                // preparation failure must not expand this observed envelope.
                coverage_begin_ = registrations_ ? std::min(coverage_begin_, begin) : begin;
                coverage_end_ = std::max(coverage_end_, end);
                ++registrations_;
            }
        };

        inline bool wire_range_overlaps(const WireValidationByteIndex& ranges,
                                        std::size_t begin_offset,
                                        std::size_t end_offset) noexcept {
            return ranges.overlaps(begin_offset, end_offset);
        }

        struct WireValidationContext {
            const char* base{};
            const char* end{};
            XArena* segment{};
            const XAllocatorStateV1* allocator_state{};
            // Host-only, lazy storage; never sized from untrusted wire counts.
            // Declaration order keeps the resource alive until both indexes die.
            std::pmr::monotonic_buffer_resource range_resource{
                std::size_t{4096}, std::pmr::new_delete_resource()};
            std::pmr::monotonic_buffer_resource free_range_resource{
                std::size_t{4096}, std::pmr::new_delete_resource()};
            WireValidationByteIndex owned_ranges{&range_resource};
            WireValidationByteIndex free_ranges{&free_range_resource};
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
                throw XException(what);
            }
            auto begin = reinterpret_cast<std::uintptr_t>(ptr);
            auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            auto end = reinterpret_cast<std::uintptr_t>(ctx.end);
            if (begin < base || begin > end || bytes > end - begin) {
                throw XException(what);
            }
            if ((reinterpret_cast<std::uintptr_t>(ptr) % alignment) != 0) {
                throw XException(what);
            }
        }

        inline void register_owned_range(const void* ptr, std::size_t bytes,
                                         WireValidationContext& ctx,
                                         const char* what) {
            if (bytes == 0 || !ptr) return;
            validate_storage_range(ptr, bytes, 1, ctx, what);
            const auto begin_offset = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ptr) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            // Validated bounds imply this sum is within the arena extent.
            const auto end_offset = begin_offset + bytes;
            const auto extent = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ctx.end) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            unsigned overlap = 0;
            const bool handled = WireValidationByteIndex::try_insert_dense_single_page(
                ctx.owned_ranges, ctx.free_ranges, false,
                begin_offset, end_offset, extent, overlap);
            if (!handled) overlap = WireValidationByteIndex::overlap_priority(
                ctx.owned_ranges, ctx.free_ranges, begin_offset, end_offset);
            if (overlap == 1) {
                throw XException(
                    "wire validation detected overlapping dynamic ranges");
            }
            if (overlap == 2) {
                throw XException("wire validation detected live/free overlap");
            }
            if (!handled) ctx.owned_ranges.insert_disjoint(begin_offset, end_offset, extent);
            /* XBV3: owned-registered */
        }

        inline void register_free_range(const void* ptr, std::size_t bytes,
                                        WireValidationContext& ctx,
                                        const char* what) {
            if (bytes == 0 || !ptr) return;
            validate_storage_range(ptr, bytes, 1, ctx, what);
            const auto begin_offset = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ptr) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            const auto end_offset = begin_offset + bytes;
            const auto extent = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ctx.end) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            unsigned overlap = 0;
            const bool handled = WireValidationByteIndex::try_insert_dense_single_page(
                ctx.owned_ranges, ctx.free_ranges, true,
                begin_offset, end_offset, extent, overlap);
            if (!handled) overlap = WireValidationByteIndex::overlap_priority(
                ctx.owned_ranges, ctx.free_ranges, begin_offset, end_offset);
            if (overlap == 1) {
                throw XException(
                    "wire validation detected free-list overlap with live ranges");
            }
            if (overlap == 2) {
                throw XException(
                    "wire validation detected overlapping free-list ranges");
            }
            if (!handled) ctx.free_ranges.insert_disjoint(begin_offset, end_offset, extent);
            /* XBV3: free-registered */
        }

        XOFFSET_DETAIL_V4_INLINE const char* checked_relative(const std::int32_t& delta,
                                            const WireValidationContext& ctx) {
            auto address = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta;
            auto* ptr = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
            validate_storage_range(ptr, 1, 1, ctx, "wire validation: relative pointer out of bounds");
            return ptr; // W9: flat (validated range), no canonical lookup
        }

        XOFFSET_DETAIL_V4_INLINE void validate_binding(const std::int32_t& delta, const WireValidationContext& ctx) {
            if (delta == 0 || checked_relative(delta, ctx) != reinterpret_cast<const char*>(ctx.allocator_state)) {
                throw XException("wire validation: invalid allocator binding");
            }
        }

#undef XOFFSET_DETAIL_V4_INLINE

        inline void register_allocation(const void* ptr, std::size_t bytes,
                                         std::size_t alignment, WireValidationContext& ctx,
                                         bool free = false) {
            using Header = XBufferCore::AllocationHeader;
            validate_storage_range(ptr, bytes, alignment, ctx, "wire validation: allocation out of bounds");
            auto address = reinterpret_cast<std::uintptr_t>(ptr);
            auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            if (address - base < sizeof(Header)) throw XException("wire validation: missing allocation header");
            auto* header_ptr = reinterpret_cast<const char*>(address - sizeof(Header));
            validate_storage_range(header_ptr, sizeof(Header), alignof(Header), ctx,
                                   "wire validation: invalid allocation header address");
            Header header;
            std::memcpy(&header, static_cast<const void*>(header_ptr), sizeof(header)); // W9: flat
            if (header.user_offset != address - base || header.user_bytes < bytes ||
                header.total_bytes != std::uint64_t(header.user_bytes) + sizeof(Header) ||
                header.alignment < alignment || header.alignment > XBufferCore::MAX_PAYLOAD ||
                (header.alignment & (header.alignment - 1)) != 0 ||
                (address & (header.alignment - 1u)) != 0) {
                throw XException("wire validation: invalid allocation metadata");
            }
            if (free) register_free_range(header_ptr, header.total_bytes, ctx, "wire validation: invalid free allocation");
            else register_owned_range(header_ptr, header.total_bytes, ctx, "wire validation: invalid live allocation");
        }

        template <typename T>
        void validate_wire_value(const T& value, WireValidationContext& ctx);
        // True means that visiting a value can reject wire bytes or validate a
        // binding/range. False is permitted only for an admitted representation
        // whose old recursive validator has no observable checks at all.
        template <typename T> struct wire_value_needs_validation;

        template <typename T, std::size_t N = 0>
        consteval bool reflected_member_needs_validation() {
            if constexpr (N >= reflected_member_count<T>()) {
                return false;
            } else {
                using namespace std::meta;
                constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[N];
                using Member = [:type_of(member):];
                return wire_value_needs_validation<Member>::value ||
                    reflected_member_needs_validation<T, N + 1>();
            }
        }
        template <typename T>
        struct wire_value_needs_validation {
            using CleanT = std::remove_cv_t<T>;
            static constexpr bool value = []() consteval {
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
            }();
        };
        template <typename T>
        inline constexpr bool wire_value_needs_validation_v = wire_value_needs_validation<T>::value;



        inline void validate_wire_value(const XFixedString& value,
                                        WireValidationContext& ctx) {
            validate_binding(value.arena_delta, ctx);
            if (value.size_ > value.capacity_ || value.capacity_ >= XBufferCore::MAX_PAYLOAD) {
                throw XException(
                    "wire validation failed: XFixedString size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    throw XException(
                        "wire validation failed: empty XFixedString has invalid payload");
                }
                return;
            }

            auto* data = checked_relative(value.data_delta, ctx);
            validate_storage_range(data, static_cast<std::size_t>(value.capacity_) + 1,
                                   alignof(char), ctx,
                                   "wire validation failed: XFixedString payload out of bounds");
            register_allocation(data, fixed_detail::round_up_to_alignment(value.capacity_ + 1, 8), 8, ctx);
            if (data[value.size_] != '\0') {
                throw XException(
                    "wire validation failed: XFixedString missing terminator");
            }
        }

        template <typename T>
        void validate_wire_value(const XFixedVector<T>& value,
                                 WireValidationContext& ctx) {
            validate_binding(value.arena_delta, ctx);
            if (value.size_ > value.capacity_ ||
                value.capacity_ > (XBufferCore::MAX_PAYLOAD - 8u) / sizeof(T)) {
                throw XException(
                    "wire validation failed: XFixedVector size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    throw XException(
                        "wire validation failed: empty XFixedVector has invalid payload");
                }
                return;
            }

            auto* data = reinterpret_cast<const T*>(checked_relative(value.data_delta, ctx));
            auto bytes = static_cast<std::size_t>(value.capacity_) * sizeof(T);
            validate_storage_range(data, bytes, alignof(T), ctx,
                                   "wire validation failed: XFixedVector payload out of bounds");
            register_allocation(data, (bytes + 7u) & ~std::size_t(7u), std::max<std::size_t>(alignof(T), 8), ctx);
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
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            validate_wire_value(static_cast<const BaseType&>(value), ctx);
        }

        template <typename T, std::size_t N>
        void validate_member_at(const T& value, WireValidationContext& ctx) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using Member = [:type_of(member):];
            if constexpr (wire_value_needs_validation_v<Member>)
                validate_wire_value(value.[:member:], ctx);
        }

        template <typename T>
        void validate_wire_value(const T& value, WireValidationContext& ctx) {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<CleanT, bool>) {
                unsigned char bits;
                std::memcpy(&bits, &value, 1);
                if (bits > 1) throw XException("wire validation: invalid bool representation");
            } else if constexpr (std::is_array_v<CleanT>) {
                if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                    for (const auto& elem : value) validate_wire_value(elem, ctx);
            } else if constexpr (is_fixed_string_v<CleanT>) {
                validate_wire_value(static_cast<const XFixedString&>(value), ctx);
            } else if constexpr (is_fixed_flat_set_v<CleanT>) {
                validate_wire_value(
                    static_cast<const XFixedVector<typename CleanT::value_type>&>(value), ctx);
                for (std::uint32_t i = 1; i < value.size(); ++i) {
                    if (!fixed_detail::ordered_less(value.data()[i - 1], value.data()[i]))
                        throw XException("wire validation: set keys not strictly ordered");
                }
            } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                validate_wire_value(
                    static_cast<const XFixedVector<typename CleanT::value_type>&>(value), ctx);
                for (std::uint32_t i = 1; i < value.size(); ++i) {
                    if (!fixed_detail::ordered_less(value.data()[i - 1].first, value.data()[i].first))
                        throw XException("wire validation: map keys not strictly ordered");
                }
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

        inline void validate_allocator_state(WireValidationContext& ctx) {
            auto* state = ctx.allocator_state;
            if (!state) {
                throw XException(
                    "wire validation failed: allocator state missing");
            }
            if (checked_relative(state->segment_delta, ctx) != reinterpret_cast<const char*>(ctx.segment) ||
                state->reserved0 != 0) {
                throw XException(
                    "wire validation failed: allocator state bound to wrong arena");
            }

            auto arena_limit = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ctx.end) - reinterpret_cast<std::uintptr_t>(state));
            if (state->arena_begin < sizeof(XAllocatorStateV1) ||
                state->arena_begin > state->arena_end ||
                state->arena_end > arena_limit) {
                throw XException(
                    "wire validation failed: allocator state arena bounds invalid");
            }

            std::uint64_t total_free_bytes = 0;
            for (std::uint32_t i = 0; i < fixed_detail::freelist_class_count; ++i) {
                std::uint32_t offset = state->freelist_head[i];
                while (offset != 0) {
                    if (offset < state->arena_begin || offset > state->arena_end ||
                        sizeof(fixed_detail::FreeBlockHeader) > state->arena_end - offset) {
                        throw XException("wire validation: free-list offset out of bounds");
                    }
                    auto* block = fixed_detail::freelist_block_from_offset(state, offset);
                    validate_storage_range(block, sizeof(*block), 8, ctx,
                                           "wire validation: invalid free-list header");
                    if (block->block_bytes < sizeof(fixed_detail::FreeBlockHeader) ||
                        block->block_bytes % 8 != 0 || block->block_bytes > state->arena_end - offset) {
                        throw XException(
                            "wire validation failed: invalid free-list block");
                    }
                    auto class_index = fixed_detail::freelist_size_class(block->block_bytes);
                    if (class_index != i) {
                        throw XException(
                            "wire validation failed: free-list block in wrong size class");
                    }
                    register_allocation(block, block->block_bytes, 8, ctx, true);
                    total_free_bytes += block->block_bytes;
                    offset = block->next_offset;
                }
            }

            if (total_free_bytes != state->free_bytes) {
                throw XException(
                    "wire validation failed: allocator state free byte count mismatch");
            }
        }

        template <typename T>
        void verify_structural_wire_graph(const T& root, XBufferCore& xbuf,
                                          std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
            const auto used = xbuf.arena()->used_end;
            // W9: flat end for every storage kind. The extent check that R1+M2
            // applied only to canonical (receive) storage now applies to all;
            // it cannot fire for a valid ordinary arena (used_end <= size is an
            // allocator/load invariant), so it only turns a corrupted extent
            // into this error instead of an out-of-bounds validation context.
            if (used > xbuf.segment_size())
                throw XException("wire validation: payload extent out of bounds");
            const char* validated_end = static_cast<const char*>(xbuf.get_address()) + used;
            WireValidationContext ctx{
                .base = static_cast<const char*>(xbuf.get_address()),
                .end = validated_end,
                .segment = xbuf.arena(),
                .allocator_state = xbuf.template find_allocator_storage<XAllocatorStateV1>(),
                .range_resource{std::size_t{4096}, upstream},
                .free_range_resource{std::size_t{4096}, upstream}
            };
            if (ctx.segment->flags != 0 || ctx.segment->reserved0 != 0) {
                throw XException("wire validation: unsupported arena header flags");
            }
            register_owned_range(ctx.base, sizeof(XArena), ctx, "wire validation: invalid arena header");
            register_allocation(ctx.allocator_state, sizeof(XAllocatorStateV1), alignof(XAllocatorStateV1), ctx);
            register_allocation(&root, sizeof(T), alignof(T), ctx);
            // Validate the allocator before following any container binding.
            validate_allocator_state(ctx);
            validate_wire_value(root, ctx);
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
                return move_assignment_contains(value.data(),
                    value.capacity_ ? std::size_t(value.capacity_) + 1u : 0u, address);
            } else if constexpr (std::is_trivially_copyable_v<typename CleanT::value_type>) {
                return move_assignment_contains(value.data(),
                    std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address);
            } else {
                return move_assignment_owns_address(value, address);
            }
        }

        template <typename T, std::size_t I>
        consteval bool member_can_relocate() {
            constexpr auto member = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())[I];
            if constexpr (std::meta::is_bit_field(member)) return false;
            else {
                using Member = [:std::meta::type_of(member):];
                return can_relocate_without_allocation<Member>();
            }
        }

        template <typename T, std::size_t I>
        consteval bool base_can_relocate() {
            constexpr auto base = std::meta::bases_of(^^T, std::meta::access_context::unchecked())[I];
            using Base = [:std::meta::type_of(base):];
            return can_relocate_without_allocation<Base>();
        }

        template <typename T>
        consteval bool can_relocate_without_allocation() {
            if constexpr (std::is_const_v<T> || !std::is_nothrow_destructible_v<T>) return false;
            else if constexpr (detail::future_custom_lifetime_supported<T>()) return true;
            else if constexpr (std::is_array_v<T>) return can_relocate_without_allocation<std::remove_extent_t<T>>();
            else if constexpr (std::is_trivially_copyable_v<T> || is_owning_head<T>::value) return true;
            else if constexpr (std::is_class_v<T> && boost::typelayout::is_byte_copy_safe_v<T>) {
                constexpr auto bases = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
                constexpr auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
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
                    return move_assignment_contains(value.data(),
                        value.capacity_ ? std::size_t(value.capacity_) + 1u : 0u, address);
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
                                 boost::typelayout::is_byte_copy_safe_v<CleanT>) {
                bool found = false;
                detail::for_each_reflected_subobject<CleanT>([&](auto index) {
                    constexpr auto base = std::meta::bases_of(^^CleanT,
                        std::meta::access_context::unchecked())[decltype(index)::value];
                    using Base = [:std::meta::type_of(base):];
                    if (!found) found = move_assignment_owns_address(
                        static_cast<const Base&>(value), address);
                }, [&](auto index) {
                    constexpr auto member = std::meta::nonstatic_data_members_of(^^CleanT,
                        std::meta::access_context::unchecked())[decltype(index)::value];
                    if constexpr (!std::meta::is_bit_field(member)) {
                        if (!found) found = move_assignment_owns_address(value.[:member:], address);
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
            } else if constexpr (is_owning_head<T>::value) {
                std::construct_at(dst, src.binding());
                set_rel32(dst->data_delta, &dst->data_delta, src.data());
                dst->size_ = src.size_;
                dst->capacity_ = src.capacity_;
                src.data_delta = 0;
                src.size_ = 0;
                src.capacity_ = 0;
            } else {
                dst = detail::begin_relocated_ordinary_outer<T>(dst, src);
                detail::for_each_reflected_subobject<T>([&](auto index) {
                    constexpr auto base = std::meta::bases_of(^^T, std::meta::access_context::unchecked())[decltype(index)::value];
                    using Base = [:std::meta::type_of(base):];
                    relocate_construct(static_cast<Base*>(dst), static_cast<Base&>(src));
                }, [&](auto index) {
                    constexpr auto member = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())[decltype(index)::value];
                    relocate_construct(&((*dst).[:member:]), src.[:member:]);
                });
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

    namespace fixed_detail {
        // [class.prop]: an aggregate whose destructor is not user-provided is
        // implicit-lifetime, even when that destructor is nontrivial because a
        // member owns storage. This independent standard branch does not
        // specialize std traits or alter V1/TypeLayout admission. The fixed
        // SDK's std trait is not used as a necessary condition for this branch.
        template<class T> consteval bool stable_erase_implicit_lifetime() {
            if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
            else if constexpr (std::is_implicit_lifetime_v<T>) return true;
            else if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> &&
                               StableEraseOrdinaryMembers<T>::value) {
                for (auto member : std::meta::members_of(^^T, std::meta::access_context::unchecked())) {
                    if (std::meta::is_destructor(member)) {
                        if (std::meta::is_user_provided(member)) return false;
                    }
                }
                return true;
            } else return false;
        }
        template<class T> consteval bool stable_erase_supported();
        template<class T, std::size_t I> consteval bool stable_erase_member_supported() {
            constexpr auto member = std::meta::nonstatic_data_members_of(^^T,
                std::meta::access_context::unchecked())[I];
            if constexpr (std::meta::is_bit_field(member)) return false;
            else {
                using Member = [:std::meta::type_of(member):];
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
                               (std::is_trivially_copyable_v<T> || boost::typelayout::is_byte_copy_safe_v<T>)) {
                constexpr auto bases = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
                constexpr auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
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
                static_assert(std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size() == 0);
                detail::for_each_reflected_subobject<T>([&](auto) {}, [&](auto index) {
                    constexpr auto member = std::meta::nonstatic_data_members_of(^^T,
                        std::meta::access_context::unchecked())[decltype(index)::value];
                    stable_erase_rebase_subobjects(&((*destination).[:member:]), source.[:member:]);
                });
            }
        }
        template<class T> void stable_erase_relocate(T* destination, T& source) noexcept {
            static_assert(stable_erase_supported<T>());
            if constexpr (StableEraseRelocator<T>::enabled) {
                StableEraseRelocator<T>::relocate_construct(destination, source);
            } else if constexpr (is_owning_head<T>::value) {
                // These explicit constructors begin owning-head lifetimes even
                // when they were not created by an enclosing implicit copy.
                std::construct_at(destination, source.binding());
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

    struct alignas(8) XWireHeaderV1 {
        char magic[8];
        std::uint16_t format_major;
        std::uint16_t format_minor;
        std::uint32_t header_size;
        std::uint64_t used_bytes;
        std::uint64_t reserved_bytes;
        std::uint64_t root_offset;
        std::uint64_t allocator_offset;
        std::uint64_t root_type_id;
        std::uint64_t schema_hash_lo;
        std::uint64_t schema_hash_hi;
        std::uint32_t flags;
        std::uint32_t endian_tag;
        std::uint32_t crc32c;
        std::uint32_t reserved0;
    };

    inline constexpr char XWIRE_MAGIC_V1[8] = {'X', 'O', 'F', 'F', 'V', '1', '\0', '\0'};
    inline constexpr std::uint16_t XWIRE_FORMAT_MAJOR_V1 = 1;
    inline constexpr std::uint16_t XWIRE_FORMAT_MINOR_V1 = 0;
    inline constexpr std::uint32_t XWIRE_ENDIAN_TAG_V1 = 0x01020304u;
    inline constexpr std::uint32_t XWIRE_FLAGS_V1 = 0;

    inline bool wire_magic_matches_v1(const char (&magic)[8]) noexcept {
        return std::memcmp(magic, XWIRE_MAGIC_V1, sizeof(XWIRE_MAGIC_V1)) == 0;
    }

    // Type admission — delegated to TypeLayout. XOffset's frozen containers
    // provide direct safety specializations and no longer rely on opaque
    // registration.
    using boost::typelayout::is_byte_copy_safe_v;

    template <typename T>
    inline constexpr bool is_v1_wire_admitted_v = detail::v1_wire_admitted<T>();

    // ========================================================================
    // Reflection-based construction and transfer (C++26 P2996).
    // ========================================================================
    namespace detail {
        namespace tl = ::boost::typelayout;

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
                return tl::FixedString{"bool"};
            } else if constexpr (std::is_same_v<U, char>) {
                return tl::FixedString{"char"};
            } else if constexpr (std::is_same_v<U, signed char>) {
                return tl::FixedString{"i8char"};
            } else if constexpr (std::is_same_v<U, unsigned char>) {
                return tl::FixedString{"u8char"};
            } else if constexpr (std::is_same_v<U, std::byte>) {
                return tl::FixedString{"byte"};
            } else if constexpr (std::is_same_v<U, int8_t>) {
                return tl::FixedString{"i8"};
            } else if constexpr (std::is_same_v<U, uint8_t>) {
                return tl::FixedString{"u8"};
            } else if constexpr (std::is_same_v<U, int16_t>) {
                return tl::FixedString{"i16"};
            } else if constexpr (std::is_same_v<U, uint16_t>) {
                return tl::FixedString{"u16"};
            } else if constexpr (std::is_same_v<U, int32_t>) {
                return tl::FixedString{"i32"};
            } else if constexpr (std::is_same_v<U, uint32_t>) {
                return tl::FixedString{"u32"};
            } else if constexpr (std::is_same_v<U, int64_t>) {
                return tl::FixedString{"i64"};
            } else if constexpr (std::is_same_v<U, uint64_t>) {
                return tl::FixedString{"u64"};
            } else if constexpr (std::is_same_v<U, float>) {
                return tl::FixedString{"f32"};
            } else if constexpr (std::is_same_v<U, double>) {
                return tl::FixedString{"f64"};
            } else {
                static_assert(sizeof(U) == 0, "unsupported v1 scalar in wire_abi_signature()");
            }
        }

        template <typename T, std::size_t N>
        consteval auto fixed_sequence_header_signature(const tl::FixedString<N>& tag) {
            return tag +
                   tl::FixedString{"[s:"} +
                   tl::to_fixed_string<sizeof(std::remove_cv_t<T>)>() +
                   tl::FixedString{",a:"} +
                   tl::to_fixed_string<alignof(std::remove_cv_t<T>)>() +
                   tl::FixedString{"]{arena:rel32,data:rel32,size:u32,capacity:u32}"};
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature() {
            using namespace std::meta;
            using U = std::remove_cv_t<T>;
            constexpr auto member =
                nonstatic_data_members_of(^^U, access_context::unchecked())[Index];
            static_assert(!is_bit_field(member),
                "wire_abi_signature() does not support bit-fields in v1");
            using FieldType = [:type_of(member):];
            return tl::FixedString{"@"} +
                   tl::to_fixed_string<offset_of(member).bytes>() +
                   tl::FixedString{":"} +
                   wire_abi_signature<FieldType>();
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature_with_sep() {
            if constexpr (Index == 0) {
                return wire_record_field_signature<T, Index>();
            } else {
                return tl::FixedString{","} + wire_record_field_signature<T, Index>();
            }
        }

        template <typename T, std::size_t... Is>
        consteval auto wire_record_fields(std::index_sequence<Is...>) {
            if constexpr (sizeof...(Is) == 0) {
                return tl::FixedString{""};
            } else {
                return (wire_record_field_signature_with_sep<T, Is>() + ...);
            }
        }

        template <typename T>
        consteval auto wire_record_signature() {
            using U = std::remove_cv_t<T>;
            static_assert(reflected_base_count<U>() == 0,
                "wire_abi_signature() does not support base classes in v1");
            constexpr std::size_t field_count = reflected_member_count<U>();
            return tl::FixedString{"record[s:"} +
                   tl::to_fixed_string<sizeof(U)>() +
                   tl::FixedString{",a:"} +
                   tl::to_fixed_string<alignof(U)>() +
                   tl::FixedString{"]{"} +
                   wire_record_fields<U>(std::make_index_sequence<field_count>{}) +
                   tl::FixedString{"}"};
        }

        template <typename T>
        consteval auto wire_abi_signature() {
            using U = std::remove_cv_t<T>;
            if constexpr (detail::fixed_size_scalar<U>) {
                return wire_scalar_signature<U>();
            } else if constexpr (std::is_enum_v<U>) {
                return tl::FixedString{"enum<"} +
                       wire_abi_signature<std::underlying_type_t<U>>() +
                       tl::FixedString{">"};
            } else if constexpr (std::is_array_v<U>) {
                return tl::FixedString{"array["} +
                       tl::to_fixed_string<std::extent_v<U>>() +
                       tl::FixedString{"]<"} +
                       wire_abi_signature<std::remove_extent_t<U>>() +
                       tl::FixedString{">"};
            } else if constexpr (is_fixed_string_v<U>) {
                return fixed_sequence_header_signature<U>(tl::FixedString{"xstring"});
            } else if constexpr (is_fixed_flat_map_v<U>) {
                return fixed_sequence_header_signature<U>(tl::FixedString{"xflatmap"}) +
                       tl::FixedString{"<"} +
                       wire_abi_signature<typename U::key_type>() +
                       tl::FixedString{","} +
                       wire_abi_signature<typename U::mapped_type>() +
                       tl::FixedString{">"};
            } else if constexpr (is_fixed_flat_set_v<U>) {
                return fixed_sequence_header_signature<U>(tl::FixedString{"xflatset"}) +
                       tl::FixedString{"<"} +
                       wire_abi_signature<typename U::value_type>() +
                       tl::FixedString{">"};
            } else if constexpr (is_fixed_vector_v<U>) {
                if constexpr (std::is_same_v<typename U::value_type, std::byte>) {
                    return fixed_sequence_header_signature<U>(tl::FixedString{"xblob"});
                } else {
                    return fixed_sequence_header_signature<U>(tl::FixedString{"xvector"}) +
                           tl::FixedString{"<"} +
                           wire_abi_signature<typename U::value_type>() +
                           tl::FixedString{">"};
                }
            } else if constexpr (is_xkeyvalue_v<U>) {
                return tl::FixedString{"xkv[s:"} +
                       tl::to_fixed_string<sizeof(U)>() +
                       tl::FixedString{",a:"} +
                       tl::to_fixed_string<alignof(U)>() +
                       tl::FixedString{"]<"} +
                       wire_abi_signature<typename U::first_type>() +
                       tl::FixedString{","} +
                       wire_abi_signature<typename U::second_type>() +
                       tl::FixedString{">"};
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

        inline XAllocatorStateV1* find_allocator_state(XBufferCore& xbuf) {
            return xbuf.template find_allocator_storage<XAllocatorStateV1>();
        }

        inline XAllocatorStateV1* ensure_allocator_state(XBufferCore& xbuf) {
            if (auto* state = find_allocator_state(xbuf)) {
                return state;
            }
            auto* state = xbuf.template construct_allocator_storage<XAllocatorStateV1>();
            fixed_detail::initialize_allocator_state(*state, xbuf.arena());
            return state;
        }

        inline fixed_detail::allocator_ref allocator_binding_for(
            XBufferCore& xbuf, bool create_if_missing = true) {
            auto* state = create_if_missing ? ensure_allocator_state(xbuf)
                                           : find_allocator_state(xbuf);
            return {state, xbuf.arena()};
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc);

        template <typename T>
        void destroy_reflected_prefix(void* raw, std::size_t built) noexcept {
            using namespace std::meta;
            constexpr auto base_count = bases_of(^^T, access_context::unchecked()).size();
            constexpr auto member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            auto* obj = reinterpret_cast<T*>(raw);
            static_for<member_count>([&](auto index) {
                constexpr auto i = member_count - 1 - decltype(index)::value;
                constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[i];
                if (base_count + i < built) std::destroy_at(&((*obj).[:member:]));
            });
            static_for<base_count>([&](auto index) {
                constexpr auto i = base_count - 1 - decltype(index)::value;
                constexpr auto base = bases_of(^^T, access_context::unchecked())[i];
                using Base = [:type_of(base):];
                if (i < built) std::destroy_at(static_cast<Base*>(obj));
            });
        }

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            T& obj = *reinterpret_cast<T*>(raw);
            fixed_detail::default_construct(&(obj.[:member:]), alloc);
        }

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_base_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
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
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            T& d = *reinterpret_cast<T*>(dst);
            fixed_detail::transfer_construct(&(d.[:member:]), src.[:member:], alloc);
        }

        template <typename T, std::size_t N, typename Src, typename Alloc>
        void reflect_transfer_base_nth(void* dst, Src&& src, Alloc alloc) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
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

    } // namespace detail (reflection)

    template <typename T>
    consteval std::uint64_t wire_root_type_id_v() {
        return detail::wire_root_type_id<T>();
    }

    template <typename T>
    consteval auto wire_abi_signature() {
        return detail::wire_abi_signature<T>();
    }

    template <typename T>
    consteval std::uint64_t wire_schema_hash_v() {
        return detail::wire_schema_hash<T>();
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
            template<class T> static XWireHeaderV1 header(XBufferCore&, bool);
            template<class T> static void save_into(XBufferCore&, std::string&);
            static void adopt_storage(XBufferCore&, VirtualMemoryStorage&, XGrowPolicy) noexcept;
            static void check_received_payload(XBufferCore&, const XWireHeaderV1&);
            static void attach_owner(XBufferCore&);
            static void rollback_storage(XBufferCore&, VirtualMemoryStorage&) noexcept;
            template<class T> static void publish_received(XBufferCore&) noexcept;
        };
    }

    class XBuffer : public XBufferCore {
    public:
        XBuffer() = default;

        explicit XBuffer(std::size_t size)
            : XBufferCore(size) {
            (void)detail::ensure_allocator_state(*this);
        }

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

        /// Legacy minimum reservation hint. Use maximum_capacity() for a hard limit.
        struct MaxCapacity { std::size_t value; };
        static MaxCapacity max_capacity(std::size_t bytes) { return {bytes}; }

        XBuffer(std::size_t size, MaxCapacity cap)
            : XBufferCore(size, cap.value) {
            (void)detail::ensure_allocator_state(*this);
        }

        XBuffer(std::size_t size, MaximumCapacity cap)
            : XBufferCore(size, cap) {
            (void)detail::ensure_allocator_state(*this);
        }

        void swap(XBuffer& other) noexcept {
            if (this == &other) return;
            XBufferCore::swap(other);
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

        XAllocatorStateV1* allocator_state() {
            return detail::find_allocator_state(*this);
        }

        const XAllocatorStateV1* allocator_state() const {
            return const_cast<XBuffer*>(this)->allocator_state();
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
        void save_verified_into(std::string& out) {
            detail::VerifiedPathsAccess::save_into<T>(*this, out);
        }

        template <typename T>
        std::string save_verified() {
            static_assert(is_v1_wire_admitted_v<T>,
                "save_verified<T>() requires a v1-admitted fixed-schema wire type.");
            require_typed_root_access("save_verified<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_verified<T>(): schema mismatch for requested root type");
            // Verified wire currently uses the same normalized transport semantics as save():
            // payload bytes are preserved, allocator slack is not.
            this->shrink_to_fit();
            auto* root_ptr = detail::find_root<T>(*this);
            if (!root_ptr) {
                throw XException(
                    "save_verified<T>(): no root object of requested type");
            }
            auto* allocator_state = detail::find_allocator_state(*this);
            if (!allocator_state) {
                throw XException(
                    "save_verified<T>(): allocator state not found");
            }

            const char* base = static_cast<const char*>(this->get_address());
            const auto payload_size = this->segment_size();
            XWireHeaderV1 header{};
            std::memcpy(header.magic, XWIRE_MAGIC_V1, sizeof(XWIRE_MAGIC_V1));
            header.format_major = XWIRE_FORMAT_MAJOR_V1;
            header.format_minor = XWIRE_FORMAT_MINOR_V1;
            header.header_size = static_cast<std::uint32_t>(sizeof(XWireHeaderV1));
            header.used_bytes = static_cast<std::uint64_t>(payload_size);
            // Keep transport normalized. OS reservations are process-local metadata.
            header.reserved_bytes = header.used_bytes;
            header.root_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(root_ptr) -
                reinterpret_cast<std::uintptr_t>(this->get_address()));
            header.allocator_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(allocator_state) -
                reinterpret_cast<std::uintptr_t>(this->get_address()));
            header.root_type_id = detail::wire_root_type_id<T>();
            header.schema_hash_lo = detail::wire_schema_hash<T>();
            header.schema_hash_hi = 0;
            header.flags = XWIRE_FLAGS_V1;
            header.endian_tag = XWIRE_ENDIAN_TAG_V1;
            header.crc32c = 0;
            header.reserved0 = 0;

            std::string out;
            if (sizeof(header) > out.max_size() ||
                payload_size > out.max_size() - sizeof(header)) {
                throw std::length_error("save_verified<T>(): output size exceeds string limit");
            }
            const auto output_size = sizeof(header) + payload_size;
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {
                std::memcpy(output, &header, sizeof(header));
                std::memcpy(output + sizeof(header), base, payload_size);
                return output_size;
            });
#else
            out.reserve(output_size);
            out.append(reinterpret_cast<const char*>(&header), sizeof(header));
            out.append(base, payload_size);
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

        // Explicit host-only upstream. A fresh context and two fresh membership
        // indexes are constructed for every invocation, including cached blocks.
        // The upstream must outlive this synchronous call and be non-null.
        template <typename T>
        static XBuffer load_verified_with_resource(std::span<const std::byte> data,
                                                  std::optional<MaximumCapacity> cap,
                                                  std::pmr::memory_resource* upstream) {
            if (!upstream) throw std::invalid_argument("validation scratch: null upstream");
            static_assert(is_v1_wire_admitted_v<T>,
                "load_verified<T>() requires a v1-admitted fixed-schema wire type.");
            if (data.size_bytes() < sizeof(XWireHeaderV1)) {
                throw XException(
                    "load_verified<T>(): wire header too small");
            }

            XWireHeaderV1 header{};
            std::memcpy(&header, data.data(), sizeof(header));
            if (!wire_magic_matches_v1(header.magic)) {
                throw XException(
                    "load_verified<T>(): bad magic");
            }
            if (header.format_major != XWIRE_FORMAT_MAJOR_V1 ||
                header.format_minor != XWIRE_FORMAT_MINOR_V1) {
                throw XException(
                    "load_verified<T>(): unsupported format version");
            }
            if (header.header_size != sizeof(XWireHeaderV1)) {
                throw XException(
                    "load_verified<T>(): header size mismatch");
            }
            if (header.endian_tag != XWIRE_ENDIAN_TAG_V1) {
                throw XException(
                    "load_verified<T>(): endian tag mismatch");
            }
            if (header.flags != XWIRE_FLAGS_V1) {
                throw XException(
                    "load_verified<T>(): unsupported flags");
            }
            if (header.root_type_id != detail::wire_root_type_id<T>()) {
                throw XException(
                    "load_verified<T>(): root type id mismatch");
            }
            if (header.schema_hash_lo != detail::wire_schema_hash<T>() ||
                header.schema_hash_hi != 0) {
                throw XException(
                    "load_verified<T>(): schema hash mismatch");
            }
            if (header.used_bytes != data.size_bytes() - sizeof(XWireHeaderV1)) {
                throw XException(
                    "load_verified<T>(): used size mismatch");
            }
            if (header.reserved_bytes < header.used_bytes ||
                header.reserved_bytes > MAX_PAYLOAD || header.used_bytes > MAX_PAYLOAD) {
                throw XException(
                    "load_verified<T>(): invalid payload or reservation size");
            }
            if (header.crc32c != 0 || header.reserved0 != 0)
                throw XException("load_verified<T>(): unsupported reserved header fields");
            if (header.root_offset >= header.used_bytes) {
                throw XException(
                    "load_verified<T>(): root offset out of bounds");
            }
            if (header.allocator_offset >= header.used_bytes) {
                throw XException(
                    "load_verified<T>(): allocator offset out of bounds");
            }

            const char* payload = reinterpret_cast<const char*>(data.data() + sizeof(XWireHeaderV1));
            XBuffer xbuf = cap
                ? XBuffer(payload, static_cast<std::size_t>(header.used_bytes), *cap)
                : XBuffer(payload, static_cast<std::size_t>(header.used_bytes),
                          static_cast<std::size_t>(header.reserved_bytes));
            if (xbuf.arena()->used_end != header.used_bytes) {
                throw XException("load_verified<T>(): non-normalized payload extent");
            }
            auto* root_ptr = detail::find_root<T>(xbuf);
            if (!root_ptr) {
                throw XException(
                    "load_verified<T>(): root not found");
            }
            auto actual_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(root_ptr) -
                reinterpret_cast<std::uintptr_t>(xbuf.get_address()));
            if (actual_offset != header.root_offset) {
                throw XException(
                    "load_verified<T>(): root offset mismatch");
            }
            auto* allocator_state = detail::find_allocator_state(xbuf);
            if (!allocator_state) {
                throw XException(
                    "load_verified<T>(): allocator state not found");
            }
            auto actual_allocator_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(allocator_state) -
                reinterpret_cast<std::uintptr_t>(xbuf.get_address()));
            if (actual_allocator_offset != header.allocator_offset) {
                throw XException(
                    "load_verified<T>(): allocator offset mismatch");
            }
            detail::verify_structural_wire_graph(*root_ptr, xbuf, upstream);
            xbuf.typed_root_access_allowed_ = true;
            xbuf.bind_schema<T>();
            return xbuf;
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

        std::string save_verified() { return XBuffer::save_verified<T>(); }

        void save_verified_into(std::string& out) { XBuffer::save_verified_into<T>(out); }

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
            static_assert(boost::typelayout::is_byte_copy_safe_v<CleanT>,
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
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            migrate_members(static_cast<const BaseType&>(old_obj),
                            static_cast<BaseType&>(new_obj), old_xbuf, new_xbuf);
        }

        template<typename T, std::size_t N>
        static void migrate_member_at(const T& old_obj, T& new_obj,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using MemberType = [:type_of(member):];
            migrate_member<MemberType>(old_obj.[:member:], new_obj.[:member:],
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

namespace boost::typelayout {
inline namespace v1 {

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

} // inline namespace v1
} // namespace boost::typelayout

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

// Compatibility wrappers. `name` is ignored now that XOffset no longer registers
// its own containers through TypeLayout's opaque mechanism.
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

#include "xoffset_verified_paths.hpp"
#include "xoffset_verified_receive.hpp"
#include "xoffset_core_lifetime.hpp"

#endif
