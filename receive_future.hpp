#pragma once
// Complete future customization surface for the separately versioned v2.
// Included by receive_raw.hpp AFTER wire_activation and raw forward declarations.
// The forwards also make a direct include safe through the main/raw cycle.
#include "receive_raw_fwd.hpp"
#include "xoffsetdatastructure.hpp"
#include <concepts>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {

template<class... T> struct wire_future_children {};

// This extension is ADDITIONAL to wire_activation<T>. Initial raw verification
// and activation alone never certify future mutation/construction.
template<class T> struct wire_future_lifetime {
    static constexpr bool enabled = false;
};

namespace detail::custom_future_contract {
template<class T> struct is_children : std::false_type {};
template<class... T>
struct is_children<wire_future_children<T...>> : std::true_type {};

template<class T>
consteval bool surface_complete() {
    using Alloc=fixed_detail::allocator_ref;
    if constexpr (!wire_future_lifetime<T>::enabled ||
                  !wire_activation<T>::enabled ||
                  !std::is_nothrow_destructible_v<T>) return false;
    else if constexpr (!requires { typename wire_future_lifetime<T>::children; })
        return false;
    else if constexpr (!is_children<typename wire_future_lifetime<T>::children>::value)
        return false;
    else return requires(void* dead, T* dst, T& src, const T& copy,
                         const Alloc& alloc, const char* read, char* write,
                         WireValidationContext& ctx, RawActivationStats& stats,
                         const CanonicalRawBytes* bytes) {
        { wire_activation<T>::validate_raw(read, ctx, bytes) } -> std::same_as<void>;
        { wire_activation<T>::activate(write, stats, bytes) } noexcept -> std::same_as<void>;
        { wire_future_lifetime<T>::default_construct(dead, alloc) } -> std::same_as<void>;
        { wire_future_lifetime<T>::transfer_construct(dead, copy, alloc) } -> std::same_as<void>;
        { wire_future_lifetime<T>::transfer_construct(dead, std::move(src), alloc) } -> std::same_as<void>;
        { wire_future_lifetime<T>::relocate(dst, src) } noexcept -> std::same_as<void>;
        { wire_future_lifetime<T>::reactivate_inline(dead, bytes) } noexcept -> std::same_as<void>;
    };
}

// The future gate must ALSO apply its existing automatic recursive member /
// built-in container value_type checks to custom types. This list adds a
// declared construction frontier; it must never bypass actual reflected fields.
// A domain-specific ChildGate is supplied by the receive implementation so
// custom<->ordinary/container recursion has a single implementation.
template<class List, template<class> class ChildGate> struct children_pass;
template<class... T, template<class> class ChildGate>
struct children_pass<wire_future_children<T...>, ChildGate>
    : std::bool_constant<(ChildGate<T>::value && ...)> {};
} // namespace detail::custom_future_contract
} // namespace XOffsetDatastructure
