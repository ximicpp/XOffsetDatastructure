#ifndef X_OFFSET_SCHEMA_NAMES_HPP
#define X_OFFSET_SCHEMA_NAMES_HPP

// Field-name fingerprint of a wire type (schema evolution, handshake).
//
// schema_hash covers offsets, sizes and scalar kinds but no member names, so
// two versions of a record that only swap or rename same-typed fields share
// it. The name signature lists, for every record reachable from T in the order
// wire_abi_signature() walks it, each member as "name:offset"; the name digest
// is fnv1a of that text. It travels in the schema handshake only, never in a
// wire header, so the wire format and schema_hash are unchanged.
//
//   record          {name:offset<field>,name:offset<field>}
//   array           [element]
//   vector, set     <element>
//   map             <key,value>
//   scalar, enum, string: empty
//
// The text is part of the handshake compatibility promise: two programs agree
// on a version only if they compute the same digest for it.

#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace XOffsetDatastructure {
    namespace detail::schema_names {
        template <typename T>
        consteval void append_signature(std::string& out);

        template <typename U, std::size_t... Is>
        consteval void append_record(std::string& out, std::index_sequence<Is...>) {
            using namespace std::meta;
            out += '{';
            auto field = [&]<std::size_t I>() {
                constexpr auto member = nonstatic_data_members_of(^^U, access_context::unchecked())[I];
                using FieldType = [:type_of(member):];
                if (I != 0) out += ',';
                out += identifier_of(member);
                out += ':';
                out += std::string_view(::boost::typelayout::to_fixed_string<offset_of(member).bytes>());
                append_signature<FieldType>(out);
            };
            (field.template operator()<Is>(), ...);
            out += '}';
        }

        template <typename T>
        consteval void append_signature(std::string& out) {
            using U = std::remove_cv_t<T>;
            if constexpr (fixed_size_scalar<U> || std::is_enum_v<U> || is_fixed_string_v<U>) {
            } else if constexpr (std::is_array_v<U>) {
                out += '[';
                append_signature<std::remove_extent_t<U>>(out);
                out += ']';
            } else if constexpr (is_fixed_flat_map_v<U>) {
                out += '<';
                append_signature<typename U::key_type>(out);
                out += ',';
                append_signature<typename U::mapped_type>(out);
                out += '>';
            } else if constexpr (is_fixed_vector_v<U> || is_fixed_flat_set_v<U>) {
                out += '<';
                append_signature<typename U::value_type>(out);
                out += '>';
            } else if constexpr (std::is_class_v<U>) {
                static_assert(reflected_base_count<U>() == 0,
                    "schema name signature: wire records have no base classes");
                append_record<U>(out, std::make_index_sequence<reflected_member_count<U>()>{});
            } else {
                static_assert(sizeof(U) == 0, "schema name signature: non-admitted wire type");
            }
        }

        template <typename T>
        consteval std::string name_signature() {
            std::string out;
            append_signature<T>(out);
            return out;
        }

        template <typename T>
        consteval std::uint64_t name_digest() {
            return fnv1a(name_signature<T>());
        }

        template <typename A, typename B>
        consteval bool same_names() {
            return name_signature<A>() == name_signature<B>();
        }
    } // namespace detail::schema_names

    // fnv1a of the name signature above.
    template <typename T>
    inline constexpr std::uint64_t schema_name_digest_v = detail::schema_names::name_digest<T>();

    // The name signature as text (diagnostics, the schema lock file).
    template <typename T>
    std::string schema_name_signature() {
        constexpr std::size_t n = detail::schema_names::name_signature<T>().size();
        static constexpr auto text = [] {
            std::array<char, n + 1> a{};
            const auto s = detail::schema_names::name_signature<T>();
            for (std::size_t i = 0; i < n; ++i) a[i] = s[i];
            return a;
        }();
        return std::string(text.data(), n);
    }
} // namespace XOffsetDatastructure

#endif // X_OFFSET_SCHEMA_NAMES_HPP
