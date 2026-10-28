// Compile only. The catalog supplies roots and independently reviewed wire pins.
#if defined(XSG_LINUX_X64)
# if !defined(__linux__) || defined(__ANDROID__) || !defined(__x86_64__)
#  error "schema guard target mismatch: expected Linux x86_64"
# endif
#elif defined(XSG_LINUX_ARM64)
# if !defined(__linux__) || defined(__ANDROID__) || !defined(__aarch64__)
#  error "schema guard target mismatch: expected Linux arm64"
# endif
#elif defined(XSG_MAC_ARM64)
# if !defined(__APPLE__) || !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) || !defined(__aarch64__)
#  error "schema guard target mismatch: expected macOS arm64"
# endif
#elif defined(XSG_MAC_X64)
# if !defined(__APPLE__) || !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) || !defined(__x86_64__)
#  error "schema guard target mismatch: expected macOS x86_64"
# endif
#elif defined(XSG_IOS_ARM64)
# if !defined(__APPLE__) || !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) || !defined(__aarch64__)
#  error "schema guard target mismatch: expected iOS arm64"
# endif
# if defined(XSG_SIMULATOR) && !defined(__APPLE_EMBEDDED_SIMULATOR__)
#  error "schema guard target mismatch: expected iOS simulator"
# endif
# if !defined(XSG_SIMULATOR) && defined(__APPLE_EMBEDDED_SIMULATOR__)
#  error "schema guard target mismatch: expected iOS device"
# endif
#elif defined(XSG_IOS_SIM_X64)
# if !defined(__APPLE__) || !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) || !defined(__APPLE_EMBEDDED_SIMULATOR__) || !defined(__x86_64__)
#  error "schema guard target mismatch: expected iOS simulator x86_64"
# endif
#elif defined(XSG_ANDROID_X64)
# if !defined(__ANDROID__) || !defined(__x86_64__)
#  error "schema guard target mismatch: expected Android x86_64"
# endif
#elif defined(XSG_ANDROID_ARM64)
# if !defined(__ANDROID__) || !defined(__aarch64__)
#  error "schema guard target mismatch: expected Android arm64"
# endif
#elif defined(XSG_WINDOWS_X64)
# if !defined(_WIN32) || !defined(__x86_64__)
#  error "schema guard target mismatch: expected Windows x86_64"
# endif
#elif defined(XSG_WINDOWS_ARM64)
# if !defined(_WIN32) || !(defined(__aarch64__) || defined(_M_ARM64))
#  error "schema guard target mismatch: expected Windows arm64"
# endif
#else
# error "schema guard: target identity must be selected"
#endif
#include "xoffsetdatastructure.hpp"
// The core headers no longer include TypeLayout; the probe checks its pins.
#include <boost/typelayout.hpp>
#include XSG_CATALOG
#ifdef XOFFSET_DISABLE_PLATFORM_CHECKS
# error "schema guard refuses disabled platform checks"
#endif
#ifndef XSG_SCHEMA_ROOTS
# error "catalog must define XSG_SCHEMA_ROOTS(X)"
#endif
#define XSG_COUNT(T, PIN, TYPE_PIN) + 1
constexpr int xsg_root_count = 0 XSG_SCHEMA_ROOTS(XSG_COUNT);
#undef XSG_COUNT
static_assert(xsg_root_count > 0, "schema guard catalog must not be empty");
template <typename T>
consteval bool xsg_typelayout_match(std::string_view expected) {
    constexpr auto sig = ::boost::typelayout::get_layout_signature<T>();
    return std::string_view(sig.value, sig.size) == expected;
}
#define XSG_CHECK(T, PIN, TYPE_PIN) \
    static_assert(::XOffsetDatastructure::is_v1_wire_admitted_v<T>, "schema not wire admitted"); \
    static_assert(::boost::typelayout::is_byte_copy_safe_v<T>, "TypeLayout byte-copy safety failed"); \
    static_assert(::XOffsetDatastructure::has_schema_name_v<T>, "root schema name missing"); \
    XOFFSET_PIN_SCHEMA(T, PIN); \
    static_assert(xsg_typelayout_match<T>(TYPE_PIN), "TypeLayout signature mismatch");
XSG_SCHEMA_ROOTS(XSG_CHECK)
#undef XSG_CHECK

#ifndef XSG_RECORD_FIELDS
# error "catalog must define XSG_RECORD_FIELDS(X)"
#endif
template <class T, std::size_t... I>
consteval auto xsg_names(std::index_sequence<I...>) {
    using namespace std::meta;
    return std::array<std::string_view, sizeof...(I)>{
        identifier_of(nonstatic_data_members_of(^^T, access_context::unchecked())[I])...
    };
}
template <class T>
consteval bool xsg_fields_match(std::string_view expected) {
    constexpr auto names = xsg_names<T>(
        std::make_index_sequence<::XOffsetDatastructure::schema::member_count_v<T>>{});
    std::size_t pos = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto end = expected.find('|', pos);
        const auto part = expected.substr(pos, end == std::string_view::npos ? end : end - pos);
        if (part != names[i]) return false;
        if (i + 1 < names.size()) {
            if (end == std::string_view::npos) return false;
            pos = end + 1;
        } else if (end != std::string_view::npos) return false;
    }
    return true;
}
#define XSG_SEMANTIC_COUNT(T, NAMES) + 1
constexpr int xsg_semantic_count = 0 XSG_RECORD_FIELDS(XSG_SEMANTIC_COUNT);
#undef XSG_SEMANTIC_COUNT
static_assert(xsg_semantic_count > 0, "schema guard semantic catalog must not be empty");
#define XSG_CHECK_FIELDS(T, NAMES) static_assert(xsg_fields_match<T>(NAMES), "schema field names/order mismatch");
XSG_RECORD_FIELDS(XSG_CHECK_FIELDS)
#undef XSG_CHECK_FIELDS

// Traverse container element/key/value types, arrays and every reflected record.
template<class T> struct xsg_fields_covered : std::false_type {};
#define XSG_DECLARE_FIELDS(T, NAMES) template<> struct xsg_fields_covered<T> : std::true_type {};
XSG_RECORD_FIELDS(XSG_DECLARE_FIELDS)
#undef XSG_DECLARE_FIELDS
template<class T> consteval bool xsg_complete_fields();
template<class T, std::size_t I> consteval bool xsg_one_member_fields() {
    using namespace std::meta;
    constexpr auto member=nonstatic_data_members_of(^^T, access_context::unchecked())[I];
    using M=[:type_of(member):];
    return xsg_complete_fields<M>();
}
template<class T, std::size_t... I>
consteval bool xsg_member_fields(std::index_sequence<I...>) {
    return (xsg_one_member_fields<T,I>() && ...);
}
template<class T> consteval bool xsg_complete_fields() {
    using U=std::remove_cv_t<T>;
    namespace d=::XOffsetDatastructure::detail;
    if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U> || std::is_same_v<U,std::byte> || d::is_fixed_string_v<U>) return true;
    else if constexpr (std::is_array_v<U>) return xsg_complete_fields<std::remove_extent_t<U>>();
    else if constexpr (d::is_fixed_flat_map_v<U>) return xsg_complete_fields<typename U::key_type>() && xsg_complete_fields<typename U::mapped_type>();
    else if constexpr (d::is_fixed_flat_set_v<U> || d::is_fixed_vector_v<U>) return xsg_complete_fields<typename U::value_type>();
    else if constexpr (requires { typename U::value_type; std::tuple_size<U>::value; }) return xsg_complete_fields<typename U::value_type>();
    else {
        static_assert(xsg_fields_covered<U>::value, "schema reachable record field list missing");
        return xsg_member_fields<U>(std::make_index_sequence<::XOffsetDatastructure::schema::member_count_v<U>>{});
    }
}
#define XSG_CHECK_ALL_FIELDS(T, PIN, TYPE_PIN) static_assert(xsg_complete_fields<T>());
XSG_SCHEMA_ROOTS(XSG_CHECK_ALL_FIELDS)
#undef XSG_CHECK_ALL_FIELDS
