#ifndef X_OFFSET_SCHEMA_REFLECT_HPP
#define X_OFFSET_SCHEMA_REFLECT_HPP

// Reflection backend of the schema field interface (see xoffset_schema.hpp):
// C++26 P2996 reflection answers every query for every class, with no
// description written by the user. Members and bases are seen regardless of
// access, as the library has always seen them.

#if __has_include(<meta>)
#include <meta>
#else
#include <experimental/meta>
#endif
#include <cstddef>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure::schema {
    namespace reflect_detail {
        consteval auto members(std::meta::info type) {
            return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked());
        }
        consteval auto bases(std::meta::info type) {
            return std::meta::bases_of(type, std::meta::access_context::unchecked());
        }
    } // namespace reflect_detail

    template <class T>
    inline constexpr std::size_t base_count_v = reflect_detail::bases(^^T).size();

    template <class T>
    inline constexpr std::size_t member_count_v = reflect_detail::members(^^T).size();

    template <class T, std::size_t I>
    struct member_t {
        static constexpr std::meta::info info = reflect_detail::members(^^T)[I];
        using type = typename [:std::meta::type_of(info):];
        static constexpr bool is_bit_field = std::meta::is_bit_field(info);
        static constexpr std::size_t offset = std::meta::offset_of(info).bytes;

        template <class Obj>
        XOFFSET_SCHEMA_INLINE static constexpr decltype(auto) get(Obj&& obj) noexcept {
            return (static_cast<Obj&&>(obj).[:info:]);
        }
    };

    template <class T, std::size_t I>
    using member_type_t = typename member_t<T, I>::type;

    namespace reflect_detail {
        template <class T, std::size_t I>
        struct base {
            using type = typename [:std::meta::type_of(bases(^^T)[I]):];
        };
    } // namespace reflect_detail

    template <class T, std::size_t I>
    using base_t = typename reflect_detail::base<T, I>::type;

    template <class T, std::size_t I>
    inline constexpr bool base_is_virtual_v = std::meta::is_virtual(reflect_detail::bases(^^T)[I]);

    template <class T>
    inline constexpr bool is_described_v = std::is_class_v<T> || std::is_union_v<T>;

    template <class T>
    inline constexpr bool is_undescribable_v = false;

    template <class E>
    consteval auto enum_range() {
        static_assert(std::is_enum_v<E>);
        using U = std::underlying_type_t<E>;
        U minimum = 0, maximum = 0;
        for (const auto enumerator : std::meta::enumerators_of(^^E)) {
            const U value = static_cast<U>(std::meta::extract<E>(enumerator));
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
        return enum_bounds<U>{minimum, maximum};
    }

    template <class T>
    consteval bool implicit_lifetime_aggregate() {
        if constexpr (!std::is_aggregate_v<T>) return false;
        else {
            for (const auto member : std::meta::members_of(^^T, std::meta::access_context::unchecked()))
                if (std::meta::is_destructor(member))
                    if (std::meta::is_user_provided(member)) return false;
            return true;
        }
    }

    template <class T>
    consteval bool only_implicit_special_members() {
        for (const auto member : std::meta::members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::is_special_member_function(member) && std::meta::is_user_declared(member))
                return false;
        return true;
    }

    // XOFFSET_RECORD under reflection: the field list must say what reflection
    // sees, and an XOFFSET_RECORD must keep its promise.
    namespace reflect_detail {
        template <class T>
        consteval bool description_bases_match() {
            using D = description<T>;
            if constexpr (D::base_count != base_count_v<T>) return false;
            else return []<std::size_t... I>(std::index_sequence<I...>) {
                return (std::is_same_v<typename D::template base<I>, base_t<T, I>> && ...);
            }(std::make_index_sequence<D::base_count>{});
        }

        template <class T>
        consteval bool description_members_match() {
            using D = description<T>;
            if constexpr (D::member_count != member_count_v<T>) return false;
            else return []<std::size_t... I>(std::index_sequence<I...>) {
                return ((!member_t<T, I>::is_bit_field &&
                         std::is_same_v<typename D::template member<I>::type, member_type_t<T, I>> &&
                         D::template member<I>::offset == member_t<T, I>::offset) && ...);
            }(std::make_index_sequence<D::member_count>{});
        }

        template <class T>
        consteval bool description_promise_holds() {
            if constexpr (!description<T>::ordinary) return true;
            else return base_count_v<T> == 0 && implicit_lifetime_aggregate<T>() &&
                        only_implicit_special_members<T>();
        }
    } // namespace reflect_detail
} // namespace XOffsetDatastructure::schema

#define XOFFSET_SCHEMA_DETAIL_REFLECTION_CHECK(macro, T)                       \
    static_assert(::XOffsetDatastructure::schema::reflect_detail::description_bases_match<T>(), \
        macro "(" #T ", ...): the base classes differ from what reflection sees"); \
    static_assert(::XOffsetDatastructure::schema::reflect_detail::description_members_match<T>(), \
        macro "(" #T ", ...): the members differ from what reflection sees "  \
        "(names, order, offsets or types)");                                   \
    static_assert(::XOffsetDatastructure::schema::reflect_detail::description_promise_holds<T>(), \
        macro "(" #T ", ...): " #T " declares a destructor, copy/move "        \
        "constructor or assignment, so it is not an ordinary record; describe " \
        "it with XOFFSET_RECORD_DERIVED(" #T ", (), ...)");

#endif // X_OFFSET_SCHEMA_REFLECT_HPP
