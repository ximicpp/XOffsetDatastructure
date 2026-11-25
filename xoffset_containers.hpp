#ifndef X_OFFSET_CONTAINERS_HPP
#define X_OFFSET_CONTAINERS_HPP

// The containers that live in an arena: XString, XVector, XSet, XMap, their
// shared core XVectorCore, and their field lists for the schema. Part of
// xoffsetdatastructure.hpp; include that header. Semantics: docs/USER_GUIDE.md.

#include "xoffset_arena.hpp"

namespace XOffsetDatastructure {
    // ========================================================================
    // Container implementation details.
    // ========================================================================
    namespace detail {

        // Container concepts -- used by XCompactor for migration dispatch.
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

        // for_each_reflected_subobject for callbacks returning vstatus: without
        // exceptions the walk stops at the first false.
        template <typename T, typename BaseFn, typename MemberFn>
        vstatus for_each_reflected_subobject_v(BaseFn&& on_base, MemberFn&& on_member) {
#if XOFFSET_NO_EXCEPTIONS
            const bool bases = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (bool(on_base(std::integral_constant<std::size_t, Is>{})) && ...);
            }(std::make_index_sequence<schema::base_count_v<T>>{});
            return bases && [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (bool(on_member(std::integral_constant<std::size_t, Is>{})) && ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
#else
            for_each_reflected_subobject<T>(on_base, on_member);
#endif
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
                record = owner.owner_identity(); // made on first use
                const auto base = reinterpret_cast<std::uintptr_t>(record->payload);
                const auto at = reinterpret_cast<std::uintptr_t>(address);
                if (at >= base && at - base < owner.buffer_.size()) {
                    head_in_payload = true;
                    head_offset = at - base;
                }
                validate();
            }

            void validate() const {
                // A record stays owned while its payload is the owner's
                // storage (a replaced storage gets a new record), so the
                // owner field alone says the payload is live, and the host
                // word names its owner.
                if (!record->owner.load(std::memory_order_acquire))
                    XOFFSET_THROW(XException(XErrc::invalid_state, "stable_erase_if owner is no longer live"));
                if (!head_in_payload) return;
                const auto size = XBufferCore::owned_record_owner(*record)->buffer_.size();
                if (head_offset > size || head_bytes > size - head_offset)
                    XOFFSET_THROW(XException(XErrc::invalid_state, "stable_erase_if target is no longer in storage"));
            }
        };

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
        XOFFSET_DETAIL_INLINE bool move_assignment_owns_address_fast(
            const T& value, const void* address) noexcept;

        template <typename T>
        XOFFSET_DETAIL_INLINE bool move_assignment_disjoint(const T& a, const T& b) noexcept {
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
        template <typename T> consteval bool default_construct_reads_binding();
        template <typename T> void stable_erase_relocate(T*, T&) noexcept;
        template <typename T> consteval bool can_relocate_without_allocation();
        template <typename T> void relocate_construct(T* dst, T& src) noexcept;

        // Flat resolution for every storage kind: origin + delta (T may be
        // const).
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

        [[noreturn]] XOFFSET_DETAIL_COLD inline void throw_rel32_range() {
            XOFFSET_THROW(XException(XErrc::capacity_exceeded, "relative offset exceeds rel32 range"));
        }

        // MV: identical checks and store; only the throw is out of line, so
        // the range test inlines at every call site.
        XOFFSET_DETAIL_INLINE void set_rel32(std::int32_t& field, const void* origin, const void* target) {
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

        // set_rel32 for a head and a block of the same payload: a payload is
        // at most MAX_PAYLOAD (< 2^31) bytes, so the offset is always in range
        // and needs no check. Every block a head allocates comes from the
        // payload that holds the head (its binding), and allocation grows a
        // payload in place, never moving it.
        XOFFSET_DETAIL_INLINE void set_rel32_in_payload(std::int32_t& field, const void* origin,
                                                             const void* target) noexcept {
            static_assert(XBufferCore::MAX_PAYLOAD <= std::size_t(std::numeric_limits<std::int32_t>::max()));
            field = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(target) -
                                              reinterpret_cast<std::intptr_t>(origin));
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
                XOFFSET_THROW(XException(XErrc::invalid_state, "allocator binding has no arena"));
            }
            if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
                alignment > XBufferCore::MAX_PAYLOAD) {
                XOFFSET_THROW(XBadAlloc());
            }
            if (bytes > XBufferCore::MAX_PAYLOAD - std::max<std::size_t>(alignment, 8u))
                detail::throw_capacity_exceeded();
            return XBufferCore::owner_from_arena(arena).allocate_block(block_bytes_of(bytes), alignment);
        }

        // allocate_dynamic_bytes for an alignment known at compile time, of
        // at most 8 (every element type that takes the first-storage path of
        // XVectorCore::reserve): the same checks, errors and placement, with
        // the alignment tests folded away and allocate_block inlined at the
        // caller. Anything else takes allocate_dynamic_bytes.
        template <std::size_t Alignment>
        XOFFSET_DETAIL_INLINE void* allocate_dynamic_bytes_aligned(const allocator_ref& alloc_ref,
                                                                        std::uint32_t bytes) {
            static_assert(Alignment != 0 && (Alignment & (Alignment - 1)) == 0);
            if constexpr (Alignment > 8) {
                return allocate_dynamic_bytes(alloc_ref, bytes, Alignment);
            } else {
                auto* owner = XBufferCore::owner_from_arena_noexcept(alloc_ref.segment);
                if (!owner || bytes > XBufferCore::MAX_PAYLOAD - 8u) [[unlikely]]
                    return allocate_dynamic_bytes(alloc_ref, bytes, Alignment); // throws the usual error
                return owner->allocate_block(block_bytes_of(bytes), Alignment);
            }
        }

        // An empty vector's first block (a vector filled anew, as after a
        // reset) for elements aligned to at most 8: `bytes` (nonzero) from
        // the payload holding the head whose offset field is `data_delta`,
        // published there, with the checks, errors and placement of
        // allocate_dynamic_bytes_aligned (alignments up to 8 place a block
        // alike: every block is 8-aligned). Out of line and one copy for
        // every element type, so a message that fills vectors of several
        // types runs the same instructions for each. The common outcomes
        // (allocate_block's inline ones) call nothing and keep no frame;
        // any other request takes publish_first_block_general.
        [[noreturn]] XOFFSET_DETAIL_COLD inline void throw_vector_unbound() {
            XOFFSET_THROW(XException(XErrc::invalid_state, "XVector needs a block but lies outside every XOffset payload"));
        }
        XOFFSET_DETAIL_NOINLINE inline void* publish_first_block_general(std::int32_t& data_delta, std::uint32_t bytes) {
            auto* const segment = reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(&data_delta));
            if (!segment) [[unlikely]] throw_vector_unbound();
            auto* owner = XBufferCore::owner_from_arena_noexcept(segment);
            void* const block = !owner || bytes > XBufferCore::MAX_PAYLOAD - 8u
                ? allocate_dynamic_bytes(allocator_ref{segment}, bytes, 8u) // throws the usual error
                : owner->allocate_block(block_bytes_of(bytes), 8u);
            set_rel32_in_payload(data_delta, &data_delta, block);
            return block;
        }
        // In-band, publish_first_block off its bump: allocate_block of the
        // owner it found (its inline path searches the directory and splits).
        XOFFSET_DETAIL_NOINLINE inline void* publish_first_block_owned(XBufferCore& owner, std::int32_t& data_delta,
                                                                       std::uint32_t bytes) {
            void* const block = owner.allocate_block(block_bytes_of(bytes), 8u);
            set_rel32_in_payload(data_delta, &data_delta, block);
            return block;
        }
        XOFFSET_DETAIL_NOINLINE inline void* publish_first_block(std::int32_t& data_delta, std::uint32_t bytes) {
            auto* const segment = reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(&data_delta));
            auto* const owner = segment ? XBufferCore::owner_from_arena_noexcept(segment) : nullptr;
            // bytes - 1 < small_block_max is both request checks (bytes is
            // nonzero, so its block is (bytes + 7) & ~7). A payload with no
            // free block (each message built anew after a reset) bumps here
            // without a frame; any other takes allocate_block.
            if (!owner || bytes - 1u >= XBufferCore::small_block_max) [[unlikely]]
                return publish_first_block_general(data_delta, bytes);
            void* const block = owner->try_bump_first_at(reinterpret_cast<char*>(segment), (bytes + 7u) & ~7u);
            if (!block) [[unlikely]]
                return publish_first_block_owned(*owner, data_delta, bytes);
            set_rel32_in_payload(data_delta, &data_delta, block);
            return block;
        }

        // The same for a string's first out-of-line block (XString::
        // move_to_heap, alignment 1), whose head already holds its binding.
        // Inlined into move_to_heap, so a string's first block costs no call
        // (with a call here reloc str.assign_long took one more instruction
        // per assign, when GCC still inlined assign_general at every caller).
        XOFFSET_DETAIL_INLINE void* allocate_first_chars(const allocator_ref& alloc_ref, std::uint32_t bytes) {
            auto* owner = XBufferCore::owner_from_arena_noexcept(alloc_ref.segment);
            if (!owner || bytes > XBufferCore::MAX_PAYLOAD - 8u) [[unlikely]]
                return allocate_dynamic_bytes(alloc_ref, bytes, alignof(char)); // throws the usual error
            const std::uint32_t block = block_bytes_of(bytes);
            if (void* fresh = owner->try_bump_first(block)) [[likely]] return fresh;
            return owner->allocate_block(block, alignof(char));
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

        // Fused regrowth: a fresh block of
        // new_bytes, `fill` (moves the live run in, publishes the block and
        // returns the old one; must not throw), then the old block retired.
        // The binding and size checks run once; the owner is read again for
        // the retirement, since allocation may run new_handler, which may move
        // the owning Core. Placement equals allocate, then retire.
        template <typename Fill>
        XOFFSET_DETAIL_INLINE void regrow_dynamic_bytes(const allocator_ref& alloc_ref,
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

        // ---- fail-soft support ----------------------

        template <class E> std::true_type deep_is_vector(const XVectorCore<E>*);
        std::false_type deep_is_vector(...);

        // Arena bytes copying `value` into a payload can take beyond its own
        // slot (an upper bound): string blocks and container arrays, deeply.
        template <class T>
        std::size_t deep_bytes(const T& value) noexcept;
        template <class T, std::size_t... I>
        std::size_t deep_member_bytes(const T& value, std::index_sequence<I...>) noexcept {
            return (std::size_t(0) + ... + deep_bytes(schema::member_t<T, I>::get(value)));
        }
        template <class T, std::size_t... I>
        std::size_t deep_base_bytes(const T& value, std::index_sequence<I...>) noexcept {
            return (std::size_t(0) + ... + deep_bytes(static_cast<const schema::base_t<T, I>&>(value)));
        }
        template <class T>
        std::size_t deep_bytes(const T& value) noexcept {
            using U = std::remove_cvref_t<T>;
            if constexpr (std::is_base_of_v<XString, U>) {
                return value.size() == 0 ? 0 : std::size_t(value.size()) + 24u;
            } else if constexpr (std::is_convertible_v<const U&, std::string_view> && !std::is_arithmetic_v<U>) {
                const std::string_view view = value;
                return view.size() + 24u;
            } else if constexpr (std::is_trivially_copyable_v<U>) {
                return 0;
            } else if constexpr (decltype(deep_is_vector(static_cast<const U*>(nullptr)))::value) {
                using E = typename U::value_type;
                std::size_t bytes = std::size_t(value.size()) * sizeof(E) + alignof(E) + 24u;
                if constexpr (!std::is_trivially_copyable_v<E>)
                    for (const auto& element : value) bytes += deep_bytes(element);
                return bytes;
            } else if constexpr (std::is_class_v<U> && schema::is_described_v<U>) {
                return deep_base_bytes(value, std::make_index_sequence<schema::base_count_v<U>>{}) +
                       deep_member_bytes(value, std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                return 0;
            }
        }
        // Arena bytes for an array of `count` E (alignment and block rounding).
        template <class E>
        constexpr std::size_t array_bytes(std::size_t count) noexcept {
            // Saturates past the payload limit (any such request fails).
            if (count > (std::size_t(1) << 32)) return std::size_t(1) << 40;
            return count * sizeof(E) + alignof(E) + 24u;
        }
        // The capacity an empty vector takes when its first element is
        // appended: 4 elements, and at least 32 bytes. Elements smaller than
        // 8 bytes (scalars: a float, an id) so start with room for 32 bytes
        // of them, and the usual handful (5 to 8 floats) is appended without
        // moving to a second block: that second block cost an allocation, a
        // copy and a retirement per vector (CppCon 2024 read/write loop: 692
        // -> 398 instructions per equipment appended and removed). reserve()
        // and the range assignments size their block exactly, as before.
        template <class E>
        constexpr std::uint32_t first_capacity() noexcept {
            return sizeof(E) >= 8u ? 4u : static_cast<std::uint32_t>(32u / sizeof(E));
        }
        // ... of a container of `capacity` that grows by one element.
        template <class E>
        constexpr std::size_t grow_bytes(std::size_t capacity) noexcept {
            return array_bytes<E>(capacity ? capacity * 2u : first_capacity<E>());
        }

#if XOFFSET_NO_EXCEPTIONS
        // Inside a fail-soft region (a pin scope or a proxy operation, builds
        // without exceptions) an allocating container operation first makes
        // sure its allocations fit without moving the payload. When they do
        // not, the failure is recorded and the operation returns without
        // effect (a reference-returning one returns soft_sink<T>()) instead of
        // calling the fatal handler. Outside a region nothing changes.
        XOFFSET_DETAIL_NOINLINE inline bool soft_preflight_slow(const void* head, std::size_t bytes) noexcept {
            auto* segment = reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(head));
            auto* owner = segment ? XBufferCore::owner_from_arena_noexcept(segment) : nullptr;
            if (!owner) {
                (void)detail::soft_fail(XErrc::invalid_state);
                return false;
            }
            const XErrc code = owner->reserve_in_place(bytes);
            if (code == XErrc::ok) return true;
            (void)detail::soft_fail(code);
            return false;
        }
        XOFFSET_DETAIL_INLINE bool soft_preflight(const void* head, std::size_t bytes) noexcept {
            return !detail::soft_fail_active() || soft_preflight_slow(head, bytes);
        }
        // Where a failed reference-returning operation points: a
        // value-initialized T in host memory, per thread. Its containers lie
        // outside every payload, so operations on them fail softly too.
        template <class T>
        T& soft_sink() noexcept {
            alignas(T) static thread_local unsigned char storage[sizeof(T)];
            std::memset(storage, 0, sizeof(T));
            // Zero bytes are an empty element (empty containers, zero
            // values); a type with allocator constructors only is used as is.
            if constexpr (std::is_default_constructible_v<T>) return *::new (static_cast<void*>(storage)) T{};
            else return *std::launder(reinterpret_cast<T*>(storage));
        }
#define XOFFSET_SOFT_PREFLIGHT(head, bytes, ...) \
    do { if (!::XOffsetDatastructure::fixed_detail::soft_preflight((head), (bytes))) return __VA_ARGS__; } while (0)
// The preflight only when `cond` holds (a full container). Without
// exceptions only: with them the condition is not evaluated.
#define XOFFSET_SOFT_PREFLIGHT_IF(cond, head, bytes, ...) \
    do { if (cond) XOFFSET_SOFT_PREFLIGHT(head, bytes, __VA_ARGS__); } while (0)
#else
#define XOFFSET_SOFT_PREFLIGHT(head, bytes, ...) ((void)0)
#define XOFFSET_SOFT_PREFLIGHT_IF(cond, head, bytes, ...) ((void)0)
#endif

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

        // Owner lookup rule 5 (WIRE_FORMAT section 2): a container that destroys its
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

        // A shift erase (ordinary records) from the middle moves the tail of records down by one
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
        XOFFSET_DETAIL_INLINE void default_construct(T* dst, const Alloc& alloc) {
            if constexpr (std::is_array_v<T>) {
                std::uint32_t built = 0;
                XOFFSET_TRY {
                    for (; built < std::extent_v<T>; ++built) default_construct(&(*dst)[built], alloc);
                } XOFFSET_CATCH(...) {
                    destroy_n(*dst, built);
                    XOFFSET_RETHROW;
                }
            } else if constexpr (requires(const Alloc& value) { T(value); }) {
                std::construct_at(dst, alloc);
            } else if constexpr (!std::is_trivially_copyable_v<T> && std::is_class_v<T>) {
                detail::reflect_init_all<T>(static_cast<void*>(dst), alloc);
            } else {
                std::construct_at(dst);
            }
            detail::scrub_implicit_padding(dst);
        }

        template <typename T, typename Src, typename Alloc>
        void transfer_construct(T* dst, Src&& src, const Alloc& alloc) {
            using CleanSrc = std::remove_reference_t<Src>;
            if constexpr (std::is_array_v<T>) {
                std::uint32_t built = 0;
                XOFFSET_TRY {
                    for (; built < std::extent_v<T>; ++built) transfer_construct(&(*dst)[built], src[built], alloc);
                } XOFFSET_CATCH(...) {
                    destroy_n(*dst, built);
                    XOFFSET_RETHROW;
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
            detail::scrub_implicit_padding(dst);
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
                XOFFSET_TRY {
                    ((*dst = std::forward<Args>(args)), ...);
                } XOFFSET_CATCH(...) {
                    std::destroy_at(dst);
                    XOFFSET_RETHROW;
                }
            } else {
                std::construct_at(dst, std::forward<Args>(args)...);
            }
            detail::scrub_implicit_padding(dst);
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

        template <typename T>
        inline constexpr bool standard_int_v =
            std::is_integral_v<T> &&
            !std::is_same_v<std::remove_cv_t<T>, bool> &&
            !std::is_same_v<std::remove_cv_t<T>, char> &&
            !std::is_same_v<std::remove_cv_t<T>, char8_t> &&
            !std::is_same_v<std::remove_cv_t<T>, char16_t> &&
            !std::is_same_v<std::remove_cv_t<T>, char32_t> &&
            !std::is_same_v<std::remove_cv_t<T>, wchar_t>;

        template <typename L, typename R>
        bool ordered_less(const L& lhs, const R& rhs) {
            if constexpr (ordered_string_like<L> && ordered_string_like<R>) {
                return ordered_view(lhs) < ordered_view(rhs);
            } else if constexpr (enum_ordered_by_spaceship_only<L, R>) {
                return (lhs <=> rhs) < 0;
            } else if constexpr (standard_int_v<L> && standard_int_v<R> &&
                                 std::is_signed_v<L> != std::is_signed_v<R>) {
                return std::cmp_less(lhs, rhs);
            } else {
                return lhs < rhs;
            }
        }

        // A NaN key of a set or map with floating-point keys: every
        // comparison with it is false, so the flat array can neither place
        // it, find it nor keep its strict order (a receiver's full check
        // rejects such a wire). Tested on the bits, so -ffast-math cannot
        // fold it away. Other key types never get here.
        template <typename K, typename Arg>
        XOFFSET_DETAIL_INLINE bool nan_key(const Arg& key) noexcept {
            static_assert(std::is_floating_point_v<K>);
            if constexpr (!std::is_arithmetic_v<std::remove_cvref_t<Arg>>) {
                return false;
            } else {
                const K k = static_cast<K>(key);
                if constexpr (sizeof(K) == 4) {
                    return (std::bit_cast<std::uint32_t>(k) & 0x7fffffffu) > 0x7f800000u;
                } else {
                    static_assert(sizeof(K) == 8, "XSet/XMap: floating-point keys are float or double");
                    return (std::bit_cast<std::uint64_t>(k) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
                }
            }
        }
        // Refuses a NaN key: XException with invalid_argument, or in a
        // fail-soft region of a build without exceptions (proxies, pin
        // scopes) the recorded code, after which the insertion returns
        // without effect.
        XOFFSET_DETAIL_COLD inline void reject_nan_key() {
#if XOFFSET_NO_EXCEPTIONS
            if (detail::soft_fail(XErrc::invalid_argument)) return;
#endif
            XOFFSET_THROW(XException(XErrc::invalid_argument,
                "XSet/XMap: a NaN key is refused (it compares false with every key, so it has no place "
                "in the sorted order)"));
        }

        template <typename L, typename R>
        bool ordered_equal(const L& lhs, const R& rhs) {
            if constexpr (ordered_string_like<L> && ordered_string_like<R>) {
                return ordered_view(lhs) == ordered_view(rhs);
            } else if constexpr (standard_int_v<L> && standard_int_v<R> &&
                                 std::is_signed_v<L> != std::is_signed_v<R>) {
                return std::cmp_equal(lhs, rhs);
            } else {
                return lhs == rhs;
            }
        }

        // Is every key of [first, last) below the next one (the
        // XOFFSET_CHECK_SORTED check of assign_sorted)? key(*it) may refer
        // into a temporary *it: both are used within one full-expression.
        template <typename It, typename S, typename KeyFn>
        bool strictly_ascending(It first, S last, KeyFn key) {
            if (first == last) return true;
            for (It prev = first; ++first != last; prev = first) {
                if (!ordered_less(key(*prev), key(*first))) return false;
            }
            return true;
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

        // Where a sorted insert of `key` goes when key <= the last of `size`
        // (>= 1) elements: in [0, size - 1]. Its inserts append a key greater
        // than the last (ascending keys: a copy, a decode) after one compare,
        // so the found check needs no bound and the insert no append check.
        // Scalar keys halve over all `size` elements (the final compare is
        // false at the last); other keys search the first size - 1. The
        // inserts are always inlined: else Clang stops inlining XSet::insert.
        template <typename Elem, typename KeyLike, typename KeyFn>
        std::uint32_t insert_index_below_last(const Elem* data, std::uint32_t size,
                                              const KeyLike& key, KeyFn&& key_fn) {
            if constexpr (std::is_scalar_v<std::remove_cvref_t<decltype(key_fn(*data))>> &&
                          std::is_scalar_v<KeyLike>) {
                const Elem* base = data;
                std::uint32_t n = size;
                while (n > 1) {
                    const std::uint32_t half = n / 2;
                    base = ordered_less(key_fn(base[half - 1]), key) ? base + half : base;
                    n -= half;
                }
                return static_cast<std::uint32_t>(base - data) +
                       static_cast<std::uint32_t>(ordered_less(key_fn(*base), key));
            } else {
                return lower_bound_index(data, size - 1, key, key_fn);
            }
        }

        // The inline head of `count` (0..10) bytes at `src` as its two words
        // (little-endian): bytes 0..7, then bytes 8..10 with the length byte
        // 0x80 | count. Reads exactly `count` bytes. Built in registers: a
        // byte-filled stack buffer reloaded as words stalls store forwarding.
        XOFFSET_DETAIL_INLINE void inline_string_words(const char* src, std::uint32_t count, std::uint64_t& lo,
                                                       std::uint32_t& hi) noexcept {
            lo = 0;
            hi = 0;
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
        }
    } // namespace fixed_detail

    template <typename K, typename V>
    struct XKeyValue {
        using first_type = K;
        using second_type = V;
        K first{};
        V second{};
    };

    // N bytes a record keeps for fields of its later versions (type
    // evolution: docs/SCHEMA_EVOLUTION.md). A program writes only
    // zero into its own spare bytes (the default member initializer) and never
    // reads them; a newer peer keeps fields there, so no check requires them
    // to be zero. Alignment 1; its own token in the wire signature, so
    // XSpare<8> and std::uint8_t[8] are different layouts.
    template <std::size_t N>
    struct XSpare {
        static_assert(N > 0, "XSpare<N>: reserve at least one byte");
        std::uint8_t bytes[N]{};
    };

    // V2.1: a container head is {data_delta, size_, capacity_}, 12 bytes,
    // 4-aligned. It belongs to the payload it lies in: the owner is found from
    // the head's address (detail::OwnerMap), never stored in the head.
    class XString {
    public:
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;

        // A string of at most inline_max bytes may live in bytes 0..10 of
        // this head. Inline is marked by bit 31 of capacity_ (byte 11, which a
        // heap capacity < 2^31 never sets); bits 24..30 hold the length. The
        // byte after the last character and every unused inline byte are 0.
        static constexpr std::uint32_t inline_flag = 0x80000000u;
        static constexpr std::uint32_t inline_max = 10;

        std::int32_t data_delta = 0;
        std::uint32_t size_ = 0;
        std::uint32_t capacity_ = 0;

        XString() = default;
        // The allocator names the payload this head lies in; V2.1 reads that
        // from the head's address, so the argument is not stored.
        template <typename Alloc>
        explicit XString(const Alloc& alloc) noexcept(noexcept(fixed_detail::allocator_ref_of(alloc))) {
            static_cast<void>(fixed_detail::allocator_ref_of(alloc));
        }
        XOFFSET_DETAIL_INLINE ~XString() {
            if (is_inline() || data_delta == 0) [[likely]] {
                reset_head();
                return;
            }
            release_live();
        }

        // Move construction copies too; only move assignment (below) can
        // take the source's block, when both heads lie in one payload.
        XString(const XString& other)
            : XString() {
            *this = other;
        }

        XString(XString&& other)
            : XString(static_cast<const XString&>(other)) {}

        template <fixed_detail::string_view_like ViewLike, typename Alloc>
        XString(const ViewLike& value, const Alloc& alloc)
            : XString(alloc) {
            assign(std::string_view(value));
        }

        template <typename Alloc>
        XString(const XString& other, const Alloc& alloc)
            : XString(alloc) {
            *this = other;
        }

        XString& operator=(const XString& other) {
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
        XString& operator=(XString&& other) {
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
            return (*this = static_cast<const XString&>(other));
        }

        // Bound = the head lies in a registered payload. Each call is one
        // owner-table read: callers look the binding up only to allocate,
        // grow or retire a block, once per operation (WIRE_FORMAT, owner by address).
        bool is_bound() const noexcept { return arena() != nullptr; }

        allocator_type binding() const noexcept {
            return {arena()};
        }

        // One lookup that both checks and returns the binding.
        XOFFSET_DETAIL_INLINE allocator_type bound_binding() const {
            auto* const a = arena();
            if (!a) [[unlikely]] throw_unbound();
            return {a};
        }
        [[noreturn]] XOFFSET_DETAIL_COLD static void throw_unbound() {
            XOFFSET_THROW(XException(XErrc::invalid_state, "XString needs a block but lies outside every XOffset payload"));
        }

        XOFFSET_DETAIL_INLINE XArena* arena() const noexcept {
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
            return fixed_detail::resolve_rel32<const char>(&data_delta, data_delta);
        }

        std::uint32_t size() const noexcept { return is_inline() ? inline_size() : size_; }
        std::uint32_t capacity() const noexcept { return is_inline() ? inline_max : capacity_; }
        bool empty() const noexcept { return size() == 0; }

        std::string_view view() const noexcept {
            if (is_inline()) return std::string_view(inline_bytes(), inline_size());
            return data_delta ? std::string_view(
                fixed_detail::resolve_rel32<const char>(&data_delta, data_delta), size_)
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
            if constexpr (detail::scrub_erased) {
                // M4: old characters never stay behind as spare capacity.
                if (auto* ptr = data()) std::memset(ptr, 0, std::size_t(size_) + 1u);
            } else {
                // Only the terminator; the old characters stay in the block.
                if (auto* ptr = data()) ptr[0] = '\0';
            }
            size_ = 0;
        }

        void reserve(std::size_t requested) {
            XOFFSET_SOFT_PREFLIGHT(this, requested + 24u, );
            if (requested >= XBufferCore::MAX_PAYLOAD) detail::throw_capacity_exceeded();
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

            fixed_detail::set_rel32_in_payload(data_delta, &data_delta, new_data);
            capacity_ = new_cap;
        }

        XOFFSET_DETAIL_INLINE void assign(std::string_view rhs) {
            // An inline head, or a null one (as emplace_back() builds it),
            // takes a nonempty short value inline: the general path below
            // would do the same after its checks.
            bool inline_head = is_inline();
            if (!inline_head) {
                if (rhs.size() - 1u < capacity_) [[likely]] {
                    // Nonempty and fits the out-of-line block. capacity_ != 0
                    // without the inline flag implies a bound head with a live
                    // block, and capacity_ < MAX_PAYLOAD: the checks below are
                    // implied. rhs may alias the block (memmove).
                    auto* const chars = fixed_detail::resolve_rel32_live<char>(&data_delta, data_delta);
                    const auto new_size = static_cast<std::uint32_t>(rhs.size());
                    [[maybe_unused]] const auto old_size = size_;
                    std::memmove(chars, rhs.data(), new_size);
                    chars[new_size] = '\0';
                    // M4: zero the old characters after the terminator.
                    if constexpr (detail::scrub_erased)
                        if (old_size > new_size) std::memset(chars + new_size + 1, 0, old_size - new_size);
                    size_ = new_size;
                    return;
                }
                inline_head = data_delta == 0;
            }
            // rhs may alias the inline bytes (store_inline reads them all first).
            if (inline_head && rhs.size() - 1u < inline_max) [[likely]] {
                store_inline(rhs.data(), static_cast<std::uint32_t>(rhs.size()));
                return;
            }
            assign_general(rhs);
        }

    private:
        // assign() off its two fast paths, out of line on every compiler (as
        // Clang already needed to inline assign()), so a caller keeps only
        // assign()'s checks and copy inline.
        XOFFSET_DETAIL_NOINLINE void assign_general(std::string_view rhs) {
            if (rhs.size() > inline_max) XOFFSET_SOFT_PREFLIGHT(this, rhs.size() + 24u, );
            if (rhs.size() >= XBufferCore::MAX_PAYLOAD) detail::throw_capacity_exceeded();
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
                [[maybe_unused]] const auto old_size = size_;
                std::memmove(old_data, rhs.data(), rhs.size());
                if constexpr (detail::scrub_erased) {
                    // M4: zero the terminator and any old characters after it.
                    std::memset(old_data + new_size, 0, std::size_t(old_size > new_size ? old_size - new_size : 0) + 1u);
                } else {
                    old_data[new_size] = '\0';
                }
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
            XOFFSET_TRY {
                fixed_detail::set_rel32_in_payload(incoming_delta, &data_delta, fresh);
            } XOFFSET_CATCH(...) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, fresh, static_cast<std::uint32_t>(new_size + 1));
                XOFFSET_RETHROW;
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
        XString& operator=(std::string_view rhs) {
            assign(rhs);
            return *this;
        }

        XString& operator=(const char* rhs) {
            assign(rhs ? std::string_view(rhs) : std::string_view{});
            return *this;
        }

        // Appends rhs, which may view this string's own characters. Growth at
        // least doubles the capacity, so a run of appends is amortized O(1)
        // per character.
        XString& append(std::string_view rhs) {
            const auto count = rhs.size();
            if (!is_inline() && count - 1u < std::size_t(capacity_ - size_)) [[likely]] {
                // Nonempty and fits the out-of-line block (capacity_ != 0
                // without the inline flag: bound, live block, below
                // MAX_PAYLOAD).
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

        XString& operator+=(std::string_view rhs) { return append(rhs); }

        bool operator==(const XString& rhs) const noexcept { return view() == rhs.view(); }
        bool operator==(std::string_view rhs) const noexcept { return view() == rhs; }
        bool operator==(const char* rhs) const noexcept {
            return view() == (rhs ? std::string_view(rhs) : std::string_view{});
        }
        bool operator<(const XString& rhs) const noexcept { return view() < rhs.view(); }
        bool operator<(std::string_view rhs) const noexcept { return view() < rhs; }
        bool operator<(const char* rhs) const noexcept {
            return view() < (rhs ? std::string_view(rhs) : std::string_view{});
        }
        friend bool operator==(std::string_view lhs, const XString& rhs) noexcept {
            return lhs == rhs.view();
        }
        friend bool operator==(const char* lhs, const XString& rhs) noexcept {
            return (lhs ? std::string_view(lhs) : std::string_view{}) == rhs.view();
        }
        friend bool operator<(std::string_view lhs, const XString& rhs) noexcept {
            return lhs < rhs.view();
        }
        friend bool operator<(const char* lhs, const XString& rhs) noexcept {
            return (lhs ? std::string_view(lhs) : std::string_view{}) < rhs.view();
        }

        friend std::ostream& operator<<(std::ostream& os, const XString& value) {
            return os << value.view();
        }

        // Moves this head's payload into dst (a freshly bound head) and empties
        // this head. Inline bytes are copied; out-of-line storage is rebased.
        void relocate_payload_to(XString& dst) noexcept {
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
        // buffer reloaded as words stalled store forwarding. Always inline:
        // it is assign()'s short-value copy, and GCC called it out of line at
        // some callers once assign() was inlined there.
        XOFFSET_DETAIL_INLINE void store_inline(const char* src, std::uint32_t count) noexcept {
            std::uint64_t lo;
            std::uint32_t hi;
            fixed_detail::inline_string_words(src, count, lo, hi);
            std::memcpy(inline_bytes(), &lo, 8);
            std::memcpy(inline_bytes() + 8, &hi, 4);
        }

        // append() when the value is inline or null, or does not fit.
        XOFFSET_DETAIL_NOINLINE void append_general(std::string_view rhs) {
            if (rhs.empty()) return;
            const std::size_t old_size = size();
            XOFFSET_SOFT_PREFLIGHT(this, 2u * (old_size + rhs.size()) + 24u, );
            if (rhs.size() >= XBufferCore::MAX_PAYLOAD - old_size) detail::throw_capacity_exceeded();
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
            // the characters, the terminator, then zero bytes (inline
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
            auto* fresh = detail::begin_dynamic_array<char>(fixed_detail::allocate_first_chars(
                alloc, static_cast<std::uint32_t>(new_cap + 1)), std::size_t(new_cap) + 1u);
            std::int32_t incoming_delta = 0;
            XOFFSET_TRY {
                fixed_detail::set_rel32_in_payload(incoming_delta, &data_delta, fresh);
            } XOFFSET_CATCH(...) {
                fixed_detail::retire_dynamic_bytes(
                    alloc, fresh, static_cast<std::uint32_t>(new_cap + 1));
                XOFFSET_RETHROW;
            }
            if (count != 0) std::memmove(fresh, src, count);
            fresh[count] = '\0';
            data_delta = incoming_delta;
            size_ = count;
            capacity_ = new_cap;
        }

        XOFFSET_DETAIL_NOINLINE void release_live() noexcept {
            release_in(binding());
        }

        XOFFSET_DETAIL_INLINE void release() noexcept {
            if (is_inline() || data_delta == 0) [[likely]] {
                reset_head();
                return;
            }
            release_live();
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

    namespace fixed_detail {
        // XVectorCore's iterator: a pointer whose operator* returns T&. The
        // reference return tells the optimizer each element is dereferenceable,
        // so a user loop over a container can load fields unconditionally
        // (select, vectorization) as it does over std::vector or std::span; a
        // raw T* iterator keeps every conditional field load behind a branch.
        template <typename T>
        class contiguous_iterator {
            T* p_ = nullptr;

        public:
            using iterator_concept = std::contiguous_iterator_tag;
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::remove_cv_t<T>;
            using element_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            contiguous_iterator() = default;
            constexpr contiguous_iterator(T* p) noexcept : p_(p) {}
            template <typename U>
                requires(std::is_same_v<const U, T> && !std::is_same_v<U, T>)
            constexpr contiguous_iterator(contiguous_iterator<U> other) noexcept : p_(other.operator->()) {}

            constexpr T& operator*() const noexcept { return *p_; }
            constexpr T* operator->() const noexcept { return p_; }
            constexpr T& operator[](difference_type n) const noexcept { return p_[n]; }

            constexpr contiguous_iterator& operator++() noexcept { ++p_; return *this; }
            constexpr contiguous_iterator operator++(int) noexcept { auto t = *this; ++p_; return t; }
            constexpr contiguous_iterator& operator--() noexcept { --p_; return *this; }
            constexpr contiguous_iterator operator--(int) noexcept { auto t = *this; --p_; return t; }
            constexpr contiguous_iterator& operator+=(difference_type n) noexcept { p_ += n; return *this; }
            constexpr contiguous_iterator& operator-=(difference_type n) noexcept { p_ -= n; return *this; }

            friend constexpr contiguous_iterator operator+(contiguous_iterator i, difference_type n) noexcept { return i.p_ + n; }
            friend constexpr contiguous_iterator operator+(difference_type n, contiguous_iterator i) noexcept { return i.p_ + n; }
            friend constexpr contiguous_iterator operator-(contiguous_iterator i, difference_type n) noexcept { return i.p_ - n; }
            friend constexpr difference_type operator-(contiguous_iterator a, contiguous_iterator b) noexcept { return a.p_ - b.p_; }
            friend constexpr bool operator==(contiguous_iterator a, contiguous_iterator b) noexcept { return a.p_ == b.p_; }
            friend constexpr std::strong_ordering operator<=>(contiguous_iterator a, contiguous_iterator b) noexcept {
                return a.p_ <=> b.p_;
            }
        };
    } // namespace fixed_detail

    // The head and element operations that XVector, XSet and XMap share: a
    // base class only, never a field type.
    template <typename T>
    struct XVectorCore {
        using allocator_type = fixed_detail::allocator_ref;
        using stored_allocator_type = fixed_detail::stored_allocator_proxy;
        using value_type = T;
        using size_type = std::uint32_t;
        using iterator = fixed_detail::contiguous_iterator<T>;
        using const_iterator = fixed_detail::contiguous_iterator<const T>;

        // size_ and capacity_ are char32_t (32-bit unsigned, converts like
        // std::uint32_t) because it is a distinct type for type-based alias
        // analysis: a store to an int or unsigned element cannot alias them,
        // so a push_back loop keeps capacity_ in a register.
        std::int32_t data_delta = 0;
        char32_t size_ = 0;
        char32_t capacity_ = 0;

        XVectorCore() = default;
        // As XString: the head's address names its payload.
        explicit XVectorCore(allocator_type) noexcept {}
        XOFFSET_DETAIL_INLINE ~XVectorCore() {
            if (data_delta != 0) [[unlikely]] release_live();
        }
        XVectorCore(const XVectorCore& other, allocator_type alloc) : XVectorCore(alloc) {
            copy_from(other);
        }

        XVectorCore(const XVectorCore&) = delete;
        XVectorCore(XVectorCore&&) = delete;
        XVectorCore& operator=(const XVectorCore& other) {
            if constexpr (detail::subtree_copy_supported<T>() && std::is_trivially_copyable_v<T> &&
                          std::is_trivially_copy_constructible_v<T> &&
                          std::is_trivially_destructible_v<T> &&
                          !std::is_const_v<T> && !std::is_volatile_v<T>) {
                // POD in place: assign_range's in-place case and the M6
                // padding pass of assign_trivial_copy, inline. Regrowth,
                // binding and an empty source take the general path.
                const size_type count = other.size_;
                if (this != &other && count != 0 && count <= capacity_ &&
                    detail::subtree_copy_enabled()) [[likely]] {
                    T* items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                    [[maybe_unused]] const size_type old_size = size_;
                    std::memmove(static_cast<void*>(items), other.data(), std::size_t(count) * sizeof(T));
                    note_shrinking(count);
                    size_ = count;
                    if constexpr (detail::scrub_erased)
                        if (old_size > count)
                            std::memset(static_cast<void*>(items + count), 0, std::size_t(old_size - count) * sizeof(T));
                    for (size_type i = 0; i < count; ++i) detail::scrub_implicit_padding(items + i);
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
        XOFFSET_DETAIL_NOINLINE XVectorCore& assign_general_out_of_line(const XVectorCore& other) {
            return assign_general(other);
        }

        XVectorCore& assign_general(const XVectorCore& other) {
            if (this == &other) return *this;
            // An empty source keeps the capacity and needs no binding.
            if (other.size_ == 0) { clear(); return *this; }
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::deep_bytes(other) + fixed_detail::array_bytes<T>(other.size_), *this);
            // A trivially copyable element is one range copy; any other
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
        XVectorCore& operator=(XVectorCore&& other) {
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
            return copy_assign_out_of_line(other);
        }

    private:
        // A move between payloads copies, out of line, so the move above
        // (a relocation within one payload) stays small enough to inline.
        XOFFSET_DETAIL_NOINLINE XVectorCore& copy_assign_out_of_line(const XVectorCore& other) {
            return *this = other;
        }

    public:
        // Bound = the head lies in a registered payload. Each call is one
        // owner-table read: callers look the binding up only to allocate,
        // grow or retire a block, once per operation (WIRE_FORMAT, owner by address).
        bool is_bound() const noexcept { return arena() != nullptr; }

        allocator_type binding() const noexcept {
            return {arena()};
        }

        // One lookup that both checks and returns the binding.
        XOFFSET_DETAIL_INLINE allocator_type bound_binding() const {
            auto* const a = arena();
            if (!a) [[unlikely]] throw_unbound();
            return {a};
        }
        [[noreturn]] XOFFSET_DETAIL_COLD static void throw_unbound() {
            XOFFSET_THROW(XException(XErrc::invalid_state, "XVector needs a block but lies outside every XOffset payload"));
        }

        // Elements without heads never use the binding, so building one does
        // not look it up.
        allocator_type element_binding() const noexcept {
            if constexpr (std::is_trivially_copyable_v<T>) return {};
            else return binding();
        }
        // Default construction reads the binding only through a user
        // allocator constructor or a custom lifetime policy; the heads'
        // own allocator constructors ignore it (the head's address names its
        // payload). Without either, appending an empty element does no
        // owner lookup.
        allocator_type default_binding() const noexcept {
            if constexpr (std::is_trivially_copyable_v<T> ||
                          !fixed_detail::default_construct_reads_binding<T>()) return {};
            else return binding();
        }

        XOFFSET_DETAIL_INLINE XArena* arena() const noexcept {
            return reinterpret_cast<XArena*>(detail::OwnerMap::payload_of(this));
        }

        stored_allocator_type get_stored_allocator() const noexcept {
            return {arena()};
        }

        T* data() noexcept {
            return fixed_detail::resolve_rel32<T>(&data_delta, data_delta);
        }

        const T* data() const noexcept {
            return fixed_detail::resolve_rel32<const T>(&data_delta, data_delta);
        }

    private:
        T* live_data() noexcept {
            return fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
        }
        const T* live_data() const noexcept {
            return fixed_detail::resolve_rel32_live<const T>(&data_delta, data_delta);
        }

    public:

        size_type size() const noexcept { return size_; }
        size_type capacity() const noexcept { return capacity_; }
        bool empty() const noexcept { return size_ == 0; }

        // Element access requires a nonempty vector, whose head names a live
        // block: the address needs no empty-head (null) test.
        T& operator[](size_type index) noexcept { return live_data()[index]; }
        const T& operator[](size_type index) const noexcept { return live_data()[index]; }

        // An empty head has a null data() and size_ 0; adding 0 to a null
        // pointer is defined (null), so end() needs no test of its own.
        iterator begin() noexcept { return data(); }
        iterator end() noexcept { return data() + size_; }
        const_iterator begin() const noexcept { return data(); }
        const_iterator end() const noexcept { return data() + size_; }
        T& front() noexcept { return live_data()[0]; }
        const T& front() const noexcept { return live_data()[0]; }
        T& back() noexcept { return live_data()[size_ - 1]; }
        const T& back() const noexcept { return live_data()[size_ - 1]; }

        // truncate(0), with the old-size hook (note_cleared).
        void clear() noexcept {
            const auto old_size = size_;
            if (old_size == 0) return;
            if constexpr (!std::is_trivially_destructible_v<T>)
                fixed_detail::destroy_n_in(binding(), data(), old_size);
            size_ = 0;
            note_cleared(old_size);
            scrub(0, old_size);
        }

        // Destroys the elements from count on with one binding lookup (rule 5).
        void truncate(size_type count) noexcept {
            const auto old_size = size_;
            if (count >= old_size) return;
            if constexpr (!std::is_trivially_destructible_v<T>)
                fixed_detail::destroy_n_in(binding(), data() + count, old_size - count);
            size_ = count;
            note_shrunk(old_size, count);
            scrub(count, old_size);
        }

        // The no-op case inline at the caller (capacity_ <= max_size(), so
        // checked_count would not throw for it). An empty vector's first
        // storage (a vector filled anew, as after a reset) takes the
        // allocation with its alignment checks folded; every other growth
        // is reserve_grow, the body reserve had, unchanged.
        XOFFSET_DETAIL_INLINE void reserve(std::size_t requested) {
            if (requested <= capacity_) return;
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(requested), );
            reserve_first_or_grow(requested);
        }

    private:
        void reserve_first_or_grow(std::size_t requested) {
            if (capacity_ != 0 || requested > max_size()) return reserve_grow(requested);
            const auto new_cap = static_cast<size_type>(requested);
            // First storage: nothing to expand, move or retire.
            if constexpr (alignof(T) <= 8) {
                (void)detail::begin_dynamic_array<T>(fixed_detail::publish_first_block(
                    data_delta, static_cast<std::uint32_t>(new_cap * sizeof(T))), new_cap);
            } else {
                auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes_aligned<alignof(T)>(
                    bound_binding(), static_cast<std::uint32_t>(new_cap * sizeof(T))), new_cap);
                fixed_detail::set_rel32_in_payload(data_delta, &data_delta, fresh);
            }
            capacity_ = new_cap;
        }

        void reserve_grow(std::size_t requested) {
            auto new_cap = checked_count(requested);
            if (new_cap <= capacity_) return;
            const auto alloc = bound_binding();
            if (capacity_ == 0) {
                // First storage: nothing to expand, move or retire.
                auto* fresh = detail::begin_dynamic_array<T>(fixed_detail::allocate_dynamic_bytes(
                    alloc, static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T)), new_cap);
                fixed_detail::set_rel32_in_payload(data_delta, &data_delta, fresh);
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
            if (new_cap > max_size()) detail::throw_capacity_exceeded();
            fixed_detail::regrow_dynamic_bytes(alloc, static_cast<std::uint32_t>(capacity_ * sizeof(T)),
                                               static_cast<std::uint32_t>(new_cap * sizeof(T)), alignof(T),
                                               [&](void* block) -> void* {
                auto* fresh = detail::begin_dynamic_array<T>(block, new_cap);
                auto* old = data();
                relocate_live_run(fresh, old);
                fixed_detail::set_rel32_in_payload(data_delta, &data_delta, fresh);
                capacity_ = new_cap;
                return old;
            });
        }

    public:
        // Replaces all elements from stable live T objects. Self/subrange and
        // other live containers (including another arena) may overlap. Private
        // allocator control bytes that allocation itself mutates are not a
        // stable source range. No default construction or old-prefix copy is
        // needed for these implicit-lifetime, trivially destructible objects.
        XOFFSET_DETAIL_INLINE void assign_range(std::span<const T> values)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            assign_bytes(values.data(), values.size());
        }
        // Append stable live objects, including a subrange of this vector.
        // A growing append reads both ranges before it retires the old block.
        XOFFSET_DETAIL_INLINE void append_range(std::span<const T> values)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            const auto added = values.size();
            if (added == 0) return;
            const auto old_size = size_;
            if (added <= std::size_t(capacity_ - old_size)) [[likely]] {
                auto* tail = live_data() + old_size;
                std::memmove(tail, values.data(), values.size_bytes());
                for (std::size_t i = 0; i < added; ++i) detail::scrub_implicit_padding(tail + i);
                size_ = static_cast<size_type>(old_size + added);
                return;
            }
            append_range_grow(values);
        }

        XOFFSET_DETAIL_NOINLINE void append_range_grow(std::span<const T> values)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            const auto old_size = size_;
            const auto added = checked_count(values.size());
            if (added > max_size() - old_size) detail::throw_capacity_exceeded();
            const auto wanted = static_cast<size_type>(old_size + added);
            const auto new_capacity = std::max(wanted, grown_capacity());
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(new_capacity), );
            const auto* old = data();
            rebuild(wanted, new_capacity, [&](T* dst, size_type i) {
                const auto& element = i < old_size ? old[i] : values[i - old_size];
                std::construct_at(dst, element);
                detail::scrub_implicit_padding(dst);
            });
        }

        // assign_range from the object representations of count elements at
        // src, which need not be aligned for T (a message or file buffer): the
        // bytes are copied as they are (memmove) and no T* to src is formed.
        // The same overlap rules as assign_range.
        XOFFSET_DETAIL_INLINE void assign_bytes(const void* src, std::size_t count)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            // One unsigned compare handles the steady-state case: 0 < count <= capacity_.
            // capacity_ <= max_size() is an invariant, so count fits in size_type,
            // checked_count would not throw, and data_delta != 0 (live block).
            if (count - 1u < std::size_t(capacity_)) [[likely]] {
                const auto n = static_cast<size_type>(count);
                const auto bytes = static_cast<std::uint32_t>(n * sizeof(T));
                [[maybe_unused]] const auto old_size = size_;
                std::memmove(live_data(), src, bytes);
                note_shrinking(n);
                size_ = n;
                scrub(n, old_size);
                return;
            }
            // An empty vector's exact first block stays inline so fixed-size
            // ranges (e.g. assign_range(float[8])) fold count checks and
            // memmove into inline SIMD stores with a single publish_first_block call.
            if constexpr (alignof(T) <= 8) {
                if (capacity_ == 0 && count - 1u < std::size_t(max_size())) {
                    XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(count), );
                    const auto n = static_cast<size_type>(count);
                    const auto bytes = static_cast<std::uint32_t>(n * sizeof(T));
                    T* const fresh = detail::begin_dynamic_array<T>(
                        fixed_detail::publish_first_block(data_delta, bytes), n);
                    capacity_ = n;
                    size_ = n;
                    std::memmove(fresh, src, bytes);
                    return;
                }
            }
            assign_bytes_slow(src, count);
        }

        // Empty-count clear, over-max-size throw, over-aligned first block, and
        // regrowth stay out of line so the fast paths above inline cleanly with
        // no exception landing pads or checked_count calls at hot callers.
        XOFFSET_DETAIL_NOINLINE void assign_bytes_slow(const void* src, std::size_t count)
            requires (!std::is_const_v<T> && !std::is_volatile_v<T> &&
                      std::is_trivially_copyable_v<T> &&
                      std::is_trivially_copy_constructible_v<T> &&
                      std::is_trivially_destructible_v<T>) {
            if (count == 0) { clear(); return; }
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(count), );
            const auto n = checked_count(count);
            const auto bytes = static_cast<std::uint32_t>(n * sizeof(T));
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(std::max<std::size_t>(n, 2u * capacity_)), );
            assign_range_regrow(src, n, bytes);
        }

        // assign_bytes into a fresh block. Inline in assign_bytes_slow, its
        // only caller, which already keeps it off the inline path above: a
        // second call level cost each regrowth ~7 instructions (formal.mutate,
        // slg.A.apply).
        XOFFSET_DETAIL_INLINE void assign_range_regrow(const void* src, size_type count,
                                                         std::uint32_t bytes) {
            const auto alloc = bound_binding();
            // A container that already has capacity regrows to
            // max(count, 2 * capacity), like libc++ assign; the first block
            // stays exact.
            const size_type new_cap = capacity_ == 0 ? count
                : std::max(count, static_cast<size_type>(
                      std::min<std::uint64_t>(max_size(), std::uint64_t(capacity_) * 2u)));
            const auto cap_bytes = static_cast<std::uint32_t>(new_cap * sizeof(T));
            // Read the complete source before publishing/retiring old storage.
            // Retirement is allowed to overwrite its first bytes as free-list
            // metadata. memmove also covers overlapping live-element ranges.
            fixed_detail::regrow_dynamic_bytes(alloc, static_cast<std::uint32_t>(capacity_ * sizeof(T)),
                                               cap_bytes, alignof(T), [&](void* block) -> void* {
                auto* fresh = detail::begin_dynamic_array<T>(block, new_cap);
                std::memmove(fresh, src, bytes);
                void* old = data();
                fixed_detail::set_rel32_in_payload(data_delta, &data_delta, fresh);
                size_ = count;
                capacity_ = new_cap;
                return old;
            });
        }

        // Replaces all elements with [first, last). A contiguous range of
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
                const auto distance = static_cast<std::size_t>(std::ranges::distance(first, last));
                XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(distance), );
                const auto count = checked_count(distance);
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
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::grow_bytes<T>(capacity_) + fixed_detail::deep_bytes(value), );
            const Grown grown = push_back_full(value);
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
                    XOFFSET_THROW(XException(XErrc::invalid_state, "stable_erase_if target changed during decisions"));
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
            note_shrunk(original_size, write);
            scrub(write, original_size);
            return removed;
        }

        // pop_back() leaves the removed element's bytes in spare capacity,
        // its container heads empty (a send copies them as they are; the
        // next append there constructs over them). Its owned blocks are
        // still retired and zeroed (M3). XOFFSET_SCRUB_POPPED=1 zeroes the slot too (M4, for
        // buffers sent to readers that must not see removed elements;
        // save_compact also rewrites spare capacity as zero).
        XOFFSET_DETAIL_INLINE void pop_back() {
            if (size_ == 0) return;
            // size_ > 0 names a live block: no null test.
            const size_type last = size_ - 1u;
            T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + last;
            fixed_detail::destroy_n(slot, 1);
            size_ = last;
            note_popped(last);
            if constexpr (detail::scrub_popped) std::memset(static_cast<void*>(slot), 0, sizeof(T));
            else if constexpr (detail::settles_vacated_heads<T>()) detail::empty_vacated_slot(slot);
        }

        XOFFSET_DETAIL_INLINE T& emplace_back() {
            const size_type n = size_;
            if (n < capacity_) [[likely]] {
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
                fixed_detail::default_construct(slot, default_binding());
                size_ = n + 1u;
                return *slot;
            }
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::grow_bytes<T>(capacity_), fixed_detail::soft_sink<T>());
            return emplace_back_general();
        }

        template <typename... Args>
            requires (sizeof...(Args) > 0)
        T& emplace_back(Args&&... args) {
            XOFFSET_SOFT_PREFLIGHT_IF(size_ >= capacity_, this, fixed_detail::grow_bytes<T>(capacity_) +
                                      (std::size_t(0) + ... + fixed_detail::deep_bytes(args)),
                                      fixed_detail::soft_sink<T>());
            if constexpr (std::is_trivially_copyable_v<T>) {
                // The append fast path at the caller; a full vector stages
                // the element (Staged), so no reference to args leaves this
                // function.
                const size_type n = size_;
                if (n < capacity_) [[likely]] {
                    T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
                    fixed_detail::construct_bound(slot, element_binding(), std::forward<Args>(args)...);
                    size_ = n + 1u;
                    return *slot;
                }
                XOFFSET_INDETERMINATE Staged staged;
                fixed_detail::construct_bound(staged.get(), element_binding(), std::forward<Args>(args)...);
                T* const slot = grow_back_slot();
                std::memcpy(static_cast<void*>(slot), staged.bytes, sizeof(T));
                return *slot;
            } else {
                return *append_construct([&](T* dst) {
                    fixed_detail::construct_bound(dst, element_binding(), std::forward<Args>(args)...);
                });
            }
        }

    protected:
        // Only a full vector (or an unbound head) gets here, so insert_body
        // directly: through insert_construct, Clang called insert_general from
        // here, a second call level and the append and insert fast paths
        // tested again on each growth (slg.A.simulate +2.3%, slg.C +0.5%).
        XOFFSET_DETAIL_NOINLINE T& emplace_back_general() {
            auto construct = [&](T* dst) {
                fixed_detail::default_construct(dst, default_binding());
            };
            return *insert_body<false>(size_, construct);
        }
        struct Grown {
            std::int32_t data_delta;
            char32_t capacity;
        };

    public:
        // push_back's full case for a fast proxy, which keeps the size in a
        // register: it restates the head from the result (as push_back does)
        // and needs no extra call level on each growth. Declared with its
        // return type so that walking the members of a recursive element
        // type does not instantiate the body.
        // A larger trivially copyable value is staged here, before the
        // growth (it may lie in the block the growth retires), and copied
        // into the new slot after it: with a reference to it passed out of
        // line, a temporary the caller built would have to live in memory
        // on the in-capacity path too (Staged).
        Grown push_back_full(const T& value) {
            if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 16) {
                return push_back_general<T>(value);
            } else if constexpr (std::is_trivially_copyable_v<T> && !std::is_array_v<T>) {
                XOFFSET_INDETERMINATE Staged staged;
                std::memcpy(staged.bytes, static_cast<const void*>(std::addressof(value)), sizeof(T));
                T* const slot = grow_back_slot();
                std::memcpy(static_cast<void*>(slot), staged.bytes, sizeof(T));
                detail::scrub_implicit_padding(slot);
                return {data_delta, capacity_};
            } else {
                return push_back_general<const T&>(value);
            }
        }

    protected:

        // push_back beyond capacity or on an unbound head. A small trivially
        // copyable value arrives by value (registers), not by reference. The
        // head fields it changed come back in registers for push_back to
        // restate.
        template <typename V>
        XOFFSET_DETAIL_NOINLINE Grown push_back_general(V value) {
            if constexpr (std::is_same_v<V, T> && std::is_trivially_copyable_v<T> && !std::is_array_v<T>) {
                // The value arrived by value, so it cannot lie in the block
                // this growth retires: grow, then store, with no staged copy
                // and none of the insert checks. An empty vector takes its
                // first block through reserve's first-storage path.
                const size_type n = size_;
                if (n >= capacity_) [[likely]] reserve_first_or_grow(grown_capacity());
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
                fixed_detail::transfer_construct(slot, static_cast<const T&>(value), element_binding());
                size_ = n + 1u;
                return {data_delta, capacity_};
            }
            append_construct([&](T* dst) {
                fixed_detail::transfer_construct(dst, static_cast<const T&>(value), element_binding());
            });
            return {data_delta, capacity_};
        }

        static size_type checked_count(std::size_t count) {
            if (count > max_size()) detail::throw_capacity_exceeded();
            return static_cast<size_type>(count);
        }

        static constexpr size_type max_size() noexcept {
            return static_cast<size_type>((XBufferCore::MAX_PAYLOAD - 8u) / sizeof(T));
        }

        size_type grown_capacity() const {
            if (size_ == max_size()) detail::throw_capacity_exceeded();
            return std::max(size_ + 1u, static_cast<size_type>(
                std::min<std::uint64_t>(max_size(), capacity_ ? std::uint64_t(capacity_) * 2u
                                                                  : fixed_detail::first_capacity<T>())));
        }

        template <bool OutOfLine>
        void grow_for_insert() {
            if constexpr (OutOfLine) reserve_for_insert(grown_capacity());
            else reserve_grow(grown_capacity());
        }
        XOFFSET_DETAIL_NOINLINE void reserve_for_insert(size_type new_cap) { reserve_grow(new_cap); }

        // Build in arena storage and publish only after all potentially throwing work.
        template <typename Construct>
        void rebuild(size_type count, size_type cap, Construct&& construct) {
            const auto alloc = bound_binding();
            if (cap > max_size() || count > cap) detail::throw_capacity_exceeded();
            if (cap == 0) { clear(); return; }
            auto bytes = static_cast<std::uint32_t>(cap * sizeof(T));
            auto* fresh = detail::begin_dynamic_array<T>(
                fixed_detail::allocate_dynamic_bytes(alloc, bytes, alignof(T)), cap);
            size_type constructed = 0;
            std::int32_t delta = 0;
            XOFFSET_TRY {
                fixed_detail::set_rel32_in_payload(delta, &data_delta, fresh);
                for (; constructed < count; ++constructed) construct(fresh + constructed, constructed);
            } XOFFSET_CATCH(...) {
                fixed_detail::destroy_n(fresh, constructed);
                fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
                XOFFSET_RETHROW;
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

        // Replaces all elements with count elements built in order by
        // construct(dst, *it) from [first, last): the sorted bulk fills of
        // XSet and XMap (one exact block, no lookup). An
        // empty vector builds in place, growing first if needed; otherwise a
        // fresh exact block is built before the old one is released, so the
        // source may alias this container, as for assign().
        template <typename It, typename S, typename Construct>
        void fill_construct(It first, S /*last*/, std::size_t count, Construct&& construct) {
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(count), );
            const auto n = checked_count(count);
            if (n == 0) { clear(); return; }
            if (size_ != 0) {
                rebuild(n, n, [&](T* dst, size_type) {
                    construct(dst, *first);
                    ++first;
                });
                return;
            }
            reserve(n);
            T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
            size_type built = 0;
            XOFFSET_TRY {
                for (; built < n; ++built, ++first) construct(items + built, *first);
            } XOFFSET_CATCH(...) {
                fixed_detail::destroy_n(items, built);
                XOFFSET_RETHROW;
            }
            size_ = n;
        }

        // Appends (emplace_back with arguments, push_back_general) keep the
        // general path in the same function: under Clang a call to it there
        // costs more than it saves (slg.A). GCC and MSVC take insert_construct
        // as before. emplace_back() without arguments differs on every
        // compiler: its fast path is at the caller and a full vector grows in
        // emplace_back_general.
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
        XOFFSET_DETAIL_INLINE iterator append_construct(Construct&& construct) {
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
#if defined(__clang__) || defined(_MSC_VER)
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

        // A trivially copyable element staged at the caller. The single-
        // element inserts build it inline, call out of line only to grow
        // (grow_back_slot, grow_gap_slot: no element argument), then copy it
        // into the slot the growth returns. No reference to the caller's
        // arguments reaches a function that is not inlined, so a temporary
        // the caller passed (an Item{...} built field by field) stays in
        // registers, on the in-capacity path and across the growth call.
        // Taken by reference, it had to live in memory on every call, and
        // the element copy then waited on the narrower stores that built it
        // (store-to-load forwarding); passed by value above 16 bytes, it is
        // stored to the argument area and reloaded the same way. Staging
        // before the growth keeps an argument that aliases an old element
        // valid: the growth retires (and zeroes) the old block.
        struct Staged {
            alignas(T) std::byte bytes[sizeof(T)];
            T* get() noexcept { return reinterpret_cast<T*>(bytes); }
        };

        // One more element at size_ in a full (or unbound) vector: grows,
        // counts the new element and returns its slot for the caller to fill
        // without a throw in between. An empty vector takes its first block
        // through reserve's first-storage path.
        XOFFSET_DETAIL_NOINLINE T* grow_back_slot() {
            const size_type n = size_;
            if (n >= capacity_) [[likely]] reserve_first_or_grow(grown_capacity());
            T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
            size_ = n + 1u;
            return slot;
        }

        // One more element at index < size_ in a full vector: grows, moves
        // the tail up one slot, counts the new element and returns the gap.
        XOFFSET_DETAIL_NOINLINE T* grow_gap_slot(size_type index) {
            reserve_grow(grown_capacity());
            const size_type n = size_;
            T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
            size_ = n + 1u;
            std::memmove(static_cast<void*>(items + index + 1), static_cast<const void*>(items + index),
                         std::size_t(n - index) * sizeof(T));
            return items + index;
        }

        // insert_construct at size_ (a sorted insert after the last element),
        // all compilers. Only the spare-capacity path is inline: with more
        // here GCC stops inlining XMap::operator[] or zero_implicit_padding.
        template <typename Construct>
        XOFFSET_DETAIL_INLINE_MSVC iterator append_after_last_construct(Construct&& construct) {
            const size_type n = size_;
            if (n < capacity_) [[likely]] {
                T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + n;
                construct(slot);
                size_ = n + 1u;
                return slot;
            }
            if constexpr (std::is_trivially_copyable_v<T>) {
                XOFFSET_INDETERMINATE Staged staged;
                construct(staged.get());
                T* const slot = grow_back_slot();
                std::memcpy(static_cast<void*>(slot), staged.bytes, sizeof(T));
                return slot;
            } else {
                return append_full_construct(construct);
            }
        }

        // append_after_last_construct into a full vector, elements with heads.
        template <typename Construct>
        XOFFSET_DETAIL_NOINLINE iterator append_full_construct(Construct& construct) {
            return insert_body<false>(size_, construct);
        }

        // insert_construct for an index below size_, without its append
        // check: into spare capacity inline on every compiler (the checks of
        // insert_body are implied); only a full vector calls out to grow.
        template <bool GrowOutOfLine = false, typename Construct>
        XOFFSET_DETAIL_INLINE_MSVC iterator insert_before_end_construct(size_type index, Construct&& construct) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                const size_type n = size_;
                XOFFSET_INDETERMINATE Staged staged;
                construct(staged.get());
                if (n < capacity_) [[likely]] {
                    T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                    size_ = n + 1u;
                    std::memmove(static_cast<void*>(items + index + 1), static_cast<const void*>(items + index),
                                 std::size_t(n - index) * sizeof(T));
                    std::memcpy(static_cast<void*>(items + index), staged.bytes, sizeof(T));
                    return items + index;
                }
                T* const slot = grow_gap_slot(index);
                std::memcpy(static_cast<void*>(slot), staged.bytes, sizeof(T));
                return slot;
            } else {
#if defined(__clang__)
                return insert_general<GrowOutOfLine>(index, construct);
#else
                return insert_body<GrowOutOfLine>(index, construct);
#endif
            }
        }

        // insert_construct off its fast paths. Clang inlines the fast paths
        // at the caller only with this part in a separate function.
        template <bool GrowOutOfLine, typename Construct>
        iterator insert_general(size_type index, Construct& construct) {
            return insert_body<GrowOutOfLine>(index, construct);
        }

        template <bool GrowOutOfLine, typename Construct>
        XOFFSET_DETAIL_INLINE iterator insert_body(size_type index, Construct& construct) {
            if (index > size_ || size_ == max_size()) detail::throw_capacity_exceeded();
            if (index == size_ && append_grow(construct)) {
                // Append into a full vector: built in place in the grown block.
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                // A native temporary is safe only for types without heads.
                alignas(T) std::byte storage[sizeof(T)];
                auto* staged = reinterpret_cast<T*>(storage);
                construct(staged);
                if (size_ == capacity_) grow_for_insert<GrowOutOfLine>();
                // An append moves nothing.
                T* const items = data();
                if (index != size_)
                    std::memmove(items + index + 1, items + index, (size_ - index) * sizeof(T));
                std::memcpy(items + index, staged, sizeof(T));
                ++size_;
            } else if constexpr (detail::shift_relocate_supported<T>()) {
                with_staged(construct, [&](T& staged) {
                    if (size_ == capacity_) grow_for_insert<GrowOutOfLine>();
                    // Ordinary records: same bytes and deltas as the loop below, in one
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

        // One hook for every branch below (a second inline copy of it was
        // enough for clang to stop inlining the flat map and set erase).
        XOFFSET_DETAIL_INLINE_MSVC void erase_index(size_type index) {
            if (index >= size_) return;
            if (index + 1 == size_) {
                // With XOFFSET_SCRUB_ERASED, erase zeroes the slot when
                // pop_back does not.
                pop_last();
                if constexpr (detail::scrub_erased && !detail::scrub_popped) scrub(size_, size_ + 1);
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                // size_ > 1 names a live block. The head stays in registers
                // across the move (compilers reload it after the memmove
                // otherwise); the vacated slot is zeroed as by scrub(n - 1, n).
                const size_type n = size_;
                T* const items = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta);
                std::memmove(items + index, items + index + 1, std::size_t(n - index - 1) * sizeof(T));
                size_ = n - 1u;
                if constexpr (detail::scrub_erased) std::memset(static_cast<void*>(items + (n - 1u)), 0, sizeof(T));
            } else if constexpr (detail::shift_relocate_supported<T>()) {
                // Ordinary records: same bytes and deltas as the loop below, in one
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
                return;
            }
            note_popped(size_);
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
                XOFFSET_TRY {
                    fixed_detail::set_rel32_in_payload(delta, &data_delta, fresh);
                    construct(fresh + size_);
                } XOFFSET_CATCH(...) {
                    fixed_detail::retire_dynamic_bytes(alloc, fresh, new_bytes);
                    XOFFSET_RETHROW;
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
        // kept). Ordinary records: one memcpy plus an inline-head rebase by the
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
            } else if constexpr (std::is_trivially_copyable_v<T> && !std::is_array_v<T> &&
                                 sizeof(T) <= 16) {
                // Small plain elements (a float, an id, a 12-byte point): one
                // block copy, the bytes relocate_construct copies one element
                // at a time. The loop below reloads size_ on every iteration
                // (a store through `fresh` may alias the head) and takes 5
                // instructions per 4-byte element. Larger elements keep the
                // loop: no measured cell gained from lifting the limit.
                std::memcpy(static_cast<void*>(fresh), static_cast<const void*>(old),
                            std::size_t(size_) * sizeof(T));
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
            XOFFSET_TRY {
                construct(staged);
                built = true;
                commit(*staged);
            } XOFFSET_CATCH(...) {
                if (built) std::destroy_at(staged);
                fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                XOFFSET_RETHROW;
            }
            std::destroy_at(staged);
            fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
        }

        XOFFSET_DETAIL_INLINE bool try_resize_trivial_in_place(std::size_t count) noexcept {
            if constexpr (std::is_trivially_destructible_v<T>) {
                const auto old_size = size_;
                if (count <= old_size) {
                    const auto n = static_cast<size_type>(count);
                    size_ = n;
                    note_shrunk(old_size, n);
                    scrub(n, old_size);
                    return true;
                }
                if constexpr (std::is_trivially_copyable_v<T> && std::is_nothrow_default_constructible_v<T>) {
                    if (count <= capacity_) {
                        const auto n = static_cast<size_type>(count);
                        T* const items = live_data();
                        std::uninitialized_value_construct_n(items + old_size, n - old_size);
                        for (size_type i = old_size; i < n; ++i) detail::scrub_implicit_padding(items + i);
                        size_ = n;
                        return true;
                    }
                }
            }
            return false;
        }

        // The default resize of scalar T (T{} and destruction run no user code;
        // fill/custom construction keeps the general transactional path below).
        // Within capacity it is inline at the caller: one compare, then shrink or
        // value-fill. Growth is out of line and goes straight to the new block:
        // its caller already knows count > capacity_ (a vector filled anew, as
        // the formal harness's assign_sequence does, pays one compare and the
        // call, not a second round of the size and capacity tests).
        XOFFSET_DETAIL_INLINE void resize_default_scalar(std::size_t count) requires std::is_scalar_v<T> {
            if (count > capacity_) [[unlikely]] {
                resize_default_scalar_grow(count);
                return;
            }
            // count <= capacity_ <= max_size(): it fits size_type, and a count
            // above size_ names a live block.
            const auto n = static_cast<size_type>(count);
            const auto old_size = size_;
            if (n <= old_size) {
                size_ = n;
                note_shrunk(old_size, n);
                scrub(n, old_size);
                return;
            }
            std::uninitialized_value_construct_n(live_data() + old_size, n - old_size);
            size_ = n;
        }

        XOFFSET_DETAIL_NOINLINE void resize_default_scalar_grow(std::size_t requested) requires std::is_scalar_v<T> {
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(std::max<std::size_t>(requested, 2u * capacity_)), );
            const auto count = checked_count(requested);
            const auto old_size = size_;
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
            if (count > max_size()) detail::throw_capacity_exceeded();
            if (count <= size_) {
                truncate(count);
            } else if (count <= capacity_) {
                auto old_size = size_;
                XOFFSET_TRY {
                    while (size_ < count) { construct(data() + size_); ++size_; }
                } XOFFSET_CATCH(...) {
                    const auto built = size_;
                    while (size_ > old_size) pop_back();
                    scrub(old_size, built);
                    XOFFSET_RETHROW;
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
                XOFFSET_TRY {
                    fixed_detail::set_rel32_in_payload(delta, &data_delta, fresh);
                    for (; tail_built < count - old_size; ++tail_built)
                        construct(fresh + old_size + tail_built);
                } XOFFSET_CATCH(...) {
                    fixed_detail::destroy_n(fresh + old_size, tail_built);
                    fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
                    XOFFSET_RETHROW;
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
        // Spare watch (XBufferCore::note_spare_head): a shrink in place that
        // takes the size from above a quarter of the capacity to a quarter or
        // below registers the head, when the capacity holds at least
        // spare_watch_min_bytes. Growth leaves the size above half of the
        // capacity, so the crossing follows a real shrink. Inline, each hook
        // is one compare (the capacity, or the old size) and a branch; the
        // rest is one more compare (one element removed) or out of line.
        // A function, not a data member: MSVC evaluates a static data
        // member's initializer when the class is instantiated, where T may
        // still be incomplete (a node holding an XVector of itself).
        static constexpr size_type spare_watch_min_capacity() noexcept {
            return static_cast<size_type>((XBufferCore::spare_watch_min_bytes + sizeof(T) - 1u) / sizeof(T));
        }
        // pop_back without the watch, for erase_index (one hook after its
        // branches): size_ > 0, which names a live block (no null test).
        size_type pop_last() {
            const size_type last = size_ - 1u;
            T* const slot = fixed_detail::resolve_rel32_live<T>(&data_delta, data_delta) + last;
            fixed_detail::destroy_n(slot, 1);
            size_ = last;
            if constexpr (detail::scrub_popped) std::memset(static_cast<void*>(slot), 0, sizeof(T));
            else if constexpr (detail::settles_vacated_heads<T>()) detail::empty_vacated_slot(slot);
            return last;
        }
        // One element removed (pop_back, erase): the size passes every value,
        // so the crossing is the size reaching a quarter. The compiler fence
        // makes the quarter test read the capacity again, so the capacity test
        // compares memory directly instead of loading a register for both
        // (one instruction fewer on every pop of a small container).
        XOFFSET_DETAIL_INLINE void note_popped(size_type new_size) const noexcept {
            if (capacity_ >= spare_watch_min_capacity()) [[unlikely]] {
                std::atomic_signal_fence(std::memory_order_seq_cst);
                if (new_size == (static_cast<size_type>(capacity_) >> 2)) [[unlikely]]
                    XBufferCore::note_spare_head(this, static_cast<std::uint32_t>(sizeof(T)));
            }
        }
        // Several removed (truncate, resize, remove): the capacity test
        // inline, the crossing test out of line.
        XOFFSET_DETAIL_INLINE void note_shrunk(size_type old_size, size_type new_size) const noexcept {
            if (capacity_ >= spare_watch_min_capacity()) [[unlikely]] note_shrunk_cold(old_size, new_size);
        }
        // All removed (clear): a crossing needs the old size, which is in a
        // register, above a quarter of the minimum capacity, so the inline
        // test reads no memory (a capacity load there is one a refill loop
        // after the clear may keep, which cost reloc's push_back loop an
        // instruction per element).
        XOFFSET_DETAIL_INLINE void note_cleared(size_type old_size) const noexcept {
            if (old_size > spare_watch_min_capacity() / 4u) [[unlikely]] note_shrunk_cold(old_size, 0);
        }
        XOFFSET_DETAIL_COLD void note_shrunk_cold(size_type old_size, size_type new_size) const noexcept {
            if (capacity_ < spare_watch_min_capacity()) return;
            const size_type quarter = static_cast<size_type>(capacity_) >> 2;
            if (new_size <= quarter && old_size > quarter)
                XBufferCore::note_spare_head(this, static_cast<std::uint32_t>(sizeof(T)));
        }
        // An assign in place (any new size), after the copy and before the
        // size is stored: the capacity test inline, compared with memory
        // after the copy; out of line the old size is read from the head, so
        // no register holds it across the copy. The out-of-line part stores
        // the new size itself before it registers (the caller stores the same).
        XOFFSET_DETAIL_INLINE void note_shrinking(size_type new_size) noexcept {
            if (capacity_ >= spare_watch_min_capacity()) [[unlikely]] note_shrinking_cold(new_size);
        }
        XOFFSET_DETAIL_COLD void note_shrinking_cold(size_type new_size) noexcept {
            const size_type quarter = static_cast<size_type>(capacity_) >> 2;
            if (new_size <= quarter && size_ > quarter) {
                size_ = new_size;
                XBufferCore::note_spare_head(this, static_cast<std::uint32_t>(sizeof(T)));
            }
        }

        // M4 (threat model T5/P2, XOFFSET_SCRUB_ERASED): storage of destroyed
        // elements is zeroed, so a later save does not emit the old values as
        // spare capacity. Without it the bytes stay until an append
        // constructs over them, with their container heads empty (settle).
        XOFFSET_DETAIL_INLINE void scrub([[maybe_unused]] size_type from,
                                              [[maybe_unused]] size_type to) noexcept {
            if constexpr (detail::scrub_erased) {
                if (to <= from) return;
                if (auto* items = data())
                    std::memset(static_cast<void*>(items + from), 0, std::size_t(to - from) * sizeof(T));
            } else if constexpr (detail::settles_vacated_heads<T>()) {
                if (to <= from) return;
                T* const items = data();
                for (size_type i = from; i < to; ++i) detail::empty_vacated_slot(items + i);
            }
        }

        void copy_from(const XVectorCore& other) {
            if (other.size_ == 0) return;
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::deep_bytes(other) + fixed_detail::array_bytes<T>(other.size_), );
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
            XOFFSET_TRY {
                for (; constructed < other.size_; ++constructed) {
                    fixed_detail::transfer_construct(
                        data() + constructed, other.data()[constructed], alloc);
                }
            } XOFFSET_CATCH(...) {
                fixed_detail::destroy_n(data(), constructed);
                size_ = 0;
                XOFFSET_RETHROW;
            }
            size_ = other.size_;
        }

        // POD: assign_range (in place when the capacity suffices, else one
        // exact block), then the M6 padding zeroing transfer_construct does.
        // Out of line under Clang: with assign_bytes inline in it, Clang
        // inlined it into assign_general and then kept assign_general out of
        // assign_general_out_of_line, whose empty-source path then paid the
        // larger body's register saves (+4.4k instructions on slg.A.apply).
        // GCC inlines both and is faster that way.
        XOFFSET_DETAIL_NOINLINE_CLANG void assign_trivial_copy(const XVectorCore& other) {
            assign_range(std::span<const T>(other.data(), other.size_));
            auto* items = data();
            for (size_type i = 0; i < size_; ++i) detail::scrub_implicit_padding(items + i);
            XOFFSET_DETAIL_SUBTREE_COUNT(pod_hits, 1);
        }

        XOFFSET_DETAIL_NOINLINE void release_live() noexcept {
            release_in(binding());
        }

        XOFFSET_DETAIL_INLINE void release() noexcept {
            if (data_delta != 0) [[unlikely]] release_live();
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
    class XSet : public XVectorCore<T> {
        using Base = XVectorCore<T>;

    public:
        using key_type = T;
        using value_type = typename Base::value_type;
        using allocator_type = typename Base::allocator_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;

        XSet() = default;

        template <typename Alloc>
        explicit XSet(const Alloc& alloc) noexcept(noexcept(fixed_detail::allocator_ref_of(alloc)))
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

        // Sorted bulk fill: replaces the contents with the values of
        // [first, last), which must be strictly ascending (the contract of
        // std::sorted_unique). One exact block and no lookup: each value is
        // built in place, in order; a contiguous range of trivially copyable
        // T is one copy. The order is checked only under
        // XOFFSET_CHECK_SORTED: an input that is not strictly ascending then
        // throws XErrc::invalid_argument and leaves the set unchanged.
        template <std::forward_iterator It, std::sentinel_for<It> S>
            requires requires(T* dst, It it, allocator_type alloc) { fixed_detail::construct_bound(dst, alloc, *it); }
        void assign_sorted(It first, S last) {
            if constexpr (std::contiguous_iterator<It> && std::sized_sentinel_for<S, It> &&
                          std::is_same_v<std::remove_cv_t<std::iter_value_t<It>>, T> &&
                          std::is_trivially_copyable_v<T>) {
                const auto count = static_cast<std::size_t>(last - first);
                assign_sorted(std::span<const T>(count ? std::to_address(first) : nullptr, count));
            } else {
                if constexpr (detail::check_sorted)
                    if (!fixed_detail::strictly_ascending(first, last, [](const auto& v) -> const auto& { return v; }))
                        XOFFSET_THROW(XException(XErrc::invalid_argument, "assign_sorted: input not strictly ascending"));
                const auto count = static_cast<std::size_t>(std::ranges::distance(first, last));
                const auto alloc = Base::element_binding();
                Base::fill_construct(first, last, count, [&](T* dst, auto&& value) {
                    fixed_detail::construct_bound(dst, alloc, std::forward<decltype(value)>(value));
                });
            }
        }

        template <std::ranges::forward_range R>
            requires requires(R& range) { std::declval<XSet&>().assign_sorted(std::ranges::begin(range), std::ranges::end(range)); }
        void assign_sorted(R&& range) {
            assign_sorted(std::ranges::begin(range), std::ranges::end(range));
        }

        void assign_sorted(std::span<const T> values)
            requires (std::is_trivially_copyable_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>) {
            if constexpr (detail::check_sorted)
                if (!fixed_detail::strictly_ascending(values.begin(), values.end(), [](const T& v) -> const T& { return v; }))
                    XOFFSET_THROW(XException(XErrc::invalid_argument, "assign_sorted: input not strictly ascending"));
            Base::assign_range(values);
        }

        XOFFSET_DETAIL_INLINE std::pair<iterator, bool> insert(const T& value) {
            if constexpr (std::is_floating_point_v<T>)
                if (fixed_detail::nan_key<T>(value)) [[unlikely]] {
                    fixed_detail::reject_nan_key();
                    return {Base::end(), false};
                }
            XOFFSET_SOFT_PREFLIGHT_IF(Base::size_ >= Base::capacity_, this, fixed_detail::grow_bytes<T>(Base::capacity_) + fixed_detail::deep_bytes(value),
                                      std::pair<iterator, bool>{Base::end(), false});
            auto* items = Base::data();
            const std::uint32_t size = Base::size();
            auto construct = [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, Base::element_binding());
            };
            if (size == 0 || fixed_detail::ordered_less(items[size - 1], value))
                return {Base::append_after_last_construct(construct), true};
            const auto index = fixed_detail::insert_index_below_last(
                items, size, value, [](const T& item) -> const T& { return item; });
            if (fixed_detail::ordered_equal(items[index], value)) return {items + index, false};
            return {Base::template insert_before_end_construct<true>(index, construct), true};
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        XOFFSET_DETAIL_INLINE std::pair<iterator, bool> insert(Arg&& arg) {
            if constexpr (std::is_floating_point_v<T>)
                if (fixed_detail::nan_key<T>(arg)) [[unlikely]] {
                    fixed_detail::reject_nan_key();
                    return {Base::end(), false};
                }
            XOFFSET_SOFT_PREFLIGHT_IF(Base::size_ >= Base::capacity_, this, fixed_detail::grow_bytes<T>(Base::capacity_) + fixed_detail::deep_bytes(arg),
                                      std::pair<iterator, bool>{Base::end(), false});
            auto* items = Base::data();
            const std::uint32_t size = Base::size();
            auto construct = [&](T* dst) {
                fixed_detail::construct_bound(dst, Base::element_binding(), std::forward<Arg>(arg));
            };
            if (size == 0 || fixed_detail::ordered_less(items[size - 1], arg))
                return {Base::append_after_last_construct(construct), true};
            const auto index = fixed_detail::insert_index_below_last(
                items, size, arg, [](const T& item) -> const T& { return item; });
            if (fixed_detail::ordered_equal(items[index], arg)) return {items + index, false};
            return {Base::template insert_before_end_construct<true>(index, construct), true};
        }

        template <typename... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            if constexpr (sizeof...(Args) == 1) {
                return insert(std::forward<Args>(args)...);
            } else {
                XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(1) + fixed_detail::grow_bytes<T>(Base::capacity_) +
                                             (std::size_t(0) + ... + fixed_detail::deep_bytes(args)),
                                       std::pair<iterator, bool>{Base::end(), false});
                const auto alloc = Base::bound_binding();
                auto* staged = detail::begin_dynamic_array<T>(
                fixed_detail::allocate_dynamic_bytes(alloc, sizeof(T), alignof(T)), 1u);
                bool constructed = false;
                XOFFSET_TRY {
                    fixed_detail::construct_bound(staged, alloc, std::forward<Args>(args)...);
                    constructed = true;
                    auto result = insert(*staged);
                    std::destroy_at(staged);
                    fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                    return result;
                } XOFFSET_CATCH(...) {
                    if (constructed) std::destroy_at(staged);
                    fixed_detail::retire_dynamic_bytes(alloc, staged, sizeof(T));
                    XOFFSET_RETHROW;
                }
            }
        }

        // Clang: inline at the caller (find plus one erase_index), as it
        // chose before the spare watch's hook made erase_index larger.
        template <typename KeyLike>
        XOFFSET_DETAIL_INLINE_CLANG std::uint32_t erase(const KeyLike& key) {
            auto it = find(key);
            if (it == Base::end()) return 0;
            auto index = static_cast<std::uint32_t>(it - Base::begin());
            Base::erase_index(index);
            return 1;
        }

        iterator insert(const_iterator, const T& value) {
            return insert(value).first;
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        iterator insert(const_iterator, Arg&& arg) {
            return insert(std::forward<Arg>(arg)).first;
        }
    };

    template <typename K, typename V>
    class XMap : public XVectorCore<XKeyValue<K, V>> {
        using value_base = XVectorCore<XKeyValue<K, V>>;

    public:
        using key_type = K;
        using mapped_type = V;
        using value_type = typename value_base::value_type;
        using allocator_type = typename value_base::allocator_type;
        using iterator = typename value_base::iterator;
        using const_iterator = typename value_base::const_iterator;

        XMap() = default;

        template <typename Alloc>
        explicit XMap(const Alloc& alloc) noexcept(noexcept(fixed_detail::allocator_ref_of(alloc)))
            : value_base(fixed_detail::allocator_ref_of(alloc)) {}

        XMap(const XMap& other)
            : value_base() {
            value_base::operator=(other);
        }

        XMap(XMap&& other)
            : XMap(static_cast<const XMap&>(other)) {}

        template <typename Alloc>
        XMap(const XMap& other, const Alloc& alloc)
            : value_base(fixed_detail::allocator_ref_of(alloc)) {
            value_base::operator=(other);
        }

        XMap& operator=(const XMap&) = default;
        XMap& operator=(XMap&&) = default;

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
        XOFFSET_DETAIL_INLINE std::pair<iterator, bool> try_emplace(KeyArg&& key, Args&&... args) {
            if constexpr (std::is_floating_point_v<K>)
                if (fixed_detail::nan_key<K>(key)) [[unlikely]] {
                    fixed_detail::reject_nan_key();
                    return {value_base::end(), false};
                }
            XOFFSET_SOFT_PREFLIGHT_IF(value_base::size_ >= value_base::capacity_, this, fixed_detail::grow_bytes<value_type>(value_base::capacity_) +
                                      fixed_detail::deep_bytes(key) +
                                      (std::size_t(0) + ... + fixed_detail::deep_bytes(args)),
                                      std::pair<iterator, bool>{value_base::end(), false});
            auto* items = value_base::data();
            const std::uint32_t size = value_base::size();
            auto construct = [&](value_type* dst) {
                construct_entry(dst, std::forward<KeyArg>(key), std::forward<Args>(args)...);
            };
            if (size == 0 || fixed_detail::ordered_less(items[size - 1].first, key))
                return {value_base::append_after_last_construct(construct), true};
            const auto index = fixed_detail::insert_index_below_last(
                items, size, key, [](const value_type& item) -> const K& { return item.first; });
            if (fixed_detail::ordered_equal(items[index].first, key)) return {items + index, false};
            return {value_base::template insert_before_end_construct<true>(index, construct), true};
        }

        template <typename KeyArg, typename ValueArg>
        std::pair<iterator, bool> emplace(KeyArg&& key, ValueArg&& value) {
            return try_emplace(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
        }

        // Sorted bulk fill: replaces the contents with the entries of
        // [first, last), each a pair-like element (e.first the key, e.second
        // the value: std::pair, XKeyValue, the elements of a std::map), whose
        // keys must be strictly ascending (the contract of std::sorted_unique).
        // One exact block and no lookup: each entry is built in place, in
        // order; a contiguous range of trivially copyable value_type is one
        // copy. The order is checked only under XOFFSET_CHECK_SORTED: an
        // input that is not strictly ascending then throws
        // XErrc::invalid_argument and leaves the map unchanged.
        template <std::forward_iterator It, std::sentinel_for<It> S>
            requires requires(It it) { (*it).first; (*it).second; }
        void assign_sorted(It first, S last) {
            if constexpr (std::contiguous_iterator<It> && std::sized_sentinel_for<S, It> &&
                          std::is_same_v<std::remove_cv_t<std::iter_value_t<It>>, value_type> &&
                          std::is_trivially_copyable_v<value_type>) {
                const auto count = static_cast<std::size_t>(last - first);
                assign_sorted(std::span<const value_type>(count ? std::to_address(first) : nullptr, count));
            } else {
                if constexpr (detail::check_sorted)
                    if (!fixed_detail::strictly_ascending(first, last, [](const auto& e) -> const auto& { return e.first; }))
                        XOFFSET_THROW(XException(XErrc::invalid_argument, "assign_sorted: keys not strictly ascending"));
                const auto count = static_cast<std::size_t>(std::ranges::distance(first, last));
                // Entries without heads never use the binding (no lookup).
                const auto alloc = value_base::element_binding();
                value_base::fill_construct(first, last, count, [&](value_type* dst, auto&& entry) {
                    construct_entry_in(alloc, dst, std::forward<decltype(entry)>(entry).first,
                                       std::forward<decltype(entry)>(entry).second);
                });
            }
        }

        template <std::ranges::forward_range R>
            requires requires(R& range) { std::declval<XMap&>().assign_sorted(std::ranges::begin(range), std::ranges::end(range)); }
        void assign_sorted(R&& range) {
            assign_sorted(std::ranges::begin(range), std::ranges::end(range));
        }

        void assign_sorted(std::span<const value_type> values)
            requires (std::is_trivially_copyable_v<value_type>) {
            if constexpr (detail::check_sorted)
                if (!fixed_detail::strictly_ascending(values.begin(), values.end(),
                                                      [](const value_type& e) -> const K& { return e.first; }))
                    XOFFSET_THROW(XException(XErrc::invalid_argument, "assign_sorted: keys not strictly ascending"));
            value_base::assign_range(values);
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
#if XOFFSET_NO_EXCEPTIONS
            if (it == value_base::end()) [[unlikely]] return fixed_detail::soft_sink<V>();
#endif
            return it->second;
        }

        // Erase a position returned by this map. As with other flat-map
        // mutations, the erased position and later positions are invalidated.
        // Passing end() leaves the map unchanged.
        XOFFSET_DETAIL_INLINE iterator erase_at(const_iterator pos) {
            if (pos == value_base::end()) return value_base::end();
            const auto index = static_cast<std::uint32_t>(pos - value_base::begin());
            value_base::erase_index(index);
            return value_base::data() + index;
        }

        // Clang: inline at the caller, as XSet::erase.
        template <typename KeyLike>
        XOFFSET_DETAIL_INLINE_CLANG std::uint32_t erase(const KeyLike& key) {
            auto it = find(key);
            if (it == value_base::end()) return 0;
            erase_at(it);
            return 1;
        }

    private:
        template <typename KeyArg, typename... Args>
        XOFFSET_DETAIL_INLINE void construct_entry(value_type* dst, KeyArg&& key, Args&&... args) {
            construct_entry_in(value_base::element_binding(), dst, std::forward<KeyArg>(key), std::forward<Args>(args)...);
        }

        template <typename KeyArg, typename... Args>
        XOFFSET_DETAIL_INLINE void construct_entry_in(const allocator_type& alloc, value_type* dst, KeyArg&& key, Args&&... args) {
            fixed_detail::construct_bound(&dst->first, alloc, std::forward<KeyArg>(key));
            if constexpr (std::is_trivially_destructible_v<K>) {
                fixed_detail::construct_bound(&dst->second, alloc, std::forward<Args>(args)...);
            } else {
                XOFFSET_TRY {
                    fixed_detail::construct_bound(&dst->second, alloc, std::forward<Args>(args)...);
                } XOFFSET_CATCH(...) {
                    std::destroy_at(&dst->first);
                    XOFFSET_RETHROW;
                }
            }
            detail::scrub_implicit_padding(dst);  // between key and value (M6)
        }
    };

    static_assert(sizeof(XString) == 12 && alignof(XString) == 4,
        "XString ABI must stay frozen at 12 bytes, 4-aligned (wire 3.0)");
    static_assert(sizeof(XVectorCore<int>) == 12 && alignof(XVectorCore<int>) == 4,
        "XVectorCore ABI must stay frozen at 12 bytes, 4-aligned (wire 3.0)");
    static_assert(sizeof(XSet<int>) == sizeof(XVectorCore<int>),
        "XSet ABI must stay frozen at 12 bytes");
    static_assert(sizeof(XMap<int, int>) == sizeof(XVectorCore<XKeyValue<int, int>>),
        "XMap ABI must stay frozen at 12 bytes");

    template <typename T>
    class XVector : public XVectorCore<T> {
        using Base = XVectorCore<T>;

        std::uint32_t index_from(typename Base::const_iterator pos) const noexcept {
            auto* begin = Base::data();
            const T* at = std::to_address(pos);
            if (!begin || !at) return 0;
            return static_cast<std::uint32_t>(at - begin);
        }

        template <typename Arg>
        typename Base::iterator insert_value(typename Base::const_iterator pos, Arg&& arg) {
            XOFFSET_SOFT_PREFLIGHT_IF(Base::size_ >= Base::capacity_, this, fixed_detail::grow_bytes<T>(Base::capacity_) + fixed_detail::deep_bytes(arg),
                                      Base::end());
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
        using Base::append_range;
        using Base::assign_range;
        using Base::assign_bytes;
        using Base::assign;
        using Base::pop_back;
        using Base::emplace_back;

        XVector() = default;

        template <typename Alloc>
        explicit XVector(const Alloc& alloc) noexcept(noexcept(fixed_detail::allocator_ref_of(alloc)))
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

        XOFFSET_DETAIL_INLINE void resize(std::size_t count) {
            if constexpr (std::is_scalar_v<T>) {
                Base::resize_default_scalar(count);
            } else {
                if (Base::try_resize_trivial_in_place(count)) [[likely]] return;
                resize_grow_or_general(count);
            }
        }

        XOFFSET_DETAIL_NOINLINE void resize_grow_or_general(std::size_t count) requires (!std::is_scalar_v<T>) {
            if (count > Base::capacity_)
                XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(std::max<std::size_t>(count, 2u * Base::capacity_)), );
            auto n = Base::checked_count(count);
            const auto alloc = Base::element_binding();
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::default_construct(dst, alloc);
            });
        }

        void resize(std::size_t count, const T& value) {
            if (count > Base::size_)
                XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(std::max<std::size_t>(count, 2u * Base::capacity_)) +
                                             (count - Base::size_) * fixed_detail::deep_bytes(value), );
            auto n = Base::checked_count(count);
            const auto alloc = Base::element_binding();
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::transfer_construct(dst, value, alloc);
            });
        }

        template <typename Arg>
            requires alloc_aware_convertible<T, Arg>
        void resize(std::size_t count, Arg&& arg) {
            if (count > Base::size_)
                XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(std::max<std::size_t>(count, 2u * Base::capacity_)) +
                                             (count - Base::size_) * fixed_detail::deep_bytes(arg), );
            auto n = Base::checked_count(count);
            const auto alloc = Base::element_binding();
            Base::resize_construct(n, [&](T* dst) {
                fixed_detail::construct_bound(dst, alloc, arg);
            });
        }

        void assign(std::size_t count, const T& value) {
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(count) + count * fixed_detail::deep_bytes(value), );
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
            XOFFSET_SOFT_PREFLIGHT(this, fixed_detail::array_bytes<T>(count) + count * fixed_detail::deep_bytes(arg), );
            auto n = Base::checked_count(count);
            if (n == 0) { Base::clear(); return; }
            const auto alloc = Base::element_binding();
            Base::rebuild(n, std::max(n, Base::capacity()), [&](T* dst, size_type) {
                fixed_detail::construct_bound(dst, alloc, arg);
            });
        }
    };

    static_assert(sizeof(XVector<int>) == sizeof(XVectorCore<int>),
        "XVector ABI must stay frozen at 12 bytes");

    using XBlob = XVector<std::byte>;
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

    template <> struct description<XString> : describe_detail::head_record<XString> {};
    template <class T> struct description<XVectorCore<T>> : describe_detail::head_record<XVectorCore<T>> {};
    template <class T> struct description<XVector<T>>
        : describe_detail::record<XVector<T>, false, describe_detail::bases<XVectorCore<T>>> {};
    template <class T> struct description<XSet<T>>
        : describe_detail::record<XSet<T>, false, describe_detail::bases<XVectorCore<T>>> {};
    template <class K, class V> struct description<XMap<K, V>>
        : describe_detail::record<XMap<K, V>, false,
                                  describe_detail::bases<XVectorCore<XKeyValue<K, V>>>> {};
    template <class K, class V> struct description<XKeyValue<K, V>>
        : describe_detail::record<XKeyValue<K, V>, true, describe_detail::bases<>,
              typename describe_detail::key_value_fields<K, V>::field_first,
              typename describe_detail::key_value_fields<K, V>::field_second> {};

    namespace describe_detail {
        template <std::size_t N>
        struct spare_fields {
            using C = XSpare<N>;
            XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(C, bytes)
        };
    } // namespace describe_detail
    template <std::size_t N> struct description<XSpare<N>>
        : describe_detail::record<XSpare<N>, true, describe_detail::bases<>,
              typename describe_detail::spare_fields<N>::field_bytes> {};

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

#endif // X_OFFSET_CONTAINERS_HPP
