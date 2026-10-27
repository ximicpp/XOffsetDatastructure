#ifndef X_OFFSET_ADMISSION_HPP
#define X_OFFSET_ADMISSION_HPP

// K0 admission: may a value of T live in an XBuffer and be moved as bytes?
//
// XOffset computes it itself through the schema field interface
// (xoffset_schema.hpp), with the rules TypeLayout's is_byte_copy_safe_v used
// before (TypeLayout scanned its layout signature for pointer tokens; this
// walks the same structure):
//
//   has_pointer<T>: a pointer, member pointer or reference; an array of such;
//   a class with a vtable or a virtual base; a class or union with such a
//   base or member, at any depth.
//
//   byte_copy_safe<T>: trivially copyable without a pointer; or a class or
//   union without a pointer whose bases and members are byte_copy_safe.
//   Anything else (an array of non-trivially-copyable elements, a function
//   type) is not.
//
//   Without reflection a union or a non-aggregate class (std::string, a
//   polymorphic class) has no field list and cannot get one: it is opaque and
//   counts as holding a pointer, so it is not admitted (specialize
//   is_byte_copy_safe for direct queries of such a type).
//
// is_byte_copy_safe<T> is the customization point: specialize it for a type
// whose safety the walk cannot see. As with TypeLayout, a specialization
// answers direct queries for that type (make<T>(), allocator<T>(), ...); the
// member walk of an enclosing record does not consult it. XOffset's own
// containers are specialized at the end of xoffsetdatastructure.hpp.
//
// Under C++26 the TypeLayout cross test (tests/test_admission_typelayout.cpp)
// checks that both give the same answer.

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
