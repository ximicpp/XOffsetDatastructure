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
#include <new>
#include <stdexcept>
#include <exception>
#include <optional>
#include <mutex>
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
        // Registration is the only registry operation allowed to allocate.
        // One-shot thread-local injection leaves existing owners untouched.
        inline thread_local bool pending_registry_failure = false;
        inline void fail_next_registry_registration() noexcept { pending_registry_failure = true; }
        inline void clear_registry_failures() noexcept { pending_registry_failure = false; }
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
            return static_cast<T*>(
                arena_->allocate_aligned(
                    static_cast<std::uint32_t>(n * sizeof(T)), alignof(T)));
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

        private:
            XBufferCore& owner() {
                return XBufferCore::owner_from_payload_ptr(this);
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
            auto& state = registry_state();
            std::lock_guard lock(state.mutex);
            buffer_.swap(other.buffer_);
            free_blocks_.swap(other.free_blocks_);
            std::swap(growth_policy_, other.growth_policy_);
            std::swap(root_type_identity_, other.root_type_identity_);
            std::swap(typed_root_access_allowed_, other.typed_root_access_allowed_);
            std::swap(schema_bound_, other.schema_bound_);
            std::swap(bound_root_type_id_, other.bound_root_type_id_);
            std::swap(bound_schema_hash_, other.bound_schema_hash_);
            ++m_epoch;
            ++other.m_epoch;
            update_registry_owner_locked(state);
            other.update_registry_owner_locked(state);
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

        struct RegistryState {
            std::mutex mutex;
            std::unordered_map<const void*, XBufferCore*, AddressHash> owners;
        };

        static RegistryState& registry_state() {
            // Globals may first register in main and unregister after ordinary
            // function statics have been destroyed. The index and mutex share
            // process lifetime; its nodes are erased as owners are destroyed.
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
            auto& state = registry_state();
            std::lock_guard lock(state.mutex);
            // The sole caller is arena_header::owner(): ptr is exactly the base.
            const auto found = state.owners.find(ptr);
            if (found != state.owners.end()) return *found->second;
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
            auto& state = registry_state();
            std::lock_guard lock(state.mutex);
            unregister_registry_locked(state);
            buffer_ = std::move(other.buffer_);
            free_blocks_ = std::move(other.free_blocks_);
            growth_policy_ = std::exchange(other.growth_policy_, XGrowPolicy::Explicit);
            root_type_identity_ = std::exchange(other.root_type_identity_, nullptr);
            typed_root_access_allowed_ = std::exchange(other.typed_root_access_allowed_, true);
            schema_bound_ = std::exchange(other.schema_bound_, false);
            bound_root_type_id_ = std::exchange(other.bound_root_type_id_, 0);
            bound_schema_hash_ = std::exchange(other.bound_schema_hash_, 0);
            ++m_epoch;
            ++other.m_epoch;
            // The source key already exists. Transfer its value in place,
            // without allocating a new node or triggering a rehash.
            update_registry_owner_locked(state);
        }

        void update_registry_owner_locked(RegistryState& state) noexcept {
            if (!buffer_.data()) return;
            const auto found = state.owners.find(buffer_.data());
            if (found == state.owners.end()) std::terminate(); // broken internal ownership
            found->second = this;
        }

        void unregister_registry() noexcept {
            if (!buffer_.data()) return;
            auto& state = registry_state();
            std::lock_guard lock(state.mutex);
            unregister_registry_locked(state);
        }

        void unregister_registry_locked(RegistryState& state) noexcept {
            if (buffer_.data()) state.owners.erase(buffer_.data());
        }

        void register_registry() {
            if (std::exchange(detail::pending_registry_failure, false)) throw XBadAlloc();
            auto& state = registry_state();
            std::lock_guard lock(state.mutex);
            try {
                const auto result = state.owners.emplace(buffer_.data(), this);
                if (!result.second) throw XException("duplicate arena owner registration");
            } catch (const std::bad_alloc&) {
                // With nonthrowing hash/equality, failed insertion preserves all
                // existing entries. The constructor releases its new VM owner.
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

        template <typename T>
        T* object_at_offset(std::uint64_t offset) noexcept {
            if (offset == 0 || offset > buffer_.size() ||
                sizeof(T) > buffer_.size() - offset) {
                return nullptr;
            }
            auto* raw = buffer_.data() + static_cast<size_type>(offset);
            if (reinterpret_cast<std::uintptr_t>(raw) % alignof(T) != 0) return nullptr;
            return std::launder(reinterpret_cast<T*>(raw));
        }

        template <typename T>
        const T* object_at_offset_const(std::uint64_t offset) const noexcept {
            if (offset == 0 || offset > buffer_.size() ||
                sizeof(T) > buffer_.size() - offset) {
                return nullptr;
            }
            auto* raw = buffer_.data() + static_cast<size_type>(offset);
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
                reinterpret_cast<const char*>(obj) - buffer_.data());
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

        void* allocate_aligned_impl(std::uint32_t bytes, std::size_t alignment, bool allow_growth = true) {
            if (bytes > MAX_PAYLOAD || alignment == 0 ||
                (alignment & (alignment - 1)) != 0 || alignment > MAX_PAYLOAD) throw XBadAlloc();
            if (bytes == 0) bytes = 1;
            alignment = std::max<std::size_t>(alignment, alignof(std::max_align_t));

            for (std::size_t i = 0; i < free_blocks_.size(); ++i) {
                const auto& block = free_blocks_[i];
                if (block.user_bytes >= bytes && block.alignment >= alignment) {
                    void* ptr = buffer_.data() + block.user_offset;
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
                throw XBadAlloc();
            const auto first_address = base + current_end + sizeof(AllocationHeader);
            if (alignment - 1u > limit - first_address) throw XBadAlloc();
            const auto user_address = align_up(first_address, alignment);
            const size_type user_offset = user_address - base;
            if (user_offset > MAX_PAYLOAD || bytes > MAX_PAYLOAD - user_offset)
                throw XBadAlloc();
            const size_type block_begin = user_offset - sizeof(AllocationHeader);
            const size_type block_end = user_offset + bytes;
            if (!ensure_allocation_extent(block_end, allow_growth)) throw XBadAlloc();

            auto* header = std::launder(
                reinterpret_cast<AllocationHeader*>(buffer_.data() + block_begin));
            header->total_bytes = static_cast<std::uint32_t>(block_end - block_begin);
            header->user_bytes = bytes;
            header->user_offset = static_cast<std::uint32_t>(user_offset);
            header->alignment = static_cast<std::uint32_t>(alignment);
            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);
            return buffer_.data() + user_offset;
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
                buffer_.data() + offset - sizeof(AllocationHeader)));
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
            auto* raw = reinterpret_cast<char*>(ptr);
            if (raw < buffer_.data() + static_cast<std::ptrdiff_t>(sizeof(AllocationHeader)) ||
                raw >= buffer_.data() + static_cast<std::ptrdiff_t>(buffer_.size())) {
                return;
            }
            auto* header = std::launder(
                reinterpret_cast<AllocationHeader*>(raw - sizeof(AllocationHeader)));
            if (header->total_bytes == 0 || header->user_offset == 0) return;
            try {
                free_blocks_.push_back(
                    {header->user_offset, header->total_bytes, header->user_bytes, header->alignment});
            } catch (const std::bad_alloc&) {
                // Reclamation is best effort; destructors must not terminate on host OOM.
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

        template <typename T> consteval bool can_relocate_without_allocation();
        template <typename T> void relocate_construct(T* dst, T& src) noexcept;

        template <typename T>
        T* resolve_rel32(const std::int32_t* origin, std::int32_t delta) noexcept {
            if (delta == 0) return nullptr;
            auto* base = reinterpret_cast<const char*>(origin);
            auto* raw = const_cast<char*>(base + delta);
            return std::launder(reinterpret_cast<T*>(raw));
        }

        template <typename T>
        const T* resolve_rel32_const(const std::int32_t* origin, std::int32_t delta) noexcept {
            if (delta == 0) return nullptr;
            auto* base = reinterpret_cast<const char*>(origin);
            auto* raw = base + delta;
            return std::launder(reinterpret_cast<const T*>(raw));
        }

        inline void set_rel32(std::int32_t& field, const void* origin, const void* target) {
            if (!target) {
                field = 0;
                return;
            }
            auto delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) -
                         static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(origin));
            if (delta < static_cast<std::ptrdiff_t>(std::numeric_limits<std::int32_t>::min()) ||
                delta > static_cast<std::ptrdiff_t>(std::numeric_limits<std::int32_t>::max())) {
                throw XException(
                    "relative offset exceeds rel32 range");
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
            auto delta = reinterpret_cast<const char*>(ptr) -
                         reinterpret_cast<const char*>(state);
            if (delta <= 0 ||
                delta > static_cast<std::ptrdiff_t>(std::numeric_limits<std::uint32_t>::max())) {
                throw XException(
                    "allocator state free-list offset out of range");
            }
            return static_cast<std::uint32_t>(delta);
        }

        inline FreeBlockHeader* freelist_block_from_offset(
            XAllocatorStateV1* state, std::uint32_t offset) noexcept {
            if (offset == 0) return nullptr;
            auto* raw = reinterpret_cast<char*>(state) + offset;
            return std::launder(reinterpret_cast<FreeBlockHeader*>(raw));
        }

        inline const FreeBlockHeader* freelist_block_from_offset(
            const XAllocatorStateV1* state, std::uint32_t offset) noexcept {
            if (offset == 0) return nullptr;
            auto* raw = reinterpret_cast<const char*>(state) + offset;
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
            for (std::uint32_t i = 0; i < freelist_class_count; ++i) {
                std::uint32_t offset = state->freelist_head[i];
                state->freelist_head[i] = 0;
                while (offset != 0) {
                    auto* block = freelist_block_from_offset(state, offset);
                    auto next = block->next_offset;
                    arena->deallocate(block);
                    offset = next;
                }
            }
            state->free_bytes = 0;
            ++state->epoch;
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

            if (auto* state = alloc_ref.state) {
                auto class_index = freelist_size_class(aligned_bytes);
                if (class_index < freelist_class_count) {
                    for (std::uint32_t i = class_index; i < freelist_class_count; ++i) {
                        auto* link = &state->freelist_head[i];
                        while (*link) {
                            auto* block = freelist_block_from_offset(state, *link);
                            if (block->block_bytes >= aligned_bytes &&
                                reinterpret_cast<std::uintptr_t>(block) % alignment == 0) {
                                *link = block->next_offset;
                                state->free_bytes -= block->block_bytes;
                                ++state->epoch;
                                return block;
                            }
                            link = &block->next_offset;
                        }
                    }
                }
            }

            void* raw = nullptr;
            try {
                raw = arena->allocate_without_growth(aligned_bytes, alignment);
            } catch (const XBadAlloc&) {
                if (alloc_ref.state && alloc_ref.state->free_bytes != 0)
                    release_cached_blocks(alloc_ref.state);
                raw = arena->allocate_aligned(aligned_bytes, alignment);
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
                std::memcpy(&header, static_cast<const char*>(ptr) - sizeof(header), sizeof(header));
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
            auto* header = std::launder(reinterpret_cast<FreeBlockHeader*>(ptr));
            header->block_bytes = aligned_bytes;
            header->next_offset = state->freelist_head[class_index];
            state->freelist_head[class_index] = offset;
            state->free_bytes += aligned_bytes;
            ++state->epoch;
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
            if constexpr (std::is_array_v<T>) {
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
            if constexpr (std::is_array_v<T>) {
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
            if (fixed_detail::try_expand_dynamic_bytes(
                    alloc, data(), static_cast<std::uint32_t>(new_cap + 1), alignof(char))) {
                capacity_ = new_cap;
                return;
            }
            auto* new_data = static_cast<char*>(fixed_detail::allocate_dynamic_bytes(
                alloc, static_cast<std::uint32_t>(new_cap + 1), alignof(char)));

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
            reserve(static_cast<std::uint32_t>(rhs.size()));
            if (!rhs.empty()) {
                std::memmove(data(), rhs.data(), rhs.size());
            }
            size_ = static_cast<std::uint32_t>(rhs.size());
            data()[size_] = '\0';
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
                capacity_ = new_cap;
                return;
            }
            if constexpr (fixed_detail::can_relocate_without_allocation<T>()) {
                if (new_cap > max_size()) throw XBadAlloc();
                auto alloc = binding();
                auto* fresh = static_cast<T*>(fixed_detail::allocate_dynamic_bytes(
                    alloc, static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T)));
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

        void push_back(const T& value) {
            insert_construct(size_, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, binding());
            });
        }

        void push_back(T&& value) {
            push_back(static_cast<const T&>(value));
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
            auto* fresh = static_cast<T*>(fixed_detail::allocate_dynamic_bytes(alloc, bytes, alignof(T)));
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
            auto* staged = static_cast<T*>(fixed_detail::allocate_dynamic_bytes(alloc, sizeof(T), alignof(T)));
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
                auto* staged = static_cast<T*>(fixed_detail::allocate_dynamic_bytes(alloc, sizeof(T), alignof(T)));
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
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::default_construct(dst, Base::binding());
            });
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

        struct WireValidationRange {
            const char* begin{};
            const char* end{};
        };

        struct WireValidationContext {
            const char* base{};
            const char* end{};
            XArena* segment{};
            const XAllocatorStateV1* allocator_state{};
            std::vector<WireValidationRange> owned_ranges;
            std::vector<WireValidationRange> free_ranges;
        };

        inline void validate_storage_range(const void* ptr, std::size_t bytes,
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
            auto* begin = static_cast<const char*>(ptr);
            auto* end = begin + bytes;
            for (const auto& range : ctx.owned_ranges) {
                if (!(end <= range.begin || begin >= range.end)) {
                    throw XException(
                        "wire validation detected overlapping dynamic ranges");
                }
            }
            for (const auto& range : ctx.free_ranges) {
                if (!(end <= range.begin || begin >= range.end)) {
                    throw XException("wire validation detected live/free overlap");
                }
            }
            ctx.owned_ranges.push_back({begin, end});
        }

        inline void register_free_range(const void* ptr, std::size_t bytes,
                                        WireValidationContext& ctx,
                                        const char* what) {
            if (bytes == 0 || !ptr) return;
            validate_storage_range(ptr, bytes, 1, ctx, what);
            auto* begin = static_cast<const char*>(ptr);
            auto* end = begin + bytes;
            for (const auto& range : ctx.owned_ranges) {
                if (!(end <= range.begin || begin >= range.end)) {
                    throw XException(
                        "wire validation detected free-list overlap with live ranges");
                }
            }
            for (const auto& range : ctx.free_ranges) {
                if (!(end <= range.begin || begin >= range.end)) {
                    throw XException(
                        "wire validation detected overlapping free-list ranges");
                }
            }
            ctx.free_ranges.push_back({begin, end});
        }

        inline const char* checked_relative(const std::int32_t& delta,
                                            const WireValidationContext& ctx) {
            auto address = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta;
            auto* ptr = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
            validate_storage_range(ptr, 1, 1, ctx, "wire validation: relative pointer out of bounds");
            return ptr;
        }

        inline void validate_binding(const std::int32_t& delta, const WireValidationContext& ctx) {
            if (delta == 0 || checked_relative(delta, ctx) != reinterpret_cast<const char*>(ctx.allocator_state)) {
                throw XException("wire validation: invalid allocator binding");
            }
        }

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
            std::memcpy(&header, header_ptr, sizeof(header));
            if (header.user_offset != address - base || header.user_bytes < bytes ||
                header.total_bytes != std::uint64_t(header.user_bytes) + sizeof(Header) ||
                header.alignment < alignment || header.alignment > XBufferCore::MAX_PAYLOAD ||
                (header.alignment & (header.alignment - 1)) != 0 ||
                address % header.alignment != 0) {
                throw XException("wire validation: invalid allocation metadata");
            }
            if (free) register_free_range(header_ptr, header.total_bytes, ctx, "wire validation: invalid free allocation");
            else register_owned_range(header_ptr, header.total_bytes, ctx, "wire validation: invalid live allocation");
        }

        template <typename T>
        void validate_wire_value(const T& value, WireValidationContext& ctx);

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
            for (std::uint32_t i = 0; i < value.size_; ++i) {
                validate_wire_value(data[i], ctx);
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
            } else if constexpr (std::is_class_v<CleanT>) {
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
                ctx.end - reinterpret_cast<const char*>(state));
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
        void verify_structural_wire_graph(const T& root, XBufferCore& xbuf) {
            WireValidationContext ctx{
                .base = static_cast<const char*>(xbuf.get_address()),
                .end = static_cast<const char*>(xbuf.get_address()) + xbuf.arena()->used_end,
                .segment = xbuf.arena(),
                .allocator_state = xbuf.template find_allocator_storage<XAllocatorStateV1>()
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

        // Both slots must be in the same bounded arena. Only inline ownership
        // heads move; out-of-line payloads keep their addresses and ownership.
        template <typename T>
        void relocate_construct(T* dst, T& src) noexcept {
            static_assert(can_relocate_without_allocation<T>());
            if constexpr (std::is_array_v<T>) {
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

        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc) {
            std::memset(raw, 0, sizeof(T));  // zero ONCE at top level
            reflect_init_all_impl<T>(raw, alloc);
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

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all(void* dst, Src&& src, Alloc alloc) {
            std::memset(dst, 0, sizeof(T));  // zero ONCE at top level
            reflect_transfer_init_all_impl<T>(dst, std::forward<Src>(src), alloc);
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
                reinterpret_cast<const char*>(root_ptr) -
                static_cast<const char*>(this->get_address()));
            header.allocator_offset = static_cast<std::uint64_t>(
                reinterpret_cast<const char*>(allocator_state) -
                static_cast<const char*>(this->get_address()));
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
                reinterpret_cast<const char*>(root_ptr) -
                static_cast<const char*>(xbuf.get_address()));
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
                reinterpret_cast<const char*>(allocator_state) -
                static_cast<const char*>(xbuf.get_address()));
            if (actual_allocator_offset != header.allocator_offset) {
                throw XException(
                    "load_verified<T>(): allocator offset mismatch");
            }
            detail::verify_structural_wire_graph(*root_ptr, xbuf);
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

#endif
