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
template<class T>
consteval bool shift_rebase_domain() {
    if constexpr (wire_activation<T>::enabled || future_custom_lifetime_supported<T>()) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) return raw_descriptor_layout<T>();
    else if constexpr (std::is_array_v<T>) return shift_rebase_domain<std::remove_extent_t<T>>();
    else if constexpr (std::is_scalar_v<T>) return true;
    else if constexpr (std::is_class_v<T>) {
        if constexpr (reflected_base_count<T>() != 0) return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) {
            return ([] {
                constexpr auto member = std::meta::nonstatic_data_members_of(
                    ^^T, std::meta::access_context::unchecked())[I];
                if constexpr (std::meta::is_bit_field(member)) return false;
                else {
                    using M = [:std::meta::type_of(member):];
                    return shift_rebase_domain<M>();
                }
            }() && ...);
        }(std::make_index_sequence<reflected_member_count<T>()>{});
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
inline std::int32_t shift_rel32(std::int32_t delta, std::int32_t shift) noexcept {
    if (delta == 0) return 0;
    const std::int64_t moved = std::int64_t(delta) + shift;
    if (moved < (std::numeric_limits<std::int32_t>::min)() ||
        moved > (std::numeric_limits<std::int32_t>::max)()) [[unlikely]] std::terminate();
    return static_cast<std::int32_t>(moved);
}

// `shift` = old address - new address, so each delta grows by it.
template<class T>
void shift_rebase_inline_value(char* address, std::int32_t shift) noexcept {
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
        object->data_delta = inline_bytes ? data_delta : shift_rel32(data_delta, shift);
        object->size_ = size;
        object->capacity_ = capacity;
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                shift_rebase_inline_value<E>(address + i * sizeof(E), shift);
    } else if constexpr (std::is_class_v<T> && raw_needs_activation<T>()) {
        static_for<reflected_member_count<T>()>([&](auto index) {
            constexpr auto member = std::meta::nonstatic_data_members_of(
                ^^T, std::meta::access_context::unchecked())[decltype(index)::value];
            using M = [:std::meta::type_of(member):];
            if constexpr (raw_needs_activation<M>())
                shift_rebase_inline_value<M>(address + std::meta::offset_of(member).bytes, shift);
        });
    }
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
        static_for<reflected_member_count<T>()>([&](auto index) {
            constexpr auto member = std::meta::nonstatic_data_members_of(
                ^^T, std::meta::access_context::unchecked())[decltype(index)::value];
            using M = [:std::meta::type_of(member):];
            if constexpr (raw_needs_activation<M>())
                empty_vacated_slot(&(slot->[:member:]));
        });
    }
}

} // namespace XOffsetDatastructure::detail
