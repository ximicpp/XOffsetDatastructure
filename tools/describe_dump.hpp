// describe_dump: prints the XOFFSET_RECORD lines of every record reachable
// from some root types, using C++26 reflection (GCC 16 or P2996 Clang). It is
// the migration helper for C++20/C++23 builds, which need one line per record.
//
//   std::vector<std::string> lines = xoffset_describe_dump::lines<Root1, Root2>();
//
// Records are visited through members, bases, C arrays, std::array and
// XOffset containers, and listed dependencies first (a record's line comes
// after the lines of the records it contains), which is the order the lines
// must appear in. XOffset's own types are described by the library and are
// skipped. A record that cannot be described becomes a comment line saying why.

#pragma once

#include "xoffsetdatastructure.hpp"

#if !XOFFSET_SCHEMA_REFLECTION
#error "describe_dump needs C++26 reflection (GCC 16 or P2996 Clang, schema backend auto)"
#endif

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace xoffset_describe_dump {
    namespace xo = ::XOffsetDatastructure;

    template <class T> struct is_std_array : std::false_type {};
    template <class E, std::size_t N> struct is_std_array<std::array<E, N>> : std::true_type {};
    template <class T> struct is_key_value : std::false_type {};
    template <class K, class V> struct is_key_value<xo::XKeyValue<K, V>> : std::true_type {};

    // XOffset's own heads, by exact type: a user record derived from one (a
    // struct with an XString base, say) still needs its own line.
    template <class T> struct is_library_head : std::false_type {};
    template <> struct is_library_head<xo::XFixedString> : std::true_type {};
    template <> struct is_library_head<xo::XString> : std::true_type {};
    template <class E> struct is_library_head<xo::XFixedVector<E>> : std::true_type {};
    template <class E> struct is_library_head<xo::XVector<E>> : std::true_type {};
    template <class E> struct is_library_head<xo::XFixedFlatSet<E>> : std::true_type {};
    template <class E> struct is_library_head<xo::XSet<E>> : std::true_type {};
    template <class K, class V> struct is_library_head<xo::XFixedFlatMap<K, V>> : std::true_type {};
    template <class K, class V> struct is_library_head<xo::XMap<K, V>> : std::true_type {};

    consteval std::string qualified_name(std::meta::info type);

    // A class, enum or template name with its enclosing namespaces and classes.
    consteval std::string scoped_name(std::meta::info entity) {
        std::string name(std::meta::identifier_of(entity));
        for (auto scope = std::meta::parent_of(entity); scope != ^^::; scope = std::meta::parent_of(scope)) {
            if (!std::meta::has_identifier(scope) || std::meta::is_function(scope))
                return std::string("<local or unnamed scope>::") + name;
            name = std::string(std::meta::identifier_of(scope)) + "::" + name;
        }
        return name;
    }

    // The name as it must be written at global scope, template arguments
    // included (XOffsetDatastructure::XMap<int, ns::Item>).
    consteval std::string qualified_name(std::meta::info type) {
        type = std::meta::dealias(type);
        if (std::meta::has_template_arguments(type)) {
            std::string name = scoped_name(std::meta::template_of(type)) + "<";
            bool first = true;
            for (auto arg : std::meta::template_arguments_of(type)) {
                if (!first) name += ", ";
                first = false;
                name += std::meta::is_type(arg) ? qualified_name(arg)
                                                : std::string(std::meta::display_string_of(arg));
            }
            return name + ">";
        }
        if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type) &&
            !std::meta::is_enum_type(type))
            return std::string(std::meta::display_string_of(type));
        return scoped_name(type);
    }

    // Why T cannot get an XOFFSET_RECORD line, or "" if it can.
    template <class T>
    consteval std::string obstacle() {
        if (std::is_union_v<T>) return "a union";
        if (!std::is_aggregate_v<T>) return "not an aggregate (user-declared constructor, private member or virtual function)";
        if (std::meta::has_template_arguments(^^T) && qualified_name(^^T).find(',') != std::string::npos)
            return "a template specialization whose name has commas: describe it through an alias";
        for (auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
            if (!std::meta::has_identifier(member)) return "an anonymous member";
            if (std::meta::is_bit_field(member)) return "a bit-field";
            if (std::meta::is_reference_type(std::meta::type_of(member))) return "a reference member";
        }
        // [[no_unique_address]] can overlap members; a field list needs
        // ascending, disjoint offsets.
        std::size_t end = 0;
        for (auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
            const auto offset = static_cast<std::size_t>(std::meta::offset_of(member).bytes);
            if (offset < end) return "overlapping members ([[no_unique_address]])";
            end = offset + std::meta::size_of(std::meta::type_of(member));
        }
        return "";
    }

    struct Dumper {
        std::set<std::string> done;
        std::vector<std::string> lines;

        template <class T>
        void visit() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_array_v<U>) visit<std::remove_extent_t<U>>();
            else if constexpr (std::is_enum_v<U>) enumeration<U>();
            else if constexpr (is_library_head<U>::value && xo::detail::is_fixed_string_v<U>) {}
            else if constexpr (is_library_head<U>::value && xo::detail::is_fixed_flat_map_v<U>) {
                visit<typename U::key_type>();
                visit<typename U::mapped_type>();
            } else if constexpr (is_library_head<U>::value)
                visit<typename U::value_type>();
            else if constexpr (is_std_array<U>::value) visit<typename U::value_type>();
            else if constexpr (is_key_value<U>::value) {
                visit<typename U::first_type>();
                visit<typename U::second_type>();
            } else if constexpr (std::is_class_v<U> || std::is_union_v<U>) record<U>();
        }

        template <class E>
        void enumeration() {
            if constexpr (!xo::schema::describe_detail::fixed_underlying_type<E>()) {
                constexpr const char* name = std::define_static_string(qualified_name(^^E));
                if (done.insert(name).second)
                    lines.push_back(std::string("// ") + name +
                                    ": give the enum a fixed underlying type (C++20 builds need one)");
            }
        }

        template <class T>
        void record() {
            constexpr const char* name = std::define_static_string(qualified_name(^^T));
            if (!done.insert(name).second) return;
            constexpr const char* why = std::define_static_string(obstacle<T>());
            // if constexpr: the member walk below must not be instantiated for
            // a type it cannot read (std::string's anonymous members, say).
            if constexpr (why[0] != '\0')
                lines.push_back(std::string("// ") + name + " cannot be described: " + why);
            else
                describable<T>(name);
        }

        template <class T>
        void describable(const char* name) {
            static constexpr auto bases = std::define_static_array(
                std::meta::bases_of(^^T, std::meta::access_context::unchecked()));
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
            std::string base_list;
            template for (constexpr auto base : bases) {
                visit<typename [:std::meta::type_of(base):]>();
                base_list += (base_list.empty() ? "" : ", ") + qualified_name_of<typename [:std::meta::type_of(base):]>();
            }
            std::string member_list;
            template for (constexpr auto member : members) {
                visit<typename [:std::meta::type_of(member):]>();
                member_list += std::string(", ") + std::string(std::meta::identifier_of(member));
            }
            constexpr bool ordinary = bases.size() == 0 && xo::schema::implicit_lifetime_aggregate<T>() &&
                                      xo::schema::only_implicit_special_members<T>();
            if (ordinary) lines.push_back(std::string("XOFFSET_RECORD(") + name + member_list + ")");
            else lines.push_back(std::string("XOFFSET_RECORD_DERIVED(") + name + ", (" + base_list + ")" +
                                 member_list + ")");
        }

        template <class T>
        static std::string qualified_name_of() {
            static constexpr const char* name = std::define_static_string(qualified_name(^^T));
            return name;
        }
    };

    template <class... Roots>
    std::vector<std::string> lines() {
        Dumper dumper;
        (dumper.visit<Roots>(), ...);
        return dumper.lines;
    }
} // namespace xoffset_describe_dump
