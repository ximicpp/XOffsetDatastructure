#pragma once
// Source-only implementation. Include after the COMPLETE main header, including
// its admission specializations. Never include from inside an existing namespace.
#include "receive_raw_fwd.hpp"
#include "xoffsetdatastructure.hpp"
#include <array>
#include <concepts>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {

// P2996 has no portable query for [[no_unique_address]]. This is a source-level
// opt-in contract, NOT inferred from physical nonoverlap alone. It promises all
// reflected members are ordinary, non-potentially-overlapping subobjects.
template<class T> struct wire_record_layout {
    static constexpr bool ordinary_members = false;
};
template<class K, class V> struct wire_record_layout<XKeyValue<K, V>> {
    static constexpr bool ordinary_members = true;
};

// Explicit user extension, reviewed independently. A specialization provides:
// enabled=true; validate_raw(const char*, detail::WireValidationContext&);
// activate(char*) noexcept, starting the WHOLE T and all necessary subobjects,
// without allocation/callback, changing any byte or taking ownership elsewhere.
// validate_raw must perform the complete old graph checks in the old order.
template<class T> struct wire_activation {
    static constexpr bool enabled = false;
};

} // namespace XOffsetDatastructure
#include "receive_future.hpp"
namespace XOffsetDatastructure {

namespace detail {
struct CanonicalRawBytes {
    std::byte* origin;
    std::size_t maximum,used;
    char* at(std::size_t offset) const noexcept {
        return reinterpret_cast<char*>(origin+offset);
    }
    std::size_t offset(const void* pointer) const noexcept {
        return reinterpret_cast<std::uintptr_t>(pointer)-reinterpret_cast<std::uintptr_t>(origin);
    }
};
inline const char* raw_advance(const char* p,std::size_t n,const CanonicalRawBytes* bytes=nullptr) noexcept {
    return bytes?bytes->at(bytes->offset(p)+n):p+n;
}
inline char* raw_advance(char* p,std::size_t n,const CanonicalRawBytes* bytes=nullptr) noexcept {
    return bytes?bytes->at(bytes->offset(p)+n):p+n;
}
inline char* raw_retreat(char* p,std::size_t n,const CanonicalRawBytes* bytes=nullptr) noexcept {
    return bytes?bytes->at(bytes->offset(p)-n):p-n;
}
struct alignas(8) RawOwningDescriptor {
    std::int32_t arena_delta, data_delta;
    std::uint32_t size, capacity;
};
static_assert(sizeof(RawOwningDescriptor) == 16);
static_assert(offsetof(RawOwningDescriptor, arena_delta) == 0);
static_assert(offsetof(RawOwningDescriptor, data_delta) == 4);
static_assert(offsetof(RawOwningDescriptor, size) == 8);
static_assert(offsetof(RawOwningDescriptor, capacity) == 12);
static_assert(sizeof(RawPayloadHeader) == sizeof(XArena));
static_assert(alignof(RawPayloadHeader) == alignof(XArena));

// Caller proves the fixed-sized source range before calling this primitive.
// The copy is to a distinct live POD; it is never a typed wire-head access.
template<class Pod> Pod raw_wire_pod(const char* address) noexcept {
    static_assert(std::is_trivially_copyable_v<Pod>);
    Pod result;
    std::memcpy(&result, address, sizeof(result));
    return result;
}

inline const char* raw_checked_relative(const char* field, std::int32_t delta,
                                        const WireValidationContext& ctx,
                                        const CanonicalRawBytes* bytes=nullptr) {
    if(bytes) {
        const auto offset=bytes->offset(field);
        std::size_t target{};
        if(delta>=0) {
            const auto amount=std::uint32_t(delta);
            if(offset>bytes->used||amount>bytes->used-offset)
                throw XException("wire validation: relative pointer out of bounds");
            target=offset+amount;
        }else{
            const auto amount=std::uint64_t(-std::int64_t(delta));
            if(amount>offset)throw XException("wire validation: relative pointer out of bounds");
            target=offset-std::size_t(amount);
        }
        if(target>=bytes->used)throw XException("wire validation: relative pointer out of bounds");
        auto* result=bytes->at(target);
        validate_storage_range(result,1,1,ctx,"wire validation: relative pointer out of bounds");
        return result;
    }
    const auto origin = reinterpret_cast<std::uintptr_t>(field);
    std::uintptr_t target{};
    if (delta >= 0) {
        const auto amount = static_cast<std::uint32_t>(delta);
        if (amount > (std::numeric_limits<std::uintptr_t>::max)() - origin)
            throw XException("wire validation: relative pointer out of bounds");
        target = origin + amount;
    } else {
        const auto amount = static_cast<std::uint64_t>(-std::int64_t(delta));
        if (amount > origin)
            throw XException("wire validation: relative pointer out of bounds");
        target = origin - static_cast<std::uintptr_t>(amount);
    }
    const auto* result = reinterpret_cast<const char*>(target);
    validate_storage_range(result, 1, 1, ctx,
        "wire validation: relative pointer out of bounds");
    return result;
}

inline void raw_validate_binding(const char* field, std::int32_t delta,
                                 const WireValidationContext& ctx,
                                 const CanonicalRawBytes* bytes=nullptr) {
    // V2: a head binds to the payload base (arena header) itself.
    if (delta == 0 ||
        raw_checked_relative(field, delta, ctx,bytes) != ctx.base)
        throw XException("wire validation: invalid allocator binding");
}

// Only called after complete raw verification. It does not evaluate a typed
// object or derive rel32 from a stack-POD field's DIFFERENT address.
inline char* raw_verified_relative(char* field, std::int32_t delta,
                                  const CanonicalRawBytes* bytes=nullptr) noexcept {
    if(bytes) {
        const auto offset=bytes->offset(field);
        return bytes->at(delta>=0?offset+std::uint32_t(delta):
            offset-std::size_t(-std::int64_t(delta)));
    }
    const auto origin = reinterpret_cast<std::uintptr_t>(field);
    const auto target = delta >= 0
        ? origin + static_cast<std::uint32_t>(delta)
        : origin - static_cast<std::uintptr_t>(-std::int64_t(delta));
    return reinterpret_cast<char*>(target);
}

inline constexpr std::uint32_t raw_inline_string_flag = 0x80000000u;
inline bool raw_inline_string(const RawOwningDescriptor& head) noexcept {
    return (head.capacity & raw_inline_string_flag) != 0;
}
inline std::uint32_t raw_inline_string_size(const RawOwningDescriptor& head) noexcept {
    return (head.capacity >> 24) & 0x7fu;
}

inline std::string_view raw_verified_string(const char* address) noexcept {
    const auto head = raw_wire_pod<RawOwningDescriptor>(address);
    if (raw_inline_string(head)) return {address + 4, raw_inline_string_size(head)};
    if (!head.capacity) return {};
    const auto* data = raw_verified_relative(
        const_cast<char*>(address + 4), head.data_delta);
    return {data, head.size};
}

template<class T> consteval bool raw_implicit_lifetime() {
    if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
#if defined(__cpp_lib_is_implicit_lifetime)
    if constexpr (std::is_implicit_lifetime_v<T>) return true;
#elif defined(__clang__)
#if __has_builtin(__builtin_is_implicit_lifetime)
    if constexpr (__builtin_is_implicit_lifetime(T)) return true;
#elif __has_builtin(__is_implicit_lifetime)
    if constexpr (__is_implicit_lifetime(T)) return true;
#endif
#endif
    // Independent [class.prop] branch, needed by the fixed SDK whose trait can
    // return false for an aggregate with an implicit nontrivial destructor.
    // No std specialization or constructor call. The explicit ordinary source
    // attestation remains necessary; a user-provided destructor is rejected.
    if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> &&
                  wire_record_layout<T>::ordinary_members) {
        return schema::implicit_lifetime_aggregate<T>();
    } else return false;
}

template<class T> consteval bool raw_customization_enabled() {
    if constexpr (!wire_activation<T>::enabled) return false;
    else return requires(const char* read, char* write, WireValidationContext& ctx) {
        { wire_activation<T>::validate_raw(read, ctx) } -> std::same_as<void>;
        { wire_activation<T>::activate(write) } noexcept -> std::same_as<void>;
    };
}

// [dcl.init.list]/3.8 permits this form from the underlying type exactly for
// fixed-underlying enums, including unscoped enums declared with explicit : U.
template<class E> consteval bool raw_enum_fixed_underlying() {
    if constexpr (!std::is_enum_v<E>) return false;
    else return requires(std::underlying_type_t<E> value) { E{value}; };
}
template<class U> struct RawEnumBounds { U low,high; };
template<class Unsigned> consteval unsigned raw_enum_bit_width(Unsigned value) {
    unsigned width=0;while(value){++width;value>>=1;}return width;
}
template<class E> consteval auto raw_enum_bounds() {
    static_assert(std::is_enum_v<E>);
    using U=std::underlying_type_t<E>;
    using V=std::make_unsigned_t<U>;
    if constexpr (raw_enum_fixed_underlying<E>()) {
        return RawEnumBounds<U>{(std::numeric_limits<U>::min)(),(std::numeric_limits<U>::max)()};
    } else {
        // Empty enumeration is treated as a zero enumerator. CWG2932 leaves
        // its minimum-width corner unresolved; the fixed Clang ABI uses at
        // least one positive bit (also for all-zero enums), accepting 0 and 1.
        constexpr auto range=schema::enum_range<E>();
        const U minimum=range.min,maximum=range.max;
        constexpr auto digits=std::numeric_limits<V>::digits;
        if(minimum>=0) {
            const auto width=std::max(1u,raw_enum_bit_width(static_cast<V>(maximum)));
            const V high=width==digits?(std::numeric_limits<V>::max)():
                         width==0?V{0}:static_cast<V>((V{1}<<width)-1);
            return RawEnumBounds<U>{U{0},static_cast<U>(high)};
        } else {
            // Unsigned subtraction avoids negating a minimum signed value.
            const V magnitude=V{0}-static_cast<V>(minimum);
            const auto width=std::max(raw_enum_bit_width(static_cast<V>(maximum))+1,
                                     raw_enum_bit_width(static_cast<V>(magnitude-1))+1);
            if(width==digits)
                return RawEnumBounds<U>{(std::numeric_limits<U>::min)(),(std::numeric_limits<U>::max)()};
            const V bound=V{1}<<(width-1);
            return RawEnumBounds<U>{static_cast<U>(-static_cast<U>(bound)),static_cast<U>(bound-1)};
        }
    }
}
template<class E>
bool raw_enum_representation_valid(std::underlying_type_t<E> value) noexcept {
    if constexpr (raw_enum_fixed_underlying<E>()) return true;
    else {constexpr auto range=raw_enum_bounds<E>();return value>=range.low&&value<=range.high;}
}

template<class T> consteval bool raw_descriptor_layout() {
    // The legacy recognition traits use is_base_of. A user-derived head with a
    // noexcept default constructor is not thereby a no-callback library head.
    return fixed_detail::is_owning_head<T>::value &&
        sizeof(T) == 16 && alignof(T) == 8 &&
        std::is_standard_layout_v<T> &&
        std::is_nothrow_default_constructible_v<T> &&
        offsetof(T, arena_delta) == 0 && offsetof(T, data_delta) == 4 &&
        offsetof(T, size_) == 8 && offsetof(T, capacity_) == 12;
}

template<class Key> consteval bool raw_builtin_enum_order() {
    if constexpr (!std::is_enum_v<Key>) return false;
    else if constexpr (std::is_same_v<std::underlying_type_t<Key>, bool>) return false;
    else return requires(const Key& a, const Key& b) {
            { a < b } noexcept -> std::same_as<bool>;
        } &&
        !requires(const Key& a, const Key& b) { operator<(a, b); } &&
        !requires(const Key& a, const Key& b) { operator<=>(a, b); };
    // Explicit function-call syntax considers ADL functions, never builtin
    // operators. The expression check also rejects ambiguous/invalid comparison.
    // User enum semantics need an explicit raw_key_order policy, not an implicit
    // underlying-value substitution or an accidental noexcept user callback.
}
} // namespace detail

// A policy sees only already raw-validated key bytes. It must not create a
// typed reference to a not-yet-active user key. A user specialization provides
// enabled=true and less(const char*,const char*,const WireValidationContext&).
// Any throwing comparison happens before activation.
template<class Key> struct raw_key_order {
    static constexpr bool canonical_bytewise_string_order =
        std::is_same_v<Key,XString>||std::is_same_v<Key,XFixedString>;
    static constexpr bool enabled = [] {
        if constexpr (std::is_integral_v<Key>) return true;
        else if constexpr (std::is_enum_v<Key>)
            return detail::raw_builtin_enum_order<Key>();
        else return detail::is_fixed_string_v<Key> &&
            (std::is_same_v<Key, XString> || std::is_same_v<Key, XFixedString>);
    }();
    static bool less(const char* lhs, const char* rhs,
                     const detail::WireValidationContext&) noexcept
        requires(enabled) {
        if constexpr (detail::is_fixed_string_v<Key>)
            return detail::raw_verified_string(lhs) < detail::raw_verified_string(rhs);
        else {
            const Key a = detail::raw_wire_pod<Key>(lhs);
            const Key b = detail::raw_wire_pod<Key>(rhs);
            // Old verification compares const element/key lvalues. In
            // particular, do not invoke an enum's rvalue-only ADL overload.
            return a < b;
        }
    }
};

namespace detail {
template<class Key>
bool raw_compare_keys(const char* lhs,const char* rhs,const WireValidationContext& ctx,
                      const CanonicalRawBytes* bytes=nullptr) {
    if constexpr (requires { raw_key_order<Key>::canonical_bytewise_string_order; }) {
        if constexpr (raw_key_order<Key>::canonical_bytewise_string_order) {
            if(bytes) {
                const auto a=raw_wire_pod<RawOwningDescriptor>(lhs);
                const auto b=raw_wire_pod<RawOwningDescriptor>(rhs);
                auto chars=[&](const char* head,const RawOwningDescriptor& d) -> const char* {
                    if(raw_inline_string(d)) return raw_advance(head,4,bytes);
                    return d.capacity?raw_verified_relative(const_cast<char*>(raw_advance(head,4,bytes)),d.data_delta,bytes):nullptr;
                };
                const auto as=raw_inline_string(a)?raw_inline_string_size(a):a.size;
                const auto bs=raw_inline_string(b)?raw_inline_string_size(b):b.size;
                auto* x=chars(lhs,a);
                auto* y=chars(rhs,b);
                for(std::size_t i=0;i<std::min(as,bs);++i) {
                    const auto u=raw_wire_pod<unsigned char>(raw_advance(x,i,bytes));
                    const auto v=raw_wire_pod<unsigned char>(raw_advance(y,i,bytes));
                    if(u!=v)return u<v;
                }
                return as<bs;
            }
        }
    }
    // Explicit policies receive positions, never an inactive typed key. Their
    // audited raw-read contract still applies; no invented default ordering.
    return raw_key_order<Key>::less(lhs,rhs,ctx);
}

inline void raw_register_allocation(const void* pointer,std::size_t n,std::size_t alignment,
        WireValidationContext& ctx,bool free=false,const CanonicalRawBytes* bytes=nullptr) {
    // V2: no block header; the extent is derived from the owner (8-aligned,
    // length rounded to 8). Canonical bytes are contiguous at ctx.base.
    (void)bytes;
    register_allocation(pointer,n,alignment,ctx,free);
}

inline void raw_register_wire_holes(const void* holes,std::size_t count,WireValidationContext& ctx,
        const CanonicalRawBytes* bytes=nullptr) {
    const auto used=static_cast<std::uint64_t>(ctx.end-ctx.base);
    for(std::size_t i=0;i<count;++i) {
        XBufferCore::WireHole hole;
        std::memcpy(&hole,static_cast<const char*>(holes)+i*sizeof(hole),sizeof(hole));
        if(hole.bytes==0||(hole.bytes&7u)!=0||(hole.offset&7u)!=0||
           hole.offset<sizeof(XArena)||hole.offset>used||hole.bytes>used-hole.offset)
            throw XException("wire validation failed: invalid hole table entry");
        register_free_range(raw_advance(ctx.base,hole.offset,bytes),hole.bytes,ctx,
                            "wire validation failed: invalid hole table entry");
    }
}

template<class Key> consteval bool raw_key_order_available() {
    if constexpr (!raw_key_order<Key>::enabled) return false;
    else return requires(const char* a, const char* b, const WireValidationContext& c) {
        { raw_key_order<Key>::less(a, b, c) } -> std::same_as<bool>;
    };
}

template<class T, std::size_t I, std::size_t J = I + 1>
consteval bool raw_member_ranges_disjoint() {
    constexpr auto count = schema::member_count_v<T>;
    if constexpr (J >= count) return true;
    else {
        using a = schema::member_t<T, I>;
        using b = schema::member_t<T, J>;
        if constexpr (a::is_bit_field || b::is_bit_field)
            return false;
        else {
            using A = typename a::type;
            using B = typename b::type;
            constexpr auto x = a::offset;
            constexpr auto y = b::offset;
            return (x + sizeof(A) <= y || y + sizeof(B) <= x) &&
                raw_member_ranges_disjoint<T, I, J + 1>();
        }
    }
}

template<class T, std::size_t I> consteval bool raw_member_supported() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else {
        using M = typename member::type;
        constexpr auto offset = member::offset;
        return offset <= sizeof(T) && sizeof(M) <= sizeof(T) - offset &&
            receive_raw_supported<M>() && raw_member_ranges_disjoint<T, I>();
    }
}

template<class T> consteval bool raw_only_implicit_special_members() {
    return schema::only_implicit_special_members<T>();
}

template<class T> consteval bool receive_raw_supported() {
    if constexpr (std::is_const_v<T> || std::is_volatile_v<T> ||
                  std::is_reference_v<T> || std::is_pointer_v<T> ||
                  std::is_member_pointer_v<T> || std::is_union_v<T>)
        return false;
    else if constexpr (!v1_wire_admitted<T>()) return false;
    else if constexpr (wire_activation<T>::enabled)
        return raw_customization_enabled<T>();
    else if constexpr (std::is_array_v<T>)
        return std::extent_v<T> != 0 &&
            receive_raw_supported<std::remove_extent_t<T>>();
    else if constexpr (fixed_size_scalar<T>) return true;
    else if constexpr (std::is_enum_v<T>)
        return !std::is_same_v<std::underlying_type_t<T>, bool>;
    else if constexpr (is_fixed_string_v<T>)
        return raw_descriptor_layout<T>();
    else if constexpr (is_fixed_flat_map_v<T>)
        return raw_descriptor_layout<T>() &&
            receive_raw_supported<typename T::value_type>() &&
            raw_key_order_available<typename T::key_type>();
    else if constexpr (is_fixed_flat_set_v<T>)
        return raw_descriptor_layout<T>() &&
            receive_raw_supported<typename T::value_type>() &&
            raw_key_order_available<typename T::value_type>();
    else if constexpr (is_fixed_vector_v<T>)
        return raw_descriptor_layout<T>() &&
            receive_raw_supported<typename T::value_type>();
    else if constexpr (std::is_class_v<T>) {
        if constexpr (!std::is_aggregate_v<T> || !raw_implicit_lifetime<T>() ||
                      !std::is_standard_layout_v<T> ||
                      schema::base_count_v<T> != 0 ||
                      !raw_only_implicit_special_members<T>() ||
                      !wire_record_layout<T>::ordinary_members)
            return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) {
            return (raw_member_supported<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

template<class T, std::size_t I> consteval bool raw_member_future_supported() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else {
        using Member=typename member::type;
        return receive_raw_future_supported<Member>();
    }
}

template<class T> consteval bool receive_raw_future_supported() {
    if constexpr (!receive_raw_supported<T>()) return false;
    else if constexpr (wire_activation<T>::enabled) {
        if constexpr (!custom_future_contract::surface_complete<T>()) return false;
        else {
            // Initial V1 admission independently rejects bases/pointers and
            // checks layout. Check BOTH declared future frontier and every
            // actual field, even when the initial graph has empty containers.
            constexpr bool fields=[] {
                if constexpr (std::is_class_v<T>)
                    return []<std::size_t... I>(std::index_sequence<I...>) {
                        return (raw_member_future_supported<T,I>() && ...);
                    }(std::make_index_sequence<schema::member_count_v<T>>{});
                else if constexpr (std::is_array_v<T>)
                    return receive_raw_future_supported<std::remove_extent_t<T>>();
                else return true;
            }();
            // Descriptor reflection alone does not reveal future element
            // types. A custom policy/list cannot hide a RawOnly value_type.
            constexpr bool builtin_elements=[] {
                if constexpr (is_fixed_vector_v<T> || is_fixed_flat_set_v<T> || is_fixed_flat_map_v<T>)
                    return receive_raw_future_supported<typename T::value_type>();
                else return true;
            }();
            constexpr bool declared=[]<class... Child>(wire_future_children<Child...>*) {
                return (receive_raw_future_supported<Child>() && ...);
            }(static_cast<typename wire_future_lifetime<T>::children*>(nullptr));
            return fields && declared && builtin_elements;
        }
    }
    else if constexpr (std::is_array_v<T>)
        return receive_raw_future_supported<std::remove_extent_t<T>>();
    else if constexpr (is_fixed_string_v<T>) return raw_descriptor_layout<T>();
    else if constexpr (is_fixed_vector_v<T> || is_fixed_flat_set_v<T> || is_fixed_flat_map_v<T>)
        return raw_descriptor_layout<T>() &&
            receive_raw_future_supported<typename T::value_type>();
    else if constexpr (std::is_class_v<T>)
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return (raw_member_future_supported<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    else return true;
}

template<class T> consteval bool raw_needs_validation() {
    if constexpr (wire_activation<T>::enabled) return true;
    else if constexpr (std::is_enum_v<T>) return !raw_enum_fixed_underlying<T>();
    else if constexpr (std::is_same_v<T, bool> ||
                       fixed_detail::is_owning_head<T>::value) return true;
    else if constexpr (std::is_array_v<T>)
        return raw_needs_validation<std::remove_extent_t<T>>();
    else if constexpr (std::is_class_v<T>) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return ([] {
                using member = schema::member_t<T, I>;
                using M = typename member::type;
                return raw_needs_validation<M>();
            }() || ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

template<class T> consteval bool raw_needs_activation() {
    if constexpr (wire_activation<T>::enabled ||
                  fixed_detail::is_owning_head<T>::value) return true;
    else if constexpr (std::is_array_v<T>)
        return raw_needs_activation<std::remove_extent_t<T>>();
    else if constexpr (std::is_class_v<T>) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return ([] {
                using member = schema::member_t<T, I>;
                using M = typename member::type;
                return raw_needs_activation<M>();
            }() || ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

inline RawPayloadHeader read_raw_arena_header(const char* base, std::size_t extent) {
    if (!base || extent < sizeof(XArena))
        throw XException("XBufferCore: payload too small");
    const auto header = raw_wire_pod<RawPayloadHeader>(base);
    constexpr char magic[8] = {'X','B','U','F','V','2','\0','\0'};
    if (std::memcmp(header.magic, magic, 8) != 0 ||
        header.header_size != sizeof(XArena))
        throw XException("XBufferCore: payload header mismatch");
    constexpr auto minimum = sizeof(XArena);
    if (header.used_end < minimum || header.used_end > extent)
        throw XException("XBufferCore: payload used_end out of range");
    return header;
}

template<class T>
std::size_t raw_root_offset(const char* base, std::size_t extent,
                            const RawPayloadHeader& h) {
    if (!h.root_offset || h.root_offset > extent ||
        sizeof(T) > extent - h.root_offset ||
        (reinterpret_cast<std::uintptr_t>(base) + h.root_offset) % alignof(T))
        throw XException("load_verified<T>(): root not found");
    return static_cast<std::size_t>(h.root_offset);
}


template<class T> void validate_wire_value_raw(const char*,WireValidationContext&,const CanonicalRawBytes* = nullptr);

inline void validate_string_raw(const char* address,WireValidationContext& ctx,const CanonicalRawBytes* bytes=nullptr) {
    const auto head = raw_wire_pod<RawOwningDescriptor>(address);
    raw_validate_binding(address,head.arena_delta,ctx,bytes);
    if (raw_inline_string(head)) {
        validate_inline_string_bytes(address);
        return;
    }
    if (head.size > head.capacity || head.capacity >= XBufferCore::MAX_PAYLOAD)
        throw XException("wire validation failed: XFixedString size exceeds capacity");
    if (!head.capacity) {
        if (head.size != 0 || head.data_delta != 0)
            throw XException("wire validation failed: empty XFixedString has invalid payload");
        return;
    }
    const auto* data=raw_checked_relative(raw_advance(address,4,bytes),head.data_delta,ctx,bytes);
    validate_storage_range(data, std::size_t(head.capacity) + 1, alignof(char), ctx,
        "wire validation failed: XFixedString payload out of bounds");
    raw_register_allocation(data,
        fixed_detail::round_up_to_alignment(head.capacity+1,8),8,ctx,false,bytes);
    if(raw_wire_pod<char>(raw_advance(data,head.size,bytes))!='\0')
        throw XException("wire validation failed: XFixedString missing terminator");
}

template<class Element>
RawOwningDescriptor validate_vector_raw(const char* address,
                                        WireValidationContext& ctx,const CanonicalRawBytes* view=nullptr) {
    const auto head = raw_wire_pod<RawOwningDescriptor>(address);
    raw_validate_binding(address,head.arena_delta,ctx,view);
    if (head.size > head.capacity ||
        head.capacity > (XBufferCore::MAX_PAYLOAD - 8u) / sizeof(Element))
        throw XException("wire validation failed: XFixedVector size exceeds capacity");
    if (!head.capacity) {
        if (head.size != 0 || head.data_delta != 0)
            throw XException("wire validation failed: empty XFixedVector has invalid payload");
        return head;
    }
    const auto* data=raw_checked_relative(raw_advance(address,4,view),head.data_delta,ctx,view);
    const auto bytes = std::size_t(head.capacity) * sizeof(Element);
    validate_storage_range(data, bytes, alignof(Element), ctx,
        "wire validation failed: XFixedVector payload out of bounds");
    raw_register_allocation(data,(bytes+7u)&~std::size_t(7u),
        std::max<std::size_t>(alignof(Element),8),ctx,false,view);
    if constexpr (raw_needs_validation<Element>())
        for (std::uint32_t i = 0; i < head.size; ++i)
            validate_wire_value_raw<Element>(raw_advance(data,std::size_t(i)*sizeof(Element),view),ctx,view);
    return head;
}

template<class T>
void validate_wire_value_raw(const char* address,WireValidationContext& ctx,const CanonicalRawBytes* bytes) {
    static_assert(receive_raw_supported<T>(), "unsupported raw wire activation domain");
    if constexpr (wire_activation<T>::enabled) {
        if(bytes) {
            if constexpr (requires(char* write,RawActivationStats& stats) {
                { wire_activation<T>::validate_raw(address,ctx,bytes) } -> std::same_as<void>;
                { wire_activation<T>::activate(write,stats,bytes) } noexcept -> std::same_as<void>;
            })
                wire_activation<T>::validate_raw(address,ctx,bytes);
            else throw XException("raw receive: custom canonical validation policy required");
        } else wire_activation<T>::validate_raw(address,ctx);
    }
    else if constexpr (std::is_same_v<T, bool>) {
        const auto bits = raw_wire_pod<unsigned char>(address);
        if (bits > 1) throw XException("wire validation: invalid bool representation");
    } else if constexpr (std::is_enum_v<T>) {
        // Read U first, never E, before the full standard value-range check.
        using U=std::underlying_type_t<T>;
        const U bits=raw_wire_pod<U>(address);
        if(!raw_enum_representation_valid<T>(bits))
            throw XException("wire validation: invalid enum representation");
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (raw_needs_validation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                validate_wire_value_raw<E>(raw_advance(address,i*sizeof(E),bytes),ctx,bytes);
    } else if constexpr (is_fixed_string_v<T>)
        validate_string_raw(address,ctx,bytes);
    else if constexpr (is_fixed_flat_set_v<T>) {
        using E = typename T::value_type;
        const auto head=validate_vector_raw<E>(address,ctx,bytes);
        if (head.size > 1) {
            const auto* data = raw_verified_relative(
                const_cast<char*>(raw_advance(address,4,bytes)),head.data_delta,bytes);
            for (std::uint32_t i = 1; i < head.size; ++i)
                if(!raw_compare_keys<E>(raw_advance(data,std::size_t(i-1)*sizeof(E),bytes),
                    raw_advance(data,std::size_t(i)*sizeof(E),bytes),ctx,bytes))
                    throw XException("wire validation: set keys not strictly ordered");
        }
    } else if constexpr (is_fixed_flat_map_v<T>) {
        using E = typename T::value_type;
        using K = typename T::key_type;
        const auto head=validate_vector_raw<E>(address,ctx,bytes);
        if (head.size > 1) {
            const auto* data = raw_verified_relative(
                const_cast<char*>(raw_advance(address,4,bytes)),head.data_delta,bytes);
            for (std::uint32_t i = 1; i < head.size; ++i)
                if(!raw_compare_keys<K>(
                    raw_advance(data,std::size_t(i-1)*sizeof(E)+offsetof(E,first),bytes),
                    raw_advance(data,std::size_t(i)*sizeof(E)+offsetof(E,first),bytes),ctx,bytes))
                    throw XException("wire validation: map keys not strictly ordered");
        }
    } else if constexpr (is_fixed_vector_v<T>)
        (void)validate_vector_raw<typename T::value_type>(address,ctx,bytes);
    else if constexpr (std::is_class_v<T>) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_validation<M>())
                validate_wire_value_raw<M>(
                    raw_advance(address,member::offset,bytes),ctx,bytes);
        });
    }
}

template<class T>
RawWireVerification verify_structural_wire_graph_raw_at(
        const char* base, std::size_t extent, const RawPayloadHeader& arena,
        std::size_t root, const void* holes, std::size_t hole_count,
        std::pmr::memory_resource* upstream,const CanonicalRawBytes* bytes=nullptr,
        const void* gaps=nullptr,std::size_t gap_count=0) {
    static_assert(receive_raw_supported<T>(), "unsupported raw wire activation domain");
    if(!upstream)throw std::invalid_argument("validation scratch: null upstream");
    WireValidationContext ctx{
        .base = base,
        .end = raw_advance(base,extent,bytes),
        .segment = reinterpret_cast<XArena*>(const_cast<char*>(base)),
        .upstream = upstream
    };
    if (arena.flags != 0 || arena.reserved0 != 0 || arena.reserved1 != 0)
        throw XException("wire validation: unsupported arena header flags");
    register_owned_range(base, sizeof(XArena), ctx, "wire validation: invalid arena header");
    raw_register_wire_holes(holes,hole_count,ctx,bytes);
    raw_register_allocation(raw_advance(base,root,bytes),sizeof(T),alignof(T),ctx,false,bytes);
    validate_wire_value_raw<T>(raw_advance(base,root,bytes),ctx,bytes);
    require_exact_coverage(ctx);
    return {root, extent};
}

template<class T>
RawWireVerification verify_structural_wire_graph_raw(
        XBufferCore& owner, std::pmr::memory_resource* upstream) {
    static_assert(receive_raw_supported<T>(), "unsupported raw wire activation domain");
    const auto* base = static_cast<const char*>(owner.get_address());
    const auto extent = owner.get_size();
    const auto arena = read_raw_arena_header(base, extent);
    if (arena.used_end != extent)
        throw XException("load_verified<T>(): non-normalized payload extent");
    const auto root = raw_root_offset<T>(base, extent, arena);
    // A live owner's free table is its hole table.
    std::vector<XBufferCore::WireHole> holes;
    if (!owner.free_table_complete())
        throw XException("wire validation: free table incomplete");
    holes.reserve(owner.free_block_count());
    owner.for_each_free_block([&](XBufferCore::WireHole hole) { holes.push_back(hole); });
    return verify_structural_wire_graph_raw_at<T>(
        base, extent, arena, root, holes.data(), holes.size(), upstream);
}

// These recovery/activation operations preserve ALL received bytes. They
// do not bit_cast/evaluate unused bool capacity, construct a byte[] overlay,
// allocate host memory, or use typed T* arithmetic before the array exists.
template<class T>
T* raw_recover_array_object(char* address,std::size_t capacity,
                            const CanonicalRawBytes* bytes=nullptr) noexcept {
    if (!capacity) return nullptr;
    // The BEFORE-FILL byte[MAX] operation (or legacy complete memcpy fixture)
    // already supplied this implicit parent array. Do not restart it after
    // initialized scalar bytes have arrived. Launder does not evaluate tails.
    void* storage=bytes?static_cast<void*>(bytes->origin+bytes->offset(address)):
                        static_cast<void*>(address);
    // LWG 4130 permits recovery of the complete element even where its lifetime
    // has not begun but its containing array is alive. T is complete; do not
    // instantiate std::launder on incomplete T[], rejected by the fixed SDK.
    T* first=std::launder(reinterpret_cast<T*>(storage));
    (void)(first+capacity);
    return first;
}
template<class Pod>
Pod activate_metadata_raw(char* address) noexcept {
    static_assert(std::is_trivially_copyable_v<Pod>);
    const auto saved = raw_wire_pod<Pod>(address);
    // memcpy explicitly supplies implicit object creation of these exact
    // library POD types; copying back the complete saved bytes preserves padding.
    std::memcpy(address, &saved, sizeof(Pod));
    return saved;
}

// Only inline subobjects whose containing array/ordinary outer was restarted.
// Recursing into out-of-line children would unnecessarily end still-live heads.
template<class T>
void reactivate_inline_value_raw(char* address,const CanonicalRawBytes* bytes=nullptr) noexcept {
    static_assert(receive_raw_future_supported<T>());
    if constexpr (wire_activation<T>::enabled) {
        wire_future_lifetime<T>::reactivate_inline(static_cast<void*>(address),bytes);
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        static_assert(raw_descriptor_layout<T>());
        const auto saved = raw_wire_pod<RawOwningDescriptor>(address);
        auto* object = ::new (static_cast<void*>(address)) T;
        object->arena_delta=saved.arena_delta; object->data_delta=saved.data_delta;
        object->size_=saved.size; object->capacity_=saved.capacity;
    } else if constexpr (std::is_array_v<T>) {
        using E=std::remove_extent_t<T>;
        for(std::size_t i=0;i<std::extent_v<T>;++i)
            reactivate_inline_value_raw<E>(raw_advance(address,i*sizeof(E),bytes),bytes);
    } else if constexpr (std::is_class_v<T>) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M=typename member::type;
            reactivate_inline_value_raw<M>(raw_advance(address,member::offset,bytes),bytes);
        });
    }
}

template<class T> void activate_wire_value_raw(char* address,
        RawActivationStats& stats,const CanonicalRawBytes* bytes=nullptr) noexcept {
    static_assert(receive_raw_supported<T>(), "unsupported raw wire activation domain");
    if constexpr (wire_activation<T>::enabled) {
        if(bytes) {
            if constexpr (requires { wire_activation<T>::activate(address,stats,bytes); })
                wire_activation<T>::activate(address,stats,bytes);
            else std::terminate(); // validate refuses this policy before proof
        } else wire_activation<T>::activate(address);
        ++stats.custom_objects;
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        const auto saved = raw_wire_pod<RawOwningDescriptor>(address);
        static_assert(raw_descriptor_layout<T>());
        auto* object = ::new (static_cast<void*>(address)) T;
        object->arena_delta = saved.arena_delta;
        object->data_delta = saved.data_delta;
        object->size_ = saved.size;
        object->capacity_ = saved.capacity;
        ++stats.owning_heads;
        if constexpr (is_fixed_string_v<T>) {
            if(saved.capacity && !raw_inline_string(saved)) {
                auto* data=raw_verified_relative(raw_advance(address,4,bytes),saved.data_delta,bytes);
                (void)raw_recover_array_object<char>(data,std::size_t(saved.capacity)+1,bytes);
                ++stats.capacity_arrays;
            }
        } else {
            using E = typename T::value_type;
            auto* data=saved.capacity?raw_verified_relative(raw_advance(address,4,bytes),saved.data_delta,bytes):nullptr;
            // ALWAYS recover the complete pre-fill parent array, including
            // scalar and size==0/spare capacity. No typed tail values are read.
            if(saved.capacity) {
                (void)raw_recover_array_object<E>(data,saved.capacity,bytes);
                ++stats.capacity_arrays;
            }
            if constexpr (raw_needs_activation<E>()) {
                for (std::uint32_t i = 0; i < saved.size; ++i)
                    activate_wire_value_raw<E>(raw_advance(data,std::size_t(i)*sizeof(E),bytes),stats,bytes);
            }
        }
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        (void)raw_recover_array_object<E>(address,std::extent_v<T>,bytes);
        ++stats.capacity_arrays;
        if constexpr (raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                activate_wire_value_raw<E>(raw_advance(address,i*sizeof(E),bytes),stats,bytes);
    } else if constexpr (std::is_class_v<T>) {
        // Recover the ordinary outer selected BEFORE FILL; a new allocation
        // event here would restart initialized scalars with indeterminate value.
        (void)std::launder(reinterpret_cast<T*>(address));
        ++stats.ordinary_outers;
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (raw_needs_activation<M>())
                activate_wire_value_raw<M>(
                    raw_advance(address,member::offset,bytes),stats,bytes);
        });
    }
}

template<class T>
RawActivationStats activate_verified_wire_at(char* base,std::size_t root,
                                            const CanonicalRawBytes* bytes) noexcept {
    // PRECONDITIONS: same address/unchanged bytes as successful full raw proof;
    // receive-before-fill byte[]/outer/T[capacity] protocol; quiescent ownership;
    // no remaining throwing registry/scratch/transport work. No validation,
    // allocation, user constructor, typed raw-head read, or full-payload copy.
    RawActivationStats stats;
    (void)activate_metadata_raw<XArena>(base); ++stats.arena_headers;
    // V2: no block headers, allocator state or in-payload free lists.
    activate_wire_value_raw<T>(raw_advance(base,root,bytes),stats,bytes);
    return stats;
}

template<class T>
RawActivationStats activate_verified_wire(char* base,std::size_t root) noexcept {
    return activate_verified_wire_at<T>(base,root,nullptr);
}

namespace receive_raw {
template<class T> requires(future_supported_v<T>)
void reactivate_inline_prefix(T* data,std::size_t live_count) noexcept {
    if constexpr (!raw_needs_activation<T>()) return;
    else {
        const auto n=live_count*sizeof(T);
        // W9: no canonical receive-array lookup; always the legacy flat
        // element-address path (origin == nullptr).
        std::byte* origin=nullptr;
        CanonicalRawBytes bytes{origin,n,n};
        for(std::size_t i=0;i<live_count;++i) {
            // Legacy has an already-created T[] from the caller's barrier;
            // form each T element address, never char* across its first element.
            auto* slot=origin?bytes.at(i*sizeof(T)):reinterpret_cast<char*>(data+i);
            reactivate_inline_value_raw<T>(slot,origin?&bytes:nullptr);
        }
    }
}

template<class T> requires(supported_v<T>)
Proof<T> validate(char* base, std::size_t used, std::uint64_t wire_root_offset,
                  const void* holes, std::size_t hole_count,
                  std::pmr::memory_resource* upstream,
                  const void* gaps, std::size_t gap_count) {
    const auto arena = read_raw_arena_header(base, used);
    if (arena.used_end != used)
        throw XException("load_verified<T>(): non-normalized payload extent");
    const auto root = raw_root_offset<T>(base, used, arena);
    if (root != wire_root_offset)
        throw XException("load_verified<T>(): root offset mismatch");
    (void)verify_structural_wire_graph_raw_at<T>(
        base, used, arena, root, holes, hole_count, upstream, nullptr, gaps, gap_count);
    // Local scratch and its upstream callbacks have finished before a proof is
    // returned. No pointer into scratch is retained; activation walks the same
    // already-proved byte graph without allocation or a typed raw-head read.
    return Proof<T>(base, root);
}

template<class T> requires(supported_v<T>)
Proof<T> validate(std::byte* origin,std::size_t maximum,std::size_t used,
        std::uint64_t wire_root_offset,const void* holes,std::size_t hole_count,
        std::pmr::memory_resource* upstream,const void* gaps,std::size_t gap_count) {
    if(!origin||used>maximum)throw XException("XBufferCore: payload too small");
    CanonicalRawBytes bytes{origin,maximum,used};
    const auto arena=read_raw_arena_header(bytes.at(0),used);
    if(arena.used_end!=used)throw XException("load_verified<T>(): non-normalized payload extent");
    const auto root=raw_root_offset<T>(bytes.at(0),used,arena);
    if(root!=wire_root_offset)throw XException("load_verified<T>(): root offset mismatch");
    (void)verify_structural_wire_graph_raw_at<T>(bytes.at(0),used,arena,root,holes,hole_count,upstream,&bytes,gaps,gap_count);
    return Proof<T>(origin,maximum,used,root);
}

template<class T>
RawActivationStats Proof<T>::activate() && noexcept {
    auto* base = base_;
    base_ = nullptr; // consume before any explicit customization can be entered
    if (!base) return {};
    if(origin_) {
        CanonicalRawBytes bytes{origin_,maximum_,used_};
        origin_=nullptr;
        return activate_verified_wire_at<T>(bytes.at(0),root_,&bytes);
    }
    return activate_verified_wire<T>(base,root_);
}
} // namespace receive_raw
} // namespace detail
} // namespace XOffsetDatastructure
