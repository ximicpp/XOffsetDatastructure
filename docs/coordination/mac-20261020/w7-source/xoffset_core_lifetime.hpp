#pragma once
// Additive source seam. The complete main and strict recursive raw/future domain
// must be available; this file does not publish a receive factory or friend.
#include "xoffsetdatastructure.hpp"
#include "receive_raw.hpp"
#include <cassert>
#include <cstring>
#include <limits>
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

template<class T>
void* future_custom_destination(void* storage) noexcept {
    if (auto* bytes = XBufferCore::canonical_bytes_at_internal(storage,
            reinterpret_cast<std::uintptr_t>(storage), sizeof(T))) return bytes;
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
    if (auto* bytes = XBufferCore::canonical_bytes_at_internal(storage,
            reinterpret_cast<std::uintptr_t>(storage), capacity * sizeof(T))) storage = bytes;
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
    if (auto* bytes = XBufferCore::canonical_bytes_at_internal(storage,
            reinterpret_cast<std::uintptr_t>(storage), capacity * sizeof(T))) storage = bytes;
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
        if (auto* bytes = XBufferCore::canonical_bytes_at_internal(zeroed,
                reinterpret_cast<std::uintptr_t>(zeroed), sizeof(T))) zeroed = bytes;
        // The original caller already zeroed this whole fresh/dead slot.
        // A supported implicit ordinary T is actually created before forming
        // member addresses. Nonimplicit heads still use the original walk.
        return static_cast<T*>(::operator new(sizeof(T), zeroed));
    } else return static_cast<T*>(zeroed);
}

template<class T>
T* begin_relocated_ordinary_outer(T* destination, const T& source) noexcept {
    if constexpr (ordinary_outer_lifetime_supported<T>()) {
        void* storage = destination;
        if (auto* bytes = XBufferCore::canonical_bytes_at_internal(destination,
                reinterpret_cast<std::uintptr_t>(destination), sizeof(T))) storage = bytes;
        // Disjoint live source -> dead slot, only sizeof(T) INLINE bytes.
        // The following original walk rebases heads and empties source owns.
        return static_cast<T*>(std::memcpy(storage, &source, sizeof(T)));
    } else return destination;
}

} // namespace XOffsetDatastructure::detail
