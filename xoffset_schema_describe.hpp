#ifndef X_OFFSET_SCHEMA_DESCRIBE_HPP
#define X_OFFSET_SCHEMA_DESCRIBE_HPP

// XOFFSET_RECORD's __VA_ARGS__ walk needs the conforming preprocessor; MSVC's
// traditional one passes the list as one argument (the CMake targets add
// /Zc:preprocessor themselves).
#if defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
#error "XOffset field lists need MSVC's conforming preprocessor: compile with /Zc:preprocessor"
#endif

// Field-list backend of the schema field interface (see xoffset_schema.hpp),
// and the XOFFSET_RECORD macros that write the field lists.
//
//   XOFFSET_RECORD(T, m1, m2, ...)
//   XOFFSET_RECORD_DERIVED(T, (Base1, Base2), m1, m2, ...)
//
// One line per record, at global scope, after the definition of T and before
// T is first used in a container or buffer. List every non-static data member
// of T, in declaration order; use an alias for a type name with commas.
// tools/describe_dump prints these lines with reflection.
//
// Each field is named through a member pointer plus offsetof, so a misspelt
// member does not compile, and the list is proven complete at compile time:
//   - offsets strictly ascending and members not overlapping (no duplicates,
//     declaration order);
//   - aggregate initialization count: T{x1..xE} is valid and T{x1..xE+1} is
//     not, where E counts the bases plus one initializer per member (a C array
//     member takes one per element). A size or tiling check is not enough: a
//     forgotten one-byte member can look like padding.
// T must therefore be an aggregate (no user-declared constructors, no private
// or protected members, no virtual functions).
//
// XOFFSET_RECORD also declares T an ordinary record, as
// XOFFSET_ORDINARY_RECORDS does (no bases, unions, bit-fields or
// [[no_unique_address]]; members never overlap), which turns on the SHIFT
// path, stable erase and raw receive for T. Without reflection the ordinary
// declaration (by either macro or a wire_record_layout specialization) also
// promises that T declares no special member function (destructor, copy/move
// constructor or assignment): the library cannot check that part, a C++26
// build checks all of it. XOFFSET_RECORD_DERIVED declares nothing; with an
// empty base list it describes a record without the ordinary declaration.
//
// Under C++26 reflection stays the backend: a record written with
// XOFFSET_RECORD is checked against reflection (members, order, offsets,
// types, bases, promise) and otherwise described by reflection as before.
// XOFFSET_SCHEMA_BACKEND_DESCRIBE=1 forces this backend on a C++26 compiler.
//
// Enums: without reflection the enumerators of an enum are unknown, so an
// enum used in raw receive must have a fixed underlying type
// (enum class, or enum E : std::uint8_t {...}).

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure::schema {
    // description<T>: specialized by XOFFSET_RECORD, XOFFSET_RECORD_DERIVED and
    // for the library's own types.
    template <class T>
    struct description {
        static constexpr bool described = false;
        static constexpr bool ordinary = false;
    };

    // fields<T>: one nested struct per member of a described T (field_<name>),
    // written by the macros below.
    template <class T> struct fields;

    namespace describe_detail {
        template <class> struct member_pointer_traits;
        template <class C, class M> struct member_pointer_traits<M C::*> {
            using class_type = C;
            using type = M;
        };

#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define XOFFSET_SCHEMA_HAS_TYPE_PACK_ELEMENT 1
#endif
#endif
        template <std::size_t I, class... Ts>
#ifdef XOFFSET_SCHEMA_HAS_TYPE_PACK_ELEMENT
        using type_at = __type_pack_element<I, Ts...>;
#else
        using type_at = std::tuple_element_t<I, std::tuple<Ts...>>;
#endif

        template <class... Bases> struct bases {};

        template <class T, bool Ordinary, class Bases, class... Fields> struct record;
        template <class T, bool Ordinary, class... B, class... F>
        struct record<T, Ordinary, bases<B...>, F...> {
            static constexpr bool described = true;
            static constexpr bool ordinary = Ordinary;
            static constexpr std::size_t base_count = sizeof...(B);
            static constexpr std::size_t member_count = sizeof...(F);
            template <std::size_t I> using base = type_at<I, B...>;
            template <std::size_t I> using member = type_at<I, F...>;
        };

        // ---- completeness proof ---------------------------------------------
        // Converts to any non-array type; only in unevaluated requires-expressions.
        struct any_init {
            template <class U> operator U() const noexcept;
        };

        // Initializer-clauses one member takes in T{...}: a C array is filled by
        // brace elision (any_init cannot become an array), anything else takes one.
        template <class M> consteval std::size_t init_clauses() {
            if constexpr (std::is_array_v<M>) return std::extent_v<M> * init_clauses<std::remove_extent_t<M>>();
            else return 1;
        }

        template <class T, std::size_t... I>
        consteval bool brace_init_with(std::index_sequence<I...>) {
            return requires { T{((void)I, any_init{})...}; };
        }

        template <class D>
        consteval std::size_t expected_clauses() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return D::base_count + (init_clauses<typename D::template member<I>::type>() + ... + 0);
            }(std::make_index_sequence<D::member_count>{});
        }

        template <class T>
        consteval bool fields_belong_to() {
            using D = description<T>;
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (std::is_same_v<typename D::template member<I>::class_type, T> && ...);
            }(std::make_index_sequence<D::member_count>{});
        }

        template <class T>
        consteval bool offsets_ascending() {
            using D = description<T>;
            return []<std::size_t... I>(std::index_sequence<I...>) {
                const std::size_t begin[] = {D::template member<I>::offset..., 0};
                const std::size_t end[] = {(D::template member<I>::offset +
                                            sizeof(typename D::template member<I>::type))..., 0};
                for (std::size_t i = 1; i < D::member_count; ++i)
                    if (begin[i] < end[i - 1]) return false;  // overlap, duplicate or out of order
                return true;
            }(std::make_index_sequence<D::member_count>{});
        }

        // Exact coverage: T{any x E} is valid and T{any x (E + 1)} is not.
        template <class T>
        consteval bool covers_all_members() {
            if constexpr (!std::is_aggregate_v<T>) return false;
            else {
                constexpr std::size_t e = expected_clauses<description<T>>();
                return brace_init_with<T>(std::make_index_sequence<e>{}) &&
                       !brace_init_with<T>(std::make_index_sequence<e + 1>{});
            }
        }

        template <class T>
        consteval bool fixed_underlying_type() {
            return requires { T{std::underlying_type_t<T>{}}; };
        }
    } // namespace describe_detail
} // namespace XOffsetDatastructure::schema

// ---- macros ------------------------------------------------------------------

// Applies macro(T, x) to each x (up to 256), each result preceded by a comma.
#define XOFFSET_SCHEMA_DETAIL_PARENS ()
#define XOFFSET_SCHEMA_DETAIL_EXPAND(...) XOFFSET_SCHEMA_DETAIL_EXPAND4(XOFFSET_SCHEMA_DETAIL_EXPAND4(XOFFSET_SCHEMA_DETAIL_EXPAND4(XOFFSET_SCHEMA_DETAIL_EXPAND4(__VA_ARGS__))))
#define XOFFSET_SCHEMA_DETAIL_EXPAND4(...) XOFFSET_SCHEMA_DETAIL_EXPAND3(XOFFSET_SCHEMA_DETAIL_EXPAND3(XOFFSET_SCHEMA_DETAIL_EXPAND3(XOFFSET_SCHEMA_DETAIL_EXPAND3(__VA_ARGS__))))
#define XOFFSET_SCHEMA_DETAIL_EXPAND3(...) XOFFSET_SCHEMA_DETAIL_EXPAND2(XOFFSET_SCHEMA_DETAIL_EXPAND2(XOFFSET_SCHEMA_DETAIL_EXPAND2(XOFFSET_SCHEMA_DETAIL_EXPAND2(__VA_ARGS__))))
#define XOFFSET_SCHEMA_DETAIL_EXPAND2(...) XOFFSET_SCHEMA_DETAIL_EXPAND1(XOFFSET_SCHEMA_DETAIL_EXPAND1(XOFFSET_SCHEMA_DETAIL_EXPAND1(XOFFSET_SCHEMA_DETAIL_EXPAND1(__VA_ARGS__))))
#define XOFFSET_SCHEMA_DETAIL_EXPAND1(...) __VA_ARGS__
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH(macro, T, ...)                          \
    __VA_OPT__(XOFFSET_SCHEMA_DETAIL_EXPAND(XOFFSET_SCHEMA_DETAIL_FOR_EACH_HELPER(macro, T, __VA_ARGS__)))
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH_HELPER(macro, T, a1, ...)               \
    , macro(T, a1)                                                             \
    __VA_OPT__(XOFFSET_SCHEMA_DETAIL_FOR_EACH_AGAIN XOFFSET_SCHEMA_DETAIL_PARENS (macro, T, __VA_ARGS__))
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH_AGAIN() XOFFSET_SCHEMA_DETAIL_FOR_EACH_HELPER
// The same without the commas.
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN(macro, T, ...)                    \
    __VA_OPT__(XOFFSET_SCHEMA_DETAIL_EXPAND(XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN_HELPER(macro, T, __VA_ARGS__)))
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN_HELPER(macro, T, a1, ...)         \
    macro(T, a1)                                                               \
    __VA_OPT__(XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN_AGAIN XOFFSET_SCHEMA_DETAIL_PARENS (macro, T, __VA_ARGS__))
#define XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN_AGAIN() XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN_HELPER
#define XOFFSET_SCHEMA_DETAIL_UNPAREN(...) __VA_ARGS__

#if defined(__GNUC__) || defined(__clang__)
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF(T, m) __builtin_offsetof(T, m)
// offsetof in a record with members in both a base and the derived class is
// conditionally-supported; GCC, Clang and MSVC support it without virtual
// bases (an aggregate has none).
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF_BEGIN                                   \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF_END _Pragma("GCC diagnostic pop")
#else
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF(T, m) offsetof(T, m)
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF_BEGIN
#define XOFFSET_SCHEMA_DETAIL_OFFSETOF_END
#endif

// The member's name after macro expansion (std::array's member is named
// through a macro).
#define XOFFSET_SCHEMA_DETAIL_STR(m) #m

// One described member of class C: its type (from &C::m, so a misspelt name
// does not compile), offsetof, and access by name. obj.m is the access
// reflection's obj.[:m:] is; a member pointer (obj.*p) does not always
// optimize the same (GCC 16).
#define XOFFSET_SCHEMA_DETAIL_NAMED_FIELD(C, m)                                \
    struct field_##m {                                                         \
        using class_type = typename ::XOffsetDatastructure::schema::describe_detail:: \
            member_pointer_traits<decltype(&C::m)>::class_type;                \
        using type = typename ::XOffsetDatastructure::schema::describe_detail:: \
            member_pointer_traits<decltype(&C::m)>::type;                      \
        static constexpr bool is_bit_field = false;                            \
        static constexpr ::std::size_t offset = XOFFSET_SCHEMA_DETAIL_OFFSETOF(C, m); \
        static constexpr ::std::string_view name = XOFFSET_SCHEMA_DETAIL_STR(m); \
        template <class Obj>                                                   \
        XOFFSET_SCHEMA_INLINE static constexpr decltype(auto) get(Obj&& obj) noexcept { \
            return (static_cast<Obj&&>(obj).m);                                \
        }                                                                      \
    };

#define XOFFSET_SCHEMA_DETAIL_FIELD_OF(T, m) ::XOffsetDatastructure::schema::fields<T>::field_##m

#define XOFFSET_SCHEMA_DETAIL_DESCRIPTION(T, ordinary, BASES, ...)              \
    XOFFSET_SCHEMA_DETAIL_OFFSETOF_BEGIN                                       \
    template <> struct XOffsetDatastructure::schema::fields<T> {               \
        XOFFSET_SCHEMA_DETAIL_FOR_EACH_PLAIN(XOFFSET_SCHEMA_DETAIL_NAMED_FIELD, T, __VA_ARGS__) \
    };                                                                         \
    XOFFSET_SCHEMA_DETAIL_OFFSETOF_END                                         \
    template <> struct XOffsetDatastructure::schema::description<T>            \
        : ::XOffsetDatastructure::schema::describe_detail::record<T, ordinary, \
              ::XOffsetDatastructure::schema::describe_detail::bases<XOFFSET_SCHEMA_DETAIL_UNPAREN BASES> \
              XOFFSET_SCHEMA_DETAIL_FOR_EACH(XOFFSET_SCHEMA_DETAIL_FIELD_OF, T, __VA_ARGS__)> {};

#define XOFFSET_SCHEMA_DETAIL_COMPLETE(macro, T)                               \
    static_assert(::std::is_class_v<T> && !::std::is_union_v<T>,              \
        macro "(" #T ", ...): " #T " must be a class type, not a union");      \
    static_assert(::std::is_aggregate_v<T>,                                    \
        macro "(" #T ", ...): " #T " must be an aggregate (no user-declared "  \
        "constructors, no private or protected members, no virtual functions)"); \
    static_assert(::XOffsetDatastructure::schema::describe_detail::fields_belong_to<T>(), \
        macro "(" #T ", ...): list only members declared in " #T " itself; "   \
        "name base classes in XOFFSET_RECORD_DERIVED(" #T ", (Base...), ...)"); \
    static_assert(::XOffsetDatastructure::schema::describe_detail::offsets_ascending<T>(), \
        macro "(" #T ", ...): list every member once, in declaration order "   \
        "(members must not overlap)");                                         \
    static_assert(::XOffsetDatastructure::schema::describe_detail::covers_all_members<T>(), \
        macro "(" #T ", ...): the list does not name every non-static data "   \
        "member of " #T ", or " #T " has a base class (XOFFSET_RECORD_DERIVED)");

// XOFFSET_SCHEMA_DETAIL_REFLECTION_CHECK is defined by the active backend,
// XOFFSET_CHECK_GLOBAL_NAMESPACE_ and XOFFSET_SCHEMA_DETAIL_ORDINARY (the
// ordinary-record declaration) by xoffsetdatastructure.hpp.
#define XOFFSET_RECORD(T, ...)                                                 \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_RECORD")                          \
    XOFFSET_SCHEMA_DETAIL_DESCRIPTION(T, true, (), __VA_ARGS__)                \
    XOFFSET_SCHEMA_DETAIL_COMPLETE("XOFFSET_RECORD", T)                        \
    XOFFSET_SCHEMA_DETAIL_REFLECTION_CHECK("XOFFSET_RECORD", T)                \
    XOFFSET_SCHEMA_DETAIL_ORDINARY(T)

#define XOFFSET_RECORD_DERIVED(T, BASES, ...)                                  \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_RECORD_DERIVED")                  \
    XOFFSET_SCHEMA_DETAIL_DESCRIPTION(T, false, BASES, __VA_ARGS__)            \
    XOFFSET_SCHEMA_DETAIL_COMPLETE("XOFFSET_RECORD_DERIVED", T)                \
    XOFFSET_SCHEMA_DETAIL_REFLECTION_CHECK("XOFFSET_RECORD_DERIVED", T)

#if !XOFFSET_SCHEMA_REFLECTION
// ---- the interface, answered from the field lists ------------------------------
namespace XOffsetDatastructure::schema {
    template <class T>
    inline constexpr bool is_described_v = description<std::remove_cv_t<T>>::described;

    // XOFFSET_RECORD takes aggregate classes only (an aggregate is never
    // polymorphic). Without reflection nothing is known about the members of
    // the rest, so the walks answer "not admitted" for them instead of asking
    // for a field list that cannot be written.
    template <class T>
    inline constexpr bool is_undescribable_v =
        (std::is_union_v<std::remove_cv_t<T>> ||
         (std::is_class_v<std::remove_cv_t<T>> && !std::is_aggregate_v<std::remove_cv_t<T>>)) &&
        !is_described_v<T>;

    namespace describe_detail {
        template <class T>
        struct of {
            static_assert(description<T>::described,
                "XOffset: this record has no field list. Add XOFFSET_RECORD(T, members...) "
                "(or XOFFSET_RECORD_DERIVED) at global scope after its definition; "
                "tools/describe_dump prints the line. See the instantiation context for T.");
            using type = description<T>;
        };
        template <class T> using of_t = typename of<std::remove_cv_t<T>>::type;
    } // namespace describe_detail

    template <class T>
    inline constexpr std::size_t base_count_v = describe_detail::of_t<T>::base_count;

    template <class T>
    inline constexpr std::size_t member_count_v = describe_detail::of_t<T>::member_count;

    template <class T, std::size_t I>
    using member_t = typename describe_detail::of_t<T>::template member<I>;

    template <class T, std::size_t I>
    using member_type_t = typename member_t<T, I>::type;

    template <class T, std::size_t I>
    using base_t = typename describe_detail::of_t<T>::template base<I>;

    // An aggregate has no virtual base.
    template <class T, std::size_t I>
    inline constexpr bool base_is_virtual_v = false;

    template <class E>
    consteval auto enum_range() {
        static_assert(std::is_enum_v<E>);
        static_assert(describe_detail::fixed_underlying_type<E>(),
            "XOffset: without reflection the enumerators of an enum without a fixed "
            "underlying type are unknown. Give it one: enum E : std::uint8_t { ... } "
            "(or enum class E { ... }), with the width the values need.");
        using U = std::underlying_type_t<E>;
        return enum_bounds<U>{};
    }

    namespace describe_detail {
        // Is every base and member of T trivial for Trait? Then T's implicit
        // special member would be trivial too, and a nontrivial one is
        // user-provided.
        template <template <class> class Trait, class T, std::size_t... I>
        consteval bool bases_trivial(std::index_sequence<I...>) {
            return (Trait<typename of_t<T>::template base<I>>::value && ...);
        }
        template <template <class> class Trait, class T, std::size_t... I>
        consteval bool members_trivial(std::index_sequence<I...>) {
            return (Trait<typename of_t<T>::template member<I>::type>::value && ...);
        }
        template <template <class> class Trait, class T>
        consteval bool user_provided() {
            return bases_trivial<Trait, T>(std::make_index_sequence<of_t<T>::base_count>{}) &&
                   members_trivial<Trait, T>(std::make_index_sequence<of_t<T>::member_count>{}) &&
                   !Trait<T>::value;
        }
    } // namespace describe_detail

    // Without reflection these are promises, checked where the traits can
    // tell. The library asks them only for a record declared ordinary
    // (XOFFSET_RECORD, XOFFSET_ORDINARY_RECORDS or a wire_record_layout
    // specialization), and that declaration promises a destructor that is not
    // user-provided and no user-declared special member function. An aggregate
    // has no user-declared constructor; a user-provided destructor or
    // assignment is caught when every base and member is trivial for it (T is
    // then not), and stays a promise when some member (an XString, say)
    // is not trivial itself.
    template <class T>
    consteval bool implicit_lifetime_aggregate() {
        if constexpr (!std::is_aggregate_v<T> || !is_described_v<T>) return false;
        else return !describe_detail::user_provided<std::is_trivially_destructible, T>();
    }

    template <class T>
    consteval bool only_implicit_special_members() {
        if constexpr (!implicit_lifetime_aggregate<T>()) return false;
        else return !describe_detail::user_provided<std::is_trivially_copy_assignable, T>() &&
                    !describe_detail::user_provided<std::is_trivially_move_assignable, T>();
    }
} // namespace XOffsetDatastructure::schema

#define XOFFSET_SCHEMA_DETAIL_REFLECTION_CHECK(macro, T)
#endif // !XOFFSET_SCHEMA_REFLECTION

#endif // X_OFFSET_SCHEMA_DESCRIBE_HPP
