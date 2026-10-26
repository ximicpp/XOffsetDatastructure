// Feasibility probe for the C++20 schema description layer (design only,
// not library code). One XO_DESCRIBE line per record type gives the library
// everything it now asks P2996 for: member count, member type, byte offset
// and member access, plus optional bases. A C++20 aggregate-arity check plus
// strictly ascending offsets prove the list is exactly the declared members.
#pragma once
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace xo_desc {

template <class> struct member_pointer_traits;
template <class C, class M> struct member_pointer_traits<M C::*> {
    using class_type = C;
    using member_type = M;
};

// One described field: member pointer + offsetof, both compile-time constants.
template <auto MemberPtr, std::size_t Offset>
struct field {
    using class_type = typename member_pointer_traits<decltype(MemberPtr)>::class_type;
    using type = typename member_pointer_traits<decltype(MemberPtr)>::member_type;
    static constexpr auto pointer = MemberPtr;
    static constexpr std::size_t offset = Offset;
    template <class Obj>
    static constexpr decltype(auto) get(Obj&& obj) noexcept { return (static_cast<Obj&&>(obj).*MemberPtr); }
};

template <class... Bases> struct bases {};

// Primary template: not described.
template <class T> struct describe { static constexpr bool enabled = false; };

template <class T, class Bases, class... Fields> struct record;
template <class T, class... B, class... Fields>
struct record<T, bases<B...>, Fields...> {
    static constexpr bool enabled = true;
    using type = T;
    using base_list = std::tuple<B...>;
    using field_list = std::tuple<Fields...>;
};

template <class T> concept described = describe<std::remove_cv_t<T>>::enabled;

template <class T> inline constexpr std::size_t member_count =
    std::tuple_size_v<typename describe<T>::field_list>;
template <class T> inline constexpr std::size_t base_count =
    std::tuple_size_v<typename describe<T>::base_list>;
template <class T, std::size_t I> using member_t = std::tuple_element_t<I, typename describe<T>::field_list>;
template <class T, std::size_t I> using base_t = std::tuple_element_t<I, typename describe<T>::base_list>;

namespace detail {
    // Converts to any non-array type; used only in unevaluated requires-expressions.
    struct any_init { template <class U> operator U() const noexcept; };

    // Initializer-clauses one member takes in T{...}: C arrays are filled by
    // brace elision (any_init cannot convert to an array), everything else takes one.
    template <class M> consteval std::size_t init_clauses() {
        if constexpr (std::is_array_v<M>) return std::extent_v<M> * init_clauses<std::remove_extent_t<M>>();
        else return 1;
    }

    template <class T, std::size_t... I>
    consteval bool brace_init_with(std::index_sequence<I...>) {
        return requires { T{((void)I, any_init{})...}; };
    }

    template <class T, class... F>
    consteval std::size_t expected_clauses(std::tuple<F...>*) {
        return base_count<T> + (init_clauses<typename F::type>() + ... + 0);
    }

    template <class T, class... F>
    consteval bool ascending(std::tuple<F...>*) {
        std::size_t offsets[] = {F::offset..., 0};
        std::size_t ends[] = {(F::offset + sizeof(typename F::type))..., 0};
        for (std::size_t i = 1; i < sizeof...(F); ++i)
            if (offsets[i] < ends[i - 1]) return false;   // overlap, duplicate or out of order
        return true;
    }

    template <class T> consteval bool offsets_ascending() {
        return ascending<T>(static_cast<typename describe<T>::field_list*>(nullptr));
    }

    // Exact coverage for aggregates: T{any x E} is valid and T{any x (E+1)} is not.
    template <class T> consteval bool covers_all_members() {
        if constexpr (!std::is_aggregate_v<T>) return false;
        else {
            constexpr std::size_t e = expected_clauses<T>(static_cast<typename describe<T>::field_list*>(nullptr));
            return brace_init_with<T>(std::make_index_sequence<e>{}) &&
                   !brace_init_with<T>(std::make_index_sequence<e + 1>{});
        }
    }
} // namespace detail

template <std::size_t Count, class Fn>
constexpr void static_for(Fn&& fn) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (fn(std::integral_constant<std::size_t, I>{}), ...);
    }(std::make_index_sequence<Count>{});
}

} // namespace xo_desc

// ---- macros -----------------------------------------------------------------
#define XO_DETAIL_PARENS ()
#define XO_DETAIL_EXPAND(...) XO_DETAIL_EXPAND4(XO_DETAIL_EXPAND4(XO_DETAIL_EXPAND4(XO_DETAIL_EXPAND4(__VA_ARGS__))))
#define XO_DETAIL_EXPAND4(...) XO_DETAIL_EXPAND3(XO_DETAIL_EXPAND3(XO_DETAIL_EXPAND3(XO_DETAIL_EXPAND3(__VA_ARGS__))))
#define XO_DETAIL_EXPAND3(...) XO_DETAIL_EXPAND2(XO_DETAIL_EXPAND2(XO_DETAIL_EXPAND2(XO_DETAIL_EXPAND2(__VA_ARGS__))))
#define XO_DETAIL_EXPAND2(...) XO_DETAIL_EXPAND1(XO_DETAIL_EXPAND1(XO_DETAIL_EXPAND1(XO_DETAIL_EXPAND1(__VA_ARGS__))))
#define XO_DETAIL_EXPAND1(...) __VA_ARGS__
#define XO_DETAIL_FOR_EACH_T(macro, T, ...) \
    __VA_OPT__(XO_DETAIL_EXPAND(XO_DETAIL_FOR_EACH_T_HELPER(macro, T, __VA_ARGS__)))
#define XO_DETAIL_FOR_EACH_T_HELPER(macro, T, a1, ...) \
    , macro(T, a1) __VA_OPT__(XO_DETAIL_FOR_EACH_T_AGAIN XO_DETAIL_PARENS (macro, T, __VA_ARGS__))
#define XO_DETAIL_FOR_EACH_T_AGAIN() XO_DETAIL_FOR_EACH_T_HELPER
#define XO_DETAIL_FIELD(T, m) ::xo_desc::field<&T::m, __builtin_offsetof(T, m)>
// offsetof on a non-standard-layout record (members in both base and derived)
// is conditionally-supported; GCC, Clang and MSVC all support it without
// virtual bases, which admission already rejects.
#if defined(__GNUC__) || defined(__clang__)
#define XO_DETAIL_OFFSETOF_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
#define XO_DETAIL_OFFSETOF_END _Pragma("GCC diagnostic pop")
#else
#define XO_DETAIL_OFFSETOF_BEGIN
#define XO_DETAIL_OFFSETOF_END
#endif

#define XO_DETAIL_CHECKS(T)                                                     \
    static_assert(::xo_desc::detail::offsets_ascending<T>(),                    \
        "XO_DESCRIBE(" #T "): list every field once, in declaration order");    \
    static_assert(::xo_desc::detail::covers_all_members<T>(),                   \
        "XO_DESCRIBE(" #T "): the field list does not name every member");

// XO_DESCRIBE(Type, field1, field2, ...) at global scope.
#define XO_DESCRIBE(T, ...)                                                     \
    template <> struct xo_desc::describe<T>                                     \
        : ::xo_desc::record<T, ::xo_desc::bases<> XO_DETAIL_FOR_EACH_T(XO_DETAIL_FIELD, T, __VA_ARGS__)> {}; \
    XO_DETAIL_CHECKS(T)

// XO_DESCRIBE_DERIVED(Type, (Base1, Base2), field1, ...): bases in declaration order.
#define XO_DETAIL_UNPAREN(...) __VA_ARGS__
#define XO_DESCRIBE_DERIVED(T, BASES, ...)                                      \
    XO_DETAIL_OFFSETOF_BEGIN                                                    \
    template <> struct xo_desc::describe<T>                                     \
        : ::xo_desc::record<T, ::xo_desc::bases<XO_DETAIL_UNPAREN BASES> XO_DETAIL_FOR_EACH_T(XO_DETAIL_FIELD, T, __VA_ARGS__)> {}; \
    XO_DETAIL_OFFSETOF_END                                                      \
    XO_DETAIL_CHECKS(T)
