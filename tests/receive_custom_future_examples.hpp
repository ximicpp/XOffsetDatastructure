#pragma once
// Concrete source reference for the v2 authors. Not compiled/executed by this
// reviewer. The existing V1/TypeLayout admission remains an independent gate.
#include "receive_raw.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace receive_custom_v2_examples {
namespace xo = XOffsetDatastructure;

// User-provided constructor AND destructor: this must not be certified by an
// ordinary implicit-lifetime fallback. No padding / invalid representations.
struct Word {
    std::uint32_t value;
    explicit Word(std::uint32_t x) noexcept : value(x) {}
    ~Word() noexcept {} // native destruction, once per live object
};
static_assert(sizeof(Word) == sizeof(std::uint32_t));
static_assert(alignof(Word) == alignof(std::uint32_t));
static_assert(std::is_standard_layout_v<Word>);

// A second positive case has a real out-of-line ownership edge. Raw activation
// and same-address inline reactivation must preserve its payload address/bytes.
struct Owned {
    struct empty_t {};
    struct descriptor_t {};
    xo::XString text;
    std::uint64_t value;

    explicit Owned(empty_t) noexcept : text(), value(0) {}
    explicit Owned(const xo::fixed_detail::allocator_ref& alloc)
        : text(alloc), value(0) {}
    Owned(const Owned& src, const xo::fixed_detail::allocator_ref& alloc)
        : text(src.text, alloc), value(src.value) {}
    // Same ADDRESS only: the already-validated relative integers are unchanged.
    Owned(descriptor_t, const xo::detail::RawOwningDescriptor& h,
          std::uint64_t n) noexcept : text(), value(n) {
        text.arena_delta=h.arena_delta; text.data_delta=h.data_delta;
        text.size_=h.size; text.capacity_=h.capacity;
    }
    ~Owned() noexcept {} // text is destroyed automatically after the body
};
static_assert(std::is_standard_layout_v<Owned>);
static_assert(offsetof(Owned, text) == 0 && offsetof(Owned, value) == 16);
static_assert(sizeof(Owned) == 24 && alignof(Owned) == 8);

// Negative surfaces. They deliberately remain well-formed source declarations.
struct RawOnly { std::uint32_t value; ~RawOnly() noexcept {} };
struct MissingRvalue { std::uint32_t value; ~MissingRvalue() noexcept {} };
struct ThrowingDestructor { std::uint32_t value; ~ThrowingDestructor() noexcept(false) {} };
struct MissingInline { std::uint32_t value; ~MissingInline() noexcept {} };
struct HiddenRawOnly { xo::XVector<RawOnly> later; };
struct HiddenByPolicy { xo::XVector<RawOnly> later; };
struct FutureOnlyChild { std::uint32_t value{}; };
} // namespace receive_custom_v2_examples
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::RawOnly, (), value)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::MissingRvalue, (), value)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::ThrowingDestructor, (), value)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::MissingInline, (), value)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::HiddenRawOnly, (), later)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::HiddenByPolicy, (), later)
XOFFSET_RECORD_DERIVED(receive_custom_v2_examples::FutureOnlyChild, (), value)
namespace receive_custom_v2_examples {
inline thread_local long transfer_failure=-1;
inline void check_transfer_failure() {
    if(transfer_failure==0){transfer_failure=-1;throw std::bad_alloc();}
    if(transfer_failure>0)--transfer_failure;
}
// An empty HiddenRawOnly::later is still future-negative: domain checking is
// over types, not only the values present in the first received fixture.
} // namespace receive_custom_v2_examples

// Existing explicit TypeLayout user extension, local to these policy examples.
// It does not bypass the separate production V1 recursive member/layout gate.
namespace XOffsetDatastructure {
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::Word> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::Owned> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::RawOnly> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::MissingRvalue> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::ThrowingDestructor> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::MissingInline> : std::true_type {};
template<> struct is_byte_copy_safe<::receive_custom_v2_examples::HiddenByPolicy> : std::true_type {};
}

namespace XOffsetDatastructure {
namespace example = ::receive_custom_v2_examples;

template<> struct wire_activation<example::Word> {
    static constexpr bool enabled=true;
    static void validate_raw(const char*, detail::WireValidationContext&) {
        // Every uint32 representation is valid; the caller has checked this
        // complete object's range and ownership before invoking the policy.
    }
    static void activate(char* p) noexcept {
        std::uint32_t saved; std::memcpy(&saved,p,sizeof(saved));
        ::new (static_cast<void*>(p)) example::Word(saved);
    }
    static void validate_raw(const char* p,detail::WireValidationContext& ctx,
                             const detail::CanonicalRawBytes*) { validate_raw(p,ctx); }
    static void activate(char* p,detail::RawActivationStats&,
                         const detail::CanonicalRawBytes*) noexcept { activate(p); }
};
template<> struct wire_future_lifetime<example::Word> {
    static constexpr bool enabled=true;
    using children=wire_future_children<std::uint32_t>;
    static void default_construct(void* p,const fixed_detail::allocator_ref&) {
        ::new (p) example::Word(0);
    }
    static void transfer_construct(void* p,const example::Word& s,
                                   const fixed_detail::allocator_ref&) {
        example::check_transfer_failure();
        ::new (p) example::Word(s.value);
    }
    static void transfer_construct(void* p,example::Word&& s,
                                   const fixed_detail::allocator_ref& a) {
        // Deliberately non-destructive: repeated fill/rollback must not acquire
        // destructive move semantics merely because this overload exists.
        transfer_construct(p,static_cast<const example::Word&>(s),a);
    }
    static void relocate(example::Word* p,example::Word& s) noexcept {
        ::new (static_cast<void*>(p)) example::Word(s.value);
        // Source remains live. Caller, not this hook, destroys it exactly once.
    }
    static void reactivate_inline(void* p,const detail::CanonicalRawBytes*) noexcept {
        wire_activation<example::Word>::activate(static_cast<char*>(p));
    }
};

template<> struct wire_activation<example::Owned> {
    static constexpr bool enabled=true;
    static void validate_raw(const char* p, detail::WireValidationContext& ctx) {
        validate_raw(p,ctx,nullptr);
    }
    static void validate_raw(const char* p,detail::WireValidationContext& ctx,
                             const detail::CanonicalRawBytes* bytes) {
        detail::validate_wire_value_raw<XString>(p,ctx,bytes);
    }
    static void restore_inline(char* p,const detail::CanonicalRawBytes* bytes) noexcept {
        auto h=detail::raw_wire_pod<detail::RawOwningDescriptor>(p);
        auto n=detail::raw_wire_pod<std::uint64_t>(detail::raw_advance(p,16,bytes));
        ::new (static_cast<void*>(p)) example::Owned(example::Owned::descriptor_t{},h,n);
    }
    static void activate(char* p,detail::RawActivationStats& stats,
                         const detail::CanonicalRawBytes* bytes) noexcept {
        restore_inline(p,bytes);
        // Initial activation has a graph obligation. Recover the child's
        // parent char array and metadata using the SAME canonical domain.
        detail::activate_wire_value_raw<XString>(p,stats,bytes);
    }
    static void activate(char* p) noexcept {
        detail::RawActivationStats ignored;
        activate(p,ignored,nullptr); // legacy standalone raw-position domain
    }
};
template<> struct wire_future_lifetime<example::Owned> {
    static constexpr bool enabled=true;
    using children=wire_future_children<XString,std::uint64_t>;
    static void default_construct(void* p,const fixed_detail::allocator_ref& a) {
        ::new (p) example::Owned(a);
    }
    static void transfer_construct(void* p,const example::Owned& s,
                                   const fixed_detail::allocator_ref& a) {
        example::check_transfer_failure();
        ::new (p) example::Owned(s,a);
    }
    static void transfer_construct(void* p,example::Owned&& s,
                                   const fixed_detail::allocator_ref& a) {
        transfer_construct(p,static_cast<const example::Owned&>(s),a);
    }
    static void relocate(example::Owned* p,example::Owned& s) noexcept {
        // Same bounded arena, disjoint destination, all throwing preparation
        // complete. Start whole T first, then replace its empty live head.
        ::new (static_cast<void*>(p)) example::Owned(example::Owned::empty_t{});
        std::destroy_at(&p->text); // empty, no payload retirement
        fixed_detail::relocate_construct(&p->text,s.text);
        p->value=s.value;
        // src.text is live and empty; native src destructor remains legal.
    }
    static void reactivate_inline(void* p,const detail::CanonicalRawBytes* bytes) noexcept {
        // Same address, after byte-preserving array rebegin. Do NOT activate
        // its still-live char payload or reconstruct any out-of-line objects.
        wire_activation<example::Owned>::restore_inline(static_cast<char*>(p),bytes);
    }
};

template<> struct wire_activation<example::RawOnly> {
    static constexpr bool enabled=true;
    static void validate_raw(const char*,detail::WireValidationContext&) {}
    static void activate(char* p) noexcept {
        auto n=detail::raw_wire_pod<std::uint32_t>(p);
        auto* result=::new(static_cast<void*>(p)) example::RawOnly;
        result->value=n;
    }
};
template<> struct wire_record_layout<example::HiddenRawOnly> {
    static constexpr bool ordinary_members=true;
};
template<class T> struct NegativeInitialSurface {
    static constexpr bool enabled=true;
    static void validate_raw(const char*,detail::WireValidationContext&);
    static void activate(char*) noexcept;
    static void validate_raw(const char*,detail::WireValidationContext&,const detail::CanonicalRawBytes*);
    static void activate(char*,detail::RawActivationStats&,const detail::CanonicalRawBytes*) noexcept;
};
template<class T> struct NegativeFutureSurface {
    static constexpr bool enabled=true;
    using children=wire_future_children<>;
    static void default_construct(void*,const fixed_detail::allocator_ref&);
    static void transfer_construct(void*,const T&,const fixed_detail::allocator_ref&);
    static void transfer_construct(void*,T&&,const fixed_detail::allocator_ref&);
    static void relocate(T*,T&) noexcept;
    static void reactivate_inline(void*,const detail::CanonicalRawBytes*) noexcept;
};
template<> struct wire_activation<example::HiddenByPolicy> : NegativeInitialSurface<example::HiddenByPolicy> {};
template<> struct wire_future_lifetime<example::HiddenByPolicy> : NegativeFutureSurface<example::HiddenByPolicy> {};
template<> struct wire_activation<example::FutureOnlyChild> : NegativeInitialSurface<example::FutureOnlyChild> {};
template<> struct wire_future_lifetime<example::FutureOnlyChild> : NegativeFutureSurface<example::FutureOnlyChild> {
    using children=wire_future_children<example::RawOnly>;
};
template<> struct wire_activation<XVector<example::RawOnly>> : NegativeInitialSurface<XVector<example::RawOnly>> {};
template<> struct wire_future_lifetime<XVector<example::RawOnly>> : NegativeFutureSurface<XVector<example::RawOnly>> {};
// No wire_future_lifetime<RawOnly>: standalone initial policy exists but mutable
// receive must reject RawOnly AND XVector<RawOnly> AND HiddenRawOnly.

// Other mechanical-negative specializations intentionally omit one obligation.
template<> struct wire_activation<example::MissingRvalue> : wire_activation<example::Word> {};
template<> struct wire_future_lifetime<example::MissingRvalue> {
    static constexpr bool enabled=true;
    using children=wire_future_children<std::uint32_t>;
    static void default_construct(void*,const fixed_detail::allocator_ref&);
    static void transfer_construct(void*,const example::MissingRvalue&,const fixed_detail::allocator_ref&);
    // const& can bind an rvalue, so the semantic requirement is a valid rvalue
    // expression; a deleted rvalue overload makes this negative unambiguous.
    static void transfer_construct(void*,example::MissingRvalue&&,const fixed_detail::allocator_ref&)=delete;
    static void relocate(example::MissingRvalue*,example::MissingRvalue&) noexcept;
    static void reactivate_inline(void*,const detail::CanonicalRawBytes*) noexcept;
};
template<> struct wire_activation<example::ThrowingDestructor> : wire_activation<example::Word> {};
template<> struct wire_future_lifetime<example::ThrowingDestructor> : wire_future_lifetime<example::Word> {};
template<> struct wire_activation<example::MissingInline> : wire_activation<example::Word> {};
template<> struct wire_future_lifetime<example::MissingInline> {
    static constexpr bool enabled=true;
    using children=wire_future_children<std::uint32_t>;
    static void default_construct(void*,const fixed_detail::allocator_ref&);
    static void transfer_construct(void*,const example::MissingInline&,const fixed_detail::allocator_ref&);
    static void transfer_construct(void*,example::MissingInline&&,const fixed_detail::allocator_ref&);
    static void relocate(example::MissingInline*,example::MissingInline&) noexcept;
    static void reactivate_inline(void*,const detail::CanonicalRawBytes*); // intentionally potentially throwing
};

static_assert(detail::custom_future_contract::surface_complete<example::Word>());
static_assert(detail::custom_future_contract::surface_complete<example::Owned>());
static_assert(!detail::custom_future_contract::surface_complete<example::RawOnly>());
static_assert(!detail::custom_future_contract::surface_complete<example::MissingRvalue>());
static_assert(!detail::custom_future_contract::surface_complete<example::ThrowingDestructor>());
static_assert(!detail::custom_future_contract::surface_complete<example::MissingInline>());
static_assert(detail::custom_future_contract::surface_complete<example::HiddenByPolicy>());
static_assert(detail::custom_future_contract::surface_complete<example::FutureOnlyChild>());
static_assert(detail::custom_future_contract::surface_complete<XVector<example::RawOnly>>());
} // namespace XOffsetDatastructure
