#ifndef X_OFFSET_EVOLUTION_HPP
#define X_OFFSET_EVOLUTION_HPP

// Schema evolution across generations (server side or an offline tool): a
// change that only adds fields from XSpare bytes keeps the fingerprint and
// needs nothing here (xoffset_schema_family.hpp); any other change starts a new
// generation. Peers of different generations do not talk (the handshake
// refuses them); stored data of an older generation is upgraded once when it
// is loaded, with evolve_load_verified and XOFFSET_SCHEMA_LINEAGE below.
//
// The conversion converts a verified object of one version of a schema into
// another, built in a library buffer. Fields correspond by name, level by
// level; XSpare bytes are never copied. An adjacent step's hook is a global
// specialization of xevolve<Old, New> (upgrade, optionally downgrade, and
// `handles`, the fields the default copy leaves to it). Rules and examples:
// docs/SCHEMA_EVOLUTION.md. Every failure throws XException naming the field
// path, with no partial result. The source is read through typed const
// references only: run it on objects that passed load_verified.

#include "xoffsetdatastructure.hpp"
#include "xoffset_schema_names.hpp"
#include "xoffset_handshake.hpp"

#if !XOFFSET_SCHEMA_REFLECTION
#error "xoffset_evolution.hpp is server-side and needs the reflection backend (C++26 reflection; clients only need xoffset_handshake.hpp)"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

namespace XOffsetDatastructure {

    // Primary template: no hook. Specializations replace it entirely.
    template <typename Old, typename New>
    struct xevolve {
        static constexpr bool xoffset_no_hook = true;
    };

    class XEvolveContext;

    namespace detail::evolve {
        template <typename From, typename To>
        void convert(const From& from, To& to, XEvolveContext& ctx);

        // One address per type, for the typed base object of a hook.
        template <typename T>
        const void* type_tag() noexcept {
            static const char unique = 0;
            return &unique;
        }
    }

    class XEvolveContext {
    public:
        XEvolveContext() = default;

        // The target object of the same type the hook may merge from (for
        // example the server's own state when a client sends an object back
        // without the fields it does not know); nullptr when none was given.
        template <typename T>
        const T* base() const noexcept {
            return base_tag_ == detail::evolve::type_tag<T>() ? static_cast<const T*>(base_) : nullptr;
        }

        template <typename T>
        void set_base(const T* object) noexcept {
            base_ = object;
            base_tag_ = object ? detail::evolve::type_tag<T>() : nullptr;
        }

        void set_base(const void* object, const void* tag) noexcept {
            base_ = object;
            base_tag_ = object ? tag : nullptr;
        }

        // Fails this conversion: throws XException with the field path (and
        // `field`, the field a hook was converting, when given).
        [[noreturn]] void fail(std::string_view why, std::string_view field = {}) const {
            std::string message = "schema evolution: ";
            if (field.empty()) {
                message += path();
            } else {
                if (depth_ != 0) {
                    message += path();
                    message += '.';
                }
                message += field;
            }
            message += ": ";
            message += why;
            throw XException(message);
        }

        // Integer narrowing that fails the conversion when the value does not
        // fit; `field` names it in the message.
        template <typename To, typename From>
        To checked_narrow(From value, std::string_view field = {}) const {
            static_assert(std::is_integral_v<To> && std::is_integral_v<From> &&
                          !std::is_same_v<To, bool> && !std::is_same_v<From, bool>,
                "checked_narrow<To>(value) converts between integer types");
            if (!std::in_range<To>(value)) fail("value out of range of the target type", field);
            return static_cast<To>(value);
        }

        // Lossless widening; a conversion that could lose a value does not compile.
        template <typename To, typename From>
        static constexpr To widen(From value) noexcept {
            static_assert(std::is_arithmetic_v<To> && std::is_arithmetic_v<From>,
                "widen<To>(value) converts between arithmetic types");
            if constexpr (std::is_integral_v<From> && std::is_integral_v<To>) {
                static_assert(std::in_range<To>(std::numeric_limits<From>::min()) &&
                              std::in_range<To>(std::numeric_limits<From>::max()),
                    "widen<To>(value): To cannot hold every value of From; use checked_narrow");
            } else {
                static_assert(std::is_floating_point_v<To> &&
                              std::numeric_limits<To>::digits >= std::numeric_limits<From>::digits &&
                              std::numeric_limits<To>::max_exponent >= std::numeric_limits<From>::max_exponent,
                    "widen<To>(value): To cannot hold every value of From exactly");
            }
            return static_cast<To>(value);
        }

        // Field path of the value being converted, e.g. "bag.count".
        std::string path() const {
            if (depth_ == 0) return "<root>";
            std::string out;
            const std::size_t shown = depth_ < max_depth ? depth_ : max_depth;
            for (std::size_t i = 0; i < shown; ++i) {
                if (i) out += '.';
                out += path_[i];
            }
            if (depth_ > max_depth) out += "...";
            return out;
        }

        std::size_t depth() const noexcept { return depth_; }

    public:
        // Used by the conversion walk; not part of the hook interface.
        void push_(std::string_view name) noexcept {
            if (depth_ < max_depth) path_[depth_] = name;
            ++depth_;
        }
        void pop_() noexcept { --depth_; }

    private:
        static constexpr std::size_t max_depth = 32;
        std::string_view path_[max_depth]{};
        std::size_t depth_ = 0;
        const void* base_ = nullptr;
        const void* base_tag_ = nullptr;
    };

    namespace detail::evolve {
        enum class Kind { scalar, enumeration, array, string, vector, set, map, record, spare, other };

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
            else if constexpr (std::is_class_v<U> && !std::is_union_v<U>) return Kind::record;
            else return Kind::other;
        }

        inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

        // ---- record fields (schema field interface) ----------------------------

        template <typename T>
        consteval std::size_t field_count() { return schema::member_count_v<T>; }

        template <typename T, std::size_t I>
        using field_t = schema::member_type_t<T, I>;

        template <typename T, std::size_t I>
        inline constexpr std::string_view field_name_v = schema::member_t<T, I>::name;

        template <typename T, std::size_t I>
        consteval std::string_view field_name() { return field_name_v<T, I>; }

        template <std::size_t I, typename T>
        constexpr decltype(auto) get(T& object) noexcept {
            return schema::member_t<std::remove_const_t<T>, I>::get(object);
        }

        template <typename T>
        consteval std::size_t find_field(std::string_view name) {
            std::size_t found = npos;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((found == npos && field_name<T, Is>() == name ? (void)(found = Is) : (void)0), ...);
            }(std::make_index_sequence<field_count<T>()>{});
            return found;
        }

        // "v1::Player" for a named class or enum, the compiler's spelling otherwise.
        template <typename T>
        consteval std::string_view type_name() { return schema::qualified_name_v<T>; }

        // ---- hooks -----------------------------------------------------------

        template <typename Old, typename New>
        concept has_hook = !requires { xevolve<Old, New>::xoffset_no_hook; };

        template <typename H>
        consteval std::size_t handles_count() {
            if constexpr (requires { std::size(H::handles); }) return std::size(H::handles);
            else return 0;
        }

        template <typename H>
        consteval bool handles_name(std::string_view name) {
            if constexpr (handles_count<H>() != 0) {
                for (std::string_view h : H::handles)
                    if (h == name) return true;
            }
            return false;
        }

        template <typename H>
        consteval std::string_view handle_at(std::size_t i) {
            if constexpr (handles_count<H>() != 0) {
                if (i < handles_count<H>()) return H::handles[i];
            }
            return {};
        }

        // First entry of H::handles that neither Old nor New has, or npos.
        template <typename H, typename Old, typename New>
        consteval std::size_t unknown_handle() {
            if constexpr (handles_count<H>() != 0) {
                for (std::size_t i = 0; i < handles_count<H>(); ++i)
                    if (find_field<Old>(H::handles[i]) == npos && find_field<New>(H::handles[i]) == npos)
                        return i;
            }
            return npos;
        }

        // ---- compile-time messages -------------------------------------------

        template <std::size_t N>
        using Message = SchemaPinMessage<N>;

        inline constexpr std::string_view msg_head = "XOffset schema evolution: field '";

        template <typename From, typename To, std::size_t J, std::size_t I>
        consteval auto type_change_message() {
            constexpr std::string_view name = field_name<To, I>();
            constexpr std::string_view a = type_name<field_t<From, J>>();
            constexpr std::string_view b = type_name<field_t<To, I>>();
            constexpr std::string_view mid = "' changed type from ";
            constexpr std::string_view to = " to ";
            constexpr std::string_view in = " (";
            constexpr std::string_view arrow = " -> ";
            constexpr std::string_view tail =
                "); name it in xevolve<Old, New>::handles and convert it in the hook";
            constexpr std::string_view from_t = type_name<From>();
            constexpr std::string_view to_t = type_name<To>();
            Message<msg_head.size() + name.size() + mid.size() + a.size() + to.size() + b.size() +
                    in.size() + from_t.size() + arrow.size() + to_t.size() + tail.size()> m;
            m.append(msg_head); m.append(name); m.append(mid); m.append(a); m.append(to); m.append(b);
            m.append(in); m.append(from_t); m.append(arrow); m.append(to_t); m.append(tail);
            return m;
        }

        template <typename H, typename Old, typename New>
        consteval auto unknown_handle_message() {
            constexpr std::size_t i = unknown_handle<H, Old, New>();
            constexpr std::string_view head = "XOffset schema evolution: xevolve<";
            constexpr std::string_view old_t = type_name<Old>();
            constexpr std::string_view comma = ", ";
            constexpr std::string_view new_t = type_name<New>();
            constexpr std::string_view mid = ">::handles names '";
            constexpr std::string_view name = handle_at<H>(i);
            constexpr std::string_view tail = "', which neither version has";
            Message<head.size() + old_t.size() + comma.size() + new_t.size() + mid.size() + name.size() +
                    tail.size()> m;
            m.append(head); m.append(old_t); m.append(comma); m.append(new_t); m.append(mid);
            m.append(name); m.append(tail);
            return m;
        }

        // ---- what converts without a hook --------------------------------------

        template <typename From, typename To>
        consteval bool key_convertible() {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            return std::is_same_v<F, T> || (std::is_enum_v<F> && std::is_enum_v<T>);
        }

        template <typename T>
        consteval bool has_enum();

        // Whether a value of type From converts into To by the default rules.
        // Records always do at this level: their fields are checked when the
        // record conversion is instantiated, so the error names the field.
        // XSpare bytes on either side are never copied, so they always do.
        template <typename From, typename To>
        consteval bool convertible() {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            constexpr Kind kf = kind_of<F>();
            constexpr Kind kt = kind_of<T>();
            if constexpr (std::is_same_v<F, T> || kf == Kind::spare || kt == Kind::spare) {
                return true;
            } else if constexpr (kf != kt) {
                return false;
            } else if constexpr (kf == Kind::scalar || kf == Kind::other) {
                return false;
            } else if constexpr (kf == Kind::enumeration || kf == Kind::record || kf == Kind::string) {
                return true;
            } else if constexpr (kf == Kind::array) {
                return std::extent_v<F> == std::extent_v<T> &&
                       convertible<std::remove_extent_t<F>, std::remove_extent_t<T>>();
            } else if constexpr (kf == Kind::vector) {
                return std::is_same_v<typename F::value_type, std::byte> ==
                           std::is_same_v<typename T::value_type, std::byte> &&
                       convertible<typename F::value_type, typename T::value_type>();
            } else if constexpr (kf == Kind::set) {
                return key_convertible<typename F::value_type, typename T::value_type>();
            } else {
                return key_convertible<typename F::key_type, typename T::key_type>() &&
                       convertible<typename F::mapped_type, typename T::mapped_type>();
            }
        }

        // Same bytes, same names, nothing to rebind: one memcpy converts. A
        // record that did not change between the versions is copied whole,
        // XSpare members included; XSpare bytes on their own are not.
        template <typename From, typename To>
        consteval bool block_copy() {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            if constexpr (!std::is_trivially_copyable_v<F> || !std::is_trivially_copyable_v<T> ||
                          sizeof(F) != sizeof(T) || kind_of<F>() == Kind::spare || kind_of<T>() == Kind::spare) {
                return false;
            } else if constexpr (kind_of<F>() != Kind::record && kind_of<F>() != Kind::array) {
                return std::is_same_v<F, T>;
            } else if constexpr (has_enum<F>() || has_enum<T>()) {
                return std::is_same_v<F, T> && !has_implicit_padding_v<T>;
            } else {
                return !has_implicit_padding_v<T> &&
                       std::string_view(wire_abi_signature<F>()) == std::string_view(wire_abi_signature<T>()) &&
                       schema_names::same_names<F, T>();
            }
        }

        // Whether an enum is reachable inside a trivially copyable value
        // (records and arrays only; containers are not trivially copyable).
        template <typename T>
        consteval bool has_enum() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_enum_v<U>) return true;
            else if constexpr (std::is_array_v<U>) return has_enum<std::remove_extent_t<U>>();
            else if constexpr (kind_of<U>() == Kind::record) {
                return []<std::size_t... Is>(std::index_sequence<Is...>) {
                    return (false || ... || has_enum<field_t<U, Is>>());
                }(std::make_index_sequence<field_count<U>()>{});
            } else return false;
        }

        // ---- enums -------------------------------------------------------------

        template <typename E>
        inline constexpr auto enumerator_values = []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<std::underlying_type_t<E>, sizeof...(Is)>{
                static_cast<std::underlying_type_t<E>>(schema::enumerator_t<E, Is>::value)...};
        }(std::make_index_sequence<schema::enumerator_count_v<E>>{});

        template <typename To, typename From>
        To convert_enum(From value, XEvolveContext& ctx) {
            using UF = std::underlying_type_t<From>;
            using UT = std::underlying_type_t<To>;
            const UF raw = static_cast<UF>(value);
            if constexpr (enumerator_values<To>.size() == 0) {
                // An enum without enumerators is a typed integer: every value of
                // the underlying type is valid.
                if (!std::in_range<UT>(raw)) ctx.fail("enum value out of range of the target enum");
                return static_cast<To>(static_cast<UT>(raw));
            } else {
                for (const UT v : enumerator_values<To>)
                    if (std::cmp_equal(v, raw)) return static_cast<To>(v);
                ctx.fail("enum value is not an enumerator of the target enum");
            }
        }

        // ---- containers -----------------------------------------------------

        template <typename T>
        struct scratch {
            alignas(T) unsigned char bytes[sizeof(T)];
            bool built = false;
            T& get() noexcept { return *std::launder(reinterpret_cast<T*>(bytes)); }
            ~scratch() { if (built) std::destroy_at(&get()); }
        };

        // Enum keys only; keys of the same type are inserted as they are.
        template <typename ToKey, typename FromKey>
        ToKey convert_key(const FromKey& key, XEvolveContext& ctx) {
            return convert_enum<ToKey>(key, ctx);
        }

        template <typename From, typename To>
        void convert_vector(const From& from, To& to, XEvolveContext& ctx) {
            using FE = typename From::value_type;
            using TE = typename To::value_type;
            to.reserve(from.size());
            if constexpr (block_copy<FE, TE>()) {
                for (const FE& e : from) {
                    TE& slot = to.emplace_back();
                    std::memcpy(static_cast<void*>(&slot), static_cast<const void*>(&e), sizeof(TE));
                }
            } else {
                for (const FE& e : from) convert(e, to.emplace_back(), ctx);
            }
        }

        template <typename From, typename To>
        void convert_set(const From& from, To& to, XEvolveContext& ctx) {
            using TK = typename To::value_type;
            to.reserve(from.size());
            for (const auto& key : from) {
                if constexpr (std::is_same_v<TK, typename From::value_type>) {
                    to.insert(key);
                } else if (!to.insert(convert_key<TK>(key, ctx)).second) {
                    ctx.fail("duplicate set key after conversion");
                }
            }
        }

        template <typename From, typename To>
        void convert_map(const From& from, To& to, XEvolveContext& ctx) {
            using TK = typename To::key_type;
            to.reserve(from.size());
            for (const auto& entry : from) {
                // A key of the same type goes in by reference: a copy of a
                // string key outside every payload could not hold a long string.
                auto [it, inserted] = [&] {
                    if constexpr (std::is_same_v<TK, typename From::key_type>) return to.try_emplace(entry.first);
                    else return to.try_emplace(convert_key<TK>(entry.first, ctx));
                }();
                if (!inserted) ctx.fail("duplicate map key after conversion");
                convert(entry.second, it->second, ctx);
            }
        }

        // ---- records --------------------------------------------------------

        template <typename From, typename To>
        struct hook_for {
            static constexpr bool up = !std::is_same_v<From, To> && has_hook<From, To>;
            static constexpr bool down = !std::is_same_v<From, To> && has_hook<To, From>;
            static_assert(!(up && down),
                "XOffset schema evolution: both xevolve<A, B> and xevolve<B, A> are specialized; "
                "write one xevolve<Old, New> with upgrade and downgrade");
            using H = std::conditional_t<up, xevolve<From, To>, xevolve<To, From>>;
            static constexpr bool present = up || down;

            static consteval bool handles(std::string_view name) {
                if constexpr (present) return handles_name<H>(name);
                else return false;
            }

            static void call(const From& from, To& to, XEvolveContext& ctx) {
                if constexpr (up) {
                    static_assert(requires { H::upgrade(from, to, ctx); },
                        "XOffset schema evolution: xevolve<Old, New> needs "
                        "static void upgrade(const Old&, New&, XEvolveContext&)");
                    H::upgrade(from, to, ctx);
                } else if constexpr (down) {
                    static_assert(requires { H::downgrade(from, to, ctx); },
                        "XOffset schema evolution: xevolve<Old, New> needs "
                        "static void downgrade(const New&, Old&, XEvolveContext&)");
                    H::downgrade(from, to, ctx);
                }
            }
        };

        template <typename From, typename To>
        consteval bool check_hook_names() {
            using Hook = hook_for<From, To>;
            if constexpr (Hook::up) return unknown_handle<typename Hook::H, From, To>() == npos;
            else if constexpr (Hook::down) return unknown_handle<typename Hook::H, To, From>() == npos;
            else return true;
        }

        template <typename From, typename To>
        consteval auto hook_names_message() {
            using Hook = hook_for<From, To>;
            if constexpr (Hook::up) return unknown_handle_message<typename Hook::H, From, To>();
            else if constexpr (Hook::down) return unknown_handle_message<typename Hook::H, To, From>();
            else return Message<1>{};
        }

        template <typename From, typename To, std::size_t I>
        void convert_field(const From& from, To& to, XEvolveContext& ctx) {
            using Hook = hook_for<From, To>;
            constexpr std::string_view name = field_name_v<To, I>;
            if constexpr (!Hook::handles(name)) {
                constexpr std::size_t J = find_field<From>(name);
                if constexpr (J != npos) {
                    static_assert(convertible<field_t<From, J>, field_t<To, I>>(),
                                  type_change_message<From, To, J, I>());
                    ctx.push_(name);
                    convert(get<J>(from), get<I>(to), ctx);
                    ctx.pop_();
                }
                // else: added field; keeps its initial value.
            }
        }

        template <typename From, typename To>
        void convert_record(const From& from, To& to, XEvolveContext& ctx) {
            static_assert(schema::base_count_v<From> == 0 && schema::base_count_v<To> == 0,
                "XOffset schema evolution: wire records have no base classes");
            static_assert(check_hook_names<From, To>(), hook_names_message<From, To>());
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (convert_field<From, To, Is>(from, to, ctx), ...);
            }(std::make_index_sequence<field_count<To>()>{});
            hook_for<From, To>::call(from, to, ctx);
        }

        template <typename From, typename To>
        void convert(const From& from, To& to, XEvolveContext& ctx) {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            constexpr Kind kind = kind_of<T>();
            if constexpr (kind == Kind::spare || kind_of<F>() == Kind::spare) {
                // Reserved bytes carry nothing: the target keeps the zeros
                // make<To>() wrote, so a later version of To finds its new
                // fields there unset.
            } else if constexpr (block_copy<F, T>() && !hook_for<F, T>::present) {
                std::memcpy(static_cast<void*>(&to), static_cast<const void*>(&from), sizeof(T));
            } else if constexpr (kind == Kind::scalar) {
                to = from;
            } else if constexpr (kind == Kind::enumeration) {
                if constexpr (std::is_same_v<F, T>) to = from;
                else to = convert_enum<T>(from, ctx);
            } else if constexpr (kind == Kind::array) {
                for (std::size_t i = 0; i < std::extent_v<T>; ++i) convert(from[i], to[i], ctx);
            } else if constexpr (kind == Kind::string) {
                to.assign(from.view());
            } else if constexpr (kind == Kind::vector) {
                convert_vector(from, to, ctx);
            } else if constexpr (kind == Kind::set) {
                convert_set(from, to, ctx);
            } else if constexpr (kind == Kind::map) {
                convert_map(from, to, ctx);
            } else {
                convert_record(from, to, ctx);
            }
        }

        // ---- plan -------------------------------------------------------------

        template <typename From, typename To>
        void describe(std::string& out, std::string_view indent);

        template <typename From, typename To>
        consteval bool needs_walk() {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            if constexpr (block_copy<F, T>() && !hook_for<F, T>::present) return false;
            else if constexpr (kind_of<T>() == Kind::record) return true;
            else if constexpr (kind_of<T>() == Kind::spare || kind_of<F>() == Kind::spare) return true;
            else if constexpr (kind_of<T>() == Kind::array)
                return needs_walk<std::remove_extent_t<F>, std::remove_extent_t<T>>();
            else if constexpr (kind_of<T>() == Kind::vector)
                return needs_walk<typename F::value_type, typename T::value_type>();
            else if constexpr (kind_of<T>() == Kind::map)
                return needs_walk<typename F::mapped_type, typename T::mapped_type>();
            else return false;
        }

        template <typename From, typename To>
        void describe_value(std::string& out, std::string_view indent) {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            if constexpr (kind_of<T>() == Kind::record && !(block_copy<F, T>() && !hook_for<F, T>::present)) {
                describe<F, T>(out, indent);
            } else if constexpr (kind_of<T>() == Kind::array) {
                describe_value<std::remove_extent_t<F>, std::remove_extent_t<T>>(out, indent);
            } else if constexpr (kind_of<T>() == Kind::vector) {
                describe_value<typename F::value_type, typename T::value_type>(out, indent);
            } else if constexpr (kind_of<T>() == Kind::map) {
                describe_value<typename F::mapped_type, typename T::mapped_type>(out, indent);
            }
        }

        template <typename From, typename To>
        std::string_view action() {
            using F = std::remove_cv_t<From>;
            using T = std::remove_cv_t<To>;
            if constexpr (block_copy<F, T>() && !hook_for<F, T>::present) return "copy";
            else if constexpr (kind_of<T>() == Kind::enumeration && !std::is_same_v<F, T>) return "enum (checked)";
            else if constexpr (kind_of<T>() == Kind::set) {
                if constexpr (!std::is_same_v<typename F::value_type, typename T::value_type>) return "convert (keys checked)";
                else return "copy";
            } else if constexpr (kind_of<T>() == Kind::map) {
                if constexpr (!std::is_same_v<typename F::key_type, typename T::key_type>) return "convert (keys checked)";
                else if constexpr (needs_walk<F, T>()) return "convert";
                else return "copy";
            } else if constexpr (needs_walk<F, T>()) return "convert";
            else return "copy";
        }

        template <typename From, typename To>
        void describe(std::string& out, std::string_view indent) {
            using Hook = hook_for<From, To>;
            const std::string inner = std::string(indent) + "  ";
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                auto one = [&]<std::size_t I>() {
                    constexpr std::string_view name = field_name_v<To, I>;
                    out += indent;
                    out += name;
                    out += ": ";
                    if constexpr (Hook::handles(name)) {
                        out += "hook\n";
                    } else if constexpr (kind_of<field_t<To, I>>() == Kind::spare) {
                        out += "spare (not copied)\n";
                    } else {
                        constexpr std::size_t J = find_field<From>(name);
                        if constexpr (J == npos) {
                            out += "init (added)\n";
                        } else if constexpr (kind_of<field_t<From, J>>() == Kind::spare) {
                            out += "init (added)\n";
                        } else {
                            using FF = field_t<From, J>;
                            using TF = field_t<To, I>;
                            out += action<FF, TF>();
                            out += '\n';
                            describe_value<FF, TF>(out, inner);
                        }
                    }
                };
                (one.template operator()<Is>(), ...);
            }(std::make_index_sequence<field_count<To>()>{});
            [&]<std::size_t... Js>(std::index_sequence<Js...>) {
                auto gone = [&]<std::size_t J>() {
                    constexpr std::string_view name = field_name_v<From, J>;
                    if constexpr (find_field<To>(name) == npos) {
                        out += indent;
                        out += name;
                        out += Hook::handles(name) ? ": hook (removed)\n" : ": drop (removed)\n";
                    }
                };
                (gone.template operator()<Js>(), ...);
            }(std::make_index_sequence<field_count<From>()>{});
            if constexpr (Hook::present) {
                out += indent;
                out += Hook::up ? "then xevolve::upgrade\n" : "then xevolve::downgrade\n";
            }
        }
    } // namespace detail::evolve

    // Converts `from` into `to`, an object of the target version that lives in
    // a buffer of the library (made by make<To>(), so every field holds its
    // initial value). Throws XException on failure; `to` is then partly
    // written and must be discarded with its buffer.
    template <typename From, typename To>
    void evolve_into(const From& from, To& to, XEvolveContext& ctx) {
        static_assert(detail::evolve::kind_of<From>() == detail::evolve::Kind::record &&
                      detail::evolve::kind_of<To>() == detail::evolve::Kind::record,
            "XOffset schema evolution converts one record version into another");
        detail::evolve::convert(from, to, ctx);
    }

    template <typename From, typename To>
    void evolve_into(const From& from, To& to) {
        XEvolveContext ctx;
        evolve_into(from, to, ctx);
    }

    struct XEvolveOptions {
        // Bytes the target buffer starts with (0: twice the source's used bytes,
        // at least 4096); it grows automatically up to maximum_capacity.
        std::size_t initial_bytes = 0;
        // Upper bound of the target buffer; the conversion fails beyond it
        // (an upgrade can make a message larger). 0: the source buffer's
        // maximum capacity.
        std::size_t maximum_capacity = 0;
        // Optional object of the target version, handed to the hooks of the
        // last step as ctx.base<To>() (set it with with_base).
        const void* base = nullptr;
        const void* base_tag = nullptr;

        template <typename T>
        XEvolveOptions& with_base(const T* object) noexcept {
            base = object;
            base_tag = object ? detail::evolve::type_tag<T>() : nullptr;
            return *this;
        }
    };

    // Converts the root of a verified buffer into a new buffer of version To.
    template <typename To, typename From>
    TypedXBuffer<To> evolve(TypedXBuffer<From>& source, XEvolveOptions options = {}) {
        const From& from = source.plain_root();
        const std::size_t cap = options.maximum_capacity ? options.maximum_capacity : source.maximum_capacity();
        std::size_t initial = options.initial_bytes;
        if (initial == 0) {
            const std::size_t used = memory_stats(source).used_size;
            initial = used > (std::numeric_limits<std::size_t>::max)() / 2 ? used : used * 2;
            if (initial < 4096) initial = 4096;
        }
        if (initial > cap) initial = cap;
        TypedXBuffer<To> target(initial, XBuffer::maximum_capacity(cap).with_growth(XGrowPolicy::Automatic));
        To& to = *target.make();
        XEvolveContext ctx;
        if (options.base_tag == detail::evolve::type_tag<To>()) ctx.set_base(options.base, options.base_tag);
        try {
            evolve_into(from, to, ctx);
        } catch (const XBadAlloc&) {
            ctx.fail("target buffer exceeds its maximum capacity");
        }
        return target;
    }

    // What one conversion From -> To does, field by field: copy, convert,
    // enum (checked), init (added), drop (removed), hook. Commit it with the
    // frozen version so a review shows which values a step loses.
    template <typename From, typename To>
    std::string evolve_plan() {
        std::string out;
        out += detail::evolve::type_name<From>();
        out += " -> ";
        out += detail::evolve::type_name<To>();
        out += '\n';
        detail::evolve::describe<From, To>(out, "  ");
        return out;
    }


    // ========================================================================
    // Lineage: stored data of older generations.
    // ========================================================================
    //
    // XOFFSET_SCHEMA_LINEAGE(Working, G1, G2, ...) lists the generations of one
    // root type whose stored data this program still loads, oldest first; each
    // entry is the last version of that generation's add-only family (it names
    // every field the generation ever had). Working, the version this program
    // works in, is last unless it is listed. Every entry registers the same
    // schema name and their fingerprints differ pairwise, checked at compile
    // time: two versions of one fingerprint are one generation (one family,
    // or the same layout under other field names). Adjacent generations
    // convert with evolve<>() and their xevolve hooks; generations k apart
    // take k steps. Peers of an older generation are not served: the
    // handshake (xoffset_handshake.hpp) refuses them.

    template <typename Working>
    struct xschema_lineage {
        static constexpr bool xoffset_no_lineage = true;
    };

    namespace detail::evolve {
        template <typename... Ts>
        struct type_list {
            static constexpr std::size_t size = sizeof...(Ts);
            template <std::size_t I>
            using at = std::tuple_element_t<I, std::tuple<Ts...>>;
        };

        template <typename W, typename... Vs>
        using lineage_list = std::conditional_t<(std::is_same_v<W, Vs> || ...),
                                                type_list<Vs...>, type_list<Vs..., W>>;

        template <typename W>
        struct lineage_of {
            using type = type_list<W>;
        };
        template <typename W>
            requires (!requires { xschema_lineage<W>::xoffset_no_lineage; })
        struct lineage_of<W> {
            using type = typename xschema_lineage<W>::versions;
        };

        template <typename W>
        using versions_t = typename lineage_of<W>::type;

        template <typename W, std::size_t I>
        using version_t = typename versions_t<W>::template at<I>;

        template <typename W>
        inline constexpr std::size_t version_count = versions_t<W>::size;

        template <typename W>
        inline constexpr auto version_hashes = []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<std::uint64_t, sizeof...(Is)>{wire_schema_hash_v<version_t<W, Is>>()...};
        }(std::make_index_sequence<version_count<W>>{});

        template <typename W>
        inline constexpr std::size_t working_index = [] {
            for (std::size_t i = 0; i < version_count<W>; ++i)
                if (version_hashes<W>[i] == wire_schema_hash_v<W>()) return i;
            return npos;
        }();

        template <typename W>
        constexpr std::size_t find_version(std::uint64_t schema_hash) noexcept {
            for (std::size_t i = 0; i < version_count<W>; ++i)
                if (version_hashes<W>[i] == schema_hash) return i;
            return npos;
        }

        // Calls fn(std::integral_constant<std::size_t, I>{}) for I == index < N.
        template <std::size_t N, typename Fn>
        auto with_index(std::size_t index, Fn&& fn) -> decltype(fn(std::integral_constant<std::size_t, 0>{})) {
            using R = decltype(fn(std::integral_constant<std::size_t, 0>{}));
            if constexpr (std::is_void_v<R>) {
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (void)((index == Is ? (fn(std::integral_constant<std::size_t, Is>{}), true) : false) || ...);
                }(std::make_index_sequence<N>{});
            } else {
                std::optional<R> result;
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (void)((index == Is ? (result.emplace(fn(std::integral_constant<std::size_t, Is>{})), true)
                                        : false) || ...);
                }(std::make_index_sequence<N>{});
                return std::move(*result);
            }
        }

        // ---- lineage checks ----

        template <typename W>
        consteval std::size_t lineage_mismatched_name() {
            for (std::size_t i = 0; i < version_count<W>; ++i)
                if ([&]<std::size_t... Is>(std::index_sequence<Is...>) {
                        std::array<std::uint64_t, sizeof...(Is)> ids{wire_root_type_id_v<version_t<W, Is>>()...};
                        return ids[i];
                    }(std::make_index_sequence<version_count<W>>{}) != wire_root_type_id_v<W>())
                    return i;
            return npos;
        }

        template <typename W>
        consteval std::array<std::size_t, 2> lineage_same_hash() {
            for (std::size_t i = 0; i < version_count<W>; ++i)
                for (std::size_t j = i + 1; j < version_count<W>; ++j)
                    if (version_hashes<W>[i] == version_hashes<W>[j]) return {i, j};
            return {npos, npos};
        }

        template <typename W>
        consteval bool lineage_ok() {
            return lineage_mismatched_name<W>() == npos && lineage_same_hash<W>()[0] == npos &&
                   working_index<W> != npos;
        }

        template <typename W, std::size_t I>
        consteval std::string_view version_name(std::size_t i) {
            if constexpr (I >= version_count<W>) return "?";
            else return i == I ? type_name<version_t<W, I>>() : version_name<W, I + 1>(i);
        }

        template <typename W>
        consteval auto lineage_message() {
            constexpr std::string_view head = "XOffset schema lineage of ";
            constexpr std::string_view w = type_name<W>();
            constexpr std::size_t bad_name = lineage_mismatched_name<W>();
            constexpr auto pair = lineage_same_hash<W>();
            constexpr std::string_view a = bad_name != npos ? version_name<W, 0>(bad_name)
                                         : pair[0] != npos ? version_name<W, 0>(pair[0]) : std::string_view{};
            constexpr std::string_view and_ = pair[0] != npos && bad_name == npos ? " and " : "";
            constexpr std::string_view b = pair[0] != npos && bad_name == npos ? version_name<W, 0>(pair[1])
                                                                               : std::string_view{};
            constexpr std::string_view why =
                bad_name != npos ? " registers another schema name; every version registers the same one"
                : pair[0] != npos ? " have the same schema_hash: they are one generation (versions of one add-only "
                                    "family, or the same layout under other field names); list only the last "
                                    "version of each generation"
                                  : "has no working version";
            constexpr std::string_view colon = ": ";
            Message<head.size() + w.size() + colon.size() + a.size() + and_.size() + b.size() + why.size()> m;
            m.append(head); m.append(w); m.append(colon); m.append(a); m.append(and_); m.append(b); m.append(why);
            return m;
        }

        // ---- k-step conversion along the lineage ----

        template <typename W, std::size_t From, std::size_t To>
        TypedXBuffer<version_t<W, To>> chain(TypedXBuffer<version_t<W, From>>& source, const XEvolveOptions& options) {
            static_assert(From != To);
            constexpr std::size_t Next = From < To ? From + 1 : From - 1;
            if constexpr (Next == To) {
                return ::XOffsetDatastructure::evolve<version_t<W, To>>(source, options);
            } else {
                XEvolveOptions step = options;
                step.base = nullptr;
                step.base_tag = nullptr;
                auto middle = ::XOffsetDatastructure::evolve<version_t<W, Next>>(source, step);
                return chain<W, Next, To>(middle, options);
            }
        }

        // Loads an image of any generation of W's lineage and upgrades it to W.
        // An image of W's own generation (W's add-only family included) and an
        // image of no listed generation go to load_verified<W>, which rejects
        // the latter with its usual message.
        template <typename W, typename Bytes>
        TypedXBuffer<W> load_any_version(const Bytes& wire, std::optional<XBuffer::MaximumCapacity> cap,
                                         const XEvolveOptions& options) {
            const auto peek = peek_wire_fingerprint(wire);
            const std::size_t index = peek ? find_version<W>(peek.fingerprint.schema_hash) : npos;
            if (index == npos || index == working_index<W> ||
                peek.fingerprint.root_type_id != wire_root_type_id_v<W>())
                return TypedXBuffer<W>::load_verified(wire, cap);
            return with_index<version_count<W>>(index, [&](auto I) -> TypedXBuffer<W> {
                if constexpr (I() == working_index<W>) {
                    return TypedXBuffer<W>::load_verified(wire, cap);
                } else {
                    auto old = TypedXBuffer<version_t<W, I()>>::load_verified(wire, cap);
                    XEvolveOptions o = options;
                    if (o.maximum_capacity == 0) o.maximum_capacity = old.maximum_capacity();
                    return chain<W, I(), working_index<W>>(old, o);
                }
            });
        }
    } // namespace detail::evolve

    // Stored data: an image of any generation of W's lineage, each verified as
    // its own generation, upgraded to W. One-way: keep a backup of the old
    // image until the upgraded one is saved.
    template <typename W>
    TypedXBuffer<W> evolve_load_verified(std::span<const std::byte> wire,
                                         std::optional<XBuffer::MaximumCapacity> cap = std::nullopt,
                                         XEvolveOptions options = {}) {
        return detail::evolve::load_any_version<W>(wire, cap, options);
    }

    template <typename W>
    TypedXBuffer<W> evolve_load_verified(const std::string& wire,
                                         std::optional<XBuffer::MaximumCapacity> cap = std::nullopt,
                                         XEvolveOptions options = {}) {
        return detail::evolve::load_any_version<W>(wire, cap, options);
    }

} // namespace XOffsetDatastructure

// XOFFSET_SCHEMA_LINEAGE(Working, G1, G2, ...): the older generations of
// Working's root type whose stored data loads, oldest first, each named by the
// last version of its family (see xschema_lineage). Global scope; use an alias
// for a type name containing commas.
#define XOFFSET_SCHEMA_LINEAGE(Working, ...)                                      \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_SCHEMA_LINEAGE")                     \
    template <> struct XOffsetDatastructure::xschema_lineage<Working> {           \
        using versions = ::XOffsetDatastructure::detail::evolve::lineage_list<    \
            Working, __VA_ARGS__>;                                                \
    };                                                                            \
    static_assert(::XOffsetDatastructure::detail::evolve::lineage_ok<Working>(),  \
                  ::XOffsetDatastructure::detail::evolve::lineage_message<Working>());


#endif // X_OFFSET_EVOLUTION_HPP
