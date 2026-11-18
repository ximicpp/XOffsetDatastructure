#ifndef X_OFFSET_ADMISSION_HPP
#define X_OFFSET_ADMISSION_HPP

// Admission: may a value of T live in an XBuffer and be moved as bytes?
// has_pointer<T>: a pointer, member pointer or reference, or a vtable or
// virtual base, at any depth; without reflection a union or non-aggregate
// class is opaque and counts as one. byte_copy_safe<T>: trivially copyable
// without a pointer, or a pointer-free class or union whose bases and members
// are byte_copy_safe. Specialize is_byte_copy_safe<T> for a type the walk
// cannot see; it answers direct queries only, not the member walk of an
// enclosing record. tests/test_admission_typelayout.cpp cross-checks.

#include "xoffset_schema.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {
    namespace admission_detail {
        template <class T> consteval bool has_pointer();
        template <class T> consteval bool byte_copy_safe();

        template <class T>
        consteval bool bases_have_pointer() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return ((schema::base_is_virtual_v<T, I> || has_pointer<schema::base_t<T, I>>()) || ...);
            }(std::make_index_sequence<schema::base_count_v<T>>{});
        }

        template <class T>
        consteval bool members_have_pointer() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (has_pointer<schema::member_type_t<T, I>>() || ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }

        template <class T>
        consteval bool has_pointer() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_pointer_v<U> || std::is_member_pointer_v<U> || std::is_reference_v<U>)
                return true;
            else if constexpr (std::is_array_v<U>) return has_pointer<std::remove_extent_t<U>>();
            else if constexpr (schema::is_undescribable_v<U>) return true;  // opaque
            else if constexpr (std::is_union_v<U>) return members_have_pointer<U>();
            else if constexpr (std::is_class_v<U>)
                return std::is_polymorphic_v<U> || bases_have_pointer<U>() || members_have_pointer<U>();
            else return false;
        }

        template <class T>
        consteval bool bases_and_members_safe() {
            constexpr std::size_t bases = std::is_union_v<T> ? 0 : schema::base_count_v<T>;
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (byte_copy_safe<schema::base_t<T, I>>() && ...);
            }(std::make_index_sequence<bases>{}) && []<std::size_t... I>(std::index_sequence<I...>) {
                return (byte_copy_safe<schema::member_type_t<T, I>>() && ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }

        template <class T>
        consteval bool byte_copy_safe() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_trivially_copyable_v<U> && !has_pointer<U>()) return true;
            else if constexpr (std::is_class_v<U> || std::is_union_v<U>) {
                if constexpr (has_pointer<U>()) return false;
                else return bases_and_members_safe<U>();
            } else return false;
        }
    } // namespace admission_detail

    template <typename T>
    struct is_byte_copy_safe : std::bool_constant<admission_detail::byte_copy_safe<T>()> {};

    template <typename T, std::size_t N>
    struct is_byte_copy_safe<T[N]> : std::bool_constant<admission_detail::byte_copy_safe<T>()> {};

    template <typename T>
    inline constexpr bool is_byte_copy_safe_v = is_byte_copy_safe<T>::value;
} // namespace XOffsetDatastructure

#endif // X_OFFSET_ADMISSION_HPP
