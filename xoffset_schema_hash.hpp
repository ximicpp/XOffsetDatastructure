#ifndef X_OFFSET_SCHEMA_HASH_HPP
#define X_OFFSET_SCHEMA_HASH_HPP

// A wire type's ABI signature and schema hash, the implicit-padding check,
// schema pins (K7), and the reflection-based construction and transfer of
// records. Part of xoffsetdatastructure.hpp; include that header. Pins:
// docs/SCHEMA_PINS.md; hash and evolution: docs/SCHEMA_EVOLUTION.md.

#include "xoffset_wire.hpp"

namespace XOffsetDatastructure {
    // Add-only type evolution (docs/SCHEMA_EVOLUTION.md):
    // XOFFSET_SCHEMA_FAMILY(New, Prev) (xoffset_schema_family.hpp) names Prev
    // the version New adds fields to. The versions of one family have the
    // same image bytes and carry the fingerprint of the first one.
    template <typename T>
    struct xschema_family {
        using previous = void;
    };

    namespace detail {
        template <typename T, typename Previous = typename xschema_family<T>::previous>
        struct schema_family_first {
            using type = typename schema_family_first<Previous>::type;
        };
        template <typename T>
        struct schema_family_first<T, void> {
            using type = T;
        };
    } // namespace detail

    // The first version of T's family (T itself when it declares none).
    template <typename T>
    using schema_family_first_t = typename detail::schema_family_first<std::remove_cv_t<T>>::type;

    // T is a later version of a family: its fingerprint is the first's.
    template <typename T>
    inline constexpr bool in_schema_family_v =
        !std::is_void_v<typename xschema_family<std::remove_cv_t<T>>::previous>;

    // ========================================================================
    // Reflection-based construction and transfer (C++26 P2996).
    // ========================================================================
    namespace detail {
        namespace ft = ::XOffsetDatastructure::fixed_text;

        consteval std::uint64_t fnv1a_append(std::uint64_t seed, std::string_view sv) {
            constexpr std::uint64_t prime = 1099511628211ull;
            for (char ch : sv) {
                seed ^= static_cast<unsigned char>(ch);
                seed *= prime;
            }
            return seed;
        }

        consteval std::uint64_t fnv1a(std::string_view sv) {
            return fnv1a_append(14695981039346656037ull, sv);
        }

        template <typename T>
        consteval std::uint64_t wire_root_type_id() {
            constexpr auto name = schema_name<T>::value;
            static_assert(!name.empty(),
                "wire_root_type_id<T> requires XOFFSET_REGISTER_SCHEMA_NAME(T, \"...\")");
            return fnv1a(name);
        }

        template <typename T>
        struct is_xkeyvalue : std::false_type {};
        template <typename K, typename V>
        struct is_xkeyvalue<XKeyValue<K, V>> : std::true_type {};
        template <typename T>
        inline constexpr bool is_xkeyvalue_v = is_xkeyvalue<std::remove_cv_t<T>>::value;

        template <typename T>
        consteval auto wire_abi_signature();

        // Does a set or map key order by plain char? char is signed on x86-64,
        // Apple arm64 and Windows and unsigned on Linux and Android arm64, so
        // the two sides sort such keys differently and each rejects the other's
        // wire while the schema hash is the same. XString compares as unsigned
        // char everywhere and is portable.
        template <typename T>
        consteval bool key_orders_by_plain_char();

        template <typename T, std::size_t... Is>
        consteval bool record_orders_by_plain_char(std::index_sequence<Is...>) {
            return (false || ... || key_orders_by_plain_char<typename schema::member_t<T, Is>::type>());
        }

        template <typename T>
        consteval bool key_orders_by_plain_char() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, char>) {
                return true;
            } else if constexpr (std::is_enum_v<U>) {
                return std::is_same_v<std::underlying_type_t<U>, char>;
            } else if constexpr (std::is_array_v<U>) {
                return key_orders_by_plain_char<std::remove_extent_t<U>>();
            } else if constexpr (is_fixed_string_v<U> || is_fixed_flat_map_v<U> ||
                                 is_fixed_flat_set_v<U> || is_fixed_vector_v<U> ||
                                 is_xkeyvalue_v<U>) {
                return false;
            } else if constexpr (std::is_class_v<U>) {
                return record_orders_by_plain_char<U>(
                    std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                return false;
            }
        }

        template <typename K>
        consteval void require_portable_key() {
            static_assert(!key_orders_by_plain_char<K>(),
                "XOffset wire schema: a set or map key orders by plain char, which is signed "
                "on x86-64, Apple and Windows and unsigned on Linux and Android arm64, so the "
                "two sides sort the keys differently and reject each other's wires. Use "
                "std::int8_t or std::uint8_t (XString for text).");
        }

        template <typename E>
        consteval void require_fixed_enum() {
            static_assert(requires { E{std::underlying_type_t<E>{}}; },
                "XOffset wire schema: an enum without a fixed underlying type has a "
                "compiler-chosen representation (int on MSVC, unsigned int on GCC and Clang "
                "when no value is negative), so its schema differs between platforms. Give it "
                "one: enum E : std::uint32_t { ... } or enum class E { ... }.");
        }

        template <typename T>
        consteval auto wire_scalar_signature() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, bool>) {
                return ft::FixedString{"bool"};
            } else if constexpr (std::is_same_v<U, char>) {
                return ft::FixedString{"char"};
            } else if constexpr (std::is_same_v<U, signed char>) {
                return ft::FixedString{"i8char"};
            } else if constexpr (std::is_same_v<U, unsigned char>) {
                return ft::FixedString{"u8char"};
            } else if constexpr (std::is_same_v<U, std::byte>) {
                return ft::FixedString{"byte"};
            } else if constexpr (std::is_same_v<U, int8_t>) {
                return ft::FixedString{"i8"};
            } else if constexpr (std::is_same_v<U, uint8_t>) {
                return ft::FixedString{"u8"};
            } else if constexpr (std::is_same_v<U, int16_t>) {
                return ft::FixedString{"i16"};
            } else if constexpr (std::is_same_v<U, uint16_t>) {
                return ft::FixedString{"u16"};
            } else if constexpr (std::is_same_v<U, int32_t>) {
                return ft::FixedString{"i32"};
            } else if constexpr (std::is_same_v<U, uint32_t>) {
                return ft::FixedString{"u32"};
            } else if constexpr (std::is_same_v<U, int64_t>) {
                return ft::FixedString{"i64"};
            } else if constexpr (std::is_same_v<U, uint64_t>) {
                return ft::FixedString{"u64"};
            } else if constexpr (std::is_same_v<U, float>) {
                return ft::FixedString{"f32"};
            } else if constexpr (std::is_same_v<U, double>) {
                return ft::FixedString{"f64"};
            } else {
                static_assert(sizeof(U) == 0, "unsupported v1 scalar in wire_abi_signature()");
            }
        }

        template <typename T, std::size_t N>
        consteval auto fixed_sequence_header_signature(const ft::FixedString<N>& tag) {
            return tag +
                   ft::FixedString{"[s:"} +
                   ft::to_fixed_string<sizeof(std::remove_cv_t<T>)>() +
                   ft::FixedString{",a:"} +
                   ft::to_fixed_string<alignof(std::remove_cv_t<T>)>() +
                   ft::FixedString{"]{data:rel32,size:u32,capacity:u32}"};
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature() {
            using U = std::remove_cv_t<T>;
            using member = schema::member_t<U, Index>;
            static_assert(!member::is_bit_field,
                "wire_abi_signature() does not support bit-fields in v1");
            using FieldType = typename member::type;
            return ft::FixedString{"@"} +
                   ft::to_fixed_string<member::offset>() +
                   ft::FixedString{":"} +
                   wire_abi_signature<FieldType>();
        }

        template <typename T, std::size_t Index>
        consteval auto wire_record_field_signature_with_sep() {
            if constexpr (Index == 0) {
                return wire_record_field_signature<T, Index>();
            } else {
                return ft::FixedString{","} + wire_record_field_signature<T, Index>();
            }
        }

        template <typename T, std::size_t... Is>
        consteval auto wire_record_fields(std::index_sequence<Is...>) {
            if constexpr (sizeof...(Is) == 0) {
                return ft::FixedString{""};
            } else {
                return (wire_record_field_signature_with_sep<T, Is>() + ...);
            }
        }

        template <typename T>
        consteval void require_natural_layout() {
            static_assert(natural_record_layout<T>() || !field_list_complete<T>(),
                "XOffset wire schema: this record does not have the natural layout (each member "
                "at the next offset its type's alignment allows, the size rounded up to the "
                "record's alignment). #pragma pack, /Zp, -fpack-struct, alignas on a member, "
                "[[no_unique_address]] or an unnamed bit-field changed it, and another compiler "
                "or another flag lays the record out differently, so peers could not exchange "
                "it byte for byte. Remove it (alignas on the record itself, up to 8, is fine).");
        }

        template <typename T>
        consteval auto wire_record_signature() {
            using U = std::remove_cv_t<T>;
            static_assert(schema::base_count_v<U> == 0,
                "XOffset wire schema: a wire type (a root with XOFFSET_REGISTER_SCHEMA_NAME and every "
                "record it contains) cannot have base classes, because compilers lay out base classes "
                "differently. Make the base a member instead; base classes are fine in local buffers "
                "(no schema name).");
            if constexpr (schema::base_count_v<U> == 0) require_natural_layout<U>();
            constexpr std::size_t field_count = schema::member_count_v<U>;
            return ft::FixedString{"record[s:"} +
                   ft::to_fixed_string<sizeof(U)>() +
                   ft::FixedString{",a:"} +
                   ft::to_fixed_string<alignof(U)>() +
                   ft::FixedString{"]{"} +
                   wire_record_fields<U>(std::make_index_sequence<field_count>{}) +
                   ft::FixedString{"}"};
        }

        template <typename T>
        consteval auto wire_abi_signature() {
            using U = std::remove_cv_t<T>;
            if constexpr (detail::fixed_size_scalar<U>) {
                return wire_scalar_signature<U>();
            } else if constexpr (std::is_enum_v<U>) {
                require_fixed_enum<U>();
                return ft::FixedString{"enum<"} +
                       wire_abi_signature<std::underlying_type_t<U>>() +
                       ft::FixedString{">"};
            } else if constexpr (std::is_array_v<U>) {
                return ft::FixedString{"array["} +
                       ft::to_fixed_string<std::extent_v<U>>() +
                       ft::FixedString{"]<"} +
                       wire_abi_signature<std::remove_extent_t<U>>() +
                       ft::FixedString{">"};
            } else if constexpr (is_spare_v<U>) {
                // Reserved bytes: opaque, alignment 1, never a record.
                return ft::FixedString{"spare["} +
                       ft::to_fixed_string<sizeof(U)>() +
                       ft::FixedString{"]"};
            } else if constexpr (is_fixed_string_v<U>) {
                return fixed_sequence_header_signature<U>(ft::FixedString{"xstring"}) +
                       ft::FixedString{"{inline:10}"};
            } else if constexpr (is_fixed_flat_map_v<U>) {
                require_portable_key<typename U::key_type>();
                return fixed_sequence_header_signature<U>(ft::FixedString{"xflatmap"}) +
                       ft::FixedString{"<"} +
                       wire_abi_signature<typename U::key_type>() +
                       ft::FixedString{","} +
                       wire_abi_signature<typename U::mapped_type>() +
                       ft::FixedString{">"};
            } else if constexpr (is_fixed_flat_set_v<U>) {
                require_portable_key<typename U::value_type>();
                return fixed_sequence_header_signature<U>(ft::FixedString{"xflatset"}) +
                       ft::FixedString{"<"} +
                       wire_abi_signature<typename U::value_type>() +
                       ft::FixedString{">"};
            } else if constexpr (is_fixed_vector_v<U>) {
                if constexpr (std::is_same_v<typename U::value_type, std::byte>) {
                    return fixed_sequence_header_signature<U>(ft::FixedString{"xblob"});
                } else {
                    return fixed_sequence_header_signature<U>(ft::FixedString{"xvector"}) +
                           ft::FixedString{"<"} +
                           wire_abi_signature<typename U::value_type>() +
                           ft::FixedString{">"};
                }
            } else if constexpr (is_xkeyvalue_v<U>) {
                require_natural_layout<U>();
                return ft::FixedString{"xkv[s:"} +
                       ft::to_fixed_string<sizeof(U)>() +
                       ft::FixedString{",a:"} +
                       ft::to_fixed_string<alignof(U)>() +
                       ft::FixedString{"]<"} +
                       wire_abi_signature<typename U::first_type>() +
                       ft::FixedString{","} +
                       wire_abi_signature<typename U::second_type>() +
                       ft::FixedString{">"};
            } else if constexpr (is_empty_std_array_v<U>) {
                static_assert(!is_empty_std_array_v<U>,
                    "XOffset wire schema: std::array<E, 0> is an empty class in libstdc++ and "
                    "libc++ and one element in MSVC's library, so its layout differs between "
                    "platforms; leave the member out.");
            } else if constexpr (std::is_class_v<U>) {
                return wire_record_signature<U>();
            } else {
                static_assert(sizeof(U) == 0,
                    "wire_abi_signature() encountered a non-admitted v1 type. Scalars must be "
                    "bool, char, signed or unsigned char, std::byte, std::int8_t to std::uint64_t, "
                    "float or double: long double, wchar_t, char8_t, char16_t, char32_t and integer "
                    "spellings whose width differs between platforms are not portable (std::int64_t "
                    "is long on Linux and long long on Windows and Apple, so write the fixed-width "
                    "name); pointers, references and unions are not admitted.");
            }
        }

        template <typename T>
        consteval std::uint64_t wire_schema_hash() {
            if constexpr (in_schema_family_v<T>) {
                // A later version of a family carries the first version's
                // fingerprint: the same image bytes under one hash.
                using First = schema_family_first_t<T>;
                static_assert(schema_name<std::remove_cv_t<T>>::value == schema_name<First>::value,
                    "XOFFSET_SCHEMA_FAMILY: the versions of one family must register the same "
                    "schema name (XOFFSET_REGISTER_SCHEMA_NAME), since the fingerprint covers it");
                return wire_schema_hash<First>();
            } else {
                constexpr auto sig = wire_abi_signature<T>();
                std::uint64_t hash = fnv1a(schema_name<T>::value);
                hash = fnv1a_append(hash, std::string_view(sig));
                return hash;
            }
        }

        template <typename T>
        concept has_wire_allocator_ctor = requires(fixed_detail::allocator_ref alloc) {
            T(alloc);
        };

        inline fixed_detail::allocator_ref allocator_binding_for(XBufferCore& xbuf) {
            return {xbuf.arena()};
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc);

        template <typename T>
        void destroy_reflected_prefix(void* raw, std::size_t built) noexcept {
            constexpr auto base_count = schema::base_count_v<T>;
            constexpr auto member_count = schema::member_count_v<T>;
            auto* obj = reinterpret_cast<T*>(raw);
            static_for<member_count>([&](auto index) {
                constexpr auto i = member_count - 1 - decltype(index)::value;
                using member = schema::member_t<T, i>;
                if (base_count + i < built) std::destroy_at(&(member::get(*obj)));
            });
            static_for<base_count>([&](auto index) {
                constexpr auto i = base_count - 1 - decltype(index)::value;
                using Base = schema::base_t<T, i>;
                if (i < built) std::destroy_at(static_cast<Base*>(obj));
            });
        }

        template <typename T, typename Alloc>
        consteval bool reflect_default_init_nothrow();

        template <typename T, typename Alloc, std::size_t... B, std::size_t... M>
        consteval bool reflect_default_init_nothrow_parts(std::index_sequence<B...>, std::index_sequence<M...>) {
            return (true && ... && reflect_default_init_nothrow<schema::base_t<T, B>, Alloc>()) &&
                   (true && ... && reflect_default_init_nothrow<typename schema::member_t<T, M>::type, Alloc>());
        }

        template <typename T, typename Alloc>
        consteval bool reflect_default_init_nothrow() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_array_v<U>) {
                return reflect_default_init_nothrow<std::remove_extent_t<U>, Alloc>();
            } else if constexpr (requires(const Alloc& value) { U(value); }) {
                return noexcept(U(std::declval<const Alloc&>()));
            } else if constexpr (!std::is_trivially_copyable_v<U> && std::is_class_v<U>) {
                return reflect_default_init_nothrow_parts<U, Alloc>(
                    std::make_index_sequence<schema::base_count_v<U>>{},
                    std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                return std::is_nothrow_default_constructible_v<U>;
            }
        }

        template <typename T, std::size_t N, typename Alloc>
        XOFFSET_DETAIL_INLINE void reflect_init_nth(void* raw, Alloc alloc) {
            using member = schema::member_t<T, N>;
            T& obj = *reinterpret_cast<T*>(raw);
            fixed_detail::default_construct(&(member::get(obj)), alloc);
        }

        template <typename T, std::size_t N, typename Alloc>
        XOFFSET_DETAIL_INLINE void reflect_init_base_nth(void* raw, Alloc alloc) {
            using BaseType = schema::base_t<T, N>;
            T* obj = reinterpret_cast<T*>(raw);
            reflect_init_all_impl<BaseType>(static_cast<void*>(static_cast<BaseType*>(obj)), alloc);
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc) {
            if constexpr (reflect_default_init_nothrow<T, Alloc>()) {
                for_each_reflected_subobject<T>(
                [&](auto index) XOFFSET_DETAIL_LAMBDA_INLINE {
                    reflect_init_base_nth<T, decltype(index)::value>(raw, alloc);
                },
                [&](auto index) XOFFSET_DETAIL_LAMBDA_INLINE {
                    reflect_init_nth<T, decltype(index)::value>(raw, alloc);
                });
            } else {
                std::size_t built = 0;
                XOFFSET_TRY {
                    for_each_reflected_subobject<T>(
                    [&](auto index) {
                        reflect_init_base_nth<T, decltype(index)::value>(raw, alloc);
                        ++built;
                    },
                    [&](auto index) {
                        reflect_init_nth<T, decltype(index)::value>(raw, alloc);
                        ++built;
                    });
                } XOFFSET_CATCH(...) {
                    destroy_reflected_prefix<T>(raw, built);
                    XOFFSET_RETHROW;
                }
            }
        }

        template <typename T, typename Alloc>
        XOFFSET_DETAIL_INLINE void reflect_init_all_small(void* raw, Alloc alloc) noexcept {
            std::memset(raw, 0, sizeof(T));
            raw = static_cast<void*>(begin_fresh_ordinary_outer<T>(raw));
            for_each_reflected_subobject<T>(
            [&](auto index) XOFFSET_DETAIL_LAMBDA_INLINE {
                reflect_init_base_nth<T, decltype(index)::value>(raw, alloc);
            },
            [&](auto index) XOFFSET_DETAIL_LAMBDA_INLINE {
                reflect_init_nth<T, decltype(index)::value>(raw, alloc);
            });
        }

        template <typename T, typename Alloc>
        XOFFSET_DETAIL_INLINE void reflect_init_all(void* raw, Alloc alloc) {
            if constexpr (sizeof(T) <= 256 && reflect_default_init_nothrow<T, Alloc>()) {
                reflect_init_all_small<T>(raw, alloc);
                return;
            }
            std::memset(raw, 0, sizeof(T));  // zero ONCE at top level
            raw = static_cast<void*>(begin_fresh_ordinary_outer<T>(raw));
            reflect_init_all_impl<T>(raw, alloc);
        }
        // reflect_init_all inlined: a root's construction (make<T>(), and
        // reset<T>() once per message), where the call costs as much as
        // the stores of a small root.
        template <typename T, typename Alloc>
        XOFFSET_DETAIL_INLINE void reflect_init_root(void* raw, Alloc alloc) {
            std::memset(raw, 0, sizeof(T));
            raw = static_cast<void*>(begin_fresh_ordinary_outer<T>(raw));
            reflect_init_all_impl<T>(raw, alloc);
        }

        // -- Reflection Transfer (move/copy with allocator injection) --

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all_impl(void* dst, Src&& src, Alloc alloc);

        template <typename T, std::size_t N, typename Src, typename Alloc>
        void reflect_transfer_init_nth(void* dst, Src&& src, Alloc alloc) {
            using member = schema::member_t<T, N>;
            T& d = *reinterpret_cast<T*>(dst);
            fixed_detail::transfer_construct(&(member::get(d)), member::get(src), alloc);
        }

        template <typename T, std::size_t N, typename Src, typename Alloc>
        void reflect_transfer_base_nth(void* dst, Src&& src, Alloc alloc) {
            using BaseType = schema::base_t<T, N>;
            T* dst_obj = reinterpret_cast<T*>(dst);
            BaseType* dst_base = static_cast<BaseType*>(dst_obj);
            if constexpr (std::is_lvalue_reference_v<Src&&>) {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<const BaseType&>(src), alloc);
            } else {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<BaseType&&>(std::move(src)), alloc);
            }
        }

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all_impl(void* dst, Src&& src, Alloc alloc) {
            std::size_t built = 0;
            XOFFSET_TRY {
                for_each_reflected_subobject<T>(
                [&](auto index) {
                    reflect_transfer_base_nth<T, decltype(index)::value>(dst, std::forward<Src>(src), alloc);
                    ++built;
                },
                [&](auto index) {
                    reflect_transfer_init_nth<T, decltype(index)::value>(dst, std::forward<Src>(src), alloc);
                    ++built;
                });
            } XOFFSET_CATCH(...) {
                destroy_reflected_prefix<T>(dst, built);
                XOFFSET_RETHROW;
            }
        }

        template <typename T, typename Src, typename Alloc>
        void reflect_transfer_init_all(void* dst, Src&& src, Alloc alloc) {
            std::memset(dst, 0, sizeof(T));  // zero ONCE at top level
            dst = static_cast<void*>(begin_fresh_ordinary_outer<T>(dst));
            reflect_transfer_init_all_impl<T>(dst, std::forward<Src>(src), alloc);
        }

        // Internal wrapper: constructs T via reflection on raw storage.
        template <typename T>
        struct ReflectRoot {
            alignas(T) unsigned char storage[sizeof(T)];

            template <typename Alloc>
            XOFFSET_DETAIL_INLINE ReflectRoot(Alloc alloc) {
                reflect_init_root<T>(storage, alloc);
            }

            T& ref() noexcept {
                return *std::launder(reinterpret_cast<T*>(storage));
            }
            const T& ref() const noexcept {
                return *std::launder(reinterpret_cast<const T*>(storage));
            }
        };

        template <typename T, bool BindType = true>
        XOFFSET_DETAIL_INLINE T* construct_root(XBufferCore& xbuf) {
            auto alloc = allocator_binding_for(xbuf);
            if constexpr (has_wire_allocator_ctor<T>) {
                return xbuf.template construct_root_storage<T, BindType>(alloc);
            } else {
                auto* wrapper = xbuf.template construct_root_storage<ReflectRoot<T>, BindType>(alloc);
                return &wrapper->ref();
            }
        }

        // T's root constructed empty in `raw`, its block at root_at (see
        // XBufferCore::reset_keeping_root).
        template <typename T>
        XOFFSET_DETAIL_INLINE T* construct_root_at(XBufferCore& xbuf, char* raw) {
            const fixed_detail::allocator_ref alloc{xbuf.arena_unchecked()};
            if constexpr (has_wire_allocator_ctor<T>) {
                return std::construct_at(reinterpret_cast<T*>(raw), alloc);
            } else {
                return &std::construct_at(reinterpret_cast<ReflectRoot<T>*>(raw), alloc)->ref();
            }
        }

        template <typename T>
        T* find_root(XBufferCore& xbuf) {
            if constexpr (has_wire_allocator_ctor<T>) {
                return xbuf.template find_root_storage<T>();
            } else {
                auto* wrapper = xbuf.template find_root_storage<ReflectRoot<T>>();
                return wrapper ? &wrapper->ref() : nullptr;
            }
        }

        // The storage type of T's root object (T itself, or its reflected wrapper).
        template <typename T>
        using root_storage_t = std::conditional_t<has_wire_allocator_ctor<T>, T, ReflectRoot<T>>;

        // find_root for an image known to hold T's root (used_end >= 40 +
        // sizeof(root_storage_t<T>), checked by the caller).
        template <typename T>
        XOFFSET_DETAIL_INLINE T* image_root(XBufferCore& xbuf) noexcept {
            auto* storage = xbuf.template root_storage_unchecked<root_storage_t<T>>();
            if constexpr (has_wire_allocator_ctor<T>) return storage;
            else return &storage->ref();
        }

        // Wire header of an image of `used` bytes with T's fingerprint and
        // the given free state; the writer sets extent_count. Sizes fit:
        // used_bytes <= MAX_PAYLOAD < 2 GiB.
        template <typename T>
        constexpr XWireHeader make_wire_header(std::uint64_t used, std::uint32_t free_dir = 0,
                                               std::uint32_t free_bytes = 0) noexcept {
            XWireHeader header{};
            header.magic = XWIRE_MAGIC;
            header.header_bytes = static_cast<std::uint32_t>(sizeof(XWireHeader));
            header.used_bytes = static_cast<std::uint32_t>(used);
            header.extent_count = 0;
            header.free_dir = free_dir;
            header.free_bytes = free_bytes;
            header.schema_hash = wire_schema_hash<T>();
            header.root_type_id = wire_root_type_id<T>();
            return header;
        }

    } // namespace detail (reflection)

    template <typename T>
    consteval std::uint64_t wire_root_type_id_v() {
        return detail::wire_root_type_id<T>();
    }

    template <typename T>
    consteval auto wire_abi_signature() {
        return detail::wire_abi_signature<T>();
    }

    // M6 (threat model T5/P3): implicit padding inside a wire type is copied
    // into the arena as whatever bytes the source object held, and from there
    // into every emitted wire. has_implicit_padding_v<T> is true when T, or any
    // record, array element or container element type reachable from it, has
    // bytes not covered by a field.
    namespace detail {
        template <typename T>
        consteval bool has_implicit_padding();

        template <typename U, std::size_t... Is>
        consteval bool record_has_implicit_padding(std::index_sequence<Is...>) {
            std::size_t covered = 0;
            bool padded = false;
            auto field = [&]<std::size_t I>() consteval {
                using member = schema::member_t<U, I>;
                using FieldType = typename member::type;
                if (member::offset != covered) padded = true;
                covered = member::offset + sizeof(FieldType);
                if (has_implicit_padding<FieldType>()) padded = true;
            };
            (field.template operator()<Is>(), ...);
            return padded || covered != sizeof(U);
        }

        template <typename T>
        consteval bool has_implicit_padding() {
            using U = std::remove_cv_t<T>;
            if constexpr (fixed_size_scalar<U> || std::is_enum_v<U> || is_fixed_string_v<U>) {
                return false;
            } else if constexpr (std::is_array_v<U>) {
                return has_implicit_padding<std::remove_extent_t<U>>();
            } else if constexpr (is_fixed_vector_v<U>) {
                // The 12-byte head is three u32 fields; elements live in the arena.
                return has_implicit_padding<typename U::value_type>();
            } else if constexpr (is_xkeyvalue_v<U>) {
                return record_has_implicit_padding<U>(std::make_index_sequence<2>{});
            } else if constexpr (std::is_class_v<U>) {
                static_assert(schema::base_count_v<U> == 0,
                    "has_implicit_padding_v does not support base classes");
                return record_has_implicit_padding<U>(
                    std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                static_assert(sizeof(U) == 0, "has_implicit_padding_v: non-admitted wire type");
            }
        }
    } // namespace detail

    template <typename T>
    inline constexpr bool has_implicit_padding_v = detail::has_implicit_padding<T>();

    // M6: the library zeroes implicit padding of every container element it
    // constructs or relocates, so neither stale arena bytes nor the caller's
    // stack (a copied temporary) reach the wire through padding. Covers
    // records, arrays and XKeyValue (map entries); padding of a record that
    // the caller assigns directly is outside the library's reach.
    namespace detail {
        // `count` runs of `bytes` padding bytes, `stride` apart from `offset`.
        // An array contributes its element's ranges once, repeated by count,
        // so a large array costs no more compile time than one element.
        struct PaddingRange { std::size_t offset; std::size_t bytes; std::size_t stride = 0; std::size_t count = 1; };

        template <typename T>
        consteval std::vector<PaddingRange> merged_padding();

        template <typename T>
        consteval void collect_padding(std::vector<PaddingRange>& out, std::size_t base);

        template <typename U, std::size_t... Is>
        consteval void collect_record_padding(std::vector<PaddingRange>& out, std::size_t base,
                                              std::index_sequence<Is...>) {
            std::size_t covered = 0;
            auto field = [&]<std::size_t I>() consteval {
                using member = schema::member_t<U, I>;
                using FieldType = typename member::type;
                constexpr std::size_t offset = member::offset;
                if (offset > covered) out.push_back({base + covered, offset - covered, 0, 1});
                collect_padding<FieldType>(out, base + offset);
                if (offset + sizeof(FieldType) > covered) covered = offset + sizeof(FieldType);
            };
            (field.template operator()<Is>(), ...);
            if (covered < sizeof(U)) out.push_back({base + covered, sizeof(U) - covered, 0, 1});
        }

        template <typename T>
        consteval void collect_padding(std::vector<PaddingRange>& out, std::size_t base) {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_array_v<U>) {
                using B = std::remove_all_extents_t<U>;
                constexpr std::size_t total = sizeof(U) / sizeof(B);
                if constexpr (std::is_class_v<B>) {
                    for (const auto& r : merged_padding<B>()) {
                        if (r.count == 1) {
                            out.push_back({base + r.offset, r.bytes, sizeof(B), total});
                        } else {
                            for (std::size_t i = 0; i < total; ++i)
                                out.push_back({base + i * sizeof(B) + r.offset, r.bytes, r.stride, r.count});
                        }
                    }
                }
            } else if constexpr (is_xkeyvalue_v<U>) {
                collect_record_padding<U>(out, base, std::make_index_sequence<2>{});
            } else if constexpr (std::is_class_v<U> && !is_fixed_string_v<U> && !is_fixed_vector_v<U>) {
                // Records with bases are left alone (layout of base subobjects
                // is not walked here).
                if constexpr (schema::base_count_v<U> == 0)
                    collect_record_padding<U>(out, base,
                        std::make_index_sequence<schema::member_count_v<U>>{});
            }
        }

        template <typename T>
        consteval std::vector<PaddingRange> merged_padding() {
            std::vector<PaddingRange> raw, merged;
            collect_padding<T>(raw, 0);
            for (const auto& r : raw) {
                if (r.count == 1 && !merged.empty() && merged.back().count == 1 &&
                    merged.back().offset + merged.back().bytes == r.offset)
                    merged.back().bytes += r.bytes;
                else
                    merged.push_back(r);
            }
            return merged;
        }

        template <typename T>
        consteval auto padding_range_array() {
            constexpr std::size_t n = merged_padding<T>().size();
            std::array<PaddingRange, n> ranges{};
            const auto v = merged_padding<T>();
            for (std::size_t i = 0; i < n; ++i) ranges[i] = v[i];
            return ranges;
        }

        template <typename T>
        inline constexpr auto padding_ranges = padding_range_array<T>();

        template <class T>
        XOFFSET_DETAIL_INLINE_GCC_BELOW_26 void zero_implicit_padding(T* object) noexcept {
            if constexpr (!std::is_class_v<std::remove_cv_t<T>> && !std::is_array_v<std::remove_cv_t<T>>) {
                (void)object;
            } else if constexpr (padding_ranges<T>.size() == 0) {
                (void)object;
            } else {
                auto* bytes = reinterpret_cast<unsigned char*>(object);
                for (const auto& r : padding_ranges<T>)
                    for (std::size_t i = 0; i < r.count; ++i) std::memset(bytes + r.offset + i * r.stride, 0, r.bytes);
            }
        }
        // M6 at the places the library constructs or copies a stored element:
        // zero_implicit_padding with XOFFSET_SCRUB_PADDING, nothing without
        // (the padding then holds whatever the source value held there).
        template <class T>
        XOFFSET_DETAIL_INLINE_GCC_BELOW_26 void scrub_implicit_padding([[maybe_unused]] T* object) noexcept {
#if XOFFSET_SCRUB_PADDING
            zero_implicit_padding(object);
#endif
        }
    } // namespace detail

    // Opt-in per type: fails compilation when T carries implicit padding.
#define XOFFSET_REQUIRE_NO_PADDING(Type) \
    static_assert(!::XOffsetDatastructure::has_implicit_padding_v<Type>, \
        "XOffset: " #Type " has implicit padding bytes, which can carry stale memory into a wire " \
        "(threat model T5/P3); add explicit fields")

    template <typename T>
    consteval std::uint64_t wire_schema_hash_v() {
        return detail::wire_schema_hash<T>();
    }

    // K7 schema pins. XOFFSET_PIN_SCHEMA(T, "<signature>") fails compilation on
    // any compiler or target whose wire layout of T differs from the pinned
    // text; the diagnostic shows the layout that compiler computed.
    // schema_pin<T>("T") returns the pin line for the current layout.
    namespace detail {
        template <std::size_t N>
        struct SchemaPinMessage {
            char text[N]{};
            std::size_t length = 0;
            constexpr void append(std::string_view part) noexcept {
                for (char c : part) text[length++] = c;
            }
            constexpr const char* data() const noexcept { return text; }
            constexpr std::size_t size() const noexcept { return length; }
        };

        inline constexpr std::string_view schema_pin_message_head =
            "XOffset schema pin mismatch (K7): this compiler and target lay the type out as \"";
        inline constexpr std::string_view schema_pin_message_tail =
            "\"; update the pin only for an intended schema change";

        // Static storage, so a constexpr string_view may refer to it.
        template <typename T>
        inline constexpr auto schema_pin_signature = wire_abi_signature<T>();

        template <typename T>
        consteval bool schema_pin_matches(std::string_view pinned) {
            return std::string_view(schema_pin_signature<T>) == pinned;
        }

        template <typename T>
        consteval auto schema_pin_message() {
            constexpr std::string_view actual(schema_pin_signature<T>);
            SchemaPinMessage<schema_pin_message_head.size() + actual.size() +
                             schema_pin_message_tail.size()> message;
            message.append(schema_pin_message_head);
            message.append(actual);
            message.append(schema_pin_message_tail);
            return message;
        }
    } // namespace detail

    template <typename T>
    std::string schema_pin(std::string_view type_name) {
        static_assert(::XOffsetDatastructure::detail::explain_wire_admission<std::remove_cvref_t<T>>(),
            "schema_pin<T>() requires an admitted fixed-schema wire type.");
        constexpr auto signature = wire_abi_signature<T>();
        std::string line = "XOFFSET_PIN_SCHEMA(";
        line.append(type_name);
        line += ", \"";
        for (char c : std::string_view(signature)) {
            if (c == '"' || c == '\\') line += '\\';
            line += c;
        }
        line += "\");";
        return line;
    }
} // namespace XOffsetDatastructure

#endif // X_OFFSET_SCHEMA_HASH_HPP
