#include "xoffsetdatastructure.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
struct Root { xo::XVector<std::uint32_t> anchor; };
enum class ZeroEnum : unsigned { nonzero = 9 };
struct Member { int field; void function() {} };
enum class PolicyEnum : std::uint32_t { hook_default = 17, prefix = 99 };
enum class ActivationOnly : unsigned {};
enum class FutureOnly : unsigned {};
inline unsigned policy_defaults = 0, policy_relocations = 0;
namespace XOffsetDatastructure {
template<> struct wire_activation<ActivationOnly> { static constexpr bool enabled = true; };
template<> struct wire_future_lifetime<FutureOnly> { static constexpr bool enabled = true; };
template<> struct wire_activation<PolicyEnum> {
    static constexpr bool enabled = true;
    static void validate_raw(const char*, detail::WireValidationContext&) {}
    static void validate_raw(const char*, detail::WireValidationContext&,
                             const detail::CanonicalRawBytes*) {}
    static void activate(char* p) noexcept {
        PolicyEnum saved; std::memcpy(&saved, p, sizeof(saved));
        std::construct_at(reinterpret_cast<PolicyEnum*>(p), saved);
    }
    static void activate(char* p, detail::RawActivationStats&,
                         const detail::CanonicalRawBytes*) noexcept { activate(p); }
};
template<> struct wire_future_lifetime<PolicyEnum> {
    static constexpr bool enabled = true;
    using children = wire_future_children<>;
    static void default_construct(void* p, const fixed_detail::allocator_ref&) {
        ++policy_defaults;
        std::construct_at(static_cast<PolicyEnum*>(p), PolicyEnum::hook_default);
    }
    static void transfer_construct(void* p, const PolicyEnum& value,
                                   const fixed_detail::allocator_ref&) {
        std::construct_at(static_cast<PolicyEnum*>(p), value);
    }
    static void transfer_construct(void* p, PolicyEnum&& value,
                                   const fixed_detail::allocator_ref& alloc) {
        transfer_construct(p, static_cast<const PolicyEnum&>(value), alloc);
    }
    static void relocate(PolicyEnum* dst, PolicyEnum& src) noexcept {
        ++policy_relocations; std::construct_at(dst, src);
    }
    static void reactivate_inline(void* p, const detail::CanonicalRawBytes*) noexcept {
        wire_activation<PolicyEnum>::activate(static_cast<char*>(p));
    }
};
}
static_assert(xo::detail::future_custom_lifetime_supported<PolicyEnum>());
static_assert(!xo::detail::default_scalar_bulk_supported<PolicyEnum>());
static_assert(!xo::detail::default_scalar_bulk_supported<ActivationOnly>());
static_assert(!xo::detail::default_scalar_bulk_supported<FutureOnly>());
void need(bool b, const char* why) { if (!b) throw std::runtime_error(why); }

// These native scalar instantiations exercise the container's generic scalar
// contract. Pointers are deliberately NOT offered as a V1 wire schema.
template<class T> void paths(T initial) {
    auto owner = xo::XBuffer::create<Root>(1u << 20,
        xo::XBuffer::maximum_capacity(4u << 20).with_growth(xo::XGrowPolicy::Automatic));
    auto alloc = owner.root<Root>().anchor.binding();
    using V = xo::XVector<T>;
    auto* storage = fd::allocate_dynamic_bytes(alloc, sizeof(V), alignof(V));
    auto* v = std::construct_at(static_cast<V*>(storage), alloc);
    v->resize(3);
    for (const auto& x : *v) need(x == T{}, "initial fresh value initialization");
    (*v)[0] = initial;
    auto* tail = v->data();
    v->resize(11);
    need(v->data() == tail && (*v)[0] == initial, "tail prefix/address");
    for (unsigned i=3; i<11; ++i) need((*v)[i] == T{}, "tail default value");
    auto* blocker = fd::allocate_dynamic_bytes(alloc, 129, 8);
    v->resize(29);
    need(v->data() != tail && (*v)[0] == initial, "blocked fresh prefix/address");
    for (unsigned i=3; i<29; ++i) need((*v)[i] == T{}, "blocked fresh default value");
    auto* fresh = v->data();
    v->resize(1); v->resize(23);
    need(v->data() == fresh && (*v)[0] == initial, "capacity prefix/address");
    for (unsigned i=1; i<23; ++i) need((*v)[i] == T{}, "capacity default value");
    v->resize(43, (*v)[0]);
    for (unsigned i=23; i<43; ++i) need((*v)[i] == initial, "self alias fill");
    std::destroy_at(v);
    fd::retire_dynamic_bytes(alloc, storage, sizeof(V));
    fd::retire_dynamic_bytes(alloc, blocker, 129);
}

struct PolicyRoot { xo::XVector<PolicyEnum> values; xo::XString blocker; };
void custom_enum_fresh_dispatch() {
    auto b = xo::XBuffer::create<PolicyRoot>(1u << 20,
        xo::XBuffer::maximum_capacity(4u << 20).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.root<PolicyRoot>();
    r.values.resize(3);
    need(policy_defaults == 3, "custom scalar first fresh default dispatch");
    for (auto value : r.values) need(value == PolicyEnum::hook_default, "custom default overwritten");
    r.values[0] = PolicyEnum::prefix;
    auto* old = r.values.data();
    r.blocker.assign(std::string(129, 'x'));
    r.values.resize(7);
    need(r.values.data() != old && policy_defaults == 7 && policy_relocations == 3,
         "custom scalar blocked fresh construction/relocation dispatch");
    need(r.values[0] == PolicyEnum::prefix, "custom scalar prefix lost");
    for (unsigned i=3; i<7; ++i)
        need(r.values[i] == PolicyEnum::hook_default, "custom scalar fresh hook bypassed");
}

int main() { try {
    paths<std::uint8_t>(93); paths<std::uint32_t>(701);
    paths<bool>(true); paths<double>(1.25);
    paths<ZeroEnum>(ZeroEnum::nonzero);
    int local = 3; paths<int*>(&local);
    // In particular, a null member pointer need not have all-zero bytes.
    paths<int Member::*>(&Member::field);
    paths<void (Member::*)()>(&Member::function);
    custom_enum_fresh_dispatch();
    std::cout << "8 generic scalar groups and custom enum fresh dispatch passed; no timing\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
