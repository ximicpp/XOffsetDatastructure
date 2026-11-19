#pragma once
// Object lifetime inside the arena: the compile-time domain of types whose
// lifetime the library restarts in place (implicit-lifetime ordinary records,
// scalars, arrays and container heads), and the helpers that begin arrays,
// restart inline heads after a move, and shift or copy runs of elements.
#include "xoffsetdatastructure.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <exception>
#include <new>
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

} // namespace XOffsetDatastructure

namespace XOffsetDatastructure::detail {

// V2.1 container head: {data_delta, size, capacity}, 12 bytes, 4-aligned. The
// data rel32 (and an inline string's bytes) start at the head's address.
struct OwningHeadBytes {
    std::int32_t data_delta;
    std::uint32_t size, capacity;
};
static_assert(sizeof(OwningHeadBytes) == 12);
static_assert(offsetof(OwningHeadBytes, data_delta) == 0);
static_assert(offsetof(OwningHeadBytes, size) == 4);
static_assert(offsetof(OwningHeadBytes, capacity) == 8);

template<class T> consteval bool implicit_lifetime_record() {
    if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
    if constexpr (fixed_detail::std_implicit_lifetime<T>()) return true;
    // Independent [class.prop] branch, needed by the fixed SDK whose trait can
    // return false for an aggregate with an implicit nontrivial destructor.
    // No std specialization or constructor call. The explicit ordinary source
    // attestation remains necessary; a user-provided destructor is rejected.
    if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> &&
                  wire_record_layout<T>::ordinary_members) {
        return schema::implicit_lifetime_aggregate<T>();
    } else return false;
}

template<class T> consteval bool owning_head_layout() {
    // The recognition traits use is_base_of. A user-derived head with a
    // noexcept default constructor is not thereby a no-callback library head.
    return fixed_detail::is_owning_head<T>::value &&
        sizeof(T) == 12 && alignof(T) == 4 &&
        std::is_standard_layout_v<T> &&
        std::is_nothrow_default_constructible_v<T> &&
        offsetof(T, data_delta) == 0 && offsetof(T, size_) == 4 &&
        offsetof(T, capacity_) == 8;
}

// Keys of an ordered set or map in the domain: integers, enums ordered by the
// built-in < (no user operator< or operator<=>, bool excluded), and strings.
template<class Key> consteval bool builtin_key_order() {
    if constexpr (std::is_integral_v<Key>) return true;
    else if constexpr (std::is_enum_v<Key>) {
        if constexpr (std::is_same_v<std::underlying_type_t<Key>, bool>) return false;
        else return requires(const Key& a, const Key& b) {
                { a < b } noexcept -> std::same_as<bool>;
            } &&
            !requires(const Key& a, const Key& b) { operator<(a, b); } &&
            !requires(const Key& a, const Key& b) { operator<=>(a, b); };
        // Explicit function-call syntax considers ADL functions, never builtin
        // operators. The expression check also rejects ambiguous/invalid comparison.
    } else return std::is_same_v<Key, XString>;
}

template<class T, std::size_t I, std::size_t J = I + 1>
consteval bool member_ranges_disjoint() {
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
                member_ranges_disjoint<T, I, J + 1>();
        }
    }
}

template<class T> consteval bool inplace_lifetime_supported();

template<class T, std::size_t I> consteval bool inplace_lifetime_member() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else {
        using M = typename member::type;
        constexpr auto offset = member::offset;
        return offset <= sizeof(T) && sizeof(M) <= sizeof(T) - offset &&
            inplace_lifetime_supported<M>() && member_ranges_disjoint<T, I>();
    }
}

// The library may begin and restart the lifetime of T in place: scalars,
// non-bool enums, arrays, container heads with the descriptor layout over such
// elements (ordered keys by builtin_key_order), and aggregate implicit-lifetime
// standard-layout records without bases or user special members that declare
// wire_record_layout ordinary members, recursively.
template<class T> consteval bool inplace_lifetime_supported() {
    if constexpr (std::is_const_v<T> || std::is_volatile_v<T> ||
                  std::is_reference_v<T> || std::is_pointer_v<T> ||
                  std::is_member_pointer_v<T> || std::is_union_v<T>)
        return false;
    else if constexpr (!wire_admitted<T>()) return false;
    else if constexpr (std::is_array_v<T>)
        return std::extent_v<T> != 0 &&
            inplace_lifetime_supported<std::remove_extent_t<T>>();
    else if constexpr (fixed_size_scalar<T>) return true;
    else if constexpr (std::is_enum_v<T>)
        return !std::is_same_v<std::underlying_type_t<T>, bool>;
    else if constexpr (is_fixed_string_v<T>)
        return owning_head_layout<T>();
    else if constexpr (is_fixed_flat_map_v<T>)
        return owning_head_layout<T>() &&
            inplace_lifetime_supported<typename T::value_type>() &&
            builtin_key_order<typename T::key_type>();
    else if constexpr (is_fixed_flat_set_v<T>)
        return owning_head_layout<T>() &&
            inplace_lifetime_supported<typename T::value_type>() &&
            builtin_key_order<typename T::value_type>();
    else if constexpr (is_fixed_vector_v<T>)
        return owning_head_layout<T>() &&
            inplace_lifetime_supported<typename T::value_type>();
    else if constexpr (std::is_class_v<T>) {
        if constexpr (!std::is_aggregate_v<T> || !implicit_lifetime_record<T>() ||
                      !std::is_standard_layout_v<T> ||
                      schema::base_count_v<T> != 0 ||
                      !schema::only_implicit_special_members<T>() ||
                      !wire_record_layout<T>::ordinary_members)
            return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) {
            return (inplace_lifetime_member<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

template<class T> consteval bool contains_owning_head();
// Per member, as a function template: a lambda nested in the pack expansion
// crashes Clang 18, and consteval calls from a lambda need P2564 (C++23).
template<class T, std::size_t I>
consteval bool contains_owning_head_member() {
    return contains_owning_head<typename schema::member_t<T, I>::type>();
}

// T holds a container head inline (itself, an array element or a member,
// recursively): its lifetime is not implicit and is restarted head by head.
template<class T> consteval bool contains_owning_head() {
    if constexpr (fixed_detail::is_owning_head<T>::value) return true;
    else if constexpr (std::is_array_v<T>)
        return contains_owning_head<std::remove_extent_t<T>>();
    else if constexpr (std::is_class_v<T>) {
        return []<std::size_t... I>(std::index_sequence<I...>) consteval {
            return (contains_owning_head_member<T, I>() || ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

// Restarts the inline heads of one T whose containing array or ordinary outer
// was restarted; the head bytes are kept. Out-of-line children stay live and
// are not reached.
template<class T>
void restart_inline_heads(char* address) noexcept {
    static_assert(inplace_lifetime_supported<T>());
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        static_assert(owning_head_layout<T>());
        OwningHeadBytes saved;
        std::memcpy(&saved, address, sizeof(saved));
        auto* object = ::new (static_cast<void*>(address)) T;
        object->data_delta=saved.data_delta;
        object->size_=saved.size; object->capacity_=saved.capacity;
    } else if constexpr (std::is_array_v<T>) {
        using E=std::remove_extent_t<T>;
        for(std::size_t i=0;i<std::extent_v<T>;++i)
            restart_inline_heads<E>(address+i*sizeof(E));
    } else if constexpr (std::is_class_v<T>) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M=typename member::type;
            restart_inline_heads<M>(address+member::offset);
        });
    }
}

// Caller has established the complete array object at this same address, kept
// its representation, and completed every potentially throwing preparation.
// Only INLINE heads are restarted; still-live out-of-line children are untouched.
template<class T> requires(inplace_lifetime_supported<T>())
void restart_inline_prefix(T* data, std::size_t live_count) noexcept {
    if constexpr (contains_owning_head<T>())
        for (std::size_t i = 0; i < live_count; ++i)
            restart_inline_heads<T>(reinterpret_cast<char*>(data + i));
}

// Private preconditions are established BEFORE the allocator call at every
// entry: checked nonzero element count, multiplication fits the actual checked
// allocation byte count, alignment and entire storage extent. No allocator,
// callback, value initialization, dirty-spare typed read or retirement here.
template<class T>
T* begin_dynamic_array(void* storage, std::size_t capacity) noexcept {
    if (capacity == 0) return nullptr;
    assert(storage && capacity <= (std::numeric_limits<std::size_t>::max)() / sizeof(T));
    assert(reinterpret_cast<std::uintptr_t>(storage) % alignof(T) == 0);
    // An explicit nonallocating allocation-function invocation implicitly
    // creates the ARRAY, even when its element is nonimplicit-lifetime.
    // Unknown-bound array pointer conversion/decay preserves that array
    // provenance instead of assuming void* directly names a live first T.
    auto* first = *static_cast<T(*)[]>(::operator new(capacity * sizeof(T), storage));
    (void)(first + capacity); // require the whole capacity, including spare slots
    return first;
}

template<class T>
T* rebuild_dynamic_array(void* storage, std::size_t capacity,
                         std::uint32_t live_size) noexcept {
    if (capacity == 0) return nullptr;
    assert(storage && live_size <= capacity &&
           capacity <= (std::numeric_limits<std::size_t>::max)() / sizeof(T));
    assert(reinterpret_cast<std::uintptr_t>(storage) % alignof(T) == 0);
    if constexpr (inplace_lifetime_supported<T>()) {
        // memmove's temporary character copy preserves the live representation
        // around implicit object creation. This is NOT start_lifetime_as_array:
        // no bit_cast-style determination of dirty spare bool/enum values.
        auto* first = *static_cast<T(*)[]>(std::memmove(storage, storage, capacity * sizeof(T)));
        (void)(first + capacity);
        // Only the old LIVE prefix's inline nonimplicit heads are restarted.
        // Out-of-line objects remain live and are not reconstructed/scanned.
        if constexpr (!std::is_scalar_v<T>)
            restart_inline_prefix<T>(first, live_size);
        return first;
    } else {
        // Types outside the domain keep the storage address as it is.
        return static_cast<T*>(storage);
    }
}

template<class T>
consteval bool ordinary_outer_lifetime_supported() {
    if constexpr (std::is_class_v<T> && !fixed_detail::is_owning_head<T>::value &&
                  std::is_aggregate_v<T> && wire_record_layout<T>::ordinary_members)
        return implicit_lifetime_record<T>() && inplace_lifetime_supported<T>();
    else return false;
}

template<class T>
T* begin_fresh_ordinary_outer(void* zeroed) noexcept {
    if constexpr (ordinary_outer_lifetime_supported<T>()) {
        // The original caller already zeroed this whole fresh/dead slot.
        // A supported implicit ordinary T is actually created before forming
        // member addresses. Nonimplicit heads still use the original walk.
        return static_cast<T*>(::operator new(sizeof(T), zeroed));
    } else return static_cast<T*>(zeroed);
}

template<class T>
T* begin_relocated_ordinary_outer(T* destination, const T& source) noexcept {
    if constexpr (ordinary_outer_lifetime_supported<T>()) {
        void* storage = destination;
        // Disjoint live source -> dead slot, only sizeof(T) INLINE bytes.
        // The following original walk rebases heads and empties source owns.
        return static_cast<T*>(std::memcpy(storage, &source, sizeof(T)));
    } else return destination;
}

// SHIFT: XVectorCore erase/insert move a contiguous run of live elements by
// one slot. The caller memmoves the run, which supplies implicit object
// creation of the ordinary outers and scalars as in rebuild_dynamic_array.
// Only the INLINE heads of the moved run are then restarted, with every
// self-relative delta rebased by the known displacement. Out-of-line payloads
// keep their addresses and are neither scanned nor restarted.
template<class T> consteval bool shift_rebase_domain();
// Per member, as a function template: a lambda nested in the pack expansion
// crashes Clang 18, and consteval calls from a lambda need P2564 (C++23).
template<class T, std::size_t I>
consteval bool shift_rebase_domain_member() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else return shift_rebase_domain<typename member::type>();
}

template<class T>
consteval bool shift_rebase_domain() {
    if constexpr (fixed_detail::is_owning_head<T>::value) return owning_head_layout<T>();
    else if constexpr (std::is_array_v<T>) return shift_rebase_domain<std::remove_extent_t<T>>();
    else if constexpr (std::is_scalar_v<T>) return true;
    else if constexpr (std::is_class_v<T>) {
        if constexpr (schema::base_count_v<T> != 0) return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) consteval {
            return (shift_rebase_domain_member<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

template<class T>
consteval bool shift_relocate_supported() {
    if constexpr (std::is_trivially_copyable_v<T> ||
                  !fixed_detail::can_relocate_without_allocation<T>()) return false;
    else if constexpr (!inplace_lifetime_supported<T>() || !contains_owning_head<T>()) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) return shift_rebase_domain<T>();
    else return ordinary_outer_lifetime_supported<T>() && shift_rebase_domain<T>();
}

// Heads live in one bounded arena, so a rebased delta stays in rel32 range;
// an impossible overflow ends the program as set_rel32 in a noexcept relocation does.
// A null delta stays null; otherwise the exact int32 sum, without a branch on
// null (heads mix bound and empty containers). One moved run shares one shift.
// Clang vectorizes the run when the overflow test is a compare against fixed
// bounds (a non-null delta outside [low, high] would overflow), collected and
// checked once after the run, so the wrapped sums are never used. GCC and MSVC
// keep scalar code, where a checked add per delta is shorter.
#if defined(__clang__)
#define XOFFSET_DETAIL_SHIFT_RUN_BOUNDS 1
#else
#define XOFFSET_DETAIL_SHIFT_RUN_BOUNDS 0
#endif

struct ShiftRun {
    std::int32_t shift;
#if XOFFSET_DETAIL_SHIFT_RUN_BOUNDS
    std::int32_t low;
    std::int32_t high;
    bool overflow = false;

    explicit ShiftRun(std::int32_t by) noexcept
        : shift(by),
          low(by < 0 ? (std::numeric_limits<std::int32_t>::min)() - by : (std::numeric_limits<std::int32_t>::min)()),
          high(by > 0 ? (std::numeric_limits<std::int32_t>::max)() - by : (std::numeric_limits<std::int32_t>::max)()) {}
#else
    explicit ShiftRun(std::int32_t by) noexcept : shift(by) {}
#endif
};

inline std::int32_t shift_rel32(std::int32_t delta, ShiftRun& run) noexcept {
#if XOFFSET_DETAIL_SHIFT_RUN_BOUNDS
    // 0 lies in [low, high] for every shift.
    run.overflow |= (delta < run.low) | (delta > run.high);
    const std::uint32_t step = delta != 0 ? static_cast<std::uint32_t>(run.shift) : 0u;
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(delta) + step);
#else
    const std::int32_t step = delta != 0 ? run.shift : 0;
    std::int32_t moved;
#if defined(__GNUC__)
    if (__builtin_add_overflow(delta, step, &moved)) [[unlikely]] std::terminate();
#else
    const std::int64_t wide = std::int64_t(delta) + step;
    if (wide < (std::numeric_limits<std::int32_t>::min)() ||
        wide > (std::numeric_limits<std::int32_t>::max)()) [[unlikely]] std::terminate();
    moved = static_cast<std::int32_t>(wide);
#endif
    return moved;
#endif
}

template<class T, std::size_t... I>
XOFFSET_DETAIL_INLINE void shift_rebase_members(char* address, ShiftRun& run,
                                                    std::index_sequence<I...>) noexcept;

// `shift` = old address - new address, so each delta grows by it. Forced
// inline down to the heads: one moved element is a few loads and stores.
template<class T>
XOFFSET_DETAIL_INLINE void shift_rebase_inline_value(char* address, ShiftRun& run) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        // Descriptor layout {data_delta, size_, capacity_} is checked by
        // owning_head_layout<T>() in shift_rebase_domain.
        std::int32_t data_delta;
        std::uint32_t size, capacity;
        std::memcpy(&data_delta, address, 4);
        std::memcpy(&size, address + 4, 4);
        std::memcpy(&capacity, address + 8, 4);
        bool inline_bytes = false;
        if constexpr (is_fixed_string_v<T>) inline_bytes = (capacity & XString::inline_flag) != 0;
        // The checked sum comes first: with no call between the new head and
        // its three stores, the default member initializers' zero stores are dead.
        // Inline string bytes are characters, not a delta: kept as they are.
        const std::int32_t moved_data = inline_bytes ? data_delta : shift_rel32(data_delta, run);
        auto* object = ::new (static_cast<void*>(address)) T;
        object->data_delta = moved_data;
        object->size_ = size;
        object->capacity_ = capacity;
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (contains_owning_head<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                shift_rebase_inline_value<E>(address + i * sizeof(E), run);
    } else if constexpr (std::is_class_v<T> && contains_owning_head<T>()) {
        shift_rebase_members<T>(address, run, std::make_index_sequence<schema::member_count_v<T>>{});
    }
}

template<class T, std::size_t I>
XOFFSET_DETAIL_INLINE void shift_rebase_member(char* address, ShiftRun& run) noexcept {
    using member = schema::member_t<T, I>;
    using M = typename member::type;
    if constexpr (contains_owning_head<M>())
        shift_rebase_inline_value<M>(address + member::offset, run);
}

template<class T, std::size_t... I>
XOFFSET_DETAIL_INLINE void shift_rebase_members(char* address, ShiftRun& run,
                                                    std::index_sequence<I...>) noexcept {
    (shift_rebase_member<T, I>(address, run), ...);
}

// Forced inline where the run vectorizes: a one-element insert or erase then
// shifts by a constant.
template<class T>
#if XOFFSET_DETAIL_SHIFT_RUN_BOUNDS
XOFFSET_DETAIL_INLINE
#endif
void shift_rebase_moved(T* first, std::size_t count, std::int32_t shift) noexcept {
    static_assert(shift_relocate_supported<T>());
    ShiftRun run(shift);
    for (std::size_t i = 0; i < count; ++i)
        shift_rebase_inline_value<T>(reinterpret_cast<char*>(first + i), run);
#if XOFFSET_DETAIL_SHIFT_RUN_BOUNDS
    if (run.overflow) [[unlikely]] std::terminate();
#endif
}

// The live slot a shift left behind still names the moved payloads. Empty its
// inline heads exactly as a relocated-from source is emptied (binding kept),
// so the caller's destroy_at releases nothing and spare bytes match the
// per-element path.
template<class T>
void empty_vacated_slot(T* slot) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        slot->data_delta = 0;
        slot->size_ = 0;
        slot->capacity_ = 0;
    } else if constexpr (std::is_array_v<T>) {
        for (auto& element : *slot) empty_vacated_slot(&element);
    } else if constexpr (std::is_class_v<T> && contains_owning_head<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (contains_owning_head<M>())
                empty_vacated_slot(&member::get(*slot));
        });
    }
}

// A destroyed element's container heads release their blocks and empty
// themselves, but those stores precede the end of the element's lifetime, so
// the optimizer may drop them (GCC's lifetime dead store elimination does,
// depending on inlining) and the spare bytes a save emits would differ
// between builds of one program (test_cross_compiler_wire,
// tools/no_exceptions_differential.sh). XVectorCore empties them again with
// empty_vacated_slot once the lifetime has ended (pop_back, scrub): heads
// empty, every other byte as the element left it.
template<class T>
consteval bool settles_vacated_heads() {
    if constexpr (std::is_trivially_destructible_v<T> || !inplace_lifetime_supported<T>()) return false;
    else return contains_owning_head<T>();
}


// Subtree copy: copy assignment of a container copies its source subtree
// (its block plus every block reachable from its live elements) into one
// fresh segment: one memcpy plus a fix-up walk when the source tiles one byte
// range, otherwise a compacting preorder walk. The domain is the SHIFT domain
// extended into container elements: plain heads, scalars, arrays and ordinary
// records, every alignment <= 8.
template<class T> consteval bool subtree_copy_domain();
// Per member, as a function template: a lambda nested in the pack expansion
// crashes Clang 18, and consteval calls from a lambda need P2564 (C++23).
template<class T, std::size_t I>
consteval bool subtree_copy_domain_member() {
    using member = schema::member_t<T, I>;
    if constexpr (member::is_bit_field) return false;
    else return subtree_copy_domain<typename member::type>();
}

template<class T>
consteval bool subtree_copy_domain() {
    if constexpr (alignof(T) > 8) return false;
    else if constexpr (fixed_detail::is_owning_head<T>::value) {
        if constexpr (!owning_head_layout<T>()) return false;
        else if constexpr (is_fixed_string_v<T>) return true;
        else return subtree_copy_domain<typename T::value_type>();
    } else if constexpr (std::is_array_v<T>) return subtree_copy_domain<std::remove_extent_t<T>>();
    else if constexpr (std::is_scalar_v<T>) return true;
    else if constexpr (std::is_class_v<T>) {
        if constexpr (schema::base_count_v<T> != 0) return false;
        else if constexpr (!std::is_trivially_copyable_v<T> &&
                           !ordinary_outer_lifetime_supported<T>()) return false;
        else return []<std::size_t... I>(std::index_sequence<I...>) consteval {
            return (subtree_copy_domain_member<T, I>() && ...);
        }(std::make_index_sequence<schema::member_count_v<T>>{});
    } else return false;
}

// T is a container element type. A trivially copyable T takes the range copy
// (assign_range plus padding zeroing); others need the subtree domain.
template<class T>
consteval bool subtree_copy_supported() {
    if constexpr (std::is_trivially_copyable_v<T>)
        return !std::is_const_v<T> && !std::is_volatile_v<T> &&
               std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>;
    else return subtree_copy_domain<T>();
}

// Blocks of one subtree. Live blocks of a valid arena are disjoint, so
// hi - lo == sum exactly when they tile [lo, hi) with no hole or foreign block.
struct SubtreeExtent {
    std::uintptr_t lo = (std::numeric_limits<std::uintptr_t>::max)();
    std::uintptr_t hi = 0;
    std::uint64_t sum = 0;    // block bytes
    std::uint64_t exact = 0;  // block bytes an element-wise copy would allocate

    void add(const void* block, std::uint32_t bytes) noexcept {
        const auto at = reinterpret_cast<std::uintptr_t>(block);
        lo = (std::min)(lo, at);
        hi = (std::max)(hi, at + bytes);
        sum += bytes;
    }
    bool contiguous() const noexcept {
        // Every block offset is 8-aligned, so the copy keeps each alignment.
        return sum != 0 && hi - lo == sum && (lo & 7u) == 0 &&
               sum <= XBufferCore::MAX_PAYLOAD;
    }
};

template<class T> void subtree_measure_value(const T& value, SubtreeExtent& extent) noexcept;

template<class E>
void subtree_measure_elements(const E* data, std::uint32_t size, std::uint32_t capacity,
                              SubtreeExtent& extent) noexcept {
    if (!data || capacity == 0) return;
    extent.add(data, fixed_detail::block_bytes_of(static_cast<std::uint32_t>(capacity * sizeof(E))));
    if (size != 0) extent.exact += fixed_detail::block_bytes_of(static_cast<std::uint32_t>(size * sizeof(E)));
    if constexpr (contains_owning_head<E>())
        for (std::uint32_t i = 0; i < size; ++i) subtree_measure_value<E>(data[i], extent);
}

template<class T>
void subtree_measure_value(const T& value, SubtreeExtent& extent) noexcept {
    if constexpr (is_fixed_string_v<T>) {
        const XString& head = value;
        if ((head.capacity_ & XString::inline_flag) != 0 || head.capacity_ == 0) return;
        const auto* data = fixed_detail::resolve_rel32<const char>(&head.data_delta, head.data_delta);
        if (!data) return;
        extent.add(data, fixed_detail::block_bytes_of(head.capacity_ + 1u));
        if (head.size_ > XString::inline_max) extent.exact += fixed_detail::block_bytes_of(head.size_ + 1u);
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        using E = typename T::value_type;
        const XVectorCore<E>& head = value;
        subtree_measure_elements<E>(head.data(), head.size_, head.capacity_, extent);
    } else if constexpr (std::is_array_v<T>) {
        for (const auto& element : value) subtree_measure_value(element, extent);
    } else if constexpr (std::is_class_v<T> && contains_owning_head<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (contains_owning_head<M>()) subtree_measure_value<M>(member::get(value), extent);
        });
    }
}

// Heads live in one bounded arena, so a rebound delta stays in rel32 range;
// an impossible overflow ends the program as set_rel32 in a noexcept walk does.
inline std::int32_t subtree_rel32(const void* origin, const void* target) noexcept {
    const std::int64_t delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) -
                               static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(origin));
    if (delta < (std::numeric_limits<std::int32_t>::min)() ||
        delta > (std::numeric_limits<std::int32_t>::max)()) [[unlikely]] std::terminate();
    return static_cast<std::int32_t>(delta);
}

template<class T> void subtree_fix_value(char* address) noexcept;

// The memcpy created the arrays, ordinary outers and scalars of the copy.
// Restart each live head (nonimplicit lifetime); data_delta moves with its
// block and is kept, and the head belongs to the destination by its address.
// Live elements get the M6 padding zeroing of the element-wise path.
template<class E>
void subtree_fix_elements(char* data, std::uint32_t size) noexcept {
    if constexpr (contains_owning_head<E>() || padding_ranges<E>.size() != 0) {
        for (std::uint32_t i = 0; i < size; ++i) {
            char* element = data + std::size_t(i) * sizeof(E);
            if constexpr (contains_owning_head<E>()) subtree_fix_value<E>(element);
            if constexpr (padding_ranges<E>.size() != 0)
                scrub_implicit_padding(std::launder(reinterpret_cast<E*>(element)));
        }
    }
}

template<class T>
void subtree_fix_value(char* address) noexcept {
    if constexpr (fixed_detail::is_owning_head<T>::value) {
        // Descriptor layout is checked by owning_head_layout<T>().
        std::int32_t data_delta;
        std::uint32_t size, capacity;
        std::memcpy(&data_delta, address, 4);
        std::memcpy(&size, address + 4, 4);
        std::memcpy(&capacity, address + 8, 4);
        auto* object = ::new (static_cast<void*>(address)) T;
        object->data_delta = data_delta;
        object->size_ = size;
        object->capacity_ = capacity;
        if constexpr (!is_fixed_string_v<T>) {
            if (data_delta != 0 && capacity != 0)
                subtree_fix_elements<typename T::value_type>(address + data_delta, size);
        }
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (contains_owning_head<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                subtree_fix_value<E>(address + i * sizeof(E));
    } else if constexpr (std::is_class_v<T> && contains_owning_head<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (contains_owning_head<M>())
                subtree_fix_value<M>(address + member::offset);
        });
    }
}

// Releases replaced content. A contiguous old subtree is retired as one range
// (one hole the next same-shape copy reuses, so a reuse loop alternates two
// ranges instead of growing); its objects end as their storage is reused and
// their destructors would only retire the same blocks. Otherwise block by block.
template<class T>
void subtree_release(fixed_detail::allocator_ref alloc, T* old, std::uint32_t size,
                     std::uint32_t capacity) noexcept {
    if (!old || capacity == 0) return;
    SubtreeExtent extent;
    subtree_measure_elements<T>(old, size, capacity, extent);
    if (extent.contiguous()) {
        fixed_detail::retire_dynamic_bytes(alloc, reinterpret_cast<void*>(extent.lo),
                                           static_cast<std::uint32_t>(extent.sum));
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_retires, 1);
    } else {
        fixed_detail::destroy_n(old, size);
        fixed_detail::retire_dynamic_bytes(alloc, old, static_cast<std::uint32_t>(capacity * sizeof(T)));
        XOFFSET_DETAIL_SUBTREE_COUNT(piecewise_retires, 1);
    }
}

// COMPACT: copies the source subtree in preorder into one fresh segment of
// SubtreeExtent::exact bytes, whatever the source layout. Each block holds
// only its live prefix (capacity = size), strings of at most inline_max bytes
// become inline, and empty containers own no block: the element-wise copy's
// shape, packed with no holes. Block tails are not cleared: the segment is a
// fresh tail or a hole zeroed on retirement (M3); a hole a loaded wire
// brought in, or a tail that a reset without XOFFSET_SCRUB_RESET left, can
// hold other bytes, so string terminators are written.
template<class E>
char* subtree_compact_elements(const E* source, std::uint32_t size,
                               char*& cursor) noexcept;

template<class T>
void subtree_compact_value(char* address, const T& source, char*& cursor) noexcept {
    if constexpr (is_fixed_string_v<T>) {
        const XString& from = source;
        auto* head = ::new (static_cast<void*>(address)) T;
        const auto view = from.view();
        const auto size = static_cast<std::uint32_t>(view.size());
        if (size == 0) {
            head->data_delta = 0;
            head->size_ = 0;
            head->capacity_ = 0;
        } else if (size <= XString::inline_max) {
            // Inline layout of store_inline: bytes 0..10 characters then
            // zeros, byte 11 = 0x80 | length.
            char bytes[12] = {};
            std::memcpy(bytes, view.data(), size);
            bytes[11] = static_cast<char>(0x80u | size);
            std::memcpy(address, bytes, sizeof(bytes));
        } else {
            char* block = cursor;
            cursor += fixed_detail::block_bytes_of(size + 1u);
            std::memcpy(block, view.data(), size);
            // A hole a loaded wire brought in may hold any bytes: the
            // terminator is written, not assumed.
            block[size] = '\0';
            head->data_delta = subtree_rel32(&head->data_delta, block);
            head->size_ = size;
            head->capacity_ = size;
        }
    } else if constexpr (fixed_detail::is_owning_head<T>::value) {
        using E = typename T::value_type;
        const XVectorCore<E>& from = source;
        const auto size = from.size_;
        auto* head = ::new (static_cast<void*>(address)) T;
        if (size == 0) {
            head->data_delta = 0;
            head->size_ = 0;
            head->capacity_ = 0;
        } else {
            char* block = subtree_compact_elements<E>(from.data(), size, cursor);
            head->data_delta = subtree_rel32(&head->data_delta, block);
            head->size_ = size;
            head->capacity_ = size;
        }
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (contains_owning_head<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                subtree_compact_value<E>(address + i * sizeof(E), source[i], cursor);
    } else if constexpr (std::is_class_v<T> && contains_owning_head<T>()) {
        static_for<schema::member_count_v<T>>([&](auto index) {
            using member = schema::member_t<T, decltype(index)::value>;
            using M = typename member::type;
            if constexpr (contains_owning_head<M>())
                subtree_compact_value<M>(address + member::offset,
                                         member::get(source), cursor);
        });
    }
}

// The memcpy of the live prefix creates the array, ordinary outers and
// scalars; heads are then restarted and pointed at their compacted children.
template<class E>
char* subtree_compact_elements(const E* source, std::uint32_t size,
                               char*& cursor) noexcept {
    char* block = cursor;
    const auto live = static_cast<std::uint32_t>(size * sizeof(E));
    cursor += fixed_detail::block_bytes_of(live);
    std::memcpy(block, static_cast<const void*>(source), live);
    if constexpr (contains_owning_head<E>() || padding_ranges<E>.size() != 0) {
        for (std::uint32_t i = 0; i < size; ++i) {
            char* element = block + std::size_t(i) * sizeof(E);
            if constexpr (contains_owning_head<E>()) subtree_compact_value<E>(element, source[i], cursor);
            if constexpr (padding_ranges<E>.size() != 0)
                scrub_implicit_padding(std::launder(reinterpret_cast<E*>(element)));
        }
    }
    return block;
}

// Returns false, with nothing changed, only when the new segment does not fit
// (XBadAlloc); the caller then copies element-wise, which may still fit in
// smaller holes or fails exactly as it did before this path existed. A source
// whose blocks tile one range with bounded spare bytes is copied whole (one
// memcpy plus a fix-up walk, source capacities kept); any other source is
// compacted. Strong guarantee: past the allocation only an impossible rel32
// overflow can throw, and it retires the fresh segment first. The source is
// fully copied before the destination is published or its old content
// released, so the source may live inside the destination's old content.
template<class T>
bool subtree_copy_assign(XVectorCore<T>& dst, const XVectorCore<T>& src) {
    static_assert(subtree_copy_supported<T>() && !std::is_trivially_copyable_v<T>);
    XArena* const arena = dst.arena();
    if (!arena || src.size_ == 0) return false;
    SubtreeExtent extent;
    subtree_measure_elements<T>(src.data(), src.size_, src.capacity_, extent);
    bool whole = extent.contiguous();
    if (!whole) {
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_skipped_layout, 1);
    } else if (extent.sum - extent.exact > (std::max<std::uint64_t>)(64u, extent.sum / 4u)) {
        // Spare bytes a whole copy would carry beyond the compacted shape.
        whole = false;
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_skipped_slack, 1);
    }
    const auto total = whole ? extent.sum : extent.exact;
    if (total > XBufferCore::MAX_PAYLOAD) return false;
    const auto bytes = static_cast<std::uint32_t>(total);
    const typename XVectorCore<T>::allocator_type alloc{arena};
    const auto size = src.size_;
    char* fresh = nullptr;
    XOFFSET_TRY {
        fresh = static_cast<char*>(fixed_detail::allocate_dynamic_bytes(alloc, bytes, 8));
    } XOFFSET_CATCH(const XBadAlloc&) {
        XOFFSET_DETAIL_SUBTREE_COUNT(fallback_alloc, 1);
        return false;
    }
    char* top;
    std::uint32_t capacity;
    if (whole) {
        const auto* first = reinterpret_cast<const char*>(extent.lo);
        std::memcpy(fresh, first, bytes);
        top = fresh + (reinterpret_cast<const char*>(src.data()) - first);
        capacity = src.capacity_;
        subtree_fix_elements<T>(top, size);
        XOFFSET_DETAIL_SUBTREE_COUNT(whole_hits, 1);
    } else {
        char* cursor = fresh;
        top = subtree_compact_elements<T>(src.data(), size, cursor);
        capacity = size;
        if (cursor != fresh + bytes) [[unlikely]] std::terminate();  // measure/compact disagree
        XOFFSET_DETAIL_SUBTREE_COUNT(compact_hits, 1);
    }
    std::int32_t delta = 0;
    XOFFSET_TRY {
        fixed_detail::set_rel32(delta, &dst.data_delta, top);
    } XOFFSET_CATCH(...) {
        fixed_detail::retire_dynamic_bytes(alloc, fresh, bytes);
        XOFFSET_RETHROW;
    }
    T* old = dst.data();
    const auto old_size = dst.size_;
    const auto old_capacity = dst.capacity_;
    dst.data_delta = delta;
    dst.size_ = size;
    dst.capacity_ = capacity;
    subtree_release<T>(alloc, old, old_size, old_capacity);
    XOFFSET_DETAIL_SUBTREE_COUNT(bytes, bytes);
    return true;
}

} // namespace XOffsetDatastructure::detail

