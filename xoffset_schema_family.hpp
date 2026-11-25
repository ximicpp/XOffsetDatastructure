#ifndef X_OFFSET_SCHEMA_FAMILY_HPP
#define X_OFFSET_SCHEMA_FAMILY_HPP

// Add-only type evolution (docs/SCHEMA_EVOLUTION.md section 9). A later
// version of a type only adds fields, and only in bytes the previous version
// reserved with XSpare<N>. Such versions have the same image bytes, so they
// form one family under the fingerprint of the first version, and a program
// of any version reads and writes the images of any other with no run-time
// step. XOFFSET_SCHEMA_FAMILY(New, Prev) declares New the next version of
// Prev and checks, at compile time, that New is one:
//   - the same sizeof;
//   - every field of Prev that is not spare is in New under the same name, at
//     the same offset, with a related type: scalars the same type, enums the
//     same underlying type, arrays the same extent and related elements,
//     containers the same kind with elements of the same size that are
//     related (an element or mapped type XSpare<N> may become a plain type of
//     N bytes), set and map keys the same type, nested records related by
//     these rules, recursively;
//   - every other field of New is plain (no container anywhere inside) and
//     lies wholly inside spare bytes of Prev;
//   - every spare byte of Prev is a field or spare in New, never padding.
// The relation asks the schema field interface only, so it is the same under
// the reflection backend and the XOFFSET_RECORD field lists (clients too).

#include "xoffset_schema_hash.hpp"
#include "xoffset_registration.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace XOffsetDatastructure {
    namespace detail::family {
        // What breaks the relation; one diagnostic each.
        enum class Error : std::uint8_t {
            none,
            not_a_record,          // New or Prev is not a record
            size_changed,          // a record or a container element has another size
            field_removed,         // a field of Prev is missing, renamed or turned into spare bytes
            field_moved,           // a field of Prev is at another offset
            type_changed,          // a field of Prev has an unrelated type
            outside_spare,         // a new field is not inside Prev's spare bytes
            container_from_spare,  // a new field taken from spare bytes holds a container
            spare_to_padding,      // a spare byte of Prev is padding in New
        };

        inline constexpr std::string_view error_text[] = {
            "",
            "New and Prev must be records",
            "a record or a container element changed its size; a later version keeps every "
            "size and takes new fields from XSpare bytes",
            "a field of the previous version is missing (removed, renamed or turned into spare "
            "bytes); a later version only adds fields",
            "a field of the previous version moved to another offset; fields keep their place",
            "a field of the previous version changed its type; fields keep their type (an enum "
            "its underlying type, a container its kind and element size)",
            "a new field is not inside the previous version's XSpare bytes (it takes padding "
            "or overlaps another field)",
            "a new field taken from XSpare bytes holds a container; spare bytes only become "
            "plain fields (scalars, enums, and arrays and records of them)",
            "spare bytes of the previous version became padding; keep them as fields or XSpare",
        };

        enum class Kind : std::uint8_t { scalar, enumeration, array, string, vector, set, map, spare, record, other };

        template <typename T>
        consteval Kind kind_of() {
            using U = std::remove_cv_t<T>;
            if constexpr (is_spare_v<U>) return Kind::spare;
            else if constexpr (fixed_size_scalar<U>) return Kind::scalar;
            else if constexpr (std::is_enum_v<U>) return Kind::enumeration;
            else if constexpr (std::is_array_v<U>) return Kind::array;
            else if constexpr (is_fixed_string_v<U>) return Kind::string;
            else if constexpr (is_fixed_flat_map_v<U>) return Kind::map;
            else if constexpr (is_fixed_flat_set_v<U>) return Kind::set;
            else if constexpr (is_fixed_vector_v<U>) return Kind::vector;
            else if constexpr (std::is_class_v<U> && schema::is_described_v<U> && !schema::is_undescribable_v<U>)
                return Kind::record;
            else return Kind::other;
        }

        inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

        template <typename T, std::size_t I>
        using field_t = std::remove_cv_t<schema::member_type_t<T, I>>;

        template <typename T>
        consteval std::size_t find_field(std::string_view name) {
            std::size_t found = npos;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((found == npos && schema::member_t<T, Is>::name == name ? (void)(found = Is) : (void)0), ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
            return found;
        }

        // No container anywhere inside.
        template <typename T>
        consteval bool plain() {
            using U = std::remove_cv_t<T>;
            constexpr Kind k = kind_of<U>();
            if constexpr (k == Kind::scalar || k == Kind::enumeration || k == Kind::spare) return true;
            else if constexpr (k == Kind::array) return plain<std::remove_extent_t<U>>();
            else if constexpr (k == Kind::record) {
                return []<std::size_t... Is>(std::index_sequence<Is...>) {
                    return (plain<field_t<U, Is>>() && ...);
                }(std::make_index_sequence<schema::member_count_v<U>>{});
            } else return false;
        }

        // The first error found and where (a path of field names). This and
        // the two text helpers below are templates, so a program that
        // declares no family instantiates none of their string code: the
        // header changes no generated code of such a program.
        template <typename S = std::string>
        struct WalkT {
            Error error = Error::none;
            S where;
            constexpr bool fail(Error e, const S& at) {
                if (error == Error::none) {
                    error = e;
                    where = at;
                }
                return false;
            }
        };

        using Walk = WalkT<>;

        template <typename S = std::string>
        constexpr S join(const S& at, std::string_view name) {
            S out = at;
            if (!out.empty()) out += '.';
            out += name;
            return out;
        }

        template <typename S = std::string>
        constexpr S offset_text(std::size_t offset) {
            char digits[20];
            std::size_t n = 0;
            do {
                digits[n++] = static_cast<char>('0' + offset % 10);
                offset /= 10;
            } while (offset != 0);
            S out = "byte ";
            while (n != 0) out += digits[--n];
            return out;
        }

        template <typename P, typename N>
        constexpr bool related(Walk& w, const std::string& at);

        // Keys order a set or a map: the same type on both sides.
        template <typename P, typename N>
        constexpr bool key_related(Walk& w, const std::string& at) {
            return std::is_same_v<std::remove_cv_t<P>, std::remove_cv_t<N>> || w.fail(Error::type_changed, at);
        }

        // Elements and mapped types: the same size; a spare element may
        // become any plain type.
        template <typename P, typename N>
        constexpr bool element_related(Walk& w, const std::string& at) {
            if constexpr (sizeof(P) != sizeof(N)) return w.fail(Error::size_changed, at);
            else if constexpr (is_spare_v<P>) return plain<N>() || w.fail(Error::container_from_spare, at);
            else return related<P, N>(w, at);
        }

        // Field J of P: in N under its name, not spare, at its offset, related.
        template <typename P, typename N, std::size_t J>
        constexpr bool kept(Walk& w, const std::string& at) {
            if constexpr (kind_of<field_t<P, J>>() == Kind::spare) {
                return true;
            } else {
                constexpr std::string_view name = schema::member_t<P, J>::name;
                const std::string here = join(at, name);
                constexpr std::size_t I = find_field<N>(name);
                if constexpr (I == npos) {
                    return w.fail(Error::field_removed, here);
                } else if constexpr (kind_of<field_t<N, I>>() == Kind::spare) {
                    return w.fail(Error::field_removed, here);
                } else if constexpr (schema::member_t<P, J>::offset != schema::member_t<N, I>::offset) {
                    return w.fail(Error::field_moved, here);
                } else {
                    return related<field_t<P, J>, field_t<N, I>>(w, here);
                }
            }
        }

        // Field I of N that Prev does not have (or a spare member of N):
        // plain, inside spare bytes of P no other new field took.
        template <typename P, typename N, std::size_t I>
        constexpr bool added(Walk& w, const std::string& at, std::vector<char>& spare, std::vector<char>& taken) {
            using F = field_t<N, I>;
            constexpr std::string_view name = schema::member_t<N, I>::name;
            constexpr std::size_t J = find_field<P>(name);
            if constexpr (J != npos && kind_of<F>() != Kind::spare) {
                if constexpr (kind_of<field_t<P, J>>() != Kind::spare) return true;   // kept: checked by kept()
            }
            const std::string here = join(at, name);
            constexpr std::size_t off = schema::member_t<N, I>::offset;
            for (std::size_t b = off; b < off + sizeof(F); ++b) {
                if (b >= spare.size() || !spare[b] || taken[b]) return w.fail(Error::outside_spare, here);
                taken[b] = 1;
            }
            return plain<F>() || w.fail(Error::container_from_spare, here);
        }

        template <typename P, typename N>
        constexpr bool record_related(Walk& w, const std::string& at) {
            if constexpr (sizeof(P) != sizeof(N)) {
                return w.fail(Error::size_changed, at);
            } else {
                std::vector<char> spare(sizeof(P), 0), taken(sizeof(P), 0);
                [&]<std::size_t... Js>(std::index_sequence<Js...>) {
                    auto mark = [&]<std::size_t J>() {
                        if constexpr (kind_of<field_t<P, J>>() == Kind::spare)
                            for (std::size_t b = 0; b < sizeof(field_t<P, J>); ++b)
                                spare[schema::member_t<P, J>::offset + b] = 1;
                    };
                    (mark.template operator()<Js>(), ...);
                }(std::make_index_sequence<schema::member_count_v<P>>{});
                const bool old_fields = [&]<std::size_t... Js>(std::index_sequence<Js...>) {
                    return (kept<P, N, Js>(w, at) && ...);
                }(std::make_index_sequence<schema::member_count_v<P>>{});
                if (!old_fields) return false;
                const bool new_fields = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    return (added<P, N, Is>(w, at, spare, taken) && ...);
                }(std::make_index_sequence<schema::member_count_v<N>>{});
                if (!new_fields) return false;
                for (std::size_t b = 0; b < spare.size(); ++b)
                    if (spare[b] && !taken[b]) return w.fail(Error::spare_to_padding, join(at, offset_text(b)));
                return true;
            }
        }

        template <typename P, typename N>
        constexpr bool related(Walk& w, const std::string& at) {
            using X = std::remove_cv_t<P>;
            using Y = std::remove_cv_t<N>;
            constexpr Kind kp = kind_of<X>(), kn = kind_of<Y>();
            if constexpr (kp != kn) {
                return w.fail(Error::type_changed, at);
            } else if constexpr (kp == Kind::scalar) {
                return std::is_same_v<X, Y> || w.fail(Error::type_changed, at);
            } else if constexpr (kp == Kind::enumeration) {
                return std::is_same_v<std::underlying_type_t<X>, std::underlying_type_t<Y>> ||
                       w.fail(Error::type_changed, at);
            } else if constexpr (kp == Kind::array) {
                if constexpr (std::extent_v<X> != std::extent_v<Y>) return w.fail(Error::type_changed, at);
                else return related<std::remove_extent_t<X>, std::remove_extent_t<Y>>(w, at + "[]");
            } else if constexpr (kp == Kind::string) {
                return true;
            } else if constexpr (kp == Kind::vector) {
                return element_related<typename X::value_type, typename Y::value_type>(w, at + "[]");
            } else if constexpr (kp == Kind::set) {
                return key_related<typename X::value_type, typename Y::value_type>(w, at + "[]");
            } else if constexpr (kp == Kind::map) {
                if (!key_related<typename X::key_type, typename Y::key_type>(w, at + "{key}")) return false;
                if constexpr (sizeof(typename X::value_type) != sizeof(typename Y::value_type))
                    return w.fail(Error::size_changed, at + "{value}");
                else
                    return element_related<typename X::mapped_type, typename Y::mapped_type>(w, at + "{value}");
            } else if constexpr (kp == Kind::spare) {
                return sizeof(X) == sizeof(Y) || w.fail(Error::size_changed, at);
            } else if constexpr (kp == Kind::record) {
                return record_related<X, Y>(w, at);
            } else {
                return w.fail(Error::type_changed, at);
            }
        }

        template <typename P, typename N>
        constexpr Walk walk() {
            Walk w;
            if constexpr (kind_of<P>() != Kind::record || kind_of<N>() != Kind::record) {
                w.fail(Error::not_a_record, std::string());
            } else {
                (void)record_related<std::remove_cv_t<P>, std::remove_cv_t<N>>(w, std::string());
            }
            return w;
        }

        template <typename P, typename N>
        inline constexpr Error error_v = walk<P, N>().error;

        // "XOFFSET_SCHEMA_FAMILY: <what> (at <field path>)", for a static_assert
        // that takes a computed message (C++26).
        template <std::size_t Size>
        struct Message {
            char text[Size]{};
            std::size_t length = 0;
            constexpr void append(std::string_view part) noexcept {
                for (char c : part) text[length++] = c;
            }
            constexpr const char* data() const noexcept { return text; }
            constexpr std::size_t size() const noexcept { return length; }
        };

        inline constexpr std::string_view message_head = "XOFFSET_SCHEMA_FAMILY: ";

        template <typename P, typename N>
        consteval auto message() {
            constexpr std::size_t where = walk<P, N>().where.size();
            constexpr std::string_view what = error_text[static_cast<std::size_t>(error_v<P, N>)];
            Message<message_head.size() + what.size() + where + 8> m;
            m.append(message_head);
            m.append(what);
            if constexpr (where != 0) {
                m.append(" (at ");
                const Walk w = walk<P, N>();
                m.append(w.where);
                m.append(")");
            }
            return m;
        }
    } // namespace detail::family

    // New is an add-only successor of Prev: the same image bytes mean the same
    // under both (the rules at the top of this header).
    template <typename New, typename Prev>
    inline constexpr bool schema_family_successor_v =
        detail::family::error_v<Prev, New> == detail::family::Error::none;
} // namespace XOffsetDatastructure

// XOFFSET_SCHEMA_FAMILY(New, Prev): New is the next version of Prev, which it
// only adds fields to (from Prev's XSpare bytes). New's fingerprint becomes the
// first version's, so every version of the family exchanges images, deltas
// and shared deltas with every other one. One line at global scope, after the
// records' XOFFSET_RECORD lines and schema name registrations, before New is
// first used. Declare it on every type whose fingerprint travels (root types)
// and on every record type of a shared-delta table; nested records need none.
// The whole previous type tree must stay in the code (by hand or as a
// snapshot of tools/schema_freeze).
#define XOFFSET_SCHEMA_FAMILY(New, Prev)                                       \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_SCHEMA_FAMILY")                   \
    static_assert(!::std::is_same_v<New, Prev>,                                \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a type is not its own next version"); \
    template <> struct XOffsetDatastructure::xschema_family<New> {            \
        using previous = Prev;                                                 \
    };                                                                         \
    XOFFSET_SCHEMA_FAMILY_DETAIL_CHECK(New, Prev)

#define XOFFSET_SCHEMA_FAMILY_DETAIL_ERROR(New, Prev) \
    ::XOffsetDatastructure::detail::family::error_v<Prev, New>
#define XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, e) \
    (XOFFSET_SCHEMA_FAMILY_DETAIL_ERROR(New, Prev) != ::XOffsetDatastructure::detail::family::Error::e)

// The diagnostic names the field where static_assert takes a computed message
// (C++26); otherwise one static_assert per rule names the rule. Clang offers
// computed messages in C++20 too, but warns (-Wc++26-extensions) in every
// translation unit that declares a family, so they are used only from C++26.
#if defined(__cpp_static_assert) && __cpp_static_assert >= 202306L && __cplusplus > 202302L
#define XOFFSET_SCHEMA_FAMILY_DETAIL_CHECK(New, Prev)                          \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_ERROR(New, Prev) ==             \
                      ::XOffsetDatastructure::detail::family::Error::none,     \
        ::XOffsetDatastructure::detail::family::message<Prev, New>());
#else
#define XOFFSET_SCHEMA_FAMILY_DETAIL_CHECK(New, Prev)                          \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, not_a_record),    \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): New and Prev must be records"); \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, size_changed),    \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a record or a container element changed " \
        "its size; a later version keeps every size and takes new fields from XSpare bytes"); \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, field_removed),   \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a field of the previous version is missing " \
        "(removed, renamed or turned into spare bytes); a later version only adds fields"); \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, field_moved),     \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a field of the previous version moved to " \
        "another offset; fields keep their place");                            \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, type_changed),    \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a field of the previous version changed its " \
        "type; fields keep their type (an enum its underlying type, a container its kind and " \
        "element size)");                                                      \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, outside_spare),   \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a new field is not inside the previous " \
        "version's XSpare bytes (it takes padding or overlaps another field)"); \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, container_from_spare), \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): a new field taken from XSpare bytes holds a " \
        "container; spare bytes only become plain fields (scalars, enums, and arrays and records " \
        "of them)");                                                           \
    static_assert(XOFFSET_SCHEMA_FAMILY_DETAIL_IS(New, Prev, spare_to_padding), \
        "XOFFSET_SCHEMA_FAMILY(" #New ", " #Prev "): spare bytes of the previous version became " \
        "padding; keep them as fields or XSpare");
#endif

#endif // X_OFFSET_SCHEMA_FAMILY_HPP
