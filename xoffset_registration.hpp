#ifndef X_OFFSET_REGISTRATION_HPP
#define X_OFFSET_REGISTRATION_HPP

// Registration macros used at global scope (XOFFSET_REGISTER_SCHEMA_NAME,
// XOFFSET_PIN_SCHEMA, XOFFSET_ORDINARY_RECORDS) and the library's own
// registrations of its containers. Part of xoffsetdatastructure.hpp;
// include that header. Usage: docs/USER_GUIDE.md.

#include "xoffset_compactor.hpp"

namespace XOffsetDatastructure {

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XVectorCore<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XVector<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename T>
struct is_byte_copy_safe<XOffsetDatastructure::XSet<T>>
    : std::bool_constant<is_byte_copy_safe_v<T>> {};

template <typename K, typename V>
struct is_byte_copy_safe<XOffsetDatastructure::XMap<K, V>>
    : std::bool_constant<is_byte_copy_safe_v<K> && is_byte_copy_safe_v<V>> {};

} // namespace XOffsetDatastructure

// ============================================================================
// Registration macros: XCompactor migration strategy registration.
// Must be placed at global namespace scope.
// ============================================================================

// Namespace sentinel -- detects accidental use inside a namespace block.
struct _XOffset_NS_Sentinel {};

// The macro declares the sentinel where it is expanded: at global scope that
// is the one above, inside a namespace a new type, and the check fails.
#define XOFFSET_CHECK_GLOBAL_NAMESPACE_(macro_name)                            \
    struct _XOffset_NS_Sentinel;                                               \
    static_assert(                                                             \
        ::std::is_same_v<::_XOffset_NS_Sentinel, _XOffset_NS_Sentinel>,       \
        macro_name " must be used at global namespace scope, "                 \
        "not inside any namespace { } block. Move the macro call outside all " \
        "namespace declarations; a type declared in a namespace is named "     \
        "qualified there, for example " macro_name "(game::Unit, ...).");

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

// XOFFSET_ORDINARY_RECORDS(A, B, ...): declares that each record type has
// only ordinary non-static data members: no bases, unions, bit-fields or
// [[no_unique_address]], members never overlap. It specializes
// wire_record_layout<T>::ordinary_members and StableEraseOrdinaryMembers<T>,
// which turns on the shift path (XVector insert/erase/growth move records with
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
    XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN(XOFFSET_DETAIL_ORDINARY_RECORD, , __VA_ARGS__)

#define XOFFSET_DETAIL_ORDINARY_RECORD(Unused, Type)                           \
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

// How XCompactor copies the library's own types.
namespace XOffsetDatastructure {
    template<> struct XCompactor::migrate_as<XString> {
        static constexpr MigrateStrategy value = MigrateStrategy::AllocatorAware;
    };
    template<typename T> struct XCompactor::migrate_as<XVectorCore<T>> {
        static constexpr MigrateStrategy value = MigrateStrategy::Container;
    };
    template<typename T> struct XCompactor::migrate_as<XVector<T>> {
        static constexpr MigrateStrategy value = MigrateStrategy::Container;
    };
    template<typename T> struct XCompactor::migrate_as<XSet<T>> {
        static constexpr MigrateStrategy value = MigrateStrategy::Container;
    };
    template<typename K, typename V> struct XCompactor::migrate_as<XMap<K, V>> {
        static constexpr MigrateStrategy value = MigrateStrategy::Container;
    };
}

#endif // X_OFFSET_REGISTRATION_HPP
