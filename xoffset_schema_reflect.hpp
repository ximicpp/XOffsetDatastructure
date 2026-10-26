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

    template <class T>
    inline constexpr bool is_described_v = std::is_class_v<T>;

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
} // namespace XOffsetDatastructure::schema

#endif // X_OFFSET_SCHEMA_REFLECT_HPP
