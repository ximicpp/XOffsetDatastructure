#pragma once
// Additive source seam. The complete main and strict recursive raw/future domain
// must be available; this file does not publish a receive factory or friend.
#include "xoffsetdatastructure.hpp"
#include "receive_raw.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <exception>
#include <new>

#if defined(_MSC_VER)
#define XOFFSET_DETAIL_SHIFT_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define XOFFSET_DETAIL_SHIFT_INLINE inline __attribute__((always_inline))
#else
#define XOFFSET_DETAIL_SHIFT_INLINE inline
#endif

namespace XOffsetDatastructure::detail {

// The receive future gate checks the COMPLETE surface, real reflected fields
// and the explicit construction frontier. It never calls this core dispatch
// predicate, so this direction does not form a can_relocate/future-gate cycle.
template<class T>
consteval bool future_custom_lifetime_supported() {
    if constexpr (wire_activation<T>::enabled && wire_future_lifetime<T>::enabled)
        return receive_raw::future_supported<T>();
    else return false;
}

// W9: every destination is used as given (flat address model for ordinary and
// receive storage alike); no canonical receive-array lookup.
template<class T>
void* future_custom_destination(void* storage) noexcept {
    return storage;
}

// Complete-T policies are semantic customization contracts, NOT member-only
// activation. A throwing construction must leave no live destination T or
// owned subobject behind; ordinary native whole-object constructors naturally
// perform that cleanup. Source reads finish before any caller commits prefix
// relocation. The ordinary implicit-object-creation helper is never used here.
template<class T, class Alloc>
void future_default_construct(void* storage, const Alloc& alloc) {
    static_assert(future_custom_lifetime_supported<T>());
    const auto binding = fixed_detail::allocator_ref_of(alloc);
    wire_future_lifetime<T>::default_construct(future_custom_destination<T>(storage), binding);
}

template<class T, class Src, class Alloc>
void future_transfer_construct(void* storage, Src&& source, const Alloc& alloc) {
    static_assert(future_custom_lifetime_supported<T>());
    const auto binding = fixed_detail::allocator_ref_of(alloc);
    wire_future_lifetime<T>::transfer_construct(future_custom_destination<T>(storage),
                                               std::forward<Src>(source), binding);
}

template<class T>
void future_relocate_construct(T* destination, T& source) noexcept {
    static_assert(future_custom_lifetime_supported<T>());
    static_assert(std::is_nothrow_destructible_v<T>);
    // The hook starts the WHOLE destination, transfers/rebases its heads, and
    // leaves the source WHOLE T live with empty ownership. The original caller
    // then destroys that source exactly once. No allocation/user callback is
    // permitted by this commit-stage contract.
    wire_future_lifetime<T>::relocate(
        static_cast<T*>(future_custom_destination<T>(destination)), source);
}

// Private preconditions are established BEFORE the allocator call at every
// entry: checked nonzero element count, multiplication fits the actual checked
// allocation byte count, alignment and entire storage extent. No allocator,
// callback, value initialization, dirty-spare typed read or retirement here.
template<class T>
T* begin_dynamic_array(void* storage, std::size_t capacity) noexcept {
    if (capacity == 0) return nullptr;
    assert(storage && capacity <= (std::numeric_limits<std::size_t>::max)() / sizeof(T));
    assert(reinterpret_cast<std::uintptr_t>(storage) % alignof(T) == 0);
    // W9: flat storage address (no canonical receive-array lookup).
    // An explicit nonallocating allocation-function invocation implicitly
    // creates the ARRAY, even when its element is nonimplicit-lifetime.
    // Unknown-bound array pointer conversion/decay preserves that array
    // provenance instead of assuming void* directly names a live first T.
    auto* first = *static_cast<T(*)[]>(::operator new(capacity * sizeof(T), storage));
    (void)(first + capacity); // require the whole capacity, including spare slots
    return first;
}

template<class T>
T* rebuild_dynamic_array(void* storage, std::size_t capacity,
                         std::uint32_t live_size) noexcept {
    if (capacity == 0) return nullptr;
    assert(storage && live_size <= capacity &&
           capacity <= (std::numeric_limits<std::size_t>::max)() / sizeof(T));
    assert(reinterpret_cast<std::uintptr_t>(storage) % alignof(T) == 0);
    // W9: flat storage address (no canonical receive-array lookup).
    if constexpr (receive_raw::future_supported<T>()) {
        // memmove's temporary character copy preserves the live representation
        // around implicit object creation. This is NOT start_lifetime_as_array:
        // no bit_cast-style determination of dirty spare bool/enum values.
        auto* first = *static_cast<T(*)[]>(std::memmove(storage, storage, capacity * sizeof(T)));
        (void)(first + capacity);
        // Only the old LIVE prefix's inline nonimplicit heads are activated.
        // Out-of-line objects remain live and are not reconstructed/scanned.
        if constexpr (!std::is_scalar_v<T>)
            receive_raw::reactivate_inline_prefix<T>(first, live_size);
        return first;
    } else {
        // Legacy unsupported/incomplete-custom types do not acquire mutable-C
        // contract. The initial receive API must reject them recursively.
        // Preserve their historical path instead of silently forcing fresh.
        return static_cast<T*>(storage);
    }
}

template<class T>
consteval bool ordinary_outer_lifetime_supported() {
    if constexpr (std::is_class_v<T> && !fixed_detail::is_owning_head<T>::value &&
                  std::is_aggregate_v<T> && !wire_activation<T>::enabled &&
                  wire_record_layout<T>::ordinary_members)
        return raw_implicit_lifetime<T>() && receive_raw::future_supported<T>();
    else return false;
}

template<class T>
T* begin_fresh_ordinary_outer(void* zeroed) noexcept {
    if constexpr (ordinary_outer_lifetime_supported<T>()) {
        // W9: flat slot address (no canonical receive-array lookup).
        // The original caller already zeroed this whole fresh/dead slot.
        // A supported implicit ordinary T is actually created before forming
        // member addresses. Nonimplicit heads still use the original walk.
        return static_cast<T*>(::operator new(sizeof(T), zeroed));
    } else return static_cast<T*>(zeroed);
}

template<class T>
T* begin_relocated_ordinary_outer(T* destination, const T& source) noexcept {
    if constexpr (ordinary_outer_lifetime_supported<T>()) {
        void* storage = destination; // W9: flat slot address
        // Disjoint live source -> dead slot, only sizeof(T) INLINE bytes.
        // The following original walk rebases heads and empties source owns.
        return static_cast<T*>(std::memcpy(storage, &source, sizeof(T)));
    } else return destination;
}

// SHIFT: XFixedVector erase/insert move a contiguous run of live elements by
// one slot. The caller memmoves the run, which supplies implicit object
// creation of the ordinary outers and scalars as in rebuild_dynamic_array.
// Only the INLINE heads of the moved run are then restarted, with every
// self-relative delta rebased by the known displacement. Out-of-line payloads
// keep their addresses and are neither scanned nor restarted. Custom activation
// or lifetime policies keep the per-element relocate_construct path.
template<class T> consteval bool shift_rebase_domain();
// Per member, as a function template: a lambda nested in the pack expansion
// crashes Clang 18, and consteval calls from a lambda need P2564 (C++23).
template<class T, std::size_t I>
consteval bool shift_rebase_domain_member() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else return shift_rebase_domain<typename member::type>();
}

template<class T>
consteval bool shift_rebase_domain() {
    if constexpr (wire_activation<T>::enabled || future_custom_lifetime_supported<T>()) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) return raw_descriptor_layout<T>();
    else if constexpr (std::is_array_v<T>) return shift_rebase_domain<std::remove_extent_t<T>>();
    else if constexpr (std::is_scalar_v<T>) return true;
    else if constexpr (std::is_class_v<T>) {
        if constexpr (schema::base_count_v<T> != 0) return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) consteval {
            return (shift_rebase_domain_member<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

template<class T>
consteval bool shift_relocate_supported() {
    if constexpr (std::is_trivially_copyable_v<T> ||
                  !fixed_detail::can_relocate_without_allocation<T>()) return false;
    else if constexpr (!receive_raw::future_supported<T>() || !raw_needs_activation<T>()) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) return shift_rebase_domain<T>();
    else return ordinary_outer_lifetime_supported<T>() && shift_rebase_domain<T>();
}

// Heads live in one bounded arena, so a rebased delta stays in rel32 range;
// an impossible overflow ends the program as set_rel32 in a noexcept relocation does.
// A null delta stays null; otherwise the exact int32 sum, without a branch on
// null (heads mix bound and empty containers).
inline std::int32_t shift_rel32(std::int32_t delta, std::int32_t shift) noexcept {
    const std::int32_t step = delta != 0 ? shift : 0;
    std::int32_t moved;
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_add_overflow(delta, step, &moved)) [[unlikely]] std::terminate();
#else
    const std::int64_t wide = std::int64_t(delta) + step;
    if (wide < (std::numeric_limits<std::int32_t>::min)() ||
        wide > (std::numeric_limits<std::int32_t>::max)()) [[unlikely]] std::terminate();
    moved = static_cast<std::int32_t>(wide);
#endif
    return moved;
}

template<class T, std::size_t... I>
XOFFSET_DETAIL_SHIFT_INLINE void shift_rebase_members(char* address, std::int32_t shift,
                                                    std::index_sequence<I...>) noexcept;

// `shift` = old address - new address, so each delta grows by it. Forced
// inline down to the heads: one moved element is a few loads and stores.
template<class T>
XOFFSET_DETAIL_SHIFT_INLINE void shift_rebase_inline_value(char* address, std::int32_t shift) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        // Descriptor layout {arena_delta, data_delta, size_, capacity_} is
        // checked by raw_descriptor_layout<T>() in shift_rebase_domain.
        std::int32_t arena_delta, data_delta;
        std::uint32_t size, capacity;
        std::memcpy(&arena_delta, address, 4);
        std::memcpy(&data_delta, address + 4, 4);
        std::memcpy(&size, address + 8, 4);
        std::memcpy(&capacity, address + 12, 4);
        auto* object = ::new (static_cast<void*>(address)) T;
        bool inline_bytes = false;
        if constexpr (is_fixed_string_v<T>) inline_bytes = (capacity & XFixedString::inline_flag) != 0;
        object->arena_delta = shift_rel32(arena_delta, shift);
        object->data_delta = shift_rel32(data_delta, inline_bytes ? 0 : shift);
        object->size_ = size;
        object->capacity_ = capacity;
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                shift_rebase_inline_value<E>(address + i * sizeof(E), shift);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        shift_rebase_members<T>(address, shift, std::make_index_sequence<schema::member_count_v<T>>{});
    }
}

template<class T, std::size_t I>
XOFFSET_DETAIL_SHIFT_INLINE void shift_rebase_member(char* address, std::int32_t shift) noexcept {
    using member = schema::member_t<T, I>;
    using M = typename member::type;
    if constexpr (raw_needs_activation<M>())
        shift_rebase_inline_value<M>(address + member::offset, shift);
}

template<class T, std::size_t... I>
XOFFSET_DETAIL_SHIFT_INLINE void shift_rebase_members(char* address, std::int32_t shift,
                                                    std::index_sequence<I...>) noexcept {
    (shift_rebase_member<T, I>(address, shift), ...);
}

template<class T>
void shift_rebase_moved(T* first, std::size_t count, std::int32_t shift) noexcept {
    static_assert(shift_relocate_supported<T>());
    for (std::size_t i = 0; i < count; ++i)
        shift_rebase_inline_value<T>(reinterpret_cast<char*>(first + i), shift);
}

// The live slot a shift left behind still names the moved payloads. Empty its
// inline heads exactly as a relocated-from source is emptied (binding kept),
// so the caller's destroy_at releases nothing and spare bytes match the
// per-element path.
template<class T>
void empty_vacated_slot(T* slot) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        slot->data_delta = 0;
        slot->size_ = 0;
        slot->capacity_ = 0;
    } else if constexpr (std::is_array_v<T>) {
        for (auto& element : *slot) empty_vacated_slot(&element);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_activation<M>())
                empty_vacated_slot(&member::get(*slot));
        });
    }
}


// T4 SUBTREE COPY: copy assignment of a container copies its source subtree
// (its block plus every block reachable from its live elements) into one
// fresh segment: one memcpy plus a fix-up walk when the source tiles one byte
// range, otherwise a compacting preorder walk. The domain is the SHIFT domain
// extended into container elements: plain heads, scalars, arrays and ordinary
// records, every alignment <= 8, no custom activation/lifetime.
template<class T> consteval bool subtree_copy_domain();
// Per member, as a function template: a lambda nested in the pack expansion
// crashes Clang 18, and consteval calls from a lambda need P2564 (C++23).
template<class T, std::size_t I>
consteval bool subtree_copy_domain_member() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else return subtree_copy_domain<typename member::type>();
}

template<class T>
consteval bool subtree_copy_domain() {
    if constexpr (alignof(T) > 8 || wire_activation<T>::enabled ||
                  future_custom_lifetime_supported<T>()) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) {
        if constexpr (!raw_descriptor_layout<T>()) return false;
        else if constexpr (is_fixed_string_v<T>) return true;
        else return subtree_copy_domain<typename T::value_type>();
    } else if constexpr (std::is_array_v<T>) return subtree_copy_domain<std::remove_extent_t<T>>();
    else if constexpr (std::is_scalar_v<T>) return true;
    else if constexpr (std::is_class_v<T>) {
        if constexpr (schema::base_count_v<T> != 0) return false;
        else if constexpr (!std::is_trivially_copyable_v<T> &&
                           !ordinary_outer_lifetime_supported<T>()) return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) consteval {
            return (subtree_copy_domain_member<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

// T is a container element type. A trivially copyable T takes the range copy
// (assign_range plus padding zeroing); others need the subtree domain.
template<class T>
consteval bool subtree_copy_supported() {
    if constexpr (wire_activation<T>::enabled || future_custom_lifetime_supported<T>()) return false;
    else if constexpr (std::is_trivially_copyable_v<T>)
        return !std::is_const_v<T> && !std::is_volatile_v<T> &&
               std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>;
    else return subtree_copy_domain<T>();
}

// Blocks of one subtree. Live blocks of a valid arena are disjoint, so
// hi - lo == sum exactly when they tile [lo, hi) with no hole or foreign block.
struct SubtreeExtent {
    std::uintptr_t lo = (std::numeric_limits<std::uintptr_t>::max)();
    std::uintptr_t hi = 0;
    std::uint64_t sum = 0;    // block bytes
    std::uint64_t exact = 0;  // block bytes an element-wise copy would allocate

    void add(const void* block, std::uint32_t bytes) noexcept {
        const auto at = reinterpret_cast<std::uintptr_t>(block);
        lo = (std::min)(lo, at);
        hi = (std::max)(hi, at + bytes);
        sum += bytes;
    }
    bool contiguous() const noexcept {
        // Every block offset is 8-aligned, so the copy keeps each alignment.
        return sum != 0 && hi - lo == sum && (lo & 7u) == 0 &&
               sum <= XBufferCore::MAX_PAYLOAD;
    }
};

template<class T> void subtree_measure_value(const T& value, SubtreeExtent& extent) noexcept;

template<class E>
void subtree_measure_elements(const E* data, std::uint32_t size, std::uint32_t capacity,
                              SubtreeExtent& extent) noexcept {
    if (!data || capacity == 0) return;
    extent.add(data, fixed_detail::block_bytes_of(static_cast<std::uint32_t>(capacity * sizeof(E))));
    if (size != 0) extent.exact += fixed_detail::block_bytes_of(static_cast<std::uint32_t>(size * sizeof(E)));
    if constexpr (raw_needs_activation<E>())
        for (std::uint32_t i = 0; i < size; ++i) subtree_measure_value<E>(data[i], extent);
}

template<class T>
void subtree_measure_value(const T& value, SubtreeExtent& extent) noexcept {
    if constexpr (is_fixed_string_v<T>) {
        const XFixedString& head = value;
        if ((head.capacity_ & XFixedString::inline_flag) != 0 || head.capacity_ == 0) return;
        const auto* data = fixed_detail::resolve_rel32_const<char>(&head.data_delta, head.data_delta);
        if (!data) return;
        extent.add(data, fixed_detail::block_bytes_of(head.capacity_ + 1u));
        if (head.size_ > XFixedString::inline_max) extent.exact += fixed_detail::block_bytes_of(head.size_ + 1u);
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        using E = typename T::value_type;
        const XFixedVector<E>& head = value;
        subtree_measure_elements<E>(head.data(), head.size_, head.capacity_, extent);
    } else if constexpr (std::is_array_v<T>) {
        for (const auto& element : value) subtree_measure_value(element, extent);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_activation<M>()) subtree_measure_value<M>(member::get(value), extent);
        });
    }
}

// Heads live in one bounded arena, so a rebound delta stays in rel32 range;
// an impossible overflow ends the program as set_rel32 in a noexcept walk does.
inline std::int32_t subtree_rel32(const void* origin, const void* target) noexcept {
    const std::int64_t delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) -
                               static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(origin));
    if (delta < (std::numeric_limits<std::int32_t>::min)() ||
        delta > (std::numeric_limits<std::int32_t>::max)()) [[unlikely]] std::terminate();
    return static_cast<std::int32_t>(delta);
}

template<class T> void subtree_fix_value(char* address, XArena* arena) noexcept;

// The memcpy created the arrays, ordinary outers and scalars of the copy.
// Restart each live head (nonimplicit lifetime) and rebind it to the
// destination arena; data_delta moves with its block and is kept. Live
// elements get the M6 padding zeroing of the element-wise path.
template<class E>
void subtree_fix_elements(char* data, std::uint32_t size, XArena* arena) noexcept {
    if constexpr (raw_needs_activation<E>() || padding_ranges<E>.size() != 0) {
        for (std::uint32_t i = 0; i < size; ++i) {
            char* element = data + std::size_t(i) * sizeof(E);
            if constexpr (raw_needs_activation<E>()) subtree_fix_value<E>(element, arena);
            if constexpr (padding_ranges<E>.size() != 0)
                zero_implicit_padding(std::launder(reinterpret_cast<E*>(element)));
        }
    }
}

template<class T>
void subtree_fix_value(char* address, XArena* arena) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        // Descriptor layout is checked by raw_descriptor_layout<T>().
        std::int32_t data_delta;
        std::uint32_t size, capacity;
        std::memcpy(&data_delta, address + 4, 4);
        std::memcpy(&size, address + 8, 4);
        std::memcpy(&capacity, address + 12, 4);
        auto* object = ::new (static_cast<void*>(address)) T;
        object->arena_delta = subtree_rel32(&object->arena_delta, arena);
        object->data_delta = data_delta;
        object->size_ = size;
        object->capacity_ = capacity;
        if constexpr (!is_fixed_string_v<T>) {
            if (data_delta != 0 && capacity != 0)
                subtree_fix_elements<typename T::value_type>(address + 4 + data_delta, size, arena);
        }
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                subtree_fix_value<E>(address + i * sizeof(E), arena);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_activation<M>())
                subtree_fix_value<M>(address + member::offset, arena);
        });
    }
}

// Releases replaced content. A contiguous old subtree is retired as one range
// (one hole the next same-shape copy reuses, so a reuse loop alternates two
// ranges instead of growing); its objects end as their storage is reused and
// their destructors would only retire the same blocks. Otherwise block by block.
template<class T>
void subtree_release(fixed_detail::allocator_ref alloc, T* old, std::uint32_t size,
                     std::uint32_t capacity) noexcept {
    if (!old || capacity == 0) return;
    SubtreeExtent extent;
    subtree_measure_elements<T>(old, size, capacity, extent);
    if (extent.contiguous()) {
        fixed_detail::retire_dynamic_bytes(alloc, reinterpret_cast<void*>(extent.lo),
                                           static_cast<std::uint32_t>(extent.sum));
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_retires, 1);
    } else {
        fixed_detail::destroy_n(old, size);
        fixed_detail::retire_dynamic_bytes(alloc, old, static_cast<std::uint32_t>(capacity * sizeof(T)));
        XOFFSET_DETAIL_SUBTREE_COUNT(piecewise_retires, 1);
    }
}

// COMPACT: copies the source subtree in preorder into one fresh segment of
// SubtreeExtent::exact bytes, whatever the source layout. Each block holds
// only its live prefix (capacity = size), strings of at most inline_max bytes
// become inline, and empty containers own no block: the element-wise copy's
// shape, packed with no holes. The segment's bytes were zero (fresh tail or a
// hole zeroed on retirement), so block tails need no clearing.
template<class E>
char* subtree_compact_elements(const E* source, std::uint32_t size, XArena* arena,
                               char*& cursor) noexcept;

template<class T>
void subtree_compact_value(char* address, const T& source, XArena* arena, char*& cursor) noexcept {
    if constexpr (is_fixed_string_v<T>) {
        const XFixedString& from = source;
        auto* head = ::new (static_cast<void*>(address)) T;
        head->arena_delta = subtree_rel32(&head->arena_delta, arena);
        const auto view = from.view();
        const auto size = static_cast<std::uint32_t>(view.size());
        if (size == 0) {
            head->data_delta = 0;
            head->size_ = 0;
            head->capacity_ = 0;
        } else if (size <= XFixedString::inline_max) {
            // Inline layout of store_inline: bytes 4..14 characters then
            // zeros, byte 15 = 0x80 | length.
            char bytes[12] = {};
            std::memcpy(bytes, view.data(), size);
            bytes[11] = static_cast<char>(0x80u | size);
            std::memcpy(address + 4, bytes, sizeof(bytes));
        } else {
            char* block = cursor;
            cursor += fixed_detail::block_bytes_of(size + 1u);
            std::memcpy(block, view.data(), size);
            head->data_delta = subtree_rel32(&head->data_delta, block);
            head->size_ = size;
            head->capacity_ = size;
        }
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        using E = typename T::value_type;
        const XFixedVector<E>& from = source;
        const auto size = from.size_;
        auto* head = ::new (static_cast<void*>(address)) T;
        head->arena_delta = subtree_rel32(&head->arena_delta, arena);
        if (size == 0) {
            head->data_delta = 0;
            head->size_ = 0;
            head->capacity_ = 0;
        } else {
            char* block = subtree_compact_elements<E>(from.data(), size, arena, cursor);
            head->data_delta = subtree_rel32(&head->data_delta, block);
            head->size_ = size;
            head->capacity_ = size;
        }
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                subtree_compact_value<E>(address + i * sizeof(E), source[i], arena, cursor);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_activation<M>())
                subtree_compact_value<M>(address + member::offset,
                                         member::get(source), arena, cursor);
        });
    }
}

// The memcpy of the live prefix creates the array, ordinary outers and
// scalars; heads are then restarted and pointed at their compacted children.
template<class E>
char* subtree_compact_elements(const E* source, std::uint32_t size, XArena* arena,
                               char*& cursor) noexcept {
    char* block = cursor;
    const auto live = static_cast<std::uint32_t>(size * sizeof(E));
    cursor += fixed_detail::block_bytes_of(live);
    std::memcpy(block, static_cast<const void*>(source), live);
    if constexpr (raw_needs_activation<E>() || padding_ranges<E>.size() != 0) {
        for (std::uint32_t i = 0; i < size; ++i) {
            char* element = block + std::size_t(i) * sizeof(E);
            if constexpr (raw_needs_activation<E>()) subtree_compact_value<E>(element, source[i], arena, cursor);
            if constexpr (padding_ranges<E>.size() != 0)
                zero_implicit_padding(std::launder(reinterpret_cast<E*>(element)));
        }
    }
    return block;
}

// Returns false, with nothing changed, only when the new segment does not fit
// (XBadAlloc); the caller then copies element-wise, which may still fit in
// smaller holes or fails exactly as it did before this path existed. A source
// whose blocks tile one range with bounded spare bytes is copied whole (one
// memcpy plus a fix-up walk, source capacities kept); any other source is
// compacted. Strong guarantee: past the allocation only an impossible rel32
// overflow can throw, and it retires the fresh segment first. The source is
// fully copied before the destination is published or its old content
// released, so the source may live inside the destination's old content.
template<class T>
bool subtree_copy_assign(XFixedVector<T>& dst, const XFixedVector<T>& src) {
    static_assert(subtree_copy_supported<T>() && !std::is_trivially_copyable_v<T>);
    XArena* arena = dst.arena();
    if (!arena || src.size_ == 0) return false;
    SubtreeExtent extent;
    subtree_measure_elements<T>(src.data(), src.size_, src.capacity_, extent);
    bool whole = extent.contiguous();
    if (!whole) {
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_skipped_layout, 1);
    } else if (extent.sum - extent.exact > (std::max<std::uint64_t>)(64u, extent.sum / 4u)) {
        // Spare bytes a whole copy would carry beyond the compacted shape.
        whole = false;
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_skipped_slack, 1);
    }
    const auto total = whole ? extent.sum : extent.exact;
    if (total > XBufferCore::MAX_PAYLOAD) return false;
    const auto bytes = static_cast<std::uint32_t>(total);
    const auto alloc = dst.binding();
    const auto size = src.size_;
    char* fresh = nullptr;
    try {
        fresh = static_cast<char*>(fixed_detail::allocate_dynamic_bytes(alloc, bytes, 8));
    } catch (const XBadAlloc&) {
        XOFFSET_DETAIL_SUBTREE_COUNT(fallback_alloc, 1);
        return false;
    }
    char* top;
    std::uint32_t capacity;
    if (whole) {
        const auto* first = reinterpret_cast<const char*>(extent.lo);
        std::memcpy(fresh, first, bytes);
        top = fresh + (reinterpret_cast<const char*>(src.data()) - first);
        capacity = src.capacity_;
        subtree_fix_elements<T>(top, size, arena);
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_hits, 1);
    } else {
        char* cursor = fresh;
        top = subtree_compact_elements<T>(src.data(), size, arena, cursor);
        capacity = size;
        if (cursor != fresh + bytes) [[unlikely]] std::terminate();  // measure/compact disagree
        XOFFSET_DETAIL_SUBTREE_COUNT(compact_hits, 1);
    }
    std::int32_t delta = 0;
    try {
        fixed_detail::set_rel32(delta, &dst.data_delta, top);
    } catch (...) {
        fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
        throw;
    }
    T* old = dst.data();
    const auto old_size = dst.size_;
    const auto old_capacity = dst.capacity_;
    dst.data_delta = delta;
    dst.size_ = size;
    dst.capacity_ = capacity;
    subtree_release<T>(alloc, old, old_size, old_capacity);
    XOFFSET_DETAIL_SUBTREE_COUNT(bytes, bytes);
    return true;
}

} // namespace XOffsetDatastructure::detail

#undef XOFFSET_DETAIL_SHIFT_INLINE
